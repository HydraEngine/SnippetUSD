//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/retainedSceneIndex.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hdsi/computeSceneIndexDiff.h"

#include <iostream>
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

//-----------------------------------------------------------------------------

TEST(TestHydraSceneIndex, test_compute_scene_index_diff_delta) {
    HdRetainedSceneIndexRefPtr siA = HdRetainedSceneIndex::New();
    siA->AddPrims({
            {SdfPath("/Prim"), TfToken("A"), nullptr},
            {SdfPath("/Unchanged"), TfToken("A"), nullptr},
            {SdfPath("/Removed"), TfToken("A"), nullptr},
    });

    HdRetainedSceneIndexRefPtr siB = HdRetainedSceneIndex::New();
    siB->AddPrims({
            {SdfPath("/Prim"), TfToken("B"), nullptr},
            {SdfPath("/Unchanged"), TfToken("A"), nullptr},
    });

    HdSceneIndexObserver::RemovedPrimEntries removedEntries;
    HdSceneIndexObserver::AddedPrimEntries addedEntries;
    HdSceneIndexObserver::RenamedPrimEntries renamedEntries;
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    HdsiComputeSceneIndexDiffDelta(siA, siB, &removedEntries, &addedEntries, &renamedEntries, &dirtiedEntries);

    ASSERT_TRUE(addedEntries.size() == 1 && addedEntries[0].primPath == SdfPath("/Prim"));
    ASSERT_TRUE(removedEntries.size() == 1 && removedEntries[0].primPath == SdfPath("/Removed"));
}
