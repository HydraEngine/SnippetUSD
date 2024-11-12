//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/base/arch/export.h"
#include "pxr/pxr.h"
#include "testArchAbi.h"

PXR_NAMESPACE_USING_DIRECTIVE

extern "C" {

ARCH_EXPORT ArchAbiBase2* newDerived() {
    return new ArchAbiDerived<int>;
}
}
