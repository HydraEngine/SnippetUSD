//
// Copyright 2020 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/pxr.h"

#include "TestArURIResolver_plugin.h"

#include "pxr/usd/ar/defaultResolverContext.h"
#include "pxr/usd/ar/resolver.h"
#include "pxr/usd/ar/resolverContext.h"
#include "pxr/usd/ar/resolverContextBinder.h"

#include "pxr/base/arch/systemInfo.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/setenv.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/token.h"

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

class TestArUIRResolver : public testing::Test {
public:
    void SetUp() override {
        // Set the preferred resolver to ArDefaultResolver before
        // running any test cases.
        ArSetPreferredResolver("ArDefaultResolver");

        // Register TestArURIResolver plugin. We assume the build system will
        // install it to the ArPlugins subdirectory in the same location as
        // this test.
        const std::string uriResolverPluginPath = TfGetPathName(ArchGetExecutablePath());
        PlugPluginPtrVector plugins = PlugRegistry::GetInstance().RegisterPlugins(uriResolverPluginPath);

        const std::string packageResolverPluginPath =
                TfGetPathName(ArchGetExecutablePath()) + "../testArPackageResolver/";
        PlugRegistry::GetInstance().RegisterPlugins(packageResolverPluginPath);
    }
};

TEST_F(TestArUIRResolver, init) {
    ASSERT_TRUE(PlugRegistry::GetInstance().GetPluginWithName("TestArURIResolver"));
    ASSERT_TRUE(TfType::FindByName("_TestURIResolver"));
    ASSERT_TRUE(TfType::FindByName("_TestOtherURIResolver"));

    ASSERT_TRUE(PlugRegistry::GetInstance().GetPluginWithName("TestArPackageResolver"));
    ASSERT_TRUE(TfType::FindByName("_TestPackageResolver"));
}

TEST_F(TestArUIRResolver, resolve) {
    ArResolver& resolver = ArGetResolver();

    // The test URI resolver handles asset paths of the form "test:..."
    // and simply returns the path unchanged. We can use this to
    // verify that our test URI resolver is getting invoked.

    // These calls to Resolve should hit the default resolver and not
    // the URI resolver, and since these files don't exist we expect
    // Resolve would return ""
    ASSERT_TRUE(resolver.Resolve("doesnotexist") == "");
    ASSERT_TRUE(resolver.Resolve("doesnotexist.package[foo.file]") == "");

    // These calls should hit the URI resolver, which should return the
    // given paths unchanged.
    ASSERT_TRUE(resolver.Resolve("test://foo") == "test://foo");
    ASSERT_TRUE(resolver.Resolve("test://foo.package[bar.file]") == "test://foo.package[bar.file]");

    ASSERT_TRUE(resolver.Resolve("test-other://foo") == "test-other://foo");
    ASSERT_TRUE(resolver.Resolve("test-other://foo.package[bar.file]") == "test-other://foo.package[bar.file]");

    // These calls should hit the URI resolver since schemes are
    // case-insensitive.
    ASSERT_TRUE(resolver.Resolve("TEST://foo") == "TEST://foo");
    ASSERT_TRUE(resolver.Resolve("TEST://foo.package[bar.file]") == "TEST://foo.package[bar.file]");

    ASSERT_TRUE(resolver.Resolve("TEST-OTHER://foo") == "TEST-OTHER://foo");
    ASSERT_TRUE(resolver.Resolve("TEST-OTHER://foo.package[bar.file]") == "TEST-OTHER://foo.package[bar.file]");
}

TEST_F(TestArUIRResolver, invalidScheme) {
    ArResolver& resolver = ArGetResolver();
    auto invalid_underbar_path = "test_other:/abc.xyz";
    auto invalid_utf8_path = "test-π-utf8:/abc.xyz";
    auto invalid_numeric_prefix_path = "113-test:/abc.xyz";
    auto invalid_colon_path = "other:test:/abc.xyz";
    ASSERT_TRUE(!resolver.Resolve(invalid_underbar_path));
    ASSERT_TRUE(!resolver.Resolve(invalid_utf8_path));
    ASSERT_TRUE(!resolver.Resolve(invalid_numeric_prefix_path));
    ASSERT_TRUE(!resolver.Resolve(invalid_colon_path));
}

