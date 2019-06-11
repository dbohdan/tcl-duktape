/*
 * Tcl bindings for Duktape.
 * Copyright (c) 2015, 2016, 2017, 2018, 2019
 * dbohdan and contributors listed in AUTHORS
 * This code is released under the terms of the MIT license. See the file
 * LICENSE for details.
 */
#include <tcl.h>
#include "duktape.h"

/* Package information. */

#define PACKAGE "duktape"
#define VERSION PACKAGE_VERSION

/* Namespace for the extension. */

#define NS "::" PACKAGE

/* Command names. */

#define INIT "::init"
#define MAKE_SAFE "::make-safe"
#define MAKE_UNSAFE "::make-unsafe"
#define CLOSE "::close"
#define EVAL "::eval"
#define TCL_FUNCTION "::tcl-function"
#define CALL_METHOD "::call-method"

/* Error messages. */

#define ERROR_TOKEN "can't parse token"
#define ERROR_ARG_LENGTH "argument must be a list of one or two elements"
#define ERROR_CREATE "can't create Duktape context"
#define ERROR_INVALID_INSTANCE "unable to locate instance"
#define ERROR_INVALID_INTERP "unable to locate interp"
#define ERROR_INVALID_STRING "unable to convert argument to string"
#define ERROR_INTERNAL_ARGS_ERROR "internal error: negative arguments?"
#define ERROR_INTERNAL_TCL_LAPPEND "internal error: lappend failed?"
#define ERROR_NOT_ALLOWED "action not permitted while safe"

/* Usage. */

#define USAGE_INIT "?-safe <boolean>?"
#define USAGE_MAKE_SAFE "token"
#define USAGE_MAKE_UNSAFE "token"
#define USAGE_CLOSE "token"
#define USAGE_EVAL "token code"
#define USAGE_TCL_FUNCTION "token name args body"
#define USAGE_CALL_METHOD "token method this ?{arg ?type?}? ..."

/* Data types. */

struct DuktapeData
{
    int counter;
    Tcl_HashTable table;
};

struct DuktapeInstanceData {
    Tcl_Interp *interp;
    duk_context *ctx;
    int isUnsafe;
};

#define DUKTCL_CDATA ((struct DuktapeData *) cdata)

/* Functions */

static duk_context *
parse_id(ClientData cdata, Tcl_Interp *interp, Tcl_Obj *const idobj, int del)
{
    struct DuktapeInstanceData *instanceData;
    duk_context *ctx;
    Tcl_HashEntry *hashPtr;

    hashPtr = Tcl_FindHashEntry(&DUKTCL_CDATA->table, Tcl_GetString(idobj));
    if (hashPtr == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_TOKEN, -1));
        return NULL;
    }
    instanceData = (struct DuktapeInstanceData *) Tcl_GetHashValue(hashPtr);
    if (!instanceData) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_INVALID_INSTANCE, -1));
        return(NULL);
    }
    ctx = instanceData->ctx;
    if (del) {
        Tcl_DeleteHashEntry(hashPtr);
        ckfree(instanceData);
    }
    return ctx;
}

static void
cleanup_interp(ClientData cdata, Tcl_Interp *interp)
{
    struct DuktapeInstanceData *instanceData;
    Tcl_HashEntry* hashPtr;
    Tcl_HashSearch search;
    duk_context* ctx;

    hashPtr = Tcl_FirstHashEntry(&DUKTCL_CDATA->table, &search);
    while (hashPtr != NULL) {
        instanceData = (struct DuktapeInstanceData *) Tcl_GetHashValue(hashPtr);
        ctx = instanceData->ctx;
        Tcl_SetHashValue(hashPtr, (ClientData) NULL);
        duk_destroy_heap(ctx);
        ckfree(instanceData);
        hashPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&DUKTCL_CDATA->table);
    ckfree((char *)DUKTCL_CDATA);
    return;
    /* UNREACH: Disable some warnings */
    interp = interp;
}

/*
 * Evaluate a Tcl string and return the result to JavaScript
 */
