//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/retainedSceneIndex.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hdsi/switchingSceneIndex.h"

#include <iostream>
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

//-----------------------------------------------------------------------------
TEST(TestHydraSceneIndex, test_switching_scene_index) {
    HdRetainedSceneIndexRefPtr siA = HdRetainedSceneIndex::New();
    siA->AddPrims({
            {SdfPath("/Prim"), TfToken("A"), nullptr},
    });

    HdRetainedSceneIndexRefPtr siB = HdRetainedSceneIndex::New();
    siB->AddPrims({
            {SdfPath("/Prim"), TfToken("B"), nullptr},
    });

    auto switchingSi = HdsiSwitchingSceneIndex::New({siA, siB});
    ASSERT_TRUE(switchingSi->GetPrim(SdfPath("/Prim")).primType == TfToken("A"));

    switchingSi->SetIndex(1);
    ASSERT_TRUE(switchingSi->GetPrim(SdfPath("/Prim")).primType == TfToken("B"));
}
