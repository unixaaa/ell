/***** Executable and Linkable Lisp Compiler *****/

#include "ellc.h"

/**** Parsing ****/

#include "grammar.c"

struct ellc_parser_stack {
    struct ellc_parser_stack *down;
    struct ell_obj *stx_lst;
};

static struct ellc_parser_stack *ellc_parser_stack_top;

static struct ell_obj *
ellc_parse()
{
    ellc_parser_stack_top = 
        (struct ellc_parser_stack *) ell_alloc(sizeof(*ellc_parser_stack_top));
    ellc_parser_stack_top->down = NULL;
    ellc_parser_stack_top->stx_lst = ell_make_stx_lst();
    while(yyparse())
        ;
    return ellc_parser_stack_top->stx_lst;
}

static void
ellc_parser_add_sym(char *chars)
{
    struct ell_obj *stx_sym = ell_make_stx_sym(ell_intern(ell_make_str(chars)));
    ELL_SEND(ellc_parser_stack_top->stx_lst, add, stx_sym);
}

static void
ellc_parser_push()
{
    struct ellc_parser_stack *new = 
        (struct ellc_parser_stack *) ell_alloc(sizeof(*new));
    struct ell_obj *new_stx_lst = ell_make_stx_lst();
    new->down = ellc_parser_stack_top;
    new->stx_lst = new_stx_lst;
    ELL_SEND(ellc_parser_stack_top->stx_lst, add, new_stx_lst);
    ellc_parser_stack_top = new;
}

static void
ellc_parser_pop()
{
    ellc_parser_stack_top = ellc_parser_stack_top->down;
}

/**** AST Utilities ****/

static struct ellc_id *
ellc_make_id(struct ell_obj *sym, enum ellc_ns ns)
{
    ell_assert_brand(sym, ELL_BRAND(sym));
    struct ellc_id *id = (struct ellc_id *) ell_alloc(sizeof(*id));
    id->sym = sym;
    id->ns = ns;
    return id;
}

static bool
ellc_id_equal(struct ellc_id *a, struct ellc_id *b)
{
    return (a->sym == b->sym) && (a->ns == b->ns);
}

static struct ellc_ast_seq *
ellc_make_ast_seq()
{
    struct ellc_ast_seq *ast_seq = ell_alloc(sizeof(*ast_seq));
    ast_seq->exprs = ell_util_make_list();
    return ast_seq;
}

static void
ellc_ast_seq_add(struct ellc_ast_seq *ast_seq, struct ellc_ast *expr)
{
    ell_util_list_add(ast_seq->exprs, expr);
}

static struct ellc_ast *
ellc_make_ast(enum ellc_ast_type type)
{
    struct ellc_ast *ast = (struct ellc_ast *) ell_alloc(sizeof(*ast));
    ast->type = type;
    return ast;
}

/**** Normalization: Syntax Objects -> Normal Form AST ****/

static struct ellc_ast *
ellc_norm_stx(struct ell_obj *stx);

typedef struct ellc_ast *
(ellc_norm_fun)(struct ell_obj *stx_lst);

/* (Simple Forms) */

static struct ellc_ast *
ellc_make_ref(struct ell_obj *stx_id, enum ellc_ns ns)
{
    struct ellc_ast *ast = ellc_make_ast(ELLC_AST_REF);
    ast->ref.id = ellc_make_id(ell_stx_sym_sym(stx_id), ns);
    return ast;
}

static struct ellc_ast *
ellc_norm_ref(struct ell_obj *stx_sym)
{
    return ellc_make_ref(stx_sym, ELLC_NS_VAR);
}

static struct ellc_ast *
ellc_norm_fref(struct ell_obj *stx_lst)
{
    ell_assert_stx_lst_len(stx_lst, 2);
    return ellc_make_ref(ELL_SEND(stx_lst, second), ELLC_NS_FUN);
}

