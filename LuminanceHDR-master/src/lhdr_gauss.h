/*
 *  This file is part of Luminance HDR.
 *
 *  Luminance HDR is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Luminance HDR is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Luminance HDR. If not, see <http://www.gnu.org/licenses/>.
 *
 *  This file was copied from RawTherapee on 23 Nov 2017, commit d61df9d.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) Ingo Weyrich <heckflosse67@gmx.de>
*/

#ifndef _LHDR_GAUSS_H_
#define _LHDR_GAUSS_H_

#include <cmath>
#include <cstdlib>
#include <cstring>
#include "opthelper.h"


namespace
{

template<class T> void calculateYvVFactors( const T sigma, T &b1, T &b2, T &b3, T &B, T M[3][3])
{
    // coefficient calculation
    T q;

    if (sigma < 2.5) {
        q = 3.97156 - 4.14554 * sqrt (1.0 - 0.26891 * sigma);
    } else {
        q = 0.98711 * sigma - 0.96330;
    }

    T b0 = 1.57825 + 2.44413 * q + 1.4281 * q * q + 0.422205 * q * q * q;
    b1 = 2.44413 * q + 2.85619 * q * q + 1.26661 * q * q * q;
    b2 = -1.4281 * q * q - 1.26661 * q * q * q;
    b3 = 0.422205 * q * q * q;
    B = 1.0 - (b1 + b2 + b3) / b0;

    b1 /= b0;
    b2 /= b0;
    b3 /= b0;

    // From: Bill Triggs, Michael Sdika: Boundary Conditions for Young-van Vliet Recursive Filtering
    M[0][0] = -b3 * b1 + 1.0 - b3 * b3 - b2;
    M[0][1] = (b3 + b1) * (b2 + b3 * b1);
    M[0][2] = b3 * (b1 + b3 * b2);
    M[1][0] = b1 + b3 * b2;
    M[1][1] = -(b2 - 1.0) * (b2 + b3 * b1);
    M[1][2] = -(b3 * b1 + b3 * b3 + b2 - 1.0) * b3;
    M[2][0] = b3 * b1 + b2 + b1 * b1 - b2 * b2;
    M[2][1] = b1 * b2 + b3 * b2 * b2 - b1 * b3 * b3 - b3 * b3 * b3 - b3 * b2 + b3;
    M[2][2] = b3 * (b1 + b3 * b2);

}

// classical filtering if the support window is small and src != dst
template<class T> void gauss3x3 (T** RESTRICT src, T** RESTRICT dst, const int W, const int H, const T c0, const T c1, const T c2, const T b0, const T b1)
{

    // first row
#ifdef _OPENMP
    #pragma omp single nowait
#endif
    {
        dst[0][0] = src[0][0];

        for (int j = 1; j < W - 1; j++)
        {
            dst[0][j] = b1 * (src[0][j - 1] + src[0][j + 1]) + b0 * src[0][j];
        }

        dst[0][W - 1] = src[0][W - 1];
    }

#ifdef _OPENMP
    #pragma omp for nowait
#endif

    for (int i = 1; i < H - 1; i++) {
        dst[i][0] = b1 * (src[i - 1][0] + src[i + 1][0]) + b0 * src[i][0];

        for (int j = 1; j < W - 1; j++) {
            dst[i][j] = c2 * (src[i - 1][j - 1] + src[i - 1][j + 1] + src[i + 1][j - 1] + src[i + 1][j + 1]) + c1 * (src[i - 1][j] + src[i][j - 1] + src[i][j + 1] + src[i + 1][j]) + c0 * src[i][j];
        }

        dst[i][W - 1] = b1 * (src[i - 1][W - 1] + src[i + 1][W - 1]) + b0 * src[i][W - 1];
    }

    // last row
#ifdef _OPENMP
    #pragma omp single
#endif
    {
        dst[H - 1][0] = src[H - 1][0];

        for (int j = 1; j < W - 1; j++) {
            dst[H - 1][j] = b1 * (src[H - 1][j - 1] + src[H - 1][j + 1]) + b0 * src[H - 1][j];
        }

        dst[H - 1][W - 1] = src[H - 1][W - 1];
    }
}


// use separated filter if the support window is small and src == dst
template<class T> void gaussHorizontal3 (T** src, T** dst, int W, int H, const float c0, const float c1)
{
    T temp[W] ALIGNED16;
#ifdef _OPENMP
    #pragma omp for
#endif

    for (int i = 0; i < H; i++) {
        for (int j = 1; j < W - 1; j++) {
            temp[j] = (T)(c1 * (src[i][j - 1] + src[i][j + 1]) + c0 * src[i][j]);
        }

        dst[i][0] = src[i][0];
        memcpy (dst[i] + 1, temp + 1, (W - 2)*sizeof(T));

        dst[i][W - 1] = src[i][W - 1];
    }
}

#ifdef __SSE2__
template<class T> SSEFUNCTION void gaussVertical3 (T** src, T** dst, int W, int H, const float c0, const float c1)
{
    vfloat Tv = F2V(0.f), Tm1v, Tp1v;
    vfloat Tv1 = F2V(0.f), Tm1v1, Tp1v1;
    vfloat c0v, c1v;
    c0v = F2V(c0);
    c1v = F2V(c1);

#ifdef _OPENMP
    #pragma omp for nowait
#endif

    // process 8 columns per iteration for better usage of cpu cache
    for (int i = 0; i < W - 7; i += 8) {
        Tm1v = LVFU( src[0][i] );
        Tm1v1 = LVFU( src[0][i + 4] );
        STVFU( dst[0][i], Tm1v);
        STVFU( dst[0][i + 4], Tm1v1);

        if (H > 1) {
            Tv = LVFU( src[1][i]);
            Tv1 = LVFU( src[1][i + 4]);
        }

        for (int j = 1; j < H - 1; j++) {
            Tp1v = LVFU( src[j + 1][i]);
            Tp1v1 = LVFU( src[j + 1][i + 4]);
            STVFU( dst[j][i], c1v * (Tp1v + Tm1v) + Tv * c0v);
            STVFU( dst[j][i + 4], c1v * (Tp1v1 + Tm1v1) + Tv1 * c0v);
            Tm1v = Tv;
            Tm1v1 = Tv1;
            Tv = Tp1v;
            Tv1 = Tp1v1;
        }

        STVFU( dst[H - 1][i], LVFU( src[H - 1][i]));
        STVFU( dst[H - 1][i + 4], LVFU( src[H - 1][i + 4]));
    }

// Borders are done without SSE
    float temp[H] ALIGNED16;
#ifdef _OPENMP
    #pragma omp single
#endif

    for (int i = W - (W % 8); i < W; i++) {
        for (int j = 1; j < H - 1; j++) {
            temp[j] = c1 * (src[j - 1][i] + src[j + 1][i]) + c0 * src[j][i];
        }

        dst[0][i] = src[0][i];

        for (int j = 1; j < H - 1; j++) {
            dst[j][i] = temp[j];
        }

        dst[H - 1][i] = src[H - 1][i];
    }
}
#else
template<class T> void gaussVertical3 (T** src, T** dst, int W, int H, const float c0, const float c1)
{
    T temp[H] ALIGNED16;
#ifdef _OPENMP
    #pragma omp for
#endif

    for (int i = 0; i < W; i++) {
        for (int j = 1; j < H - 1; j++) {
            temp[j] = (T)(c1 * (src[j - 1][i] + src[j + 1][i]) + c0 * src[j][i]);
        }

        dst[0][i] = src[0][i];

        for (int j = 1; j < H - 1; j++) {
            dst[j][i] = temp[j];
        }

        dst[H - 1][i] = src[H - 1][i];
    }
}
#endif

#ifdef __SSE2__
// fast gaussian approximation if the support window is large
template<class T> SSEFUNCTION void gaussHorizontalSse (T** src, T** dst, const int W, const int H, const float sigma)
{
    double b1, b2, b3, B, M[3][3];
    calculateYvVFactors<double>(sigma, b1, b2, b3, B, M);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            M[i][j] *= (1.0 + b2 + (b1 - b3) * b3);
            M[i][j] /= (1.0 + b1 - b2 + b3) * (1.0 - b1 - b2 - b3);
        }