static duk_ret_t EvalTclFromJSWithInterp(Tcl_Interp *interp, duk_context *ctx) {
    Tcl_Obj *evalScript, *evalResult, *dukStringObj;
    duk_size_t dukStringLength;
    duk_idx_t numArgs;
    const char *dukString;
    char *evalResultString;
    int evalResultStringLength;
    int tclRet;
    int idx;

    numArgs = duk_get_top(ctx);
    if (numArgs < 0) {
        duk_push_error_object(ctx, DUK_ERR_ERROR, "%s", ERROR_INTERNAL_ARGS_ERROR);
        return(duk_throw(ctx));
    }

    evalScript = Tcl_NewListObj(0, NULL);
    for (idx = 0; idx < numArgs; idx++) {
        /*
         * XXX:TODO: Convert types ?
         *     object          => dict ?  or JSON ?
         *     array           => list ?  or JSON ?
         *     null/undefined  => empty string ?
         *     everything else => string
         */
        dukString = NULL;
        if (duk_check_type_mask(ctx, idx, DUK_TYPE_MASK_NULL | DUK_TYPE_MASK_UNDEFINED)) {
            dukString = "";
            dukStringLength = 0;
        }

        if (duk_is_buffer_data(ctx, idx)) {
            duk_buffer_to_string(ctx, idx);
        }

        if (duk_check_type(ctx, idx, DUK_TYPE_OBJECT)) {
            duk_json_encode(ctx, idx);
        }

        if (!dukString) {
            dukString = duk_safe_to_lstring(ctx, idx, &dukStringLength);
        }

        if (!dukString) {
            duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "%s", ERROR_INVALID_STRING);
            return(duk_throw(ctx));
        }

        /* XXX:TODO: Encoding */
        dukStringObj = Tcl_NewStringObj(dukString, dukStringLength);

        tclRet = Tcl_ListObjAppendElement(interp, evalScript, dukStringObj);
        if (tclRet != TCL_OK) {
            duk_push_error_object(ctx, DUK_ERR_ERROR, "%s", ERROR_INTERNAL_TCL_LAPPEND);
            return(duk_throw(ctx));
        }
    }
    for (idx = 0; idx < numArgs; idx++) {
        duk_pop(ctx);
    }

    tclRet = Tcl_EvalObjEx(interp, evalScript, 0);
    if (tclRet != TCL_OK) {
        duk_push_error_object(ctx, DUK_ERR_ERROR, "%s", Tcl_GetStringResult(interp));
        return(duk_throw(ctx));
    }

    evalResult = Tcl_GetObjResult(interp);

    evalResultString = Tcl_GetStringFromObj(evalResult, &evalResultStringLength);
    duk_push_lstring(ctx, evalResultString, evalResultStringLength);

    Tcl_FreeResult(interp);

    return(1);
}

static duk_ret_t EvalTclFromJS(duk_context *ctx) {
    struct DuktapeInstanceData *instanceData;
    duk_memory_functions funcs;
    Tcl_Interp *interp;

    duk_get_memory_functions(ctx, &funcs);
    instanceData = funcs.udata;

    if (!instanceData) {
        duk_push_error_object(ctx, DUK_ERR_ERROR, "%s", ERROR_INVALID_INSTANCE);
        return(duk_throw(ctx));
    }

    if (!instanceData->isUnsafe) {
        duk_push_error_object(ctx, DUK_ERR_ERROR, "%s", ERROR_NOT_ALLOWED);
        return(duk_throw(ctx));
    }

    interp = instanceData->interp;

    if (!interp) {
        duk_push_error_object(ctx, DUK_ERR_ERROR, "%s", ERROR_INVALID_INTERP);
        return(duk_throw(ctx));
    }

    return(EvalTclFromJSWithInterp(interp, ctx));
}

