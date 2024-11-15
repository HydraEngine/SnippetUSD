//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/pxr.h"

#include "pxr/imaging/hd/dataSource.h"
#include "pxr/imaging/hd/utils.h"
#include "pxr/base/tf/errorMark.h"

#include <iostream>
#include <fstream>
#include <string>
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

static bool BasicTest() {
    // Create a representation of a material network
    HdMaterialNetwork materialNetwork;
    materialNetwork.nodes.reserve(3);

    const SdfPath materialPath("/Asset/Looks/Material");

    HdMaterialNode textureNode;
    textureNode.path = SdfPath("/Asset/Looks/Material/Texture");
    textureNode.identifier = TfToken("Texture_5");
    textureNode.parameters[TfToken("inputs:filename")] = VtValue("studio/patterns/checkerboard/checkerboard.tex");
    materialNetwork.nodes.push_back(textureNode);

    HdMaterialNode materialLayerNode;
    materialLayerNode.path = SdfPath("/Asset/Looks/Material/MaterialLayer");
    materialLayerNode.identifier = TfToken("MaterialLayer_3");
    materialNetwork.nodes.push_back(materialLayerNode);

    HdMaterialNode standInNode;
    standInNode.path = SdfPath("/Asset/Looks/Material/StandIn");
    standInNode.identifier = TfToken("PbsNetworkMaterialStandIn_3");
    materialNetwork.nodes.push_back(standInNode);

    // Connect the nodes
    HdMaterialRelationship textureMaterialLayerRel;
    textureMaterialLayerRel.inputId = textureNode.path;
    textureMaterialLayerRel.inputName = TfToken("resultRGB");
    textureMaterialLayerRel.outputId = materialLayerNode.path;
    textureMaterialLayerRel.outputName = TfToken("albedo");
    materialNetwork.relationships.push_back(textureMaterialLayerRel);

    HdMaterialRelationship materialLayerStandInRel;
    materialLayerStandInRel.inputId = materialLayerNode.path;
    materialLayerStandInRel.inputName = TfToken("pbsMaterialOut");
    materialLayerStandInRel.outputId = standInNode.path;
    materialLayerStandInRel.outputName = TfToken("multiMaterialIn");
    materialNetwork.relationships.push_back(materialLayerStandInRel);

    HdMaterialNetworkMap networkMap;
    networkMap.map[TfToken("surface")] = materialNetwork;

    HdContainerDataSourceHandle ds = HdUtils::ConvertHdMaterialNetworkToHdMaterialSchema(networkMap);

    std::ofstream outdata("testHdUtils_material.txt", std::ios::out);
    HdDebugPrintDataSource(outdata, ds);
    outdata.close();

    return true;
}

TEST(TestHydra, test_utils) {
    TfErrorMark mark;
    bool success = BasicTest();

    TF_VERIFY(mark.IsClean());
    ASSERT_TRUE(success && mark.IsClean());
}
