//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.a

#include "pxr/imaging/hd/basisCurvesSchema.h"
#include "pxr/imaging/hd/basisCurvesTopologySchema.h"
#include "pxr/imaging/hd/primvarSchema.h"
#include "pxr/imaging/hd/primvarsSchema.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/retainedSceneIndex.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hdsi/pinnedCurveExpandingSceneIndex.h"

#include "pxr/base/tf/errorMark.h"

#include <iostream>
#include <sstream>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using _TokenDs = HdRetainedTypedSampledDataSource<TfToken>;

struct _Primvar {
    _Primvar(TfToken _name,
             VtValue _value,
             TfToken _interp,
             TfToken _role = TfToken(),
             VtIntArray _indices = VtIntArray())
        : name(std::move(_name)),
          value(std::move(_value)),
          interp(std::move(_interp)),
          role(std::move(_role)),
          indices(std::move(_indices)) {}

    TfToken name;
    VtValue value;
    TfToken interp;
    TfToken role;
    VtIntArray indices;
};

using _Primvars = std::vector<_Primvar>;

struct _Curve {
    VtIntArray curveVertexCounts;
    VtIntArray curveIndices;
    TfToken type;
    TfToken basis;
    TfToken wrap;
    _Primvars primvars;
};

// Returns a typed sampled data source for a small number of VtArray types.
HdSampledDataSourceHandle _GetRetainedDataSource(VtValue const& val) {
    // Support just the types used for this test:
    //  int, VtIntArray, VtFloatArray, VtVec3fArray
    if (val.IsHolding<int>()) {
        return HdRetainedTypedSampledDataSource<int>::New(val.UncheckedGet<int>());
    }
    if (val.IsHolding<VtIntArray>()) {
        return HdRetainedTypedSampledDataSource<VtIntArray>::New(val.UncheckedGet<VtIntArray>());
    }
    if (val.IsHolding<VtFloatArray>()) {
        return HdRetainedTypedSampledDataSource<VtFloatArray>::New(val.UncheckedGet<VtFloatArray>());
    }
    if (val.IsHolding<VtVec3fArray>()) {
        return HdRetainedTypedSampledDataSource<VtVec3fArray>::New(val.UncheckedGet<VtVec3fArray>());
    }

    TF_WARN("Unsupported primvar type %s", val.GetTypeName().c_str());
    return HdRetainedTypedSampledDataSource<VtValue>::New(val);
}

HdContainerDataSourceHandle _BuildCurveDataSource(_Curve const& curve) {
    HdDataSourceBaseHandle bcs =
            HdBasisCurvesSchema::Builder()
                    .SetTopology(HdBasisCurvesTopologySchema::Builder()
                                         .SetCurveVertexCounts(HdRetainedTypedSampledDataSource<VtIntArray>::New(
                                                 curve.curveVertexCounts))
                                         .SetCurveIndices(
                                                 HdRetainedTypedSampledDataSource<VtIntArray>::New(curve.curveIndices))
                                         .SetBasis(_TokenDs::New(curve.basis))
                                         .SetType(_TokenDs::New(curve.type))
                                         .SetWrap(_TokenDs::New(curve.wrap))
                                         .Build())
                    .Build();

    const _Primvars& primvars = curve.primvars;
    std::vector<TfToken> primvarNames;
    primvarNames.reserve(primvars.size());
    std::vector<HdDataSourceBaseHandle> primvarDataSources;
    primvarDataSources.reserve(primvars.size());

    for (_Primvar const& pv : primvars) {
        primvarNames.push_back(pv.name);

        primvarDataSources.push_back(HdPrimvarSchema::Builder()
                                             .SetPrimvarValue(pv.indices.empty() ? _GetRetainedDataSource(pv.value)
                                                                                 : HdSampledDataSourceHandle())
                                             .SetIndexedPrimvarValue(!pv.indices.empty()
                                                                             ? _GetRetainedDataSource(pv.value)
                                                                             : HdSampledDataSourceHandle())
                                             .SetIndices(HdRetainedTypedSampledDataSource<VtIntArray>::New(pv.indices))
                                             .SetInterpolation(HdPrimvarSchema::BuildInterpolationDataSource(pv.interp))
                                             .SetRole(HdPrimvarSchema::BuildInterpolationDataSource(pv.role))
                                             .Build());
    }

    HdDataSourceBaseHandle primvarsDs =
            HdRetainedContainerDataSource::New(primvars.size(), primvarNames.data(), primvarDataSources.data());

    return HdRetainedContainerDataSource::New(HdBasisCurvesSchemaTokens->basisCurves, bcs,
                                              HdPrimvarsSchemaTokens->primvars, primvarsDs);
}