static void MakeContextUnsafe(duk_context *ctx) {
    struct DuktapeInstanceData *instanceData;
    duk_memory_functions funcs;

    duk_get_memory_functions(ctx, &funcs);
    instanceData = funcs.udata;

    instanceData->isUnsafe = 1;

    duk_push_global_object(ctx);           /* => [global] */
    duk_push_string(ctx, "Duktape");       /* => [global] ["duktape"] */
    duk_get_prop(ctx, -2);                 /* => [global] [duktape] */
    if (duk_is_object(ctx, -1)) {
        duk_push_string(ctx, "tcl");   /* => [global] [duktape] ["tcl"] */
        duk_push_object(ctx);          /* => [global] [duktape] ["tcl"] [object] */
        duk_push_string(ctx, "eval");  /* => [global] [duktape] ["tcl"] [object] ["eval"] */
        duk_push_c_function(ctx, EvalTclFromJS, DUK_VARARGS);
                                       /* => [global] [duktape] ["tcl"] [object] ["eval"] [function] */
        duk_put_prop(ctx, -3);         /* => [global] [duktape] ["tcl"] [object.eval=function] */
        duk_put_prop(ctx, -3);         /* => [global] [duktape.tcl=object] */
    }
    duk_pop(ctx);                          /* => [global] */
    duk_pop(ctx);                          /* => */

    return;
}

static void MakeContextSafe(duk_context *ctx) {
    struct DuktapeInstanceData *instanceData;
    duk_memory_functions funcs;

    duk_get_memory_functions(ctx, &funcs);
    instanceData = funcs.udata;

    instanceData->isUnsafe = 0;

    duk_push_global_object(ctx);           /* => [global] */
    duk_push_string(ctx, "Duktape");       /* => [global] ["duktape"] */
    duk_get_prop(ctx, -2);                 /* => [global] [duktape] */
    if (duk_is_object(ctx, -1)) {
        duk_push_string(ctx, "tcl");   /* => [global] [duktape] ["tcl"] */
        duk_del_prop(ctx, -2);         /* => [global] [duktape]  */
    }
    duk_pop(ctx);                          /* => [global] */
    duk_pop(ctx);                          /* => */

    return;
}

/*
 * Initialize a Duktape intepreter.
 * Return value: string token of the form "::duktape::(integer)".
 * Side effects: creates an Duktape heap.
 */
static int
Init_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    struct DuktapeInstanceData *instanceData;
    duk_context *ctx;
    Tcl_HashEntry *hashPtr;
    int isNew;
    Tcl_Obj *token;
    int makeSafe = 1;
    int tclRet;

    if (objc != 1 && objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_INIT);
        return TCL_ERROR;
    }

    if (objc == 3) {
        if (strcmp(Tcl_GetStringFromObj(objv[1], NULL), "-safe") != 0) {
            Tcl_WrongNumArgs(interp, 1, objv, USAGE_INIT);
            return TCL_ERROR;
        }
        tclRet = Tcl_GetBoolean(interp, Tcl_GetStringFromObj(objv[2], NULL), &makeSafe);
        if (tclRet != TCL_OK) {
            return(tclRet);
        }
    }

    instanceData = ckalloc(sizeof(*instanceData));
    instanceData->interp = interp;

    ctx = duk_create_heap(NULL, NULL, NULL, instanceData, NULL);
    if (ctx == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_CREATE, -1));
        free(instanceData);
        return TCL_ERROR;
    }

    instanceData->ctx = ctx;

    DUKTCL_CDATA->counter++;
    token = Tcl_ObjPrintf(NS "::%d", DUKTCL_CDATA->counter);
    hashPtr = Tcl_CreateHashEntry(&DUKTCL_CDATA->table, Tcl_GetString(token),
            &isNew);
    Tcl_SetHashValue(hashPtr, (ClientData) instanceData);

    if (!makeSafe) {
        MakeContextUnsafe(ctx);
    }

    Tcl_SetObjResult(interp, token);
    return TCL_OK;
}

static int MakeSafe_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    duk_context *ctx;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_MAKE_SAFE);
        return(TCL_ERROR);
    }

    ctx = parse_id(cdata, interp, objv[1], 0);
    if (!ctx) {
        return(TCL_ERROR);
    }

    MakeContextSafe(ctx);

    return(TCL_OK);
}

static int MakeUnsafe_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    duk_context *ctx;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_MAKE_UNSAFE);
        return(TCL_ERROR);
    }

    ctx = parse_id(cdata, interp, objv[1], 0);
    if (!ctx) {
        return(TCL_ERROR);
    }

    MakeContextUnsafe(ctx);

    return(TCL_OK);
}

/*
 * Destroy a Duktape interpreter heap.
 * Return value: nothing.
 * Side effects: destroys a Duktape interpreter heap.
 */