    vfloat Rv;
    vfloat Tv, Tm2v, Tm3v;
    vfloat Bv, b1v, b2v, b3v;
    vfloat temp2W, temp2Wp1;
    float tmp[W][4] ALIGNED16;
    Bv = F2V(B);
    b1v = F2V(b1);
    b2v = F2V(b2);
    b3v = F2V(b3);

#ifdef _OPENMP
    #pragma omp for nowait
#endif

    for (int i = 0; i < H - 3; i += 4) {
        Tv = _mm_set_ps(src[i][0], src[i + 1][0], src[i + 2][0], src[i + 3][0]);
        Tm3v = Tv * (Bv + b1v + b2v + b3v);
        STVF( tmp[0][0], Tm3v );

        Tm2v = _mm_set_ps(src[i][1], src[i + 1][1], src[i + 2][1], src[i + 3][1]) * Bv + Tm3v * b1v + Tv * (b2v + b3v);
        STVF( tmp[1][0], Tm2v );

        Rv = _mm_set_ps(src[i][2], src[i + 1][2], src[i + 2][2], src[i + 3][2]) * Bv + Tm2v * b1v + Tm3v * b2v + Tv * b3v;
        STVF( tmp[2][0], Rv );

        for (int j = 3; j < W; j++) {
            Tv = Rv;
            Rv = _mm_set_ps(src[i][j], src[i + 1][j], src[i + 2][j], src[i + 3][j]) * Bv + Tv * b1v + Tm2v * b2v + Tm3v * b3v;
            STVF( tmp[j][0], Rv );
            Tm3v = Tm2v;
            Tm2v = Tv;
        }

        Tv = _mm_set_ps(src[i][W - 1], src[i + 1][W - 1], src[i + 2][W - 1], src[i + 3][W - 1]);

        temp2Wp1 = Tv + F2V(M[2][0]) * (Rv - Tv) + F2V(M[2][1]) * ( Tm2v - Tv ) +  F2V(M[2][2]) * (Tm3v - Tv);
        temp2W = Tv + F2V(M[1][0]) * (Rv - Tv) + F2V(M[1][1]) * (Tm2v - Tv) + F2V(M[1][2]) * (Tm3v - Tv);

        Rv = Tv + F2V(M[0][0]) * (Rv - Tv) + F2V(M[0][1]) * (Tm2v - Tv) + F2V(M[0][2]) * (Tm3v - Tv);
        STVF(tmp[W - 1][0], Rv);

        Tm2v = Bv * Tm2v + b1v * Rv + b2v * temp2W + b3v * temp2Wp1;
        STVF(tmp[W - 2][0], Tm2v);

        Tm3v = Bv * Tm3v + b1v * Tm2v + b2v * Rv + b3v * temp2W;
        STVF(tmp[W - 3][0], Tm3v);

        Tv = Rv;
        Rv = Tm3v;
        Tm3v = Tv;

        for (int j = W - 4; j >= 0; j--) {
            Tv = Rv;
            Rv = LVF(tmp[j][0]) * Bv + Tv * b1v + Tm2v * b2v + Tm3v * b3v;
            STVF(tmp[j][0], Rv);
            Tm3v = Tm2v;
            Tm2v = Tv;
        }

        for (int j = 0; j < W; j++) {
            dst[i + 3][j] = tmp[j][0];
            dst[i + 2][j] = tmp[j][1];
            dst[i + 1][j] = tmp[j][2];
            dst[i + 0][j] = tmp[j][3];
        }


    }

// Borders are done without SSE
#ifdef _OPENMP
    #pragma omp single
#endif

