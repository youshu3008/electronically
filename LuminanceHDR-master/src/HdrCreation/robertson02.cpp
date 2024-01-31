/*
 * This file is a part of Luminance HDR package
 * ----------------------------------------------------------------------
 * Copyright (C) 2013-2014 Davide Anastasia
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------
 */

//! \brief Robertson02 algorithm for automatic self-calibration.
//! \author Davide Anastasia <davideanastasia@users.sourceforge.net>
//! \note heavily inspired by the implementation of the same algorithm
//! in PFSTools (thanks to Grzegorz Krawczyk) and the original QTpfsgui (thanks
//! to Giuseppe Rota)

#include "robertson02.h"
#include <Libpfs/colorspace/normalizer.h>
#include "arch/math.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <vector>
#include <functional>

#include <Libpfs/array2d.h>

#ifndef NDEBUG
#define PRINT_DEBUG(str) std::cerr << "Robertson: " << str << std::endl
#else
#define PRINT_DEBUG(str)
#endif

using namespace pfs;
using namespace pfs::colorspace;
using namespace std;
using namespace std::placeholders;

namespace libhdr {
namespace fusion {

void RobertsonOperator::applyResponse(
    ResponseCurve &response, WeightFunction &weight, ResponseChannel channel,
    const DataList &inputData, float *outputData, size_t width, size_t height,
    float minAllowedValue, float maxAllowedValue, const float *arrayofexptime) {
    assert(inputData.size());

    size_t saturatedPixels = 0;
    
    // find index of next darker frame for each frame
    vector<size_t> i_sorted((int)inputData.size());
    iota(i_sorted.begin(), i_sorted.end(), 0);
    sort(i_sorted.begin(), i_sorted.end(), [&arrayofexptime](const size_t &a, const size_t &b)
         { return arrayofexptime[a] < arrayofexptime[b];});
    
    vector<int> idx_darker((int)inputData.size());
    idx_darker[i_sorted[0]] = -1; // darkest frame has no darker predecessor
    for (int i = 1; i < (int)inputData.size(); ++i) {
        if (arrayofexptime[i_sorted[i]] > arrayofexptime[i_sorted[i-1]]) {
            // frame is truly brighter than previous one
            idx_darker[i_sorted[i]] = i_sorted[i-1];
        }
        else {
            // same brightness as previous frame -> take next darker one
            idx_darker[i_sorted[i]] = idx_darker[i_sorted[i-1]];
        }
    }
    
    float r_max = response(maxAllowedValue, channel);

    int numPixels = (int)width * height;
    for (int j = 0; j < numPixels; ++j) {
        // all exposures for each pixel
        float sum = 0.0f;
        float div = 0.0f;
        float maxti = -1e6f;
        float minti = +1e6f;

        // for all exposures
        for (int i = 0; i < (int)inputData.size(); ++i) {
            float m = inputData[i][j];
            float ti = arrayofexptime[i];

            float w = weight(m);
            float r = response(m, channel);
            
            // robust anti-overexposure: suppress values when overexposure was
            // expected from next-darker frame
            int i_darker = idx_darker[i];
            if (i_darker < 0) {
                // always give nonzero weight to darkest frame
                if (w < 1e-6)
                    w = 1e-6;
            }
            else {
                // suppress value, when overexposure is expected
                float m_darker = inputData[i_darker][j];
                float r_darker = response(m_darker, channel);
                float ti_darker = arrayofexptime[i_darker];
                float expected_brightness = (ti / ti_darker) * r_darker / r_max;
                // this weighting function could be pre-computed in a LUT
                float weight_darker = 1.f - pow(expected_brightness, 4);
                if (weight_darker < 0.f)
                    weight_darker = 0.f;
                w *= weight_darker;
            }
            
            // --- anti saturation: observe minimum exposure time at which
            // saturated value is present, and maximum exp time at which
            // black value is present
            if (m > maxAllowedValue) {
                minti = min(minti, ti);
            }
            if (m < minAllowedValue) {
                maxti = max(maxti, ti);
            }

            // --- anti-ghosting: monotonous increase in time should result
            // in monotonous increase in intensity; make forward and
            // backward check, ignore value if condition not satisfied
            //            int m_lower = inputData.getSample(i_lower[i], j);
            //            int m_upper = inputData.getSample(i_upper[i], j);

            //            if ( N > 1) {
            //                if ( m_lower > m || m_upper < m ) {
            //                    continue;
            //                }
            //            }

            // Robertson originally weights response values with exposure time,
            // which assumes that noise is dominated by discretization noise
            //sum += w * ti * r;
            //div += w * ti * ti;
            // In reality, noise is rather dominated by photon shot-noise.
            // In that case, optimal weighting is without an additional
            // exposure time factor. Simple, robust and intuitive.
            sum += w * r;
            div += w * ti;
        }

        // --- anti saturation: if a meaningful representation of pixel
        // was not found, replace it with information from observed data
        if (div == 0.0f) {
            ++saturatedPixels;
        }
        if (div == 0.0f && maxti > -1e6f) {
            sum = response(minAllowedValue, channel);
            div = maxti;
        }
        if (div == 0.0f && minti < +1e6f) {
            sum = response(maxAllowedValue, channel);
            div = minti;
        }

        if (div != 0.0f) {
            outputData[j] = sum / div;
        } else {
            outputData[j] = 0.0f;
        }
    }

    PRINT_DEBUG("Saturated pixels: " << saturatedPixels);
}

void RobertsonOperator::computeFusion(ResponseCurve &response,
                                      WeightFunction &weight,
                                      const vector<FrameEnhanced> &frames,
                                      pfs::Frame &frame) {
    assert(frames.size());

    const size_t numExposures = frames.size();
    const size_t W = frames[0].frame()->getWidth();
    const size_t H = frames[0].frame()->getHeight();
    Frame tempFrame(W, H);

    Channel *outputRed;
    Channel *outputGreen;
    Channel *outputBlue;
    tempFrame.createXYZChannels(outputRed, outputGreen, outputBlue);

    DataList redChannels(numExposures);
    DataList greenChannels(numExposures);
    DataList blueChannels(numExposures);

    fillDataLists(frames, redChannels, greenChannels, blueChannels);

    float maxAllowedValue = weight.maxTrustedValue();
    float minAllowedValue = weight.minTrustedValue();

    vector<float> averageLuminances;
    transform(frames.begin(), frames.end(),
                   back_inserter(averageLuminances),
                   bind(&FrameEnhanced::averageLuminance, _1));

    applyResponse(response, weight, RESPONSE_CHANNEL_RED, redChannels,
                  outputRed->data(), tempFrame.getWidth(),
                  tempFrame.getHeight(), minAllowedValue, maxAllowedValue,
                  averageLuminances.data());  // red
    applyResponse(response, weight, RESPONSE_CHANNEL_BLUE, blueChannels,
                  outputBlue->data(), tempFrame.getWidth(),
                  tempFrame.getHeight(), minAllowedValue, maxAllowedValue,
                  averageLuminances.data());  // blue
    applyResponse(response, weight, RESPONSE_CHANNEL_GREEN, greenChannels,
                  outputGreen->data(), tempFrame.getWidth(),
                  tempFrame.getHeight(), minAllowedValue, maxAllowedValue,
                  averageLuminances.data());  // green

    float cmax[3];
    cmax[0] = *max_element(outputRed->begin(), outputRed->end());
    cmax[1] = *max_element(outputGreen->begin(), outputGreen->end());
    cmax[2] = *max_element(outputBlue->begin(), outputBlue->end());
    float Max = max(cmax[0], max(cmax[1], cmax[2]));

    Channel *Ch[3] = { outputRed, outputGreen, outputBlue };

#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (size_t k = 0; k < W*H; k++) {
        float r = (*Ch[0])(k);
        float g = (*Ch[1])(k);
        float b = (*Ch[2])(k);
        if(std::isnan(r) || std::isnan(g) || std::isnan(b)) {
            if (!std::isnan(r)) {
                (*Ch[1])(k) = r;
                (*Ch[2])(k) = r;
            }
            else if (!std::isnan(g)) {
                (*Ch[0])(k) = g;
                (*Ch[2])(k) = g;
            }
            else if (!std::isnan(b)) {
                (*Ch[0])(k) = b;
                (*Ch[1])(k) = b;
            }
            else {
                (*Ch[0])(k) = Max;
                (*Ch[1])(k) = Max;
                (*Ch[2])(k) = Max;
            }
        }
    }
    frame.swap(tempFrame);
}

}  // namespace fusion
}  // namespace libhdr