std::pair<_Curve, _Curve> _GetAuthoredAndExpectedTestCurves(TfToken const& basis,
                                                            bool useCurveIndices,
                                                            bool hasIndexedPrimvar) {
    ///
    // Authored Data
    ///
    // Topology & points
    const VtIntArray counts = {4, 7, 4, 2};

    const VtVec3fArray points = {
            {0, 0, 0},  {0, 0, 1},  {0, 0, 2},  {0, 0, 3},

            {0, 0, 4},  {0, 0, 5},  {0, 0, 6},  {0, 0, 7},  {0, 0, 8}, {0, 0, 9}, {0, 0, 10},

            {0, 0, 11}, {0, 0, 12}, {0, 0, 13}, {0, 0, 14},

            {0, 0, 15}, {0, 0, 16},
    };

    const VtIntArray curveIndices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    // Primvars
    const int constantPrimvar = 42;

    const VtFloatArray uniformPrimvar = {// One per curve
                                         0.0f, 1.0f, 2.0f, 3.0f};

    const VtVec3fArray indexedVertexPrimvar = {
            {0, 0, 0},
            {0, 0, 1},
            {0, 0, 2},
            {0, 0, 3},
    };
    const VtIntArray vertexPrimvarIndices = {0, 1, 2, 3, 1, 2, 3, 0, 1, 2, 3, 2, 3, 0, 1, 1, 2};

    const VtFloatArray varyingPrimvar = {// 1 segment => 2 varying values
                                         0.0f, 0.1f,

                                         // 4 segments => 5 varying values
                                         0.2f, 0.3f, 0.4f, 0.5f, 0.6f,

                                         // 1 segment => 2 varying values
                                         0.7f, 0.8f,

                                         // For the min vert count (2), we still treat it as a segment (from an
                                         // authoring point of view) and so, we expect 2 varying values authored.
                                         0.9f, 1.0f};

    const VtFloatArray indexedVaryingPrimvar{0.0f, 0.1f, 0.2f, 0.3f};
    const VtIntArray varyingPrimvarIndices = {0, 1, 1, 2, 3, 0, 1, 2, 3, 1, 2};

    ///
    // Authored curve configuration
    ///
    _Curve c;
    {
        // Configure authored curve with above data.
        if (useCurveIndices) {
            c.curveIndices = curveIndices;
        }
        c.curveVertexCounts = counts;
        c.type = HdTokens->cubic;
        c.basis = basis;
        c.wrap = HdTokens->pinned;

        // Add points and a primvar for each relevant interp type.
        _Primvars& primvars = c.primvars;

        primvars.emplace_back(HdTokens->points, VtValue(points), HdPrimvarSchemaTokens->vertex,
                              HdPrimvarSchemaTokens->point);

        primvars.emplace_back(TfToken("fooConstant"), VtValue(constantPrimvar), HdPrimvarSchemaTokens->constant);

        primvars.emplace_back(TfToken("fooUniform"), VtValue(uniformPrimvar), HdPrimvarSchemaTokens->uniform);

        if (hasIndexedPrimvar) {
            primvars.emplace_back(TfToken("fooVertexIndexed"), VtValue(indexedVertexPrimvar),
                                  HdPrimvarSchemaTokens->vertex, TfToken("testRole"), vertexPrimvarIndices);

            primvars.emplace_back(TfToken("fooVaryingIndexed"), VtValue(indexedVaryingPrimvar),
                                  HdPrimvarSchemaTokens->varying, TfToken("testRole"), varyingPrimvarIndices);

        } else {
            primvars.emplace_back(TfToken("fooVarying"), VtValue(varyingPrimvar), HdPrimvarSchemaTokens->varying);
        }
    }

    ///
    // Expected curve configuration
    ///
    _Curve e;
    {
        VtVec3fArray ePoints;
        VtIntArray eCounts;
        VtIntArray eCurveIndices;
        VtIntArray eVertexPrimvarIndices;
        VtFloatArray eVaryingPrimvar;
        VtIntArray eVaryingPrimvarIndices;

        if (basis == HdTokens->bspline) {
            // Topology & points
            if (!useCurveIndices) {
                ePoints = {
                        {0, 0, 0},                                                  // added
                        {0, 0, 0},                                                  // added
                        {0, 0, 0},  {0, 0, 1},  {0, 0, 2},  {0, 0, 3},  {0, 0, 3},  // added
                        {0, 0, 3},                                                  // added

                        {0, 0, 4},  // added
                        {0, 0, 4},  // added
                        {0, 0, 4},  {0, 0, 5},  {0, 0, 6},  {0, 0, 7},  {0, 0, 8},
                        {0, 0, 9},  {0, 0, 10}, {0, 0, 10},  // added
                        {0, 0, 10},                          // added

                        {0, 0, 11},                                                  // added
                        {0, 0, 11},                                                  // added
                        {0, 0, 11}, {0, 0, 12}, {0, 0, 13}, {0, 0, 14}, {0, 0, 14},  // added
                        {0, 0, 14},                                                  // added

                        {0, 0, 15},                          // added
                        {0, 0, 15},                          // added
                        {0, 0, 15}, {0, 0, 16}, {0, 0, 16},  // added
                        {0, 0, 16},                          // added
                };
            } else {
                ePoints = points;  // unexpanded
            }

            eCounts = {8, 11, 8, 6};

            eCurveIndices = {0,  0,  0,  1,  2,  3,  3,  3,  4,  4,  4,  5,  6,  7,  8,  9, 10,
                             10, 10, 11, 11, 11, 12, 13, 14, 14, 14, 15, 15, 15, 16, 16, 16};

            // Primvars
            eVertexPrimvarIndices = {0, 0, 0, 1, 2, 3, 3, 3, 1, 1, 1, 2, 3, 0, 1, 2, 3,
                                     3, 3, 2, 2, 2, 3, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2};

            eVaryingPrimvar = {// 5 segments (1 authored, 4 added) => 6 varying values
                               0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.1f,

                               // 8 segments (4 authored, 4 added) => 9 varying values
                               0.2f, 0.2f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.6f, 0.6f,

                               // 5 segments (1 authored, 4 added) => 6 varying values
                               0.7f, 0.7f, 0.7f, 0.8f, 0.8f, 0.8f,

                               // 3 segments on expansion => 4 varying values
                               0.9f, 0.9f, 1.0f, 1.0f};

            eVaryingPrimvarIndices = {0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 3, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 1, 1, 2, 2};
        }

        else if (basis == HdTokens->catmullRom) {
            // Topology & points
            if (!useCurveIndices) {
                ePoints = {
                        {0, 0, 0},                                                  // added
                        {0, 0, 0},  {0, 0, 1},  {0, 0, 2},  {0, 0, 3},  {0, 0, 3},  // added

                        {0, 0, 4},  // added
                        {0, 0, 4},  {0, 0, 5},  {0, 0, 6},  {0, 0, 7},  {0, 0, 8},
                        {0, 0, 9},  {0, 0, 10}, {0, 0, 10},  // added

                        {0, 0, 11},                                                  // added
                        {0, 0, 11}, {0, 0, 12}, {0, 0, 13}, {0, 0, 14}, {0, 0, 14},  // added

                        {0, 0, 15},                          // added
                        {0, 0, 15}, {0, 0, 16}, {0, 0, 16},  // added
                };
            } else {
                ePoints = points;  // unexpanded
            }

            eCounts = {6, 9, 6, 4};

            eCurveIndices = {0, 0, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 10, 10, 11, 11, 12, 13, 14, 14, 15, 15, 16, 16};

            // Primvars
            eVertexPrimvarIndices = {0, 0, 1, 2, 3, 3, 1, 1, 2, 3, 0, 1, 2, 3, 3, 2, 2, 3, 0, 1, 1, 1, 1, 2, 2};

            eVaryingPrimvar = {// 3 segments (1 authored, 2 added) => 4 varying values
                               0.0f, 0.0f, 0.1f, 0.1f,

                               // 6 segments (4 authored, 2 added) => 7 varying values
                               0.2f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.6f,

                               // 3 segments (1 authored, 2 added) => 4 varying values
                               0.7f, 0.7f, 0.8f, 0.8f,

                               // 1 segment on expansion => 2 varying values
                               // (this means that the authored values are not duplicated!)
                               0.9f, 1.0f};

            eVaryingPrimvarIndices = {0, 0, 1, 1, 1, 1, 2, 3, 0, 1, 1, 2, 2, 3, 3, 1, 2};
        }

        if (useCurveIndices) {
            e.curveIndices = eCurveIndices;
        }
        e.curveVertexCounts = eCounts;
        e.type = HdTokens->cubic;
        e.basis = basis;
        e.wrap = HdTokens->nonperiodic;

        _Primvars& primvars = e.primvars;

        primvars.emplace_back(HdTokens->points, VtValue(ePoints), HdPrimvarSchemaTokens->vertex,
                              HdPrimvarSchemaTokens->point);

        // unchanged
        primvars.emplace_back(TfToken("fooConstant"), VtValue(constantPrimvar), HdPrimvarSchemaTokens->constant);

        // unchanged
        primvars.emplace_back(TfToken("fooUniform"), VtValue(uniformPrimvar), HdPrimvarSchemaTokens->uniform);

        if (hasIndexedPrimvar) {
            primvars.emplace_back(TfToken("fooVertexIndexed"), VtValue(indexedVertexPrimvar),
                                  HdPrimvarSchemaTokens->vertex, TfToken("testRole"), eVertexPrimvarIndices);

            primvars.emplace_back(TfToken("fooVaryingIndexed"), VtValue(indexedVaryingPrimvar),
                                  HdPrimvarSchemaTokens->varying, TfToken("testRole"), eVaryingPrimvarIndices);

        } else {
            primvars.emplace_back(TfToken("fooVarying"), VtValue(eVaryingPrimvar), HdPrimvarSchemaTokens->varying);
        }
    }
    return std::make_pair(/* authored */ c, /* expected */ e);
}