    for (int i = H - (H % 4); i < H; i++) {
        tmp[0][0] = src[i][0] * (B + b1 + b2 + b3);
        tmp[1][0] = B * src[i][1] + b1 * tmp[0][0]  + src[i][0] * (b2 + b3);
        tmp[2][0] = B * src[i][2] + b1 * tmp[1][0]  + b2 * tmp[0][0]  + b3 * src[i][0];

        for (int j = 3; j < W; j++) {
            tmp[j][0] = B * src[i][j] + b1 * tmp[j - 1][0] + b2 * tmp[j - 2][0] + b3 * tmp[j - 3][0];
        }

        float temp2Wm1 = src[i][W - 1] + M[0][0] * (tmp[W - 1][0] - src[i][W - 1]) + M[0][1] * (tmp[W - 2][0] - src[i][W - 1]) + M[0][2] * (tmp[W - 3][0] - src[i][W - 1]);
        float temp2W   = src[i][W - 1] + M[1][0] * (tmp[W - 1][0] - src[i][W - 1]) + M[1][1] * (tmp[W - 2][0] - src[i][W - 1]) + M[1][2] * (tmp[W - 3][0] - src[i][W - 1]);
        float temp2Wp1 = src[i][W - 1] + M[2][0] * (tmp[W - 1][0] - src[i][W - 1]) + M[2][1] * (tmp[W - 2][0] - src[i][W - 1]) + M[2][2] * (tmp[W - 3][0] - src[i][W - 1]);

        tmp[W - 1][0] = temp2Wm1;
        tmp[W - 2][0] = B * tmp[W - 2][0] + b1 * tmp[W - 1][0] + b2 * temp2W + b3 * temp2Wp1;
        tmp[W - 3][0] = B * tmp[W - 3][0] + b1 * tmp[W - 2][0] + b2 * tmp[W - 1][0] + b3 * temp2W;

        for (int j = W - 4; j >= 0; j--) {
            tmp[j][0] = B * tmp[j][0] + b1 * tmp[j + 1][0] + b2 * tmp[j + 2][0] + b3 * tmp[j + 3][0];
        }

        for (int j = 0; j < W; j++) {
            dst[i][j] = tmp[j][0];
        }
    }
}
#endif

