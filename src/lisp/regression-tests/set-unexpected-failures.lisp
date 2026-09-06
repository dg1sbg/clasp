(in-package #:clasp-tests)

(setq *expected-failures*
      '(random-short random-double random-long
        ;; compile-file-no-unwind
        types-classes-10
        sbcl-cross-compile-4 ;;;not important
        ;; include-level-2a
        include-level-2b include-level-3 ;;; a problem for sbcl x-compiling
        frame-function frame-locals
        ;; allocation.lisp: the bytecode tier's control-flow constructs still heap-allocate.
        ;; A name is removed from this list by the change that removes its bytes; until then a
        ;; test that starts reading 0 shows up as an Unexpected Success (the run still exits 0).
        ;; unwind-protect: the protect opcode builds the cleanup closure
        alloc.unwind-protect.nocapture
        alloc.unwind-protect.capture-1 alloc.unwind-protect.capture-3
        alloc.unwind-protect.assigned-capture alloc.with-lock-held
        ;; multiple-value-bind: a macro over multiple-value-call of a lambda
        alloc.mvb.capturing alloc.mvb.surplus-value
        ;; handler-bind / handler-case: the cluster conses, the entry dynenv, the handler closure
        alloc.handler-bind alloc.handler-bind.global-handler
        alloc.handler-case.novar alloc.handler-case.var
        ;; a closed-over tagbody: the entry opcode's heap dynenv
        alloc.closed-over-tagbody
        ;; on boehm key-or-value tables are effectively strong.
        #+use-boehm weak-key-or-value-weakness))
