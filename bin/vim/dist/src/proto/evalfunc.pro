/* evalfunc.c */
char_u *get_function_name(expand_T *xp, int idx);
char_u *get_expr_name(expand_T *xp, int idx);
int find_internal_func(char_u *name);
int has_internal_func(char_u *name);
char *internal_func_name(int idx);
int internal_func_check_arg_types(type_T **types, int idx, int argcount, cctx_T *cctx);
type_T *internal_func_ret_type(int idx, int argcount, type_T **argtypes);
int internal_func_is_map(int idx);
int check_internal_func(int idx, int argcount);
int call_internal_func(char_u *name, int argcount, typval_T *argvars, typval_T *rettv);
void call_internal_func_by_idx(int idx, typval_T *argvars, typval_T *rettv);
int call_internal_method(char_u *name, int argcount, typval_T *argvars, typval_T *rettv, typval_T *basetv);
int non_zero_arg(typval_T *argvars);
buf_T *get_buf_arg(typval_T *arg);
win_T *get_optional_window(typval_T *argvars, int idx);
void execute_redir_str(char_u *value, int value_len);
void execute_common(typval_T *argvars, typval_T *rettv, int arg_off);
void f_has(typval_T *argvars, typval_T *rettv);
int dynamic_feature(char_u *feature);
void mzscheme_call_vim(char_u *name, typval_T *args, typval_T *rettv);
void range_list_materialize(list_T *list);
float_T vim_round(float_T f);
long do_searchpair(char_u *spat, char_u *mpat, char_u *epat, int dir, typval_T *skip, int flags, pos_T *match_pos, linenr_T lnum_stop, long time_limit);
void f_string(typval_T *argvars, typval_T *rettv);
void f_fullcommand(typval_T *argvars, typval_T *rettv);
/* vim: set ft=c : */