static struct ellc_ast *
ellc_make_def(struct ell_obj *stx_lst, enum ellc_ns ns)
{
    ell_assert_stx_lst_len(stx_lst, 3);
    struct ellc_ast *ast = ellc_make_ast(ELLC_AST_DEF);
    ast->def.id = ellc_make_id(ell_stx_sym_sym(ELL_SEND(stx_lst, second)), ns);
    ast->def.val = ellc_norm_stx(ELL_SEND(stx_lst, third));
    return ast;
}

static struct ellc_ast *
ellc_norm_def(struct ell_obj *stx_lst)
{
    return ellc_make_def(stx_lst, ELLC_NS_VAR);
}

static struct ellc_ast *
ellc_norm_fdef(struct ell_obj *stx_lst)
{
    return ellc_make_def(stx_lst, ELLC_NS_FUN);
}

static struct ellc_ast *
ellc_make_set(struct ell_obj *stx_lst, enum ellc_ns ns)
{
    ell_assert_stx_lst_len(stx_lst, 3);
    struct ellc_ast *ast = ellc_make_ast(ELLC_AST_SET);
    ast->set.id = ellc_make_id(ell_stx_sym_sym(ELL_SEND(stx_lst, second)), ns);
    ast->set.val = ellc_norm_stx(ELL_SEND(stx_lst, third));
    return ast;    
}

static struct ellc_ast *
ellc_norm_set(struct ell_obj *stx_lst)
{
    return ellc_make_set(stx_lst, ELLC_NS_VAR);
}

static struct ellc_ast *
ellc_norm_fset(struct ell_obj *stx_lst)
{
    return ellc_make_set(stx_lst, ELLC_NS_VAR);
}

static struct ellc_ast *
ellc_norm_cond(struct ell_obj *stx_lst)
{
    ell_assert_stx_lst_len(stx_lst, 4);
    struct ellc_ast *ast = ellc_make_ast(ELLC_AST_COND);
    ast->cond.test = ellc_norm_stx(ELL_SEND(stx_lst, second));
    ast->cond.consequent = ellc_norm_stx(ELL_SEND(stx_lst, third));
    ast->cond.alternative = ellc_norm_stx(ELL_SEND(stx_lst, fourth));
    return ast;
}

static struct ellc_ast *
ellc_norm_seq(struct ell_obj *stx_lst)
{
    ell_assert_stx_lst_len_min(stx_lst, 1);
    struct ellc_ast *ast = ellc_make_ast(ELLC_AST_SEQ);
    list_init(ast->seq.exprs, LISTCOUNT_T_MAX);
    list_t *elts_stx = ell_util_sublist(ell_stx_lst_elts(stx_lst), 1);

    for (lnode_t *n = list_first(elts_stx); n; n = list_next(elts_stx, n)) {
        struct ell_obj *stx = (struct ell_obj *) lnode_get(n);
        ellc_ast_seq_add(&ast->seq, ellc_norm_stx(stx));
    }

    return ast;
}

/* (Application and Arguments Dissection) */

static bool
ellc_is_key_arg_sym(struct ell_obj *sym)
{
    ell_assert_brand(sym, ELL_BRAND(sym));
    struct ell_obj *name_str = ell_sym_name(sym);
    size_t len = ell_str_len(name_str);
    return (len > 1) && (ell_str_char_at(name_str, len - 1) == ':');
}

static struct ell_obj *
ellc_clean_key_arg_sym(struct ell_obj *sym)
{
    ell_assert_brand(sym, ELL_BRAND(sym));
    struct ell_obj *name_str = ell_sym_name(sym);
    return ell_intern(ell_str_poplast(name_str));
}

static struct ellc_args *
ellc_make_args()
{
    struct ellc_args *args = (struct ellc_args *) ell_alloc(sizeof(*args));
    list_init(&args->pos, LISTCOUNT_T_MAX);
    dict_init(&args->key, DICTCOUNT_T_MAX, (dict_comp_t) &ell_sym_cmp);
    return args;
}

