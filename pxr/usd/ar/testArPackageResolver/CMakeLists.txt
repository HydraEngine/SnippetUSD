#  Copyright (c) 2024 Feng Yang
#
#  I am making my contributions/submissions to this project solely in my
#  personal capacity and am not conveying any rights to any intellectual
#  property of any third parties.

usd_library(TestArPackageResolver_plugin
        LIBRARIES
        ${PXR_LIBRARY_NAMES}

        CPPFILES
        TestArPackageResolver_plugin.cpp

        RESOURCE_FILES
        plugInfo.json
)