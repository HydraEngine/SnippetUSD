//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include <iostream>

#include "pxr/imaging/hd/collectionExpressionEvaluator.h"
#include "pxr/imaging/hd/collectionPredicateLibrary.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/retainedSceneIndex.h"
#include "pxr/imaging/hd/sceneIndex.h"

#include "pxr/imaging/hd/materialBindingSchema.h"
#include "pxr/imaging/hd/materialBindingsSchema.h"
#include "pxr/imaging/hd/primvarSchema.h"
#include "pxr/imaging/hd/primvarsSchema.h"
#include "pxr/imaging/hd/purposeSchema.h"
#include "pxr/imaging/hd/visibilitySchema.h"

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

TF_DEFINE_PRIVATE_TOKENS(_primTypeTokens, (fruit)(mesh)(scope)(veg)(foo)(bar)(baz)(b));

TF_DEFINE_PRIVATE_TOKENS(_purposeTokens, (food)(furniture));

TF_DEFINE_PRIVATE_TOKENS(_primvarTokens, (fresh)(sour)(roughness)((glossy, "foo:glossy")));

TF_DEFINE_PRIVATE_TOKENS(_matBindingPurposeTokens, (preview)(render));

HdDataSourceBaseHandle _MakeVisibilityDataSource(bool visible) {
    return HdVisibilitySchema::Builder().SetVisibility(HdRetainedTypedSampledDataSource<bool>::New(visible)).Build();
}

HdDataSourceBaseHandle _MakePurposeDataSource(const TfToken& purpose) {
    return HdPurposeSchema::Builder().SetPurpose(HdRetainedTypedSampledDataSource<TfToken>::New(purpose)).Build();
}

HdDataSourceBaseHandle _MakePrimvarsDataSource(const TfTokenVector& primvarNames) {
    const size_t count = primvarNames.size();
    std::vector<HdDataSourceBaseHandle> primvarsDs(
            count, HdPrimvarSchema::Builder().SetPrimvarValue(HdRetainedTypedSampledDataSource<int>::New(1)).Build());

    return HdPrimvarsSchema::BuildRetained(count, primvarNames.data(), primvarsDs.data());
}

using _TokenPathPair = std::pair<TfToken, SdfPath>;
using _TokenPathPairVector = std::vector<_TokenPathPair>;

HdDataSourceBaseHandle _MakeMaterialBindingsDataSource(const _TokenPathPairVector& bindings) {
    const size_t count = bindings.size();

    TfTokenVector purposes;
    purposes.reserve(count);
    std::vector<HdDataSourceBaseHandle> bindingsDs;
    bindingsDs.reserve(count);

    for (const auto& b : bindings) {
        purposes.push_back(b.first);
        bindingsDs.push_back(HdMaterialBindingSchema::Builder()
                                     .SetPath(HdRetainedTypedSampledDataSource<SdfPath>::New(b.second))
                                     .Build());
    }

    return HdMaterialBindingsSchema::BuildRetained(count, purposes.data(), bindingsDs.data());
}

HdContainerDataSourceHandle _MakePrimContainer(bool visibility,
                                               const TfToken& purpose,
                                               const TfTokenVector& primvarNames = {},
                                               const _TokenPathPairVector& matBindings = {}) {
    return HdRetainedContainerDataSource::New(
            HdVisibilitySchemaTokens->visibility, _MakeVisibilityDataSource(visibility),

            HdPurposeSchemaTokens->purpose, _MakePurposeDataSource(purpose),

            HdPrimvarsSchemaTokens->primvars, _MakePrimvarsDataSource(primvarNames),

            HdMaterialBindingsSchemaTokens->materialBindings, _MakeMaterialBindingsDataSource(matBindings));
}

