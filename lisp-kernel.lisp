(defpackage Lisp-Kernel ()
  (interface
    ;; Built-in classes and objects
    (defclass <object>)
    (defclass <class>)
    (defclass <function>)
    (defclass <generic> <: <function>)
    (defclass <method> <: <function>)
    (defclass <package>)
    (defclass <form>)
    (defclass <symbol> <: <form>)
    (defclass <list> <: <form>)
    (defclass <null>)
    (defclass <boolean>)
    (defvar nil)
    (defvar true)
    (defvar false)
    ;; Variables
    (defmacro defparameter (<symbol> <object>))
    (defmacro setq (<symbol> <object>))
    (defun boundp (<symbol> -> <boolean>))
    ;; Functions
    (defmacro lambda (<signature> <form> -> <function>))
    (defmacro defun (<symbol> <function>))
    (defmacro function (<symbol> -> <function>))
    (defun apply (<function> &rest args &all-keys key-args -> <object>))
    (defun funcall (<function> args key-args))
    (defun fboundp (<symbol> -> <boolean>))
    ;; Control
    (defmacro if (<boolean> <form> <form> -> <object>))
    (defmacro progn (&body forms -> <object>))
    (defmacro unwind-protect (<form> <form> -> <object>))
    (defun call-with-escape-continuation (<function> -> <object>))
    ;; Syntax transformers and reflective tower
    (defmacro defmacro (<signature> <function>))
    (defmacro macrolet (transformer-bindings <form> -> <object>))
    (defmacro eval-when-compile (<form>))
    ;; Syntax objects
    (defun first (<list> -> <form>))
    (defun rest (<list> -> <list>))
    (defun elt (<list> <number> -> <form>))
    (defun quote (<form> -> <form>))
    (defun quasiquote (<form> -> <form>))
    (defun datum->syntax-object (<environment> <form> -> <form>))
    ;; Types and Objects
    (defun call-method (<object> <symbol> &rest args &all-keys key-args -> <object>))
    (defun slot-value (<object> <symbol> -> <symbol>))
    (defun set-slot-value (<object> <symbol> <object>))
    (defun class-of (<object> -> <class>))
    ;; Classes
    (defmacro make-class ((<symbol> &rest super-classes) &rest slot-specs
                           &key super-classes-mutable-p 
                                slot-specs-mutable-p
                                methods-mutable-p
                           -> <class>))
    (defun set-super-classes (<class> &rest super-classes))
    (defun set-slot-specs (<class> &rest slot-specs))
    (defun set-method (<class> <symbol> <function>))
    ;; Conditions
    (defmacro handler-bind (handler-bindings <form> -> <object>))
    (defun signal (<condition> -> <object>))
    ;; Evaluation
    (defun eval (<form> -> <object>))
    ;; Packages
    (defmacro defpackage (<symbol> <signature> <package>))
    (defmacro use (&all-keys package-bindings))
    (defmacro interface (<declarations> -> <package>))
    (defmacro implementation (<definitions> -> <package>))
    (defmacro include <package>)
    ;; UNIX
    (defmacro c (<string> -> <object>))
  )
)

;; Control flow and condition system: Dylan, Goo, Common Lisp, PLT Scheme
;; Inline C: Alien Goo
;; Packages and functors: OCaml, C++, Dylan, PLOT
;; Object system and generic functions: CLOS, Dylan, Factor, C++
;; Hygienic macros, reflective tower, syntax objects: SRFI-72
;; Variables, functions, terminology: Common Lisp, Dylan, Goo, PLOT