static int
Close_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    duk_context *ctx;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_CLOSE);
        return TCL_ERROR;
    }

    ctx = parse_id(cdata, interp, objv[1], 1);
    if (ctx == NULL) {
        return TCL_ERROR;
    }

    duk_destroy_heap(ctx);

    return TCL_OK;
}

/*
 * Register a new global function within a context
 */
/*
 * Evaluate a Tcl string and return the result to JavaScript
 */
static duk_ret_t EvalTclCmdFromJS(duk_context *ctx) {
    struct DuktapeInstanceData *instanceData;
    duk_memory_functions funcs;
    Tcl_Interp *interp;

    duk_get_memory_functions(ctx, &funcs);
    instanceData = funcs.udata;

    interp = instanceData->interp;

    duk_push_current_function(ctx);          /* => [args...] [function] */
    duk_push_literal(ctx, "apply");          /* => [args...] [function] ["apply"] */
    duk_get_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("lambda"));  /* => [args...] [function] ["apply"] [lambda] */
    duk_insert(ctx, 0);                      /* => [lambda] [args...] [function] ["apply"] */
    duk_insert(ctx, 0);                      /* => ["apply"] [lambda] [args...] [function] */
    duk_pop(ctx);                            /* => ["apply"] [lambda] [args...] */

    return(EvalTclFromJSWithInterp(interp, ctx));
}

static int RegisterFunction_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    duk_context *ctx;
    Tcl_Obj *lambdaObj;
    const char *functionName, *lambdaString;
    int lambdaStringLength;

    if (objc != 5) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_TCL_FUNCTION);
        return(TCL_ERROR);
    }

    ctx = parse_id(cdata, interp, objv[1], 0);
    if (ctx == NULL) {
        return(TCL_ERROR);
    }

    functionName = Tcl_GetStringFromObj(objv[2], NULL);

        lambdaObj = Tcl_NewListObj(2, objv + 3);
    lambdaString = Tcl_GetStringFromObj(lambdaObj, &lambdaStringLength);
    Tcl_DecrRefCount(lambdaObj);

    duk_push_global_object(ctx);                               /* => [global] */
    duk_push_c_function(ctx, EvalTclCmdFromJS, DUK_VARARGS);   /* => [global] [function] */
    duk_push_lstring(ctx, lambdaString, lambdaStringLength);   /* => [global] [function] [lambda] */
    duk_put_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("lambda")); /* => [global] [function] */
    duk_put_prop_string(ctx, -2, functionName);                /* => [global] */
    duk_pop(ctx);                                              /* => */

    return(TCL_OK);
}


/*
 * Evaluate a string as Duktape code in the selected heap.
 * Usage: eval token code
 * Return value: the result of the evaluation coerced to string.
 * Side effects: may change the Duktape interpreter heap.
 */
static int
Eval_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    duk_context *ctx;
    duk_int_t duk_result;
    const char *js_code;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_EVAL);
        return TCL_ERROR;
    }

    ctx = parse_id(cdata, interp, objv[1], 0);
    if (ctx == NULL) {
        return TCL_ERROR;
    }

    js_code = Tcl_GetString(objv[2]);

    duk_result = duk_peval_string(ctx, js_code);

    Tcl_SetObjResult(interp,
            Tcl_NewStringObj(
                duk_safe_to_string(ctx, -1), -1));
    duk_pop(ctx);

    if (duk_result == 0) {
        return TCL_OK;
    } else {
        return TCL_ERROR;
    }
}


/*
 * Call a JS method/function.
 * Usage: call token method this ?{arg ?type?}? ...
 * Return value: the result of the method call coerced to string.
 * Side effects: may change the Duktape interpreter heap.
 */