void _Compare(const HdContainerDataSourceHandle& baseline, const HdContainerDataSourceHandle& output) {
    std::ostringstream baselineBuffer, outputBuffer;

    HdDebugPrintDataSource(baselineBuffer, baseline);
    HdDebugPrintDataSource(outputBuffer, output);

    if (baselineBuffer.str() == outputBuffer.str()) {
        return;
    }

    std::cerr << "FAILED. Expected:" << std::endl;
    std::cerr << baselineBuffer.str();
    std::cerr << "Got: " << std::endl;
    std::cerr << outputBuffer.str();

    GTEST_FAIL();
}

void TestPinnedCurves(bool hasCurveIndices, bool hasIndexedPrimvar) {
    {
        // 1. Pinned bspline
        const auto curves = _GetAuthoredAndExpectedTestCurves(HdTokens->bspline, hasCurveIndices, hasIndexedPrimvar);

        HdRetainedSceneIndexRefPtr retainedScene = HdRetainedSceneIndex::New();
        retainedScene->AddPrims({{
                SdfPath("/simpleCurve"),
                HdBasisCurvesSchemaTokens->basisCurves,
                _BuildCurveDataSource(curves.first),
        }});

        HdSceneIndexBaseRefPtr expandingScene = HdsiPinnedCurveExpandingSceneIndex::New(retainedScene);

        _Compare(_BuildCurveDataSource(curves.second), expandingScene->GetPrim(SdfPath("/simpleCurve")).dataSource);
    }

    // 2. Pinned catmullRom
    {
        const auto curves = _GetAuthoredAndExpectedTestCurves(HdTokens->catmullRom, hasCurveIndices, hasIndexedPrimvar);

        HdRetainedSceneIndexRefPtr retainedScene = HdRetainedSceneIndex::New();
        retainedScene->AddPrims({{
                SdfPath("/simpleCurve"),
                HdBasisCurvesSchemaTokens->basisCurves,
                _BuildCurveDataSource(curves.first),
        }});

        HdSceneIndexBaseRefPtr expandingScene = HdsiPinnedCurveExpandingSceneIndex::New(retainedScene);

        _Compare(_BuildCurveDataSource(curves.second), expandingScene->GetPrim(SdfPath("/simpleCurve")).dataSource);
    }
}

//-----------------------------------------------------------------------------

TEST(TestHydraSceneIndex, test_simple_pinned_curves) {
    TestPinnedCurves(/* hasCurveIndices   */ false,
                     /* hasIndexedPrimvar */ false);
}

TEST(TestHydraSceneIndex, test_pinned_curves_with_indexed_primvar) {
    TestPinnedCurves(/* hasCurveIndices   */ false,
                     /* hasIndexedPrimvar */ true);
}

TEST(TestHydraSceneIndex, test_pinned_curves_with_curve_indices) {
    TestPinnedCurves(/* hasCurveIndices   */ true,
                     /* hasIndexedPrimvar */ false);
}

TEST(TestHydraSceneIndex, test_pinned_curves_with_curve_indices_and_indexed_primvar) {
    TestPinnedCurves(/* hasCurveIndices   */ true,
                     /* hasIndexedPrimvar */ true);
}