// fast gaussian approximation if the support window is large
template<class T> void gaussHorizontal (T** src, T** dst, const int W, const int H, const double sigma)
{
    double b1, b2, b3, B, M[3][3];
    calculateYvVFactors<double>(sigma, b1, b2, b3, B, M);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            M[i][j] /= (1.0 + b1 - b2 + b3) * (1.0 + b2 + (b1 - b3) * b3);
        }

    double temp2[W] ALIGNED16;

#ifdef _OPENMP
    #pragma omp for
#endif

    for (int i = 0; i < H; i++) {

        temp2[0] = B * src[i][0] + b1 * src[i][0] + b2 * src[i][0] + b3 * src[i][0];
        temp2[1] = B * src[i][1] + b1 * temp2[0]  + b2 * src[i][0] + b3 * src[i][0];
        temp2[2] = B * src[i][2] + b1 * temp2[1]  + b2 * temp2[0]  + b3 * src[i][0];

        for (int j = 3; j < W; j++) {
            temp2[j] = B * src[i][j] + b1 * temp2[j - 1] + b2 * temp2[j - 2] + b3 * temp2[j - 3];
        }

        double temp2Wm1 = src[i][W - 1] + M[0][0] * (temp2[W - 1] - src[i][W - 1]) + M[0][1] * (temp2[W - 2] - src[i][W - 1]) + M[0][2] * (temp2[W - 3] - src[i][W - 1]);
        double temp2W   = src[i][W - 1] + M[1][0] * (temp2[W - 1] - src[i][W - 1]) + M[1][1] * (temp2[W - 2] - src[i][W - 1]) + M[1][2] * (temp2[W - 3] - src[i][W - 1]);
        double temp2Wp1 = src[i][W - 1] + M[2][0] * (temp2[W - 1] - src[i][W - 1]) + M[2][1] * (temp2[W - 2] - src[i][W - 1]) + M[2][2] * (temp2[W - 3] - src[i][W - 1]);

        temp2[W - 1] = temp2Wm1;
        temp2[W - 2] = B * temp2[W - 2] + b1 * temp2[W - 1] + b2 * temp2W + b3 * temp2Wp1;
        temp2[W - 3] = B * temp2[W - 3] + b1 * temp2[W - 2] + b2 * temp2[W - 1] + b3 * temp2W;

        for (int j = W - 4; j >= 0; j--) {
            temp2[j] = B * temp2[j] + b1 * temp2[j + 1] + b2 * temp2[j + 2] + b3 * temp2[j + 3];
        }

        for (int j = 0; j < W; j++) {
            dst[i][j] = (T)temp2[j];
        }

    }
}

