//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/containerDataSourceEditor.h"
#include "pxr/imaging/hd/overlayContainerDataSource.h"
#include "pxr/imaging/hd/retainedDataSource.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>

#include <gtest/gtest.h>

using namespace pxr;

// ----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& stream, HdContainerDataSourceHandle const& container) {
    HdDebugPrintDataSource(stream, container);
    return stream;
}

#define COMPAREVALUE(T, A, B)                                                             \
    if (A == B) {                                                                         \
        std::cout << T << " matches." << std::endl;                                       \
    } else {                                                                              \
        std::cerr << T << " doesn't match. Expecting " << B << " got " << A << std::endl; \
    }

#define COMPARECONTAINERS(T, A, B)                    \
    {                                                 \
        std::ostringstream aStream;                   \
        aStream << std::endl;                         \
        HdDebugPrintDataSource(aStream, A);           \
        std::ostringstream bStream;                   \
        bStream << std::endl;                         \
        HdDebugPrintDataSource(bStream, B);           \
        COMPAREVALUE(T, aStream.str(), bStream.str()) \
    }

// test brevity conveniences

#define I(v) (HdRetainedTypedSampledDataSource<int>::New(v))

HdDataSourceLocator L(const std::string& inputStr) {
    std::vector<TfToken> tokens;
    for (const std::string& s : TfStringSplit(inputStr, "/")) {
        if (!s.empty()) {
            tokens.emplace_back(s);
        }
    }
    return {tokens.size(), tokens.data()};
}

// ----------------------------------------------------------------------------

TEST(TestHydra, test_simple_overlay) {
    HdContainerDataSourceHandle containers[] = {
            HdRetainedContainerDataSource::New(TfToken("A"), I(1), TfToken("F"), I(7)),

            HdRetainedContainerDataSource::New(TfToken("B"), I(2), TfToken("C"), I(3)),

            HdRetainedContainerDataSource::New(TfToken("D"), HdRetainedContainerDataSource::New(TfToken("E"), I(4)),
                                               TfToken("F"), I(6), TfToken("G"), I(8)),
    };

    HdOverlayContainerDataSourceHandle test = HdOverlayContainerDataSource::New(3, containers);

    HdContainerDataSourceHandle baseline = HdRetainedContainerDataSource::New(
            TfToken("A"), I(1), TfToken("B"), I(2), TfToken("C"), I(3), TfToken("D"),
            HdRetainedContainerDataSource::New(TfToken("E"), I(4)), TfToken("F"), I(7), TfToken("G"), I(8));

    COMPARECONTAINERS("three container overlay:", test, baseline);
}

// ----------------------------------------------------------------------------

