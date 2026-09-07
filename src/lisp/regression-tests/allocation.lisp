(in-package #:clasp-tests)

;;;; Byte-exact allocation tests for the control-flow constructs of the BYTECODE tier.
;;;;
;;;; Every measured function is built with CMP:BYTECOMPILE, so the tier under test is bytecode
;;;; whatever CMP:*COMPILE-FILE-NATIVE* says for this file.  GCTOOLS:BYTES-ALLOCATED is per-thread
;;;; and counts every Clasp allocation with its header, so one cons reads exactly 24 and the
;;;; control below proves the instrument is live in this process.
;;;;
;;;; Each test asserts 0 bytes per entry.  A construct that still allocates is listed in
;;;; set-unexpected-failures.lisp under the change that will remove it, with the figure it reads
;;;; today in the test's :description.  The figure is exact by construction: the driver divides
;;;; the total by the call count, so a partial result shows up as a ratio, never as a rounded 0.

;;; The sinks and the CLOS fixture are shared with allocation-native.lisp, which measures the same shapes.
(load "sys:src;lisp;regression-tests;allocation-helpers.lisp")

;;; The driver is bytecode too, so its own loop cannot be charged to the construct.
(defparameter *alloc-driver*
  (cmp:bytecompile
   '(lambda (f n)
     (funcall f 0)                       ; warm up once, outside the window
     (let ((before (gctools:bytes-allocated)))
       (dotimes (i n) (funcall f i))
       (- (gctools:bytes-allocated) before)))))

(defun bytes-per-call (lambda-expression &optional (n 10000))
  "Bytes allocated per call of the bytecompiled LAMBDA-EXPRESSION over N calls, exact.
The autocompile hook is bound off for the window: the shared helpers above cross
BYTECODE_COMPILE_THRESHOLD part-way through this file, and a live hook would run on this
thread inside the window and cons for every later call."
  (let ((cmp:*autocompile-hook* nil))
    (/ (funcall *alloc-driver* (cmp:bytecompile lambda-expression) n) n)))

;;; ---- controls: the instrument moves, and the free shapes are free -----------------------

(test alloc.control-cons
      (bytes-per-call '(lambda (i) (%alloc-touch (cons i i))))
      (24)
      :description "one cons per call: the counter is live and byte-exact")

(test alloc.catch-throw
      (bytes-per-call '(lambda (i) (catch 'alloc-tag (%alloc-touch i) (throw 'alloc-tag nil))))
      (0)
      :description "a catch dynenv is stack-allocated and a throw allocates nothing")

(test alloc.block-return
      (bytes-per-call '(lambda (i) (block b (%alloc-touch i) (return-from b nil))))
      (0)
      :description "a block that is not closed over is a saved stack pointer; a local return-from is a jump")

(test alloc.special-bind
      (bytes-per-call '(lambda (i) (let ((*alloc-sink* i)) (%alloc-touch *alloc-sink*))))
      (0)
      :description "a special binding is a stack-allocated dynenv")

(test alloc.mvb.capture-free
      (bytes-per-call '(lambda (i) (multiple-value-bind (a b) (%alloc-two i) (%alloc-touch (+ a b)))))
      (0)
      :description "the bind's lambda references only its own parameters, so it is a constant; its &rest list is NIL because %ALLOC-TWO returns exactly two values")

(test alloc.mvb.surplus-value
      (bytes-per-call '(lambda (i) (multiple-value-bind (a b) (%alloc-three i) (%alloc-touch (+ a b)))))
      (0)
      :description "24 today: the bind's lambda has an ignored &rest, and listify_rest_args conses the third value anyway")

;;; ---- unwind-protect: the protect opcode builds the cleanup closure ----------------------

(test alloc.unwind-protect.nocapture
      (bytes-per-call '(lambda (i) (unwind-protect (%alloc-touch i) (%alloc-touch 0))))
      (0)
      :description "32 on the unmodified VM: a cleanup closure with no captured variable")

(test alloc.unwind-protect.capture-1
      (bytes-per-call '(lambda (i) (let ((y i)) (unwind-protect (%alloc-touch i) (%alloc-touch y)))))
      (0)
      :description "40 = 32 + 8 x 1 captured variable")

(test alloc.unwind-protect.capture-3
      (bytes-per-call '(lambda (i)
                        (let ((a i) (b i) (c i))
                          (unwind-protect (%alloc-touch i)
                            (%alloc-touch a) (%alloc-touch b) (%alloc-touch c)))))
      (0)
      :description "56 = 32 + 8 x 3 captured variables")

(test alloc.unwind-protect.assigned-capture
      (bytes-per-call '(lambda (i) (let ((y i)) (unwind-protect (setq y (1+ y)) (%alloc-touch y)))))
      (0)
      :description "64 = 40 + a 24 B cell: Y is captured by the cleanup and assigned in the body")

(test alloc.with-lock-held
      (bytes-per-call '(lambda (i) (mp:with-lock (*alloc-lock*) (%alloc-touch i))))
      (0)
      :description "40: one unwind-protect whose cleanup captures the lock")

;;; ---- multiple values --------------------------------------------------------------------

(test alloc.values-trampoline
      (bytes-per-call '(lambda (i) (multiple-value-call #'values (%alloc-three i))))
      (0)
      :description "mv_call: a full call to VALUES over three values in the MV register, all three returned")

(test alloc.mvb.capturing
      (bytes-per-call '(lambda (i)
                        (let ((z i))
                          (multiple-value-bind (a b) (%alloc-two i) (%alloc-touch (+ a b z))))))
      (0)
      :description "40 = 32 + 8: the bind expands to a lambda that captures Z")

;;; ---- the condition system ---------------------------------------------------------------

(test alloc.handler-bind
      (bytes-per-call '(lambda (i)
                        (handler-bind ((error (lambda (c) (declare (ignore c)) (%alloc-touch 0))))
                          (%alloc-touch i))))
      (0)
      :description "72: three conses for the cluster; the handler lambda captures nothing")

(test alloc.handler-bind.global-handler
      (bytes-per-call '(lambda (i) (handler-bind ((error #'%alloc-global-handler)) (%alloc-touch i))))
      (0)
      :description "72 today: the same three conses as alloc.handler-bind; only the type-test lambda is a literal")

(test alloc.handler-case.novar
      (bytes-per-call '(lambda (i) (handler-case (%alloc-touch i) (error () (%alloc-touch 0)))))
      (0)
      :description "144 = 72 cluster + 32 entry dynenv + 40 handler closure")

(test alloc.handler-case.var
      (bytes-per-call '(lambda (i) (handler-case (%alloc-touch i) (error (c) (%alloc-touch c)))))
      (0)
      :description "176 = 144 + a 24 B cell and an 8 B slot for the clause variable")

;;; ---- a closed-over tagbody: the entry opcode's heap dynenv ------------------------------

(test alloc.closed-over-tagbody
      (bytes-per-call '(lambda (i) (tagbody (funcall (lambda () (go end))) (%alloc-touch i) end)))
      (0)
      :description "a lambda closing over a tagbody: the heap dynenv plus a one-slot closure")
