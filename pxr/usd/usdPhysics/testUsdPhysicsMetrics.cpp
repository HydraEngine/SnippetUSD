//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include <gtest/gtest.h>
#include <pxr/usd/usdPhysics/metrics.h>
#include <pxr/usd/usd/stage.h>

using namespace pxr;

TEST(USDPhysics, test_kilogramsPerUnit) {
    auto stage = UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);

    ASSERT_EQ(UsdPhysicsGetStageKilogramsPerUnit(stage), UsdPhysicsMassUnits::kilograms);
    ASSERT_FALSE(UsdPhysicsStageHasAuthoredKilogramsPerUnit(stage));

    ASSERT_TRUE(UsdPhysicsSetStageKilogramsPerUnit(stage, UsdPhysicsMassUnits::grams));
    ASSERT_TRUE(UsdPhysicsStageHasAuthoredKilogramsPerUnit(stage));
    auto authored = UsdPhysicsGetStageKilogramsPerUnit(stage);
    ASSERT_TRUE(UsdPhysicsMassUnitsAre(authored, UsdPhysicsMassUnits::grams));
}