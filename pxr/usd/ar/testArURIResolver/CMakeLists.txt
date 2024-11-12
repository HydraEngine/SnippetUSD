#  Copyright (c) 2024 Feng Yang
#
#  I am making my contributions/submissions to this project solely in my
#  personal capacity and am not conveying any rights to any intellectual
#  property of any third parties.

usd_library(TestArURIResolver_plugin
        LIBRARIES
        ${PXR_LIBRARY_NAMES}

        PUBLIC_HEADERS
        TestArURIResolver_plugin.h

        CPPFILES
        TestArURIResolver_plugin.cpp

        RESOURCE_FILES
        plugInfo.json
)

usd_executable(testArURIResolver-CPP
        CPPFILES
        testArURIResolver.cpp

        LIBRARIES
        ${PXR_LIBRARY_NAMES}
)

add_dependencies(testArURIResolver-CPP TestArURIResolver_plugin TestArPackageResolver_plugin)