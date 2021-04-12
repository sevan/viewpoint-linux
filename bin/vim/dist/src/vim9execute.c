/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * vim9execute.c: execute Vim9 script instructions
 */

#define USING_FLOAT_STUFF
#include "vim.h"

#if defined(FEAT_EVAL) || defined(PROTO)

#ifdef VMS
# include <float.h>
#endif

#include "vim9.h"

// Structure put on ec_trystack when ISN_TRY is encountered.
typedef struct {
    int	    tcd_frame_idx;	// ec_frame_idx at ISN_TRY
    int	    tcd_stack_len;	// size of ectx.ec_stack at ISN_TRY
    int	    tcd_catch_idx;	// instruction of the first :catch or :finally
    int	    tcd_finally_idx;	// instruction of the :finally block or zero
    int	    tcd_endtry_idx;	// instruction of the :endtry
    int	    tcd_caught;		// catch block entered
    int	    tcd_cont;		// :continue encountered, jump here (minus one)
    int	    tcd_return;		// when TRUE return from end of :finally
} trycmd_T;


// A stack is used to store:
// - arguments passed to a :def function
// - info about the calling function, to use when returning
// - local variables
// - temporary values
//
// In detail (FP == Frame Pointer):
//	  arg1		first argument from caller (if present)
//	  arg2		second argument from caller (if present)
//	  extra_arg1	any missing optional argument default value
// FP ->  cur_func	calling function
//        current	previous instruction pointer
//        frame_ptr	previous Frame Pointer
//        var1		space for local variable
//        var2		space for local variable
//        ....		fixed space for max. number of local variables
//        temp		temporary values
//        ....		flexible space for temporary values (can grow big)

/*
 * Execution context.
 */
struct ectx_S {
    garray_T	ec_stack;	// stack of typval_T values
    int		ec_frame_idx;	// index in ec_stack: context of ec_dfunc_idx

    outer_T	*ec_outer;	// outer scope used for closures, allocated

    garray_T	ec_trystack;	// stack of trycmd_T values
    int		ec_in_catch;	// when TRUE in catch or finally block

    int		ec_dfunc_idx;	// current function index
    isn_T	*ec_instr;	// array with instructions
    int		ec_iidx;	// index in ec_instr: instruction to execute

    garray_T	ec_funcrefs;	// partials that might be a closure
};

#ifdef FEAT_PROFILE
// stack of profinfo_T used when profiling.
static garray_T profile_info_ga = {0, 0, sizeof(profinfo_T), 20, NULL};
#endif

// Get pointer to item relative to the bottom of the stack, -1 is the last one.
#define STACK_TV_BOT(idx) (((typval_T *)ectx->ec_stack.ga_data) + ectx->ec_stack.ga_len + (idx))

    void
to_string_error(vartype_T vartype)
{
    semsg(_(e_cannot_convert_str_to_string), vartype_name(vartype));
}

/*
 * Return the number of arguments, including optional arguments and any vararg.
 */
    static int
ufunc_argcount(ufunc_T *ufunc)
{
    return ufunc->uf_args.ga_len + (ufunc->uf_va_name != NULL ? 1 : 0);
}

/*
 * Create a new list from "count" items at the bottom of the stack.
 * When "count" is zero an empty list is added to the stack.
 */
    static int
exe_newlist(int count, ectx_T *ectx)
{
    list_T	*list = list_alloc_with_items(count);
    int		idx;
    typval_T	*tv;

    if (list == NULL)
	return FAIL;
    for (idx = 0; idx < count; ++idx)
	list_set_item(list, idx, STACK_TV_BOT(idx - count));

    if (count > 0)
	ectx->ec_stack.ga_len -= count - 1;
    else if (GA_GROW(&ectx->ec_stack, 1) == FAIL)
	return FAIL;
    else
	++ectx->ec_stack.ga_len;
    tv = STACK_TV_BOT(-1);
    tv->v_type = VAR_LIST;
    tv->vval.v_list = list;
    ++list->lv_refcount;
    return OK;
}

// Data local to a function.
// On a function call, if not empty, is saved on the stack and restored when
// returning.
typedef struct {
    int		floc_restore_cmdmod;
    cmdmod_T	floc_save_cmdmod;
    int		floc_restore_cmdmod_stacklen;
} funclocal_T;

/*
 * Call compiled function "cdf_idx" from compiled code.
 * This adds a stack frame and sets the instruction pointer to the start of the
 * called function.
 * If "pt" is not null use "pt->pt_outer" for ec_outer.
 *
 * Stack has:
 * - current arguments (already there)
 * - omitted optional argument (default values) added here
 * - stack frame:
 *	- pointer to calling function
 *	- Index of next instruction in calling function
 *	- previous frame pointer
 * - reserved space for local variables
 */
    static int
call_dfunc(
	int		cdf_idx,
	partial_T	*pt,
	int		argcount_arg,
	funclocal_T	*funclocal,
	ectx_T		*ectx)
{
    int		argcount = argcount_arg;
    dfunc_T	*dfunc = ((dfunc_T *)def_functions.ga_data) + cdf_idx;
    ufunc_T	*ufunc = dfunc->df_ufunc;
    int		arg_to_add;
    int		vararg_count = 0;
    int		varcount;
    int		idx;
    estack_T	*entry;
    funclocal_T	*floc = NULL;

    if (dfunc->df_deleted)
    {
	// don't use ufunc->uf_name, it may have been freed
	emsg_funcname(e_func_deleted,
		dfunc->df_name == NULL ? (char_u *)"unknown" : dfunc->df_name);
	return FAIL;
    }

#ifdef FEAT_PROFILE
    if (do_profiling == PROF_YES)
    {
	if (ga_grow(&profile_info_ga, 1) == OK)
	{
	    profinfo_T *info = ((profinfo_T *)profile_info_ga.ga_data)
						      + profile_info_ga.ga_len;
	    ++profile_info_ga.ga_len;
	    CLEAR_POINTER(info);
	    profile_may_start_func(info, ufunc,
			(((dfunc_T *)def_functions.ga_data)
					      + ectx->ec_dfunc_idx)->df_ufunc);
	}

	// Profiling might be enabled/disabled along the way.  This should not
	// fail, since the function was compiled before and toggling profiling
	// doesn't change any errors.
	if (func_needs_compiling(ufunc, PROFILING(ufunc))
		&& compile_def_function(ufunc, FALSE, PROFILING(ufunc), NULL)
								       == FAIL)
	    return FAIL;
    }
#endif

    if (ufunc->uf_va_name != NULL)
    {
	// Need to make a list out of the vararg arguments.
	// Stack at time of call with 2 varargs:
	//   normal_arg
	//   optional_arg
	//   vararg_1
	//   vararg_2
	// After creating the list:
	//   normal_arg
	//   optional_arg
	//   vararg-list
	// With missing optional arguments we get:
	//    normal_arg
	// After creating the list
	//    normal_arg
	//    (space for optional_arg)
	//    vararg-list
	vararg_count = argcount - ufunc->uf_args.ga_len;
	if (vararg_count < 0)
	    vararg_count = 0;
	else
	    argcount -= vararg_count;
	if (exe_newlist(vararg_count, ectx) == FAIL)
	    return FAIL;

	vararg_count = 1;
    }

    arg_to_add = ufunc->uf_args.ga_len - argcount;
    if (arg_to_add < 0)
    {
	if (arg_to_add == -1)
	    emsg(_(e_one_argument_too_many));
	else
	    semsg(_(e_nr_arguments_too_many), -arg_to_add);
	return FAIL;
    }

    // Reserve space for:
    // - missing arguments
    // - stack frame
    // - local variables
    // - if needed: a counter for number of closures created in
    //   ectx->ec_funcrefs.
    varcount = dfunc->df_varcount + dfunc->df_has_closure;
    if (ga_grow(&ectx->ec_stack, arg_to_add + STACK_FRAME_SIZE + varcount)
								       == FAIL)
	return FAIL;

    // If depth of calling is getting too high, don't execute the function.
    if (funcdepth_increment() == FAIL)
	return FAIL;

    // Only make a copy of funclocal if it contains something to restore.
    if (funclocal->floc_restore_cmdmod)
    {
	floc = ALLOC_ONE(funclocal_T);
	if (floc == NULL)
	    return FAIL;
	*floc = *funclocal;
	funclocal->floc_restore_cmdmod = FALSE;
    }

    // Move the vararg-list to below the missing optional arguments.
    if (vararg_count > 0 && arg_to_add > 0)
	*STACK_TV_BOT(arg_to_add - 1) = *STACK_TV_BOT(-1);

    // Reserve space for omitted optional arguments, filled in soon.
    for (idx = 0; idx < arg_to_add; ++idx)
	STACK_TV_BOT(idx - vararg_count)->v_type = VAR_UNKNOWN;
    ectx->ec_stack.ga_len += arg_to_add;

    // Store current execution state in stack frame for ISN_RETURN.
    STACK_TV_BOT(STACK_FRAME_FUNC_OFF)->vval.v_number = ectx->ec_dfunc_idx;
    STACK_TV_BOT(STACK_FRAME_IIDX_OFF)->vval.v_number = ectx->ec_iidx;
    STACK_TV_BOT(STACK_FRAME_OUTER_OFF)->vval.v_string = (void *)ectx->ec_outer;
    STACK_TV_BOT(STACK_FRAME_FUNCLOCAL_OFF)->vval.v_string = (void *)floc;
    STACK_TV_BOT(STACK_FRAME_IDX_OFF)->vval.v_number = ectx->ec_frame_idx;
    ectx->ec_frame_idx = ectx->ec_stack.ga_len;

    // Initialize local variables
    for (idx = 0; idx < dfunc->df_varcount; ++idx)
	STACK_TV_BOT(STACK_FRAME_SIZE + idx)->v_type = VAR_UNKNOWN;
    if (dfunc->df_has_closure)
    {
	typval_T *tv = STACK_TV_BOT(STACK_FRAME_SIZE + dfunc->df_varcount);

	tv->v_type = VAR_NUMBER;
	tv->vval.v_number = 0;
    }
    ectx->ec_stack.ga_len += STACK_FRAME_SIZE + varcount;

    if (pt != NULL || ufunc->uf_partial != NULL
					     || (ufunc->uf_flags & FC_CLOSURE))
    {
	outer_T *outer = ALLOC_CLEAR_ONE(outer_T);

	if (outer == NULL)
	    return FAIL;
	if (pt != NULL)
	{
	    *outer = pt->pt_outer;
	    outer->out_up_is_copy = TRUE;
	}
	else if (ufunc->uf_partial != NULL)
	{
	    *outer = ufunc->uf_partial->pt_outer;
	    outer->out_up_is_copy = TRUE;
	}
	else
	{
	    outer->out_stack = &ectx->ec_stack;
	    outer->out_frame_idx = ectx->ec_frame_idx;
	    outer->out_up = ectx->ec_outer;
	}
	ectx->ec_outer = outer;
    }
    else
	ectx->ec_outer = NULL;

    ++ufunc->uf_calls;

    // Set execution state to the start of the called function.
    ectx->ec_dfunc_idx = cdf_idx;
    ectx->ec_instr = INSTRUCTIONS(dfunc);
    entry = estack_push_ufunc(ufunc, 1);
    if (entry != NULL)
    {
	// Set the script context to the script where the function was defined.
	// TODO: save more than the SID?
	entry->es_save_sid = current_sctx.sc_sid;
	current_sctx.sc_sid = ufunc->uf_script_ctx.sc_sid;
    }

    // Start execution at the first instruction.
    ectx->ec_iidx = 0;

    return OK;
}

// Get pointer to item in the stack.
#define STACK_TV(idx) (((typval_T *)ectx->ec_stack.ga_data) + idx)

/*
 * Used when returning from a function: Check if any closure is still
 * referenced.  If so then move the arguments and variables to a separate piece
 * of stack to be used when the closure is called.
 * When "free_arguments" is TRUE the arguments are to be freed.
 * Returns FAIL when out of memory.
 */
    static int
handle_closure_in_use(ectx_T *ectx, int free_arguments)
{
    dfunc_T	*dfunc = ((dfunc_T *)def_functions.ga_data)
							  + ectx->ec_dfunc_idx;
    int		argcount;
    int		top;
    int		idx;
    typval_T	*tv;
    int		closure_in_use = FALSE;
    garray_T	*gap = &ectx->ec_funcrefs;
    varnumber_T	closure_count;

    if (dfunc->df_ufunc == NULL)
	return OK;  // function was freed
    if (dfunc->df_has_closure == 0)
	return OK;  // no closures
    tv = STACK_TV(ectx->ec_frame_idx + STACK_FRAME_SIZE + dfunc->df_varcount);
    closure_count = tv->vval.v_number;
    if (closure_count == 0)
	return OK;  // no funcrefs created

    argcount = ufunc_argcount(dfunc->df_ufunc);
    top = ectx->ec_frame_idx - argcount;

    // Check if any created closure is still in use.
    for (idx = 0; idx < closure_count; ++idx)
    {
	partial_T   *pt;
	int	    off = gap->ga_len - closure_count + idx;

	if (off < 0)
	    continue;  // count is off or already done
	pt = ((partial_T **)gap->ga_data)[off];
	if (pt->pt_refcount > 1)
	{
	    int refcount = pt->pt_refcount;
	    int i;

	    // A Reference in a local variables doesn't count, it gets
	    // unreferenced on return.
	    for (i = 0; i < dfunc->df_varcount; ++i)
	    {
		typval_T *stv = STACK_TV(ectx->ec_frame_idx
						       + STACK_FRAME_SIZE + i);
		if (stv->v_type == VAR_PARTIAL && pt == stv->vval.v_partial)
		    --refcount;
	    }
	    if (refcount > 1)
	    {
		closure_in_use = TRUE;
		break;
	    }
	}
    }

    if (closure_in_use)
    {
	funcstack_T *funcstack = ALLOC_CLEAR_ONE(funcstack_T);
	typval_T    *stack;

	// A closure is using the arguments and/or local variables.
	// Move them to the called function.
	if (funcstack == NULL)
	    return FAIL;
	funcstack->fs_var_offset = argcount + STACK_FRAME_SIZE;
	funcstack->fs_ga.ga_len = funcstack->fs_var_offset + dfunc->df_varcount;
	stack = ALLOC_CLEAR_MULT(typval_T, funcstack->fs_ga.ga_len);
	funcstack->fs_ga.ga_data = stack;
	if (stack == NULL)
	{
	    vim_free(funcstack);
	    return FAIL;
	}

	// Move or copy the arguments.
	for (idx = 0; idx < argcount; ++idx)
	{
	    tv = STACK_TV(top + idx);
	    if (free_arguments)
	    {
		*(stack + idx) = *tv;
		tv->v_type = VAR_UNKNOWN;
	    }
	    else
		copy_tv(tv, stack + idx);
	}
	// Move the local variables.
	for (idx = 0; idx < dfunc->df_varcount; ++idx)
	{
	    tv = STACK_TV(ectx->ec_frame_idx + STACK_FRAME_SIZE + idx);

	    // A partial created for a local function, that is also used as a
	    // local variable, has a reference count for the variable, thus
	    // will never go down to zero.  When all these refcounts are one
	    // then the funcstack is unused.  We need to count how many we have
	    // so we need when to check.
	    if (tv->v_type == VAR_PARTIAL && tv->vval.v_partial != NULL)
	    {
		int	    i;

		for (i = 0; i < closure_count; ++i)
		    if (tv->vval.v_partial == ((partial_T **)gap->ga_data)[
					      gap->ga_len - closure_count + i])
			++funcstack->fs_min_refcount;
	    }

	    *(stack + funcstack->fs_var_offset + idx) = *tv;
	    tv->v_type = VAR_UNKNOWN;
	}

	for (idx = 0; idx < closure_count; ++idx)
	{
	    partial_T *pt = ((partial_T **)gap->ga_data)[gap->ga_len
							- closure_count + idx];
	    if (pt->pt_refcount > 1)
	    {
		++funcstack->fs_refcount;
		pt->pt_funcstack = funcstack;
		pt->pt_outer.out_stack = &funcstack->fs_ga;
		pt->pt_outer.out_frame_idx = ectx->ec_frame_idx - top;
		pt->pt_outer.out_up = ectx->ec_outer;
	    }
	}
    }

    for (idx = 0; idx < closure_count; ++idx)
	partial_unref(((partial_T **)gap->ga_data)[gap->ga_len
						       - closure_count + idx]);
    gap->ga_len -= closure_count;
    if (gap->ga_len == 0)
	ga_clear(gap);

    return OK;
}

/*
 * Called when a partial is freed or its reference count goes down to one.  The
 * funcstack may be the only reference to the partials in the local variables.
 * Go over all of them, the funcref and can be freed if all partials
 * referencing the funcstack have a reference count of one.
 */
    void
funcstack_check_refcount(funcstack_T *funcstack)
{
    int		    i;
    garray_T	    *gap = &funcstack->fs_ga;
    int		    done = 0;

    if (funcstack->fs_refcount > funcstack->fs_min_refcount)
	return;
    for (i = funcstack->fs_var_offset; i < gap->ga_len; ++i)
    {
	typval_T *tv = ((typval_T *)gap->ga_data) + i;

	if (tv->v_type == VAR_PARTIAL && tv->vval.v_partial != NULL
		&& tv->vval.v_partial->pt_funcstack == funcstack
		&& tv->vval.v_partial->pt_refcount == 1)
	    ++done;
    }
    if (done == funcstack->fs_min_refcount)
    {
	typval_T	*stack = gap->ga_data;

	// All partials referencing the funcstack have a reference count of
	// one, thus the funcstack is no longer of use.
	for (i = 0; i < gap->ga_len; ++i)
	    clear_tv(stack + i);
	vim_free(stack);
	vim_free(funcstack);
    }
}

/*
 * Return from the current function.
 */
    static int
func_return(funclocal_T *funclocal, ectx_T *ectx)
{
    int		idx;
    int		ret_idx;
    dfunc_T	*dfunc = ((dfunc_T *)def_functions.ga_data)
							  + ectx->ec_dfunc_idx;
    int		argcount = ufunc_argcount(dfunc->df_ufunc);
    int		top = ectx->ec_frame_idx - argcount;
    estack_T	*entry;
    int		prev_dfunc_idx = STACK_TV(ectx->ec_frame_idx
					+ STACK_FRAME_FUNC_OFF)->vval.v_number;
    dfunc_T	*prev_dfunc = ((dfunc_T *)def_functions.ga_data)
							      + prev_dfunc_idx;
    funclocal_T	*floc;

#ifdef FEAT_PROFILE
    if (do_profiling == PROF_YES)
    {
	ufunc_T *caller = prev_dfunc->df_ufunc;

	if (dfunc->df_ufunc->uf_profiling
				   || (caller != NULL && caller->uf_profiling))
	{
	    profile_may_end_func(((profinfo_T *)profile_info_ga.ga_data)
			+ profile_info_ga.ga_len - 1, dfunc->df_ufunc, caller);
	    --profile_info_ga.ga_len;
	}
    }
#endif
    // TODO: when is it safe to delete the function when it is no longer used?
    --dfunc->df_ufunc->uf_calls;

    // execution context goes one level up
    entry = estack_pop();
    if (entry != NULL)
	current_sctx.sc_sid = entry->es_save_sid;

    if (handle_closure_in_use(ectx, TRUE) == FAIL)
	return FAIL;

    // Clear the arguments.
    for (idx = top; idx < ectx->ec_frame_idx; ++idx)
	clear_tv(STACK_TV(idx));

    // Clear local variables and temp values, but not the return value.
    for (idx = ectx->ec_frame_idx + STACK_FRAME_SIZE;
					idx < ectx->ec_stack.ga_len - 1; ++idx)
	clear_tv(STACK_TV(idx));

    // The return value should be on top of the stack.  However, when aborting
    // it may not be there and ec_frame_idx is the top of the stack.
    ret_idx = ectx->ec_stack.ga_len - 1;
    if (ret_idx == ectx->ec_frame_idx + STACK_FRAME_IDX_OFF)
	ret_idx = 0;

    vim_free(ectx->ec_outer);

    // Restore the previous frame.
    ectx->ec_dfunc_idx = prev_dfunc_idx;
    ectx->ec_iidx = STACK_TV(ectx->ec_frame_idx
					+ STACK_FRAME_IIDX_OFF)->vval.v_number;
    ectx->ec_outer = (void *)STACK_TV(ectx->ec_frame_idx
				       + STACK_FRAME_OUTER_OFF)->vval.v_string;
    floc = (void *)STACK_TV(ectx->ec_frame_idx
				   + STACK_FRAME_FUNCLOCAL_OFF)->vval.v_string;
    // restoring ec_frame_idx must be last
    ectx->ec_frame_idx = STACK_TV(ectx->ec_frame_idx
				       + STACK_FRAME_IDX_OFF)->vval.v_number;
    ectx->ec_instr = INSTRUCTIONS(prev_dfunc);

    if (floc == NULL)
	funclocal->floc_restore_cmdmod = FALSE;
    else
    {
	*funclocal = *floc;
	vim_free(floc);
    }

    if (ret_idx > 0)
    {
	// Reset the stack to the position before the call, with a spot for the
	// return value, moved there from above the frame.
	ectx->ec_stack.ga_len = top + 1;
	*STACK_TV_BOT(-1) = *STACK_TV(ret_idx);
    }
    else
	// Reset the stack to the position before the call.
	ectx->ec_stack.ga_len = top;

    funcdepth_decrement();
    return OK;
}

