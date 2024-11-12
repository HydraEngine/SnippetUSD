//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include <gtest/gtest.h>
#include <pxr/base/gf/range2d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/camera.h>
#include <pxr/imaging/cameraUtil/screenWindowParameters.h>

using namespace pxr;

class TestCameraUtil : public testing::Test {
public:
    template <typename T>
    void IsClose(T a, T b) const {
        ASSERT_TRUE(GfIsClose(a, b, eps));
    }

    void IsClose(const GfMatrix4d& a, const GfMatrix4d& b) const {
        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(GfIsClose(a.GetRow(i), b.GetRow(i), eps));
        }
    }

    void IsClose(const GfRange2d& a, const GfRange2d& b) const {
        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(GfIsClose(a.GetMin(), b.GetMin(), eps));
            ASSERT_TRUE(GfIsClose(a.GetMax(), b.GetMax(), eps));
        }
    }

    float eps = 1e-5;
};

TEST_F(TestCameraUtil, test_ScreenWindowParameters) {
    auto cam = GfCamera();
    cam.SetProjection(GfCamera::Perspective);
    cam.SetHorizontalAperture(184.5);
    cam.SetHorizontalApertureOffset(15.45);
    cam.SetVerticalAperture(20.6);
    cam.SetFocalLength(10.8);
    cam.SetTransform(GfMatrix4d(0.890425533492, 0.433328071165, -0.13917310100, 0.0, -0.373912364534, 0.870830610429,
                                0.31912942765, 0.0, 0.259483935801, -0.232122447617, 0.93743653457, 0.0, 6.533573569142,
                                9.880622442086, 1.89848943302, 1.0));

    IsClose(CameraUtilScreenWindowParameters(cam).GetScreenWindow(),
            GfVec4d(-0.8325203582, 1.167479724, -0.1116531185, 0.1116531185));
    IsClose(CameraUtilScreenWindowParameters(cam).GetFieldOfView(), 166.645202637);
    IsClose(CameraUtilScreenWindowParameters(cam).GetZFacingViewMatrix(),
            GfMatrix4d(0.8904255335028, -0.3739123645233, -0.259483935838, 0, 0.4333280711640, 0.8708306104262,
                       0.232122447596, 0, -0.1391731009620, 0.3191294276581, -0.937436534593, 0, -9.8349931341753,
                       -6.7672283767831, 1.181556474823, 1));

    cam.SetProjection(GfCamera::Orthographic);
    IsClose(CameraUtilScreenWindowParameters(cam).GetScreenWindow(),
            GfVec4d(-7.6800003051, 10.770000457, -1.0300000190, 1.0300000190));
}