// Rather than define a standalone set of custom predicates and have the user
// stitch various predicate libraries together, use the pattern below to
// extend the provided predicate library.
// That way, the net library can be built up in a chaining fashion:
// HdCollectionPredicateLibrary myLib =
//      _MakeCustomN(
//          ...(
//              _MakeCustom2(
//                  _MakeCustom1(HdGetCollectionPredicateLibrary()))...));
//
HdCollectionPredicateLibrary _MakeCustomPredicateLibrary(const HdCollectionPredicateLibrary& base) {
    using PredResult = SdfPredicateFunctionResult;

    // Extend provided library with additional predicate(s).
    HdCollectionPredicateLibrary lib = base;

    lib

            .Define("eatable",
                    [](HdSceneIndexPrim const& p, bool eatability) {
                        return PredResult::MakeVarying(p.primType == _primTypeTokens->veg ||
                                                       p.primType == _primTypeTokens->fruit);
                    },
                    {{"isEatable", true}});

    return lib;
}

HdCollectionPredicateLibrary _GetCustomPredicateLibrary() {
    static HdCollectionPredicateLibrary lib = _MakeCustomPredicateLibrary(HdGetCollectionPredicateLibrary());
    return lib;
}

HdSceneIndexBaseRefPtr _CreateTestScene() {
    HdRetainedSceneIndexRefPtr sceneIndex_ = HdRetainedSceneIndex::New();
    HdRetainedSceneIndex& sceneIndex = *sceneIndex_;

    // We don't need to explicitly add each of the ancestors for a given path
    // since HdRetainedSceneIndex uses a SdfPathTable to manage entries.
    // We do so in this test scene for clarity sake.
    //
    sceneIndex.AddPrims({
            {SdfPath("/A"), _primTypeTokens->scope, nullptr},
            {SdfPath("/A/B"), _primTypeTokens->scope, nullptr},
            {SdfPath("/A/B/Carrot"), _primTypeTokens->veg,
             _MakePrimContainer(
                     /* visibility */ true, _purposeTokens->food, {_primvarTokens->fresh},
                     {{HdMaterialBindingsSchemaTokens->allPurpose, SdfPath("/Looks/OrangeMat")}})},
            {SdfPath("/A/B/Broccoli"), _primTypeTokens->veg,
             _MakePrimContainer(
                     /* visibility */ true, _purposeTokens->food, {},  // no primvars
                     {
                             {_matBindingPurposeTokens->preview, SdfPath("/Looks/GreenMat")},
                             {HdMaterialBindingsSchemaTokens->allPurpose, SdfPath("/Looks/WiltedGreenMat")},
                     })},
            {SdfPath("/A/B/Tomato"), _primTypeTokens->fruit,
             _MakePrimContainer(
                     /* visibility */ true, _purposeTokens->food, {_primvarTokens->fresh, _primvarTokens->glossy},
                     {{_matBindingPurposeTokens->preview, SdfPath("/Looks/GlossyRedMat")}})},
            {SdfPath("/A/B/Apricot"), _primTypeTokens->fruit,
             _MakePrimContainer(
                     /* visibility */ true, _purposeTokens->food, {},  // no primvars
                     {{_matBindingPurposeTokens->preview, SdfPath("/Looks/DriedOrangeMat")},
                      {HdMaterialBindingsSchemaTokens->allPurpose, SdfPath("/Looks/DriedOrangeMat")}})},
            {SdfPath("/A/C"), _primTypeTokens->scope, nullptr},
            {SdfPath("/A/C/Table"), TfToken("mesh"),
             _MakePrimContainer(
                     /* visibility */ true, _purposeTokens->furniture)},
            {SdfPath("/A/C/Chair1"), TfToken("mesh"),
             _MakePrimContainer(
                     /* visibility */ true, _purposeTokens->furniture, {_primvarTokens->glossy},
                     {{_matBindingPurposeTokens->preview, SdfPath("/Looks/MetallicMat")}})},
            {SdfPath("/A/C/Chair2"), TfToken("mesh"),
             _MakePrimContainer(
                     /* visibility */ false, _purposeTokens->furniture)},
    });

    return sceneIndex_;
}

}  // namespace