static struct ellc_args *
ellc_dissect_args(list_t *args_stx)
{
    struct ellc_args *args = ellc_make_args();
    for (lnode_t *n = list_first(args_stx); n; n = list_next(args_stx, n)) {
        struct ell_obj *arg_stx = lnode_get(n);
        if ((arg_stx->brand == ELL_BRAND(stx_sym)) &&
            ellc_is_key_arg_sym(ell_stx_sym_sym(arg_stx)))
        {
            n = list_next(args_stx, n);
            if (!n) {
                printf("missing value for keyword argument\n");
                exit(EXIT_FAILURE);
            }
            struct ell_obj *key_arg_sym =
                ellc_clean_key_arg_sym(ell_stx_sym_sym(arg_stx));
            struct ellc_ast *key_arg_val_ast =
                ellc_norm_stx((struct ell_obj *) lnode_get(n));
            ell_util_dict_put(&args->key, key_arg_sym, key_arg_val_ast);
        } else {
            ell_util_list_add(&args->pos, ellc_norm_stx(arg_stx));
        }
    }
    return args;
}

static struct ellc_ast *
ellc_make_app(struct ellc_ast *op, list_t *arg_stx_lst)
{
    struct ellc_ast *ast = ellc_make_ast(ELLC_AST_APP);
    ast->app.op = op;
    ast->app.args = ellc_dissect_args(arg_stx_lst);
    return ast;
}

static struct ellc_ast *
ellc_norm_app(struct ell_obj *stx_lst)
{
    ell_assert_stx_lst_len_min(stx_lst, 2);
    return ellc_make_app(ellc_norm_stx(ELL_SEND(stx_lst, second)),
                         ell_util_sublist(ell_stx_lst_elts(stx_lst), 2));
}

static struct ellc_ast *
ellc_norm_ordinary_app(struct ell_obj *stx_lst)
{
    ell_assert_stx_lst_len_min(stx_lst, 1);
    struct ell_obj *op_sym_stx = ELL_SEND(stx_lst, first);
    ell_assert_brand(op_sym_stx, ELL_BRAND(stx_sym));
    struct ellc_ast *op_ast = ellc_make_ast(ELLC_AST_REF);
    op_ast->ref.id = ellc_make_id(ell_stx_sym_sym(op_sym_stx), ELLC_NS_FUN);
    return ellc_make_app(op_ast,
                         ell_util_sublist(ell_stx_lst_elts(stx_lst), 1));
}

/* (Abstraction and Parameters Dissection) */

static struct ellc_param *
ellc_dissect_param(struct ell_obj *p_stx)
{
    struct ellc_param *p = (struct ellc_param *) ell_alloc(sizeof(*p));
    if (p_stx->brand == ELL_BRAND(stx_sym)) {
        p->id = ellc_make_id(ell_stx_sym_sym(p_stx), ELLC_NS_VAR);
    } else if (p_stx->brand == ELL_BRAND(stx_lst)) {
        ell_assert_stx_lst_len(p_stx, 2);
        struct ell_obj *name_stx = ELL_SEND(p_stx, first);
        struct ell_obj *init_stx = ELL_SEND(p_stx, second);
        p->id = ellc_make_id(ell_stx_sym_sym(name_stx), ELLC_NS_VAR);
        p->init = ellc_norm_stx(init_stx);
    }
    return p;
}

static struct ellc_params *
ellc_dissect_params(list_t *params_stx)
{
    struct ellc_params *params =
        (struct ellc_params *) ell_alloc(sizeof(*params));

    list_t *req = ell_util_make_list();
    list_t *opt = ell_util_make_list();
    list_t *key = ell_util_make_list();
    list_t *rest = ell_util_make_list();
    list_t *all_keys = ell_util_make_list();

    list_t *cur = req;
    for (lnode_t *n = list_first(params_stx); n; n = list_next(params_stx, n)) {
        struct ell_obj *p_stx = lnode_get(n);
        if (p_stx->brand == ELL_BRAND(stx_sym)) {
            struct ell_obj *p_sym = ell_stx_sym_sym(p_stx);
            if (p_sym == ELL_SYM(param_optional)) {
                cur = opt;
                continue;
            } else if (p_sym == ELL_SYM(param_key)) {
                cur = key;
                continue;
            } else if (p_sym == ELL_SYM(param_rest)) {
                cur = rest;
                continue;
            } else if (p_sym == ELL_SYM(param_all_keys)) {
                cur = all_keys;
                continue;
            }
        }
        ell_util_list_add(cur, ellc_dissect_param(p_stx));
    }

    if ((list_count(rest) > 1) || (list_count(all_keys) > 1)) {
        printf("more than one rest or all-keys parameter\n");
        exit(EXIT_FAILURE);
    }

    params->req = req;
    params->opt = opt;
    params->key = key;
    if (list_count(rest) == 1)
        params->rest = lnode_get(list_first(rest));
    if (list_count(all_keys) == 1)
        params->all_keys = lnode_get(list_first(all_keys));

    return params;
}