#ifdef __SSE2__
template<class T> SSEFUNCTION void gaussVerticalSse (T** src, T** dst, const int W, const int H, const float sigma)
{
    double b1, b2, b3, B, M[3][3];
    calculateYvVFactors<double>(sigma, b1, b2, b3, B, M);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            M[i][j] *= (1.0 + b2 + (b1 - b3) * b3);
            M[i][j] /= (1.0 + b1 - b2 + b3) * (1.0 - b1 - b2 - b3);
        }

    float tmp[H][8] ALIGNED16;
    vfloat Rv;
    vfloat Tv, Tm2v, Tm3v;
    vfloat Rv1;
    vfloat Tv1, Tm2v1, Tm3v1;
    vfloat Bv, b1v, b2v, b3v;
    vfloat temp2W, temp2Wp1;
    vfloat temp2W1, temp2Wp11;
    Bv = F2V(B);
    b1v = F2V(b1);
    b2v = F2V(b2);
    b3v = F2V(b3);

#ifdef _OPENMP
    #pragma omp for nowait
#endif

    // process 8 columns per iteration for better usage of cpu cache
    for (int i = 0; i < W - 7; i += 8) {
        Tv = LVFU( src[0][i]);
        Tv1 = LVFU( src[0][i + 4]);
        Rv = Tv * (Bv + b1v + b2v + b3v);
        Rv1 = Tv1 * (Bv + b1v + b2v + b3v);
        Tm3v = Rv;
        Tm3v1 = Rv1;
        STVF( tmp[0][0], Rv );
        STVF( tmp[0][4], Rv1 );

        Rv = LVFU(src[1][i]) * Bv + Rv * b1v + Tv * (b2v + b3v);
        Rv1 = LVFU(src[1][i + 4]) * Bv + Rv1 * b1v + Tv1 * (b2v + b3v);
        Tm2v = Rv;
        Tm2v1 = Rv1;
        STVF( tmp[1][0], Rv );
        STVF( tmp[1][4], Rv1 );

        Rv = LVFU(src[2][i]) * Bv + Rv * b1v + Tm3v * b2v + Tv * b3v;
        Rv1 = LVFU(src[2][i + 4]) * Bv + Rv1 * b1v + Tm3v1 * b2v + Tv1 * b3v;
        STVF( tmp[2][0], Rv );
        STVF( tmp[2][4], Rv1 );

        for (int j = 3; j < H; j++) {
            Tv = Rv;
            Tv1 = Rv1;
            Rv = LVFU(src[j][i]) * Bv +  Tv * b1v + Tm2v * b2v + Tm3v * b3v;
            Rv1 = LVFU(src[j][i + 4]) * Bv +  Tv1 * b1v + Tm2v1 * b2v + Tm3v1 * b3v;
            STVF( tmp[j][0], Rv );
            STVF( tmp[j][4], Rv1 );
            Tm3v = Tm2v;
            Tm3v1 = Tm2v1;
            Tm2v = Tv;
            Tm2v1 = Tv1;
        }

        Tv = LVFU(src[H - 1][i]);
        Tv1 = LVFU(src[H - 1][i + 4]);

        temp2Wp1 = Tv + F2V(M[2][0]) * (Rv - Tv) + F2V(M[2][1]) * (Tm2v - Tv) + F2V(M[2][2]) * (Tm3v - Tv);
        temp2Wp11 = Tv1 + F2V(M[2][0]) * (Rv1 - Tv1) + F2V(M[2][1]) * (Tm2v1 - Tv1) + F2V(M[2][2]) * (Tm3v1 - Tv1);
        temp2W = Tv + F2V(M[1][0]) * (Rv - Tv) + F2V(M[1][1]) * (Tm2v - Tv) + F2V(M[1][2]) * (Tm3v - Tv);
        temp2W1 = Tv1 + F2V(M[1][0]) * (Rv1 - Tv1) + F2V(M[1][1]) * (Tm2v1 - Tv1) + F2V(M[1][2]) * (Tm3v1 - Tv1);

        Rv = Tv + F2V(M[0][0]) * (Rv - Tv) + F2V(M[0][1]) * (Tm2v - Tv) + F2V(M[0][2]) * (Tm3v - Tv);
        Rv1 = Tv1 + F2V(M[0][0]) * (Rv1 - Tv1) + F2V(M[0][1]) * (Tm2v1 - Tv1) + F2V(M[0][2]) * (Tm3v1 - Tv1);
        STVFU( dst[H - 1][i], Rv );
        STVFU( dst[H - 1][i + 4], Rv1 );

        Tm2v = Bv * Tm2v + b1v * Rv + b2v * temp2W + b3v * temp2Wp1;
        Tm2v1 = Bv * Tm2v1 + b1v * Rv1 + b2v * temp2W1 + b3v * temp2Wp11;
        STVFU( dst[H - 2][i], Tm2v );
        STVFU( dst[H - 2][i + 4], Tm2v1 );

        Tm3v = Bv * Tm3v + b1v * Tm2v + b2v * Rv + b3v * temp2W;
        Tm3v1 = Bv * Tm3v1 + b1v * Tm2v1 + b2v * Rv1 + b3v * temp2W1;
        STVFU( dst[H - 3][i], Tm3v );
        STVFU( dst[H - 3][i + 4], Tm3v1 );

        Tv = Rv;
        Tv1 = Rv1;
        Rv = Tm3v;
        Rv1 = Tm3v1;
        Tm3v = Tv;
        Tm3v1 = Tv1;

        for (int j = H - 4; j >= 0; j--) {
            Tv = Rv;
            Tv1 = Rv1;
            Rv = LVF(tmp[j][0]) * Bv +  Tv * b1v + Tm2v * b2v + Tm3v * b3v;
            Rv1 = LVF(tmp[j][4]) * Bv +  Tv1 * b1v + Tm2v1 * b2v + Tm3v1 * b3v;
            STVFU( dst[j][i], Rv );
            STVFU( dst[j][i + 4], Rv1 );
            Tm3v = Tm2v;
            Tm3v1 = Tm2v1;
            Tm2v = Tv;
            Tm2v1 = Tv1;
        }
    }

