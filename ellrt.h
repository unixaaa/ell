/***** Executable and Linkable Lisp Runtime *****/

#ifndef ELL_H
#define ELL_H

#include <gc/gc.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "dict.h"
#include "list.h"

/**** Allocation ****/

#define ell_alloc GC_MALLOC

/**** Objects, Wrappers, Classes ****/

/* Wrappers introduce a level of indirection between objects and their
   classes, which will allow efficient implementation of method
   lookup.  See the paper ``Efficient Method Dispatch in PCL'' by
   Gregor J. Kiczales and Luis H. Rodriguez Jr. */

struct ell_obj;

struct ell_wrapper {
    struct ell_obj *class;
    list_t *type_args; // class object
};

struct ell_obj {
    struct ell_wrapper *wrapper;
    void *data;
};

struct ell_class_data {
    struct ell_obj *name;
    list_t *superclasses;
    struct ell_wrapper *wrapper;
    unsigned *type_params_ct;
};

struct ell_obj *
ell_make_obj(struct ell_wrapper *wrapper, void *data);
struct ell_obj *
ell_slot_value(struct ell_obj *obj, struct ell_obj *slot_sym);
struct ell_obj *
ell_set_slot_value(struct ell_obj *obj, struct ell_obj *slot_sym, struct ell_obj *val);
struct ell_obj *
ell_obj_class(struct ell_obj *obj);
bool
ell_is_instance(struct ell_obj *obj, struct ell_obj *class);

struct ell_wrapper *
ell_make_wrapper(struct ell_obj *class);
struct ell_obj *
ell_wrapper_class(struct ell_wrapper *wrapper);
void
ell_assert_wrapper(struct ell_obj *obj, struct ell_wrapper *wrapper);

struct ell_obj *
ell_make_class(struct ell_obj *name);
void
ell_add_superclass(struct ell_obj *class, struct ell_obj *superclass);
list_t *
ell_class_superclasses(struct ell_obj *class);
struct ell_wrapper *
ell_class_wrapper(struct ell_obj *class);
bool
ell_is_subclass(struct ell_obj *class, struct ell_obj *superclass);
struct ell_obj *
ell_class_name(struct ell_obj *class);

#define ELL_CLASS(name) __ell_class_##name
#define ELL_WRAPPER(name) __ell_wrapper_##name

/* Class class, the class of which classes are instances.
   Is a subclass of <object>.
   Is an instance of itself, although that may change. */
struct ell_obj *ELL_CLASS(class);
struct ell_wrapper *ELL_WRAPPER(class);

#define ELL_DEFCLASS(name, lisp_name)           \
    struct ell_obj *ELL_CLASS(name);            \
    struct ell_wrapper *ELL_WRAPPER(name);
ELL_DEFCLASS(obj, "<object>")
#include "defclass.h"
#undef ELL_DEFCLASS

/*** Built-in classes exported to Lisp ***/

struct ell_obj *__ell_g_LobjectG_1_;
struct ell_obj *__ell_g_LbooleanG_1_;
struct ell_obj *__ell_g_LclassG_1_;
struct ell_obj *__ell_g_LfunctionG_1_;
struct ell_obj *__ell_g_LgenericDfunctionG_1_;
struct ell_obj *__ell_g_LlinkedDlistG_1_;
struct ell_obj *__ell_g_LlistDrangeG_1_;
struct ell_obj *__ell_g_LstringG_1_;
struct ell_obj *__ell_g_LintegerG_1_;
struct ell_obj *__ell_g_LsymbolG_1_;
struct ell_obj *__ell_g_LsyntaxDlistG_1_;
struct ell_obj *__ell_g_LsyntaxDstringG_1_;
struct ell_obj *__ell_g_LsyntaxDsymbolG_1_;
struct ell_obj *__ell_g_LunspecifiedG_1_;
struct ell_obj *__ell_g_LconditionG_1_;

/* As a special case, `signal' is used from C but written in Lisp. */
struct ell_obj *__ell_g_signal_2_;

/**** Closures ****/

/* The calling convention for closures: the closure receives itself as
   first argument, the counts of positional and keyword arguments
   next, and finally the actual arguments array, allocated on the
   stack by the caller.  The array contains the positional arguments,
   followed by symbol/argument pairs for the keyword arguments. */

