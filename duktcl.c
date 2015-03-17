/*
 * DukTcl, Tcl bindings for Duktape.
 * Copyright (C) 2015 Danyil Bohdan.
 * This code is released under the terms of the MIT license. See the file
 * LICENSE for details.
 */
#include <tcl.h>
#include "external/duktape.h"

/* Package information. */

#define PACKAGE "duktape"
#define VERSION "0.0.1"

/* Namespace for the extension. */

#define NS "::" PACKAGE

/* Limit on the maximum number of Duktape objects. */

#define MAX_COUNT 16

/* Command names. */

#define INIT "::init"
#define CLOSE "::close"
#define EVAL "::eval"
#define CALL "::call"

/* Error messages. */

#define ERROR_TOKEN "can't parse token"
#define ERROR_ARG_LENGTH "argument must be a list of one or two elements"

/* Usage. */

#define USAGE_INIT ""
#define USAGE_CLOSE "token"
#define USAGE_EVAL "token code"
#define USAGE_CALL "token function ?{arg ?type?}? ..."

/* Data types. */

struct DuktapeData
{
    duk_context *object[MAX_COUNT];
    int active[MAX_COUNT];
};

#define DUKTCL_CDATA ((struct DuktapeData *) cdata)

/* Functions */


/* Set id to the integer value of the interpreter token. */
static int
parse_id(ClientData cdata, Tcl_Interp *interp, Tcl_Obj *const idobj, int *id)
{
    Tcl_Obj* cmd[2];
    Tcl_Obj* result_obj = NULL;
    int success;
    int conv_result;

    cmd[0] = Tcl_NewStringObj(NS "::parseToken", -1);
    cmd[1] = idobj;
    Tcl_IncrRefCount(cmd[0]);
    Tcl_IncrRefCount(cmd[1]);
    success = Tcl_EvalObjv(interp, 2, cmd, 0);
    Tcl_DecrRefCount(cmd[0]);
    Tcl_DecrRefCount(cmd[1]);

    if (success == TCL_OK) {
        result_obj = Tcl_GetObjResult(interp);
        Tcl_IncrRefCount(result_obj);
        conv_result = Tcl_GetIntFromObj(interp, result_obj, id);
        Tcl_DecrRefCount(result_obj);
        Tcl_FreeResult(interp);

        if (conv_result != TCL_OK) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_TOKEN, -1));
            return TCL_ERROR;
        }

        (*id)--; /* Tokens start from one while actual ids start from zero. */

        if (DUKTCL_CDATA->active[*id] != 1) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("object not found", -1));
            return TCL_ERROR;
        }

        return TCL_OK;
    } else {
        return success;
    }
}


/*
 * Initialize a Duktape intepreter.
 * Return value: string token of the form "::duktape::(integer)".
 * Side effects: creates an Duktape heap and marks its slot as in use.
 */
static int
Init_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int id = -1;
    int i;
    Tcl_Obj *token[1];
    Tcl_Obj *result;

    if (objc != 1) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_INIT);
        return TCL_ERROR;
    }

    /* Find an unused object slot. */
    for (i = 0; i < MAX_COUNT; i++) {
        if (DUKTCL_CDATA->active[i] == 0) {
            id = i;
            break;
        }
    }
    if (id == -1) {
        Tcl_SetObjResult(interp,
                Tcl_NewStringObj("out of interpreter slots", -1));
        return TCL_ERROR;
    }

    DUKTCL_CDATA->object[id] = duk_create_heap_default();
    DUKTCL_CDATA->active[id] = 1;

    token[0] = Tcl_NewIntObj(id + 1);
    Tcl_IncrRefCount(token[0]);
    result = Tcl_Format(interp, NS "::%d", 1, token);
    Tcl_DecrRefCount(token[0]);

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

/*
 * Destroy a Duktape interpreter heap.
 * Return value: nothing.
 * Side effects: destroys a Duktape interpreter heap and marks its slot as
 * available.
 */
