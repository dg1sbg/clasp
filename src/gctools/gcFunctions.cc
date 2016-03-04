/* -^- */
#include <boost/mpl/list.hpp>
#ifdef USE_BOEHM
#include <clasp/gc/gc_mark.h>
#endif
#ifdef USE_MPS
extern "C" {
#include <clasp/mps/code/mpscamc.h>
};
#endif

#include <stdint.h>

#include <clasp/core/foundation.h>
#include <clasp/core/object.h>
#include <clasp/core/lisp.h>
#include <clasp/core/instance.h>
#include <clasp/core/builtInClass.h>
#include <clasp/core/fileSystem.h>
#include <clasp/core/environment.h>
#include <clasp/core/standardClass.h>
#include <clasp/core/activationFrame.h>
#include <clasp/core/structureClass.h>
#include <clasp/core/lispStream.h>
#include <clasp/core/str.h>
#include <clasp/core/symbolTable.h>
#include <clasp/gctools/gctoolsPackage.h>
#include <clasp/core/wrappers.h>

namespace gctools {

/*! Hardcode a few kinds of objects for bootstrapping
 */

const char *global_HardcodedKinds[] = {
    "", "core::T_O", "core::StandardObject_O", "core::Metaobject_O", "core::Specializer_O", "core::Class_O", "core::BuiltInClass_O", "core::StdClass_O", "core::StandardClass_O", "core::StructureClass_O", "core::Symbol_O", "core::Str_O"};

CL_DEFUN int gctools__max_bootstrap_kinds() {
  return sizeof(global_HardcodedKinds) / sizeof(global_HardcodedKinds[0]);
}

int iBootstrapKind(const string &name) {
  for (int i(0), iEnd(gctools__max_bootstrap_kinds()); i < iEnd; ++i) {
    //            printf("%s:%d i[%d]Checking if %s == %s\n", __FILE__, __LINE__, i, global_HardcodedKinds[i], name.c_str());
    if (strcmp(global_HardcodedKinds[i], name.c_str()) == 0) {
      return i;
    }
  }
  SIMPLE_ERROR(BF("Illegal bootstrap-kind %s") % name);
}

void initialize_bootstrap_kinds() {
  DEPRECIATED();
// Hard-coded bootstrap kinds
#define SetupKind(_x_) _x_::static_Kind = iBootstrapKind(#_x_)
  SetupKind(core::T_O);
  SetupKind(core::StandardObject_O);
  SetupKind(core::Metaobject_O);
  SetupKind(core::Specializer_O);
  SetupKind(core::Class_O);
  SetupKind(core::BuiltInClass_O);
  SetupKind(core::StdClass_O);
  SetupKind(core::StandardClass_O);
  SetupKind(core::StructureClass_O);
  SetupKind(core::Symbol_O);
  SetupKind(core::Str_O);
}

CL_DEFUN core::Cons_sp gctools__bootstrap_kind_symbols() {
  core::Cons_sp list(_Nil<core::Cons_O>());
  for (int i(gctools__max_bootstrap_kinds() - 1); i > 0; --i) {
    string name = global_HardcodedKinds[i];
    list = core::Cons_O::create(core::Str_O::create(name), list);
  }
  return list;
}

CL_DEFUN core::T_sp gctools__bootstrap_kind_p(const string &name) {
  for (int i(0), iEnd(gctools__max_bootstrap_kinds()); i < iEnd; ++i) {
    //            printf("%s:%d i[%d]Checking if %s == %s\n", __FILE__, __LINE__, i, global_HardcodedKinds[i], name.c_str());
    if (strcmp(global_HardcodedKinds[i], name.c_str()) == 0) {
      return core::make_fixnum(i);
    }
  }
  return _Nil<core::T_O>();
}



CL_DEFUN void gctools__deallocate_unmanaged_instance(core::T_sp obj) {
  obj_deallocate_unmanaged_instance(obj);
}

CL_DEFUN core::T_mv gctools__bytes_allocated() {
  size_t gc_bytes = 0;
#ifdef USE_BOEHM
  gc_bytes = GC_get_total_bytes();
#endif
#ifdef USE_MPS
  gc_bytes = 0; // IMPLEMENT_MEF(BF("Figure out how to get the total bytes allocated using MPS"));
#endif
  size_t my_bytes = globalBytesAllocated;
  ASSERT(gc_bytes < gc::most_positive_fixnum && my_bytes < gc::most_positive_fixnum);
  return Values(core::clasp_make_fixnum(gc_bytes), core::clasp_make_fixnum(my_bytes));
}

#if defined(DEBUG_OBJECT_UNIQUE_ID)
CL_LAMBDA();
CL_DECLARE();
CL_DOCSTRING("Return the header uid for the object");
CL_DEFUN Fixnum core__header_unique_id(core::T_sp obj) {
  if (obj.consp()) {
    return header_pointer(&(*obj))->uid;
  } else if (obj.generalp()) {
    return header_pointer(&(*obj))->uid;
  }
  return -1;
}
#endif //#if defined(DEBUG_OBJECT_UNIQUE_ID)


CL_LAMBDA();
CL_DECLARE();
CL_DOCSTRING("Return the header kind for the object");
CL_DEFUN Fixnum core__header_kind(core::T_sp obj) {
  if (obj.consp()) {
#if defined(USE_BOEHM) && defined(USE_CXX_DYNAMIC_CAST)
    return reinterpret_cast<Fixnum>(&typeid(*obj));
#else
    return gctools::GCKind<core::Cons_O>::Kind;
#endif
  } else if (obj.generalp()) {
#if defined(USE_BOEHM) && defined(USE_CXX_DYNAMIC_CAST)
    return reinterpret_cast<Fixnum>(&typeid(*obj));
#else
    void *mostDerived = gctools::untag_general<void *>(obj.raw_());
    gctools::Header_s *header = reinterpret_cast<gctools::Header_s *>(ClientPtrToBasePtr(mostDerived));
#ifdef USE_MPS
    ASSERT(header->kindP());
#endif
    return header->kind();
#endif
  } else if (obj.fixnump()) {
    return kind_fixnum;
  } else if (obj.single_floatp()) {
    return kind_single_float;
  } else if (obj.characterp()) {
    return kind_character;
  }
  printf("%s:%d HEADER-KIND requested for a non-general object - Clasp needs to define hard-coded kinds for non-general objects - returning -1 for now", __FILE__, __LINE__);
  return core::clasp_make_fixnum(-1);
}

CL_LAMBDA(obj);
CL_DOCSTRING(R"doc(Return true if the object inherits from core:instance based on its header value)doc");
CL_DEFUN bool core__inherits_from_instance(core::T_sp obj)
{
  if (core::Instance_sp iobj = obj.asOrNull<core::Instance_O>() ) {
    return true;
  }
  return false;
}

CL_LAMBDA();
CL_DECLARE();
CL_DOCSTRING("Return the header kind for the object");
CL_DEFUN core::T_mv core__hardwired_kinds() {
  std::vector<Immediate_info> immediates = get_immediate_info();
  core::List_sp result = _Nil<core::T_O>();
  for ( int i=0; i<immediates.size(); ++i ) {
    result = core::Cons_O::create(core::Cons_O::create(core::Str_O::create(immediates[i]._name), core::clasp_make_fixnum(immediates[i]._kind)),result);
  }
  core::List_sp ignoreClasses = _Nil<core::T_O>(); // core::Cons_O::createList(Str_O::create("core__Cons_O") <-- future when CONS are in their own pool
  return Values(result, ignoreClasses, core::clasp_make_fixnum(kind_first_general), core::clasp_make_fixnum(kind_first_alien), core::clasp_make_fixnum(kind_last_alien), core::clasp_make_fixnum(kind_first_instance));
}

#if 0
#define ARGS_af_testVec0 "()"
#define DECL_af_testVec0 ""
#define DOCS_af_testVec0 "testVec0"
    void af_testVec0()
    {
        int N = 4;
        printf("Creating vec(4)\n");
        gctools::Vec0<TestingClass> v;
        v.resize(4);
        vector_dump(v);
        printf("About to Push_back TestingClass(2)\n");
        v.push_back(TestingClass(2));
        vector_dump(v);
        printf("About to Push_back %d times TestingClass(i)\n", N);
        for ( int i(0); i<N; ++i ) {v.push_back(TestingClass(i));}
        vector_dump(v); 
        printf("About to Push_back %d times TestingClass(i+10)\n", N);
        for ( int i(0); i<N; ++i )
        {
            v.push_back(TestingClass(i+10));
            vector_dump(v,"step");
        }
        vector_dump(v,"done"); 
        printf("Start again");
        gctools::Vec0<TestingClass> u;
        u.resize(4);
        vector_dump(u,"new");
        u.resize(8,TestingClass(-8));
        vector_dump(u,"resized to 8 ");
        u.resize(16,TestingClass(-16));
        vector_dump(u,"resized to 16");
        u.resize(4,TestingClass(-99999));
        vector_dump(u,"resized to 4 ");
        printf("Test emplace and erase\n");
        gctools::Vec0<TestingClass> w;
        for ( int zi(0); zi<20; ++zi ) {
            w.emplace(w.begin(),zi);
        }
        vector_dump(w,"after 10 emplace");
        w.erase(w.begin()+4);
        vector_dump(w,"removed begin()+4");
        for ( int zz(0); zz<5; ++zz ) w.erase(w.begin()+4);
        vector_dump(w,"removed begin()+4 4 times");
        w.erase(w.begin()+13);
        vector_dump(w,"removed begin()+13 times");
    }
#endif

#if 0
#define ARGS_af_testArray0 "()"
#define DECL_af_testArray0 ""
#define DOCS_af_testArray0 "testArray0"
    void af_testArray0()
    {
//        int N = 4;
        printf("Creating Array0(4)\n");
        gctools::Array0<core::core::T_sp> v;
        v.allocate(4,_Nil<core::T_O>());
        Array0_dump(v,"Nil*4");
        gctools::Array0<core::core::T_sp> w;
        w.allocate(4,_Nil<core::T_O>());
        for (int i(0); i<4; ++i ) {
            w[i] = core::make_fixnum(i);
        }
        Array0_dump(w,"Fixnum*4");
        printf("Creating ValueFrame\n");
        core::ValueFrame_sp vf = ValueFrame_O::create_fill_args(_Nil<core::ActivationFrame_O>(),core::make_fixnum(1), core::make_fixnum(2), core::make_fixnum(3));
        printf("Creating another 1 ValueFrame\n");
        vf = ValueFrame_O::create_fill_args(_Nil<core::ActivationFrame_O>(),core::make_fixnum(1), core::make_fixnum(2), core::make_fixnum(3));
        printf("Creating another 2 ValueFrame\n");
        vf = ValueFrame_O::create_fill_args(_Nil<core::ActivationFrame_O>(),core::make_fixnum(1), core::make_fixnum(2), core::make_fixnum(3));
        printf("Leaving scope\n");
    }
#endif
};