#undef STACK_TV

/*
 * Prepare arguments and rettv for calling a builtin or user function.
 */
    static int
call_prepare(int argcount, typval_T *argvars, ectx_T *ectx)
{
    int		idx;
    typval_T	*tv;

    // Move arguments from bottom of the stack to argvars[] and add terminator.
    for (idx = 0; idx < argcount; ++idx)
	argvars[idx] = *STACK_TV_BOT(idx - argcount);
    argvars[argcount].v_type = VAR_UNKNOWN;

    // Result replaces the arguments on the stack.
    if (argcount > 0)
	ectx->ec_stack.ga_len -= argcount - 1;
    else if (GA_GROW(&ectx->ec_stack, 1) == FAIL)
	return FAIL;
    else
	++ectx->ec_stack.ga_len;

    // Default return value is zero.
    tv = STACK_TV_BOT(-1);
    tv->v_type = VAR_NUMBER;
    tv->vval.v_number = 0;

    return OK;
}

// Ugly global to avoid passing the execution context around through many
// layers.
static ectx_T *current_ectx = NULL;

/*
 * Call a builtin function by index.
 */
    static int
call_bfunc(int func_idx, int argcount, ectx_T *ectx)
{
    typval_T	argvars[MAX_FUNC_ARGS];
    int		idx;
    int		did_emsg_before = did_emsg;
    ectx_T	*prev_ectx = current_ectx;

    if (call_prepare(argcount, argvars, ectx) == FAIL)
	return FAIL;

    // Call the builtin function.  Set "current_ectx" so that when it
    // recursively invokes call_def_function() a closure context can be set.
    current_ectx = ectx;
    call_internal_func_by_idx(func_idx, argvars, STACK_TV_BOT(-1));
    current_ectx = prev_ectx;

    // Clear the arguments.
    for (idx = 0; idx < argcount; ++idx)
	clear_tv(&argvars[idx]);

    if (did_emsg > did_emsg_before)
	return FAIL;
    return OK;
}

/*
 * Execute a user defined function.
 * If the function is compiled this will add a stack frame and set the
 * instruction pointer at the start of the function.
 * Otherwise the function is called here.
 * If "pt" is not null use "pt->pt_outer" for ec_outer.
 * "iptr" can be used to replace the instruction with a more efficient one.
 */
    static int
call_ufunc(
	ufunc_T	    *ufunc,
	partial_T   *pt,
	int	    argcount,
	funclocal_T *funclocal,
	ectx_T	    *ectx,
	isn_T	    *iptr)
{
    typval_T	argvars[MAX_FUNC_ARGS];
    funcexe_T   funcexe;
    int		error;
    int		idx;
    int		did_emsg_before = did_emsg;
#ifdef FEAT_PROFILE
    int		profiling = do_profiling == PROF_YES && ufunc->uf_profiling;
#else
# define profiling FALSE
#endif

    if (func_needs_compiling(ufunc, profiling)
		&& compile_def_function(ufunc, FALSE, profiling, NULL) == FAIL)
	return FAIL;
    if (ufunc->uf_def_status == UF_COMPILED)
    {
	error = check_user_func_argcount(ufunc, argcount);
	if (error != FCERR_UNKNOWN)
	{
	    if (error == FCERR_TOOMANY)
		semsg(_(e_toomanyarg), ufunc->uf_name);
	    else
		semsg(_(e_toofewarg), ufunc->uf_name);
	    return FAIL;
	}

	// The function has been compiled, can call it quickly.  For a function
	// that was defined later: we can call it directly next time.
	// TODO: what if the function was deleted and then defined again?
	if (iptr != NULL)
	{
	    delete_instr(iptr);
	    iptr->isn_type = ISN_DCALL;
	    iptr->isn_arg.dfunc.cdf_idx = ufunc->uf_dfunc_idx;
	    iptr->isn_arg.dfunc.cdf_argcount = argcount;
	}
	return call_dfunc(ufunc->uf_dfunc_idx, pt, argcount, funclocal, ectx);
    }

    if (call_prepare(argcount, argvars, ectx) == FAIL)
	return FAIL;
    CLEAR_FIELD(funcexe);
    funcexe.evaluate = TRUE;

    // Call the user function.  Result goes in last position on the stack.
    // TODO: add selfdict if there is one
    error = call_user_func_check(ufunc, argcount, argvars,
					     STACK_TV_BOT(-1), &funcexe, NULL);

    // Clear the arguments.
    for (idx = 0; idx < argcount; ++idx)
	clear_tv(&argvars[idx]);

    if (error != FCERR_NONE)
    {
	user_func_error(error, ufunc->uf_name);
	return FAIL;
    }
    if (did_emsg > did_emsg_before)
	// Error other than from calling the function itself.
	return FAIL;
    return OK;
}

/*
 * If command modifiers were applied restore them.
 */
    static void
may_restore_cmdmod(funclocal_T *funclocal)
{
    if (funclocal->floc_restore_cmdmod)
    {
	cmdmod.cmod_filter_regmatch.regprog = NULL;
	undo_cmdmod(&cmdmod);
	cmdmod = funclocal->floc_save_cmdmod;
	funclocal->floc_restore_cmdmod = FALSE;
    }
}

/*
 * Return TRUE if an error was given or CTRL-C was pressed.
 */
    static int
vim9_aborting(int prev_called_emsg)
{
    return called_emsg > prev_called_emsg || got_int || did_throw;
}

/*
 * Execute a function by "name".
 * This can be a builtin function or a user function.
 * "iptr" can be used to replace the instruction with a more efficient one.
 * Returns FAIL if not found without an error message.
 */
    static int
call_by_name(
	char_u	    *name,
	int	    argcount,
	funclocal_T *funclocal,
	ectx_T	    *ectx,
	isn_T	    *iptr)
{
    ufunc_T *ufunc;

    if (builtin_function(name, -1))
    {
	int func_idx = find_internal_func(name);

	if (func_idx < 0)
	    return FAIL;
	if (check_internal_func(func_idx, argcount) < 0)
	    return FAIL;
	return call_bfunc(func_idx, argcount, ectx);
    }

    ufunc = find_func(name, FALSE, NULL);

    if (ufunc == NULL)
    {
	int called_emsg_before = called_emsg;

	if (script_autoload(name, TRUE))
	    // loaded a package, search for the function again
	    ufunc = find_func(name, FALSE, NULL);
	if (vim9_aborting(called_emsg_before))
	    return FAIL;  // bail out if loading the script caused an error
    }

    if (ufunc != NULL)
    {
	if (ufunc->uf_arg_types != NULL)
	{
	    int i;
	    typval_T	*argv = STACK_TV_BOT(0) - argcount;

	    // The function can change at runtime, check that the argument
	    // types are correct.
	    for (i = 0; i < argcount; ++i)
	    {
		type_T *type = NULL;

		if (i < ufunc->uf_args.ga_len)
		    type = ufunc->uf_arg_types[i];
		else if (ufunc->uf_va_type != NULL)
		    type = ufunc->uf_va_type->tt_member;
		if (type != NULL && check_typval_arg_type(type,
						      &argv[i], i + 1) == FAIL)
		    return FAIL;
	    }
	}

	return call_ufunc(ufunc, NULL, argcount, funclocal, ectx, iptr);
    }

    return FAIL;
}

    static int
call_partial(
	typval_T    *tv,
	int	    argcount_arg,
	funclocal_T *funclocal,
	ectx_T	    *ectx)
{
    int		argcount = argcount_arg;
    char_u	*name = NULL;
    int		called_emsg_before = called_emsg;
    int		res = FAIL;

    if (tv->v_type == VAR_PARTIAL)
    {
	partial_T   *pt = tv->vval.v_partial;
	int	    i;

	if (pt->pt_argc > 0)
	{
	    // Make space for arguments from the partial, shift the "argcount"
	    // arguments up.
	    if (ga_grow(&ectx->ec_stack, pt->pt_argc) == FAIL)
		return FAIL;
	    for (i = 1; i <= argcount; ++i)
		*STACK_TV_BOT(-i + pt->pt_argc) = *STACK_TV_BOT(-i);
	    ectx->ec_stack.ga_len += pt->pt_argc;
	    argcount += pt->pt_argc;

	    // copy the arguments from the partial onto the stack
	    for (i = 0; i < pt->pt_argc; ++i)
		copy_tv(&pt->pt_argv[i], STACK_TV_BOT(-argcount + i));
	}

	if (pt->pt_func != NULL)
	    return call_ufunc(pt->pt_func, pt, argcount, funclocal, ectx, NULL);

	name = pt->pt_name;
    }
    else if (tv->v_type == VAR_FUNC)
	name = tv->vval.v_string;
    if (name != NULL)
    {
	char_u	fname_buf[FLEN_FIXED + 1];
	char_u	*tofree = NULL;
	int	error = FCERR_NONE;
	char_u	*fname;

	// May need to translate <SNR>123_ to K_SNR.
	fname = fname_trans_sid(name, fname_buf, &tofree, &error);
	if (error != FCERR_NONE)
	    res = FAIL;
	else
	    res = call_by_name(fname, argcount, funclocal, ectx, NULL);
	vim_free(tofree);
    }

    if (res == FAIL)
    {
	if (called_emsg == called_emsg_before)
	    semsg(_(e_unknownfunc),
				  name == NULL ? (char_u *)"[unknown]" : name);
	return FAIL;
    }
    return OK;
}

/*
 * Check if "lock" is VAR_LOCKED or VAR_FIXED.  If so give an error and return
 * TRUE.
 */
    static int
error_if_locked(int lock, char *error)
{
    if (lock & (VAR_LOCKED | VAR_FIXED))
    {
	emsg(_(error));
	return TRUE;
    }
    return FALSE;
}

/*
 * Give an error if "tv" is not a number and return FAIL.
 */
    static int
check_for_number(typval_T *tv)
{
    if (tv->v_type != VAR_NUMBER)
    {
	semsg(_(e_expected_str_but_got_str),
		vartype_name(VAR_NUMBER), vartype_name(tv->v_type));
	return FAIL;
    }
    return OK;
}

/*
 * Store "tv" in variable "name".
 * This is for s: and g: variables.
 */
    static void
store_var(char_u *name, typval_T *tv)
{
    funccal_entry_T entry;
    int		    flags = ASSIGN_DECL;

    if (tv->v_lock)
	flags |= ASSIGN_CONST;
    save_funccal(&entry);
    set_var_const(name, NULL, tv, FALSE, flags, 0);
    restore_funccal();
}

/*
 * Convert "tv" to a string.
 * Return FAIL if not allowed.
 */
    static int
do_2string(typval_T *tv, int is_2string_any)
{
    if (tv->v_type != VAR_STRING)
    {
	char_u *str;

	if (is_2string_any)
	{
	    switch (tv->v_type)
	    {
		case VAR_SPECIAL:
		case VAR_BOOL:
		case VAR_NUMBER:
		case VAR_FLOAT:
		case VAR_BLOB:	break;
		default:	to_string_error(tv->v_type);
				return FAIL;
	    }
	}
	str = typval_tostring(tv, TRUE);
	clear_tv(tv);
	tv->v_type = VAR_STRING;
	tv->vval.v_string = str;
    }
    return OK;
}

/*
 * When the value of "sv" is a null list of dict, allocate it.
 */
    static void
allocate_if_null(typval_T *tv)
{
    switch (tv->v_type)
    {
	case VAR_LIST:
	    if (tv->vval.v_list == NULL)
		(void)rettv_list_alloc(tv);
	    break;
	case VAR_DICT:
	    if (tv->vval.v_dict == NULL)
		(void)rettv_dict_alloc(tv);
	    break;
	default:
	    break;
    }
}

/*
 * Return the character "str[index]" where "index" is the character index,
 * including composing characters.
 * If "index" is out of range NULL is returned.
 */
    char_u *
char_from_string(char_u *str, varnumber_T index)
{
    size_t	    nbyte = 0;
    varnumber_T	    nchar = index;
    size_t	    slen;

    if (str == NULL)
	return NULL;
    slen = STRLEN(str);

    // Do the same as for a list: a negative index counts from the end.
    // Optimization to check the first byte to be below 0x80 (and no composing
    // character follows) makes this a lot faster.
    if (index < 0)
    {
	int	clen = 0;

	for (nbyte = 0; nbyte < slen; ++clen)
	{
	    if (str[nbyte] < 0x80 && str[nbyte + 1] < 0x80)
		++nbyte;
	    else if (enc_utf8)
		nbyte += utfc_ptr2len(str + nbyte);
	    else
		nbyte += mb_ptr2len(str + nbyte);
	}
	nchar = clen + index;
	if (nchar < 0)
	    // unlike list: index out of range results in empty string
	    return NULL;
    }

    for (nbyte = 0; nchar > 0 && nbyte < slen; --nchar)
    {
	if (str[nbyte] < 0x80 && str[nbyte + 1] < 0x80)
	    ++nbyte;
	else if (enc_utf8)
	    nbyte += utfc_ptr2len(str + nbyte);
	else
	    nbyte += mb_ptr2len(str + nbyte);
    }
    if (nbyte >= slen)
	return NULL;
    return vim_strnsave(str + nbyte, mb_ptr2len(str + nbyte));
}

/*
 * Get the byte index for character index "idx" in string "str" with length
 * "str_len".  Composing characters are included.
 * If going over the end return "str_len".
 * If "idx" is negative count from the end, -1 is the last character.
 * When going over the start return -1.
 */
    static long
char_idx2byte(char_u *str, size_t str_len, varnumber_T idx)
{
    varnumber_T nchar = idx;
    size_t	nbyte = 0;

    if (nchar >= 0)
    {
	while (nchar > 0 && nbyte < str_len)
	{
	    nbyte += mb_ptr2len(str + nbyte);
	    --nchar;
	}
    }
    else
    {
	nbyte = str_len;
	while (nchar < 0 && nbyte > 0)
	{
	    --nbyte;
	    nbyte -= mb_head_off(str, str + nbyte);
	    ++nchar;
	}
	if (nchar < 0)
	    return -1;
    }
    return (long)nbyte;
}

/*
 * Return the slice "str[first : last]" using character indexes.  Composing
 * characters are included.
 * "exclusive" is TRUE for slice().
 * Return NULL when the result is empty.
 */
    char_u *
string_slice(char_u *str, varnumber_T first, varnumber_T last, int exclusive)
{
    long	start_byte, end_byte;
    size_t	slen;

    if (str == NULL)
	return NULL;
    slen = STRLEN(str);
    start_byte = char_idx2byte(str, slen, first);
    if (start_byte < 0)
	start_byte = 0; // first index very negative: use zero
    if ((last == -1 && !exclusive) || last == VARNUM_MAX)
	end_byte = (long)slen;
    else
    {
	end_byte = char_idx2byte(str, slen, last);
	if (!exclusive && end_byte >= 0 && end_byte < (long)slen)
	    // end index is inclusive
	    end_byte += mb_ptr2len(str + end_byte);
    }

    if (start_byte >= (long)slen || end_byte <= start_byte)
	return NULL;
    return vim_strnsave(str + start_byte, end_byte - start_byte);
}

    static svar_T *
get_script_svar(scriptref_T *sref, ectx_T *ectx)
{
    scriptitem_T    *si = SCRIPT_ITEM(sref->sref_sid);
    dfunc_T	    *dfunc = ((dfunc_T *)def_functions.ga_data)
							  + ectx->ec_dfunc_idx;
    svar_T	    *sv;

    if (sref->sref_seq != si->sn_script_seq)
    {
	// The script was reloaded after the function was
	// compiled, the script_idx may not be valid.
	semsg(_(e_script_variable_invalid_after_reload_in_function_str),
						 dfunc->df_ufunc->uf_name_exp);
	return NULL;
    }
    sv = ((svar_T *)si->sn_var_vals.ga_data) + sref->sref_idx;
    if (!equal_type(sv->sv_type, sref->sref_type))
    {
	emsg(_(e_script_variable_type_changed));
	return NULL;
    }
    return sv;
}

/*
 * Execute a function by "name".
 * This can be a builtin function, user function or a funcref.
 * "iptr" can be used to replace the instruction with a more efficient one.
 */
    static int
call_eval_func(
	char_u	    *name,
	int	    argcount,
	funclocal_T *funclocal,
	ectx_T	    *ectx,
	isn_T	    *iptr)
{
    int	    called_emsg_before = called_emsg;
    int	    res;

    res = call_by_name(name, argcount, funclocal, ectx, iptr);
    if (res == FAIL && called_emsg == called_emsg_before)
    {
	dictitem_T	*v;

	v = find_var(name, NULL, FALSE);
	if (v == NULL)
	{
	    semsg(_(e_unknownfunc), name);
	    return FAIL;
	}
	if (v->di_tv.v_type != VAR_PARTIAL && v->di_tv.v_type != VAR_FUNC)
	{
	    semsg(_(e_unknownfunc), name);
	    return FAIL;
	}
	return call_partial(&v->di_tv, argcount, funclocal, ectx);
    }
    return res;
}

/*
 * When a function reference is used, fill a partial with the information
 * needed, especially when it is used as a closure.
 */
    int
fill_partial_and_closure(partial_T *pt, ufunc_T *ufunc, ectx_T *ectx)
{
    pt->pt_func = ufunc;
    pt->pt_refcount = 1;

    if (ufunc->uf_flags & FC_CLOSURE)
    {
	dfunc_T	*dfunc = ((dfunc_T *)def_functions.ga_data)
							  + ectx->ec_dfunc_idx;

	// The closure needs to find arguments and local
	// variables in the current stack.
	pt->pt_outer.out_stack = &ectx->ec_stack;
	pt->pt_outer.out_frame_idx = ectx->ec_frame_idx;
	pt->pt_outer.out_up = ectx->ec_outer;
	pt->pt_outer.out_up_is_copy = TRUE;

	// If this function returns and the closure is still
	// being used, we need to make a copy of the context
	// (arguments and local variables). Store a reference
	// to the partial so we can handle that.
	if (ga_grow(&ectx->ec_funcrefs, 1) == FAIL)
	{
	    vim_free(pt);
	    return FAIL;
	}
	// Extra variable keeps the count of closures created
	// in the current function call.
	++(((typval_T *)ectx->ec_stack.ga_data) + ectx->ec_frame_idx
		       + STACK_FRAME_SIZE + dfunc->df_varcount)->vval.v_number;

	((partial_T **)ectx->ec_funcrefs.ga_data)
			       [ectx->ec_funcrefs.ga_len] = pt;
	++pt->pt_refcount;
	++ectx->ec_funcrefs.ga_len;
    }
    ++ufunc->uf_refcount;
    return OK;
}


