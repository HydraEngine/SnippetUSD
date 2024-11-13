//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"
#include "pxr/base/vt/value.h"

#include "pxr/base/tf/errorMark.h"

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

TEST(TestHydra, test_buffer_source_empty_val) {
    TfErrorMark mark;

    VtValue v;
    HdVtBufferSource b(HdTokens->points, v);

    // Above could throw errors.
    mark.Clear();

    bool valid = b.IsValid();

    std::cout << "Buffer is ";
    std::cout << (valid ? "Valid" : "Invalid");
    std::cout << "\n";

    ASSERT_TRUE(mark.IsClean() && (!valid));
}
