// ============================================================
// Arrays specialized for byte8_t
//
namespace core {
FORWARD(SimpleVector_byte8_t);
FORWARD(MDArray_byte8_t);
FORWARD(SimpleMDArray_byte8_t);
FORWARD(ComplexVector_byte8_t);
};
template <>
struct gctools::GCInfo<core::SimpleVector_byte8_t_O> {
  static bool constexpr NeedsInitialization = false;
  static bool constexpr NeedsFinalization = false;
  static GCInfo_policy constexpr Policy = atomic;
};

namespace core {
  class SimpleVector_byte8_t_O;
  typedef template_SimpleVector<SimpleVector_byte8_t_O,byte8_t,AbstractSimpleVector_O> specialized_SimpleVector_byte8_t;
  class SimpleVector_byte8_t_O : public specialized_SimpleVector_byte8_t {
    LISP_CLASS(core, CorePkg, SimpleVector_byte8_t_O, "SimpleVector_byte8_t",AbstractSimpleVector_O);
    virtual ~SimpleVector_byte8_t_O() {};
  public:
    typedef specialized_SimpleVector_byte8_t TemplatedBase;
    typedef typename TemplatedBase::leaf_type leaf_type;
    typedef typename TemplatedBase::value_type value_type;
    typedef typename TemplatedBase::simple_element_type simple_element_type;
    typedef typename TemplatedBase::vector_type vector_type;
    typedef typename TemplatedBase::iterator iterator;
    typedef typename TemplatedBase::const_iterator const_iterator;
    typedef value_type container_value_type;
  public:
    static value_type default_initial_element(void) {return 0;}
    static value_type initial_element_from_object(T_sp obj, bool supplied) {
      if (supplied) return from_object(obj);
      return 0;
    }
    static value_type from_object(T_sp obj) { return clasp_to_uint8_t(obj); };
    static T_sp to_object(const value_type& v) { return Integer_O::create(v); };
  public:
  SimpleVector_byte8_t_O(size_t length, value_type initialElement=value_type(),
                          bool initialElementSupplied=false,
                          size_t initialContentsSize=0,
                          const value_type* initialContents=NULL)
    : TemplatedBase(length,initialElement,initialElementSupplied,initialContentsSize,initialContents) {};
    static smart_ptr_type make(size_t length,
                                         value_type initialElement=value_type(),
                                         bool initialElementSupplied=false,
                                         size_t initialContentsSize=0,
                               const value_type* initialContents=NULL,
                               bool static_vector_p = false) {
      auto bs = gctools::GC<my_type>::allocate_container(static_vector_p,length,initialElement,initialElementSupplied,initialContentsSize,initialContents);
      return bs;
    }
  public:
    virtual T_sp array_type() const final { return cl::_sym_simple_array; };
    virtual T_sp element_type() const override { return ext::_sym_byte8;};
    virtual T_sp arrayElementType() const override { return ext::_sym_byte8; };
    virtual clasp_elttype elttype() const { return clasp_aet_byte8_t; };
    virtual void __write__(T_sp stream) const;
  };
};


namespace core {
  class MDArray_byte8_t_O : public template_Array<MDArray_byte8_t_O,SimpleMDArray_byte8_t_O,SimpleVector_byte8_t_O,MDArray_O> {
    LISP_CLASS(core, CorePkg, MDArray_byte8_t_O, "MDArray_byte8_t",MDArray_O);
    virtual ~MDArray_byte8_t_O() {};
  public:
    typedef template_Array<MDArray_byte8_t_O,SimpleMDArray_byte8_t_O,SimpleVector_byte8_t_O,MDArray_O> TemplatedBase;
    typedef typename TemplatedBase::simple_element_type simple_element_type;
    typedef typename TemplatedBase::simple_type simple_type;
  public: // make array
  MDArray_byte8_t_O(size_t rank,
                     List_sp dimensions,
                     Array_sp data,
                     bool displacedToP,
                     Fixnum_sp displacedIndexOffset) : TemplatedBase(rank,dimensions,data,displacedToP,displacedIndexOffset) {};
    static smart_ptr_type make_multi_dimensional(List_sp dim_desig, simple_element_type initialElement, T_sp dataOrDisplacedTo, bool displacedToP, Fixnum_sp displacedIndexOffset) {
      ASSERT(dim_desig.consp()||dim_desig.nilp());
      size_t rank;
      size_t arrayTotalSize = calculateArrayTotalSizeAndValidateDimensions(dim_desig,rank);
      LIKELY_if (dataOrDisplacedTo.nilp()) {
        dataOrDisplacedTo = simple_type::make(arrayTotalSize,initialElement,true);
      }
      return gctools::GC<my_type>::allocate_container(false,rank,dim_desig,gc::As<Array_sp>(dataOrDisplacedTo),displacedToP,displacedIndexOffset);
    }
  public:
//    virtual bool equalp(T_sp o) const final;
    // vectorPushExtend of the specialized type
    void vectorPushExtend(value_type newElement, size_t extension = 0) {
      unlikely_if (!this->_Flags.fillPointerP()) noFillPointerSpecializedArrayError(this->asSmartPtr());
      cl_index idx = this->_FillPointerOrLengthOrDummy;
      unlikely_if (idx >= this->_ArrayTotalSize) {
        if (extension <= 0) extension = calculate_extension(this->_ArrayTotalSize);
        cl_index new_size = this->_ArrayTotalSize+extension;
        unlikely_if (!cl::_sym_adjust_array || !lisp_boundp(cl::_sym_adjust_array)) {
          this->internalAdjustSize_(new_size);
        } else {
          lisp_adjust_array(this->asSmartPtr(),clasp_make_fixnum(new_size),clasp_make_fixnum(this->_FillPointerOrLengthOrDummy));
        }
      }
      (*this)[idx] = newElement;
      ++this->_FillPointerOrLengthOrDummy;
    }
  };
};