static int
Close_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int id;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_CLOSE);
        return TCL_ERROR;
    }

    if (parse_id(cdata, interp, objv[1], &id) != TCL_OK) {
        return TCL_ERROR;
    }

    duk_destroy_heap(DUKTCL_CDATA->object[id]);
    DUKTCL_CDATA->active[id] = 0;

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
    int id;
    const char* js_code;
    duk_int_t duk_result;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_EVAL);
        return TCL_ERROR;
    }

    if (parse_id(cdata, interp, objv[1], &id) != TCL_OK) {
        return TCL_ERROR;
    }

    js_code = Tcl_GetString(objv[2]);

    duk_result = duk_peval_string(DUKTCL_CDATA->object[id], js_code);

    Tcl_SetObjResult(interp,
            Tcl_NewStringObj(
                duk_safe_to_string(DUKTCL_CDATA->object[id], -1), -1));
    duk_pop(DUKTCL_CDATA->object[id]);

    if (duk_result == 0) {
        return TCL_OK;
    } else {
        return TCL_ERROR;
    }
}


/*
 * Call a JS function.
 * Usage: call token function ?{arg ?type?}? ...
 * Return value: the result of the function call coerced to string.
 * Side effects: may change the Duktape interpreter heap.
 */
static int
Call_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int id;
    int i;
    int list_length;
    int tableIndex;
    int int_value;
    double double_value;
    duk_int_t duk_result;
    Tcl_Obj *value;
    Tcl_Obj *type;

    static char *types[] = {
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

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_CALL);
        return TCL_ERROR;
    }

    if (parse_id(cdata, interp, objv[1], &id) != TCL_OK) {
        return TCL_ERROR;
    }

    /* Eval the function name to put it on the stack. */
    duk_eval_string(DUKTCL_CDATA->object[id], Tcl_GetString(objv[2]));
    /* Push the arguments. */
    for (i = 3; i < objc; i++) {
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
                duk_push_boolean(DUKTCL_CDATA->object[id], int_value);
                break;
            case TYPE_NAN:
                duk_push_nan(DUKTCL_CDATA->object[id]);
                break;
            case TYPE_NULL:
                duk_push_null(DUKTCL_CDATA->object[id]);
                break;
            case TYPE_NUMBER:
                if (Tcl_GetDoubleFromObj(interp, value,
                        &double_value) != TCL_OK) {
                    return TCL_ERROR;
                }
                duk_push_number(DUKTCL_CDATA->object[id], double_value);
                break;
            case TYPE_UNDEFINED:
                duk_push_undefined(DUKTCL_CDATA->object[id]);
                break;
            case TYPE_STRING:
            default:
                duk_push_string(DUKTCL_CDATA->object[id], Tcl_GetString(value));
                break;
        }
    }
    duk_result = duk_pcall(DUKTCL_CDATA->object[id], objc - 3);

    Tcl_SetObjResult(interp,
            Tcl_NewStringObj(
                duk_safe_to_string(DUKTCL_CDATA->object[id], -1), -1));
    duk_pop(DUKTCL_CDATA->object[id]);

    if (duk_result == 0) {
        return TCL_OK;
    } else {
        return TCL_ERROR;
    }
}

/*
 * Duktcl_Init -- Called when Tcl loads the extension.
 */
int DLLEXPORT
Duktcl_Init(Tcl_Interp *interp)
{
    Tcl_Namespace *nsPtr;
    int i;
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

    for (i = 0; i < MAX_COUNT; i++) {
        duktape_data->active[i] = 0;
    }

    Tcl_CreateObjCommand(interp, NS INIT, Init_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS CLOSE, Close_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS EVAL, Eval_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS CALL, Call_Cmd, duktape_data, NULL);
    Tcl_PkgProvide(interp, PACKAGE, VERSION);

    Tcl_Eval(interp, "proc " NS "::parseToken token { \
        if {![regexp {^(?:" NS "::)?([1-9]+[0-9]*)$} $token _ id]} { \
            error {" ERROR_TOKEN "}\n\
        } \n\
        return $id \n\
    }");

    return TCL_OK;
}