/*
 * Call a "def" function from old Vim script.
 * Return OK or FAIL.
 */
    int
call_def_function(
    ufunc_T	*ufunc,
    int		argc_arg,	// nr of arguments
    typval_T	*argv,		// arguments
    partial_T	*partial,	// optional partial for context
    typval_T	*rettv)		// return value
{
    ectx_T	ectx;		// execution context
    int		argc = argc_arg;
    int		initial_frame_idx;
    typval_T	*tv;
    int		idx;
    int		ret = FAIL;
    int		defcount = ufunc->uf_args.ga_len - argc;
    sctx_T	save_current_sctx = current_sctx;
    int		breakcheck_count = 0;
    int		did_emsg_before = did_emsg_cumul + did_emsg;
    int		save_suppress_errthrow = suppress_errthrow;
    msglist_T	**saved_msg_list = NULL;
    msglist_T	*private_msg_list = NULL;
    funclocal_T funclocal;
    int		save_emsg_silent_def = emsg_silent_def;
    int		save_did_emsg_def = did_emsg_def;
    int		trylevel_at_start = trylevel;
    int		orig_funcdepth;
    where_T	where;

// Get pointer to item in the stack.
#define STACK_TV(idx) (((typval_T *)ectx.ec_stack.ga_data) + idx)

// Get pointer to item at the bottom of the stack, -1 is the bottom.
#undef STACK_TV_BOT
#define STACK_TV_BOT(idx) (((typval_T *)ectx.ec_stack.ga_data) + ectx.ec_stack.ga_len + idx)

// Get pointer to a local variable on the stack.  Negative for arguments.
#define STACK_TV_VAR(idx) (((typval_T *)ectx.ec_stack.ga_data) + ectx.ec_frame_idx + STACK_FRAME_SIZE + idx)

    if (ufunc->uf_def_status == UF_NOT_COMPILED
	    || ufunc->uf_def_status == UF_COMPILE_ERROR
	    || (func_needs_compiling(ufunc, PROFILING(ufunc))
		&& compile_def_function(ufunc, FALSE, PROFILING(ufunc), NULL)
								      == FAIL))
    {
	if (did_emsg_cumul + did_emsg == did_emsg_before)
	    semsg(_(e_function_is_not_compiled_str),
						   printable_func_name(ufunc));
	return FAIL;
    }

    {
	// Check the function was really compiled.
	dfunc_T	*dfunc = ((dfunc_T *)def_functions.ga_data)
							 + ufunc->uf_dfunc_idx;
	if (INSTRUCTIONS(dfunc) == NULL)
	{
	    iemsg("using call_def_function() on not compiled function");
	    return FAIL;
	}
    }

    // If depth of calling is getting too high, don't execute the function.
    orig_funcdepth = funcdepth_get();
    if (funcdepth_increment() == FAIL)
	return FAIL;

    CLEAR_FIELD(funclocal);
    CLEAR_FIELD(ectx);
    ectx.ec_dfunc_idx = ufunc->uf_dfunc_idx;
    ga_init2(&ectx.ec_stack, sizeof(typval_T), 500);
    if (ga_grow(&ectx.ec_stack, 20) == FAIL)
    {
	funcdepth_decrement();
	return FAIL;
    }
    ga_init2(&ectx.ec_trystack, sizeof(trycmd_T), 10);
    ga_init2(&ectx.ec_funcrefs, sizeof(partial_T *), 10);

    idx = argc - ufunc->uf_args.ga_len;
    if (idx > 0 && ufunc->uf_va_name == NULL)
    {
	if (idx == 1)
	    emsg(_(e_one_argument_too_many));
	else
	    semsg(_(e_nr_arguments_too_many), idx);
	goto failed_early;
    }

    // Put arguments on the stack, but no more than what the function expects.
    // A lambda can be called with more arguments than it uses.
    for (idx = 0; idx < argc
	    && (ufunc->uf_va_name != NULL || idx < ufunc->uf_args.ga_len);
									 ++idx)
    {
	if (idx >= ufunc->uf_args.ga_len - ufunc->uf_def_args.ga_len
		&& argv[idx].v_type == VAR_SPECIAL
		&& argv[idx].vval.v_number == VVAL_NONE)
	{
	    // Use the default value.
	    STACK_TV_BOT(0)->v_type = VAR_UNKNOWN;
	}
	else
	{
	    if (ufunc->uf_arg_types != NULL && idx < ufunc->uf_args.ga_len
		    && check_typval_arg_type(
			ufunc->uf_arg_types[idx], &argv[idx], idx + 1) == FAIL)
		goto failed_early;
	    copy_tv(&argv[idx], STACK_TV_BOT(0));
	}
	++ectx.ec_stack.ga_len;
    }

    // Turn varargs into a list.  Empty list if no args.
    if (ufunc->uf_va_name != NULL)
    {
	int vararg_count = argc - ufunc->uf_args.ga_len;

	if (vararg_count < 0)
	    vararg_count = 0;
	else
	    argc -= vararg_count;
	if (exe_newlist(vararg_count, &ectx) == FAIL)
	    goto failed_early;

	// Check the type of the list items.
	tv = STACK_TV_BOT(-1);
	if (ufunc->uf_va_type != NULL
		&& ufunc->uf_va_type != &t_list_any
		&& ufunc->uf_va_type->tt_member != &t_any
		&& tv->vval.v_list != NULL)
	{
	    type_T	*expected = ufunc->uf_va_type->tt_member;
	    listitem_T	*li = tv->vval.v_list->lv_first;

	    for (idx = 0; idx < vararg_count; ++idx)
	    {
		if (check_typval_arg_type(expected, &li->li_tv,
						       argc + idx + 1) == FAIL)
		    goto failed_early;
		li = li->li_next;
	    }
	}

	if (defcount > 0)
	    // Move varargs list to below missing default arguments.
	    *STACK_TV_BOT(defcount - 1) = *STACK_TV_BOT(-1);
	--ectx.ec_stack.ga_len;
    }

    // Make space for omitted arguments, will store default value below.
    // Any varargs list goes after them.
    if (defcount > 0)
	for (idx = 0; idx < defcount; ++idx)
	{
	    STACK_TV_BOT(0)->v_type = VAR_UNKNOWN;
	    ++ectx.ec_stack.ga_len;
	}
    if (ufunc->uf_va_name != NULL)
	++ectx.ec_stack.ga_len;

    // Frame pointer points to just after arguments.
    ectx.ec_frame_idx = ectx.ec_stack.ga_len;
    initial_frame_idx = ectx.ec_frame_idx;

    {
	dfunc_T	*dfunc = ((dfunc_T *)def_functions.ga_data)
							 + ufunc->uf_dfunc_idx;
	ufunc_T *base_ufunc = dfunc->df_ufunc;

	// "uf_partial" is on the ufunc that "df_ufunc" points to, as is done
	// by copy_func().
	if (partial != NULL || base_ufunc->uf_partial != NULL)
	{
	    ectx.ec_outer = ALLOC_CLEAR_ONE(outer_T);
	    if (ectx.ec_outer == NULL)
		goto failed_early;
	    if (partial != NULL)
	    {
		if (partial->pt_outer.out_stack == NULL && current_ectx != NULL)
		{
		    if (current_ectx->ec_outer != NULL)
			*ectx.ec_outer = *current_ectx->ec_outer;
		}
		else
		    *ectx.ec_outer = partial->pt_outer;
	    }
	    else
		*ectx.ec_outer = base_ufunc->uf_partial->pt_outer;
	    ectx.ec_outer->out_up_is_copy = TRUE;
	}
    }

    // dummy frame entries
    for (idx = 0; idx < STACK_FRAME_SIZE; ++idx)
    {
	STACK_TV(ectx.ec_stack.ga_len)->v_type = VAR_UNKNOWN;
	++ectx.ec_stack.ga_len;
    }

    {
	// Reserve space for local variables and any closure reference count.
	dfunc_T	*dfunc = ((dfunc_T *)def_functions.ga_data)
							 + ufunc->uf_dfunc_idx;

	for (idx = 0; idx < dfunc->df_varcount; ++idx)
	    STACK_TV_VAR(idx)->v_type = VAR_UNKNOWN;
	ectx.ec_stack.ga_len += dfunc->df_varcount;
	if (dfunc->df_has_closure)
	{
	    STACK_TV_VAR(idx)->v_type = VAR_NUMBER;
	    STACK_TV_VAR(idx)->vval.v_number = 0;
	    ++ectx.ec_stack.ga_len;
	}

	ectx.ec_instr = INSTRUCTIONS(dfunc);
    }

    // Following errors are in the function, not the caller.
    // Commands behave like vim9script.
    estack_push_ufunc(ufunc, 1);
    current_sctx = ufunc->uf_script_ctx;
    current_sctx.sc_version = SCRIPT_VERSION_VIM9;

    // Use a specific location for storing error messages to be converted to an
    // exception.
    saved_msg_list = msg_list;
    msg_list = &private_msg_list;

    // Do turn errors into exceptions.
    suppress_errthrow = FALSE;

    // Do not delete the function while executing it.
    ++ufunc->uf_calls;

    // When ":silent!" was used before calling then we still abort the
    // function.  If ":silent!" is used in the function then we don't.
    emsg_silent_def = emsg_silent;
    did_emsg_def = 0;

    where.wt_index = 0;
    where.wt_variable = FALSE;

    // Start execution at the first instruction.
    ectx.ec_iidx = 0;

    for (;;)
    {
	isn_T	    *iptr;

	if (++breakcheck_count >= 100)
	{
	    line_breakcheck();
	    breakcheck_count = 0;
	}
	if (got_int)
	{
	    // Turn CTRL-C into an exception.
	    got_int = FALSE;
	    if (throw_exception("Vim:Interrupt", ET_INTERRUPT, NULL) == FAIL)
		goto failed;
	    did_throw = TRUE;
	}

	if (did_emsg && msg_list != NULL && *msg_list != NULL)
	{
	    // Turn an error message into an exception.
	    did_emsg = FALSE;
	    if (throw_exception(*msg_list, ET_ERROR, NULL) == FAIL)
		goto failed;
	    did_throw = TRUE;
	    *msg_list = NULL;
	}

	if (did_throw && !ectx.ec_in_catch)
	{
	    garray_T	*trystack = &ectx.ec_trystack;
	    trycmd_T    *trycmd = NULL;

	    // An exception jumps to the first catch, finally, or returns from
	    // the current function.
	    if (trystack->ga_len > 0)
		trycmd = ((trycmd_T *)trystack->ga_data) + trystack->ga_len - 1;
	    if (trycmd != NULL && trycmd->tcd_frame_idx == ectx.ec_frame_idx)
	    {
		// jump to ":catch" or ":finally"
		ectx.ec_in_catch = TRUE;
		ectx.ec_iidx = trycmd->tcd_catch_idx;
	    }
	    else
	    {
		// Not inside try or need to return from current functions.
		// Push a dummy return value.
		if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
		    goto failed;
		tv = STACK_TV_BOT(0);
		tv->v_type = VAR_NUMBER;
		tv->vval.v_number = 0;
		++ectx.ec_stack.ga_len;
		if (ectx.ec_frame_idx == initial_frame_idx)
		{
		    // At the toplevel we are done.
		    need_rethrow = TRUE;
		    if (handle_closure_in_use(&ectx, FALSE) == FAIL)
			goto failed;
		    goto done;
		}

		if (func_return(&funclocal, &ectx) == FAIL)
		    goto failed;
	    }
	    continue;
	}

	iptr = &ectx.ec_instr[ectx.ec_iidx++];
	switch (iptr->isn_type)
	{
	    // execute Ex command line
	    case ISN_EXEC:
		{
		    source_cookie_T cookie;

		    SOURCING_LNUM = iptr->isn_lnum;
		    // Pass getsourceline to get an error for a missing ":end"
		    // command.
		    CLEAR_FIELD(cookie);
		    cookie.sourcing_lnum = iptr->isn_lnum - 1;
		    if (do_cmdline(iptr->isn_arg.string,
				getsourceline, &cookie,
				   DOCMD_VERBOSE|DOCMD_NOWAIT|DOCMD_KEYTYPED)
									== FAIL
				|| did_emsg)
			goto on_error;
		}
		break;

	    // execute Ex command from pieces on the stack
	    case ISN_EXECCONCAT:
		{
		    int	    count = iptr->isn_arg.number;
		    size_t  len = 0;
		    int	    pass;
		    int	    i;
		    char_u  *cmd = NULL;
		    char_u  *str;

		    for (pass = 1; pass <= 2; ++pass)
		    {
			for (i = 0; i < count; ++i)
			{
			    tv = STACK_TV_BOT(i - count);
			    str = tv->vval.v_string;
			    if (str != NULL && *str != NUL)
			    {
				if (pass == 2)
				    STRCPY(cmd + len, str);
				len += STRLEN(str);
			    }
			    if (pass == 2)
				clear_tv(tv);
			}
			if (pass == 1)
			{
			    cmd = alloc(len + 1);
			    if (cmd == NULL)
				goto failed;
			    len = 0;
			}
		    }

		    SOURCING_LNUM = iptr->isn_lnum;
		    do_cmdline_cmd(cmd);
		    vim_free(cmd);
		}
		break;

	    // execute :echo {string} ...
	    case ISN_ECHO:
		{
		    int count = iptr->isn_arg.echo.echo_count;
		    int	atstart = TRUE;
		    int needclr = TRUE;

		    for (idx = 0; idx < count; ++idx)
		    {
			tv = STACK_TV_BOT(idx - count);
			echo_one(tv, iptr->isn_arg.echo.echo_with_white,
							   &atstart, &needclr);
			clear_tv(tv);
		    }
		    if (needclr)
			msg_clr_eos();
		    ectx.ec_stack.ga_len -= count;
		}
		break;

	    // :execute {string} ...
	    // :echomsg {string} ...
	    // :echoerr {string} ...
	    case ISN_EXECUTE:
	    case ISN_ECHOMSG:
	    case ISN_ECHOERR:
		{
		    int		count = iptr->isn_arg.number;
		    garray_T	ga;
		    char_u	buf[NUMBUFLEN];
		    char_u	*p;
		    int		len;
		    int		failed = FALSE;

		    ga_init2(&ga, 1, 80);
		    for (idx = 0; idx < count; ++idx)
		    {
			tv = STACK_TV_BOT(idx - count);
			if (iptr->isn_type == ISN_EXECUTE)
			{
			    if (tv->v_type == VAR_CHANNEL
						      || tv->v_type == VAR_JOB)
			    {
				SOURCING_LNUM = iptr->isn_lnum;
				emsg(_(e_inval_string));
				break;
			    }
			    else
				p = tv_get_string_buf(tv, buf);
			}
			else
			    p = tv_stringify(tv, buf);

			len = (int)STRLEN(p);
			if (ga_grow(&ga, len + 2) == FAIL)
			    failed = TRUE;
			else
			{
			    if (ga.ga_len > 0)
				((char_u *)(ga.ga_data))[ga.ga_len++] = ' ';
			    STRCPY((char_u *)(ga.ga_data) + ga.ga_len, p);
			    ga.ga_len += len;
			}
			clear_tv(tv);
		    }
		    ectx.ec_stack.ga_len -= count;
		    if (failed)
		    {
			ga_clear(&ga);
			goto on_error;
		    }

		    if (ga.ga_data != NULL)
		    {
			if (iptr->isn_type == ISN_EXECUTE)
			{
			    SOURCING_LNUM = iptr->isn_lnum;
			    do_cmdline_cmd((char_u *)ga.ga_data);
			    if (did_emsg)
			    {
				ga_clear(&ga);
				goto on_error;
			    }
			}
			else
			{
			    msg_sb_eol();
			    if (iptr->isn_type == ISN_ECHOMSG)
			    {
				msg_attr(ga.ga_data, echo_attr);
				out_flush();
			    }
			    else
			    {
				SOURCING_LNUM = iptr->isn_lnum;
				emsg(ga.ga_data);
			    }
			}
		    }
		    ga_clear(&ga);
		}
		break;

	    // load local variable or argument
	    case ISN_LOAD:
		if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
		    goto failed;
		copy_tv(STACK_TV_VAR(iptr->isn_arg.number), STACK_TV_BOT(0));
		++ectx.ec_stack.ga_len;
		break;

	    // load v: variable
	    case ISN_LOADV:
		if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
		    goto failed;
		copy_tv(get_vim_var_tv(iptr->isn_arg.number), STACK_TV_BOT(0));
		++ectx.ec_stack.ga_len;
		break;

	    // load s: variable in Vim9 script
	    case ISN_LOADSCRIPT:
		{
		    scriptref_T	*sref = iptr->isn_arg.script.scriptref;
		    svar_T	 *sv;

		    sv = get_script_svar(sref, &ectx);
		    if (sv == NULL)
			goto failed;
		    allocate_if_null(sv->sv_tv);
		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    copy_tv(sv->sv_tv, STACK_TV_BOT(0));
		    ++ectx.ec_stack.ga_len;
		}
		break;

	    // load s: variable in old script
	    case ISN_LOADS:
		{
		    hashtab_T	*ht = &SCRIPT_VARS(
					       iptr->isn_arg.loadstore.ls_sid);
		    char_u	*name = iptr->isn_arg.loadstore.ls_name;
		    dictitem_T	*di = find_var_in_ht(ht, 0, name, TRUE);

		    if (di == NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			semsg(_(e_undefined_variable_str), name);
			goto on_error;
		    }
		    else
		    {
			if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			    goto failed;
			copy_tv(&di->di_tv, STACK_TV_BOT(0));
			++ectx.ec_stack.ga_len;
		    }
		}
		break;

	    // load g:/b:/w:/t: variable
	    case ISN_LOADG:
	    case ISN_LOADB:
	    case ISN_LOADW:
	    case ISN_LOADT:
		{
		    dictitem_T *di = NULL;
		    hashtab_T *ht = NULL;
		    char namespace;

		    switch (iptr->isn_type)
		    {
			case ISN_LOADG:
			    ht = get_globvar_ht();
			    namespace = 'g';
			    break;
			case ISN_LOADB:
			    ht = &curbuf->b_vars->dv_hashtab;
			    namespace = 'b';
			    break;
			case ISN_LOADW:
			    ht = &curwin->w_vars->dv_hashtab;
			    namespace = 'w';
			    break;
			case ISN_LOADT:
			    ht = &curtab->tp_vars->dv_hashtab;
			    namespace = 't';
			    break;
			default:  // Cannot reach here
			    goto failed;
		    }
		    di = find_var_in_ht(ht, 0, iptr->isn_arg.string, TRUE);

		    if (di == NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			semsg(_(e_undefined_variable_char_str),
					     namespace, iptr->isn_arg.string);
			goto on_error;
		    }
		    else
		    {
			if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			    goto failed;
			copy_tv(&di->di_tv, STACK_TV_BOT(0));
			++ectx.ec_stack.ga_len;
		    }
		}
		break;

	    // load autoload variable
	    case ISN_LOADAUTO:
		{
		    char_u *name = iptr->isn_arg.string;

		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    SOURCING_LNUM = iptr->isn_lnum;
		    if (eval_variable(name, (int)STRLEN(name),
			      STACK_TV_BOT(0), NULL, EVAL_VAR_VERBOSE) == FAIL)
			goto on_error;
		    ++ectx.ec_stack.ga_len;
		}
		break;

	    // load g:/b:/w:/t: namespace
	    case ISN_LOADGDICT:
	    case ISN_LOADBDICT:
	    case ISN_LOADWDICT:
	    case ISN_LOADTDICT:
		{
		    dict_T *d = NULL;

		    switch (iptr->isn_type)
		    {
			case ISN_LOADGDICT: d = get_globvar_dict(); break;
			case ISN_LOADBDICT: d = curbuf->b_vars; break;
			case ISN_LOADWDICT: d = curwin->w_vars; break;
			case ISN_LOADTDICT: d = curtab->tp_vars; break;
			default:  // Cannot reach here
			    goto failed;
		    }
		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    tv = STACK_TV_BOT(0);
		    tv->v_type = VAR_DICT;
		    tv->v_lock = 0;
		    tv->vval.v_dict = d;
		    ++d->dv_refcount;
		    ++ectx.ec_stack.ga_len;
		}
		break;

	    // load &option
	    case ISN_LOADOPT:
		{
		    typval_T	optval;
		    char_u	*name = iptr->isn_arg.string;

		    // This is not expected to fail, name is checked during
		    // compilation: don't set SOURCING_LNUM.
		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    if (eval_option(&name, &optval, TRUE) == FAIL)
			goto failed;
		    *STACK_TV_BOT(0) = optval;
		    ++ectx.ec_stack.ga_len;
		}
		break;

	    // load $ENV
	    case ISN_LOADENV:
		{
		    typval_T	optval;
		    char_u	*name = iptr->isn_arg.string;

		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    // name is always valid, checked when compiling
		    (void)eval_env_var(&name, &optval, TRUE);
		    *STACK_TV_BOT(0) = optval;
		    ++ectx.ec_stack.ga_len;
		}
		break;

	    // load @register
	    case ISN_LOADREG:
		if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
		    goto failed;
		tv = STACK_TV_BOT(0);
		tv->v_type = VAR_STRING;
		tv->v_lock = 0;
		// This may result in NULL, which should be equivalent to an
		// empty string.
		tv->vval.v_string = get_reg_contents(
					  iptr->isn_arg.number, GREG_EXPR_SRC);
		++ectx.ec_stack.ga_len;
		break;

	    // store local variable
	    case ISN_STORE:
		--ectx.ec_stack.ga_len;
		tv = STACK_TV_VAR(iptr->isn_arg.number);
		clear_tv(tv);
		*tv = *STACK_TV_BOT(0);
		break;

	    // store s: variable in old script
	    case ISN_STORES:
		{
		    hashtab_T	*ht = &SCRIPT_VARS(
					       iptr->isn_arg.loadstore.ls_sid);
		    char_u	*name = iptr->isn_arg.loadstore.ls_name;
		    dictitem_T	*di = find_var_in_ht(ht, 0, name + 2, TRUE);

		    --ectx.ec_stack.ga_len;
		    if (di == NULL)
			store_var(name, STACK_TV_BOT(0));
		    else
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			if (var_check_permission(di, name) == FAIL)
			{
			    clear_tv(STACK_TV_BOT(0));
			    goto on_error;
			}
			clear_tv(&di->di_tv);
			di->di_tv = *STACK_TV_BOT(0);
		    }
		}
		break;

	    // store script-local variable in Vim9 script
	    case ISN_STORESCRIPT:
		{
		    scriptref_T	    *sref = iptr->isn_arg.script.scriptref;
		    svar_T	    *sv;

		    sv = get_script_svar(sref, &ectx);
		    if (sv == NULL)
			goto failed;
		    --ectx.ec_stack.ga_len;

		    // "const" and "final" are checked at compile time, locking
		    // the value needs to be checked here.
		    SOURCING_LNUM = iptr->isn_lnum;
		    if (value_check_lock(sv->sv_tv->v_lock, sv->sv_name, FALSE))
		    {
			clear_tv(STACK_TV_BOT(0));
			goto on_error;
		    }

		    clear_tv(sv->sv_tv);
		    *sv->sv_tv = *STACK_TV_BOT(0);
		}
		break;

	    // store option
	    case ISN_STOREOPT:
		{
		    long	n = 0;
		    char_u	*s = NULL;
		    char	*msg;

		    --ectx.ec_stack.ga_len;
		    tv = STACK_TV_BOT(0);
		    if (tv->v_type == VAR_STRING)
		    {
			s = tv->vval.v_string;
			if (s == NULL)
			    s = (char_u *)"";
		    }
		    else
			// must be VAR_NUMBER, CHECKTYPE makes sure
			n = tv->vval.v_number;
		    msg = set_option_value(iptr->isn_arg.storeopt.so_name,
					n, s, iptr->isn_arg.storeopt.so_flags);
		    clear_tv(tv);
		    if (msg != NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			emsg(_(msg));
			goto on_error;
		    }
		}
		break;

	    // store $ENV
	    case ISN_STOREENV:
		--ectx.ec_stack.ga_len;
		tv = STACK_TV_BOT(0);
		vim_setenv_ext(iptr->isn_arg.string, tv_get_string(tv));
		clear_tv(tv);
		break;

	    // store @r
	    case ISN_STOREREG:
		{
		    int	reg = iptr->isn_arg.number;

		    --ectx.ec_stack.ga_len;
		    tv = STACK_TV_BOT(0);
		    write_reg_contents(reg == '@' ? '"' : reg,
						 tv_get_string(tv), -1, FALSE);
		    clear_tv(tv);
		}
		break;

	    // store v: variable
	    case ISN_STOREV:
		--ectx.ec_stack.ga_len;
		if (set_vim_var_tv(iptr->isn_arg.number, STACK_TV_BOT(0))
								       == FAIL)
		    // should not happen, type is checked when compiling
		    goto on_error;
		break;

	    // store g:/b:/w:/t: variable
	    case ISN_STOREG:
	    case ISN_STOREB:
	    case ISN_STOREW:
	    case ISN_STORET:
		{
		    dictitem_T	*di;
		    hashtab_T	*ht;
		    char_u	*name = iptr->isn_arg.string + 2;

		    switch (iptr->isn_type)
		    {
			case ISN_STOREG:
			    ht = get_globvar_ht();
			    break;
			case ISN_STOREB:
			    ht = &curbuf->b_vars->dv_hashtab;
			    break;
			case ISN_STOREW:
			    ht = &curwin->w_vars->dv_hashtab;
			    break;
			case ISN_STORET:
			    ht = &curtab->tp_vars->dv_hashtab;
			    break;
			default:  // Cannot reach here
			    goto failed;
		    }

		    --ectx.ec_stack.ga_len;
		    di = find_var_in_ht(ht, 0, name, TRUE);
		    if (di == NULL)
			store_var(iptr->isn_arg.string, STACK_TV_BOT(0));
		    else
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			if (var_check_permission(di, name) == FAIL)
			    goto on_error;
			clear_tv(&di->di_tv);
			di->di_tv = *STACK_TV_BOT(0);
		    }
		}
		break;

	    // store an autoload variable
	    case ISN_STOREAUTO:
		SOURCING_LNUM = iptr->isn_lnum;
		set_var(iptr->isn_arg.string, STACK_TV_BOT(-1), TRUE);
		clear_tv(STACK_TV_BOT(-1));
		--ectx.ec_stack.ga_len;
		break;

	    // store number in local variable
	    case ISN_STORENR:
		tv = STACK_TV_VAR(iptr->isn_arg.storenr.stnr_idx);
		clear_tv(tv);
		tv->v_type = VAR_NUMBER;
		tv->vval.v_number = iptr->isn_arg.storenr.stnr_val;
		break;

	    // store value in list or dict variable
	    case ISN_STOREINDEX:
		{
		    vartype_T	dest_type = iptr->isn_arg.vartype;
		    typval_T	*tv_idx = STACK_TV_BOT(-2);
		    typval_T	*tv_dest = STACK_TV_BOT(-1);
		    int		status = OK;

		    // Stack contains:
		    // -3 value to be stored
		    // -2 index
		    // -1 dict or list
		    tv = STACK_TV_BOT(-3);
		    SOURCING_LNUM = iptr->isn_lnum;
		    if (dest_type == VAR_ANY)
		    {
			dest_type = tv_dest->v_type;
			if (dest_type == VAR_DICT)
			    status = do_2string(tv_idx, TRUE);
			else if (dest_type == VAR_LIST
					       && tv_idx->v_type != VAR_NUMBER)
			{
			    emsg(_(e_number_exp));
			    status = FAIL;
			}
		    }
		    else if (dest_type != tv_dest->v_type)
		    {
			// just in case, should be OK
			semsg(_(e_expected_str_but_got_str),
				    vartype_name(dest_type),
				    vartype_name(tv_dest->v_type));
			status = FAIL;
		    }

		    if (status == OK && dest_type == VAR_LIST)
		    {
			long	    lidx = (long)tv_idx->vval.v_number;
			list_T	    *list = tv_dest->vval.v_list;

			if (list == NULL)
			{
			    emsg(_(e_list_not_set));
			    goto on_error;
			}
			if (lidx < 0 && list->lv_len + lidx >= 0)
			    // negative index is relative to the end
			    lidx = list->lv_len + lidx;
			if (lidx < 0 || lidx > list->lv_len)
			{
			    semsg(_(e_listidx), lidx);
			    goto on_error;
			}
			if (lidx < list->lv_len)
			{
			    listitem_T *li = list_find(list, lidx);

			    if (error_if_locked(li->li_tv.v_lock,
						    e_cannot_change_list_item))
				goto on_error;
			    // overwrite existing list item
			    clear_tv(&li->li_tv);
			    li->li_tv = *tv;
			}
			else
			{
			    if (error_if_locked(list->lv_lock,
							 e_cannot_change_list))
				goto on_error;
			    // append to list, only fails when out of memory
			    if (list_append_tv(list, tv) == FAIL)
				goto failed;
			    clear_tv(tv);
			}
		    }
		    else if (status == OK && dest_type == VAR_DICT)
		    {
			char_u		*key = tv_idx->vval.v_string;
			dict_T		*dict = tv_dest->vval.v_dict;
			dictitem_T	*di;

			SOURCING_LNUM = iptr->isn_lnum;
			if (dict == NULL)
			{
			    emsg(_(e_dictionary_not_set));
			    goto on_error;
			}
			if (key == NULL)
			    key = (char_u *)"";
			di = dict_find(dict, key, -1);
			if (di != NULL)
			{
			    if (error_if_locked(di->di_tv.v_lock,
						    e_cannot_change_dict_item))
				goto on_error;
			    // overwrite existing value
			    clear_tv(&di->di_tv);
			    di->di_tv = *tv;
			}
			else
			{
			    if (error_if_locked(dict->dv_lock,
							 e_cannot_change_dict))
				goto on_error;
			    // add to dict, only fails when out of memory
			    if (dict_add_tv(dict, (char *)key, tv) == FAIL)
				goto failed;
			    clear_tv(tv);
			}
		    }
		    else
		    {
			status = FAIL;
			semsg(_(e_cannot_index_str), vartype_name(dest_type));
		    }

		    clear_tv(tv_idx);
		    clear_tv(tv_dest);
		    ectx.ec_stack.ga_len -= 3;
		    if (status == FAIL)
		    {
			clear_tv(tv);
			goto on_error;
		    }
		}
		break;

	    // load or store variable or argument from outer scope
	    case ISN_LOADOUTER:
	    case ISN_STOREOUTER:
		{
		    int		depth = iptr->isn_arg.outer.outer_depth;
		    outer_T	*outer = ectx.ec_outer;

		    while (depth > 1 && outer != NULL)
		    {
			outer = outer->out_up;
			--depth;
		    }
		    if (outer == NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			iemsg("LOADOUTER depth more than scope levels");
			goto failed;
		    }
		    tv = ((typval_T *)outer->out_stack->ga_data)
				    + outer->out_frame_idx + STACK_FRAME_SIZE
				    + iptr->isn_arg.outer.outer_idx;
		    if (iptr->isn_type == ISN_LOADOUTER)
		    {
			if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			    goto failed;
			copy_tv(tv, STACK_TV_BOT(0));
			++ectx.ec_stack.ga_len;
		    }
		    else
		    {
			--ectx.ec_stack.ga_len;
			clear_tv(tv);
			*tv = *STACK_TV_BOT(0);
		    }
		}
		break;

	    // unlet item in list or dict variable
	    case ISN_UNLETINDEX:
		{
		    typval_T	*tv_idx = STACK_TV_BOT(-2);
		    typval_T	*tv_dest = STACK_TV_BOT(-1);
		    int		status = OK;

		    // Stack contains:
		    // -2 index
		    // -1 dict or list
		    if (tv_dest->v_type == VAR_DICT)
		    {
			// unlet a dict item, index must be a string
			if (tv_idx->v_type != VAR_STRING)
			{
			    SOURCING_LNUM = iptr->isn_lnum;
			    semsg(_(e_expected_str_but_got_str),
					vartype_name(VAR_STRING),
					vartype_name(tv_idx->v_type));
			    status = FAIL;
			}
			else
			{
			    dict_T	*d = tv_dest->vval.v_dict;
			    char_u	*key = tv_idx->vval.v_string;
			    dictitem_T  *di = NULL;

			    if (key == NULL)
				key = (char_u *)"";
			    if (d != NULL)
				di = dict_find(d, key, (int)STRLEN(key));
			    if (di == NULL)
			    {
				// NULL dict is equivalent to empty dict
				SOURCING_LNUM = iptr->isn_lnum;
				semsg(_(e_dictkey), key);
				status = FAIL;
			    }
			    else
			    {
				// TODO: check for dict or item locked
				dictitem_remove(d, di);
			    }
			}
		    }
		    else if (tv_dest->v_type == VAR_LIST)
		    {
			// unlet a List item, index must be a number
			SOURCING_LNUM = iptr->isn_lnum;
			if (check_for_number(tv_idx) == FAIL)
			{
			    status = FAIL;
			}
			else
			{
			    list_T	*l = tv_dest->vval.v_list;
			    long	n = (long)tv_idx->vval.v_number;
			    listitem_T	*li = NULL;

			    li = list_find(l, n);
			    if (li == NULL)
			    {
				SOURCING_LNUM = iptr->isn_lnum;
				semsg(_(e_listidx), n);
				status = FAIL;
			    }
			    else
				// TODO: check for list or item locked
				listitem_remove(l, li);
			}
		    }
		    else
		    {
			status = FAIL;
			semsg(_(e_cannot_index_str),
						vartype_name(tv_dest->v_type));
		    }

		    clear_tv(tv_idx);
		    clear_tv(tv_dest);
		    ectx.ec_stack.ga_len -= 2;
		    if (status == FAIL)
			goto on_error;
		}
		break;

	    // unlet range of items in list variable
	    case ISN_UNLETRANGE:
		{
		    // Stack contains:
		    // -3 index1
		    // -2 index2
		    // -1 dict or list
		    typval_T	*tv_idx1 = STACK_TV_BOT(-3);
		    typval_T	*tv_idx2 = STACK_TV_BOT(-2);
		    typval_T	*tv_dest = STACK_TV_BOT(-1);
		    int		status = OK;

		    if (tv_dest->v_type == VAR_LIST)
		    {
			// indexes must be a number
			SOURCING_LNUM = iptr->isn_lnum;
			if (check_for_number(tv_idx1) == FAIL
				|| check_for_number(tv_idx2) == FAIL)
			{
			    status = FAIL;
			}
			else
			{
			    list_T	*l = tv_dest->vval.v_list;
			    long	n1 = (long)tv_idx1->vval.v_number;
			    long	n2 = (long)tv_idx2->vval.v_number;
			    listitem_T	*li;

			    li = list_find_index(l, &n1);
			    if (li == NULL
				     || list_unlet_range(l, li, NULL, n1,
							    TRUE, n2) == FAIL)
				status = FAIL;
			}
		    }
		    else
		    {
			status = FAIL;
			SOURCING_LNUM = iptr->isn_lnum;
			semsg(_(e_cannot_index_str),
						vartype_name(tv_dest->v_type));
		    }

		    clear_tv(tv_idx1);
		    clear_tv(tv_idx2);
		    clear_tv(tv_dest);
		    ectx.ec_stack.ga_len -= 3;
		    if (status == FAIL)
			goto on_error;
		}
		break;

	    // push constant
	    case ISN_PUSHNR:
	    case ISN_PUSHBOOL:
	    case ISN_PUSHSPEC:
	    case ISN_PUSHF:
	    case ISN_PUSHS:
	    case ISN_PUSHBLOB:
	    case ISN_PUSHFUNC:
	    case ISN_PUSHCHANNEL:
	    case ISN_PUSHJOB:
		if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
		    goto failed;
		tv = STACK_TV_BOT(0);
		tv->v_lock = 0;
		++ectx.ec_stack.ga_len;
		switch (iptr->isn_type)
		{
		    case ISN_PUSHNR:
			tv->v_type = VAR_NUMBER;
			tv->vval.v_number = iptr->isn_arg.number;
			break;
		    case ISN_PUSHBOOL:
			tv->v_type = VAR_BOOL;
			tv->vval.v_number = iptr->isn_arg.number;
			break;
		    case ISN_PUSHSPEC:
			tv->v_type = VAR_SPECIAL;
			tv->vval.v_number = iptr->isn_arg.number;
			break;
#ifdef FEAT_FLOAT
		    case ISN_PUSHF:
			tv->v_type = VAR_FLOAT;
			tv->vval.v_float = iptr->isn_arg.fnumber;
			break;
#endif
		    case ISN_PUSHBLOB:
			blob_copy(iptr->isn_arg.blob, tv);
			break;
		    case ISN_PUSHFUNC:
			tv->v_type = VAR_FUNC;
			if (iptr->isn_arg.string == NULL)
			    tv->vval.v_string = NULL;
			else
			    tv->vval.v_string =
					     vim_strsave(iptr->isn_arg.string);
			break;
		    case ISN_PUSHCHANNEL:
#ifdef FEAT_JOB_CHANNEL
			tv->v_type = VAR_CHANNEL;
			tv->vval.v_channel = iptr->isn_arg.channel;
			if (tv->vval.v_channel != NULL)
			    ++tv->vval.v_channel->ch_refcount;
#endif
			break;
		    case ISN_PUSHJOB:
#ifdef FEAT_JOB_CHANNEL
			tv->v_type = VAR_JOB;
			tv->vval.v_job = iptr->isn_arg.job;
			if (tv->vval.v_job != NULL)
			    ++tv->vval.v_job->jv_refcount;
#endif
			break;
		    default:
			tv->v_type = VAR_STRING;
			tv->vval.v_string = vim_strsave(
				iptr->isn_arg.string == NULL
					? (char_u *)"" : iptr->isn_arg.string);
		}
		break;

	    case ISN_UNLET:
		if (do_unlet(iptr->isn_arg.unlet.ul_name,
				       iptr->isn_arg.unlet.ul_forceit) == FAIL)
		    goto on_error;
		break;
	    case ISN_UNLETENV:
		vim_unsetenv(iptr->isn_arg.unlet.ul_name);
		break;

	    case ISN_LOCKCONST:
		item_lock(STACK_TV_BOT(-1), 100, TRUE, TRUE);
		break;

	    // create a list from items on the stack; uses a single allocation
	    // for the list header and the items
	    case ISN_NEWLIST:
		if (exe_newlist(iptr->isn_arg.number, &ectx) == FAIL)
		    goto failed;
		break;

	    // create a dict from items on the stack
	    case ISN_NEWDICT:
		{
		    int		count = iptr->isn_arg.number;
		    dict_T	*dict = dict_alloc();
		    dictitem_T	*item;
		    char_u	*key;

		    if (dict == NULL)
			goto failed;
		    for (idx = 0; idx < count; ++idx)
		    {
			// have already checked key type is VAR_STRING
			tv = STACK_TV_BOT(2 * (idx - count));
			// check key is unique
			key = tv->vval.v_string == NULL
					    ? (char_u *)"" : tv->vval.v_string;
			item = dict_find(dict, key, -1);
			if (item != NULL)
			{
			    SOURCING_LNUM = iptr->isn_lnum;
			    semsg(_(e_duplicate_key), key);
			    dict_unref(dict);
			    goto on_error;
			}
			item = dictitem_alloc(key);
			clear_tv(tv);
			if (item == NULL)
			{
			    dict_unref(dict);
			    goto failed;
			}
			item->di_tv = *STACK_TV_BOT(2 * (idx - count) + 1);
			item->di_tv.v_lock = 0;
			if (dict_add(dict, item) == FAIL)
			{
			    // can this ever happen?
			    dict_unref(dict);
			    goto failed;
			}
		    }

		    if (count > 0)
			ectx.ec_stack.ga_len -= 2 * count - 1;
		    else if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    else
			++ectx.ec_stack.ga_len;
		    tv = STACK_TV_BOT(-1);
		    tv->v_type = VAR_DICT;
		    tv->v_lock = 0;
		    tv->vval.v_dict = dict;
		    ++dict->dv_refcount;
		}
		break;

	    // call a :def function
	    case ISN_DCALL:
		SOURCING_LNUM = iptr->isn_lnum;
		if (call_dfunc(iptr->isn_arg.dfunc.cdf_idx,
				NULL,
				iptr->isn_arg.dfunc.cdf_argcount,
				&funclocal,
				&ectx) == FAIL)
		    goto on_error;
		break;

	    // call a builtin function
	    case ISN_BCALL:
		SOURCING_LNUM = iptr->isn_lnum;
		if (call_bfunc(iptr->isn_arg.bfunc.cbf_idx,
			      iptr->isn_arg.bfunc.cbf_argcount,
			      &ectx) == FAIL)
		    goto on_error;
		break;

	    // call a funcref or partial
	    case ISN_PCALL:
		{
		    cpfunc_T	*pfunc = &iptr->isn_arg.pfunc;
		    int		r;
		    typval_T	partial_tv;

		    SOURCING_LNUM = iptr->isn_lnum;
		    if (pfunc->cpf_top)
		    {
			// funcref is above the arguments
			tv = STACK_TV_BOT(-pfunc->cpf_argcount - 1);
		    }
		    else
		    {
			// Get the funcref from the stack.
			--ectx.ec_stack.ga_len;
			partial_tv = *STACK_TV_BOT(0);
			tv = &partial_tv;
		    }
		    r = call_partial(tv, pfunc->cpf_argcount,
							    &funclocal, &ectx);
		    if (tv == &partial_tv)
			clear_tv(&partial_tv);
		    if (r == FAIL)
			goto on_error;
		}
		break;

	    case ISN_PCALL_END:
		// PCALL finished, arguments have been consumed and replaced by
		// the return value.  Now clear the funcref from the stack,
		// and move the return value in its place.
		--ectx.ec_stack.ga_len;
		clear_tv(STACK_TV_BOT(-1));
		*STACK_TV_BOT(-1) = *STACK_TV_BOT(0);
		break;

	    // call a user defined function or funcref/partial
	    case ISN_UCALL:
		{
		    cufunc_T	*cufunc = &iptr->isn_arg.ufunc;

		    SOURCING_LNUM = iptr->isn_lnum;
		    if (call_eval_func(cufunc->cuf_name, cufunc->cuf_argcount,
					      &funclocal, &ectx, iptr) == FAIL)
			goto on_error;
		}
		break;

	    // return from a :def function call
	    case ISN_RETURN_ZERO:
		if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
		    goto failed;
		tv = STACK_TV_BOT(0);
		++ectx.ec_stack.ga_len;
		tv->v_type = VAR_NUMBER;
		tv->vval.v_number = 0;
		tv->v_lock = 0;
		// FALLTHROUGH

	    case ISN_RETURN:
		{
		    garray_T	*trystack = &ectx.ec_trystack;
		    trycmd_T    *trycmd = NULL;

		    if (trystack->ga_len > 0)
			trycmd = ((trycmd_T *)trystack->ga_data)
							+ trystack->ga_len - 1;
		    if (trycmd != NULL
				 && trycmd->tcd_frame_idx == ectx.ec_frame_idx)
		    {
			// jump to ":finally" or ":endtry"
			if (trycmd->tcd_finally_idx != 0)
			    ectx.ec_iidx = trycmd->tcd_finally_idx;
			else
			    ectx.ec_iidx = trycmd->tcd_endtry_idx;
			trycmd->tcd_return = TRUE;
		    }
		    else
			goto func_return;
		}
		break;

	    // push a function reference to a compiled function
	    case ISN_FUNCREF:
		{
		    partial_T   *pt = ALLOC_CLEAR_ONE(partial_T);
		    dfunc_T	*pt_dfunc = ((dfunc_T *)def_functions.ga_data)
					       + iptr->isn_arg.funcref.fr_func;

		    if (pt == NULL)
			goto failed;
		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
		    {
			vim_free(pt);
			goto failed;
		    }
		    if (fill_partial_and_closure(pt, pt_dfunc->df_ufunc,
								&ectx) == FAIL)
			goto failed;

		    tv = STACK_TV_BOT(0);
		    ++ectx.ec_stack.ga_len;
		    tv->vval.v_partial = pt;
		    tv->v_type = VAR_PARTIAL;
		    tv->v_lock = 0;
		}
		break;

	    // Create a global function from a lambda.
	    case ISN_NEWFUNC:
		{
		    newfunc_T	*newfunc = &iptr->isn_arg.newfunc;

		    if (copy_func(newfunc->nf_lambda, newfunc->nf_global,
								&ectx) == FAIL)
			goto failed;
		}
		break;

	    // List functions
	    case ISN_DEF:
		if (iptr->isn_arg.string == NULL)
		    list_functions(NULL);
		else
		{
		    exarg_T ea;

		    CLEAR_FIELD(ea);
		    ea.cmd = ea.arg = iptr->isn_arg.string;
		    define_function(&ea, NULL);
		}
		break;

	    // jump if a condition is met
	    case ISN_JUMP:
		{
		    jumpwhen_T	when = iptr->isn_arg.jump.jump_when;
		    int		error = FALSE;
		    int		jump = TRUE;

		    if (when != JUMP_ALWAYS)
		    {
			tv = STACK_TV_BOT(-1);
			if (when == JUMP_IF_COND_FALSE
				|| when == JUMP_IF_FALSE
				|| when == JUMP_IF_COND_TRUE)
			{
			    SOURCING_LNUM = iptr->isn_lnum;
			    jump = tv_get_bool_chk(tv, &error);
			    if (error)
				goto on_error;
			}
			else
			    jump = tv2bool(tv);
			if (when == JUMP_IF_FALSE
					     || when == JUMP_AND_KEEP_IF_FALSE
					     || when == JUMP_IF_COND_FALSE)
			    jump = !jump;
			if (when == JUMP_IF_FALSE || !jump)
			{
			    // drop the value from the stack
			    clear_tv(tv);
			    --ectx.ec_stack.ga_len;
			}
		    }
		    if (jump)
			ectx.ec_iidx = iptr->isn_arg.jump.jump_where;
		}
		break;

	    // Jump if an argument with a default value was already set and not
	    // v:none.
	    case ISN_JUMP_IF_ARG_SET:
		tv = STACK_TV_VAR(iptr->isn_arg.jumparg.jump_arg_off);
		if (tv->v_type != VAR_UNKNOWN
			&& !(tv->v_type == VAR_SPECIAL
					    && tv->vval.v_number == VVAL_NONE))
		    ectx.ec_iidx = iptr->isn_arg.jumparg.jump_where;
		break;

	    // top of a for loop
	    case ISN_FOR:
		{
		    typval_T	*ltv = STACK_TV_BOT(-1);
		    typval_T	*idxtv =
				   STACK_TV_VAR(iptr->isn_arg.forloop.for_idx);

		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    if (ltv->v_type == VAR_LIST)
		    {
			list_T *list = ltv->vval.v_list;

			// push the next item from the list
			++idxtv->vval.v_number;
			if (list == NULL
				       || idxtv->vval.v_number >= list->lv_len)
			{
			    // past the end of the list, jump to "endfor"
			    ectx.ec_iidx = iptr->isn_arg.forloop.for_end;
			    may_restore_cmdmod(&funclocal);
			}
			else if (list->lv_first == &range_list_item)
			{
			    // non-materialized range() list
			    tv = STACK_TV_BOT(0);
			    tv->v_type = VAR_NUMBER;
			    tv->v_lock = 0;
			    tv->vval.v_number = list_find_nr(
					     list, idxtv->vval.v_number, NULL);
			    ++ectx.ec_stack.ga_len;
			}
			else
			{
			    listitem_T *li = list_find(list,
							 idxtv->vval.v_number);

			    copy_tv(&li->li_tv, STACK_TV_BOT(0));
			    ++ectx.ec_stack.ga_len;
			}
		    }
		    else if (ltv->v_type == VAR_STRING)
		    {
			char_u	*str = ltv->vval.v_string;

			// Push the next character from the string.  The index
			// is for the last byte of the previous character.
			++idxtv->vval.v_number;
			if (str == NULL || str[idxtv->vval.v_number] == NUL)
			{
			    // past the end of the string, jump to "endfor"
			    ectx.ec_iidx = iptr->isn_arg.forloop.for_end;
			    may_restore_cmdmod(&funclocal);
			}
			else
			{
			    int	clen = mb_ptr2len(str + idxtv->vval.v_number);

			    tv = STACK_TV_BOT(0);
			    tv->v_type = VAR_STRING;
			    tv->vval.v_string = vim_strnsave(
					     str + idxtv->vval.v_number, clen);
			    ++ectx.ec_stack.ga_len;
			    idxtv->vval.v_number += clen - 1;
			}
		    }
		    else
		    {
			// TODO: support Blob
			semsg(_(e_for_loop_on_str_not_supported),
						    vartype_name(ltv->v_type));
			goto failed;
		    }
		}
		break;

	    // start of ":try" block
	    case ISN_TRY:
		{
		    trycmd_T    *trycmd = NULL;

		    if (GA_GROW(&ectx.ec_trystack, 1) == FAIL)
			goto failed;
		    trycmd = ((trycmd_T *)ectx.ec_trystack.ga_data)
						     + ectx.ec_trystack.ga_len;
		    ++ectx.ec_trystack.ga_len;
		    ++trylevel;
		    CLEAR_POINTER(trycmd);
		    trycmd->tcd_frame_idx = ectx.ec_frame_idx;
		    trycmd->tcd_stack_len = ectx.ec_stack.ga_len;
		    trycmd->tcd_catch_idx =
					  iptr->isn_arg.try.try_ref->try_catch;
		    trycmd->tcd_finally_idx =
					iptr->isn_arg.try.try_ref->try_finally;
		    trycmd->tcd_endtry_idx =
					 iptr->isn_arg.try.try_ref->try_endtry;
		}
		break;

	    case ISN_PUSHEXC:
		if (current_exception == NULL)
		{
		    SOURCING_LNUM = iptr->isn_lnum;
		    iemsg("Evaluating catch while current_exception is NULL");
		    goto failed;
		}
		if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
		    goto failed;
		tv = STACK_TV_BOT(0);
		++ectx.ec_stack.ga_len;
		tv->v_type = VAR_STRING;
		tv->v_lock = 0;
		tv->vval.v_string = vim_strsave(
					   (char_u *)current_exception->value);
		break;

	    case ISN_CATCH:
		{
		    garray_T	*trystack = &ectx.ec_trystack;

		    may_restore_cmdmod(&funclocal);
		    if (trystack->ga_len > 0)
		    {
			trycmd_T    *trycmd = ((trycmd_T *)trystack->ga_data)
							+ trystack->ga_len - 1;
			trycmd->tcd_caught = TRUE;
		    }
		    did_emsg = got_int = did_throw = FALSE;
		    force_abort = need_rethrow = FALSE;
		    catch_exception(current_exception);
		}
		break;

	    case ISN_TRYCONT:
		{
		    garray_T	*trystack = &ectx.ec_trystack;
		    trycont_T	*trycont = &iptr->isn_arg.trycont;
		    int		i;
		    trycmd_T    *trycmd;
		    int		iidx = trycont->tct_where;

		    if (trystack->ga_len < trycont->tct_levels)
		    {
			siemsg("TRYCONT: expected %d levels, found %d",
					trycont->tct_levels, trystack->ga_len);
			goto failed;
		    }
		    // Make :endtry jump to any outer try block and the last
		    // :endtry inside the loop to the loop start.
		    for (i = trycont->tct_levels; i > 0; --i)
		    {
			trycmd = ((trycmd_T *)trystack->ga_data)
							+ trystack->ga_len - i;
			// Add one to tcd_cont to be able to jump to
			// instruction with index zero.
			trycmd->tcd_cont = iidx + 1;
			iidx = trycmd->tcd_finally_idx == 0
			    ? trycmd->tcd_endtry_idx : trycmd->tcd_finally_idx;
		    }
		    // jump to :finally or :endtry of current try statement
		    ectx.ec_iidx = iidx;
		}
		break;

	    case ISN_FINALLY:
		{
		    garray_T	*trystack = &ectx.ec_trystack;
		    trycmd_T    *trycmd = ((trycmd_T *)trystack->ga_data)
							+ trystack->ga_len - 1;

		    // Reset the index to avoid a return statement jumps here
		    // again.
		    trycmd->tcd_finally_idx = 0;
		    break;
		}

	    // end of ":try" block
	    case ISN_ENDTRY:
		{
		    garray_T	*trystack = &ectx.ec_trystack;

		    if (trystack->ga_len > 0)
		    {
			trycmd_T    *trycmd;

			--trystack->ga_len;
			--trylevel;
			ectx.ec_in_catch = FALSE;
			trycmd = ((trycmd_T *)trystack->ga_data)
							    + trystack->ga_len;
			if (trycmd->tcd_caught && current_exception != NULL)
			{
			    // discard the exception
			    if (caught_stack == current_exception)
				caught_stack = caught_stack->caught;
			    discard_current_exception();
			}

			if (trycmd->tcd_return)
			    goto func_return;

			while (ectx.ec_stack.ga_len > trycmd->tcd_stack_len)
			{
			    --ectx.ec_stack.ga_len;
			    clear_tv(STACK_TV_BOT(0));
			}
			if (trycmd->tcd_cont != 0)
			    // handling :continue: jump to outer try block or
			    // start of the loop
			    ectx.ec_iidx = trycmd->tcd_cont - 1;
		    }
		}
		break;

	    case ISN_THROW:
		{
		    garray_T	*trystack = &ectx.ec_trystack;

		    if (trystack->ga_len == 0 && trylevel == 0 && emsg_silent)
		    {
			// throwing an exception while using "silent!" causes
			// the function to abort but not display an error.
			tv = STACK_TV_BOT(-1);
			clear_tv(tv);
			tv->v_type = VAR_NUMBER;
			tv->vval.v_number = 0;
			goto done;
		    }
		    --ectx.ec_stack.ga_len;
		    tv = STACK_TV_BOT(0);
		    if (tv->vval.v_string == NULL
				       || *skipwhite(tv->vval.v_string) == NUL)
		    {
			vim_free(tv->vval.v_string);
			SOURCING_LNUM = iptr->isn_lnum;
			emsg(_(e_throw_with_empty_string));
			goto failed;
		    }

		    // Inside a "catch" we need to first discard the caught
		    // exception.
		    if (trystack->ga_len > 0)
		    {
			trycmd_T    *trycmd = ((trycmd_T *)trystack->ga_data)
							+ trystack->ga_len - 1;
			if (trycmd->tcd_caught && current_exception != NULL)
			{
			    // discard the exception
			    if (caught_stack == current_exception)
				caught_stack = caught_stack->caught;
			    discard_current_exception();
			    trycmd->tcd_caught = FALSE;
			}
		    }

		    if (throw_exception(tv->vval.v_string, ET_USER, NULL)
								       == FAIL)
		    {
			vim_free(tv->vval.v_string);
			goto failed;
		    }
		    did_throw = TRUE;
		}
		break;

	    // compare with special values
	    case ISN_COMPAREBOOL:
	    case ISN_COMPARESPECIAL:
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    varnumber_T arg1 = tv1->vval.v_number;
		    varnumber_T arg2 = tv2->vval.v_number;
		    int		res;

		    switch (iptr->isn_arg.op.op_type)
		    {
			case EXPR_EQUAL: res = arg1 == arg2; break;
			case EXPR_NEQUAL: res = arg1 != arg2; break;
			default: res = 0; break;
		    }

		    --ectx.ec_stack.ga_len;
		    tv1->v_type = VAR_BOOL;
		    tv1->vval.v_number = res ? VVAL_TRUE : VVAL_FALSE;
		}
		break;

	    // Operation with two number arguments
	    case ISN_OPNR:
	    case ISN_COMPARENR:
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    varnumber_T arg1 = tv1->vval.v_number;
		    varnumber_T arg2 = tv2->vval.v_number;
		    varnumber_T res;

		    switch (iptr->isn_arg.op.op_type)
		    {
			case EXPR_MULT: res = arg1 * arg2; break;
			case EXPR_DIV: res = arg1 / arg2; break;
			case EXPR_REM: res = arg1 % arg2; break;
			case EXPR_SUB: res = arg1 - arg2; break;
			case EXPR_ADD: res = arg1 + arg2; break;

			case EXPR_EQUAL: res = arg1 == arg2; break;
			case EXPR_NEQUAL: res = arg1 != arg2; break;
			case EXPR_GREATER: res = arg1 > arg2; break;
			case EXPR_GEQUAL: res = arg1 >= arg2; break;
			case EXPR_SMALLER: res = arg1 < arg2; break;
			case EXPR_SEQUAL: res = arg1 <= arg2; break;
			default: res = 0; break;
		    }

		    --ectx.ec_stack.ga_len;
		    if (iptr->isn_type == ISN_COMPARENR)
		    {
			tv1->v_type = VAR_BOOL;
			tv1->vval.v_number = res ? VVAL_TRUE : VVAL_FALSE;
		    }
		    else
			tv1->vval.v_number = res;
		}
		break;

	    // Computation with two float arguments
	    case ISN_OPFLOAT:
	    case ISN_COMPAREFLOAT:
