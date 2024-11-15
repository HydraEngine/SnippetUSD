//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/pxr.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/sdf/path.h"

#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/attributeQuery.h"
#include "pxr/usd/usd/references.h"

#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <tuple>
#include <gtest/gtest.h>

using std::string;
using std::tuple;
using std::vector;

PXR_NAMESPACE_USING_DIRECTIVE

constexpr size_t TIME_SAMPLE_BEGIN = 101.0;
constexpr size_t TIME_SAMPLE_END = 120.0;
constexpr double DEFAULT_VALUE = 4.0;

tuple<UsdStageRefPtr, UsdAttribute, UsdAttribute, UsdAttribute> _GenerateStage(const string& fmt) {
    const TfToken defAttrTk = TfToken("size");
    const TfToken sampleAttrTk = TfToken("points");
    const SdfPath primPath = SdfPath("/Sphere");
    const SdfPath localRefPrimPath = SdfPath("/SphereOver");

    auto stage = UsdStage::CreateInMemory("test" + fmt);
    auto prim = stage->DefinePrim(primPath);

    auto defAttr = prim.CreateAttribute(defAttrTk, SdfValueTypeNames->Double);
    defAttr.Set<double>(1.0);

    auto sampleAttr = prim.CreateAttribute(sampleAttrTk, SdfValueTypeNames->Double);
    for (size_t i = TIME_SAMPLE_BEGIN; i < TIME_SAMPLE_END; ++i) {
        const auto sample = static_cast<double>(i);
        sampleAttr.Set<double>(sample, sample);
    }

    auto localRefPrim = stage->OverridePrim(localRefPrimPath);
    localRefPrim.GetReferences().AddInternalReference(primPath);
    auto localRefAttr = localRefPrim.CreateAttribute(defAttrTk, SdfValueTypeNames->Double);
    localRefAttr.Block();

    return std::make_tuple(stage, defAttr, sampleAttr, localRefAttr);
}

template <typename T>
void _CheckDefaultNotBlocked(UsdAttribute& attr, const T expectedValue) {
    T value;
    VtValue untypedValue;
    UsdAttributeQuery query(attr);

    ASSERT_TRUE(attr.Get<T>(&value));
    ASSERT_TRUE(query.Get<T>(&value));
    ASSERT_TRUE(attr.Get(&untypedValue));
    ASSERT_TRUE(query.Get(&untypedValue));
    ASSERT_TRUE(value == expectedValue);
    ASSERT_TRUE(untypedValue.UncheckedGet<T>() == expectedValue);
    ASSERT_TRUE(attr.HasValue());
    ASSERT_TRUE(attr.HasAuthoredValue());
}

template <typename T>
void _CheckDefaultBlocked(UsdAttribute& attr) {
    T value;
    VtValue untypedValue;
    UsdAttributeQuery query(attr);
    UsdResolveInfo info = attr.GetResolveInfo();

    ASSERT_TRUE(!attr.Get<T>(&value));
    ASSERT_TRUE(!query.Get<T>(&value));
    ASSERT_TRUE(!attr.Get(&untypedValue));
    ASSERT_TRUE(!query.Get(&untypedValue));
    ASSERT_TRUE(!attr.HasValue());
    ASSERT_TRUE(!attr.HasAuthoredValue());
    ASSERT_TRUE(info.HasAuthoredValueOpinion());
}

template <typename T>
void _CheckSampleNotBlocked(UsdAttribute& attr, const double time, const T expectedValue) {
    T value;
    VtValue untypedValue;
    UsdAttributeQuery query(attr);

    ASSERT_TRUE(attr.Get<T>(&value, time));
    ASSERT_TRUE(query.Get<T>(&value, time));
    ASSERT_TRUE(attr.Get(&untypedValue, time));
    ASSERT_TRUE(query.Get(&untypedValue, time));
    ASSERT_TRUE(value == expectedValue);
    ASSERT_TRUE(untypedValue.UncheckedGet<T>() == expectedValue);
}

template <typename T>
void _CheckSampleBlocked(UsdAttribute& attr, const double time) {
    T value;
    VtValue untypedValue;
    UsdAttributeQuery query(attr);

    ASSERT_TRUE(!attr.Get<T>(&value, time));
    ASSERT_TRUE(!query.Get<T>(&value, time));
    ASSERT_TRUE(!attr.Get(&untypedValue, time));
    ASSERT_TRUE(!query.Get(&untypedValue, time));
}

