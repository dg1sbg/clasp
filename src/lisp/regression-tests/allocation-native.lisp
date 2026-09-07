(in-package #:clasp-tests)

;;;; Byte-exact allocation tests for the control-flow constructs of the NATIVE tier.
;;;;
;;;; Every measured function is built by CL:COMPILE under CMP:*COMPILE-NATIVE*, and NATIVE-COMPILE
;;;; refuses a result whose entry is not a SIMPLE-CORE-FUN, so a reading here is a native reading
;;;; whatever tier the image defaults to.  This is the (COMPILE NIL ...) pipeline; it agrees with
;;;; COMPILE-FILE's on every shape below except a surplus MULTIPLE-VALUE-BIND value, which is
;;;; asserted as this pipeline's fact and claims nothing about a compiled file.
;;;;
;;;; A row whose construct is a LAW (an escaping closure costs 32 + 8 per captured variable) asserts
;;;; the law, so a change that moves it is seen.  A row whose construct is a TARGET asserts 0 and is
;;;; listed in set-unexpected-failures.lisp under the change that removes its bytes, with today's
;;;; reading in its :description.  The figure is exact by construction: the driver divides the total
;;;; by the call count, so a partial result shows up as a ratio, never as a rounded 0.

(load "sys:src;lisp;regression-tests;allocation-helpers.lisp")

(defun native-compile (lambda-expression)
  "LAMBDA-EXPRESSION compiled by CL:COMPILE under CMP:*COMPILE-NATIVE*.  Signals unless the
result's entry is a SIMPLE-CORE-FUN, so this file never measures a bytecode function by accident."
  (let ((f (let ((cmp:*compile-native* t)) (compile nil lambda-expression))))
    (unless (string= (symbol-name (type-of f)) "SIMPLE-CORE-FUN")
      (error "~s compiled to a ~a, not native code" lambda-expression (type-of f)))
    f))

;;; The driver is native too, so its own loop cannot be charged to the construct.
(defparameter *native-driver*
  (native-compile
   '(lambda (f n)
     (funcall f 0)                       ; warm up once, outside the window
     (let ((before (gctools:bytes-allocated)))
       (dotimes (i n) (funcall f i))
       (- (gctools:bytes-allocated) before)))))

(defun native-bytes-per-call (lambda-expression &optional (n 10000))
  "Bytes allocated per call of the natively compiled LAMBDA-EXPRESSION over N calls, exact."
  (let ((cmp:*autocompile-hook* nil))
    (/ (funcall *native-driver* (native-compile lambda-expression) n) n)))

;;; ---- controls: the instrument moves, and the free shapes are free -----------------------

