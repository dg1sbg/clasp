(in-package :cmp)

(export '(
          *cleavir-compile-file-hook*
          *cleavir-compile-hook*
          *compile-print*
          *compile-counter*
          *compile-duration-ns*
          *current-function*
          *current-function-name*
          *debug-compile-file*
          *generate-compile-file-load-time-values*
          *gv-current-function-name*
          *implicit-compile-hook*
          *irbuilder*
          *llvm-context*
          *load-time-value-holder-global-var*
          *low-level-trace*
          *low-level-trace-print*
          *run-time-literal-holder*
          #+(or)*run-time-values-table-name*
          #+(or)*run-time-values-table-global-var*
          *the-module*
          +cons-tag+
          +fixnum-tag+
          +vaslist-tag+
          +single-float-tag+
          +character-tag+
          +general-tag+
          %i1%
          %exception-struct%
          %fn-prototype%
          +fn-prototype-argument-names+
          %i32%
          %i64%
          %i8*%
          %i8%
          %mv-struct%
          %size_t%
          %t*%
          %t**%
          calling-convention-args
          calling-convention-args.va-arg
          calling-convention-va-list*
          calling-convention-nargs
          calling-convention-register-args
          calling-convention-write-registers-to-multiple-values
          cmp-log
          cmp-log-dump-module
          cmp-log-dump-function
          codegen-literal
          reference-literal
          codegen-rtv-cclasp
          compile-error-if-not-enough-arguments
          compile-in-env
          compile-lambda-function
          compile-lambda-list-code
          setup-calling-convention
          bclasp-compile-form
          compile-form
          compiler-error
          compiler-fatal-error
          compiler-message-file
          compiler-message-file-position
          safe-system
          irc-verify-module-safe
          irc-add
          irc-add-clause
          irc-basic-block-create
          irc-begin-block
          irc-br
          irc-branch-to-and-begin-block
          irc-cond-br
          irc-intrinsic-call
          irc-create-invoke
          irc-create-invoke-default-unwind
          irc-create-landing-pad
          irc-exception-typeid*
          irc-extract-value
          irc-generate-terminate-code
          irc-size_t-*current-source-pos-info*-filepos
          irc-size_t-*current-source-pos-info*-column
          irc-size_t-*current-source-pos-info*-lineno
          irc-icmp-eq
          irc-icmp-slt
          irc-intrinsic
          irc-load
          irc-low-level-trace
          irc-phi
          irc-personality-function
          irc-phi-add-incoming
          irc-renv
          irc-ret-void
          irc-store
          irc-switch
          irc-unreachable
          irc-trunc
          jit-constant-i1
          jit-constant-i8
          jit-constant-i32
          jit-constant-i64
          jit-constant-size_t
          jit-constant-unique-string-ptr
          jit-function-name
          module-make-global-string
          llvm-link
          link-builtins-module
          load-bitcode
          initialize-calling-convention
          treat-as-special-operator-p
          typeid-core-unwind
          walk-form-for-source-info
          with-catch
          with-try
          with-new-function-prepare-for-try
          with-dbg-function
          with-dbg-lexical-block
          dbg-set-current-source-pos
          dbg-set-current-source-pos-for-irbuilder
          *irbuilder-function-alloca*
          with-debug-info-generator
          with-irbuilder
          with-landing-pad
          irc-alloca-vaslist
          irc-alloca-va_list
          irc-alloca-register-save-area
          irc-alloca-invocation-history-frame
          irc-alloca-size_t
          compile-reference-to-literal
          ltv-global
          bclasp-compile
          make-uintptr_t
          make-calling-convention-impl
          +cons-car-offset+
          +cons-cdr-offset+
          +simple-vector._length-offset+
          %uintptr_t%
          %return_type%
          %vaslist%
          null-t-ptr
          compile-error-if-wrong-number-of-arguments
          compile-error-if-too-many-arguments
          compile-throw-if-excess-keyword-arguments
          compute-rest-alloc
          ))