TEST(TestHydra, test_empty_evaluator) {
    {
        HdCollectionExpressionEvaluator eval;
        ASSERT_TRUE(eval.IsEmpty());
        ASSERT_TRUE(!eval.Match(SdfPath("/A")));
    }

    {
        HdCollectionExpressionEvaluator eval(nullptr, SdfPathExpression("/Foo"));
        ASSERT_TRUE(eval.IsEmpty());
        ASSERT_TRUE(!eval.Match(SdfPath("/A")));
    }

    {
        HdSceneIndexBaseRefPtr si = _CreateTestScene();
        HdCollectionExpressionEvaluator eval(si, SdfPathExpression{});
        ASSERT_TRUE(eval.IsEmpty());
        ASSERT_TRUE(!eval.Match(SdfPath("/A")));
    }
}

TEST(TestHydra, test_path_expressions) {
    // Ensure that path expressions without predicates match only those prims
    // that exist in the scene index.
    // This isn't actually the case! See the XXX comment below.
    //

    // Populate test scene index.
    HdRetainedSceneIndexRefPtr si = HdRetainedSceneIndex::New();

    // Ancestors are implicitly added.
    si->AddPrims({
            {SdfPath("/a/b/c/x/y/z/a/b/c"), _primTypeTokens->foo, nullptr},
            {SdfPath("/a/b/c/d/e/f/a/b/a/b/c"), _primTypeTokens->scope, nullptr},
    });

    {
        const SdfPathExpression expr("//b");
        HdCollectionExpressionEvaluator eval(si, expr);

        ASSERT_TRUE(eval.Match(SdfPath("/a/b")));
        ASSERT_TRUE(eval.Match(SdfPath("/a/b/c/x/y/z/a/b")));
        ASSERT_TRUE(eval.Match(SdfPath("/a/b/c/d/e/f/a/b")));
        ASSERT_TRUE(eval.Match(SdfPath("/a/b/c/d/e/f/a/b/a/b")));

        // XXX The scenario below is interesting. We shouldn't be matching a
        //     non-existent prim path, but handling this comes at a performance
        //     cost.
        //     See relevant comment in HdCollectionExpressionEvaluator::Match
        //
        ASSERT_TRUE(eval.Match(SdfPath("/PrimDoesNotExist/b")));

        ASSERT_TRUE(!eval.Match(SdfPath("/a/b/c")));
        // Even though this is a descendant, it won't be matched by expr.
        ASSERT_TRUE(!eval.Match(SdfPath("/a/b/c/x/y/z/a/b/c")));
    }

    {
        const SdfPathExpression expr("//x//a//");
        HdCollectionExpressionEvaluator eval(si, expr);

        ASSERT_TRUE(eval.Match(SdfPath("/a/b/c/x/y/z/a")));
        ASSERT_TRUE(eval.Match(SdfPath("/a/b/c/x/y/z/a/b")));
        ASSERT_TRUE(eval.Match(SdfPath("/a/b/c/x/y/z/a/b/c")));

        // XXX Same scenario as above. While the path matches the expression,
        //     such a prim does not exist in the scene index.
        ASSERT_TRUE(eval.Match(SdfPath("/a/b/PrimDoesNotExist/x/y/z/a")));
    }
}