#ifdef FEAT_FLOAT
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    float_T	arg1 = tv1->vval.v_float;
		    float_T	arg2 = tv2->vval.v_float;
		    float_T	res = 0;
		    int		cmp = FALSE;

		    switch (iptr->isn_arg.op.op_type)
		    {
			case EXPR_MULT: res = arg1 * arg2; break;
			case EXPR_DIV: res = arg1 / arg2; break;
			case EXPR_SUB: res = arg1 - arg2; break;
			case EXPR_ADD: res = arg1 + arg2; break;

			case EXPR_EQUAL: cmp = arg1 == arg2; break;
			case EXPR_NEQUAL: cmp = arg1 != arg2; break;
			case EXPR_GREATER: cmp = arg1 > arg2; break;
			case EXPR_GEQUAL: cmp = arg1 >= arg2; break;
			case EXPR_SMALLER: cmp = arg1 < arg2; break;
			case EXPR_SEQUAL: cmp = arg1 <= arg2; break;
			default: cmp = 0; break;
		    }
		    --ectx.ec_stack.ga_len;
		    if (iptr->isn_type == ISN_COMPAREFLOAT)
		    {
			tv1->v_type = VAR_BOOL;
			tv1->vval.v_number = cmp ? VVAL_TRUE : VVAL_FALSE;
		    }
		    else
			tv1->vval.v_float = res;
		}
