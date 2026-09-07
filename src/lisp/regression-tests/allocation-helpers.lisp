(in-package #:clasp-tests)

;;;; The helpers the two allocation suites share: allocation.lisp (the bytecode tier) and
;;;; allocation-native.lisp (the native tier) measure the same shapes through the same sinks, so the
;;;; sinks live once, here, and each suite LOADs this file at its top.  Loading it twice is harmless.

(defvar *alloc-sink* nil)
(defvar *alloc-lock* (mp:make-lock :name "allocation-test"))

(define-condition %alloc-full (error) ())

(declaim (notinline %alloc-touch %alloc-two %alloc-three %alloc-global-handler %alloc-call))
(defun %alloc-touch (x) (setq *alloc-sink* x) nil)
(defun %alloc-two (i) (values i (1+ i)))
(defun %alloc-three (i) (values i (1+ i) (+ i 2)))
(defun %alloc-global-handler (c) (declare (ignore c)) nil)
(defun %alloc-call (f) (funcall f))

;;; A two-class hierarchy for the CALL-NEXT-METHOD rows: a method that mentions CALL-NEXT-METHOD
;;; takes the contf convention, and the convention's cost is what the rows read.
(defclass %alloc-base () ())
(defclass %alloc-sub (%alloc-base) ())
(defgeneric %alloc-gf1 (x))
(defmethod %alloc-gf1 ((x %alloc-base)) x)
(defmethod %alloc-gf1 ((x %alloc-sub)) (call-next-method))
(defgeneric %alloc-gf2 (x y))
(defmethod %alloc-gf2 ((x %alloc-base) y) (declare (ignore y)) x)
(defmethod %alloc-gf2 ((x %alloc-sub) y) (declare (ignore y)) (call-next-method))
(defvar *alloc-sub* (make-instance '%alloc-sub))
