//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/renderIndex.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include <iostream>
#include <cmath>
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

static bool _IsClose(double a, double b) {
    return std::abs(a - b) < 0.0000001;
}

TEST(TestHydra, counter_test) {
    HdPerfLog& perfLog = HdPerfLog::GetInstance();
    TfToken foo("foo");
    TfToken bar("bar");

    // Make sure the log is disabled.
    perfLog.Disable();

    // Performance logging is disabled, expect no tracking
    perfLog.IncrementCounter(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    perfLog.DecrementCounter(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    perfLog.AddCounter(foo, 5);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    perfLog.SubtractCounter(foo, 6);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    // Macros
    HD_PERF_COUNTER_DECR(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    HD_PERF_COUNTER_INCR(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    HD_PERF_COUNTER_SET(foo, 42);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    HD_PERF_COUNTER_ADD(foo, 5);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    HD_PERF_COUNTER_SUBTRACT(foo, 6);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);

    // Enable logging
    perfLog.Enable();
    // Still expect zero
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);

    // Incr, Decr, Set
    perfLog.IncrementCounter(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 1);
    perfLog.DecrementCounter(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    perfLog.SetCounter(foo, 42);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 42);
    perfLog.AddCounter(foo, 5);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 47);
    perfLog.SubtractCounter(foo, 6);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);

    perfLog.SetCounter(bar, .1);
    ASSERT_TRUE(_IsClose(perfLog.GetCounter(bar), .1));
    perfLog.IncrementCounter(bar);
    ASSERT_TRUE(_IsClose(perfLog.GetCounter(bar), 1.1));
    perfLog.DecrementCounter(bar);
    ASSERT_TRUE(_IsClose(perfLog.GetCounter(bar), 0.1));

    perfLog.SetCounter(foo, 0);
    perfLog.SetCounter(bar, 0);

    // Macros
    HD_PERF_COUNTER_DECR(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == -1);
    HD_PERF_COUNTER_INCR(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 0);
    HD_PERF_COUNTER_SET(foo, 42);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 42);
    HD_PERF_COUNTER_DECR(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);
    HD_PERF_COUNTER_INCR(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 42);
    HD_PERF_COUNTER_ADD(foo, 5);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 47);
    HD_PERF_COUNTER_SUBTRACT(foo, 6);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);

    HD_PERF_COUNTER_SET(bar, 0.1);
    ASSERT_TRUE(_IsClose(perfLog.GetCounter(bar), 0.1));
    HD_PERF_COUNTER_DECR(bar);
    ASSERT_TRUE(_IsClose(perfLog.GetCounter(bar), -0.9));
    HD_PERF_COUNTER_INCR(bar);
    ASSERT_TRUE(_IsClose(perfLog.GetCounter(bar), 0.1));

    // When the log is disabled, we expect to still be able to read the existing
    // values.
    perfLog.Disable();
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);
    perfLog.IncrementCounter(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);
    perfLog.DecrementCounter(foo);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);
    perfLog.SetCounter(foo, 0);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);
    perfLog.AddCounter(foo, 5);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);
    perfLog.SubtractCounter(foo, 6);
    ASSERT_TRUE(perfLog.GetCounter(foo) == 41);
}

TEST(TestHydra, cache_test) {
    HdPerfLog& perfLog = HdPerfLog::GetInstance();
    TfToken foo("foo");
    TfToken bar("bar");
    SdfPath id("/Some/Path");
    TfTokenVector emptyNames;
    TfTokenVector populatedNames;
    populatedNames.push_back(bar);
    populatedNames.push_back(foo);

    // Make sure the log is disabled.
    perfLog.Disable();

    // Performance logging is disabled, expect no tracking.
    ASSERT_TRUE(perfLog.GetCacheHits(foo) == 0);
    ASSERT_TRUE(perfLog.GetCacheMisses(foo) == 0);
    ASSERT_TRUE(perfLog.GetCacheHitRatio(foo) == 0);
    ASSERT_TRUE(perfLog.GetCacheHits(bar) == 0);
    ASSERT_TRUE(perfLog.GetCacheMisses(bar) == 0);
    ASSERT_TRUE(perfLog.GetCacheHitRatio(bar) == 0);
    ASSERT_TRUE(perfLog.GetCacheNames() == emptyNames);

    // Enable perf logging.
    perfLog.Enable();
    // Nothing should have changed yet.
    ASSERT_TRUE(perfLog.GetCacheHits(foo) == 0);
    ASSERT_TRUE(perfLog.GetCacheMisses(foo) == 0);
    ASSERT_TRUE(perfLog.GetCacheHitRatio(foo) == 0);
    ASSERT_TRUE(perfLog.GetCacheHits(bar) == 0);
    ASSERT_TRUE(perfLog.GetCacheMisses(bar) == 0);
    ASSERT_TRUE(perfLog.GetCacheHitRatio(bar) == 0);
    ASSERT_TRUE(perfLog.GetCacheNames() == emptyNames);

    perfLog.AddCacheHit(foo, id);
    perfLog.AddCacheHit(foo, id);
    perfLog.AddCacheMiss(foo, id);
    perfLog.AddCacheMiss(foo, id);
    ASSERT_TRUE(perfLog.GetCacheHits(foo) == 2);
    ASSERT_TRUE(perfLog.GetCacheMisses(foo) == 2);
    ASSERT_TRUE(_IsClose(perfLog.GetCacheHitRatio(foo), .5));

    ASSERT_TRUE(perfLog.GetCacheHits(bar) == 0);
    ASSERT_TRUE(perfLog.GetCacheMisses(bar) == 0);
    ASSERT_TRUE(_IsClose(perfLog.GetCacheHitRatio(bar), 0));
    perfLog.AddCacheHit(bar, id);
    perfLog.AddCacheHit(bar, id);
    perfLog.AddCacheHit(bar, id);
    perfLog.AddCacheMiss(bar, id);
    ASSERT_TRUE(perfLog.GetCacheHits(bar) == 3);
    ASSERT_TRUE(perfLog.GetCacheMisses(bar) == 1);
    ASSERT_TRUE(_IsClose(perfLog.GetCacheHitRatio(bar), .75));

    if (perfLog.GetCacheNames() != populatedNames) {
        TfTokenVector names = perfLog.GetCacheNames();
        TF_FOR_ALL(i, names) {
            std::cout << *i << "\n";
        }
    }
    ASSERT_TRUE(perfLog.GetCacheNames() == populatedNames);

    // Make sure the log is disabled.
    perfLog.Disable();

    // We still expect to read results, even when disabled.
    ASSERT_TRUE(perfLog.GetCacheHits(foo) == 2);
    ASSERT_TRUE(perfLog.GetCacheMisses(foo) == 2);
    ASSERT_TRUE(_IsClose(perfLog.GetCacheHitRatio(foo), .5));
    ASSERT_TRUE(perfLog.GetCacheHits(bar) == 3);
    ASSERT_TRUE(perfLog.GetCacheMisses(bar) == 1);
    ASSERT_TRUE(_IsClose(perfLog.GetCacheHitRatio(bar), .75));

    if (perfLog.GetCacheNames() != populatedNames) {
        TfTokenVector names = perfLog.GetCacheNames();
        TF_FOR_ALL(i, names) {
            std::cout << *i << "\n";
        }
    }
    ASSERT_TRUE(perfLog.GetCacheNames() == populatedNames);
}
