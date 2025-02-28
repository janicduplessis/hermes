/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if (                                              \
    defined(HERMESVM_GC_NONCONTIG_GENERATIONAL) || \
    defined(HERMESVM_GC_HADES)) &&                 \
    defined(NDEBUG) && !defined(HERMESVM_ALLOW_HUGE_PAGES)

#include "gtest/gtest.h"

#include "EmptyCell.h"
#include "TestHelpers.h"

#include "hermes/VM/AlignedHeapSegment.h"

using namespace hermes::vm;
using namespace hermes::vm::detail;

namespace {

const GCConfig kGCConfig = TestGCConfigFixedSize(16 << 20);

TEST(GCReturnUnusedMemoryTest, CollectReturnsFreeMemory) {
  // TODO(T40416012) Re-enable this test when vm_unused is fixed.
  // Skip this test in Windows because vm_unused has a no-op implementation.
#ifndef _WINDOWS
  // Use an mmap-based storage for this test.
  std::unique_ptr<StorageProvider> provider = StorageProvider::mmapProvider();
  auto runtime = DummyRuntime::create(kGCConfig, std::move(provider));
  DummyRuntime &rt = *runtime;
  auto &gc = rt.getHeap();

  using SemiCell = EmptyCell<AlignedHeapSegment::maxSize() * 8 / 10>;

  // Allocate cells directly in the old generation.
  auto *cell1 = SemiCell::createLongLived(rt);
  rt.pointerRoots.push_back(reinterpret_cast<GCCell **>(&cell1));

  auto *cell2 = SemiCell::createLongLived(rt);
  rt.pointerRoots.push_back(reinterpret_cast<GCCell **>(&cell2));

  auto *cell3 = SemiCell::createLongLived(rt);
  rt.pointerRoots.push_back(reinterpret_cast<GCCell **>(&cell3));

  auto before = gc.getVMFootprintForTest();
  ASSERT_TRUE(before);

  // Make the pages dirty
  (void)cell1->touch();
  (void)cell2->touch();

  auto touched = gc.getVMFootprintForTest();
  ASSERT_TRUE(touched);

  rt.pointerRoots.clear();

  // Collect should return the unused memory back to the OS.
  rt.collect();
#ifdef HERMESVM_GC_HADES
  // Hades can only return memory after a compaction. The very first collection
  // will just free up the originally allocated memory.
  // This collection will identify the segment to compact and prepare it.
  rt.collect();
  // This collection will actually compact the segment.
  rt.collect();
#endif

  auto collected = gc.getVMFootprintForTest();
  ASSERT_TRUE(collected);

  EXPECT_LT(*before, *touched);
  EXPECT_GT(*touched, *collected);
#endif // _WINDOWS
}

} // namespace

#endif