namespace core {
  class SimpleMDArray_byte8_t_O : public template_SimpleArray<SimpleMDArray_byte8_t_O,SimpleVector_byte8_t_O,SimpleMDArray_O> {
    LISP_CLASS(core, CorePkg, SimpleMDArray_byte8_t_O, "SimpleMDArray_byte8_t",SimpleMDArray_O);
    virtual ~SimpleMDArray_byte8_t_O() {};
  public:
    typedef template_SimpleArray<SimpleMDArray_byte8_t_O,SimpleVector_byte8_t_O,SimpleMDArray_O> TemplatedBase;
    typedef typename TemplatedBase::simple_element_type simple_element_type;
    typedef typename TemplatedBase::simple_type simple_type;
  public: // make vector
  SimpleMDArray_byte8_t_O(size_t rank1, size_t dimension, Array_sp data) : TemplatedBase(dimension,data) {};
    static smart_ptr_type make(size_t dimension, simple_element_type initialElement/*=_Nil<T_O>()*/, T_sp data/*=_Nil<T_O>()*/) {
      printf("%s:%d This should never be called\n", __FILE__, __LINE__);
      LIKELY_if (data.nilp()) {
        data = simple_type::make(dimension,initialElement,true);
      }
      return gctools::GC<my_type>::allocate_container(false,1,dimension,gc::As_unsafe<Array_sp>(data));
    }
    static smart_ptr_type make(size_t dimension, simple_element_type initialElement) {
      return make(dimension,initialElement,_Nil<T_O>());
    }
  public: // make array
  SimpleMDArray_byte8_t_O(size_t rank,
                           List_sp dimensions,
                           Array_sp data) : TemplatedBase(rank,dimensions,data) {};
    static smart_ptr_type make_multi_dimensional(List_sp dim_desig, simple_element_type initialElement, T_sp data) {
      ASSERT(dim_desig.consp()||dim_desig.nilp());
      size_t rank;
      size_t arrayTotalSize = calculateArrayTotalSizeAndValidateDimensions(dim_desig,rank);
      LIKELY_if (data.nilp()) {
        data = SimpleVector_byte8_t_O::make(arrayTotalSize,initialElement,true);
      }
      return gctools::GC<my_type>::allocate_container(false,rank,dim_desig,gc::As<Array_sp>(data));
    }
  };
};