#ifdef USE_BOEHM

struct ReachableClass {
  ReachableClass() : _Kind(gctools::KIND_null){};
  ReachableClass(gctools::GCKindEnum tn) : _Kind(tn), instances(0), totalSize(0) {}
  void update(size_t sz) {
    ++this->instances;
    this->totalSize += sz;
  };
  gctools::GCKindEnum _Kind;
  size_t instances;
  size_t totalSize;
  size_t print(const std::string &shortName) {
    core::Fixnum k = this->_Kind;
    stringstream className;
    if (k <= gctools::KIND_max) {
      className << obj_name(this->_Kind);
    } else {
      className << "Unknown";
    }
    printf("%s: total_size: %10lu count: %8lu avg.sz: %8lu kind: %s/%zu\n", shortName.c_str(),
           this->totalSize, this->instances, this->totalSize / this->instances, className.str().c_str(), k);
    return this->totalSize;
  }
};

#if 0
struct ReachableContainer {
  ReachableContainer() : typeidName(NULL){};
  ReachableContainer(const char *tn) : typeidName(tn), instances(0), totalSize(0), largest(0) {}
  void update(size_t sz) {
    ++this->instances;
    this->totalSize += sz;
    if (sz > this->largest) {
      this->largest = sz;
    }
  };
  const char *typeidName;
  size_t instances;
  size_t totalSize;
  size_t largest;
  size_t print(const std::string &shortName) {
    printf("%s: total_size: %10lu count: %8lu  avg.sz: %8lu  largest: %8lu %s\n", shortName.c_str(),
           this->totalSize, this->instances, this->totalSize / this->instances, this->largest, this->typeidName);
    return this->totalSize;
  }
};
#endif

