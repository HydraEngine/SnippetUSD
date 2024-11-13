//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/timeSampleArray.h"
#include "pxr/base/tf/errorMark.h"
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

TEST(TestHydra, test_time_sample_array) {
    TfErrorMark errorMark;

    //
    // HdResampleNeighbors
    //
    {
        // Exact values at endpoints
        ASSERT_TRUE(HdResampleNeighbors(0.0f, VtValue(0.0f), VtValue(256.0f)).Get<float>() == 0.0f);
        ASSERT_TRUE(HdResampleNeighbors(1.0f, VtValue(0.0f), VtValue(256.0f)).Get<float>() == 256.0f);

        // Interpolation -- we don't check exact values, just approximate intervals here
        ASSERT_TRUE(HdResampleNeighbors(0.25f, VtValue(0.0f), VtValue(256.0f)).Get<float>() > 63.0f);
        ASSERT_TRUE(HdResampleNeighbors(0.25f, VtValue(0.0f), VtValue(256.0f)).Get<float>() < 65.0f);
        ASSERT_TRUE(HdResampleNeighbors(0.50f, VtValue(0.0f), VtValue(256.0f)).Get<float>() > 127.0f);
        ASSERT_TRUE(HdResampleNeighbors(0.50f, VtValue(0.0f), VtValue(256.0f)).Get<float>() < 129.0f);
        ASSERT_TRUE(HdResampleNeighbors(0.75f, VtValue(0.0f), VtValue(256.0f)).Get<float>() > 191.0f);
        ASSERT_TRUE(HdResampleNeighbors(0.75f, VtValue(0.0f), VtValue(256.0f)).Get<float>() < 193.0f);

        // Extrapolation
        ASSERT_TRUE(HdResampleNeighbors(-1.0f, VtValue(0.0f), VtValue(256.0f)).Get<float>() > -257.0f);
        ASSERT_TRUE(HdResampleNeighbors(-1.0f, VtValue(0.0f), VtValue(256.0f)).Get<float>() < -255.0f);
        ASSERT_TRUE(HdResampleNeighbors(+2.0f, VtValue(0.0f), VtValue(256.0f)).Get<float>() > 511.0f);
        ASSERT_TRUE(HdResampleNeighbors(+2.0f, VtValue(0.0f), VtValue(256.0f)).Get<float>() < 513.0f);

        // Coding error with mismatched types
        ASSERT_TRUE(errorMark.IsClean());
        HdResampleNeighbors(0.5f, VtValue(1.0), VtValue(2.0f));  // double != float
        ASSERT_TRUE(!errorMark.IsClean());
        errorMark.Clear();

        // Coding error with empty values
        ASSERT_TRUE(errorMark.IsClean());
        HdResampleNeighbors(0.5f, VtValue(1.0), VtValue());
        ASSERT_TRUE(!errorMark.IsClean());
        errorMark.Clear();
    }

    //
    // HdResampleRawTimeSamples
    //
    {
        float times[] = {0.0f, 1.0f};
        float values[] = {0.0f, 256.0f};

        // Exact values at endpoints
        ASSERT_TRUE(HdResampleRawTimeSamples(0.0f, 2, times, values) == 0.0f);
        ASSERT_TRUE(HdResampleRawTimeSamples(1.0f, 2, times, values) == 256.0f);

        // Interpolation
        ASSERT_TRUE(HdResampleRawTimeSamples(0.25f, 2, times, values) > 63.0f);
        ASSERT_TRUE(HdResampleRawTimeSamples(0.25f, 2, times, values) < 65.0f);
        ASSERT_TRUE(HdResampleRawTimeSamples(0.50f, 2, times, values) > 127.0f);
        ASSERT_TRUE(HdResampleRawTimeSamples(0.50f, 2, times, values) < 129.0f);
        ASSERT_TRUE(HdResampleRawTimeSamples(0.75f, 2, times, values) > 191.0f);
        ASSERT_TRUE(HdResampleRawTimeSamples(0.75f, 2, times, values) < 193.0f);

        // Extrapolation -- this returns constant values outside the sample range.
        ASSERT_TRUE(HdResampleRawTimeSamples(-1.0f, 2, times, values) == 0.0f);
        ASSERT_TRUE(HdResampleRawTimeSamples(+2.0f, 2, times, values) == 256.0f);

        // Coding error with empty sample list
        ASSERT_TRUE(errorMark.IsClean());
        HdResampleRawTimeSamples(0.5f, 0, times, values);
        ASSERT_TRUE(!errorMark.IsClean());
        errorMark.Clear();
    }
}