namespace core {
  class ComplexVector_byte8_t_O : public template_Array<ComplexVector_byte8_t_O,ComplexVector_byte8_t_O,SimpleVector_byte8_t_O,ComplexVector_O> {
    LISP_CLASS(core, CorePkg, ComplexVector_byte8_t_O, "ComplexVector_byte8_t",ComplexVector_O);
    virtual ~ComplexVector_byte8_t_O() {};
  public:
    typedef template_Array<ComplexVector_byte8_t_O,ComplexVector_byte8_t_O,SimpleVector_byte8_t_O,ComplexVector_O> TemplatedBase;
    typedef typename TemplatedBase::simple_element_type simple_element_type;
    typedef typename TemplatedBase::simple_type simple_type;
  public: // make vector
  ComplexVector_byte8_t_O(size_t dimension,
                          T_sp fillPointer,
                          Array_sp data,
                          bool displacedToP,
                          Fixnum_sp displacedIndexOffset) : TemplatedBase(Rank1(),dimension,fillPointer,data,displacedToP,displacedIndexOffset) {};
    static smart_ptr_type make_vector(size_t dimension, simple_element_type initialElement/*=simple_element_type()*/, T_sp fillPointer/*=_Nil<T_O>()*/, T_sp dataOrDisplacedTo/*=_Nil<T_O>()*/, bool displacedToP/*=false*/, Fixnum_sp displacedIndexOffset/*=clasp_make_fixnum(0)*/ ) {
      LIKELY_if (dataOrDisplacedTo.nilp()) {
        dataOrDisplacedTo = simple_type::make(dimension,initialElement,true);
      }
      return gctools::GC<my_type>::allocate_container(false,dimension,fillPointer,gc::As_unsafe<Array_sp>(dataOrDisplacedTo),displacedToP,displacedIndexOffset);
    }
    static smart_ptr_type make_vector(size_t dimension) {
      return make_vector(dimension,0,_Nil<T_O>(),_Nil<T_O>(),false,clasp_make_fixnum(0));
    }
  public:
//    virtual bool equalp(T_sp o) const final;
    // vectorPushExtend of the specialized type
    void vectorPushExtend(value_type newElement, size_t extension = 0) {
      unlikely_if (!this->_Flags.fillPointerP()) noFillPointerSpecializedArrayError(this->asSmartPtr());
      cl_index idx = this->_FillPointerOrLengthOrDummy;
      unlikely_if (idx >= this->_ArrayTotalSize) {
        if (extension <= 0) extension = calculate_extension(this->_ArrayTotalSize);
        cl_index new_size = this->_ArrayTotalSize+extension;
        unlikely_if (!cl::_sym_adjust_array || !lisp_boundp(cl::_sym_adjust_array)) {
          this->internalAdjustSize_(new_size);
        } else {
          lisp_adjust_array(this->asSmartPtr(),clasp_make_fixnum(new_size),clasp_make_fixnum(this->_FillPointerOrLengthOrDummy));
        }
      }
      (*this)[idx] = newElement;
      ++this->_FillPointerOrLengthOrDummy;
    }
  };
};



// ----------------------------------------------------------------------
// Arrays specialized for int8_t
//
namespace core {
  FORWARD(SimpleVector_int8_t);
  FORWARD(MDArray_int8_t);
  FORWARD(SimpleMDArray_int8_t);
FORWARD(ComplexVector_int8_t);
};
template <>
struct gctools::GCInfo<core::SimpleVector_int8_t_O> {
  static bool constexpr NeedsInitialization = false;
  static bool constexpr NeedsFinalization = false;
  static GCInfo_policy constexpr Policy = atomic;
};

namespace core {
  class SimpleVector_int8_t_O;
  typedef template_SimpleVector<SimpleVector_int8_t_O,int8_t,AbstractSimpleVector_O> specialized_SimpleVector_int8_t;
  class SimpleVector_int8_t_O : public specialized_SimpleVector_int8_t {
    LISP_CLASS(core, CorePkg, SimpleVector_int8_t_O, "SimpleVector_int8_t",AbstractSimpleVector_O);
    virtual ~SimpleVector_int8_t_O() {};
  public:
    typedef specialized_SimpleVector_int8_t TemplatedBase;
    typedef typename TemplatedBase::leaf_type leaf_type;
    typedef typename TemplatedBase::value_type value_type;
    typedef typename TemplatedBase::simple_element_type simple_element_type;
    typedef typename TemplatedBase::vector_type vector_type;
    typedef typename TemplatedBase::iterator iterator;
    typedef typename TemplatedBase::const_iterator const_iterator;
    typedef value_type container_value_type;
  public:
    static value_type default_initial_element(void) {return 0;}
    static value_type initial_element_from_object(T_sp obj, bool supplied) {
      if (supplied) return from_object(obj);
      return 0;
    }
    static value_type from_object(T_sp obj) { return clasp_to_int8_t(obj); };
    static T_sp to_object(const value_type& v) { return Integer_O::create(v); };
  public:
  SimpleVector_int8_t_O(size_t length, value_type initialElement=value_type(),
                          bool initialElementSupplied=false,
                          size_t initialContentsSize=0,
                          const value_type* initialContents=NULL)
    : TemplatedBase(length,initialElement,initialElementSupplied,initialContentsSize,initialContents) {};
    static smart_ptr_type make(size_t length,
                                         value_type initialElement=value_type(),
                                         bool initialElementSupplied=false,
                                         size_t initialContentsSize=0,
                               const value_type* initialContents=NULL,
                               bool static_vector_p = false) {
      auto bs = gctools::GC<my_type>::allocate_container(static_vector_p,length,initialElement,initialElementSupplied,initialContentsSize,initialContents);
      return bs;
    }
  public:
    virtual T_sp array_type() const final { return cl::_sym_simple_array; };
    virtual T_sp element_type() const override { return ext::_sym_integer8;};
    virtual T_sp arrayElementType() const override { return ext::_sym_integer8; };
    virtual clasp_elttype elttype() const { return clasp_aet_int8_t; };
    virtual void __write__(T_sp stream) const;
  };
};