typedef map<gctools::GCKindEnum, ReachableClass> ReachableClassMap;
static ReachableClassMap *static_ReachableClassKinds;
static size_t invalidHeaderTotalSize = 0;
int globalSearchMarker = 0;
extern "C" {
void boehm_callback_reachable_object(void *ptr, size_t sz, void *client_data) {
  gctools::Header_s *h = reinterpret_cast<gctools::Header_s *>(ptr);
  if (h->isValid()) {
    if (h->markerMatches(globalSearchMarker)) {
      ReachableClassMap::iterator it = static_ReachableClassKinds->find(h->kind());
      if (it == static_ReachableClassKinds->end()) {
        ReachableClass reachableClass(h->kind());
        reachableClass.update(sz);
        (*static_ReachableClassKinds)[h->kind()] = reachableClass;
      } else {
        it->second.update(sz);
      }
    }
  } else {
    invalidHeaderTotalSize += sz;
  }
}
};

template <typename T>
size_t dumpResults(const std::string &name, const std::string &shortName, T *data) {
  printf("-------------------- %s -------------------\n", name.c_str());
  typedef typename T::value_type::second_type value_type;
  vector<value_type> values;
  for (auto it : *data) {
    values.push_back(it.second);
  }
  size_t totalSize(0);
  sort(values.begin(), values.end(), [](const value_type &x, const value_type &y) {
            return (x.totalSize > y.totalSize);
  });
  for (auto it : values) {
    totalSize += it.print(shortName);
  }
  return totalSize;
}

