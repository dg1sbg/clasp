/*
    File: gcalloc.cc
*/

/*
Copyright (c) 2014, Christian E. Schafmeister

CLASP is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

See directory 'clasp/licenses' for full details.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* -^- */
#include <clasp/core/foundation.h>
#include <clasp/gctools/memoryManagement.h>
#include <clasp/gctools/gcalloc.h>

namespace gctools {

/*! Used to signal recursive allocations */
int global_recursive_allocation_counter = 0;

void* malloc_kind_error(uintptr_t expected_kind, uintptr_t kind, uintptr_t size, uintptr_t stmp, void* addr) {
  // print message and abort
  printf("%s:%d Got an unexpected kind: %lu size: %lu stamp: %lu addr: %p   expected: %lu\n", __FILE__, __LINE__, kind, size, stmp,
         addr, expected_kind);
  abort();
}

__attribute__((noinline))
void AllocationProfiler::registerAllocationSlow(stamp_t stamp,
                                                size_t size,
                                                uint32_t flags) {
  size_t threshold = this->_AllocationSizeThreshold;
  if (threshold == 0) return;

  size_t counter = static_cast<size_t>(this->_AllocationSizeCounter);
  if (counter < threshold) return;

  size_t sampled_bytes = counter - (counter % threshold);
  this->_AllocationSizeCounter =
    static_cast<int64_t>(counter - sampled_bytes);

  // Always normalize the polling counter before checking profiler state.
  // It therefore remains a bounded modulo counter even while profiling is
  // inactive.
  uint64_t session_epoch = 0;
  size_t bytes_per_sample = 0;
  if (!core::allocation_profiler_session(session_epoch,
                                         bytes_per_sample))
    return;

  if (this->_AllocationProfileEpoch != session_epoch) {
    this->_AllocationProfileEpoch = session_epoch;
    this->_AllocationProfileBytesPending = 0;

    // The old polling remainder may contain bytes allocated before this
    // session. Discard that ambiguous prefix, but retain every complete
    // polling quantum belonging to a large triggering allocation.
    sampled_bytes = size - (size % threshold);
    this->_AllocationSizeCounter =
      static_cast<int64_t>(size % threshold);
  }

  if (sampled_bytes == 0 || bytes_per_sample == 0) return;

  size_t pending =
    this->_AllocationProfileBytesPending + sampled_bytes;
  size_t attributed_bytes =
    pending - (pending % bytes_per_sample);
  this->_AllocationProfileBytesPending =
    pending - attributed_bytes;

  if (attributed_bytes == 0) return;

  // The recorder must never recursively sample an allocation it causes.
  if (this->_InAllocationProfiler) return;

  this->_InAllocationProfiler = true;
  core::allocation_profiler_record(
    static_cast<uint32_t>(stamp), size, attributed_bytes, flags,
    session_epoch);
  this->_InAllocationProfiler = false;
}
}; // namespace gctools