// Borders are done without SSE
#ifdef _OPENMP
    #pragma omp single
#endif

    for (int i = W - (W % 8); i < W; i++) {
        tmp[0][0] = src[0][i] * (B + b1 + b2 + b3);
        tmp[1][0] = B * src[1][i] + b1 * tmp[0][0] + src[0][i] * (b2 + b3);
        tmp[2][0] = B * src[2][i] + b1 * tmp[1][0] + b2 * tmp[0][0] + b3 * src[0][i];

        for (int j = 3; j < H; j++) {
            tmp[j][0] = B * src[j][i] + b1 * tmp[j - 1][0] + b2 * tmp[j - 2][0] + b3 * tmp[j - 3][0];
        }

        float temp2Hm1 = src[H - 1][i] + M[0][0] * (tmp[H - 1][0] - src[H - 1][i]) + M[0][1] * (tmp[H - 2][0] - src[H - 1][i]) + M[0][2] * (tmp[H - 3][0] - src[H - 1][i]);
        float temp2H   = src[H - 1][i] + M[1][0] * (tmp[H - 1][0] - src[H - 1][i]) + M[1][1] * (tmp[H - 2][0] - src[H - 1][i]) + M[1][2] * (tmp[H - 3][0] - src[H - 1][i]);
        float temp2Hp1 = src[H - 1][i] + M[2][0] * (tmp[H - 1][0] - src[H - 1][i]) + M[2][1] * (tmp[H - 2][0] - src[H - 1][i]) + M[2][2] * (tmp[H - 3][0] - src[H - 1][i]);

        tmp[H - 1][0] = temp2Hm1;
        tmp[H - 2][0] = B * tmp[H - 2][0] + b1 * tmp[H - 1][0] + b2 * temp2H + b3 * temp2Hp1;
        tmp[H - 3][0] = B * tmp[H - 3][0] + b1 * tmp[H - 2][0] + b2 * tmp[H - 1][0] + b3 * temp2H;

        for (int j = H - 4; j >= 0; j--) {
            tmp[j][0] = B * tmp[j][0] + b1 * tmp[j + 1][0] + b2 * tmp[j + 2][0] + b3 * tmp[j + 3][0];
        }

        for (int j = 0; j < H; j++) {
            dst[j][i] = tmp[j][0];
        }

    }
}
#endif