TEST(TestUSD, UsdAttributeBlocking) {
    vector<string> formats = {".usda", ".usdc"};
    auto block = SdfValueBlock();

    for (const auto& fmt : formats) {
        std::cout << "\n+------------------------------------------+" << std::endl;
        std::cout << "Testing format: " << fmt << std::endl;

        UsdStageRefPtr stage;
        UsdAttribute defAttr, sampleAttr, localRefAttr;
        std::tie(stage, defAttr, sampleAttr, localRefAttr) = _GenerateStage(fmt);

        std::cout << "Testing blocks through local references" << std::endl;
        _CheckDefaultBlocked<double>(localRefAttr);
        _CheckDefaultNotBlocked(defAttr, 1.0);

        std::cout << "Testing blocks on default values" << std::endl;
        defAttr.Set<SdfValueBlock>(block);
        _CheckDefaultBlocked<double>(defAttr);

        defAttr.Set<double>(DEFAULT_VALUE);
        _CheckDefaultNotBlocked(defAttr, DEFAULT_VALUE);

        defAttr.Set(VtValue(block));
        _CheckDefaultBlocked<double>(defAttr);

        // Reset our value
        defAttr.Set<double>(DEFAULT_VALUE);
        _CheckDefaultNotBlocked(defAttr, DEFAULT_VALUE);

        defAttr.Block();
        _CheckDefaultBlocked<double>(defAttr);

        std::cout << "Testing typed time sample operations" << std::endl;
        for (size_t i = TIME_SAMPLE_BEGIN; i < TIME_SAMPLE_END; ++i) {
            const auto sample = static_cast<double>(i);
            bool hasSamplesPre, hasSamplePost;
            double upperPre, lowerPre, lowerPost, upperPost;
            sampleAttr.GetBracketingTimeSamples(sample, &lowerPre, &upperPre, &hasSamplesPre);

            _CheckSampleNotBlocked(sampleAttr, sample, sample);

            sampleAttr.Set<SdfValueBlock>(block, sample);
            _CheckSampleBlocked<double>(sampleAttr, sample);

            // ensure bracketing time samples continues to report all
            // things properly even in the presence of blocks
            sampleAttr.GetBracketingTimeSamples(sample, &lowerPost, &upperPost, &hasSamplePost);

            ASSERT_EQ(hasSamplesPre, hasSamplePost);
            ASSERT_EQ(lowerPre, lowerPost);
            ASSERT_EQ(upperPre, upperPost);
        }

        // Reset our value
        for (size_t i = TIME_SAMPLE_BEGIN; i < TIME_SAMPLE_END; ++i) {
            const auto sample = static_cast<double>(i);
            sampleAttr.Set<double>(sample, sample);
        }

        std::cout << "Testing untyped time sample operations" << std::endl;
        for (size_t i = TIME_SAMPLE_BEGIN; i < TIME_SAMPLE_END; ++i) {
            const auto sample = static_cast<double>(i);

            _CheckSampleNotBlocked(sampleAttr, sample, sample);

            sampleAttr.Set(VtValue(block), sample);
            _CheckSampleBlocked<double>(sampleAttr, sample);
        }

        // Reset our value
        for (size_t i = TIME_SAMPLE_BEGIN; i < TIME_SAMPLE_END; ++i) {
            const auto sample = static_cast<double>(i);
            sampleAttr.Set<double>(sample, sample);
        }

        // ensure that both default values and time samples are blown away.
        sampleAttr.Block();
        _CheckDefaultBlocked<double>(sampleAttr);
        ASSERT_EQ(sampleAttr.GetNumTimeSamples(), 0);
        UsdAttributeQuery sampleQuery(sampleAttr);
        ASSERT_EQ(sampleQuery.GetNumTimeSamples(), 0);

        for (size_t i = TIME_SAMPLE_BEGIN; i < TIME_SAMPLE_END; ++i) {
            const auto sample = static_cast<double>(i);
            _CheckSampleBlocked<double>(sampleAttr, sample);
        }

        // Reset our value
        for (size_t i = TIME_SAMPLE_BEGIN; i < TIME_SAMPLE_END; ++i) {
            const auto sample = static_cast<double>(i);
            sampleAttr.Set<double>(sample, sample);
        }

        // Test attribute blocking behavior in between blocked/unblocked times
        for (size_t i = TIME_SAMPLE_BEGIN; i < TIME_SAMPLE_END; i += 2) {
            const auto sample = static_cast<double>(i);
            sampleAttr.Set<SdfValueBlock>(block, sample);

            _CheckSampleBlocked<double>(sampleAttr, sample);

            if (sample + 1 < TIME_SAMPLE_END) {
                double sampleStepHalf = sample + 0.5;
                _CheckSampleBlocked<double>(sampleAttr, sampleStepHalf);
                _CheckSampleNotBlocked(sampleAttr, sample + 1.0, sample + 1.0);
            }
        }
        std::cout << "+------------------------------------------+" << std::endl;
    }

    printf("\n\n>>> Test SUCCEEDED\n");
}