#endif
		break;

	    case ISN_COMPARELIST:
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    list_T	*arg1 = tv1->vval.v_list;
		    list_T	*arg2 = tv2->vval.v_list;
		    int		cmp = FALSE;
		    int		ic = iptr->isn_arg.op.op_ic;

		    switch (iptr->isn_arg.op.op_type)
		    {
			case EXPR_EQUAL: cmp =
				      list_equal(arg1, arg2, ic, FALSE); break;
			case EXPR_NEQUAL: cmp =
				     !list_equal(arg1, arg2, ic, FALSE); break;
			case EXPR_IS: cmp = arg1 == arg2; break;
			case EXPR_ISNOT: cmp = arg1 != arg2; break;
			default: cmp = 0; break;
		    }
		    --ectx.ec_stack.ga_len;
		    clear_tv(tv1);
		    clear_tv(tv2);
		    tv1->v_type = VAR_BOOL;
		    tv1->vval.v_number = cmp ? VVAL_TRUE : VVAL_FALSE;
		}
		break;

	    case ISN_COMPAREBLOB:
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    blob_T	*arg1 = tv1->vval.v_blob;
		    blob_T	*arg2 = tv2->vval.v_blob;
		    int		cmp = FALSE;

		    switch (iptr->isn_arg.op.op_type)
		    {
			case EXPR_EQUAL: cmp = blob_equal(arg1, arg2); break;
			case EXPR_NEQUAL: cmp = !blob_equal(arg1, arg2); break;
			case EXPR_IS: cmp = arg1 == arg2; break;
			case EXPR_ISNOT: cmp = arg1 != arg2; break;
			default: cmp = 0; break;
		    }
		    --ectx.ec_stack.ga_len;
		    clear_tv(tv1);
		    clear_tv(tv2);
		    tv1->v_type = VAR_BOOL;
		    tv1->vval.v_number = cmp ? VVAL_TRUE : VVAL_FALSE;
		}
		break;

		// TODO: handle separately
	    case ISN_COMPARESTRING:
	    case ISN_COMPAREDICT:
	    case ISN_COMPAREFUNC:
	    case ISN_COMPAREANY:
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    exprtype_T	exprtype = iptr->isn_arg.op.op_type;
		    int		ic = iptr->isn_arg.op.op_ic;

		    SOURCING_LNUM = iptr->isn_lnum;
		    typval_compare(tv1, tv2, exprtype, ic);
		    clear_tv(tv2);
		    --ectx.ec_stack.ga_len;
		}
		break;

	    case ISN_ADDLIST:
	    case ISN_ADDBLOB:
		{
		    typval_T *tv1 = STACK_TV_BOT(-2);
		    typval_T *tv2 = STACK_TV_BOT(-1);

		    // add two lists or blobs
		    if (iptr->isn_type == ISN_ADDLIST)
			eval_addlist(tv1, tv2);
		    else
			eval_addblob(tv1, tv2);
		    clear_tv(tv2);
		    --ectx.ec_stack.ga_len;
		}
		break;

	    case ISN_LISTAPPEND:
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    list_T	*l = tv1->vval.v_list;

		    // add an item to a list
		    if (l == NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			emsg(_(e_cannot_add_to_null_list));
			goto on_error;
		    }
		    if (list_append_tv(l, tv2) == FAIL)
			goto failed;
		    clear_tv(tv2);
		    --ectx.ec_stack.ga_len;
		}
		break;

	    case ISN_BLOBAPPEND:
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    blob_T	*b = tv1->vval.v_blob;
		    int		error = FALSE;
		    varnumber_T n;

		    // add a number to a blob
		    if (b == NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			emsg(_(e_cannot_add_to_null_blob));
			goto on_error;
		    }
		    n = tv_get_number_chk(tv2, &error);
		    if (error)
			goto on_error;
		    ga_append(&b->bv_ga, (int)n);
		    --ectx.ec_stack.ga_len;
		}
		break;

	    // Computation with two arguments of unknown type
	    case ISN_OPANY:
		{
		    typval_T	*tv1 = STACK_TV_BOT(-2);
		    typval_T	*tv2 = STACK_TV_BOT(-1);
		    varnumber_T	n1, n2;
#ifdef FEAT_FLOAT
		    float_T	f1 = 0, f2 = 0;
#endif
		    int		error = FALSE;

		    if (iptr->isn_arg.op.op_type == EXPR_ADD)
		    {
			if (tv1->v_type == VAR_LIST && tv2->v_type == VAR_LIST)
			{
			    eval_addlist(tv1, tv2);
			    clear_tv(tv2);
			    --ectx.ec_stack.ga_len;
			    break;
			}
			else if (tv1->v_type == VAR_BLOB
						    && tv2->v_type == VAR_BLOB)
			{
			    eval_addblob(tv1, tv2);
			    clear_tv(tv2);
			    --ectx.ec_stack.ga_len;
			    break;
			}
		    }
#ifdef FEAT_FLOAT
		    if (tv1->v_type == VAR_FLOAT)
		    {
			f1 = tv1->vval.v_float;
			n1 = 0;
		    }
		    else
#endif
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			n1 = tv_get_number_chk(tv1, &error);
			if (error)
			    goto on_error;
#ifdef FEAT_FLOAT
			if (tv2->v_type == VAR_FLOAT)
			    f1 = n1;
#endif
		    }