template<class T> void gaussVertical (T** src, T** dst, const int W, const int H, const double sigma)
{
    double b1, b2, b3, B, M[3][3];
    calculateYvVFactors<double>(sigma, b1, b2, b3, B, M);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            M[i][j] /= (1.0 + b1 - b2 + b3) * (1.0 + b2 + (b1 - b3) * b3);
        }

    // process 'numcols' columns for better usage of L1 cpu cache (especially faster for large values of H)
    static const int numcols = 8;
    double temp2[H][numcols] ALIGNED16;
    double temp2Hm1[numcols], temp2H[numcols], temp2Hp1[numcols];
#ifdef _OPENMP
    #pragma omp for nowait
#endif

    for (unsigned int i = 0; i < static_cast<unsigned>(std::max(0, W - numcols + 1)); i += numcols) {
        for (int k = 0; k < numcols; k++) {
            temp2[0][k] = B * src[0][i + k] + b1 * src[0][i + k] + b2 * src[0][i + k] + b3 * src[0][i + k];
            temp2[1][k] = B * src[1][i + k] + b1 * temp2[0][k] + b2 * src[0][i + k] + b3 * src[0][i + k];
            temp2[2][k] = B * src[2][i + k] + b1 * temp2[1][k] + b2 * temp2[0][k] + b3 * src[0][i + k];
        }

        for (int j = 3; j < H; j++) {
            for (int k = 0; k < numcols; k++) {
                temp2[j][k] = B * src[j][i + k] + b1 * temp2[j - 1][k] + b2 * temp2[j - 2][k] + b3 * temp2[j - 3][k];
            }
        }

        for (int k = 0; k < numcols; k++) {
            temp2Hm1[k] = src[H - 1][i + k] + M[0][0] * (temp2[H - 1][k] - src[H - 1][i + k]) + M[0][1] * (temp2[H - 2][k] - src[H - 1][i + k]) + M[0][2] * (temp2[H - 3][k] - src[H - 1][i + k]);
            temp2H[k]   = src[H - 1][i + k] + M[1][0] * (temp2[H - 1][k] - src[H - 1][i + k]) + M[1][1] * (temp2[H - 2][k] - src[H - 1][i + k]) + M[1][2] * (temp2[H - 3][k] - src[H - 1][i + k]);
            temp2Hp1[k] = src[H - 1][i + k] + M[2][0] * (temp2[H - 1][k] - src[H - 1][i + k]) + M[2][1] * (temp2[H - 2][k] - src[H - 1][i + k]) + M[2][2] * (temp2[H - 3][k] - src[H - 1][i + k]);
        }

        for (int k = 0; k < numcols; k++) {
            dst[H - 1][i + k] = temp2[H - 1][k] = temp2Hm1[k];
            dst[H - 2][i + k] = temp2[H - 2][k] = B * temp2[H - 2][k] + b1 * temp2[H - 1][k] + b2 * temp2H[k] + b3 * temp2Hp1[k];
            dst[H - 3][i + k] = temp2[H - 3][k] = B * temp2[H - 3][k] + b1 * temp2[H - 2][k] + b2 * temp2[H - 1][k] + b3 * temp2H[k];
        }

        for (int j = H - 4; j >= 0; j--) {
            for (int k = 0; k < numcols; k++) {
                dst[j][i + k] = temp2[j][k] = B * temp2[j][k] + b1 * temp2[j + 1][k] + b2 * temp2[j + 2][k] + b3 * temp2[j + 3][k];
            }
        }
    }

#ifdef _OPENMP
    #pragma omp single