TEST(TestHydra, test_predicate_library) {
    HdSceneIndexBaseRefPtr si = _CreateTestScene();

    // prim type queries.
    {
        // Match prims with type "scope".
        {
            const SdfPathExpression expr("//{hdType:scope}");
            HdCollectionExpressionEvaluator eval(si, expr);
            // ^ This will use the predicate library that ships with hd.

            ASSERT_TRUE(eval.Match(SdfPath("/A")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/C")));

            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Carrot")));
        }

        // Match children of any prim "B" whose type is "fruit".
        // "type" is deprecated, but let's test it nonetheless.
        {
            const SdfPathExpression expr("//B/{type:fruit}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Tomato")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Apricot")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Carrot")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/C")));
        }
    }

    // locator presence queries
    {
        // Match prims whose prim container has a data source at "purpose"
        {
            const SdfPathExpression expr("//{hdHasDataSource:purpose}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/C/Table")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A/B")));
            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A")));
        }

        // Match prims that have a data source at "materialBindings.''".
        // i.e. match prims with an allPurpose (empty token) binding.
        {
            const SdfPathExpression expr("//{hdHasDataSource:\"materialBindings.\"}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Broccoli")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Apricot")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Tomato")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/B")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/C/Chair1")));
            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
        }
    }

    // primvar presence queries
    {
        // Match prims that have a primvar "fresh".
        {
            const SdfPathExpression expr("//{hdHasPrimvar:fresh}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Tomato")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Broccoli")));
            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A")));
        }

        // Match prims that have a namespaced primvar "foo:glossy".
        {
            // "hasPrimvar" is deprecated, but let's test it nonetheless.
            const SdfPathExpression expr("//{hasPrimvar:'foo:glossy'}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Tomato")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/C/Chair1")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Broccoli")));
            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A")));
        }
    }

    // purpose queries.
    {
        // Match prims with purpose "food".
        {
            const SdfPathExpression expr("//{hdPurpose:food}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Broccoli")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A")));
            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/C/Table")));
        }

        // Match prims with purpose "furniture".
        {
            const SdfPathExpression expr("//{hdPurpose:furniture}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/C/Table")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/C/Chair2")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Tomato")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Apricot")));
        }
    }

    // visibility queries.
    {
        // Match all visible prims.
        {
            const SdfPathExpression expr("//{hdVisible:true}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/C/Table")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Broccoli")));

            // If visibility is not authored, predicate should return false.
            ASSERT_TRUE(!eval.Match(SdfPath("/A")));
            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/C/Chair2")));
        }

        // Alias for the above query. This is equivalent to the test case above.
        {
            const SdfPathExpression expr("//{hdVisible}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/C/Table")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Broccoli")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A")));
            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/C/Chair2")));
        }
    }

    // material binding queries
    {
        // Match prims bound to a material whose path contains "Orange".
        // This queries only the allPurpose binding currently.
        // We could improve the predicate to take the purpose as an additional
        // arg.
        {
            const SdfPathExpression expr("//{hdHasMaterialBinding:\"Orange\"}");
            HdCollectionExpressionEvaluator eval(si, expr);

            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
            ASSERT_TRUE(eval.Match(SdfPath("/A/B/Apricot")));

            ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Tomato")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/B")));
            ASSERT_TRUE(!eval.Match(SdfPath("/A/C/Chair1")));
            ASSERT_TRUE(!eval.Match(SdfPath("/PrimDoesNotExist/C")));
        }
    }
}

TEST(TestHydra, test_custom_predicate_library) {
    HdSceneIndexBaseRefPtr si = _CreateTestScene();

    // Match prims that are deemed "eatable".
    {
        const SdfPathExpression expr("//{eatable:true}");
        HdCollectionExpressionEvaluator eval(si, expr, _GetCustomPredicateLibrary());

        ASSERT_TRUE(eval.Match(SdfPath("/A/B/Tomato")));
        ASSERT_TRUE(eval.Match(SdfPath("/A/B/Apricot")));
        ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
        ASSERT_TRUE(!eval.Match(SdfPath("/A/C")));
        ASSERT_TRUE(!eval.Match(SdfPath("/A/C/Chair")));
    }

    // Test predicate alias. This is equivalent to the test case above.
    {
        const SdfPathExpression expr("//{eatable}");
        HdCollectionExpressionEvaluator eval(si, expr, _GetCustomPredicateLibrary());

        ASSERT_TRUE(eval.Match(SdfPath("/A/B/Tomato")));
        ASSERT_TRUE(eval.Match(SdfPath("/A/B/Apricot")));
        ASSERT_TRUE(eval.Match(SdfPath("/A/B/Carrot")));
        ASSERT_TRUE(!eval.Match(SdfPath("/A/C")));
        ASSERT_TRUE(!eval.Match(SdfPath("/A/C/Chair")));
    }

    // Foundational predicates should continue to work.
    // Match prims with purpose "furniture".
    {
        const SdfPathExpression expr("//{hdPurpose:furniture}");
        HdCollectionExpressionEvaluator eval(si, expr, _GetCustomPredicateLibrary());
        ASSERT_TRUE(eval.Match(SdfPath("/A/C/Table")));
        ASSERT_TRUE(eval.Match(SdfPath("/A/C/Chair2")));

        ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Tomato")));
        ASSERT_TRUE(!eval.Match(SdfPath("/A/B/Apricot")));
    }
}

