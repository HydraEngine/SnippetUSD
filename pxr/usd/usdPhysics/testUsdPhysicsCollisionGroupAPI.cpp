//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include <gtest/gtest.h>
#include <pxr/usd/usdPhysics/collisionGroup.h>

using namespace pxr;

class TestUsdPhysicsCollisionGroupAPI : public testing::Test {
public:
    static void validate_table_symmetry(UsdPhysicsCollisionGroup::CollisionGroupTable& table) {
        for (int iA = 0; iA < table.GetCollisionGroups().size(); ++iA) {
            auto a = table.GetCollisionGroups()[iA];
            for (int iB = 0; iB < table.GetCollisionGroups().size(); ++iB) {
                auto b = table.GetCollisionGroups()[iB];
                ASSERT_EQ(table.IsCollisionEnabled(iA, iB), table.IsCollisionEnabled(iB, iA));
                ASSERT_EQ(table.IsCollisionEnabled(a, b), table.IsCollisionEnabled(b, a));
                ASSERT_EQ(table.IsCollisionEnabled(a, b), table.IsCollisionEnabled(iA, iB));
            }
        }
    }
};

TEST_F(TestUsdPhysicsCollisionGroupAPI, test_collision_group_table) {
    auto stage = UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);

    auto a = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/a"));
    auto b = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/b"));
    auto c = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/c"));

    b.CreateFilteredGroupsRel().AddTarget(c.GetPath());
    c.CreateFilteredGroupsRel().AddTarget(c.GetPath());

    auto table = UsdPhysicsCollisionGroup::ComputeCollisionGroupTable(*stage);

    // Check the results contain all the groups:
    ASSERT_TRUE(table.GetCollisionGroups().size() == 3);

    ASSERT_TRUE(
            std::count(table.GetCollisionGroups().begin(), table.GetCollisionGroups().end(), a.GetPrim().GetPath()));
    ASSERT_TRUE(
            std::count(table.GetCollisionGroups().begin(), table.GetCollisionGroups().end(), b.GetPrim().GetPath()));
    ASSERT_TRUE(
            std::count(table.GetCollisionGroups().begin(), table.GetCollisionGroups().end(), c.GetPrim().GetPath()));

    // A should collide with everything
    // B should only collide with A and B
    // C should only collide with A
    ASSERT_TRUE(table.IsCollisionEnabled(a.GetPrim().GetPath(), a.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(a.GetPrim().GetPath(), b.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(a.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(b.GetPrim().GetPath(), b.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(b.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(c.GetPrim().GetPath(), c.GetPrim().GetPath()));
    validate_table_symmetry(table);
}