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

TEST_F(TestUsdPhysicsCollisionGroupAPI, test_collision_group_inversion) {
    auto stage = UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);

    auto a = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/a"));
    auto b = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/b"));
    auto c = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/c"));

    a.CreateFilteredGroupsRel().AddTarget(c.GetPath());
    a.CreateInvertFilteredGroupsAttr().Set(true);

    auto table = UsdPhysicsCollisionGroup::ComputeCollisionGroupTable(*stage);

    // A should collide with only C
    // B should collide with only B and C
    // C should collide with only B and C
    ASSERT_FALSE(table.IsCollisionEnabled(a.GetPrim().GetPath(), a.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(a.GetPrim().GetPath(), b.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(a.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(b.GetPrim().GetPath(), b.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(b.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(c.GetPrim().GetPath(), c.GetPrim().GetPath()));
    validate_table_symmetry(table);

    // Explicitly test the inversion scenario which may "re-enable" a
    // collision filter pair that has been disabled (refer docs on why care
    // should be taken to avoid such scenarios)
    auto allOthers = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/allOthers"));

    // - grpX is set to ONLY collide with grpXCollider by setting an inversion
    auto grpXCollider = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/grpXCollider"));
    auto grpX = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/grpX"));
    grpX.CreateFilteredGroupsRel().AddTarget(grpXCollider.GetPath());
    grpX.CreateInvertFilteredGroupsAttr().Set(true);
    table = UsdPhysicsCollisionGroup::ComputeCollisionGroupTable(*stage);
    ASSERT_TRUE(table.IsCollisionEnabled(grpX.GetPrim().GetPath(), grpXCollider.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(grpX.GetPrim().GetPath(), allOthers.GetPrim().GetPath()));

    // - grpX is added to a new merge group "mergetest"
    grpX.CreateMergeGroupNameAttr().Set("mergeTest");

    // - grpA now creates a filter to disable its collision with grpXCollider
    auto grpA = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/grpA"));
    grpA.CreateFilteredGroupsRel().AddTarget(grpXCollider.GetPath());
    table = UsdPhysicsCollisionGroup::ComputeCollisionGroupTable(*stage);
    ASSERT_FALSE(table.IsCollisionEnabled(grpA.GetPrim().GetPath(), grpXCollider.GetPrim().GetPath()));
    // - above doesn't affect any of grpX's collision pairs
    ASSERT_TRUE(table.IsCollisionEnabled(grpX.GetPrim().GetPath(), grpXCollider.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(grpX.GetPrim().GetPath(), allOthers.GetPrim().GetPath()));

    // - grpA is now added to same "mergetest" merge group (care was not
    // taken in doing so and this disables all collision pairs!!)
    grpA.CreateMergeGroupNameAttr().Set("mergeTest");
    table = UsdPhysicsCollisionGroup::ComputeCollisionGroupTable(*stage);
    ASSERT_FALSE(table.IsCollisionEnabled(grpX.GetPrim().GetPath(), grpXCollider.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(grpX.GetPrim().GetPath(), allOthers.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(grpA.GetPrim().GetPath(), grpXCollider.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(grpA.GetPrim().GetPath(), allOthers.GetPrim().GetPath()));
}

TEST_F(TestUsdPhysicsCollisionGroupAPI, test_collision_group_simple_merging) {
    auto stage = UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);

    auto a = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/a"));
    auto b = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/b"));
    auto c = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/c"));

    a.CreateFilteredGroupsRel().AddTarget(c.GetPath());
    // Assign A and B to the same merge group:
    a.CreateMergeGroupNameAttr().Set("mergeTest");
    b.CreateMergeGroupNameAttr().Set("mergeTest");

    auto table = UsdPhysicsCollisionGroup::ComputeCollisionGroupTable(*stage);

    // A should collide with only A and B
    // B should collide with only A and B
    // C should collide with only C
    ASSERT_TRUE(table.IsCollisionEnabled(a.GetPrim().GetPath(), a.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(a.GetPrim().GetPath(), b.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(a.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(b.GetPrim().GetPath(), b.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(b.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(c.GetPrim().GetPath(), c.GetPrim().GetPath()));
    validate_table_symmetry(table);
}

TEST_F(TestUsdPhysicsCollisionGroupAPI, test_collision_group_complex_merging) {
    auto stage = UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);

    auto a = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/a"));
    auto b = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/b"));
    auto c = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/c"));
    auto d = UsdPhysicsCollisionGroup::Define(stage, SdfPath("/d"));

    a.CreateFilteredGroupsRel().AddTarget(c.GetPath());
    // Assign A and B to the same merge group:
    a.CreateMergeGroupNameAttr().Set("mergeAB");
    b.CreateMergeGroupNameAttr().Set("mergeAB");
    // Assign C and D to the same merge group:
    c.CreateMergeGroupNameAttr().Set("mergeCD");
    d.CreateMergeGroupNameAttr().Set("mergeCD");

    auto table = UsdPhysicsCollisionGroup::ComputeCollisionGroupTable(*stage);

    // A should collide with only A and B
    // B should collide with only A and B
    // C should collide with only C and D
    // D should collide with only C and D
    ASSERT_TRUE(table.IsCollisionEnabled(a.GetPrim().GetPath(), a.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(a.GetPrim().GetPath(), b.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(a.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(a.GetPrim().GetPath(), d.GetPrim().GetPath()));

    ASSERT_TRUE(table.IsCollisionEnabled(b.GetPrim().GetPath(), b.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(b.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_FALSE(table.IsCollisionEnabled(b.GetPrim().GetPath(), d.GetPrim().GetPath()));

    ASSERT_TRUE(table.IsCollisionEnabled(c.GetPrim().GetPath(), c.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(c.GetPrim().GetPath(), d.GetPrim().GetPath()));
    ASSERT_TRUE(table.IsCollisionEnabled(d.GetPrim().GetPath(), d.GetPrim().GetPath()));
    validate_table_symmetry(table);
}