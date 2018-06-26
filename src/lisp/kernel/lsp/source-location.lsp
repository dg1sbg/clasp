;;;
;;; Any symbols we want to export from EXT must be done in init.lsp
;;;

(in-package :ext)
(defun compiled-function-name (x)
  (core:function-name x))

(defun compiled-function-file* (x)
  (assert (functionp x))
  (multiple-value-bind (sfi pos lineno)
      (core:function-source-pos x)
    (let* ((source-file (core:source-file-info-source-debug-namestring sfi)))
      (when source-file
        (let* ((src-pathname (pathname source-file))
               (src-directory (pathname-directory src-pathname))
               (src-name (pathname-name src-pathname))
               (src-type (pathname-type src-pathname))
               (filepos (+ (core:source-file-info-source-debug-offset sfi) pos)))
          (let* ((pn (if (eq (car src-directory) :relative)
                         (merge-pathnames src-pathname (translate-logical-pathname "source-dir:"))
                         src-pathname)))
            (return-from compiled-function-file* (values pn filepos lineno))))))))

(defun compiled-function-file (x)
  "This is provided for slime - it can removed once slime switches to source-location"
  (cond
    ((and x (core:single-dispatch-generic-function-p x))
     (let ((locs (source-location x t)))
       (values (source-location-pathname (car locs)) (source-location-offset (car locs)))))
    ((and x (functionp x)) (compiled-function-file* x))
    (t (values nil 0 0))))

#+(or)
(eval-when (:compile-toplevel :load-toplevel :execute)
  (format t "About to define the first defstruct~%")
  (setq core:*echo-repl-read* t))


(defstruct source-location
  pathname offset)

;;; FIXME: above struct is redundant and should be vaporized.
(defun class-source-position (class)
  (let ((csp (clos:class-source-position class)))
    (when csp
      (make-source-location
       :pathname (core:source-file-info-pathname
                  (core:source-file-info
                   (core:source-pos-info-file-handle csp)))
       :offset (core:source-pos-info-filepos csp)))))

(defun source-location-impl (name kind)
  "* Arguments
- name : A symbol.
- kind : A symbol (:function :method :class)
Return the source-location for the name/kind pair"
  (labels ((fix-paths-and-make-source-locations (rels)
             (declare (core:lambda-name 'fix-paths-and-make-source-locations))
             (let ((source-dir (translate-logical-pathname #P"source-dir:")))
               (mapcar (lambda (dir-pos)
                         (let ((dir (first dir-pos))
                               (pos (second dir-pos)))
                           (make-source-location :pathname (merge-pathnames dir source-dir)
                                                 :offset pos)))
                       rels))))
    (case kind
      (:class
       (let ((class (find-class name nil)))
         (when class
           (let ((source-loc (class-source-position class)))
             (when source-loc (list source-loc))))))
      (:method
          (let ((source-loc (core:get-sysprop name 'core:cxx-method-source-location)))
            (fix-paths-and-make-source-locations source-loc)))
      (:function
       (cond
         ((and (fboundp name) (core:single-dispatch-generic-function-p (fdefinition name)))
          (source-location name :method))
         ((fboundp name)
          (multiple-value-bind (file pos)
              (compiled-function-file* (fdefinition name))
            (list (make-source-location :pathname file :offset pos)))))))))

(defparameter *source-location-kinds* '(:class :method :function))

(defparameter *source-location-kinds* '(:class :method :function))

(defun source-location (obj kind)
  "* Arguments
- obj : A symbol or object.
- kind : A symbol (:function :method :class t)
Return the source-location for the name/kind pair"
  (cond
    ((eq kind t)
     (cond
       ((clos:classp obj)
        (let ((source-loc (class-source-position obj)))
          (when source-loc (list source-loc))))
       ((core:single-dispatch-generic-function-p obj)
        (source-location (core:function-name obj) :method))
       ((functionp obj)
        (multiple-value-bind (file pos)
            (compiled-function-file* obj)
          (list (make-source-location :pathname file :offset pos))))))
    ((symbolp kind) (source-location-impl obj kind))
    (t (error "Cannot obtain source-location for ~a of kind ~a" obj kind))))

#||
#+cclasp
(defmethod make-load-form ((object core:source-file-info) &optional environment)
  `(core:decode (core:make-cxx-object 'core:source-file-info) ',(core:encode object)))

#+cclasp
(defmethod make-load-form ((object core:source-pos-info) &optional environment)
  `(core:decode (core:make-cxx-object 'core:source-pos-info) ',(core:encode object)))
||#
