//  Copyright (c) 2024 Feng Yang
//
//  I am making my contributions/submissions to this project solely in my
//  personal capacity and am not conveying any rights to any intellectual
//  property of any third parties.

#include "pxr/imaging/hd/filteringSceneIndex.h"
#include "pxr/imaging/hd/mergingSceneIndex.h"
#include "pxr/imaging/hd/retainedSceneIndex.h"

#include "pxr/base/tf/declarePtrs.h"

#include <iostream>
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

template <typename T>
void _CompareValue(const char* msg, const T& v1, const T& v2) {
    if (v1 == v2) {
        std::cout << msg << " matches." << std::endl;
    } else {
        //        std::cerr << msg << " doesn't match. Expecting " << v2 << " got " << v1 << std::endl;
        GTEST_FAIL();
    }
}

TF_DECLARE_REF_PTRS(_MySceneIndex);

using _LogEntry = std::tuple<std::string, SdfPath>;

static std::ostream& operator<<(std::ostream& out, const _LogEntry& entry) {
    auto [what, path] = entry;
    out << "LogEntry(" << what << ", " << path << ")";
    return out;
}

template <typename T>
static std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
    out << "[";
    for (const auto& elem : v) {
        out << elem << ", ";
    }
    out << "]";
    return out;
}

class _MySceneIndex final : public HdSingleInputFilteringSceneIndexBase {
public:
    static _MySceneIndexRefPtr New(const HdSceneIndexBaseRefPtr& inputScene) {
        return TfCreateRefPtr(new _MySceneIndex(inputScene));
    }

    void Disable() {
        _enabled = false;

        HdSceneIndexObserver::RemovedPrimEntries entries;
        entries.emplace_back(SdfPath("/"));
        _SendPrimsRemoved(entries);
    }

    HdSceneIndexPrim GetPrim(const SdfPath& primPath) const override {
        if (_enabled) {
            if (auto si = _GetInputSceneIndex()) {
                return si->GetPrim(primPath);
            }
        }
        return {};
    }

    SdfPathVector GetChildPrimPaths(const SdfPath& primPath) const override {
        if (_enabled) {
            if (auto si = _GetInputSceneIndex()) {
                return si->GetChildPrimPaths(primPath);
            }
        }
        return {};
    }

protected:
    _MySceneIndex(const HdSceneIndexBaseRefPtr& inputScene) : HdSingleInputFilteringSceneIndexBase(inputScene) {}
    ~_MySceneIndex() override = default;

    void _PrimsAdded(const HdSceneIndexBase& sender, const HdSceneIndexObserver::AddedPrimEntries& entries) override {
        if (_enabled) {
            _SendPrimsAdded(entries);
        }
    }

    void _PrimsRemoved(const HdSceneIndexBase& sender,
                       const HdSceneIndexObserver::RemovedPrimEntries& entries) override {
        if (_enabled) {
            _SendPrimsRemoved(entries);
        }
    }

    void _PrimsDirtied(const HdSceneIndexBase& sender,
                       const HdSceneIndexObserver::DirtiedPrimEntries& entries) override {
        if (_enabled) {
            _SendPrimsDirtied(entries);
        }
    }

private:
    bool _enabled = true;
};

class _Logger : public HdSceneIndexObserver {
public:
    void Reset() { _log.clear(); }

public:
    void PrimsAdded(const HdSceneIndexBase& sender, const AddedPrimEntries& entries) override {
        for (const AddedPrimEntry& entry : entries) {
            _log.emplace_back("add", entry.primPath);
        }
    }

    void PrimsRemoved(const HdSceneIndexBase& sender, const RemovedPrimEntries& entries) override {
        for (const RemovedPrimEntry& entry : entries) {
            _log.emplace_back("remove", entry.primPath);
        }
    }

    void PrimsDirtied(const HdSceneIndexBase& sender, const DirtiedPrimEntries& entries) override {
        for (const DirtiedPrimEntry& entry : entries) {
            _log.emplace_back("dirty", entry.primPath);
        }
    }

    void PrimsRenamed(const HdSceneIndexBase& sender, const RenamedPrimEntries& entries) override {
        for (const RenamedPrimEntry& entry : entries) {
            _log.emplace_back("rename", entry.oldPrimPath);
        }
    }

    std::vector<_LogEntry> GetLog() const { return _log; }

private:
    std::vector<std::tuple<std::string, SdfPath>> _log;
};

TEST(TestHydra, test_notices_after_remove) {
    HdRetainedSceneIndexRefPtr siA = HdRetainedSceneIndex::New();
    siA->AddPrims({{SdfPath("/Parent"), TfToken("A"), nullptr}, {SdfPath("/Parent/Child"), TfToken("A"), nullptr}});

    HdRetainedSceneIndexRefPtr siB = HdRetainedSceneIndex::New();
    siB->AddPrims({{SdfPath("/Parent"), TfToken("B"), nullptr}, {SdfPath("/Parent/Child"), TfToken("B"), nullptr}});

    _MySceneIndexRefPtr dA = _MySceneIndex::New(siA);
    _MySceneIndexRefPtr dB = _MySceneIndex::New(siB);

    // mergingSceneIndex merges 2 scene indices that have the same prim
    // hierarchy, but the "A" branch has type "A" and the "B" branch has type
    // "B".
    HdMergingSceneIndexRefPtr mergingSceneIndex = HdMergingSceneIndex::New();
    static const SdfPath rootPath = SdfPath::AbsoluteRootPath();
    mergingSceneIndex->AddInputScene(dA, rootPath);
    mergingSceneIndex->AddInputScene(dB, rootPath);

    // We attach a logger so we can see what entries get emitted when we disable
    // "A" (the stronger of the input scenes).  When we disable "A", the merging
    // scene index will get a notice that the "A" prims are removed.  Since it
    // is a merging scene index, downstream scene indices should now be seeing
    // all of those prim, but now with type "B".
    _Logger logger;
    mergingSceneIndex->AddObserver(HdSceneIndexObserverPtr(&logger));
    dA->Disable();

    auto logEntries = logger.GetLog();

    _CompareValue("NOTICES", logEntries,
                  {
                          _LogEntry("add", "/"),
                          _LogEntry("add", "/Parent"),
                          _LogEntry("add", "/Parent/Child"),
                  });
}