typedef unsigned int ell_arg_ct;

typedef struct ell_obj *
ell_code(struct ell_obj *clo, ell_arg_ct npos, ell_arg_ct nkey,
         struct ell_obj **args);

/* A closure consists of a code pointer and an environment pointer.
   Usually, the environment contains the free variables of the
   closure, and is populated by the compiler, but sometimes the
   runtime constructs special-purpose closures (such as the exit
   function passed to a block), that make special (or no) use of the
   environment pointer. */

struct ell_clo_data {
    ell_code *code;
    void *env;
};

struct ell_obj *
ell_make_clo(ell_code *code, void *env);
void *
ell_clo_env(struct ell_obj *clo);
struct ell_obj *
ell_call_unchecked(struct ell_obj *clo, ell_arg_ct npos, ell_arg_ct nkey, struct ell_obj **args);
struct ell_obj *
ell_call(struct ell_obj *clo, ell_arg_ct npos, ell_arg_ct nkey, struct ell_obj **args);
void
ell_check_npos(ell_arg_ct formal_npos, ell_arg_ct actual_npos);

#define ELL_CALL(clo, ...)                                              \
    ({                                                                  \
        struct ell_obj *__ell_args[] = { __VA_ARGS__ };                 \
        ell_arg_ct npos = sizeof(__ell_args) / sizeof(struct ell_obj *); \
        ell_call(clo, npos, 0, __ell_args);                             \
    })

/**** Generic Functions ****/

struct ell_method_entry {
    struct ell_obj *method; // clo
    list_t *specializers; // class
};

struct ell_generic_data {
    list_t *method_entries;
};

struct ell_obj *
ell_make_generic();
void
ell_generic_add_method(struct ell_obj *generic, struct ell_obj *clo,
                       list_t *specializers);
struct ell_obj *
ell_generic_find_method(struct ell_obj *generic, list_t *specialized_args);

#define ELL_GENERIC(name) __ell_g_##name##_2_

#define ELL_DEFGENERIC(name, lisp_name) struct ell_obj *ELL_GENERIC(name);
#include "defgeneric.h"
#undef ELL_DEFGENERIC

/**** Methods ****/

void
ell_put_method(struct ell_obj *gf, struct ell_obj *clo, list_t *specializers);

/* This basically simulates a Smalltalk-like OO system on top of the
   built-in generic functions, for bootstrap simplicity. */
void
ell_put_method_legacy(struct ell_obj *class, struct ell_obj *gf,
                      struct ell_obj *clo, int args_ct);
struct ell_obj *
ell_send(struct ell_obj *rcv, struct ell_obj *generic,
         ell_arg_ct npos, ell_arg_ct nkey, struct ell_obj **args);

#define ELL_SEND(rcv, msg, ...)                                         \
    ({                                                                  \
        struct ell_obj *__ell_rcv = rcv;                                \
        struct ell_obj *__ell_send_args[] = { __ell_rcv, __VA_ARGS__ }; \
        ell_arg_ct npos = sizeof(__ell_send_args) / sizeof(struct ell_obj *); \
        ell_send(__ell_rcv, ELL_GENERIC(msg), npos, 0, __ell_send_args); \
    })

#define ELL_METHOD_CODE(class, msg) __ell_method_code_##class##_##msg

#define ELL_DEFMETHOD(class, msg, formal_npos)                          \
    ell_code ELL_METHOD_CODE(class, msg);                               \
                                                                        \
    __attribute__((constructor(201))) static void                       \
    __ell_init_method_##class##_##msg()                                 \
    {                                                                   \
        struct ell_obj *clo =                                           \
            ell_make_clo(&ELL_METHOD_CODE(class, msg), NULL);           \
        ell_put_method_legacy(ELL_CLASS(class), ELL_GENERIC(msg), clo,  \
                              formal_npos);                             \
    }                                                                   \
                                                                        \
    struct ell_obj *                                                    \
    ELL_METHOD_CODE(class, msg)(struct ell_obj *clo, ell_arg_ct npos,   \
                                ell_arg_ct nkey, struct ell_obj **args) \
    {

#define ELL_PARAM(name, i)                      \
    struct ell_obj *name = args[i];

#define ELL_END                                 \
    }

/**** Control Flow ****/

