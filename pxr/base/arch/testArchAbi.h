//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/pxr.h"
#include "pxr/base/arch/api.h"

PXR_NAMESPACE_OPEN_SCOPE

struct ArchAbiBase1 {
    void* dummy;
};

struct ArchAbiBase2 {
    virtual ~ArchAbiBase2() = default;
    [[nodiscard]] virtual const char* name() const = 0;
};

template <class T>
struct ArchAbiDerived : public ArchAbiBase1, public ArchAbiBase2 {
    ~ArchAbiDerived() override = default;
    [[nodiscard]] const char* name() const override { return "ArchAbiDerived"; }
};

PXR_NAMESPACE_CLOSE_SCOPE
