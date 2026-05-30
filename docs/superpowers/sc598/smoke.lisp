;;;; M0 smoke test: the aarch64-linux Clasp runs, is the bytecode build, and can compile+run.
(format t "~&clasp-in-features: ~a~%" (and (member :clasp *features*) t))
(format t "impl: ~a ~a~%" (lisp-implementation-type) (lisp-implementation-version))
(defun fib (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))
(format t "fib(25)=~a~%" (fib 25))
(let ((sq (compile nil '(lambda (x) (* x x)))))
  (format t "compiled-square-7=~a~%" (funcall sq 7)))
(format t "smoke-ok~%")