#ifdef FEAT_FLOAT
		    if (tv2->v_type == VAR_FLOAT)
		    {
			f2 = tv2->vval.v_float;
			n2 = 0;
		    }
		    else
#endif
		    {
			n2 = tv_get_number_chk(tv2, &error);
			if (error)
			    goto on_error;
#ifdef FEAT_FLOAT
			if (tv1->v_type == VAR_FLOAT)
			    f2 = n2;
#endif
		    }
#ifdef FEAT_FLOAT
		    // if there is a float on either side the result is a float
		    if (tv1->v_type == VAR_FLOAT || tv2->v_type == VAR_FLOAT)
		    {
			switch (iptr->isn_arg.op.op_type)
			{
			    case EXPR_MULT: f1 = f1 * f2; break;
			    case EXPR_DIV:  f1 = f1 / f2; break;
			    case EXPR_SUB:  f1 = f1 - f2; break;
			    case EXPR_ADD:  f1 = f1 + f2; break;
			    default: SOURCING_LNUM = iptr->isn_lnum;
				     emsg(_(e_modulus));
				     goto on_error;
			}
			clear_tv(tv1);
			clear_tv(tv2);
			tv1->v_type = VAR_FLOAT;
			tv1->vval.v_float = f1;
			--ectx.ec_stack.ga_len;
		    }
		    else
#endif
		    {
			int failed = FALSE;

			switch (iptr->isn_arg.op.op_type)
			{
			    case EXPR_MULT: n1 = n1 * n2; break;
			    case EXPR_DIV:  n1 = num_divide(n1, n2, &failed);
					    if (failed)
						goto on_error;
					    break;
			    case EXPR_SUB:  n1 = n1 - n2; break;
			    case EXPR_ADD:  n1 = n1 + n2; break;
			    default:	    n1 = num_modulus(n1, n2, &failed);
					    if (failed)
						goto on_error;
					    break;
			}
			clear_tv(tv1);
			clear_tv(tv2);
			tv1->v_type = VAR_NUMBER;
			tv1->vval.v_number = n1;
			--ectx.ec_stack.ga_len;
		    }
		}
		break;

	    case ISN_CONCAT:
		{
		    char_u *str1 = STACK_TV_BOT(-2)->vval.v_string;
		    char_u *str2 = STACK_TV_BOT(-1)->vval.v_string;
		    char_u *res;

		    res = concat_str(str1, str2);
		    clear_tv(STACK_TV_BOT(-2));
		    clear_tv(STACK_TV_BOT(-1));
		    --ectx.ec_stack.ga_len;
		    STACK_TV_BOT(-1)->vval.v_string = res;
		}
		break;

	    case ISN_STRINDEX:
	    case ISN_STRSLICE:
		{
		    int		is_slice = iptr->isn_type == ISN_STRSLICE;
		    varnumber_T	n1 = 0, n2;
		    char_u	*res;

		    // string index: string is at stack-2, index at stack-1
		    // string slice: string is at stack-3, first index at
		    // stack-2, second index at stack-1
		    if (is_slice)
		    {
			tv = STACK_TV_BOT(-2);
			n1 = tv->vval.v_number;
		    }

		    tv = STACK_TV_BOT(-1);
		    n2 = tv->vval.v_number;

		    ectx.ec_stack.ga_len -= is_slice ? 2 : 1;
		    tv = STACK_TV_BOT(-1);
		    if (is_slice)
			// Slice: Select the characters from the string
			res = string_slice(tv->vval.v_string, n1, n2, FALSE);
		    else
			// Index: The resulting variable is a string of a
			// single character (including composing characters).
			// If the index is too big or negative the result is
			// empty.
			res = char_from_string(tv->vval.v_string, n2);
		    vim_free(tv->vval.v_string);
		    tv->vval.v_string = res;
		}
		break;

	    case ISN_LISTINDEX:
	    case ISN_LISTSLICE:
	    case ISN_BLOBINDEX:
	    case ISN_BLOBSLICE:
		{
		    int		is_slice = iptr->isn_type == ISN_LISTSLICE
					    || iptr->isn_type == ISN_BLOBSLICE;
		    int		is_blob = iptr->isn_type == ISN_BLOBINDEX
					    || iptr->isn_type == ISN_BLOBSLICE;
		    varnumber_T	n1, n2;
		    typval_T	*val_tv;

		    // list index: list is at stack-2, index at stack-1
		    // list slice: list is at stack-3, indexes at stack-2 and
		    // stack-1
		    // Same for blob.
		    val_tv = is_slice ? STACK_TV_BOT(-3) : STACK_TV_BOT(-2);

		    tv = STACK_TV_BOT(-1);
		    n1 = n2 = tv->vval.v_number;
		    clear_tv(tv);

		    if (is_slice)
		    {
			tv = STACK_TV_BOT(-2);
			n1 = tv->vval.v_number;
			clear_tv(tv);
		    }

		    ectx.ec_stack.ga_len -= is_slice ? 2 : 1;
		    tv = STACK_TV_BOT(-1);
		    SOURCING_LNUM = iptr->isn_lnum;
		    if (is_blob)
		    {
			if (blob_slice_or_index(val_tv->vval.v_blob, is_slice,
						    n1, n2, FALSE, tv) == FAIL)
			    goto on_error;
		    }
		    else
		    {
			if (list_slice_or_index(val_tv->vval.v_list, is_slice,
					      n1, n2, FALSE, tv, TRUE) == FAIL)
			    goto on_error;
		    }
		}
		break;

	    case ISN_ANYINDEX:
	    case ISN_ANYSLICE:
		{
		    int		is_slice = iptr->isn_type == ISN_ANYSLICE;
		    typval_T	*var1, *var2;
		    int		res;

		    // index: composite is at stack-2, index at stack-1
		    // slice: composite is at stack-3, indexes at stack-2 and
		    // stack-1
		    tv = is_slice ? STACK_TV_BOT(-3) : STACK_TV_BOT(-2);
		    SOURCING_LNUM = iptr->isn_lnum;
		    if (check_can_index(tv, TRUE, TRUE) == FAIL)
			goto on_error;
		    var1 = is_slice ? STACK_TV_BOT(-2) : STACK_TV_BOT(-1);
		    var2 = is_slice ? STACK_TV_BOT(-1) : NULL;
		    res = eval_index_inner(tv, is_slice, var1, var2,
							FALSE, NULL, -1, TRUE);
		    clear_tv(var1);
		    if (is_slice)
			clear_tv(var2);
		    ectx.ec_stack.ga_len -= is_slice ? 2 : 1;
		    if (res == FAIL)
			goto on_error;
		}
		break;

	    case ISN_SLICE:
		{
		    list_T	*list;
		    int		count = iptr->isn_arg.number;

		    // type will have been checked to be a list
		    tv = STACK_TV_BOT(-1);
		    list = tv->vval.v_list;

		    // no error for short list, expect it to be checked earlier
		    if (list != NULL && list->lv_len >= count)
		    {
			list_T	*newlist = list_slice(list,
						      count, list->lv_len - 1);

			if (newlist != NULL)
			{
			    list_unref(list);
			    tv->vval.v_list = newlist;
			    ++newlist->lv_refcount;
			}
		    }
		}
		break;

	    case ISN_GETITEM:
		{
		    listitem_T	*li;
		    int		index = iptr->isn_arg.number;

		    // Get list item: list is at stack-1, push item.
		    // List type and length is checked for when compiling.
		    tv = STACK_TV_BOT(-1);
		    li = list_find(tv->vval.v_list, index);

		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    ++ectx.ec_stack.ga_len;
		    copy_tv(&li->li_tv, STACK_TV_BOT(-1));

		    // Useful when used in unpack assignment.  Reset at
		    // ISN_DROP.
		    where.wt_index = index + 1;
		    where.wt_variable = TRUE;
		}
		break;

	    case ISN_MEMBER:
		{
		    dict_T	*dict;
		    char_u	*key;
		    dictitem_T	*di;
		    typval_T	temp_tv;

		    // dict member: dict is at stack-2, key at stack-1
		    tv = STACK_TV_BOT(-2);
		    // no need to check for VAR_DICT, CHECKTYPE will check.
		    dict = tv->vval.v_dict;

		    tv = STACK_TV_BOT(-1);
		    // no need to check for VAR_STRING, 2STRING will check.
		    key = tv->vval.v_string;
		    if (key == NULL)
			key = (char_u *)"";

		    if ((di = dict_find(dict, key, -1)) == NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			semsg(_(e_dictkey), key);

			// If :silent! is used we will continue, make sure the
			// stack contents makes sense.
			clear_tv(tv);
			--ectx.ec_stack.ga_len;
			tv = STACK_TV_BOT(-1);
			clear_tv(tv);
			tv->v_type = VAR_NUMBER;
			tv->vval.v_number = 0;
			goto on_fatal_error;
		    }
		    clear_tv(tv);
		    --ectx.ec_stack.ga_len;
		    // Clear the dict only after getting the item, to avoid
		    // that it makes the item invalid.
		    tv = STACK_TV_BOT(-1);
		    temp_tv = *tv;
		    copy_tv(&di->di_tv, tv);
		    clear_tv(&temp_tv);
		}
		break;

	    // dict member with string key
	    case ISN_STRINGMEMBER:
		{
		    dict_T	*dict;
		    dictitem_T	*di;
		    typval_T	temp_tv;

		    tv = STACK_TV_BOT(-1);
		    if (tv->v_type != VAR_DICT || tv->vval.v_dict == NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			emsg(_(e_dictreq));
			goto on_error;
		    }
		    dict = tv->vval.v_dict;

		    if ((di = dict_find(dict, iptr->isn_arg.string, -1))
								       == NULL)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			semsg(_(e_dictkey), iptr->isn_arg.string);
			goto on_error;
		    }
		    // Clear the dict after getting the item, to avoid that it
		    // make the item invalid.
		    temp_tv = *tv;
		    copy_tv(&di->di_tv, tv);
		    clear_tv(&temp_tv);
		}
		break;

	    case ISN_NEGATENR:
		tv = STACK_TV_BOT(-1);
		if (tv->v_type != VAR_NUMBER
#ifdef FEAT_FLOAT
			&& tv->v_type != VAR_FLOAT
#endif
			)
		{
		    SOURCING_LNUM = iptr->isn_lnum;
		    emsg(_(e_number_exp));
		    goto on_error;
		}
#ifdef FEAT_FLOAT
		if (tv->v_type == VAR_FLOAT)
		    tv->vval.v_float = -tv->vval.v_float;
		else
