//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/pxr.h"
#include "pxr/imaging/hd/extComputation.h"
#include "pxr/imaging/hd/extComputationUtils.h"
#include "pxr/usd/sdf/path.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using HdExtComputationSharedPtr = std::shared_ptr<HdExtComputation>;

void PrintComputations(HdExtComputationConstPtrVector const& comps, std::string const& prefix) {
    std::cout << std::endl << prefix << " Computation Order: ";
    for (auto const& comp : comps) {
        std::cout << comp->GetId() << ", ";
    }
    std::cout << std::endl;
}

bool OccursBefore(HdExtComputationConstPtrVector const& comps,
                  HdExtComputationConstPtr comp1,
                  HdExtComputationConstPtr comp2) {
    auto it1 = std::find(comps.begin(), comps.end(), comp1);
    auto it2 = std::find(comps.begin(), comps.end(), comp2);

    return (it1 < it2);
}

TEST(TestHydra, test_linear_chain_dependency) {
    // Simple linear chain of computations:
    // A <-- B <-- C
    // Read as A depends on B, B depends on C, C does not depend on anything.
    // i.e., A takes as input one or more output(s) of B
    //       B takes as input one or more output(s) of C
    HdExtComputationSharedPtr compA(new HdExtComputation(SdfPath("A")));
    HdExtComputationSharedPtr compB(new HdExtComputation(SdfPath("B")));
    HdExtComputationSharedPtr compC(new HdExtComputation(SdfPath("C")));
    HdExtComputationUtils::ComputationDependencyMap cdm;
    cdm[compA.get()] = {compB.get()};
    cdm[compB.get()] = {compC.get()};
    cdm[compC.get()] = {};

    HdExtComputationUtils::PrintDependencyMap(cdm);

    HdExtComputationConstPtrVector expectedOrder = {compC.get(), compB.get(), compA.get()};
    PrintComputations(expectedOrder, "Expected");

    HdExtComputationConstPtrVector sortedComps;
    bool success = HdExtComputationUtils::DependencySort(cdm, &sortedComps);

    PrintComputations(sortedComps, "Sorted");

    ASSERT_TRUE(success && (sortedComps == expectedOrder));
}

TEST(TestHydra, test_tree_chain_dependency) {
    // Tree chain of computations:
    // A <-- B <-- C
    // ^     ^
    // |     '-- D <-- E
    // '-- F
    // Read as A depends on B and F,
    //         B depends on C and D,
    //         D depends on E
    //         C, E and F do not depend on anything.
    HdExtComputationSharedPtr compA(new HdExtComputation(SdfPath("A")));
    HdExtComputationSharedPtr compB(new HdExtComputation(SdfPath("B")));
    HdExtComputationSharedPtr compC(new HdExtComputation(SdfPath("C")));
    HdExtComputationSharedPtr compD(new HdExtComputation(SdfPath("D")));
    HdExtComputationSharedPtr compE(new HdExtComputation(SdfPath("E")));
    HdExtComputationSharedPtr compF(new HdExtComputation(SdfPath("F")));
    HdExtComputationUtils::ComputationDependencyMap cdm;
    cdm[compA.get()] = {compB.get(), compF.get()};
    cdm[compB.get()] = {compC.get(), compD.get()};
    cdm[compD.get()] = {compE.get()};
    cdm[compC.get()] = {};
    cdm[compE.get()] = {};
    cdm[compF.get()] = {};

    HdExtComputationUtils::PrintDependencyMap(cdm);

    HdExtComputationConstPtrVector sortedComps;
    bool success = HdExtComputationUtils::DependencySort(cdm, &sortedComps);
    PrintComputations(sortedComps, "Sorted");

    // We can't compare with an "expected ordering" since it isn't a simple
    // linear chain. Just ensure depdendencies are handled.
    ASSERT_TRUE(success && OccursBefore(sortedComps, compF.get(), compA.get()) &&
                OccursBefore(sortedComps, compC.get(), compB.get()) &&
                OccursBefore(sortedComps, compE.get(), compB.get()) &&
                OccursBefore(sortedComps, compC.get(), compB.get()));
}

TEST(TestHydra, test_cycle_dependency) {
    // Chain of computations with a cycle:
    // A <-- B  -->  C
    // ^     ^       |
    // |     '       v
    //       '------ D  <-- E
    // '-- F
    // Read as A depends on B and F,
    //         B depends on D,
    //         C depends on B,
    //         D depends on C and E
    //         E and F do not depend on anything.
    HdExtComputationSharedPtr compA(new HdExtComputation(SdfPath("A")));
    HdExtComputationSharedPtr compB(new HdExtComputation(SdfPath("B")));
    HdExtComputationSharedPtr compC(new HdExtComputation(SdfPath("C")));
    HdExtComputationSharedPtr compD(new HdExtComputation(SdfPath("D")));
    HdExtComputationSharedPtr compE(new HdExtComputation(SdfPath("E")));
    HdExtComputationSharedPtr compF(new HdExtComputation(SdfPath("F")));
    HdExtComputationUtils::ComputationDependencyMap cdm;
    cdm[compA.get()] = {compB.get(), compF.get()};
    cdm[compB.get()] = {compD.get()};
    cdm[compC.get()] = {compB.get()};
    cdm[compD.get()] = {compC.get(), compE.get()};
    cdm[compE.get()] = {};
    cdm[compF.get()] = {};

    HdExtComputationUtils::PrintDependencyMap(cdm);

    HdExtComputationConstPtrVector sortedComps;
    ASSERT_FALSE(HdExtComputationUtils::DependencySort(cdm, &sortedComps));
}
