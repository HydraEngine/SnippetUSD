//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/unitTestHelper.h"

#include "pxr/base/tf/errorMark.h"

#include <iostream>
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

static void HdCommandBasicTest() {
    Hd_TestDriver driver;
    HdUnitTestDelegate& sceneDelegate = driver.GetDelegate();

    driver.Draw();

    HdRenderDelegate* renderDelegate = sceneDelegate.GetRenderIndex().GetRenderDelegate();

    if (!renderDelegate) {
        std::cout << "Failed to get a render delegate" << std::endl;
        GTEST_FAIL();
    }

    HdCommandDescriptors commands = renderDelegate->GetCommandDescriptors();

    if (commands.empty()) {
        std::cout << "Failed to get commands" << std::endl;
        GTEST_FAIL();
    }

    if (commands.size() == 1) {
        std::cout << "Got the following command: " << std::endl;
        std::cout << "    " << commands.front().commandName << std::endl;
    } else {
        std::cout << "Got the following commands: " << std::endl;
        for (const HdCommandDescriptor& cmd : commands) {
            std::cout << "    " << cmd.commandName << std::endl;
        }
    }
    std::cout << std::endl;

    // Try to invoke the print command
    HdCommandArgs args;
    args[TfToken("message")] = "Hello from test.";
    if (!renderDelegate->InvokeCommand(TfToken("print"), args)) {
        GTEST_FAIL();
    }
}

TEST(TestHydra, test_command) {
    TfErrorMark mark;

    HdCommandBasicTest();

    TF_VERIFY(mark.IsClean());
}