TEST(TestHydra, test_container_editor) {
    {
        HdContainerDataSourceHandle baseline =
                HdRetainedContainerDataSource::New(TfToken("A"), I(1), TfToken("B"), I(2));

        HdContainerDataSourceHandle test = HdContainerDataSourceEditor().Set(L("A"), I(1)).Set(L("B"), I(2)).Finish();

        COMPARECONTAINERS("one level:", test, baseline);
    }

    {
        HdContainerDataSourceHandle test = HdContainerDataSourceEditor()
                                                   .Set(L("A"), I(1))
                                                   .Set(L("B"), I(2))
                                                   .Set(L("C/D"), I(3))
                                                   .Set(L("C/E"), I(4))
                                                   .Set(L("B"), I(5))
                                                   .Finish();

        HdContainerDataSourceHandle baseline = HdRetainedContainerDataSource::New(
                TfToken("A"), I(1), TfToken("B"), I(5), TfToken("C"),
                HdRetainedContainerDataSource::New(TfToken("D"), I(3), TfToken("E"), I(4)));

        COMPARECONTAINERS("two levels with override:", test, baseline);
    }

    {
        HdContainerDataSourceHandle test = HdContainerDataSourceEditor()
                                                   .Set(L("A"), HdRetainedContainerDataSource::New(TfToken("B"), I(1)))
                                                   .Set(L("A/C"), I(2))
                                                   .Set(L("A/D/E"), I(3))
                                                   .Finish();

        HdContainerDataSourceHandle baseline = HdRetainedContainerDataSource::New(
                TfToken("A"),
                HdRetainedContainerDataSource::New(TfToken("B"), I(1), TfToken("C"), I(2), TfToken("D"),
                                                   HdRetainedContainerDataSource::New(TfToken("E"), I(3))));

        COMPARECONTAINERS("set with container and then override:", test, baseline);
    }

    {
        HdContainerDataSourceHandle subcontainer =
                HdContainerDataSourceEditor().Set(L("B/C/E"), I(2)).Set(L("Z/Y"), I(3)).Finish();

        HdContainerDataSourceHandle test = HdContainerDataSourceEditor()
                                                   .Set(L("A"), subcontainer)
                                                   .Set(L("A/B/Q"), I(5))
                                                   .Set(L("A/B/C/F"), I(6))
                                                   .Set(L("A/Z/Y"), nullptr)
                                                   .Finish();

        HdContainerDataSourceHandle baseline = HdRetainedContainerDataSource::New(
                TfToken("A"),
                HdRetainedContainerDataSource::New(
                        TfToken("B"),
                        HdRetainedContainerDataSource::New(
                                TfToken("C"),
                                HdRetainedContainerDataSource::New(TfToken("E"), I(2), TfToken("F"), I(6)),
                                TfToken("Q"), I(5)),
                        TfToken("Z"), HdRetainedContainerDataSource::New()));

        COMPARECONTAINERS("set with container, override deeply + delete:", test, baseline);
    }

    {
        HdContainerDataSourceHandle initialContainer = HdContainerDataSourceEditor().Set(L("A/B"), I(1)).Finish();

        HdContainerDataSourceHandle test =
                HdContainerDataSourceEditor(initialContainer).Set(L("A/C"), I(2)).Set(L("D"), I(3)).Finish();

        HdContainerDataSourceHandle baseline = HdRetainedContainerDataSource::New(
                TfToken("A"), HdRetainedContainerDataSource::New(TfToken("B"), I(1), TfToken("C"), I(2)), TfToken("D"),
                I(3));

        COMPARECONTAINERS("initial container + overrides:", test, baseline);
    }

    {
        // Setting with a container data source masks the children of an
        // existing container on the editors initialContainer.

        // Confirm that A/B and A/C are not present after setting A directly
        // from a container.

        HdContainerDataSourceHandle initialContainer =
                HdContainerDataSourceEditor()
                        .Set(L("A"), HdRetainedContainerDataSource::New(TfToken("B"), I(1), TfToken("C"), I(2)))
                        .Finish();

        HdContainerDataSourceHandle test =
                HdContainerDataSourceEditor(initialContainer)
                        .Set(L("A"), HdRetainedContainerDataSource::New(TfToken("D"), I(3), TfToken("E"), I(4)))
                        .Finish();

        HdContainerDataSourceHandle baseline =
                HdContainerDataSourceEditor().Set(L("A/D"), I(3)).Set(L("A/E"), I(4)).Finish();

        COMPARECONTAINERS("sub-container replacement + masking:", test, baseline);
    }

    {
        // Setting with a container data source masks the children of an
        // existing container on the editors initialContainer.

        // Confirm that A/B and A/C are not present after setting A directly
        // from a container.

        HdContainerDataSourceHandle initialContainer =
                HdContainerDataSourceEditor()
                        .Set(L("A"), HdRetainedContainerDataSource::New(TfToken("B"), I(1), TfToken("C"), I(2)))
                        .Finish();

        HdContainerDataSourceHandle subcontainer = HdContainerDataSourceEditor().Set(L("D"), I(3)).Finish();

        HdContainerDataSourceHandle test =
                HdContainerDataSourceEditor(initialContainer).Overlay(L("A"), subcontainer).Finish();

        HdContainerDataSourceHandle baseline =
                HdContainerDataSourceEditor().Set(L("A/B"), I(1)).Set(L("A/C"), I(2)).Set(L("A/D"), I(3)).Finish();

        COMPARECONTAINERS("sub-container overlay:", test, baseline);
    }
}