#endif

    // process remaining columns
    for (int i = W - (W % numcols); i < W; i++) {
        temp2[0][0] = B * src[0][i] + b1 * src[0][i] + b2 * src[0][i] + b3 * src[0][i];
        temp2[1][0] = B * src[1][i] + b1 * temp2[0][0]  + b2 * src[0][i] + b3 * src[0][i];
        temp2[2][0] = B * src[2][i] + b1 * temp2[1][0]  + b2 * temp2[0][0]  + b3 * src[0][i];

        for (int j = 3; j < H; j++) {
            temp2[j][0] = B * src[j][i] + b1 * temp2[j - 1][0] + b2 * temp2[j - 2][0] + b3 * temp2[j - 3][0];
        }

        double temp2Hm1 = src[H - 1][i] + M[0][0] * (temp2[H - 1][0] - src[H - 1][i]) + M[0][1] * (temp2[H - 2][0] - src[H - 1][i]) + M[0][2] * (temp2[H - 3][0] - src[H - 1][i]);
        double temp2H   = src[H - 1][i] + M[1][0] * (temp2[H - 1][0] - src[H - 1][i]) + M[1][1] * (temp2[H - 2][0] - src[H - 1][i]) + M[1][2] * (temp2[H - 3][0] - src[H - 1][i]);
        double temp2Hp1 = src[H - 1][i] + M[2][0] * (temp2[H - 1][0] - src[H - 1][i]) + M[2][1] * (temp2[H - 2][0] - src[H - 1][i]) + M[2][2] * (temp2[H - 3][0] - src[H - 1][i]);

        dst[H - 1][i] = temp2[H - 1][0] = temp2Hm1;
        dst[H - 2][i] = temp2[H - 2][0] = B * temp2[H - 2][0] + b1 * temp2[H - 1][0] + b2 * temp2H + b3 * temp2Hp1;
        dst[H - 3][i] = temp2[H - 3][0] = B * temp2[H - 3][0] + b1 * temp2[H - 2][0] + b2 * temp2[H - 1][0] + b3 * temp2H;

        for (int j = H - 4; j >= 0; j--) {
            dst[j][i] = temp2[j][0] = B * temp2[j][0] + b1 * temp2[j + 1][0] + b2 * temp2[j + 2][0] + b3 * temp2[j + 3][0];
        }
    }
}

template<class T> void gaussianBlurImpl(T** src, T** dst, const int W, const int H, const double sigma)
{
    static constexpr auto GAUSS_SKIP = 0.25;
    static constexpr auto GAUSS_3X3_LIMIT = 0.6;
    static constexpr auto GAUSS_DOUBLE = 70.0;

    if (sigma < GAUSS_SKIP) {
        // don't perform filtering
        if (src != dst) {
            memcpy (dst[0], src[0], static_cast<T>(W * H) * sizeof(T));
        }
    } else if (sigma < GAUSS_3X3_LIMIT) {
        if(src != dst) {
            // If src != dst we can take the fast way
            // compute 3x3 kernel values
            double c0 = 1.0;
            double c1 = exp( -0.5 * (lhdrengine::SQR(1.0 / sigma)) );
            double c2 = exp( -lhdrengine::SQR(1.0 / sigma) );

            // normalize kernel values
            double sum = c0 + 4.0 * (c1 + c2);
            c0 /= sum;
            c1 /= sum;
            c2 /= sum;
            // compute kernel values for border pixels
            double b1 = exp (-1.0 / (2.0 * sigma * sigma));
            double bsum = 2.0 * b1 + 1.0;
            b1 /= bsum;
            double b0 = 1.0 / bsum;
            gauss3x3<T> (src, dst, W, H, c0, c1, c2, b0, b1);
        } else {
            // compute kernel values for separated 3x3 gaussian blur
            double c1 = exp (-1.0 / (2.0 * sigma * sigma));
            double csum = 2.0 * c1 + 1.0;
            c1 /= csum;
            double c0 = 1.0 / csum;
            gaussHorizontal3<T> (src, dst, W, H, c0, c1);
            gaussVertical3<T>   (dst, dst, W, H, c0, c1);
        }
    } else {
#ifdef __SSE2__

        if (sigma < GAUSS_DOUBLE) {
            gaussHorizontalSse<T> (src, dst, W, H, sigma);
            gaussVerticalSse<T> (dst, dst, W, H, sigma);
        } else { // large sigma only with double precision
            gaussHorizontal<T> (src, dst, W, H, sigma);
            gaussVertical<T>   (dst, dst, W, H, sigma);
        }

#else

        if (sigma < GAUSS_DOUBLE) {
            gaussHorizontal<T> (src, dst, W, H, sigma);
            gaussVertical<T> (dst, dst, W, H, sigma);
        } else { // large sigma only with double precision
            gaussHorizontal<T> (src, dst, W, H, sigma);
            gaussVertical<T>   (dst, dst, W, H, sigma);
        }

#endif
    }
}
}

void gaussianBlur(float** src, float** dst, const int W, const int H, const double sigma)
{
    gaussianBlurImpl<float>(src, dst, W, H, sigma);
}

void gaussianBlur(float** src, float** dst, const int W, const int H, const double sigma);

#endif