(test alloc-native.driver-alone
      (native-bytes-per-call '(lambda (i) (%alloc-touch i)))
      (0)
      :description "the native driver and a call into a notinline sink allocate nothing")

(test alloc-native.control-cons
      (native-bytes-per-call '(lambda (i) (%alloc-touch (cons i i))))
      (24)
      :description "one cons per call: the counter is live and byte-exact")

(test alloc-native.catch-throw
      (native-bytes-per-call '(lambda (i) (catch 'alloc-tag (%alloc-touch i) (throw 'alloc-tag nil))))
      (0)
      :description "the catch dynenv is stack-initialised; a throw allocates nothing")

(test alloc-native.block-return
      (native-bytes-per-call '(lambda (i) (block b (%alloc-touch i) (return-from b nil))))
      (0)
      :description "a block nothing closes over is a simple unwind; a local return-from is a jump")

(test alloc-native.special-bind
      (native-bytes-per-call '(lambda (i) (let ((*alloc-sink* i)) (%alloc-touch *alloc-sink*))))
      (0)
      :description "a special binding is a stack dynenv")

(test alloc-native.values-trampoline
      (native-bytes-per-call '(lambda (i) (multiple-value-call #'values (%alloc-three i))))
      (0)
      :description "passing three values through VALUES allocates nothing")

(test alloc-native.mvb.capturing
      (native-bytes-per-call '(lambda (i) (let ((y i)) (multiple-value-bind (a b) (%alloc-two i) (%alloc-touch (+ a b y))))))
      (0)
      :description "a MULTIPLE-VALUE-BIND whose body captures an outer variable: Cleavir inlines the bind lambda")

;;; ---- the laws: what an escaping closure costs natively -------------------------------------

(test alloc-native.closure-0
      (native-bytes-per-call '(lambda (i) (%alloc-call (lambda () (%alloc-touch 0)))))
      (0)
      :description "a capture-free lambda is a literal function object, whatever it is passed to")

(test alloc-native.closure-1
      (native-bytes-per-call '(lambda (i) (let ((y i)) (%alloc-call (lambda () (%alloc-touch y))))))
      (40)
      :description "an escaping closure over one variable: 32 + 8 (the law)")

(test alloc-native.closure-3
      (native-bytes-per-call '(lambda (i) (let ((y i) (z (1+ i)) (w (+ i 2))) (%alloc-call (lambda () (%alloc-touch (+ y z w)))))))
      (56)
      :description "an escaping closure over three variables: 32 + 24 (the law)")

(test alloc-native.assigned-capture
      (native-bytes-per-call '(lambda (i) (let ((y i)) (%alloc-call (lambda () (%alloc-touch y))) (setq y 0) (%alloc-touch y))))
      (64)
      :description "an escaping closure over a variable with two writers: 40 + a 24-byte cell (the law)")

(test alloc-native.escaping-block-closure
      (native-bytes-per-call '(lambda (i) (block b (%alloc-call (lambda () (return-from b nil))) (%alloc-touch i))))
      (72)
      :description "a lambda that RETURN-FROMs an outer block: a 40-byte closure plus a 32-byte heap BlockDynEnv (the law)")

(test alloc-native.mvb.surplus-value-in-this-pipeline
      (native-bytes-per-call '(lambda (i) (multiple-value-bind (a b) (%alloc-three i) (%alloc-touch (+ a b)))))
      (24)
      :description "a surplus value costs one cons in the (COMPILE NIL ...) pipeline, whose bind lambda is called through its general entry; COMPILE-FILE local-calls it and reads 0 -- a pipeline fact, not a leg fact")

;;; ---- the targets: the constructs the closure-law arc takes to 0 ---------------------------

(test alloc-native.unwind-protect.nocapture
      (native-bytes-per-call '(lambda (i) (unwind-protect (%alloc-touch i) (%alloc-touch 0))))
      (0)
      :description "a cleanup that captures nothing is a literal; the dynenv is on the stack")

(test alloc-native.unwind-protect.capture-1
      (native-bytes-per-call '(lambda (i) (let ((y i)) (unwind-protect (%alloc-touch i) (%alloc-touch y)))))
      (0)
      :description "read 40 until slice 4: the cleanup thunk was a heap closure over one variable; it lives in the frame now")

(test alloc-native.unwind-protect.capture-3
      (native-bytes-per-call '(lambda (i) (let ((y i) (z (1+ i)) (w (+ i 2))) (unwind-protect (%alloc-touch i) (%alloc-touch (+ y z w))))))
      (0)
      :description "read 56 until slice 4: 32 + 8 per captured variable, on the heap; in the frame now")

(test alloc-native.unwind-protect.assigned-capture
      (native-bytes-per-call '(lambda (i) (let ((y i)) (unwind-protect (setq y (1+ y)) (%alloc-touch y)))))
      (0)
      :description "read 64 until slice 4: the closure plus a cell for the captured, assigned variable; the variable is an alloca now")

(test alloc-native.with-lock-held
      (native-bytes-per-call '(lambda (i) (mp:with-lock (*alloc-lock*) (%alloc-touch i))))
      (0)
      :description "read 40 until slice 4: WITH-LOCK's cleanup captures the lock it must release, and that closure is in the frame now")

(test alloc-native.dx-flet
      (native-bytes-per-call '(lambda (i) (flet ((f () (%alloc-touch i))) (declare (dynamic-extent #'f)) (%alloc-call #'f))))
      (0)
      :description "reads 40 today: the DYNAMIC-EXTENT declaration reaches no BIR slot, and ENCLOSE puts only an unwind-protect cleanup in the frame (slice 7)")

(test alloc-native.handler-bind
      (native-bytes-per-call '(lambda (i) (handler-bind ((%alloc-full #'%alloc-global-handler)) (%alloc-touch i))))
      (0)
      :description "reads 72 today: three conses push the cluster onto *HANDLER-CLUSTERS* (slice 6)")

(test alloc-native.handler-bind.two
      (native-bytes-per-call '(lambda (i) (handler-bind ((%alloc-full #'%alloc-global-handler) (error #'%alloc-global-handler)) (%alloc-touch i))))
      (0)
      :description "reads 120 today: 72 for the first binding and 48 for each further one (slice 6)")

(test alloc-native.catch-handler-bind
      (native-bytes-per-call '(lambda (i) (catch 'alloc-tag (handler-bind ((%alloc-full #'%alloc-global-handler)) (%alloc-touch i)))))
      (0)
      :description "reads 72 today: the cheapest guarded region natively is the cluster alone (slice 6)")

(test alloc-native.handler-case.novar
      (native-bytes-per-call '(lambda (i) (handler-case (%alloc-touch i) (%alloc-full () (%alloc-touch 0)))))
      (0)
      :description "reads 144 today: the cluster 72, an escaping closure that GOes 40, a heap TagbodyDynEnv 32 (slice 5 to 72, slice 6 to 0)")

(test alloc-native.handler-case.var
      (native-bytes-per-call '(lambda (i) (handler-case (%alloc-touch i) (%alloc-full (c) (%alloc-touch c)))))
      (0)
      :description "reads 176 today: 144 plus a cell and a slot for the SETQ'd clause variable (slice 5 to 72, slice 6 to 0)")

(test alloc-native.ignore-errors
      (native-bytes-per-call '(lambda (i) (ignore-errors (%alloc-touch i))))
      (0)
      :description "reads 176 today: IGNORE-ERRORS is HANDLER-CASE with a clause variable (slice 5 to 72, slice 6 to 0)")

(test alloc-native.cnm-1arg
      (native-bytes-per-call '(lambda (i) (%alloc-touch (%alloc-gf1 *alloc-sub*))))
      (0)
      :description "reads 24 today: a method that mentions CALL-NEXT-METHOD pays the contf &REST gather, 24 per required argument, on every call (slice 5)")

(test alloc-native.cnm-2args
      (native-bytes-per-call '(lambda (i) (%alloc-touch (%alloc-gf2 *alloc-sub* i))))
      (0)
      :description "reads 48 today: 24 per required argument (slice 5)")

;;; ---- the stack cleanup's correctness: a THROW through it, and the collector under it -------

(test alloc-native.throw-through-cleanup
      (let ((seen nil))
        (funcall (native-compile
                  '(lambda (i)
                    (let ((y i))
                      (catch 'alloc-tag
                        (unwind-protect (progn (setq y (+ y 1)) (throw 'alloc-tag nil))
                          (%alloc-touch y))))))
                 41)
        (setq seen *alloc-sink*)
        seen)
      (42)
      :description "a stack cleanup closing over an assigned variable sees the assignment after a THROW through the protected form")

(test alloc-native.gc-stress-stack-cleanup
      (let ((total 0))
        (dotimes (round 60 total)
          (incf total
                (funcall (native-compile
                          '(lambda (i)
                            (let ((acc 0) (y i))
                              (unwind-protect
                                   (progn (dotimes (k 200) (setq acc (+ acc (length (list k k k)))))
                                          (gctools:garbage-collect)
                                          acc)
                                (setq acc (+ acc y)) (%alloc-touch acc)))))
                         1))))
      (36000)
      :description "60 rounds of a live stack cleanup across a forced collection with allocation inside the protected form: the process survives and every value is right (600 = 200 x 3 per round)")
