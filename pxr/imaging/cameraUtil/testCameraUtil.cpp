//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include <gtest/gtest.h>
#include <pxr/base/gf/range2d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/imaging/cameraUtil/screenWindowParameters.h>
#include <pxr/imaging/cameraUtil/conformWindow.h>

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

TEST_F(TestCameraUtil, test_ConformedWindowGfVec2d) {
    IsClose(CameraUtilConformedWindow(GfVec2d(1.0, 2.0), CameraUtilFit, 3.0), GfVec2d(6.0, 2.0));
    IsClose(CameraUtilConformedWindow(GfVec2d(9.0, 2.0), CameraUtilFit, 3.0), GfVec2d(9.0, 3.0));
    IsClose(CameraUtilConformedWindow(GfVec2d(3.3, 4.0), CameraUtilCrop, 1.5), GfVec2d(3.3, 2.2));
    IsClose(CameraUtilConformedWindow(GfVec2d(10.0, 2.0), CameraUtilCrop, 4), GfVec2d(8.0, 2.0));
    IsClose(CameraUtilConformedWindow(GfVec2d(0.1, 2.0), CameraUtilCrop, 0.1), GfVec2d(0.1, 1.0));
    IsClose(CameraUtilConformedWindow(GfVec2d(2.0, 1.9), CameraUtilMatchVertically, 2.0), GfVec2d(3.8, 1.9));
    IsClose(CameraUtilConformedWindow(GfVec2d(2.1, 1.9), CameraUtilMatchHorizontally, 1.0), GfVec2d(2.1, 2.1));
    IsClose(CameraUtilConformedWindow(GfVec2d(2.1, 1.9), CameraUtilDontConform, 1.0), GfVec2d(2.1, 1.9));
}

TEST_F(TestCameraUtil, test_ConformedWindowGfRange2d) {
    IsClose(CameraUtilConformedWindow(GfRange2d(GfVec2d(-8, -6), GfVec2d(-4, -2)), CameraUtilFit, 3.0),
            GfRange2d(GfVec2d(-12, -6), GfVec2d(0, -2)));
    IsClose(CameraUtilConformedWindow(GfRange2d(GfVec2d(-10, -11), GfVec2d(-1, -1)), CameraUtilMatchHorizontally, 1.5),
            GfRange2d(GfVec2d(-10, -9), GfVec2d(-1, -3)));
    IsClose(CameraUtilConformedWindow(GfRange2d(GfVec2d(-10, -11), GfVec2d(-1, -1)), CameraUtilMatchVertically, 1.5),
            GfRange2d(GfVec2d(-13, -11), GfVec2d(2, -1)));
    IsClose(CameraUtilConformedWindow(GfRange2d(GfVec2d(-10, -11), GfVec2d(-1, -1)), CameraUtilDontConform, 1.5),
            GfRange2d(GfVec2d(-10, -11), GfVec2d(-1, -1)));
}

TEST_F(TestCameraUtil, test_ConformedWindowGfVec4d) {
    IsClose(CameraUtilConformedWindow(GfVec4d(-10, -1, -11, -1), CameraUtilMatchHorizontally, 1.5),
            GfVec4d(-10, -1, -9, -3));
}

TEST_F(TestCameraUtil, test_ConformProjectionMatrix) {
    for (int i = 0; i < 2; ++i) {
        auto projection = GfCamera::Projection(i);
        for (int j = 0; j < 5; ++j) {
            auto policy = CameraUtilConformWindowPolicy(j);
            for (auto targetAspect : {0.5, 1.0, 2.0}) {
                for (auto xMirror : {-1, 1}) {
                    for (auto yMirror : {-1, 1}) {
                        auto mirrorMatrix = GfMatrix4d(xMirror, 0, 0, 0, 0, yMirror, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
                        auto cam = GfCamera(GfMatrix4d(1.0), projection, 100.0, 75.0, 11.0, 12.0);
                        auto originalMatrix = cam.GetFrustum().ComputeProjectionMatrix();
                        CameraUtilConformWindow(&cam, policy, targetAspect);

                        IsClose(cam.GetFrustum().ComputeProjectionMatrix() * mirrorMatrix,
                                CameraUtilConformedWindow(originalMatrix * mirrorMatrix, policy, targetAspect));
                    }
                }
            }
        }
    }
}

TEST_F(TestCameraUtil, test_ConformWindow) {
    auto cam = GfCamera();
    cam.SetHorizontalAperture(100.0);
    cam.SetVerticalAperture(75.0);
    cam.SetHorizontalApertureOffset(11.0);
    cam.SetVerticalApertureOffset(12.0);

    CameraUtilConformWindow(&cam, CameraUtilFit, 2.0);

    IsClose(cam.GetHorizontalAperture(), 150.0f);
    IsClose(cam.GetVerticalAperture(), 75.0f);
    IsClose(cam.GetHorizontalApertureOffset(), 11.0f);
    IsClose(cam.GetVerticalApertureOffset(), 12.0f);

    CameraUtilConformWindow(&cam, CameraUtilFit, 1.5);

    IsClose(cam.GetHorizontalAperture(), 150.0f);
    IsClose(cam.GetVerticalAperture(), 100.0f);
    IsClose(cam.GetHorizontalApertureOffset(), 11.0f);
    IsClose(cam.GetVerticalApertureOffset(), 12.0f);
}

TEST_F(TestCameraUtil, test_ConformFrustum) {
    auto frustum = GfFrustum();
    frustum.SetWindow(GfRange2d(GfVec2d(-1.2, -1.0), GfVec2d(1.0, 1.5)));

    CameraUtilConformWindow(&frustum, CameraUtilCrop, 1.3333);

    IsClose(frustum.GetWindow().GetMin(), GfVec2d(-1.2, -0.575020625515638));
    IsClose(frustum.GetWindow().GetMax(), GfVec2d(1.0, 1.075020625515638));

    frustum.SetWindow(GfRange2d(GfVec2d(-1.2, -1.0), GfVec2d(1.0, 1.5)));
    CameraUtilConformWindow(&frustum, CameraUtilDontConform, 1.3333);
    IsClose(frustum.GetWindow().GetMin(), GfVec2d(-1.2, -1.0));
    IsClose(frustum.GetWindow().GetMax(), GfVec2d(1.0, 1.5));
}