#endif
		    tv->vval.v_number = -tv->vval.v_number;
		break;

	    case ISN_CHECKNR:
		{
		    int		error = FALSE;

		    tv = STACK_TV_BOT(-1);
		    SOURCING_LNUM = iptr->isn_lnum;
		    if (check_not_string(tv) == FAIL)
			goto on_error;
		    (void)tv_get_number_chk(tv, &error);
		    if (error)
			goto on_error;
		}
		break;

	    case ISN_CHECKTYPE:
		{
		    checktype_T *ct = &iptr->isn_arg.type;

		    tv = STACK_TV_BOT((int)ct->ct_off);
		    SOURCING_LNUM = iptr->isn_lnum;
		    if (!where.wt_variable)
			where.wt_index = ct->ct_arg_idx;
		    if (check_typval_type(ct->ct_type, tv, where) == FAIL)
			goto on_error;
		    if (!where.wt_variable)
			where.wt_index = 0;

		    // number 0 is FALSE, number 1 is TRUE
		    if (tv->v_type == VAR_NUMBER
			    && ct->ct_type->tt_type == VAR_BOOL
			    && (tv->vval.v_number == 0
						|| tv->vval.v_number == 1))
		    {
			tv->v_type = VAR_BOOL;
			tv->vval.v_number = tv->vval.v_number
						      ? VVAL_TRUE : VVAL_FALSE;
		    }
		}
		break;

	    case ISN_CHECKLEN:
		{
		    int	    min_len = iptr->isn_arg.checklen.cl_min_len;
		    list_T  *list = NULL;

		    tv = STACK_TV_BOT(-1);
		    if (tv->v_type == VAR_LIST)
			    list = tv->vval.v_list;
		    if (list == NULL || list->lv_len < min_len
			    || (list->lv_len > min_len
					&& !iptr->isn_arg.checklen.cl_more_OK))
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			semsg(_(e_expected_nr_items_but_got_nr),
				     min_len, list == NULL ? 0 : list->lv_len);
			goto on_error;
		    }
		}
		break;

	    case ISN_SETTYPE:
		{
		    checktype_T *ct = &iptr->isn_arg.type;

		    tv = STACK_TV_BOT(-1);
		    if (tv->v_type == VAR_DICT && tv->vval.v_dict != NULL)
		    {
			free_type(tv->vval.v_dict->dv_type);
			tv->vval.v_dict->dv_type = alloc_type(ct->ct_type);
		    }
		    else if (tv->v_type == VAR_LIST && tv->vval.v_list != NULL)
		    {
			free_type(tv->vval.v_list->lv_type);
			tv->vval.v_list->lv_type = alloc_type(ct->ct_type);
		    }
		}
		break;

	    case ISN_2BOOL:
	    case ISN_COND2BOOL:
		{
		    int n;
		    int error = FALSE;

		    tv = STACK_TV_BOT(-1);
		    if (iptr->isn_type == ISN_2BOOL)
		    {
			n = tv2bool(tv);
			if (iptr->isn_arg.number)  // invert
			    n = !n;
		    }
		    else
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			n = tv_get_bool_chk(tv, &error);
			if (error)
			    goto on_error;
		    }
		    clear_tv(tv);
		    tv->v_type = VAR_BOOL;
		    tv->vval.v_number = n ? VVAL_TRUE : VVAL_FALSE;
		}
		break;

	    case ISN_2STRING:
	    case ISN_2STRING_ANY:
		SOURCING_LNUM = iptr->isn_lnum;
		if (do_2string(STACK_TV_BOT(iptr->isn_arg.number),
			iptr->isn_type == ISN_2STRING_ANY) == FAIL)
			    goto on_error;
		break;

	    case ISN_RANGE:
		{
		    exarg_T	ea;
		    char	*errormsg;

		    ea.line2 = 0;
		    ea.addr_count = 0;
		    ea.addr_type = ADDR_LINES;
		    ea.cmd = iptr->isn_arg.string;
		    ea.skip = FALSE;
		    if (parse_cmd_address(&ea, &errormsg, FALSE) == FAIL)
			goto on_error;

		    if (GA_GROW(&ectx.ec_stack, 1) == FAIL)
			goto failed;
		    ++ectx.ec_stack.ga_len;
		    tv = STACK_TV_BOT(-1);
		    tv->v_type = VAR_NUMBER;
		    tv->v_lock = 0;
		    if (ea.addr_count == 0)
			tv->vval.v_number = curwin->w_cursor.lnum;
		    else
			tv->vval.v_number = ea.line2;
		}
		break;

	    case ISN_PUT:
		{
		    int		regname = iptr->isn_arg.put.put_regname;
		    linenr_T	lnum = iptr->isn_arg.put.put_lnum;
		    char_u	*expr = NULL;
		    int		dir = FORWARD;

		    if (lnum < -2)
		    {
			// line number was put on the stack by ISN_RANGE
			tv = STACK_TV_BOT(-1);
			curwin->w_cursor.lnum = tv->vval.v_number;
			if (lnum == LNUM_VARIABLE_RANGE_ABOVE)
			    dir = BACKWARD;
			--ectx.ec_stack.ga_len;
		    }
		    else if (lnum == -2)
			// :put! above cursor
			dir = BACKWARD;
		    else if (lnum >= 0)
			curwin->w_cursor.lnum = iptr->isn_arg.put.put_lnum;

		    if (regname == '=')
		    {
			tv = STACK_TV_BOT(-1);
			if (tv->v_type == VAR_STRING)
			    expr = tv->vval.v_string;
			else
			{
			    expr = typval2string(tv, TRUE); // allocates value
			    clear_tv(tv);
			}
			--ectx.ec_stack.ga_len;
		    }
		    check_cursor();
		    do_put(regname, expr, dir, 1L, PUT_LINE|PUT_CURSLINE);
		    vim_free(expr);
		}
		break;

	    case ISN_CMDMOD:
		funclocal.floc_save_cmdmod = cmdmod;
		funclocal.floc_restore_cmdmod = TRUE;
		funclocal.floc_restore_cmdmod_stacklen = ectx.ec_stack.ga_len;
		cmdmod = *iptr->isn_arg.cmdmod.cf_cmdmod;
		apply_cmdmod(&cmdmod);
		break;

	    case ISN_CMDMOD_REV:
		// filter regprog is owned by the instruction, don't free it
		cmdmod.cmod_filter_regmatch.regprog = NULL;
		undo_cmdmod(&cmdmod);
		cmdmod = funclocal.floc_save_cmdmod;
		funclocal.floc_restore_cmdmod = FALSE;
		break;

	    case ISN_UNPACK:
		{
		    int		count = iptr->isn_arg.unpack.unp_count;
		    int		semicolon = iptr->isn_arg.unpack.unp_semicolon;
		    list_T	*l;
		    listitem_T	*li;
		    int		i;

		    // Check there is a valid list to unpack.
		    tv = STACK_TV_BOT(-1);
		    if (tv->v_type != VAR_LIST)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			emsg(_(e_for_argument_must_be_sequence_of_lists));
			goto on_error;
		    }
		    l = tv->vval.v_list;
		    if (l == NULL
				|| l->lv_len < (semicolon ? count - 1 : count))
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			emsg(_(e_list_value_does_not_have_enough_items));
			goto on_error;
		    }
		    else if (!semicolon && l->lv_len > count)
		    {
			SOURCING_LNUM = iptr->isn_lnum;
			emsg(_(e_list_value_has_more_items_than_targets));
			goto on_error;
		    }

		    CHECK_LIST_MATERIALIZE(l);
		    if (GA_GROW(&ectx.ec_stack, count - 1) == FAIL)
			goto failed;
		    ectx.ec_stack.ga_len += count - 1;

		    // Variable after semicolon gets a list with the remaining
		    // items.
		    if (semicolon)
		    {
			list_T	*rem_list =
				  list_alloc_with_items(l->lv_len - count + 1);

			if (rem_list == NULL)
			    goto failed;
			tv = STACK_TV_BOT(-count);
			tv->vval.v_list = rem_list;
			++rem_list->lv_refcount;
			tv->v_lock = 0;
			li = l->lv_first;
			for (i = 0; i < count - 1; ++i)
			    li = li->li_next;
			for (i = 0; li != NULL; ++i)
			{
			    list_set_item(rem_list, i, &li->li_tv);
			    li = li->li_next;
			}
			--count;
		    }

		    // Produce the values in reverse order, first item last.
		    li = l->lv_first;
		    for (i = 0; i < count; ++i)
		    {
			tv = STACK_TV_BOT(-i - 1);
			copy_tv(&li->li_tv, tv);
			li = li->li_next;
		    }

		    list_unref(l);
		}
		break;

	    case ISN_PROF_START:
	    case ISN_PROF_END:
		{
#ifdef FEAT_PROFILE
		    funccall_T cookie;
		    ufunc_T	    *cur_ufunc =
				    (((dfunc_T *)def_functions.ga_data)
						 + ectx.ec_dfunc_idx)->df_ufunc;

		    cookie.func = cur_ufunc;
		    if (iptr->isn_type == ISN_PROF_START)
		    {
			func_line_start(&cookie, iptr->isn_lnum);
			// if we get here the instruction is executed
			func_line_exec(&cookie);
		    }
		    else
			func_line_end(&cookie);
#endif
		}
		break;

	    case ISN_SHUFFLE:
		{
		    typval_T	tmp_tv;
		    int		item = iptr->isn_arg.shuffle.shfl_item;
		    int		up = iptr->isn_arg.shuffle.shfl_up;

		    tmp_tv = *STACK_TV_BOT(-item);
		    for ( ; up > 0 && item > 1; --up)
		    {
			*STACK_TV_BOT(-item) = *STACK_TV_BOT(-item + 1);
			--item;
		    }
		    *STACK_TV_BOT(-item) = tmp_tv;
		}
		break;

	    case ISN_DROP:
		--ectx.ec_stack.ga_len;
		clear_tv(STACK_TV_BOT(0));
		where.wt_index = 0;
		where.wt_variable = FALSE;
		break;
	}
	continue;

func_return:
	// Restore previous function. If the frame pointer is where we started
	// then there is none and we are done.
	if (ectx.ec_frame_idx == initial_frame_idx)
	    goto done;

	if (func_return(&funclocal, &ectx) == FAIL)
	    // only fails when out of memory
	    goto failed;
	continue;

on_error:
	// Jump here for an error that does not require aborting execution.
	// If "emsg_silent" is set then ignore the error, unless it was set
	// when calling the function.
	if (did_emsg_cumul + did_emsg == did_emsg_before
					   && emsg_silent && did_emsg_def == 0)
	{
	    // If a sequence of instructions causes an error while ":silent!"
	    // was used, restore the stack length and jump ahead to restoring
	    // the cmdmod.
	    if (funclocal.floc_restore_cmdmod)
	    {
		while (ectx.ec_stack.ga_len
				      > funclocal.floc_restore_cmdmod_stacklen)
		{
		    --ectx.ec_stack.ga_len;
		    clear_tv(STACK_TV_BOT(0));
		}
		while (ectx.ec_instr[ectx.ec_iidx].isn_type != ISN_CMDMOD_REV)
		    ++ectx.ec_iidx;
	    }
	    continue;
	}
on_fatal_error:
	// Jump here for an error that messes up the stack.
	// If we are not inside a try-catch started here, abort execution.
	if (trylevel <= trylevel_at_start)
	    goto failed;
    }

done:
    // function finished, get result from the stack.
    if (ufunc->uf_ret_type == &t_void)
    {
	rettv->v_type = VAR_VOID;
    }
    else
    {
	tv = STACK_TV_BOT(-1);
	*rettv = *tv;
	tv->v_type = VAR_UNKNOWN;
    }
    ret = OK;

failed:
    // When failed need to unwind the call stack.
    while (ectx.ec_frame_idx != initial_frame_idx)
	func_return(&funclocal, &ectx);

    // Deal with any remaining closures, they may be in use somewhere.
    if (ectx.ec_funcrefs.ga_len > 0)
    {
	handle_closure_in_use(&ectx, FALSE);
	ga_clear(&ectx.ec_funcrefs);  // TODO: should not be needed?
    }

    estack_pop();
    current_sctx = save_current_sctx;

    // TODO: when is it safe to delete the function if it is no longer used?
    --ufunc->uf_calls;

    if (*msg_list != NULL && saved_msg_list != NULL)
    {
	msglist_T **plist = saved_msg_list;

	// Append entries from the current msg_list (uncaught exceptions) to
	// the saved msg_list.
	while (*plist != NULL)
	    plist = &(*plist)->next;

	*plist = *msg_list;
    }
    msg_list = saved_msg_list;

    if (funclocal.floc_restore_cmdmod)
    {
	cmdmod.cmod_filter_regmatch.regprog = NULL;
	undo_cmdmod(&cmdmod);
	cmdmod = funclocal.floc_save_cmdmod;
    }
    emsg_silent_def = save_emsg_silent_def;
    did_emsg_def += save_did_emsg_def;

failed_early:
    // Free all local variables, but not arguments.
    for (idx = 0; idx < ectx.ec_stack.ga_len; ++idx)
	clear_tv(STACK_TV(idx));

    vim_free(ectx.ec_stack.ga_data);
    vim_free(ectx.ec_trystack.ga_data);

    while (ectx.ec_outer != NULL)
    {
	outer_T	    *up = ectx.ec_outer->out_up_is_copy
						? NULL : ectx.ec_outer->out_up;

	vim_free(ectx.ec_outer);
	ectx.ec_outer = up;
    }

    // Not sure if this is necessary.
    suppress_errthrow = save_suppress_errthrow;

    if (ret != OK && did_emsg_cumul + did_emsg == did_emsg_before)
	semsg(_(e_unknown_error_while_executing_str),
						   printable_func_name(ufunc));
    funcdepth_restore(orig_funcdepth);
    return ret;
}

/*
 * ":disassemble".
 * We don't really need this at runtime, but we do have tests that require it,
 * so always include this.
 */
    void