namespace {
using namespace libhdr::fusion;

// maximum iterations after algorithm accepts local minima
const int MAXIT = 35;  // 500;

// maximum accepted error
const float MAX_DELTA = 1e-3f;  // 1e-5f;

float normalizeI(ResponseCurve::ResponseContainer &I) {
    size_t M = I.size();
    size_t Mmin;
    size_t Mmax;
    // find min max
    for (Mmin = 0; Mmin < M && I[Mmin] == 0; ++Mmin)
        ;
    for (Mmax = M - 1; Mmax > 0 && I[Mmax] == 0; --Mmax)
        ;

    size_t Mmid = Mmin + (Mmax - Mmin) / 2;
    float mid = I[Mmid];

    if (mid == 0.0f) {
        // find first non-zero middle response
        while ((Mmid < Mmax) && (I[Mmid] == 0.0f)) {
            Mmid++;
        }
        mid = I[Mmid];
    }

    PRINT_DEBUG("robertson02: middle response, mid = "
                << mid << " [" << Mmid << "]"
                << " " << Mmin << "..." << Mmax);

    if (mid != 0.0f) {
        for (size_t m = 0; m < M; ++m) {
            I[m] /= mid;
        }
    }
    return mid;
}

/*
void pseudoSort(const float* arrayofexptime, int* i_lower, int* i_upper, int N)
{
    for (int i = 0; i < N; ++i)
    {
        i_lower[i] = i;
        i_upper[i] = i;
        float ti = arrayofexptime[i];
        float ti_upper = arrayofexptime[0];
        float ti_lower = arrayofexptime[0];

        for ( int j = 0; j < N; ++j ) {
            if ( i != j ) {
                if ( arrayofexptime[j] > ti && arrayofexptime[j] < ti_upper ) {
                    ti_upper = arrayofexptime[j];
                    i_upper[i] = j;
                }
                if ( arrayofexptime[j] < ti && arrayofexptime[j] > ti_lower ) {
                    ti_lower = arrayofexptime[j];
                    i_lower[i] = j;
                }
            }
        }
        // if ( i_lower[i] == -1 ) i_lower[i] = i;
        // if ( i_upper[i] == -1 ) i_upper[i] = i;
    }
}
*/

}  // anonymous

