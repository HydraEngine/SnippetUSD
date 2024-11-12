//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/pxr.h"
#include "testArchAbi.h"
#include "pxr/base/arch/library.h"
#include "pxr/base/arch/systemInfo.h"

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

typedef ArchAbiBase2* (*NewDerived)();

TEST(TestArch, ABI) {
    // Compute the plugin directory.
    std::string path = ArchGetExecutablePath();
    // Get directories.
    path = path.substr(0, path.find_last_of("/\\"));

    // Load the plugin and get the factory function.
#if defined(ARCH_OS_WINDOWS)
    path += "\\lib\\testArchAbiPlugin.dll";
#elif defined(ARCH_OS_DARWIN)
    path += "/libTestArchAbiPlugin.dylib";
#else
    path += "/lib/libtestArchAbiPlugin.so";
#endif
    auto plugin = ArchLibraryOpen(path, ARCH_LIBRARY_LAZY);
    if (!plugin) {
        std::string error = ArchLibraryError();
        std::cerr << "Failed to load plugin: " << error << std::endl;
        ASSERT_TRUE(plugin);
    }

    auto newPluginDerived = (NewDerived)ArchLibraryGetSymbolAddress(plugin, "newDerived");
    if (!newPluginDerived) {
        std::cerr << "Failed to find factory symbol" << std::endl;
        ASSERT_TRUE(newPluginDerived);
    }

    // Create a derived object in this executable and in the plugin.
    ArchAbiBase2* mainDerived = new ArchAbiDerived<int>;
    ArchAbiBase2* pluginDerived = newPluginDerived();

    // Compare.  The types should be equal and the dynamic cast should not
    // change the pointer.
    std::cout << "Derived types are equal: " << ((typeid(*mainDerived) == typeid(*pluginDerived)) ? "yes" : "no")
              << ", cast: " << pluginDerived << "->" << dynamic_cast<ArchAbiDerived<int>*>(pluginDerived) << std::endl;
    ASSERT_EQ(typeid(*mainDerived), typeid(*pluginDerived));
    ASSERT_EQ(pluginDerived, dynamic_cast<ArchAbiDerived<int>*>(pluginDerived));
}
