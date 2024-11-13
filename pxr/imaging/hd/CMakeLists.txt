#  Copyright (c) 2024 Feng Yang
#
#  I am making my contributions/submissions to this project solely in my
#  personal capacity and am not conveying any rights to any intellectual
#  property of any third parties.

usd_executable(TestHydra
        CPPFILES
        testHdBufferSourceEmptyVal.cpp
        testHdBufferSpec.cpp
        testHdCollectionExpressionEvaluator.cpp
        testHdCommand.cpp
        testHdContainerDataSourceUtils.cpp
        testHdDataSource.cpp
        testHdDataSourceLocator.cpp
        testHdDirtyBitsTranslator.cpp
        testHdDirtyList.cpp
        testHdExtCompDependencySort.cpp
        testHdExtComputationUtils.cpp
        testHdMergingSceneIndex.cpp
        testHdPerfLog.cpp
        testHdSceneIndex.cpp
        testHdSortedIds.cpp
        testHdSortedIdsPerf.cpp

        LIBRARIES
        ${PXR_LIBRARY_NAMES}
        GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
)