namespace libhdr {
namespace fusion {

void RobertsonOperatorAuto::computeResponse(
    ResponseCurve &response, WeightFunction &weight, ResponseChannel channel,
    const DataList &inputData, float *outputData, size_t width, size_t height,
    float minAllowedValue, float maxAllowedValue, const float *arrayofexptime) {
    typedef ResponseCurve::ResponseContainer ResponseContainer;

    int N = inputData.size();

    // 0 . initialization
    // a. normalize response
    ResponseContainer &I = response.get(channel);
    normalizeI(I);
    // b. copy response
    ResponseContainer Ip = response.get(channel);
    // c. set previous delta
    double pdelta = 0.0;

    applyResponse(response, weight, channel, inputData, outputData, width,
                  height, minAllowedValue, maxAllowedValue, arrayofexptime);

    vector<long> cardEm(ResponseCurve::NUM_BINS);
    ResponseContainer sum;

    assert(sum.size() == cardEm.size());
    assert(sum.size() == Ip.size());
    assert(sum.size() == I.size());

    for (size_t cur_it = 0; cur_it < MAXIT; ++cur_it) {
        // reset buffers
        fill(cardEm.begin(), cardEm.end(), 0);
        fill(sum.begin(), sum.end(), 0.f);

        // 1. Minimize with respect to I
        for (int i = 0; i < N; ++i) {
            float ti = arrayofexptime[i];
            // this is probably uglier than necessary, (I copy th FOR in order
            // not to do the IFs inside them) but I don't know how to improve it
            for (size_t j = 0; j < width * height; ++j) {
                size_t sample = response.getIdx(inputData[i][j]);
                // if ((sample < ResponseCurve::NUM_BINS) && (sample >= 0)) //
                // sample is
                // unsigned so always >= 0
                if (sample < ResponseCurve::NUM_BINS) {
                    sum[sample] += ti * outputData[j];
                    cardEm[sample]++;
                }
#ifndef NDEBUG
                else
                    PRINT_DEBUG("robertson02: sample out of range: " << sample);
#endif
            }
        }

        float Iprevious = 0.f;
        for (size_t m = 0; m < I.size(); ++m) {
            if (cardEm[m] != 0) {
                I[m] = Iprevious = sum[m] / cardEm[m];
            } else {
                I[m] = Iprevious;
            }
        }

        // 2. Normalize I
        normalizeI(I);

        // 3. Apply new response
        applyResponse(response, weight, channel, inputData, outputData, width,
                      height, minAllowedValue, maxAllowedValue, arrayofexptime);

        // 4. Check stopping condition
        double delta = 0.0;
        int hits = 0;
        for (size_t m = 0; m < I.size(); ++m) {
            if (I[m] != 0.0f) {
                float diff = I[m] - Ip[m];
                delta += (double) diff * diff;
                Ip[m] = I[m];
                hits++;
            }
        }
        delta /= hits;

#ifndef NDEBUG
        PRINT_DEBUG(" #" << cur_it << " delta=" << delta
                         << " (coverage: " << 100 * hits / I.size() << "%)");
#endif
        if (delta < MAX_DELTA) {
#ifndef NDEBUG
            cerr << " #" << cur_it << " delta=" << pdelta
                      << " <- converged\n";
#endif
            break;
        } else if (std::isnan(delta) ||
                   (cur_it > MAXIT && pdelta < delta)) {
#ifndef NDEBUG
            cerr
                << "algorithm failed to converge, too noisy data in range\n";
#endif
            break;
        }

        pdelta = delta;
    }
}

void RobertsonOperatorAuto::computeFusion(
    ResponseCurve &response, WeightFunction &weight,
    const vector<FrameEnhanced> &frames, pfs::Frame &outFrame) {
    assert(frames.size());

    const size_t numExposures = frames.size();
    const size_t W = frames[0].frame()->getWidth();
    const size_t H = frames[0].frame()->getHeight();
    Frame tempFrame(W, H);

    Channel *outputRed;
    Channel *outputGreen;
    Channel *outputBlue;
    tempFrame.createXYZChannels(outputRed, outputGreen, outputBlue);

    DataList redChannels(numExposures);
    DataList greenChannels(numExposures);
    DataList blueChannels(numExposures);

    fillDataLists(frames, redChannels, greenChannels, blueChannels);

    float maxAllowedValue = weight.maxTrustedValue();
    float minAllowedValue = weight.minTrustedValue();

    vector<float> averageLuminances;
    transform(frames.begin(), frames.end(),
                   back_inserter(averageLuminances),
                   bind(&FrameEnhanced::averageLuminance, _1));

    // red
    computeResponse(response, weight, RESPONSE_CHANNEL_RED, redChannels,
                    outputRed->data(), tempFrame.getWidth(),
                    tempFrame.getHeight(), minAllowedValue, maxAllowedValue,
                    averageLuminances.data());
    // green
    computeResponse(response, weight, RESPONSE_CHANNEL_GREEN, greenChannels,
                    outputGreen->data(), tempFrame.getWidth(),
                    tempFrame.getHeight(), minAllowedValue, maxAllowedValue,
                    averageLuminances.data());
    // blue
    computeResponse(response, weight, RESPONSE_CHANNEL_BLUE, blueChannels,
                    outputBlue->data(), tempFrame.getWidth(),
                    tempFrame.getHeight(), minAllowedValue, maxAllowedValue,
                    averageLuminances.data());

    float cmax[3];
    cmax[0] = *max_element(outputRed->begin(), outputRed->end());
    cmax[1] = *max_element(outputGreen->begin(), outputGreen->end());
    cmax[2] = *max_element(outputBlue->begin(), outputBlue->end());
    float Max = max(cmax[0], max(cmax[1], cmax[2]));

    Channel *Ch[3] = { outputRed, outputGreen, outputBlue };

#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (size_t k = 0; k < W*H; k++) {
        float r = (*Ch[0])(k);
        float g = (*Ch[1])(k);
        float b = (*Ch[2])(k);
        if(std::isnan(r) || std::isnan(g) || std::isnan(b)) {
            if (!std::isnan(r)) {
                (*Ch[1])(k) = r;
                (*Ch[2])(k) = r;
            }
            else if (!std::isnan(g)) {
                (*Ch[0])(k) = g;
                (*Ch[2])(k) = g;
            }
            else if (!std::isnan(b)) {
                (*Ch[0])(k) = b;
                (*Ch[1])(k) = b;
            }
            else {
                (*Ch[0])(k) = Max;
                (*Ch[1])(k) = Max;
                (*Ch[2])(k) = Max;
            }
        }
    }

    outFrame.swap(tempFrame);
}

}  // namespace fusion
}  // namespace libhdr
