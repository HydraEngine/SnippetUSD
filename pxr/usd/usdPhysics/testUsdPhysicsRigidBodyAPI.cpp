//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include <gtest/gtest.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/transform.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdPhysics/metrics.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdPhysics/collisionAPI.h>
#include <pxr/usd/usd/stage.h>

using namespace pxr;

class TestUsdPhysicsRigidBodyAPI : public testing::Test {
public:
    void setup_scene(float metersPerUnit = 1.0, float kilogramsPerUnit = 1.0) {
        stage = UsdStage::CreateInMemory();

        // setup stage units
        UsdGeomSetStageUpAxis(stage, TfToken("Z"));
        UsdGeomSetStageMetersPerUnit(stage, metersPerUnit);
        UsdPhysicsSetStageKilogramsPerUnit(stage, kilogramsPerUnit);
    }

    static std::pair<GfVec3d, GfQuatd> get_collision_shape_local_transform(GfMatrix4d collisionLocalToWorld,
                                                                           GfMatrix4d bodyLocalToWorld) {
        auto mat = collisionLocalToWorld * bodyLocalToWorld.GetInverse();
        auto colLocalTransform = GfTransform(mat);

        auto localPos = GfVec3d(0.0);
        auto localRotOut = GfQuatd(1.0);

        localPos = colLocalTransform.GetTranslation();
        localRotOut = colLocalTransform.GetRotation().GetQuat();

        // now apply the body scale to localPos
        // physics does not support scales, so a rigid body scale has to be baked into the localPos
        auto tr = GfTransform(bodyLocalToWorld);
        auto sc = tr.GetScale();

        localPos[0] = localPos[0] * sc[0];
        localPos[1] = localPos[1] * sc[1];
        localPos[2] = localPos[2] * sc[2];

        return std::make_pair(localPos, localRotOut);
    }

    UsdPhysicsRigidBodyAPI::MassInformation mass_information_fn(const UsdPrim& prim) const {
        auto massInfo = UsdPhysicsRigidBodyAPI::MassInformation();
        if (prim.IsA<UsdGeomCube>()) {
            auto cubeLocalToWorldTransform =
                    UsdGeomXformable(prim).ComputeLocalToWorldTransform(UsdTimeCode::Default());
            auto extents = GfTransform(cubeLocalToWorldTransform).GetScale();

            auto cube = UsdGeomCube(prim);
            double sizeAttr;
            cube.GetSizeAttr().Get(&sizeAttr);
            sizeAttr = abs(sizeAttr);
            extents = extents * sizeAttr;

            // cube volume
            massInfo.volume = extents[0] * extents[1] * extents[2];

            // cube inertia
            auto inertia_diagonal = GfVec3f(1.0 / 12.0 * (extents[1] * extents[1] + extents[2] * extents[2]),
                                            1.0 / 12.0 * (extents[0] * extents[0] + extents[2] * extents[2]),
                                            1.0 / 12.0 * (extents[0] * extents[0] + extents[1] * extents[1]));
            massInfo.inertia = GfMatrix3f(1.0f);
            massInfo.inertia.SetDiagonal(inertia_diagonal);

            // CoM
            massInfo.centerOfMass = GfVec3f(0.0);

            // local pose
            if (prim == rigidBodyPrim) {
                massInfo.localPos = GfVec3f(0.0);
                massInfo.localRot = GfQuatf(1.0);
            } else {
                // massInfo.localPos, massInfo.localRot
                GfVec3d lp{};
                GfQuatd lr;
                std::tie(lp, lr) =
                        get_collision_shape_local_transform(cubeLocalToWorldTransform, rigidBodyWorldTransform);
                massInfo.localPos = GfVec3f(lp);
                massInfo.localRot = GfQuatf(lr);
            }
        } else {
            printf("UsdGeom type not supported.");
            massInfo.volume = -1.0;
        }
        return massInfo;
    }

    void compare_mass_information(const UsdPhysicsRigidBodyAPI& rigidBodyAPI,
                                  float expectedMass,
                                  std::optional<GfVec3f> expectedInertia = std::nullopt,
                                  std::optional<GfVec3f> expectedCoM = std::nullopt,
                                  std::optional<GfQuatf> expectedPrincipalAxes = std::nullopt) const {
        GfVec3f inertia{};
        GfVec3f centerOfMass{};
        GfQuatf principalAxes;
        auto mass = rigidBodyAPI.ComputeMassProperties(
                &inertia, &centerOfMass, &principalAxes,
                [this](const UsdPrim& prim) -> UsdPhysicsRigidBodyAPI::MassInformation {
                    return mass_information_fn(prim);
                });

        float toleranceEpsilon = 0.01;

        ASSERT_TRUE(abs(mass - expectedMass) < toleranceEpsilon);

        if (expectedCoM.has_value()) {
            ASSERT_TRUE(GfIsClose(centerOfMass, expectedCoM.value(), toleranceEpsilon));
        }

        if (expectedInertia.has_value()) {
            ASSERT_TRUE(GfIsClose(inertia, expectedInertia.value(), toleranceEpsilon));
        }

        if (expectedPrincipalAxes.has_value()) {
            ASSERT_TRUE(GfIsClose(principalAxes.GetImaginary(), expectedPrincipalAxes.value().GetImaginary(),
                                  toleranceEpsilon));
            ASSERT_TRUE(abs(principalAxes.GetReal() - expectedPrincipalAxes.value().GetReal()) < toleranceEpsilon);
        }
    }
    UsdStageRefPtr stage;
    UsdPrim rigidBodyPrim;
    GfMatrix4d rigidBodyWorldTransform{};
};

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube) {
    setup_scene();

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(cube.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = cube.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0, GfVec3f(166.667), GfVec3f(0.0));
}