ex_disassemble(exarg_T *eap)
{
    char_u	*arg = eap->arg;
    char_u	*fname;
    ufunc_T	*ufunc;
    dfunc_T	*dfunc;
    isn_T	*instr;
    int		instr_count;
    int		current;
    int		line_idx = 0;
    int		prev_current = 0;
    int		is_global = FALSE;

    if (STRNCMP(arg, "<lambda>", 8) == 0)
    {
	arg += 8;
	(void)getdigits(&arg);
	fname = vim_strnsave(eap->arg, arg - eap->arg);
    }
    else
	fname = trans_function_name(&arg, &is_global, FALSE,
		      TFN_INT | TFN_QUIET | TFN_NO_AUTOLOAD, NULL, NULL, NULL);
    if (fname == NULL)
    {
	semsg(_(e_invarg2), eap->arg);
	return;
    }

    ufunc = find_func(fname, is_global, NULL);
    if (ufunc == NULL)
    {
	char_u *p = untrans_function_name(fname);

	if (p != NULL)
	    // Try again without making it script-local.
	    ufunc = find_func(p, FALSE, NULL);
    }
    vim_free(fname);
    if (ufunc == NULL)
    {
	semsg(_(e_cannot_find_function_str), eap->arg);
	return;
    }
    if (func_needs_compiling(ufunc, eap->forceit)
	    && compile_def_function(ufunc, FALSE, eap->forceit, NULL) == FAIL)
	return;
    if (ufunc->uf_def_status != UF_COMPILED)
    {
	semsg(_(e_function_is_not_compiled_str), eap->arg);
	return;
    }
    if (ufunc->uf_name_exp != NULL)
	msg((char *)ufunc->uf_name_exp);
    else
	msg((char *)ufunc->uf_name);

    dfunc = ((dfunc_T *)def_functions.ga_data) + ufunc->uf_dfunc_idx;
#ifdef FEAT_PROFILE
    instr = eap->forceit ? dfunc->df_instr_prof : dfunc->df_instr;
    instr_count = eap->forceit ? dfunc->df_instr_prof_count
						       : dfunc->df_instr_count;
#else
    instr = dfunc->df_instr;
    instr_count = dfunc->df_instr_count;
#endif
    for (current = 0; current < instr_count; ++current)
    {
	isn_T	    *iptr = &instr[current];
	char	    *line;

	while (line_idx < iptr->isn_lnum && line_idx < ufunc->uf_lines.ga_len)
	{
	    if (current > prev_current)
	    {
		msg_puts("\n\n");
		prev_current = current;
	    }
	    line = ((char **)ufunc->uf_lines.ga_data)[line_idx++];
	    if (line != NULL)
		msg(line);
	}

	switch (iptr->isn_type)
	{
	    case ISN_EXEC:
		smsg("%4d EXEC %s", current, iptr->isn_arg.string);
		break;
	    case ISN_EXECCONCAT:
		smsg("%4d EXECCONCAT %lld", current,
					      (varnumber_T)iptr->isn_arg.number);
		break;
	    case ISN_ECHO:
		{
		    echo_T *echo = &iptr->isn_arg.echo;

		    smsg("%4d %s %d", current,
			    echo->echo_with_white ? "ECHO" : "ECHON",
			    echo->echo_count);
		}
		break;
	    case ISN_EXECUTE:
		smsg("%4d EXECUTE %lld", current,
					    (varnumber_T)(iptr->isn_arg.number));
		break;
	    case ISN_ECHOMSG:
		smsg("%4d ECHOMSG %lld", current,
					    (varnumber_T)(iptr->isn_arg.number));
		break;
	    case ISN_ECHOERR:
		smsg("%4d ECHOERR %lld", current,
					    (varnumber_T)(iptr->isn_arg.number));
		break;
	    case ISN_LOAD:
		{
		    if (iptr->isn_arg.number < 0)
			smsg("%4d LOAD arg[%lld]", current,
				(varnumber_T)(iptr->isn_arg.number
							  + STACK_FRAME_SIZE));
		    else
			smsg("%4d LOAD $%lld", current,
					  (varnumber_T)(iptr->isn_arg.number));
		}
		break;
	    case ISN_LOADOUTER:
		{
		    if (iptr->isn_arg.number < 0)
			smsg("%4d LOADOUTER level %d arg[%d]", current,
				iptr->isn_arg.outer.outer_depth,
				iptr->isn_arg.outer.outer_idx
							  + STACK_FRAME_SIZE);
		    else
			smsg("%4d LOADOUTER level %d $%d", current,
					      iptr->isn_arg.outer.outer_depth,
					      iptr->isn_arg.outer.outer_idx);
		}
		break;
	    case ISN_LOADV:
		smsg("%4d LOADV v:%s", current,
				       get_vim_var_name(iptr->isn_arg.number));
		break;
	    case ISN_LOADSCRIPT:
		{
		    scriptref_T	*sref = iptr->isn_arg.script.scriptref;
		    scriptitem_T *si = SCRIPT_ITEM(sref->sref_sid);
		    svar_T *sv = ((svar_T *)si->sn_var_vals.ga_data)
							      + sref->sref_idx;

		    smsg("%4d LOADSCRIPT %s-%d from %s", current,
					    sv->sv_name,
					    sref->sref_idx,
					    si->sn_name);
		}
		break;
	    case ISN_LOADS:
		{
		    scriptitem_T *si = SCRIPT_ITEM(
					       iptr->isn_arg.loadstore.ls_sid);

		    smsg("%4d LOADS s:%s from %s", current,
				 iptr->isn_arg.loadstore.ls_name, si->sn_name);
		}
		break;
	    case ISN_LOADAUTO:
		smsg("%4d LOADAUTO %s", current, iptr->isn_arg.string);
		break;
	    case ISN_LOADG:
		smsg("%4d LOADG g:%s", current, iptr->isn_arg.string);
		break;
	    case ISN_LOADB:
		smsg("%4d LOADB b:%s", current, iptr->isn_arg.string);
		break;
	    case ISN_LOADW:
		smsg("%4d LOADW w:%s", current, iptr->isn_arg.string);
		break;
	    case ISN_LOADT:
		smsg("%4d LOADT t:%s", current, iptr->isn_arg.string);
		break;
	    case ISN_LOADGDICT:
		smsg("%4d LOAD g:", current);
		break;
	    case ISN_LOADBDICT:
		smsg("%4d LOAD b:", current);
		break;
	    case ISN_LOADWDICT:
		smsg("%4d LOAD w:", current);
		break;
	    case ISN_LOADTDICT:
		smsg("%4d LOAD t:", current);
		break;
	    case ISN_LOADOPT:
		smsg("%4d LOADOPT %s", current, iptr->isn_arg.string);
		break;
	    case ISN_LOADENV:
		smsg("%4d LOADENV %s", current, iptr->isn_arg.string);
		break;
	    case ISN_LOADREG:
		smsg("%4d LOADREG @%c", current, (int)(iptr->isn_arg.number));
		break;

	    case ISN_STORE:
		if (iptr->isn_arg.number < 0)
		    smsg("%4d STORE arg[%lld]", current,
				      iptr->isn_arg.number + STACK_FRAME_SIZE);
		else
		    smsg("%4d STORE $%lld", current, iptr->isn_arg.number);
		break;
	    case ISN_STOREOUTER:
		{
		if (iptr->isn_arg.number < 0)
		    smsg("%4d STOREOUTEr level %d arg[%d]", current,
			    iptr->isn_arg.outer.outer_depth,
			    iptr->isn_arg.outer.outer_idx + STACK_FRAME_SIZE);
		else
		    smsg("%4d STOREOUTER level %d $%d", current,
			    iptr->isn_arg.outer.outer_depth,
			    iptr->isn_arg.outer.outer_idx);
		}
		break;
	    case ISN_STOREV:
		smsg("%4d STOREV v:%s", current,
				       get_vim_var_name(iptr->isn_arg.number));
		break;
	    case ISN_STOREAUTO:
		smsg("%4d STOREAUTO %s", current, iptr->isn_arg.string);
		break;
	    case ISN_STOREG:
		smsg("%4d STOREG %s", current, iptr->isn_arg.string);
		break;
	    case ISN_STOREB:
		smsg("%4d STOREB %s", current, iptr->isn_arg.string);
		break;
	    case ISN_STOREW:
		smsg("%4d STOREW %s", current, iptr->isn_arg.string);
		break;
	    case ISN_STORET:
		smsg("%4d STORET %s", current, iptr->isn_arg.string);
		break;
	    case ISN_STORES:
		{
		    scriptitem_T *si = SCRIPT_ITEM(
					       iptr->isn_arg.loadstore.ls_sid);

		    smsg("%4d STORES %s in %s", current,
				 iptr->isn_arg.loadstore.ls_name, si->sn_name);
		}
		break;
	    case ISN_STORESCRIPT:
		{
		    scriptref_T	*sref = iptr->isn_arg.script.scriptref;
		    scriptitem_T *si = SCRIPT_ITEM(sref->sref_sid);
		    svar_T *sv = ((svar_T *)si->sn_var_vals.ga_data)
							      + sref->sref_idx;

		    smsg("%4d STORESCRIPT %s-%d in %s", current,
					     sv->sv_name,
					     sref->sref_idx,
					     si->sn_name);
		}
		break;
	    case ISN_STOREOPT:
		smsg("%4d STOREOPT &%s", current,
					       iptr->isn_arg.storeopt.so_name);
		break;
	    case ISN_STOREENV:
		smsg("%4d STOREENV $%s", current, iptr->isn_arg.string);
		break;
	    case ISN_STOREREG:
		smsg("%4d STOREREG @%c", current, (int)iptr->isn_arg.number);
		break;
	    case ISN_STORENR:
		smsg("%4d STORE %lld in $%d", current,
				iptr->isn_arg.storenr.stnr_val,
				iptr->isn_arg.storenr.stnr_idx);
		break;

	    case ISN_STOREINDEX:
		switch (iptr->isn_arg.vartype)
		{
		    case VAR_LIST:
			    smsg("%4d STORELIST", current);
			    break;
		    case VAR_DICT:
			    smsg("%4d STOREDICT", current);
			    break;
		    case VAR_ANY:
			    smsg("%4d STOREINDEX", current);
			    break;
		    default: break;
		}
		break;

	    // constants
	    case ISN_PUSHNR:
		smsg("%4d PUSHNR %lld", current,
					    (varnumber_T)(iptr->isn_arg.number));
		break;
	    case ISN_PUSHBOOL:
	    case ISN_PUSHSPEC:
		smsg("%4d PUSH %s", current,
				   get_var_special_name(iptr->isn_arg.number));
		break;
	    case ISN_PUSHF:
#ifdef FEAT_FLOAT
		smsg("%4d PUSHF %g", current, iptr->isn_arg.fnumber);
#endif
		break;
	    case ISN_PUSHS:
		smsg("%4d PUSHS \"%s\"", current, iptr->isn_arg.string);
		break;
	    case ISN_PUSHBLOB:
		{
		    char_u	*r;
		    char_u	numbuf[NUMBUFLEN];
		    char_u	*tofree;

		    r = blob2string(iptr->isn_arg.blob, &tofree, numbuf);
		    smsg("%4d PUSHBLOB %s", current, r);
		    vim_free(tofree);
		}
		break;
	    case ISN_PUSHFUNC:
		{
		    char *name = (char *)iptr->isn_arg.string;

		    smsg("%4d PUSHFUNC \"%s\"", current,
					       name == NULL ? "[none]" : name);
		}
		break;
	    case ISN_PUSHCHANNEL:
#ifdef FEAT_JOB_CHANNEL
		{
		    channel_T *channel = iptr->isn_arg.channel;

		    smsg("%4d PUSHCHANNEL %d", current,
					 channel == NULL ? 0 : channel->ch_id);
		}
#endif
		break;
	    case ISN_PUSHJOB:
#ifdef FEAT_JOB_CHANNEL
		{
		    typval_T	tv;
		    char_u	*name;

		    tv.v_type = VAR_JOB;
		    tv.vval.v_job = iptr->isn_arg.job;
		    name = tv_get_string(&tv);
		    smsg("%4d PUSHJOB \"%s\"", current, name);
		}
#endif
		break;
	    case ISN_PUSHEXC:
		smsg("%4d PUSH v:exception", current);
		break;
	    case ISN_UNLET:
		smsg("%4d UNLET%s %s", current,
			iptr->isn_arg.unlet.ul_forceit ? "!" : "",
			iptr->isn_arg.unlet.ul_name);
		break;
	    case ISN_UNLETENV:
		smsg("%4d UNLETENV%s $%s", current,
			iptr->isn_arg.unlet.ul_forceit ? "!" : "",
			iptr->isn_arg.unlet.ul_name);
		break;
	    case ISN_UNLETINDEX:
		smsg("%4d UNLETINDEX", current);
		break;
	    case ISN_UNLETRANGE:
		smsg("%4d UNLETRANGE", current);
		break;
	    case ISN_LOCKCONST:
		smsg("%4d LOCKCONST", current);
		break;
	    case ISN_NEWLIST:
		smsg("%4d NEWLIST size %lld", current,
					    (varnumber_T)(iptr->isn_arg.number));
		break;
	    case ISN_NEWDICT:
		smsg("%4d NEWDICT size %lld", current,
					    (varnumber_T)(iptr->isn_arg.number));
		break;

	    // function call
	    case ISN_BCALL:
		{
		    cbfunc_T	*cbfunc = &iptr->isn_arg.bfunc;

		    smsg("%4d BCALL %s(argc %d)", current,
			    internal_func_name(cbfunc->cbf_idx),
			    cbfunc->cbf_argcount);
		}
		break;
	    case ISN_DCALL:
		{
		    cdfunc_T	*cdfunc = &iptr->isn_arg.dfunc;
		    dfunc_T	*df = ((dfunc_T *)def_functions.ga_data)
							     + cdfunc->cdf_idx;

		    smsg("%4d DCALL %s(argc %d)", current,
			    df->df_ufunc->uf_name_exp != NULL
				? df->df_ufunc->uf_name_exp
				: df->df_ufunc->uf_name, cdfunc->cdf_argcount);
		}
		break;
	    case ISN_UCALL:
		{
		    cufunc_T	*cufunc = &iptr->isn_arg.ufunc;

		    smsg("%4d UCALL %s(argc %d)", current,
				       cufunc->cuf_name, cufunc->cuf_argcount);
		}
		break;
	    case ISN_PCALL:
		{
		    cpfunc_T	*cpfunc = &iptr->isn_arg.pfunc;

		    smsg("%4d PCALL%s (argc %d)", current,
			   cpfunc->cpf_top ? " top" : "", cpfunc->cpf_argcount);
		}
		break;
	    case ISN_PCALL_END:
		smsg("%4d PCALL end", current);
		break;
	    case ISN_RETURN:
		smsg("%4d RETURN", current);
		break;
	    case ISN_RETURN_ZERO:
		smsg("%4d RETURN 0", current);
		break;
	    case ISN_FUNCREF:
		{
		    funcref_T	*funcref = &iptr->isn_arg.funcref;
		    dfunc_T	*df = ((dfunc_T *)def_functions.ga_data)
							    + funcref->fr_func;

		    smsg("%4d FUNCREF %s", current, df->df_ufunc->uf_name);
		}
		break;

	    case ISN_NEWFUNC:
		{
		    newfunc_T	*newfunc = &iptr->isn_arg.newfunc;

		    smsg("%4d NEWFUNC %s %s", current,
				       newfunc->nf_lambda, newfunc->nf_global);
		}
		break;

	    case ISN_DEF:
		{
		    char_u *name = iptr->isn_arg.string;

		    smsg("%4d DEF %s", current,
					   name == NULL ? (char_u *)"" : name);
		}
		break;

	    case ISN_JUMP:
		{
		    char *when = "?";

		    switch (iptr->isn_arg.jump.jump_when)
		    {
			case JUMP_ALWAYS:
			    when = "JUMP";
			    break;
			case JUMP_AND_KEEP_IF_TRUE:
			    when = "JUMP_AND_KEEP_IF_TRUE";
			    break;
			case JUMP_IF_FALSE:
			    when = "JUMP_IF_FALSE";
			    break;
			case JUMP_AND_KEEP_IF_FALSE:
			    when = "JUMP_AND_KEEP_IF_FALSE";
			    break;
			case JUMP_IF_COND_FALSE:
			    when = "JUMP_IF_COND_FALSE";
			    break;
			case JUMP_IF_COND_TRUE:
			    when = "JUMP_IF_COND_TRUE";
			    break;
		    }
		    smsg("%4d %s -> %d", current, when,
						iptr->isn_arg.jump.jump_where);
		}
		break;

	    case ISN_JUMP_IF_ARG_SET:
		smsg("%4d JUMP_IF_ARG_SET arg[%d] -> %d", current,
			 iptr->isn_arg.jumparg.jump_arg_off + STACK_FRAME_SIZE,
						iptr->isn_arg.jump.jump_where);
		break;

	    case ISN_FOR:
		{
		    forloop_T *forloop = &iptr->isn_arg.forloop;

		    smsg("%4d FOR $%d -> %d", current,
					   forloop->for_idx, forloop->for_end);
		}
		break;

	    case ISN_TRY:
		{
		    try_T *try = &iptr->isn_arg.try;

		    if (try->try_ref->try_finally == 0)
			smsg("%4d TRY catch -> %d, endtry -> %d",
				current,
				try->try_ref->try_catch,
				try->try_ref->try_endtry);
		    else
			smsg("%4d TRY catch -> %d, finally -> %d, endtry -> %d",
				current,
				try->try_ref->try_catch,
				try->try_ref->try_finally,
				try->try_ref->try_endtry);
		}
		break;
	    case ISN_CATCH:
		// TODO
		smsg("%4d CATCH", current);
		break;
	    case ISN_TRYCONT:
		{
		    trycont_T *trycont = &iptr->isn_arg.trycont;

		    smsg("%4d TRY-CONTINUE %d level%s -> %d", current,
				      trycont->tct_levels,
				      trycont->tct_levels == 1 ? "" : "s",
				      trycont->tct_where);
		}
		break;
	    case ISN_FINALLY:
		smsg("%4d FINALLY", current);
		break;
	    case ISN_ENDTRY:
		smsg("%4d ENDTRY", current);
		break;
	    case ISN_THROW:
		smsg("%4d THROW", current);
		break;

	    // expression operations on number
	    case ISN_OPNR:
	    case ISN_OPFLOAT:
	    case ISN_OPANY:
		{
		    char *what;
		    char *ins;

		    switch (iptr->isn_arg.op.op_type)
		    {
			case EXPR_MULT: what = "*"; break;
			case EXPR_DIV: what = "/"; break;
			case EXPR_REM: what = "%"; break;
			case EXPR_SUB: what = "-"; break;
			case EXPR_ADD: what = "+"; break;
			default:       what = "???"; break;
		    }
		    switch (iptr->isn_type)
		    {
			case ISN_OPNR: ins = "OPNR"; break;
			case ISN_OPFLOAT: ins = "OPFLOAT"; break;
			case ISN_OPANY: ins = "OPANY"; break;
			default: ins = "???"; break;
		    }
		    smsg("%4d %s %s", current, ins, what);
		}
		break;

	    case ISN_COMPAREBOOL:
	    case ISN_COMPARESPECIAL:
	    case ISN_COMPARENR:
	    case ISN_COMPAREFLOAT:
	    case ISN_COMPARESTRING:
	    case ISN_COMPAREBLOB:
	    case ISN_COMPARELIST:
	    case ISN_COMPAREDICT:
	    case ISN_COMPAREFUNC:
	    case ISN_COMPAREANY:
		   {
		       char *p;
		       char buf[10];
		       char *type;

		       switch (iptr->isn_arg.op.op_type)
		       {
			   case EXPR_EQUAL:	 p = "=="; break;
			   case EXPR_NEQUAL:    p = "!="; break;
			   case EXPR_GREATER:   p = ">"; break;
			   case EXPR_GEQUAL:    p = ">="; break;
			   case EXPR_SMALLER:   p = "<"; break;
			   case EXPR_SEQUAL:    p = "<="; break;
			   case EXPR_MATCH:	 p = "=~"; break;
			   case EXPR_IS:	 p = "is"; break;
			   case EXPR_ISNOT:	 p = "isnot"; break;
			   case EXPR_NOMATCH:	 p = "!~"; break;
			   default:  p = "???"; break;
		       }
		       STRCPY(buf, p);
		       if (iptr->isn_arg.op.op_ic == TRUE)
			   strcat(buf, "?");
		       switch(iptr->isn_type)
		       {
			   case ISN_COMPAREBOOL: type = "COMPAREBOOL"; break;
			   case ISN_COMPARESPECIAL:
						 type = "COMPARESPECIAL"; break;
			   case ISN_COMPARENR: type = "COMPARENR"; break;
			   case ISN_COMPAREFLOAT: type = "COMPAREFLOAT"; break;
			   case ISN_COMPARESTRING:
						  type = "COMPARESTRING"; break;
			   case ISN_COMPAREBLOB: type = "COMPAREBLOB"; break;
			   case ISN_COMPARELIST: type = "COMPARELIST"; break;
			   case ISN_COMPAREDICT: type = "COMPAREDICT"; break;
			   case ISN_COMPAREFUNC: type = "COMPAREFUNC"; break;
			   case ISN_COMPAREANY: type = "COMPAREANY"; break;
			   default: type = "???"; break;
		       }

		       smsg("%4d %s %s", current, type, buf);
		   }
		   break;

	    case ISN_ADDLIST: smsg("%4d ADDLIST", current); break;
	    case ISN_ADDBLOB: smsg("%4d ADDBLOB", current); break;

	    // expression operations
	    case ISN_CONCAT: smsg("%4d CONCAT", current); break;
	    case ISN_STRINDEX: smsg("%4d STRINDEX", current); break;
	    case ISN_STRSLICE: smsg("%4d STRSLICE", current); break;
	    case ISN_BLOBINDEX: smsg("%4d BLOBINDEX", current); break;
	    case ISN_BLOBSLICE: smsg("%4d BLOBSLICE", current); break;
	    case ISN_LISTAPPEND: smsg("%4d LISTAPPEND", current); break;
	    case ISN_BLOBAPPEND: smsg("%4d BLOBAPPEND", current); break;
	    case ISN_LISTINDEX: smsg("%4d LISTINDEX", current); break;
	    case ISN_LISTSLICE: smsg("%4d LISTSLICE", current); break;
	    case ISN_ANYINDEX: smsg("%4d ANYINDEX", current); break;
	    case ISN_ANYSLICE: smsg("%4d ANYSLICE", current); break;
	    case ISN_SLICE: smsg("%4d SLICE %lld",
					 current, iptr->isn_arg.number); break;
	    case ISN_GETITEM: smsg("%4d ITEM %lld",
					 current, iptr->isn_arg.number); break;
	    case ISN_MEMBER: smsg("%4d MEMBER", current); break;
	    case ISN_STRINGMEMBER: smsg("%4d MEMBER %s", current,
						  iptr->isn_arg.string); break;
	    case ISN_NEGATENR: smsg("%4d NEGATENR", current); break;

	    case ISN_CHECKNR: smsg("%4d CHECKNR", current); break;
	    case ISN_CHECKTYPE:
		  {
		      checktype_T *ct = &iptr->isn_arg.type;
		      char *tofree;

		      if (ct->ct_arg_idx == 0)
			  smsg("%4d CHECKTYPE %s stack[%d]", current,
					  type_name(ct->ct_type, &tofree),
					  (int)ct->ct_off);
		      else
			  smsg("%4d CHECKTYPE %s stack[%d] arg %d", current,
					  type_name(ct->ct_type, &tofree),
					  (int)ct->ct_off,
					  (int)ct->ct_arg_idx);
		      vim_free(tofree);
		      break;
		  }
	    case ISN_CHECKLEN: smsg("%4d CHECKLEN %s%d", current,
				iptr->isn_arg.checklen.cl_more_OK ? ">= " : "",
				iptr->isn_arg.checklen.cl_min_len);
			       break;
	    case ISN_SETTYPE:
		  {
		      char *tofree;

		      smsg("%4d SETTYPE %s", current,
			      type_name(iptr->isn_arg.type.ct_type, &tofree));
		      vim_free(tofree);
		      break;
		  }
	    case ISN_COND2BOOL: smsg("%4d COND2BOOL", current); break;
	    case ISN_2BOOL: if (iptr->isn_arg.number)
				smsg("%4d INVERT (!val)", current);
			    else
				smsg("%4d 2BOOL (!!val)", current);
			    break;
	    case ISN_2STRING: smsg("%4d 2STRING stack[%lld]", current,
					 (varnumber_T)(iptr->isn_arg.number));
			      break;
	    case ISN_2STRING_ANY: smsg("%4d 2STRING_ANY stack[%lld]", current,
					 (varnumber_T)(iptr->isn_arg.number));
			      break;
	    case ISN_RANGE: smsg("%4d RANGE %s", current, iptr->isn_arg.string);
			    break;
	    case ISN_PUT:
	        if (iptr->isn_arg.put.put_lnum == LNUM_VARIABLE_RANGE_ABOVE)
		    smsg("%4d PUT %c above range",
				       current, iptr->isn_arg.put.put_regname);
		else if (iptr->isn_arg.put.put_lnum == LNUM_VARIABLE_RANGE)
		    smsg("%4d PUT %c range",
				       current, iptr->isn_arg.put.put_regname);
		else
		    smsg("%4d PUT %c %ld", current,
						 iptr->isn_arg.put.put_regname,
					     (long)iptr->isn_arg.put.put_lnum);
		break;

		// TODO: summarize modifiers
	    case ISN_CMDMOD:
		{
		    char_u  *buf;
		    size_t  len = produce_cmdmods(
				  NULL, iptr->isn_arg.cmdmod.cf_cmdmod, FALSE);

		    buf = alloc(len + 1);
		    if (buf != NULL)
		    {
			(void)produce_cmdmods(
				   buf, iptr->isn_arg.cmdmod.cf_cmdmod, FALSE);
			smsg("%4d CMDMOD %s", current, buf);
			vim_free(buf);
		    }
		    break;
		}
	    case ISN_CMDMOD_REV: smsg("%4d CMDMOD_REV", current); break;

	    case ISN_PROF_START:
		 smsg("%4d PROFILE START line %d", current, iptr->isn_lnum);
		 break;

	    case ISN_PROF_END:
		smsg("%4d PROFILE END", current);
		break;

	    case ISN_UNPACK: smsg("%4d UNPACK %d%s", current,
			iptr->isn_arg.unpack.unp_count,
			iptr->isn_arg.unpack.unp_semicolon ? " semicolon" : "");
			      break;
	    case ISN_SHUFFLE: smsg("%4d SHUFFLE %d up %d", current,
					 iptr->isn_arg.shuffle.shfl_item,
					 iptr->isn_arg.shuffle.shfl_up);
			      break;
	    case ISN_DROP: smsg("%4d DROP", current); break;
	}

	out_flush();	    // output one line at a time
	ui_breakcheck();
	if (got_int)
	    break;
    }
}

/*
 * Return TRUE when "tv" is not falsy: non-zero, non-empty string, non-empty
 * list, etc.  Mostly like what JavaScript does, except that empty list and
 * empty dictionary are FALSE.
 */
    int
tv2bool(typval_T *tv)
{
    switch (tv->v_type)
    {
	case VAR_NUMBER:
	    return tv->vval.v_number != 0;
	case VAR_FLOAT:
#ifdef FEAT_FLOAT
	    return tv->vval.v_float != 0.0;
#else
	    break;
#endif
	case VAR_PARTIAL:
	    return tv->vval.v_partial != NULL;
	case VAR_FUNC:
	case VAR_STRING:
	    return tv->vval.v_string != NULL && *tv->vval.v_string != NUL;
	case VAR_LIST:
	    return tv->vval.v_list != NULL && tv->vval.v_list->lv_len > 0;
	case VAR_DICT:
	    return tv->vval.v_dict != NULL
				    && tv->vval.v_dict->dv_hashtab.ht_used > 0;
	case VAR_BOOL:
	case VAR_SPECIAL:
	    return tv->vval.v_number == VVAL_TRUE ? TRUE : FALSE;
	case VAR_JOB:
#ifdef FEAT_JOB_CHANNEL
	    return tv->vval.v_job != NULL;
#else
	    break;
#endif
	case VAR_CHANNEL:
#ifdef FEAT_JOB_CHANNEL
	    return tv->vval.v_channel != NULL;
#else
	    break;
#endif
	case VAR_BLOB:
	    return tv->vval.v_blob != NULL && tv->vval.v_blob->bv_ga.ga_len > 0;
	case VAR_UNKNOWN:
	case VAR_ANY:
	case VAR_VOID:
	    break;
    }
    return FALSE;
}

    void
emsg_using_string_as(typval_T *tv, int as_number)
{
    semsg(_(as_number ? e_using_string_as_number_str
						 : e_using_string_as_bool_str),
		       tv->vval.v_string == NULL
					   ? (char_u *)"" : tv->vval.v_string);
}

/*
 * If "tv" is a string give an error and return FAIL.
 */
    int
check_not_string(typval_T *tv)
{
    if (tv->v_type == VAR_STRING)
    {
	emsg_using_string_as(tv, TRUE);
	clear_tv(tv);
	return FAIL;
    }
    return OK;
}


#endif // FEAT_EVAL