struct ell_unwind_protect {
    struct ell_unwind_protect *parent;
    struct ell_obj *cleanup;
};

struct ell_block {
    struct ell_unwind_protect *parent;
    struct ell_obj *volatile val;
    jmp_buf dest;
};

struct ell_unwind_protect *ell_current_unwind_protect;

struct ell_obj *
ell_block(struct ell_obj *fun);

struct ell_obj *
ell_unwind_protect(struct ell_obj *protected, struct ell_obj *cleanup);

/**** Strings ****/

struct ell_str_data {
    char *chars;
};

struct ell_obj *
ell_make_strn(char *chars, size_t len);
struct ell_obj *
ell_make_str(char *chars);
char *
ell_str_chars(struct ell_obj *str);
size_t
ell_str_len(struct ell_obj *str);
char
ell_str_char_at(struct ell_obj *str, size_t i);
struct ell_obj *
ell_str_poplast(struct ell_obj *str);

/**** Numbers ****/

struct ell_num_int_data {
    int int_value;
};

struct ell_obj *
ell_make_num(char *chars);
struct ell_obj *
ell_make_num_from_int(int i);
int
ell_num_int(struct ell_obj *num);

/**** Symbols ****/

struct dict_t ell_sym_tab; // char* -> sym

struct ell_sym_data {
    struct ell_obj *name; // str
};

#define ELL_SYM(name) __ell_sym_##name

#define ELL_DEFSYM(name, lisp_name) __attribute__((weak)) struct ell_obj *ELL_SYM(name);
#include "defsym.h"
#undef ELL_DEFSYM

struct ell_obj *
ell_intern(struct ell_obj *str);
struct ell_obj *
ell_sym_name(struct ell_obj *sym);
int
ell_sym_cmp(struct ell_obj *sym_a, struct ell_obj *sym_b);

/**** Booleans ****/

// Lisp names
struct ell_obj *__ell_g_Ot_1_;
struct ell_obj *__ell_g_Of_1_;
// C names for convenience
struct ell_obj *ell_t;
struct ell_obj *ell_f;

bool
ell_is_true(struct ell_obj *obj);
struct ell_obj *
ell_truth(bool b);

/**** Unspecified value ****/

struct ell_obj *__ell_g_unspecified_1_;
struct ell_obj *ell_unspecified;

/**** Unbound marker ****/

struct ell_obj *ell_unbound;

/**** Lists ****/

struct ell_lst_data {
    list_t elts;
};

struct ell_obj *
ell_make_lst();

/**** Ranges ****/

struct ell_list_range_data {
    list_t *elts;
    lnode_t *cur;
};

struct ell_obj *
ell_make_range_from_list(list_t *elts);

/**** Syntax Objects ****/

struct ell_obj *
ell_parse();

struct ell_cx {
    uuid_t uuid;
};

struct ell_stx_sym_data {
    struct ell_obj *sym;
    struct ell_cx *cx; // maybe NULL
};

struct ell_stx_str_data {
    struct ell_obj *str;
};

struct ell_stx_num_data {
    struct ell_obj *num;
};

struct ell_stx_lst_data {
    list_t elts;
};

struct ell_obj *
ell_make_stx_sym(struct ell_obj *sym);
struct ell_obj *
ell_make_stx_sym_cx(struct ell_obj *sym, struct ell_cx *cx);
struct ell_obj *
ell_make_stx_str(struct ell_obj *str);
struct ell_obj *
ell_make_stx_num(struct ell_obj *num);
struct ell_obj *
ell_make_stx_lst();
struct ell_obj *
ell_stx_sym_sym(struct ell_obj *stx_sym);
struct ell_cx *
ell_stx_sym_cx(struct ell_obj *stx_sym);
struct ell_obj *
ell_stx_str_str(struct ell_obj *stx_str);
struct ell_obj *
ell_stx_num_num(struct ell_obj *stx_num);
list_t *
ell_stx_lst_elts(struct ell_obj *stx_lst);
listcount_t
ell_stx_lst_len(struct ell_obj *stx_lst);
void
ell_assert_stx_lst_len(struct ell_obj *stx_lst, listcount_t len);
void
ell_assert_stx_lst_len_min(struct ell_obj *stx_lst, listcount_t len);
struct ell_cx *
ell_make_cx();
bool
ell_cx_equal(struct ell_cx *cxa, struct ell_cx *cxb);
int
ell_cx_cmp(struct ell_cx *cxa, struct ell_cx *cxb);

