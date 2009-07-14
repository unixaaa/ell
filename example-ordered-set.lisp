;;; The classical set example of higher-order modules from
;;; http://caml.inria.fr/pub/docs/manual-ocaml/manual004.html 
;;;
;;; Also shows how functors are instantiated and how finally modules
;;; are erased from the resulting code by the compiler.

(defpackage Ordered-Type ()
  (interface
    (defclass <t>)
    (defun compare (<t> <t> -> <int>))))

(defpackage Set ()
  (interface
    (defclass <t>)
    (defclass <elt>)
    (defun make (-> <t>))
    (defun add (<t> <elt>))))

(defpackage Set-Impl ((Elt Ordered-Type))
  (implementation
    (defclass <t> (elements <list> init: (list)))
    (defclass <elt> Elt::<t>)
    (defun make (-> <t>) (make-t))
    (defun add (<t> <elt>) (... (Elt::compare elt ...) ...))))

(defpackage Make-Set ((Elt Ordered-Type) -> Set)
  (Set-Impl Elt))

;;;

(defpackage CI-String ()
  (implementation
    (deftype <t> <str>)
    (defun compare ((s1 <str>) (s2 <str>) -> <int>)
      (declare inline)
      (strcmp (tolower s1) (tolower s2)))))

(defpackage CI-String-Set ()
  (Make-Set CI-String))

;;;

(defpackage My ()
  (implementation
    (use S = CI-String-Set)
    (let set = (S::make)
      (S::add set "foo"))))

;; after defunctorization:

(defpackage My ()
  (implementation
    (let set = (Set-Impl##CI-String::make)
      (Set-Impl##CI-String::add set "foo"))))

(defpackage Set-Impl##CI-String ()
  (implementation
    (defclass <t> (elements <list> init: (list)))
    (deftype <elt> CI-String::<t>)
    (defun make (-> <t>) (make-t))
    (defun add (<t> <elt>) (... (strcmp (tolower elt) (tolower ...)) ...)))) ; inlining

;; after demodularization:

(let My::set = (Set-Impl##CI-String::make)
  (Set-Impl##CI-String::add My::set "foo"))

(defclass Set-Impl##CI-String::<t> (elements <list> init: (list)))
(deftype Set-Impl##CI-String::<elt> CI-String::<t>)
(defun Set-Impl##CI-String::make (-> Set-Impl##CI-String::<t>) (Set-Impl##CI-String::make-t))
(defun Set-Impl##CI-String::add (Set-Impl##CI-String::<t> Set-Impl##CI-String::<elt>)
  (... (strcmp (tolower elt) (tolower ...)) ...))
   
(deftype CI-String::<t> <str>)
(defun CI-String::compare ((s1 <str>) (s2 <str>) -> <int>)
  (declare inline)
  (strcmp (tolower s1) (tolower s2)))