namespace core {
  class MDArray_int8_t_O : public template_Array<MDArray_int8_t_O,SimpleMDArray_int8_t_O,SimpleVector_int8_t_O,MDArray_O> {
    LISP_CLASS(core, CorePkg, MDArray_int8_t_O, "MDArray_int8_t",MDArray_O);
    virtual ~MDArray_int8_t_O() {};
  public:
    typedef template_Array<MDArray_int8_t_O,SimpleMDArray_int8_t_O,SimpleVector_int8_t_O,MDArray_O> TemplatedBase;
    typedef typename TemplatedBase::simple_element_type simple_element_type;
    typedef typename TemplatedBase::simple_type simple_type;
  public: // make array
  MDArray_int8_t_O(size_t rank,
                     List_sp dimensions,
                     Array_sp data,
                     bool displacedToP,
                     Fixnum_sp displacedIndexOffset) : TemplatedBase(rank,dimensions,data,displacedToP,displacedIndexOffset) {};
    static smart_ptr_type make_multi_dimensional(List_sp dim_desig, simple_element_type initialElement, T_sp dataOrDisplacedTo, bool displacedToP, Fixnum_sp displacedIndexOffset) {
      ASSERT(dim_desig.consp()||dim_desig.nilp());
      size_t rank;
      size_t arrayTotalSize = calculateArrayTotalSizeAndValidateDimensions(dim_desig,rank);
      LIKELY_if (dataOrDisplacedTo.nilp()) {
        dataOrDisplacedTo = simple_type::make(arrayTotalSize,initialElement,true);
      }
      return gctools::GC<my_type>::allocate_container(false,rank,dim_desig,gc::As<Array_sp>(dataOrDisplacedTo),displacedToP,displacedIndexOffset);
    }
  public:
//    virtual bool equalp(T_sp o) const final;
    // vectorPushExtend of the specialized type
    void vectorPushExtend(value_type newElement, size_t extension = 0) {
      unlikely_if (!this->_Flags.fillPointerP()) noFillPointerSpecializedArrayError(this->asSmartPtr());
      cl_index idx = this->_FillPointerOrLengthOrDummy;
      unlikely_if (idx >= this->_ArrayTotalSize) {
        if (extension <= 0) extension = calculate_extension(this->_ArrayTotalSize);
        cl_index new_size = this->_ArrayTotalSize+extension;
        unlikely_if (!cl::_sym_adjust_array || !lisp_boundp(cl::_sym_adjust_array)) {
          this->internalAdjustSize_(new_size);
        } else {
          lisp_adjust_array(this->asSmartPtr(),clasp_make_fixnum(new_size),clasp_make_fixnum(this->_FillPointerOrLengthOrDummy));
        }
      }
      (*this)[idx] = newElement;
      ++this->_FillPointerOrLengthOrDummy;
    }
  };
};