static int
CallMethod_Cmd(ClientData cdata, Tcl_Interp *interp, int objc,
        Tcl_Obj *const objv[])
{
    int i;
    int list_length;
    int tableIndex;
    int int_value;
    double double_value;
    duk_context *ctx;
    duk_int_t duk_result;
    Tcl_Obj *value;
    Tcl_Obj *type;

    static const char *types[] = {
        "boolean",
        "nan",
        "null",
        "number",
        "string",
        "undefined",
        (char *)NULL
    };
    enum types {
        TYPE_BOOLEAN,
        TYPE_NAN,
        TYPE_NULL,
        TYPE_NUMBER,
        TYPE_STRING,
        TYPE_UNDEFINED
    };

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_CALL_METHOD);
        return TCL_ERROR;
    }

    ctx = parse_id(cdata, interp, objv[1], 0);
    if (ctx == NULL) {
        return TCL_ERROR;
    }

    /* Eval the function name and "this" to put them on the stack. */
    for (i = 2; i < 4; i++)
    {
        duk_result = duk_peval_string(ctx, Tcl_GetString(objv[i]));
        if (duk_result != 0) {
            Tcl_SetObjResult(interp,
                    Tcl_NewStringObj(
                        duk_safe_to_string(ctx, -1), -1));
            duk_pop(ctx);
            return TCL_ERROR;
        }
    }

    /* Push the arguments. */
    for (i = 4; i < objc; i++) {
        if (Tcl_ListObjIndex(interp, objv[i], 0, &value) != TCL_OK) {
            return TCL_ERROR;
        }

        if (Tcl_ListObjLength(interp, objv[i], &list_length) != TCL_OK) {
            return TCL_ERROR;
        }

        if (list_length == 2) {
            if (Tcl_ListObjIndex(interp, objv[i], 1, &type) != TCL_OK) {
                return TCL_ERROR;
            }

            if (Tcl_GetIndexFromObj(interp, type, types, "type", 0,
                    &tableIndex) != TCL_OK) {
                return TCL_ERROR;
            }
        } else if (list_length == 1) {
            tableIndex = TYPE_STRING;
        } else {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_ARG_LENGTH, -1));
            return TCL_ERROR;
        }

        switch ((enum types) tableIndex) {
            case TYPE_BOOLEAN:
                if (Tcl_GetIntFromObj(interp, value, &int_value) != TCL_OK) {
                    return TCL_ERROR;
                }
                duk_push_boolean(ctx, int_value);
                break;
            case TYPE_NAN:
                duk_push_nan(ctx);
                break;
            case TYPE_NULL:
                duk_push_null(ctx);
                break;
            case TYPE_NUMBER:
                if (Tcl_GetDoubleFromObj(interp, value,
                        &double_value) != TCL_OK) {
                    return TCL_ERROR;
                }
                duk_push_number(ctx, double_value);
                break;
            case TYPE_UNDEFINED:
                duk_push_undefined(ctx);
                break;
            case TYPE_STRING:
            default:
                duk_push_string(ctx, Tcl_GetString(value));
                break;
        }
    }
    duk_result = duk_pcall_method(ctx, objc - 4);

    Tcl_SetObjResult(interp, Tcl_NewStringObj(duk_safe_to_string(ctx, -1), -1));
    duk_pop(ctx);

    if (duk_result == 0) {
        return TCL_OK;
    } else {
        return TCL_ERROR;
    }
}

/*
 * Tclduktape_Init -- Called when Tcl loads the extension.
 */
int DLLEXPORT
Tclduktape_Init(Tcl_Interp *interp)
{
    Tcl_Namespace *nsPtr;
    struct DuktapeData *duktape_data;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }
#endif

    duktape_data = (struct DuktapeData *) ckalloc(sizeof(struct DuktapeData));

    /* Create the namespace. */
    if (Tcl_FindNamespace(interp, NS, NULL, 0) == NULL) {
        nsPtr = Tcl_CreateNamespace(interp, NS, NULL, NULL);
        if (nsPtr == NULL) {
            return TCL_ERROR;
        }
    }

    duktape_data->counter = 0;
    Tcl_InitHashTable(&duktape_data->table, TCL_STRING_KEYS);

    Tcl_CreateObjCommand(interp, NS INIT, Init_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS MAKE_SAFE, MakeSafe_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS MAKE_UNSAFE, MakeUnsafe_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS CLOSE, Close_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS EVAL, Eval_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS TCL_FUNCTION, RegisterFunction_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS CALL_METHOD,
            CallMethod_Cmd, duktape_data, NULL);
    Tcl_CallWhenDeleted(interp, cleanup_interp, duktape_data);
    Tcl_PkgProvide(interp, PACKAGE, VERSION);

    return TCL_OK;
}
