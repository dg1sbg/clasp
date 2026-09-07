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
        ;; unwind-protect: the protect opcode builds the cleanup closure (a capture-free one no longer)
        alloc.unwind-protect.capture-1 alloc.unwind-protect.capture-3
        alloc.unwind-protect.assigned-capture alloc.with-lock-held
        ;; multiple-value-bind: a macro over multiple-value-call of a lambda
        alloc.mvb.capturing alloc.mvb.surplus-value
        ;; handler-bind / handler-case: the cluster conses, the entry dynenv, the handler closure
        alloc.handler-bind alloc.handler-bind.global-handler
        alloc.handler-case.novar alloc.handler-case.var
        ;; a closed-over tagbody: the entry opcode's heap dynenv
        alloc.closed-over-tagbody
        ;; allocation-native.lisp: the NATIVE tier's targets, each under the slice of the closure-law
        ;; plan (neoseidr docs/plans/2026-09-06-clasp-closure-law/04-slices.md) that removes its bytes.
        ;; slice 4 made an unwind-protect cleanup a stack closure: its four rows and with-lock read 0
        ;; slice 5 -- the contf &rest gather becomes a vaslist
        alloc-native.cnm-1arg alloc-native.cnm-2args
        ;; slice 5 takes handler-case to the cluster's 72; slice 6 takes the cluster to 0
        alloc-native.handler-bind alloc-native.handler-bind.two alloc-native.catch-handler-bind
        alloc-native.handler-case.novar alloc-native.handler-case.var alloc-native.ignore-errors
        ;; slice 7 -- a declared DYNAMIC-EXTENT local function becomes a stack closure
        alloc-native.dx-flet
        ;; on boehm key-or-value tables are effectively strong.
        #+use-boehm weak-key-or-value-weakness))
