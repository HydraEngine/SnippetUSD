//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/bufferSpec.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/base/tf/errorMark.h"

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

void BufferSpecTest() {
    // test comparison operators
    {
        TF_VERIFY(HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}) ==
                  HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}));
        TF_VERIFY(HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}) !=
                  HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec4, 1}));
        TF_VERIFY(HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}) !=
                  HdBufferSpec(HdTokens->normals, HdTupleType{HdTypeFloatVec3, 1}));
        TF_VERIFY(HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}) !=
                  HdBufferSpec(HdTokens->points, HdTupleType{HdTypeDoubleVec3, 1}));

        TF_VERIFY(!(HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}) <
                    HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1})));
        TF_VERIFY(HdBufferSpec(HdTokens->normals, HdTupleType{HdTypeFloatVec3, 1}) <
                  HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}));
        TF_VERIFY(HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}) <
                  HdBufferSpec(HdTokens->points, HdTupleType{HdTypeDoubleVec3, 1}));
        TF_VERIFY(HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1}) <
                  HdBufferSpec(HdTokens->points, HdTupleType{HdTypeFloatVec4, 1}));
    }

    // test set operations
    {
        HdBufferSpecVector spec1;
        HdBufferSpecVector spec2;

        spec1.emplace_back(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1});
        spec1.emplace_back(HdTokens->displayColor, HdTupleType{HdTypeFloatVec3, 1});

        spec2.emplace_back(HdTokens->points, HdTupleType{HdTypeFloatVec3, 1});

        TF_VERIFY(HdBufferSpec::IsSubset(spec2, spec1) == true);
        TF_VERIFY(HdBufferSpec::IsSubset(spec1, spec2) == false);

        spec2.emplace_back(HdTokens->normals, HdTupleType{HdTypeFloatVec4, 1});

        TF_VERIFY(HdBufferSpec::IsSubset(spec2, spec1) == false);
        TF_VERIFY(HdBufferSpec::IsSubset(spec1, spec2) == false);

        HdBufferSpecVector spec3 = HdBufferSpec::ComputeUnion(spec1, spec2);

        TF_VERIFY(HdBufferSpec::IsSubset(spec1, spec3) == true);
        TF_VERIFY(HdBufferSpec::IsSubset(spec2, spec3) == true);
    }
}

TEST(TestHydra, test_buffer_spec) {
    TfErrorMark mark;

    BufferSpecTest();

    TF_VERIFY(mark.IsClean());
    ASSERT_TRUE(mark.IsClean());
}
