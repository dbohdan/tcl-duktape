/*
 * Tcl bindings for Duktape.
 * Copyright (c) 2015, 2016, 2017
 * dbohdan and contributors listed in AUTHORS
 * This code is released under the terms of the MIT license. See the file
 * LICENSE for details.
 */
#include <tcl.h>
#include "../vendor/duktape/duktape.h"

/* Package information. */

#define PACKAGE "duktape"
#define VERSION "0.5.0"

/* Namespace for the extension. */

#define NS "::" PACKAGE

/* Command names. */

#define INIT "::init"
#define CLOSE "::close"
#define EVAL "::eval"
#define CALL_METHOD "::call-method"

/* Error messages. */

#define ERROR_TOKEN "can't parse token"
#define ERROR_ARG_LENGTH "argument must be a list of one or two elements"
#define ERROR_CREATE "can't create Duktape context"

/* Usage. */

#define USAGE_INIT ""
#define USAGE_CLOSE "token"
#define USAGE_EVAL "token code"
#define USAGE_CALL_METHOD "token method this ?{arg ?type?}? ..."

/* Data types. */

struct DuktapeData
{
    int counter;
    Tcl_HashTable table;
};

#define DUKTCL_CDATA ((struct DuktapeData *) cdata)

/* Functions */

static duk_context *
parse_id(ClientData cdata, Tcl_Interp *interp, Tcl_Obj *const idobj, int del)
{
    duk_context *ctx;
    Tcl_HashEntry *hashPtr;

    hashPtr = Tcl_FindHashEntry(&DUKTCL_CDATA->table, Tcl_GetString(idobj));
    if (hashPtr == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_TOKEN, -1));
        return NULL;
    }
    ctx = (duk_context *) Tcl_GetHashValue(hashPtr);
    if (del) {
        Tcl_DeleteHashEntry(hashPtr);
    }
    return ctx;
}

static void
cleanup_interp(ClientData cdata, Tcl_Interp *interp)
{
    Tcl_HashEntry* hashPtr;
    Tcl_HashSearch search;
    duk_context* ctx;

    hashPtr = Tcl_FirstHashEntry(&DUKTCL_CDATA->table, &search);
    while (hashPtr != NULL) {
        ctx = (duk_context *) Tcl_GetHashValue(hashPtr);
        Tcl_SetHashValue(hashPtr, (ClientData) NULL);
        duk_destroy_heap(ctx);
        hashPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&DUKTCL_CDATA->table);
    ckfree((char *)DUKTCL_CDATA);
}

/*
 * Initialize a Duktape intepreter.
 * Return value: string token of the form "::duktape::(integer)".
 * Side effects: creates an Duktape heap.
 */
static int
Init_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    duk_context *ctx;
    Tcl_HashEntry *hashPtr;
    int isNew;
    Tcl_Obj *token;

    if (objc != 1) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_INIT);
        return TCL_ERROR;
    }

    ctx = duk_create_heap_default();
    if (ctx == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_CREATE, -1));
        return TCL_ERROR;
    }

    DUKTCL_CDATA->counter++;
    token = Tcl_ObjPrintf(NS "::%d", DUKTCL_CDATA->counter);
    hashPtr = Tcl_CreateHashEntry(&DUKTCL_CDATA->table, Tcl_GetString(token),
            &isNew);
    Tcl_SetHashValue(hashPtr, (ClientData) ctx);

    Tcl_SetObjResult(interp, token);
    return TCL_OK;
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
    duktape_data = (struct DuktapeData *) ckalloc(sizeof(struct DuktapeData));

    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }

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
    Tcl_CreateObjCommand(interp, NS CLOSE, Close_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS EVAL, Eval_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS CALL_METHOD,
            CallMethod_Cmd, duktape_data, NULL);
    Tcl_CallWhenDeleted(interp, cleanup_interp, duktape_data);
    Tcl_PkgProvide(interp, PACKAGE, VERSION);

    return TCL_OK;
}
