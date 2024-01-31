/*
 * This file is a part of Luminance HDR package
 * ----------------------------------------------------------------------
 * Copyright (C) 2012 Davide Anastasia
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

#include <gtest/gtest.h>
#include <algorithm>
#include <tuple>

#include "TonemappingOperators/mantiuk06/pyramid.h"
#include "mantiuk06/contrast_domain.h"

using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::Combine;

struct RandZeroOne
{
    float operator()()
    {
        return (static_cast<float>(rand())/RAND_MAX);
    }
};

TEST(TestPyramidS, Ctor)
{
    PyramidS t(50, 100);

    EXPECT_EQ(t.getCols(), 50u);
    EXPECT_EQ(t.getRows(), 100u);
}

void compareVectors(const float* a, const float* b, size_t size)
{
    for (size_t idx = 0; idx < size; ++idx)
    {
        ASSERT_NEAR(a[idx], b[idx], 0.001); // 0001);
    }
}

void compareVectors(const XYGradient* inXYGradient,
                    const float* inGx, const float* inGy, size_t size)
{
    for (size_t idx = 0; idx < size; idx++)
    {
        ASSERT_NEAR(inXYGradient[idx].gX(), inGx[idx], 10e-2f);
        ASSERT_NEAR(inXYGradient[idx].gY(), inGy[idx], 10e-2f);
    }
}

class TestPyramidT : public TestWithParam< ::std::tuple<int, int> >
{
protected:
    size_t m_rows;
    size_t m_cols;

    pfs::Array2Df samples_;
    PyramidT newPyramid_;
    test_mantiuk06::pyramid_t* oldPyramid_;


public:
    TestPyramidT()
        : m_rows( ::std::get<1>( GetParam() ))
        , m_cols( ::std::get<0>( GetParam() ))
        , samples_(m_cols, m_rows)
        , newPyramid_(m_rows, m_cols)
        , oldPyramid_( test_mantiuk06::pyramid_allocate(m_cols, m_rows) )
    {
        std::cout << "(rows: " << m_rows << ", cols: " << m_cols << ")" << std::endl;

        std::generate(samples_.begin(), samples_.end(), RandZeroOne());
    }

    ~TestPyramidT()
    {
        test_mantiuk06::pyramid_free(oldPyramid_);
    }

    size_t cols() const { return m_cols; }
    size_t rows() const { return m_rows; }
    size_t size() const { return m_rows*m_cols; }

    size_t computeLevels(const test_mantiuk06::pyramid_t* t)
    {
        const test_mantiuk06::pyramid_t* cursor = t;

        size_t levels = 0;
        while (cursor != NULL) { levels++; cursor = cursor->next; }
        return levels;
    }

    void comparePyramids()
    {
        PyramidT::iterator it = newPyramid_.begin();
        test_mantiuk06::pyramid_t* cursor = oldPyramid_;
        while ( cursor != NULL )
        {
            EXPECT_EQ(static_cast<size_t>(cursor->cols), it->getCols());
            EXPECT_EQ(static_cast<size_t>(cursor->rows), it->getRows());

            compareVectors(it->data(), cursor->Gx, cursor->Gy, cursor->cols*cursor->rows);

            cursor = cursor->next;
            ++it;
        }
    }

    void populatePyramids()
    {
        newPyramid_.computeGradients(samples_);
        test_mantiuk06::pyramid_calculate_gradient(oldPyramid_, samples_.data());
    }
};

TEST(TestPyramidTBasic, CopyCtor)
{
    PyramidT pyramid(2000, 1500);

    EXPECT_EQ(pyramid.numLevels(), 9u);

    PyramidT copyPyramid = pyramid;

    EXPECT_EQ(copyPyramid.numLevels(), 9u);
}

TEST_P(TestPyramidT, Ctor)
{
    // EXPECT_EQ(newPyramid_.numLevels(), 9u);
    EXPECT_EQ(newPyramid_.numLevels(), computeLevels(oldPyramid_));

    PyramidT::iterator it = newPyramid_.begin();
    test_mantiuk06::pyramid_t* cursor = oldPyramid_;
    while ( cursor != NULL )
    {
        EXPECT_EQ(static_cast<size_t>(cursor->cols), it->getCols());
        EXPECT_EQ(static_cast<size_t>(cursor->rows), it->getRows());

        cursor = cursor->next;
        ++it;
    }
}

TEST_P(TestPyramidT, FillGradient)
{
    populatePyramids();

    comparePyramids();
}

TEST_P(TestPyramidT, SumOfDivergence)
{
    // ...and fill gradient!
    populatePyramids();

    // reference
    std::vector<float> samplesRef( size() );
    test_mantiuk06::pyramid_calculate_divergence_sum(oldPyramid_, samplesRef.data());

    // under test!
    pfs::Array2Df samplesTest( cols(), rows() );
    newPyramid_.computeSumOfDivergence( samplesTest );

    compareVectors(samplesRef.data(), samplesTest.data(), size());
}

TEST_P(TestPyramidT, TranformToR)
{
    populatePyramids();

    newPyramid_.transformToR( 1.5f );
    test_mantiuk06::pyramid_transform_to_R(oldPyramid_, 1.5f);

    comparePyramids();
}

TEST_P(TestPyramidT, TranformToG)
{
    populatePyramids();

    newPyramid_.transformToG( 1.5f );
    test_mantiuk06::pyramid_transform_to_G(oldPyramid_, 1.5f);

    comparePyramids();
}

TEST_P(TestPyramidT, Scale)
{
    populatePyramids();

    newPyramid_.scale( 2.5f );
    test_mantiuk06::pyramid_gradient_multiply(oldPyramid_, 2.5f);

    comparePyramids();
}

// This test will always fail, because the internal implementation is slightly
// different. In particular, because internally there are sort functions, the
// final ordering can be different in the two implementations
#if 0
void contrastEqualization(PyramidT& pp, const float contrastFactor);

TEST_P(TestPyramidT, ContrastEqualization)
{
    populatePyramids();

    test_mantiuk06::contrast_equalization(oldPyramid_, -1.5f);
    contrastEqualization(newPyramid_, -1.5f);

    comparePyramids();
}
#endif


INSTANTIATE_TEST_CASE_P(Mantiuk06,
                        TestPyramidT,
                        Combine(Values(765, 320, 96),
                                Values(521, 123))
                        );

class TestDualPyramidT : public TestWithParam< ::std::tuple<int, int> >
{
protected:
    size_t m_rows;
    size_t m_cols;

    pfs::Array2Df samples_;
    PyramidT newPyramid1_;
    PyramidT newPyramid2_;
    test_mantiuk06::pyramid_t* oldPyramid1_;
    test_mantiuk06::pyramid_t* oldPyramid2_;

public:
    TestDualPyramidT()
        : m_rows( ::std::get<1>( GetParam() ))
        , m_cols( ::std::get<0>( GetParam() ))
        , samples_(m_cols, m_rows)
        , newPyramid1_(m_rows, m_cols)
        , newPyramid2_(m_rows, m_cols)
        , oldPyramid1_( test_mantiuk06::pyramid_allocate(m_cols, m_rows) )
        , oldPyramid2_( test_mantiuk06::pyramid_allocate(m_cols, m_rows) )
    {
        std::generate(samples_.begin(), samples_.end(), RandZeroOne());

        // fill gradients
        newPyramid1_.computeGradients( samples_ );
        newPyramid2_.computeGradients( samples_ );

        test_mantiuk06::pyramid_calculate_gradient(oldPyramid1_, samples_.data());
        test_mantiuk06::pyramid_calculate_gradient(oldPyramid2_, samples_.data());
    }

    ~TestDualPyramidT()
    {
        test_mantiuk06::pyramid_free(oldPyramid1_);
        test_mantiuk06::pyramid_free(oldPyramid2_);
    }

    size_t cols() const { return m_cols; }
    size_t rows() const { return m_rows; }
    size_t size() const { return m_rows*m_cols; }

    size_t computeLevels(const test_mantiuk06::pyramid_t* t)
    {
        const test_mantiuk06::pyramid_t* cursor = t;

        size_t levels = 0;
        while (cursor != NULL) { levels++; cursor = cursor->next; }
        return levels;
    }

    void comparePyramids()
    {
        {
        PyramidT::iterator it = newPyramid1_.begin();
        test_mantiuk06::pyramid_t* cursor = oldPyramid1_;
        while ( cursor != NULL )
        {
            EXPECT_EQ(static_cast<size_t>(cursor->cols), it->getCols());
            EXPECT_EQ(static_cast<size_t>(cursor->rows), it->getRows());

            compareVectors(it->data(), cursor->Gx, cursor->Gy, cursor->cols*cursor->rows);

            cursor = cursor->next;
            ++it;
        }
        }
        {
        PyramidT::iterator it = newPyramid2_.begin();
        test_mantiuk06::pyramid_t* cursor = oldPyramid2_;
        while ( cursor != NULL )
        {
            EXPECT_EQ(static_cast<size_t>(cursor->cols), it->getCols());
            EXPECT_EQ(static_cast<size_t>(cursor->rows), it->getRows());

            compareVectors(it->data(), cursor->Gx, cursor->Gy, cursor->cols*cursor->rows);

            cursor = cursor->next;
            ++it;
        }
        }
    }
};


TEST_P(TestDualPyramidT, ComputeScaleFactors)
{
    // compute scalingFactors!
    newPyramid1_.computeScaleFactors( newPyramid2_ );
    test_mantiuk06::pyramid_calculate_scale_factor(oldPyramid1_, oldPyramid2_);

    comparePyramids();
}

TEST_P(TestDualPyramidT, ScaleByGradient)
{
    // compute scalingFactors!
    newPyramid1_.multiply( newPyramid2_ );
    test_mantiuk06::pyramid_scale_gradient(oldPyramid1_, oldPyramid2_);

    comparePyramids();
}

//// divG_sum = A * x = sum(divG(x))
void multiplyA(PyramidT& px, const PyramidT& pC,
               const pfs::Array2Df& x, pfs::Array2Df& sumOfDivG);

TEST_P(TestDualPyramidT, multiplyA)
{
    // generate fake image...
    pfs::Array2Df frame1(cols(), rows());
    pfs::Array2Df frame2(cols(), rows());
    pfs::Array2Df frame3(cols(), rows());
    pfs::Array2Df frame4(cols(), rows());

    std::generate(frame1.begin(), frame1.end(), RandZeroOne());
    std::generate(frame2.begin(), frame2.end(), RandZeroOne());
    std::copy(frame1.begin(), frame1.end(), frame3.begin());
    std::copy(frame2.begin(), frame2.end(), frame4.begin());

    compareVectors(frame1.data(), frame3.data(), size());
    compareVectors(frame2.data(), frame4.data(), size());

    multiplyA(newPyramid1_, newPyramid2_, frame1, frame2);
    // old API
    test_mantiuk06::multiplyA(oldPyramid1_, oldPyramid2_,
                              frame3.data(), frame4.data());

    // invariant!
    comparePyramids();
    compareVectors(frame1.data(), frame3.data(), size());

    // actual result!
    compareVectors(frame2.data(), frame4.data(), size());
}

INSTANTIATE_TEST_CASE_P(Mantiuk06,
                        TestDualPyramidT,
                        Combine(Values(765, 320, 96),
                                Values(521, 123))
                        );
