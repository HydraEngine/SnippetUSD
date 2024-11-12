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
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/tokens.h>
#include <pxr/usd/usdPhysics/metrics.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdPhysics/collisionAPI.h>
#include <pxr/usd/usdPhysics/massAPI.h>
#include <pxr/usd/usdPhysics/materialAPI.h>
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

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_rigid_body_density) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());

    // set half the default gravity
    massAPI.GetDensityAttr().Set(500.0f);

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 500.0, GfVec3f(166.667 * 0.5), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_collider_density) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(cube.GetPrim());

    // set half the default gravity
    massAPI.GetDensityAttr().Set(500.0f);

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 500.0, GfVec3f(166.667 * 0.5), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_material_density) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());

    // Create base physics material
    auto basePhysicsMaterial = UsdShadeMaterial::Define(stage, SdfPath("/basePhysicsMaterial"));
    auto materialAPI = UsdPhysicsMaterialAPI::Apply(basePhysicsMaterial.GetPrim());
    // set half the default gravity
    materialAPI.GetDensityAttr().Set(500.0f);
    auto bindingAPI = UsdShadeMaterialBindingAPI::Apply(cube.GetPrim());
    bindingAPI.Bind(basePhysicsMaterial, UsdShadeTokens->weakerThanDescendants, TfToken("physics"));

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 500.0, GfVec3f(166.667 * 0.5), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_density_precedence) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());
    massAPI.GetDensityAttr().Set(5000.0f);

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    massAPI = UsdPhysicsMassAPI::Apply(cube.GetPrim());

    // collision density does have precedence
    massAPI.GetDensityAttr().Set(500.0f);

    // Create base physics material
    auto basePhysicsMaterial = UsdShadeMaterial::Define(stage, SdfPath("/basePhysicsMaterial"));
    auto materialAPI = UsdPhysicsMaterialAPI::Apply(basePhysicsMaterial.GetPrim());
    // set half the default gravity
    materialAPI.GetDensityAttr().Set(2000.0f);
    auto bindingAPI = UsdShadeMaterialBindingAPI::Apply(cube.GetPrim());
    bindingAPI.Bind(basePhysicsMaterial, UsdShadeTokens->weakerThanDescendants, TfToken("physics"));

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 500.0, GfVec3f(166.667 * 0.5), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_rigid_body_mass) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());

    // increase the mass twice
    massAPI.GetMassAttr().Set(2000.0f);

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 2000.0, GfVec3f(166.667 * 2.0), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_collider_mass) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(cube.GetPrim());

    // increase the mass twice
    massAPI.GetMassAttr().Set(2000.0f);

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 2000.0, GfVec3f(166.667 * 2.0), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_mass_precedence) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());

    // increase the mass twice (has a precedence)
    massAPI.GetMassAttr().Set(2000.0f);

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    massAPI = UsdPhysicsMassAPI::Apply(cube.GetPrim());

    // increase the mass twice
    massAPI.GetMassAttr().Set(500.0f);

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 2000.0, GfVec3f(166.667 * 2.0), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_rigid_body_com) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());

    // custom CoM
    massAPI.GetCenterOfMassAttr().Set(GfVec3f(2.0));

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0, std::nullopt, GfVec3f(2.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_collider_com) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(cube.GetPrim());

    // custom CoM
    massAPI.GetCenterOfMassAttr().Set(GfVec3f(2.0));

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0, std::nullopt, GfVec3f(2.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_com_precedence) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());

    // custom CoM (has precedence)
    massAPI.GetCenterOfMassAttr().Set(GfVec3f(2.0));

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    massAPI = UsdPhysicsMassAPI::Apply(cube.GetPrim());

    // custom CoM
    massAPI.GetCenterOfMassAttr().Set(GfVec3f(1.0));

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0, std::nullopt, GfVec3f(2.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_rigid_body_inertia) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());

    // custom inertia
    massAPI.GetDiagonalInertiaAttr().Set(GfVec3f(2.0));

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0, std::nullopt, GfVec3f(2.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_collider_inertia) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(cube.GetPrim());

    // custom inertia
    massAPI.GetDiagonalInertiaAttr().Set(GfVec3f(2.0));

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0, std::nullopt, GfVec3f(2.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_inertia_precedence) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());

    // custom inertia (has precedence)
    massAPI.GetDiagonalInertiaAttr().Set(GfVec3f(2.0));

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    massAPI = UsdPhysicsMassAPI::Apply(cube.GetPrim());

    // custom inertia
    massAPI.GetDiagonalInertiaAttr().Set(GfVec3f(1.0));

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0, std::nullopt, GfVec3f(2.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_rigid_body_compound) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());

    auto size = 1.0;
    auto scale = GfVec3f(3.0, 2.0, 3.0);

    // Create test collider cube0
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube0"));
    cube.CreateSizeAttr(VtValue(size));
    cube.AddTranslateOp().Set(GfVec3d(100.0, 20.0, 10.0));
    auto cube0RotateOp = cube.AddRotateXYZOp();
    cube0RotateOp.Set(GfVec3f(0.0, 0.0, 45.0));
    cube.AddScaleOp().Set(scale);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());

    // Create test collider cube1
    cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube1"));
    cube.CreateSizeAttr(VtValue(size));
    cube.AddTranslateOp().Set(GfVec3d(-100.0, 20.0, 10.0));
    auto cube1RotateOp = cube.AddRotateXYZOp();
    cube1RotateOp.Set(GfVec3f(0.0, 0.0, 45.0));
    cube.AddScaleOp().Set(scale);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    GfVec3f inertia_compare{};
    GfVec3f centerOfMass{};
    GfQuatf principalAxes;
    auto mass = rigidBodyAPI.ComputeMassProperties(&inertia_compare, &centerOfMass, &principalAxes,
                                                   [this](const UsdPrim& prim) {
                                                       return mass_information_fn(prim);
                                                   });

    cube0RotateOp.Set(GfVec3f(0.0, 90.0, 45.0));
    cube1RotateOp.Set(GfVec3f(0.0, 0.0, 45.0));

    GfVec3f inertia{};
    mass = rigidBodyAPI.ComputeMassProperties(&inertia, &centerOfMass, &principalAxes, [this](const UsdPrim& prim) {
        return mass_information_fn(prim);
    });

    auto toleranceEpsilon = 1;
    ASSERT_TRUE(GfIsClose(inertia, inertia_compare, toleranceEpsilon));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_rigid_body_principal_axes) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());
    auto massAPI = UsdPhysicsMassAPI::Apply(xform.GetPrim());

    // custom principal axes
    massAPI.GetPrincipalAxesAttr().Set(GfQuatf(0.707, 0.0, 0.707, 0.0));

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0, std::nullopt, std::nullopt, GfQuatf(0.707, 0.0, 0.707, 0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_cm_units) {
    setup_scene(0.01);

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(cube.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = cube.GetPrim();

    float massScale = 0.01 * 0.01 * 0.01;
    compare_mass_information(rigidBodyAPI, 1000.0f * massScale, GfVec3f(166.667f * massScale), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_decagram_units) {
    setup_scene(1.0, 0.1);

    // Create test collider cube
    auto cube = UsdGeomCube::Define(stage, SdfPath("/cube"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(cube.GetPrim());

    rigidBodyWorldTransform = UsdGeomXformable(cube.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = cube.GetPrim();

    float massScale = 1.0 / 0.1;
    compare_mass_information(rigidBodyAPI, 1000.0f * massScale, GfVec3f(166.667f * massScale), GfVec3f(0.0));
}

TEST_F(TestUsdPhysicsRigidBodyAPI, test_mass_rigid_body_cube_compound) {
    setup_scene();

    // top level xform - rigid body
    auto xform = UsdGeomXform::Define(stage, SdfPath("/xform"));
    auto rigidBodyAPI = UsdPhysicsRigidBodyAPI::Apply(xform.GetPrim());

    // Create test collider cube0
    auto cube = UsdGeomCube::Define(stage, SdfPath("/xform/cube0"));
    cube.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    cube.AddTranslateOp().Set(GfVec3d(0, 0, -2.0));

    // Create test collision
    auto cube2 = UsdGeomCube::Define(stage, SdfPath("/xform/cube1"));
    cube2.GetSizeAttr().Set(1.0);
    UsdPhysicsCollisionAPI::Apply(cube2.GetPrim());
    cube2.AddTranslateOp().Set(GfVec3d(0, 0, 2.0));

    rigidBodyWorldTransform = UsdGeomXformable(xform.GetPrim()).ComputeLocalToWorldTransform(UsdTimeCode::Default());
    rigidBodyPrim = xform.GetPrim();

    compare_mass_information(rigidBodyAPI, 1000.0 * 2.0, std::nullopt, GfVec3f(0.0));
}