static struct ellc_ast *
ellc_norm_lam(struct ell_obj *stx_lst)
{
    ell_assert_stx_lst_len(stx_lst, 3);
    struct ell_obj *params_stx = ELL_SEND(stx_lst, second);
    ell_assert_brand(params_stx, ELL_BRAND(stx_lst));
    struct ellc_ast *ast = ellc_make_ast(ELLC_AST_LAM);
    ast->lam.params = ellc_dissect_params(ell_stx_lst_elts(params_stx));
    ast->lam.body = ellc_norm_stx(ELL_SEND(stx_lst, third));
    return ast;
}

/* (Putting it All Together) */

static struct dict_t ellc_norm_tab;

__attribute__((constructor(301))) static void
ellc_init_norm_tab()
{
    dict_init(&ellc_norm_tab, DICTCOUNT_T_MAX, (dict_comp_t) &ell_sym_cmp);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_fref), &ellc_norm_fref);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_def), &ellc_norm_def);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_fdef), &ellc_norm_fdef);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_set), &ellc_norm_set);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_fset), &ellc_norm_fset);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_cond), &ellc_norm_cond);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_seq), &ellc_norm_seq);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_app), &ellc_norm_app);
    ell_util_dict_put(&ellc_norm_tab, ELL_SYM(core_lam), &ellc_norm_lam);
}

static struct ellc_ast *
ellc_norm_stx_lst(struct ell_obj *stx_lst)
{
    ell_assert_brand(stx_lst, ELL_BRAND(stx_lst));
    struct ell_obj *op_stx = ELL_SEND(stx_lst, first);
    ell_assert_brand(op_stx, ELL_BRAND(stx_sym));
    struct ell_obj *op_sym = ell_stx_sym_sym(op_stx);
    dnode_t *node = dict_lookup(&ellc_norm_tab, op_sym);
    if (node) {
        ellc_norm_fun *norm_fun = (ellc_norm_fun *) dnode_get(node);
        return norm_fun(stx_lst);
    } else {
        return ellc_norm_ordinary_app(stx_lst);
    }
}

static struct ellc_ast *
ellc_norm_stx(struct ell_obj *stx)
{
    if (stx->brand == ELL_BRAND(stx_sym)) {
        return ellc_norm_ref(stx);
    } else if (stx->brand == ELL_BRAND(stx_lst)) {
        return ellc_norm_stx_lst(stx);
    } else {
        printf("syntax normalization failure\n");
        exit(EXIT_FAILURE);
    }
}

static void
ellc_norm_process(list_t *list, lnode_t *node, void *ast_seq_arg)
{
    struct ellc_ast_seq *ast_seq = (struct ellc_ast_seq *) ast_seq_arg;
    struct ell_obj *stx = (struct ell_obj *) lnode_get(node);
    ellc_ast_seq_add(ast_seq, ellc_norm_stx(stx));
}

static struct ellc_ast_seq *
ellc_norm(struct ell_obj *stx_lst)
{
    ell_assert_brand(stx_lst, ELL_BRAND(stx_lst));
    struct ellc_ast_seq *ast_seq = ellc_make_ast_seq();
    list_process(ell_stx_lst_elts(stx_lst), ast_seq, &ellc_norm_process);
    return ast_seq;
}

/**** Emission: Produces "C" ****/

/**** Main ****/

int main()
{
    struct ell_obj *stx_lst = ellc_parse();
    struct ellc_ast_seq *norm_form = ellc_norm(stx_lst);
    ELL_SEND(stx_lst, print_object);
}