#endif

#ifdef USE_MPS
struct ReachableMPSObject {
  ReachableMPSObject(int k) : kind(k){};
  int kind = 0;
  size_t instances = 0;
  size_t totalMemory = 0;
  size_t largest = 0;
  size_t print(const std::string &shortName) {
    if (this->instances > 0) {
      printf("%s: totalMemory: %10lu count: %8lu largest: %8lu avg.sz: %8lu %s/%d\n", shortName.c_str(),
             this->totalMemory, this->instances, this->largest, this->totalMemory / this->instances, obj_name((gctools::GCKindEnum) this->kind), this->kind);
    }
    return this->totalMemory;
  }
};

extern "C" {
void amc_apply_stepper(mps_addr_t client, void *p, size_t s) {
  gctools::Header_s *header = reinterpret_cast<gctools::Header_s *>(gctools::ClientPtrToBasePtr(client));
  vector<ReachableMPSObject> *reachablesP = reinterpret_cast<vector<ReachableMPSObject> *>(p);
  if (header->kindP()) {
    ReachableMPSObject &obj = (*reachablesP)[header->kind()];
    ++obj.instances;
    size_t sz = (char *)(obj_skip(client)) - (char *)client;
    obj.totalMemory += sz;
    if (sz > obj.largest)
      obj.largest = sz;
  }
}
};

size_t dumpMPSResults(const std::string &name, const std::string &shortName, vector<ReachableMPSObject> &values) {
  printf("-------------------- %s -------------------\n", name.c_str());
  size_t totalSize(0);
  typedef ReachableMPSObject value_type;
  sort(values.begin(), values.end(), [](const value_type &x, const value_type &y) {
            return (x.totalMemory > y.totalMemory);
  });
  for (auto it : values) {
    totalSize += it.print(shortName);
  }
  return totalSize;
}

#endif // USE_MPS