TEST(TestHydra, test_evaluator_utilities) {
    HdSceneIndexBaseRefPtr si = _CreateTestScene();

    // Match all prims with purpose "food" and a primvar "fresh".
    {
        const SdfPathExpression expr("//{hdPurpose:food and hdHasPrimvar:fresh}");
        HdCollectionExpressionEvaluator eval(si, expr);

        SdfPathVector resultVec;
        eval.PopulateAllMatches(SdfPath::AbsoluteRootPath(), &resultVec);

        SdfPathSet result(resultVec.begin(), resultVec.end());
        const SdfPathSet expected = {
                SdfPath("/A/B/Carrot"),
                SdfPath("/A/B/Tomato"),
        };
        ASSERT_TRUE(result == expected);
    }

    // Match all prims that have an authored visibility opinion and are
    // invisible. This is redundant right now, since the predicate returns false
    // for prims that don't have a visibility opinion. If we change that
    // behavior (to use a fallback for example), this test case should catch it.
    {
        const SdfPathExpression expr("//{hdHasDataSource:visibility and hdVisible:false}");
        HdCollectionExpressionEvaluator eval(si, expr);

        SdfPathVector resultVec;
        eval.PopulateAllMatches(SdfPath::AbsoluteRootPath(), &resultVec);

        // The set isn't necessary here, but future proofing just in case...
        SdfPathSet result(resultVec.begin(), resultVec.end());
        const SdfPathSet expected = {
                SdfPath("/A/C/Chair2"),
        };
        ASSERT_TRUE(result == expected);
    }

    // Test PopulateMatches with supported "MatchKind" options.
    {
        // Populate test scene index.
        HdRetainedSceneIndexRefPtr si = HdRetainedSceneIndex::New();

        // Ancestors are implicitly added. Prim type isn't relevant for this
        // test case.
        si->AddPrims({
                {SdfPath("/a/foobar/b"), _primTypeTokens->b, nullptr},
                {SdfPath("/a/foobar/bar"), _primTypeTokens->bar, nullptr},
                {SdfPath("/a/foobar/baz"), _primTypeTokens->baz, nullptr},
        });
        // This scene index would contain:
        // {"/a", "/a/foobar", "/a/foobar/b", "/a/foobar/bar", "/a/foobar/baz"}
        //

        const SdfPathExpression expr("//*bar");
        HdCollectionExpressionEvaluator eval(si, expr);

        {
            // MatchAll matches what we'd expect. Any prim whose path ends with
            // "bar".
            SdfPathVector resultVec;
            eval.PopulateMatches(SdfPath::AbsoluteRootPath(), HdCollectionExpressionEvaluator::MatchAll, &resultVec);

            SdfPathSet result(resultVec.begin(), resultVec.end());
            const SdfPathSet expected = {SdfPath("/a/foobar"), SdfPath("/a/foobar/bar")};

            ASSERT_TRUE(result == expected);
        }

        {
            // We'd skip traversal/evaluation for "/a/foobar/bar".
            SdfPathVector resultVec;
            eval.PopulateMatches(SdfPath::AbsoluteRootPath(), HdCollectionExpressionEvaluator::ShallowestMatches,
                                 &resultVec);

            SdfPathSet result(resultVec.begin(), resultVec.end());
            const SdfPathSet expected = {SdfPath("/a/foobar")};

            ASSERT_TRUE(result == expected);
        }

        {
            // We add all descendants of "/a/foobar" because it matches the
            // expression.
            SdfPathVector resultVec;
            eval.PopulateMatches(SdfPath::AbsoluteRootPath(),
                                 HdCollectionExpressionEvaluator::ShallowestMatchesAndAllDescendants, &resultVec);

            SdfPathSet result(resultVec.begin(), resultVec.end());
            const SdfPathSet expected = {SdfPath("/a/foobar"), SdfPath("/a/foobar/b"), SdfPath("/a/foobar/bar"),
                                         SdfPath("/a/foobar/baz")};

            ASSERT_TRUE(result == expected);
        }
    }
}