TEST_F(TestArUIRResolver, resolveWithContext) {
    ArResolver& resolver = ArGetResolver();

    // Verify that the context object is getting bound in the _TestURIResolver.
    // The test resolver simply appends the string in the context object
    // to the end of the given path when resolving.
    ArResolverContext ctx(_TestURIResolverContext("context"));
    ArResolverContextBinder binder(ctx);
    ASSERT_TRUE(resolver.Resolve("test://foo") == "test://foo?context");

    // Verify that binding another context overrides the previously-bound
    // context until the new binding is dropped.
    {
        ArResolverContext ctx2(_TestURIResolverContext("context2"));
        ArResolverContextBinder binder2(ctx2);
        ASSERT_TRUE(resolver.Resolve("test://foo") == "test://foo?context2");
    }
    ASSERT_TRUE(resolver.Resolve("test://foo") == "test://foo?context");

    // Verify that binding an unrelated context blocks the previously-bound
    // context.
    {
        ArResolverContext ctx3(ArDefaultResolverContext{});
        ArResolverContextBinder binder3(ctx3);
        ASSERT_TRUE(resolver.Resolve("test://foo") == "test://foo");
    }
    ASSERT_TRUE(resolver.Resolve("test://foo") == "test://foo?context");
}

TEST_F(TestArUIRResolver, createContextFromString) {
    ArResolver& resolver = ArGetResolver();

    const std::vector<std::string> searchPaths = {"/a", "/b"};
    const std::string searchPathStr = TfStringJoin(searchPaths, ARCH_PATH_LIST_SEP);

    // CreateContextFromString with an empty URI scheme should be
    // equivalent to CreateContextFromString without a URI scheme.
    ASSERT_TRUE(resolver.CreateContextFromString("", searchPathStr) ==
                ArResolverContext(ArDefaultResolverContext(searchPaths)));

    ASSERT_TRUE(resolver.CreateContextFromString("", searchPathStr) == resolver.CreateContextFromString(searchPathStr));

    // CreateContextFromString with a URI scheme that has no registered
    // resolver results in an empty ArResolverContext.
    ASSERT_TRUE(resolver.CreateContextFromString("bogus", "context string") == ArResolverContext());

    // CreateContextFromString with a URI scheme with a registered resolver
    // results in whatever context is returned from that resolver.
    ASSERT_TRUE(resolver.CreateContextFromString("test", "context string") ==
                ArResolverContext(_TestURIResolverContext("context string")));

    // CreateContextFromStrings should return a single ArResolverContext
    // containing context objects based on the given URI schemes and
    // context strings.
    ASSERT_TRUE(resolver.CreateContextFromStrings({{"test", "context string"}}) ==
                ArResolverContext(_TestURIResolverContext("context string")));

    ASSERT_TRUE(resolver.CreateContextFromStrings({{"", TfStringJoin(searchPaths, ARCH_PATH_LIST_SEP)},
                                                   {"test", "context string"},
                                                   {"bogus", "context string"}}) ==
                ArResolverContext(ArDefaultResolverContext(searchPaths), _TestURIResolverContext("context string")));
}

TEST_F(TestArUIRResolver, createDefaultContext) {
    ArResolver& resolver = ArGetResolver();

    // CreateDefaultContext should return an ArResolverContext containing
    // the union of the default contexts returned by all registered resolvers.
    // ArDefaultResolver returns an empty ArResolverContext object as its
    // default so we can't test for that, but TestArURIResolver returns a
    // _TestURIResolverContext which we can check for here.
    const ArResolverContext defaultContext = resolver.CreateDefaultContext();

    const auto* uriCtx = defaultContext.Get<_TestURIResolverContext>();
    ASSERT_TRUE(uriCtx);
    ASSERT_TRUE(uriCtx->data == "CreateDefaultContext");
}

TEST_F(TestArUIRResolver, createDefaultContextForAsset) {
    auto runTest = [](const std::string& assetPath) {
        ArResolver& resolver = ArGetResolver();

        // CreateDefaultContextForAsset should return an ArResolverContext
        // containing the union of the default contexts returned by all
        // registered resolvers.
        const ArResolverContext defaultContext = resolver.CreateDefaultContextForAsset(assetPath);

        // ArDefaultResolver returns an ArDefaultResolverContext whose search
        // path is set to the directory of the asset.
        {
            const auto* defaultCtx = defaultContext.Get<ArDefaultResolverContext>();
            ASSERT_TRUE(defaultCtx);

            const ArDefaultResolverContext expectedCtx(std::vector<std::string>{TfGetPathName(TfAbsPath(assetPath))});
            ASSERT_TRUE(*defaultCtx == expectedCtx);
        }

        // TestArURIResolver returns a _TestURIResolverContext whose data field
        // is set to the absolute path of the asset.
        {
            const auto* uriCtx = defaultContext.Get<_TestURIResolverContext>();
            ASSERT_TRUE(uriCtx);

            const _TestURIResolverContext expectedCtx(TfAbsPath("test/test.file"));
            ASSERT_TRUE(*uriCtx == expectedCtx);
        }
    };

    runTest("test/test.file");

    // For a package-relative path, CreateDefaultContextForAsset should only
    // consider the outer-most package path.
    runTest("test/test.file[in_package]");
}