namespace gctools {

CL_LAMBDA(&optional x (marker 0));
CL_DEFUN core::T_mv gctools__gc_info(core::T_sp x, core::Fixnum_sp marker) {
#ifdef USE_MPS
  return Values(_Nil<core::T_O>());
#endif
#ifdef USE_BOEHM
  return Values(_Nil<core::T_O>());
#endif
};

CL_LAMBDA(on &key (backtrace-start 0) (backtrace-count 0) (backtrace-depth 6));
CL_DEFUN void gctools__monitor_allocations(bool on, core::Fixnum_sp backtraceStart, core::Fixnum_sp backtraceCount, core::Fixnum_sp backtraceDepth) {
  global_monitorAllocations.on = on;
  global_monitorAllocations.counter = 0;
  if (backtraceStart.unsafe_fixnum() < 0 ||
      backtraceCount.unsafe_fixnum() < 0 ||
      backtraceDepth.unsafe_fixnum() < 0) {
    SIMPLE_ERROR(BF("Keyword arguments must all be >= 0"));
  }
  global_monitorAllocations.start = backtraceStart.unsafe_fixnum();
  global_monitorAllocations.end = backtraceStart.unsafe_fixnum() + backtraceCount.unsafe_fixnum();
  global_monitorAllocations.backtraceDepth = backtraceDepth.unsafe_fixnum();
  printf("%s:%d  monitorAllocations set to %d\n", __FILE__, __LINE__, on);
};

CL_LAMBDA(&optional marker);
CL_DEFUN Fixnum gctools__gc_marker(core::Fixnum_sp marker) {
#ifdef USE_BOEHM
#ifdef USE_BOEHM_MEMORY_MARKER
  if (marker.nilp()) {
    return gctools::globalBoehmMarker;
  }
  ASSERT(marker.fixnump());
  Fixnum oldm = gctools::globalBoehmMarker;
  Fixnum m = marker.unsafe_fixnum();
  gctools::globalBoehmMarker = m;
  return oldm;
#endif
#else
  printf("%s:%d Only boehm supports memory markers\n", __FILE__, __LINE__);
#endif
  return 0;
}

SYMBOL_EXPORT_SC_(GcToolsPkg, STARallocPatternStackSTAR);
SYMBOL_EXPORT_SC_(GcToolsPkg, ramp);
SYMBOL_EXPORT_SC_(GcToolsPkg, rampCollectAll);

CL_DEFUN void gctools__alloc_pattern_begin(core::Symbol_sp pattern) {
#ifdef USE_MPS
  if (pattern == _sym_ramp || pattern == _sym_rampCollectAll) {
    core::List_sp patternStack = gctools::_sym_STARallocPatternStackSTAR->symbolValue();
    patternStack = core::Cons_O::create(pattern, patternStack);
    gctools::_sym_STARallocPatternStackSTAR->setf_symbolValue(patternStack);
    if (pattern == _sym_ramp) {
      mps_ap_alloc_pattern_begin(_global_automatic_mostly_copying_allocation_point, mps_alloc_pattern_ramp());
    } else {
      mps_ap_alloc_pattern_begin(_global_automatic_mostly_copying_allocation_point, mps_alloc_pattern_ramp_collect_all());
    }
    return;
  }
  SIMPLE_ERROR(BF("Only options for alloc-pattern-begin is %s@%p and %s@%p - you passed: %s@%p") % _rep_(_sym_ramp) % _sym_rampCollectAll.raw_() % _rep_(_sym_rampCollectAll) % _sym_rampCollectAll.raw_() % _rep_(pattern) % pattern.raw_());
#endif
};

CL_DEFUN core::Symbol_sp gctools__alloc_pattern_end() {
  core::Symbol_sp pattern(_Nil<core::Symbol_O>());
#ifdef USE_MPS
  core::List_sp patternStack = gctools::_sym_STARallocPatternStackSTAR->symbolValue();
  if (patternStack.nilp())
    return _Nil<core::Symbol_O>();
  pattern = gc::As<core::Symbol_sp>(oCar(patternStack));
  gctools::_sym_STARallocPatternStackSTAR->setf_symbolValue(oCdr(patternStack));
  if (pattern == _sym_ramp) {
    mps_ap_alloc_pattern_end(_global_automatic_mostly_copying_allocation_point, mps_alloc_pattern_ramp());
  } else {
    mps_ap_alloc_pattern_end(_global_automatic_mostly_copying_allocation_point, mps_alloc_pattern_ramp_collect_all());
  }
#endif
  return pattern;
};

CL_LAMBDA(&optional x (marker 0) msg);
CL_DECLARE();
CL_DOCSTRING("room - Return info about the reachable objects.  x can be T, nil, :default - as in ROOM.  marker can be a fixnum (0 - matches everything, any other number/only objects with that marker)");
CL_DEFUN core::T_mv cl__room(core::T_sp x, core::Fixnum_sp marker, core::T_sp tmsg) {
  string smsg = "Total";
  if (core::Str_sp msg = tmsg.asOrNull<core::Str_O>()) {
    smsg = msg->get();
  }
#ifdef USE_MPS
  mps_word_t numCollections = mps_collections(gctools::_global_arena);
  size_t arena_committed = mps_arena_committed(gctools::_global_arena);
  size_t arena_reserved = mps_arena_reserved(gctools::_global_arena);
  printf("%12lu collections\n", numCollections);
  printf("%12lu mps_arena_committed\n", arena_committed);
  printf("%12lu mps_arena_reserved\n", arena_reserved);
  printf("%12lu finalization requests\n", globalMpsMetrics.finalizationRequests);
  size_t totalAllocations = globalMpsMetrics.nonMovingAllocations + globalMpsMetrics.movingAllocations + globalMpsMetrics.movingZeroRankAllocations + globalMpsMetrics.unknownAllocations;
  printf("%12lu total allocations\n", totalAllocations);
  printf("%12lu    non-moving(AWL) allocations\n", globalMpsMetrics.nonMovingAllocations);
  printf("%12lu    moving(AMC) allocations\n", globalMpsMetrics.movingAllocations);
  printf("%12lu    moving zero-rank(AMCZ) allocations\n", globalMpsMetrics.movingZeroRankAllocations);
  printf("%12lu    unknown(configurable) allocations\n", globalMpsMetrics.unknownAllocations);
  printf("%12lu total memory allocated\n", globalMpsMetrics.totalMemoryAllocated);
  vector<ReachableMPSObject> reachables;
  for (int i = 0; i < gctools::KIND_max; ++i) {
    reachables.push_back(ReachableMPSObject(i));
  }
  mps_amc_apply(_global_amc_pool, amc_apply_stepper, &reachables, 0);
  dumpMPSResults("Reachable Kinds", "AMCpool", reachables);
#endif
#ifdef USE_BOEHM
  globalSearchMarker = core::unbox_fixnum(marker);
  static_ReachableClassKinds = new (ReachableClassMap);
  invalidHeaderTotalSize = 0;
  GC_enumerate_reachable_objects_inner(boehm_callback_reachable_object, NULL);
  printf("Walked LispKinds\n");
  size_t totalSize(0);
  totalSize += dumpResults("Reachable ClassKinds", "class", static_ReachableClassKinds);
  printf("Done walk of memory  %lu ClassKinds\n", static_ReachableClassKinds->size());
  printf("%s invalidHeaderTotalSize = %12lu\n", smsg.c_str(), invalidHeaderTotalSize);
  printf("%s memory usage (bytes):    %12lu\n", smsg.c_str(), totalSize);
  printf("%s GC_get_heap_size()       %12lu\n", smsg.c_str(), GC_get_heap_size());
  printf("%s GC_get_free_bytes()      %12lu\n", smsg.c_str(), GC_get_free_bytes());
  printf("%s GC_get_bytes_since_gc()  %12lu\n", smsg.c_str(), GC_get_bytes_since_gc());
  printf("%s GC_get_total_bytes()     %12lu\n", smsg.c_str(), GC_get_total_bytes());

  delete static_ReachableClassKinds;
#endif
  gc::GCStack *stack = threadLocalStack();
  size_t totalMaxSize = stack->maxSize();
#if defined(USE_BOEHM) && defined(BOEHM_ONE_BIG_STACK)
  printf("Lisp-stack bottom %p cur %p limit %p\n", stack->_StackBottom, stack->_StackCur, stack->_StackLimit);
#endif
  printf("High water mark (max used) side-stack size: %u\n", totalMaxSize);
  return Values(_Nil<core::T_O>());
};
};