namespace core {
  class SimpleMDArray_int8_t_O : public template_SimpleArray<SimpleMDArray_int8_t_O,SimpleVector_int8_t_O,SimpleMDArray_O> {
    LISP_CLASS(core, CorePkg, SimpleMDArray_int8_t_O, "SimpleMDArray_int8_t",SimpleMDArray_O);
    virtual ~SimpleMDArray_int8_t_O() {};
  public:
    typedef template_SimpleArray<SimpleMDArray_int8_t_O,SimpleVector_int8_t_O,SimpleMDArray_O> TemplatedBase;
    typedef typename TemplatedBase::simple_element_type simple_element_type;
    typedef typename TemplatedBase::simple_type simple_type;
  public: // make vector
  SimpleMDArray_int8_t_O(size_t rank1, size_t dimension, Array_sp data) : TemplatedBase(dimension,data) {};
    static smart_ptr_type make(size_t dimension, simple_element_type initialElement/*=_Nil<T_O>()*/, T_sp data/*=_Nil<T_O>()*/) {
      printf("%s:%d This should never be called\n", __FILE__, __LINE__);
      LIKELY_if (data.nilp()) {
        data = simple_type::make(dimension,initialElement,true);
      }
      return gctools::GC<my_type>::allocate_container(false,1,dimension,gc::As_unsafe<Array_sp>(data));
    }
    static smart_ptr_type make(size_t dimension, simple_element_type initialElement) {
      return make(dimension,initialElement,_Nil<T_O>());
    }
  public: // make array
  SimpleMDArray_int8_t_O(size_t rank,
                         List_sp dimensions,
                         Array_sp data) : TemplatedBase(rank,dimensions,data) {};
    static smart_ptr_type make_multi_dimensional(List_sp dim_desig, simple_element_type initialElement, T_sp data) {
      ASSERT(dim_desig.consp()||dim_desig.nilp());
      size_t rank;
      size_t arrayTotalSize = calculateArrayTotalSizeAndValidateDimensions(dim_desig,rank);
      LIKELY_if (data.nilp()) {
        data = SimpleVector_int8_t_O::make(arrayTotalSize,initialElement,true);
      }
      return gctools::GC<my_type>::allocate_container(false,rank,dim_desig,gc::As<Array_sp>(data));
    }
  };
};


namespace core {
  class ComplexVector_int8_t_O : public template_Array<ComplexVector_int8_t_O,ComplexVector_int8_t_O,SimpleVector_int8_t_O,ComplexVector_O> {
    LISP_CLASS(core, CorePkg, ComplexVector_int8_t_O, "ComplexVector_int8_t",ComplexVector_O);
    virtual ~ComplexVector_int8_t_O() {};
  public:
    typedef template_Array<ComplexVector_int8_t_O,ComplexVector_int8_t_O,SimpleVector_int8_t_O,ComplexVector_O> TemplatedBase;
    typedef typename TemplatedBase::simple_element_type simple_element_type;
    typedef typename TemplatedBase::simple_type simple_type;
  public: // make vector
    ComplexVector_int8_t_O(size_t dimension,
                           T_sp fillPointer,
                           Array_sp data,
                           bool displacedToP,
                           Fixnum_sp displacedIndexOffset) : TemplatedBase(Rank1(),dimension,fillPointer,data,displacedToP,displacedIndexOffset) {};
    static smart_ptr_type make_vector(size_t dimension, simple_element_type initialElement/*=simple_element_type()*/, T_sp fillPointer/*=_Nil<T_O>()*/, T_sp dataOrDisplacedTo/*=_Nil<T_O>()*/, bool displacedToP/*=false*/, Fixnum_sp displacedIndexOffset/*=clasp_make_fixnum(0)*/ ) {
      LIKELY_if (dataOrDisplacedTo.nilp()) {
        dataOrDisplacedTo = simple_type::make(dimension,initialElement,true);
      }
      return gctools::GC<my_type>::allocate_container(false,dimension,fillPointer,gc::As_unsafe<Array_sp>(dataOrDisplacedTo),displacedToP,displacedIndexOffset);
    }
    static smart_ptr_type make_vector(size_t dimension) {
      return make_vector(dimension,0,_Nil<T_O>(),_Nil<T_O>(),false,clasp_make_fixnum(0));
    }
  public:
//    virtual bool equalp(T_sp o) const final;
    // vectorPushExtend of the specialized type
    void vectorPushExtend(value_type newElement, size_t extension = 0) {
      unlikely_if (!this->_Flags.fillPointerP()) noFillPointerSpecializedArrayError(this->asSmartPtr());
      cl_index idx = this->_FillPointerOrLengthOrDummy;
      unlikely_if (idx >= this->_ArrayTotalSize) {
        if (extension <= 0) extension = calculate_extension(this->_ArrayTotalSize);
        cl_index new_size = this->_ArrayTotalSize+extension;
        unlikely_if (!cl::_sym_adjust_array || !lisp_boundp(cl::_sym_adjust_array)) {
          this->internalAdjustSize_(new_size);
        } else {
          lisp_adjust_array(this->asSmartPtr(),clasp_make_fixnum(new_size),clasp_make_fixnum(this->_FillPointerOrLengthOrDummy));
        }
      }
      (*this)[idx] = newElement;
      ++this->_FillPointerOrLengthOrDummy;
    }
  };
};