/* The use of this hygiene context is described in 'ellc.c' a bit
   more.  Basically, it always holds the hygiene context of the
   current quasisyntax, if we're in a quasisyntax, to implement
   SRFI-72's hygiene condition, which states that quasiyntaxes
   "enclosed" in another quasisyntax share the enclosing quasisyntax's
   hygiene context.

   Note that SRFI-72 isn't completely clear on what "enclosed" really
   means.  Our interpretation here, which seems to check out in the
   tests done so far, is that "enclosure" stops at lambda boundaries.
   This means when the evaluation of a quasisyntax leads to the
   evaluation of a nested lambda that returns a quasisyntax, that
   quasisyntax is _not_ considered to be enclosed in the outer
   quasisyntax. */
static struct ell_cx *__ell_cur_cx = NULL;

/**** Utilities ****/

list_t *
ell_util_make_list();
void
ell_util_list_add(list_t *list, void *elt);
list_t *
ell_util_sublist(list_t *list, listcount_t start);
bool
ell_util_list_contains(list_t *list, void *elt, dict_comp_t compare);
void
ell_util_assert_list_len(list_t *list, listcount_t len);
void
ell_util_assert_list_len_min(list_t *list, listcount_t len);
bool
ell_util_lists_equal(list_t *l1, list_t *l2, dict_comp_t compare);
dict_t *
ell_util_make_dict(dict_comp_t comp);
void *
ell_util_dict_put(dict_t *dict, void *key, void *val);
void
ell_util_set_add(list_t *set, void *elt, dict_comp_t compare);
int
ell_ptr_cmp(void *a, void *b);

/**** Utilities for Generated Code ****/

__attribute__((weak)) struct ell_obj *ell_result;

void
ell_arity_error();
struct ell_obj *
ell_unbound_arg();
struct ell_obj *
ell_unbound_var(char *name);
struct ell_obj *
ell_unbound_fun(char *name);
struct ell_obj **
ell_make_box(struct ell_obj *value);
struct ell_obj *
ell_box_read(struct ell_obj **box);
struct ell_obj *
ell_box_write(struct ell_obj **box, struct ell_obj *value);
struct ell_obj *
ell_lookup_key(struct ell_obj *key_sym, ell_arg_ct npos, ell_arg_ct nkey,
               struct ell_obj **args);

/**** Emitted Code Macros ****/

// mid = mangled ID
// sid = original ID literal string

#define ELL_GEN_GLO_REF(mid, sid)  (mid != ell_unbound ? mid : ell_unbound_var(sid))
#define ELL_GEN_GLO_FREF(mid, sid) (mid != ell_unbound ? mid : ell_unbound_fun(sid))
#define ELL_GEN_ARG_REF_PLAIN(mid) (mid)
#define ELL_GEN_ARG_REF_BOXED(mid) (ell_box_read(mid))
#define ELL_GEN_ENV_REF_PLAIN(mid) (__ell_env->mid)
#define ELL_GEN_ENV_REF_BOXED(mid) (ell_box_read(__ell_env->mid))
#define ELL_GEN_DEF(mid, val)      (mid = val)
#define ELL_GEN_DEFP(mid)          (mid != ell_unbound ? ell_t : ell_f)
#define ELL_GEN_GLO_SET(mid, sid, val)  ({ if (mid == ell_unbound) ell_unbound_var(sid); mid = val; })
#define ELL_GEN_ARG_SET_PLAIN(mid, val) (mid = val)
#define ELL_GEN_ARG_SET_BOXED(mid, val) (ell_box_write(mid, val))
#define ELL_GEN_ENV_SET_PLAIN(mid, val) (__ell_env->mid = val)
#define ELL_GEN_ENV_SET_BOXED(mid, val) (ell_box_write(__ell_env->mid, val))
#define ELL_GEN_COND(test, _then, _else) (ell_is_true(test) ? _then : _else)
#define ELL_GEN_LOOP(expr)              ({ for(;;) { expr; }; ell_unspecified; })

/**** Misc ****/

#define ell_fail(...)                                   \
    ({ printf(__VA_ARGS__); exit(EXIT_FAILURE); })

#endif