extern "C" {
void dbg_room() {
  cl__room(_Nil<core::T_O>(), core::make_fixnum(0), _Nil<core::T_O>());
}
}

namespace gctools {

#ifdef USE_MPS
CL_DEFUN void gctools__mpsTelemetryFlush() {
  mps_telemetry_flush();
};

CL_DEFUN void gctools__mpsTelemetrySet(core::Fixnum_sp flags) {
  mps_telemetry_set(unbox_fixnum(flags));
};

CL_DEFUN void gctools__mpsTelemetryReset(core::Fixnum_sp flags) {
  mps_telemetry_reset(unbox_fixnum(flags));
};

#endif

CL_DEFUN core::T_sp gctools__stack_depth() {
  int z = 0;
  void *zp = &z;
  size_t stackDepth = (char *)_global_stack_marker - (char *)zp;
  return core::make_fixnum((uint)stackDepth);
};

CL_DEFUN void gctools__garbage_collect() {
#ifdef USE_BOEHM
  GC_gcollect();
#endif
//        printf("%s:%d Starting garbage collection of arena\n", __FILE__, __LINE__ );
#ifdef USE_MPS
  mps_arena_collect(_global_arena);
  processMpsMessages();
  mps_arena_release(_global_arena);
#endif
  //        printf("Garbage collection done\n");
};

CL_DEFUN void gctools__cleanup() {
#ifdef USE_MPS
  processMpsMessages();
#endif
};




bool debugging_configuration(stringstream& ss) {
  bool debugging = false;
  bool use_boehm_memory_marker = false;
#ifdef USE_BOEHM_MEMORY_MARKER
  use_boehm_memory_marker = true;
  debugging = true;
#endif
  ss << (BF("USE_BOEHM_MEMORY_MARKER = %s\n") % (use_boehm_memory_marker ? "defined" : "undefined") ).str();
  bool use_alloca_for_frame = false;
#ifdef USE_ALLOCA_FOR_FRAME
  use_alloca_for_frame = true;
  debugging = true;
#endif
  ss << (BF("USE_ALLOCA_FOR_FRAME = %s\n") % (use_alloca_for_frame ? "defined" : "undefined") ).str();

  bool mps_recognize_all_tags = false;
#ifdef MPS_RECOGNIZE_ALL_TAGS
  mps_recognize_all_tags = true;
  debugging = true;
#endif
  ss << (BF("MPS_RECOGNIZE_ALL_TAGS = %s\n") % (mps_recognize_all_tags ? "defined" : "undefined") ).str();

  bool mps_recognize_zero_tags = false;
#ifdef MPS_RECOGNIZE_ZERO_TAGS
  mps_recognize_zero_tags = true;
  debugging = true;
#endif
  ss << (BF("MPS_RECOGNIZE_ZERO_TAGS = %s\n") % (mps_recognize_zero_tags ? "defined" : "undefined") ).str();

  bool use_symbols_in_global_array = false;
#ifdef USE_SYMBOLS_IN_GLOBAL_ARRAY
  use_symbols_in_global_array = true;
#endif
  ss << (BF("USE_SYMBOLS_IN_GLOBAL_ARRAY = %s\n") % (use_symbols_in_global_array ? "defined" : "undefined") ).str();

  bool use_static_analyzer_global_symbols = false;
#ifdef USE_STATIC_ANALYZER_GLOBAL_SYMBOLS
  use_static_analyzer_global_symbols = true;
#endif
  ss << (BF("USE_STATIC_ANALYZER_GLOBAL_SYMBOLS = %s\n") % (use_static_analyzer_global_symbols ? "defined" : "undefined") ).str();

  bool debug_throw_if_invalid_client_on = false;
#ifdef DEBUG_THROW_IF_INVALID_CLIENT_ON
  debug_throw_if_invalid_client_on = true;
  debugging = true;
#endif
  ss << (BF("DEBUG_THROW_IF_INVALID_CLIENT_ON = %s\n") % (debug_throw_if_invalid_client_on ? "defined" : "undefined") ).str();

    bool debug_telemetry = false;
#ifdef DEBUG_TELEMETRY
  debug_telemetry = true;
  debugging = true;
#endif
  ss << (BF("DEBUG_TELEMETRY = %s\n") % (debug_telemetry ? "defined" : "undefined") ).str();

  bool debug_stack_telemetry = false;
#ifdef DEBUG_STACK_TELEMETRY
  debug_stack_telemetry = true;
  debugging = true;
#endif
  ss << (BF("DEBUG_STACK_TELEMETRY = %s\n") % (debug_stack_telemetry ? "defined" : "undefined") ).str();

  bool debug_mps_underscanning = false;
#ifdef DEBUG_MPS_UNDERSCANNING
  debug_mps_underscanning = true;
  bool debug_mps_underscanning_initial = DEBUG_MPS_UNDERSCANNING_INITIAL;
  debugging = true;
#else
  bool debug_mps_underscanning_initial = false;
#endif
  ss << (BF("DEBUG_MPS_UNDERSCANNING = %s\n") % (debug_mps_underscanning ? "defined" : "undefined") ).str();
  ss << (BF("DEBUG_MPS_UNDERSCANNING_INITIAL = %s\n") % (debug_mps_underscanning_initial ? "true" : "false") ).str();

  bool debug_recursive_allocations = false;
#ifdef DEBUG_RECURSIVE_ALLOCATIONS
  debug_recursive_allocations = true;
  debugging = true;
#endif
  ss << (BF("DEBUG_RECURSIVE_ALLOCATIONS = %s\n") % (debug_recursive_allocations ? "defined" : "undefined") ).str();

  bool config_var_cool = false;
#ifdef CONFIG_VAR_COOL
  config_var_cool = true;
  debugging = true;
#endif
  ss << (BF("CONFIG_VAR_COOL = %s\n") % (config_var_cool ? "defined" : "undefined") ).str();

  bool debug_hash_table_as_alist = false;
#ifdef DEBUG_HASH_TABLE_AS_ALIST
  debug_hash_table_as_alist = true;
  debugging = true;
#endif
  ss << (BF("DEBUG_HASH_TABLE_AS_ALIST = %s\n") % (debug_hash_table_as_alist ? "defined" : "undefined") ).str();

    bool debug_hash_table_rehash_every_gc = false;
#ifdef DEBUG_HASH_TABLE_REHASH_EVERY_GC
  debug_hash_table_rehash_every_gc = true;
  debugging = true;
#endif
  ss << (BF("DEBUG_HASH_TABLE_REHASH_EVERY_GC = %s\n") % (debug_hash_table_rehash_every_gc ? "defined" : "undefined") ).str();

  bool debug_object_unique_id = false;
#ifdef DEBUG_OBJECT_UNIQUE_ID
  debug_object_unique_id = true;
  debugging = true;
#endif
  ss << (BF("DEBUG_OBJECT_UNIQUE_ID = %s\n") % (debug_object_unique_id ? "defined" : "undefined") ).str();
  
  return debugging;
}

CL_DEFUN void gctools__configuration()
{
  stringstream ss;
  bool debugging = debugging_configuration(ss);
  core::clasp_writeln_string(ss.str());
}


#define ARGS_gctools__debug_allocations "(arg)"
#define DECL_gctools__debug_allocations ""
#define DOCS_gctools__debug_allocations "debugAllocations"
CL_DEFUN void gctools__debug_allocations(core::T_sp debugOn) {
  _GlobalDebugAllocations = debugOn.isTrue();
};

#ifdef USE_GC_REF_COUNT_WRAPPER
#define ARGS_af_gcheader "(addr &optional (msg \"\") )"
#define DECL_af_gcheader ""
#define DOCS_af_gcheader "gcheader"
void af_gcheader(core::Pointer_sp addr, const string &msg) {
  GCWrapper<core::T_O>::GCHeader *gch = reinterpret_cast<GCWrapper<core::T_O>::GCHeader *>(addr->ptr());
  //        printf("%s Kind = %lu   RefCount=%d\n", msg.c_str(), gch->_Kind, gch->_ReferenceCount);
};

#define ARGS_af_gcaddress "(obj)"
#define DECL_af_gcaddress ""
#define DOCS_af_gcaddress "gcaddress"
core::Pointer_sp af_gcaddress(core::T_sp obj) {
  core::T_O *ptr = dynamic_cast<core::T_O *>(obj.get());
  void *hptr = reinterpret_cast<void *>(GCWrapper<core::T_O>::gcHeader(ptr));
  core::Pointer_sp po = core::Pointer_O::create(hptr);
  return po;
};

#define ARGS_af_testReferenceCounting "()"
#define DECL_af_testReferenceCounting ""
#define DOCS_af_testReferenceCounting "testReferenceCounting"
void af_testReferenceCounting() {
  core::Pointer_sp p;
  {
    core::Fixnum_sp fn = core::make_fixnum(20);
    p = af_gcaddress(fn);
    af_gcheader(p, "Start");
    core::Fixnum_sp fn2 = fn;
    af_gcheader(p, "Assignment");
    fn2 = fn2;
    af_gcheader(p, "fn2 = fn2");
    fn2 = core::make_fixnum(1);
    af_gcheader(p, "fn2 = make_fixnum(1)");
    fn = core::make_fixnum(0);
    //            af_gcheader(p,"fn = make_fixnum(0)");
  }
  //        af_gcheader(p,"fn went out of scope");
};
#endif

void initialize_gc_functions() {

  //            core::af_def(GcToolsPkg,"testVec0",&af_testVec0);
  //            core::af_def(GcToolsPkg,"testArray0",&af_testArray0);
//  core::af_def(GcToolsPkg, "gcInfo", &gctools__gc_info);
//  core::af_def(GcToolsPkg, "gcMarker", &gctools__gc_marker, ARGS_gctools__gc_marker, DECL_gctools__gc_marker, DOCS_gctools__gc_marker);
//  core::af_def(GcToolsPkg, "monitorAllocations", &gctools__monitor_allocations, ARGS_gctools__monitor_allocations, DECL_gctools__monitor_allocations, DOCS_gctools__monitor_allocations);
//  core::af_def(GcToolsPkg, "garbageCollect", &gctools__garbage_collect);
//  core::af_def(GcToolsPkg, "stackDepth", &gctools__stack_depth);
//  core::af_def(GcToolsPkg, "cleanup", &gctools__cleanup);
//  core::af_def(GcToolsPkg, "maxBootstrapKinds", &gctools__max_bootstrap_kinds);
//  core::af_def(GcToolsPkg, "bootstrapKindP", &gctools__bootstrap_kind_p);
//  core::af_def(GcToolsPkg, "bootstrapKindSymbols", &gctools__bootstrap_kind_symbols);
//  core::af_def(GcToolsPkg, "allocPatternBegin", &gctools__alloc_pattern_begin);
//  core::af_def(GcToolsPkg, "allocPatternEnd", &gctools__alloc_pattern_end);
//  core::af_def(GcToolsPkg, "bytes_allocated", &gctools__bytes_allocated);
//  core::af_def(GcToolsPkg, "deallocate_unmanaged_instance", &gctools__deallocate_unmanaged_instance );

  _sym_STARallocPatternStackSTAR->defparameter(_Nil<core::T_O>());
#ifdef USE_MPS
//  core::af_def(GcToolsPkg, "mpsTelemetrySet", &gctools__mpsTelemetrySet);
//  core::af_def(GcToolsPkg, "mpsTelemetryReset", &gctools__mpsTelemetryReset);
//  core::af_def(GcToolsPkg, "mpsTelemetryFlush", &gctools__mpsTelemetryFlush);
#endif

  //	    SYMBOL_EXPORT_SC_(GcTools,linkExternalGlobalsInModule);
  //	    Defun(linkExternalGlobalsInModule);
//  Gctools_temp_Defun(debug_allocations);
};
};
