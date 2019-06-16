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
#define EVALLAMBDA "::evallambda"
#define TCL_FUNCTION "::tcl-function"
#define CALL_METHOD "::call-method"

/* Error messages. */

#define ERROR_TOKEN "can't parse token"
#define ERROR_ARG_LENGTH "argument must be a list of one or two elements"
#define ERROR_CREATE "can't create Duktape context"
#define ERROR_INVALID_INSTANCE "unable to locate instance"
#define ERROR_INVALID_INTERP "unable to locate interp"
#define ERROR_INVALID_STRING "unable to convert argument to string"
#define ERROR_INVALID_TYPE "unable to convert argument to invalid type %s"
#define ERROR_INTERNAL_ARGS_ERROR "internal error: negative arguments?"
#define ERROR_INTERNAL_TCL_LAPPEND "internal error: lappend failed?"
#define ERROR_NOT_ALLOWED "action not permitted while safe"

/* Usage. */

#define USAGE_INIT "?-safe <boolean>?"
#define USAGE_MAKE_SAFE "token"
#define USAGE_MAKE_UNSAFE "token"
#define USAGE_CLOSE "token"
#define USAGE_EVAL "token code"
#define USAGE_EVALLAMBDA "token bytecode lambdaHandle args"
#define USAGE_TCL_FUNCTION "token name ?returnType? args body"
#define USAGE_CALL_METHOD "token method this ?{arg ?type?}? ..."

/* Data types. */

struct DuktapeData
{
    int counter;
    Tcl_HashTable table;
};

struct DuktapeInstanceData {
    Tcl_Interp *interp;
    Tcl_Obj *handle;
    duk_context *ctx;
    struct DuktapeData *cdata;
    int isUnsafe;
    int lambdaCount;
};

struct DuktapeLambdaInstanceData {
    int refCount;
    struct DuktapeData *cdata;
    Tcl_Obj *handle;
    Tcl_Obj *lambdaName;
    Tcl_Obj *bytecode;
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
        if (interp) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_TOKEN, -1));
        }
        return NULL;
    }
    instanceData = (struct DuktapeInstanceData *) Tcl_GetHashValue(hashPtr);
    if (!instanceData) {
        if (interp) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_INVALID_INSTANCE, -1));
        }
        return(NULL);
    }
    ctx = instanceData->ctx;
    if (del) {
        Tcl_DeleteHashEntry(hashPtr);
        ckfree(instanceData);
    }
    return ctx;
}

/*
 * Deal with Duktape Lambdas using a custom Tcl Obj type which
 * we can use to free the lambda when the object goes away
 */
/**
 ** Free a Duktape lambda object when the Tcl one is freed
 ** also free all the internal mechanisms required to get
 ** this far
 **/
static void Tclduk_LambdaObjType_Free(Tcl_Obj *lambdaObj) {
    struct DuktapeLambdaInstanceData *instanceData;
    ClientData cdata;
    Tcl_Obj *handle, *lambdaNameObj, *bytecode;
    const char *lambdaName;
    int lambdaNameLength;
    duk_context *ctx;
    int freeDukLambda;

    instanceData = lambdaObj->internalRep.otherValuePtr;

    freeDukLambda = 1;

    instanceData->refCount--;

    if (instanceData->refCount > 0) {
        return;
    }

    cdata         = instanceData->cdata;
    lambdaNameObj = instanceData->lambdaName;
    handle        = instanceData->handle;
    bytecode      = instanceData->bytecode;

    ckfree(instanceData);

    Tcl_DecrRefCount(bytecode);

    /*
     * Find the Duktape heap context from the handle
     */
    ctx = parse_id(cdata, NULL, handle, 0);
    Tcl_DecrRefCount(handle);
    if (ctx == NULL) {
        Tcl_DecrRefCount(lambdaNameObj);
        return;
    }

    /*
     * Schedule the deletion of the lambda from Duktape
     */
    if (freeDukLambda) {
        lambdaName = Tcl_GetStringFromObj(lambdaNameObj, &lambdaNameLength);
        duk_push_global_stash(ctx);                                  /* => ... [stash] */
        duk_get_prop_literal(ctx, -1, "freeableLambdas");            /* => ... [stash] [object|undefined] */
        if (duk_is_undefined(ctx, -1)) {
            duk_pop(ctx);                                            /* => ... [stash] */
            duk_push_object(ctx);                                    /* => ... [stash] [object] */
        }
        duk_push_true(ctx);                                          /* => ... [stash] [object] [true] */
        duk_put_prop_lstring(ctx, -2, lambdaName, lambdaNameLength); /* => ... [stash] [object.lambdaName=true] */
        duk_put_prop_literal(ctx, -2, "freeableLambdas");            /* => ... [stash.freeableLambdas=object] */
        duk_pop(ctx);                                                /* => ... */
    }

    /*
     * Finish cleanup
     */
    Tcl_DecrRefCount(lambdaNameObj);

    return;
}

static void Tclduk_LambdaObjType_Dup(Tcl_Obj *src, Tcl_Obj *dest) {
    struct DuktapeLambdaInstanceData *instanceData;

    instanceData = src->internalRep.otherValuePtr;

    instanceData->refCount++;

    dest->internalRep.otherValuePtr = instanceData;

    return;
}

static void Tclduk_LambdaObjType_String(Tcl_Obj *lambdaObj) {
    struct DuktapeLambdaInstanceData *instanceData;
    Tcl_Obj *handle, *lambdaName, *bytecode;
    Tcl_Obj *dukStringObj, *dukItemObj, *dukCodeObj, *dukCodeLineObj;
    char *stringRep;
    int stringRepLength;

    instanceData = lambdaObj->internalRep.otherValuePtr;

    lambdaName = instanceData->lambdaName;
    handle     = instanceData->handle;
    bytecode   = instanceData->bytecode;

    dukStringObj = Tcl_NewObj();
    dukItemObj = Tcl_NewObj();
    dukCodeObj = Tcl_NewObj();

    /*
     * Set the bytecode
     */
    dukCodeLineObj = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, Tcl_NewStringObj("set", -1));
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, Tcl_NewStringObj("code", -1));
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, bytecode);
    Tcl_AppendObjToObj(dukCodeObj, dukCodeLineObj);
    Tcl_AppendObjToObj(dukCodeObj, Tcl_NewStringObj("\n", -1));

    /*
     * Set the Duktape lambda handle
     */
    dukCodeLineObj = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, Tcl_NewStringObj("set", -1));
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, Tcl_NewStringObj("lambdaName", -1));
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, lambdaName);
    Tcl_AppendObjToObj(dukCodeObj, dukCodeLineObj);
    Tcl_AppendObjToObj(dukCodeObj, Tcl_NewStringObj("\n", -1));

    /*
     * Set the Duktape token handle
     */
    dukCodeLineObj = Tcl_NewObj();
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, Tcl_NewStringObj("set", -1));
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, Tcl_NewStringObj("handle", -1));
    Tcl_ListObjAppendElement(NULL, dukCodeLineObj, handle);
    Tcl_AppendObjToObj(dukCodeObj, dukCodeLineObj);
    Tcl_AppendObjToObj(dukCodeObj, Tcl_NewStringObj("\n", -1));

    /*
     * Add the evaluation commands
     */
    Tcl_AppendObjToObj(dukCodeObj, Tcl_NewStringObj("return [" NS "::evallambda $handle $code $lambdaName {*}$args]", -1));

    /*
     * Produce a Tcl lambda
     */
    Tcl_ListObjAppendElement(NULL, dukItemObj, Tcl_NewStringObj("args", -1));
    Tcl_ListObjAppendElement(NULL, dukItemObj, dukCodeObj);
    Tcl_ListObjAppendElement(NULL, dukStringObj, Tcl_NewStringObj("apply", -1));
    Tcl_ListObjAppendElement(NULL, dukStringObj, dukItemObj);

    /*
     * Use this string rep as our object's string rep
     */
    stringRep = Tcl_GetStringFromObj(dukStringObj, &stringRepLength);
    lambdaObj->bytes  = ckalloc(stringRepLength + 1);
    lambdaObj->bytes[stringRepLength] = '\0';

    memcpy(lambdaObj->bytes, stringRep, stringRepLength);
    lambdaObj->length = stringRepLength;

    /*
     * Free the temporary object
     */
    Tcl_DecrRefCount(dukStringObj);
}

/**
 ** Process-wide reference to the Lambda Object Type
 ** so it may be registered with Tcl.
 **/
static Tcl_ObjType Tclduk_LambdaObjType = {
    "duktape_lambda" /* name */,
    Tclduk_LambdaObjType_Free,
    Tclduk_LambdaObjType_Dup,
    Tclduk_LambdaObjType_String,
    NULL
};

/**
 ** Allocate a new Tcl_Obj for a Lambda Object
 **/
static Tcl_Obj *Tclduk_LambdaObjType_New(ClientData cdata, Tcl_Obj *handle, Tcl_Obj *lambdaName, Tcl_Obj *bytecode) {
    struct DuktapeLambdaInstanceData *instanceData;
    Tcl_Obj *retval;

    instanceData = ckalloc(sizeof(*instanceData));
    retval       = ckalloc(sizeof(*retval));

    retval->refCount = 0;
    retval->bytes    = NULL;
    retval->length   = 0;
    retval->typePtr  = &Tclduk_LambdaObjType;

    instanceData->refCount = 1;
    instanceData->cdata  = cdata;
    instanceData->handle = handle;
    instanceData->bytecode = bytecode;
    instanceData->lambdaName = lambdaName;

    Tcl_IncrRefCount(handle);
    Tcl_IncrRefCount(lambdaName);
    Tcl_IncrRefCount(bytecode);

    retval->internalRep.otherValuePtr = instanceData;

    return(retval);
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
        Tcl_SetHashValue(hashPtr, (ClientData) NULL);

        ctx = instanceData->ctx;
        duk_destroy_heap(ctx);

        Tcl_DecrRefCount(instanceData->handle);

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
 * Convert JavaScript item to a Tcl item
 */
static Tcl_Obj *Tclduk_JSToTcl_function(duk_context *ctx, duk_idx_t idx) {
    struct DuktapeInstanceData *instanceData;
    duk_memory_functions funcs;
    struct DuktapeData *cdata;
    const char *dukString;
    Tcl_Obj *dukStringObj, *dukCodeByteCodeObj;
    Tcl_Obj *handle, *lambdaNameObj;
    duk_size_t dukStringLength;

    duk_get_memory_functions(ctx, &funcs);
    instanceData = funcs.udata;

    handle = instanceData->handle;
    cdata  = instanceData->cdata;

    lambdaNameObj = Tcl_ObjPrintf("lambda_%i%i%i_%i", rand(), rand(), rand(), instanceData->lambdaCount);
    instanceData->lambdaCount++;

    idx = duk_normalize_index(ctx, idx);
    duk_push_global_stash(ctx);                                 /* => ... [stash] */
    duk_dup(ctx, idx);                                          /* => ... [stash] [function] */
    duk_dump_function(ctx);                                     /* => ... [stash] [bytecode] */
    duk_base64_encode(ctx, -1);                                 /* => ... [stash] [bytecode.b64] */
    dukString = duk_safe_to_lstring(ctx, -1, &dukStringLength); /* => ... [stash] [bytecode.b64] */
    dukCodeByteCodeObj = Tcl_NewStringObj(dukString, dukStringLength);
    duk_pop(ctx);                                               /* => ... [stash] */
    duk_dup(ctx, idx);                                          /* => ... [stash] [function] */
    duk_put_prop_string(ctx, -2, Tcl_GetStringFromObj(lambdaNameObj, NULL));   /* => ... [stash] */

    duk_get_prop_literal(ctx, -1, "freeableLambdas");           /* => ... [stash] [freeableLambdas] */
    if (!duk_is_undefined(ctx, -1)) {
        duk_enum(ctx, -1, 0);                                   /* => ... [stash] [freeableLambdas] [enum] */
        while (duk_next(ctx, -1, 0)) {                          /* => ... [stash] [freeableLambdas] [enum] [key] */
            duk_del_prop(ctx, -4);                              /* => ... [stash] [freeableLambdas] [enum] */
        }
        duk_pop(ctx);                                           /* => ... [stash] [freeableLambdas] */
    }
    duk_pop(ctx);                                               /* => ... [stash] */
    duk_del_prop_literal(ctx, -1, "freeableLambdas");           /* => ... [stash] */

    duk_pop(ctx);                                               /* => ... */

    dukStringObj = Tclduk_LambdaObjType_New(cdata, handle, lambdaNameObj, dukCodeByteCodeObj);

    return(dukStringObj);
}

static Tcl_Obj *Tclduk_JSToTcl(duk_context *ctx, duk_idx_t idx) {
    const char *dukString;
    duk_size_t dukStringLength;
    duk_int_t arrayLength;
    duk_idx_t arrayIndex;
    Tcl_Obj *dukStringObj, *dukItemObj;
    enum {
        TCLDUK_TYPE_BYTEARRAY,
        TCLDUK_TYPE_STRING
    } string_format;

    /*
     * Convert types
     *     buffer          => ByteArray
     *     array           => List
     *     null/undefined  => empty string
     *     other object    => JSON
     *     everything else => string
     */
    dukString = NULL;
    dukStringObj = NULL;
    string_format = TCLDUK_TYPE_STRING;
    if (duk_check_type_mask(ctx, idx, DUK_TYPE_MASK_NULL | DUK_TYPE_MASK_UNDEFINED)) {
        dukString = "";
        dukStringLength = 0;
    }

    if (!dukString && !dukStringObj && duk_is_array(ctx, idx)) {
        duk_get_prop_string(ctx, idx, "length");
        arrayLength = duk_get_int(ctx, -1);
        duk_pop(ctx);

        dukStringObj = Tcl_NewObj();
        for (arrayIndex = 0; arrayIndex < arrayLength; arrayIndex++) {
            duk_get_prop_index(ctx, idx, arrayIndex);
            dukItemObj = Tclduk_JSToTcl(ctx, -1);
            duk_pop(ctx);

            Tcl_ListObjAppendElement(NULL, dukStringObj, dukItemObj);
        }
    }

    if (!dukString && !dukStringObj && duk_is_function(ctx, idx)) {
        dukStringObj = Tclduk_JSToTcl_function(ctx, idx);
    }

    if (!dukString && !dukStringObj && duk_is_buffer_data(ctx, idx)) {
        duk_buffer_to_string(ctx, idx);
        string_format = TCLDUK_TYPE_BYTEARRAY;
    }

    if (!dukString && !dukStringObj && duk_check_type(ctx, idx, DUK_TYPE_OBJECT)) {
        duk_json_encode(ctx, idx);
    }

    if (!dukStringObj) {
        if (!dukString) {
            dukString = duk_safe_to_lstring(ctx, idx, &dukStringLength);
        }

        if (!dukString) {
            return(NULL);
        }

        switch (string_format) {
            case TCLDUK_TYPE_BYTEARRAY:
                dukStringObj = Tcl_NewByteArrayObj((const unsigned char *) dukString, dukStringLength);
                break;
            case TCLDUK_TYPE_STRING:
                dukStringObj = Tcl_NewStringObj(dukString, dukStringLength);
                break;
        }
    }

    return(dukStringObj);
}

/*
 * Convert Tcl item to a JavaScript item and push that item to the end of the stack
 */
static duk_idx_t Tclduk_TclToJS(Tcl_Interp *interp, Tcl_Obj *value, duk_context *ctx, const char *type) {
    Tcl_Obj *typeObj, *firstTypeObj, *itemObj;
    char *firstTypeString, *otherTypesString;
    const char *valueString;
    unsigned int type_hash;
    enum {
        TCLDUK_TYPE_BOOLEAN,
        TCLDUK_TYPE_BYTEARRAY,
        TCLDUK_TYPE_STRING,
        TCLDUK_TYPE_UNDEFINED,
        TCLDUK_TYPE_NULL,
        TCLDUK_TYPE_DOUBLE,
        TCLDUK_TYPE_INTEGER,
        TCLDUK_TYPE_BIGINT,
        TCLDUK_TYPE_ARRAY,
        TCLDUK_TYPE_JSON
    } string_format;
    double valueDouble;
    duk_idx_t checkRet;
    int valueBoolean;
    int valueStringLength, firstTypeStringLength;
    int idx, numItems;
    int tclRet;

    /*
     * If the return type is ignored, return without bothering
     * fetch the result
     */
    if (!type) {
        type = "string";
    }

    typeObj = Tcl_NewStringObj(type, -1);
    tclRet = Tcl_ListObjIndex(NULL, typeObj, 0, &firstTypeObj);
    if (tclRet != TCL_OK || !firstTypeObj) {
        firstTypeObj = typeObj;
    }
    firstTypeString = Tcl_GetStringFromObj(firstTypeObj, &firstTypeStringLength);

    type_hash = Tcl_ZlibCRC32(0, (const unsigned char *) firstTypeString, firstTypeStringLength);

    switch (type_hash) {
        case 0x0: /* (empty string) */
        case 0x9ebeb2a9: /* string */
            string_format = TCLDUK_TYPE_STRING;
            break;
        case 0x8a858868: /* boolean */
            string_format = TCLDUK_TYPE_BOOLEAN;
            break;
        case 0xb3444c: /* bytearray */
            string_format = TCLDUK_TYPE_BYTEARRAY;
            break;
        case 0xfedc304f: /* undefined */
            string_format = TCLDUK_TYPE_UNDEFINED;
            break;
        case 0x25cbfc4f: /* null */
            string_format = TCLDUK_TYPE_NULL;
            break;
        case 0xdae7f2ef: /* double */
            string_format = TCLDUK_TYPE_DOUBLE;
            break;
        case 0x5a0fab0b: /* integer */
            string_format = TCLDUK_TYPE_INTEGER;
            break;
        case 0x2d3dd6d7: /* bigint */
            string_format = TCLDUK_TYPE_BIGINT;
            break;
        case 0xa10ceeb7: /* array */
            string_format = TCLDUK_TYPE_ARRAY;
            break;
        case 0x6b072545: /* json */
            string_format = TCLDUK_TYPE_JSON;
            break;
        default:
            duk_push_error_object(ctx, DUK_ERR_ERROR, ERROR_INVALID_TYPE, type);
            return(-1);
            break;
    }

    switch (string_format) {
        case TCLDUK_TYPE_UNDEFINED:
            return(0);
        case TCLDUK_TYPE_BOOLEAN:
            tclRet = Tcl_GetBooleanFromObj(NULL, value, &valueBoolean);
            if (tclRet != TCL_OK) {
                return(0);
            }
            duk_push_boolean(ctx, valueBoolean);
            return(1);
        case TCLDUK_TYPE_NULL:
            duk_push_null(ctx);
            return(1);
        case TCLDUK_TYPE_BYTEARRAY:
            valueString = (const char *) Tcl_GetByteArrayFromObj(value, &valueStringLength);
            duk_push_lstring(ctx, valueString, valueStringLength);
            duk_to_buffer(ctx, -1, NULL);
            return(1);
        case TCLDUK_TYPE_BIGINT:
        case TCLDUK_TYPE_STRING:
        case TCLDUK_TYPE_JSON:
            valueString = Tcl_GetStringFromObj(value, &valueStringLength);
            duk_push_lstring(ctx, valueString, valueStringLength);

            /* If JSON is being pushed, convert to an object */
            if (string_format == TCLDUK_TYPE_JSON) {
                duk_json_decode(ctx, -1);
            }

            return(1);
        case TCLDUK_TYPE_DOUBLE:
        case TCLDUK_TYPE_INTEGER:
            tclRet = Tcl_GetDoubleFromObj(NULL, value, &valueDouble);
            if (tclRet != TCL_OK) {
                return(0);
            }
            if (valueDouble != valueDouble) {
                duk_push_nan(ctx);
            } else {
                duk_push_number(ctx, valueDouble);
            }
            return(1);
        case TCLDUK_TYPE_ARRAY:
            /*
             * Remove the first item from the list of types
             */
            tclRet = Tcl_ListObjReplace(NULL, typeObj, 0, 1, 0, NULL);
            otherTypesString = Tcl_GetStringFromObj(typeObj, NULL);

            /*
             * For each item in the list, format using this
             * function
             */
            tclRet = Tcl_ListObjLength(NULL, value, &numItems);
            if (tclRet != TCL_OK) {
                duk_push_null(ctx);
                return(1);
            }

            duk_push_array(ctx);
            for (idx = 0; idx < numItems; idx++) {
                tclRet = Tcl_ListObjIndex(NULL, value, idx, &itemObj);
                if (tclRet != TCL_OK) {
                    duk_push_null(ctx);
                } else {
                    checkRet = Tclduk_TclToJS(interp, itemObj, ctx, otherTypesString);
                    if (checkRet == 0) {
                        duk_push_null(ctx);
                    } else if (checkRet != 1) {
                        /* XXX:TODO: Handle this ? */
                    }
                }
                duk_put_prop_index(ctx, -2, idx);
            }

            return(1);
    }

    return(0);
}

/*
 * Evaluate a Tcl string and return the result to JavaScript
 */
static duk_ret_t EvalTclFromJSWithInterp(Tcl_Interp *interp, duk_context *ctx, const char *returnType) {
    Tcl_Obj *evalScript, *evalResult, *dukStringObj;
    duk_idx_t numArgs, numRetVals;
    int tclRet;
    int idx;

    numArgs = duk_get_top(ctx);
    if (numArgs < 0) {
        duk_push_error_object(ctx, DUK_ERR_ERROR, "%s", ERROR_INTERNAL_ARGS_ERROR);
        return(duk_throw(ctx));
    }

    evalScript = Tcl_NewListObj(0, NULL);
    for (idx = 0; idx < numArgs; idx++) {
        dukStringObj = Tclduk_JSToTcl(ctx, idx);
        if (!dukStringObj) {
            duk_push_error_object(ctx, DUK_ERR_TYPE_ERROR, "%s", ERROR_INVALID_STRING);
            return(duk_throw(ctx));
        }

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

    numRetVals = Tclduk_TclToJS(interp, evalResult, ctx, returnType);

    Tcl_FreeResult(interp);

    return(numRetVals);
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

    return(EvalTclFromJSWithInterp(interp, ctx, NULL));
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
    instanceData->lambdaCount = 0;
    instanceData->cdata = cdata;

    ctx = duk_create_heap(NULL, NULL, NULL, instanceData, NULL);
    if (ctx == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(ERROR_CREATE, -1));
        free(instanceData);
        return TCL_ERROR;
    }

    instanceData->ctx = ctx;

    DUKTCL_CDATA->counter++;
    token = Tcl_ObjPrintf(NS "::%d", DUKTCL_CDATA->counter);
    instanceData->handle = token;
    Tcl_IncrRefCount(token);

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
    struct DuktapeInstanceData *instanceData;
    duk_memory_functions funcs;
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

    duk_get_memory_functions(ctx, &funcs);
    instanceData = funcs.udata;

    Tcl_DecrRefCount(instanceData->handle);


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
    const char *returnType;

    duk_get_memory_functions(ctx, &funcs);
    instanceData = funcs.udata;

    interp = instanceData->interp;

    duk_push_current_function(ctx);          /* => [args...] [function] */
    duk_push_literal(ctx, "apply");          /* => [args...] [function] ["apply"] */
    duk_get_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("lambda"));  /* => [args...] [function] ["apply"] [lambda] */
    duk_insert(ctx, 0);                      /* => [lambda] [args...] [function] ["apply"] */
    duk_insert(ctx, 0);                      /* => ["apply"] [lambda] [args...] [function] */

    duk_get_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL("returnType"));  /* => [args...] [function] [returnType] */
    returnType = duk_get_string(ctx, -1);                           /* => [args...] [function] [returnType] */

    duk_pop(ctx);                            /* => ["apply"] [lambda] [args...] [function] */
    duk_pop(ctx);                            /* => ["apply"] [lambda] [args...] */

    return(EvalTclFromJSWithInterp(interp, ctx, returnType));
}

static int RegisterFunction_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    duk_context *ctx;
    Tcl_Obj *lambdaObj;
    const char *functionName, *returnType, *lambdaString;
    int lambdaStringLength, returnTypeLength;

    if (objc != 5 && objc != 6) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_TCL_FUNCTION);
        return(TCL_ERROR);
    }

    ctx = parse_id(cdata, interp, objv[1], 0);
    if (ctx == NULL) {
        return(TCL_ERROR);
    }

    functionName = Tcl_GetStringFromObj(objv[2], NULL);

    if (objc == 6) {
        returnType = Tcl_GetStringFromObj(objv[3], &returnTypeLength);
        objv++;
    } else {
        returnType = "string";
        returnTypeLength = 6;
    }

    lambdaObj = Tcl_NewListObj(2, objv + 3);
    lambdaString = Tcl_GetStringFromObj(lambdaObj, &lambdaStringLength);
    Tcl_DecrRefCount(lambdaObj);

    duk_push_global_object(ctx);                                   /* => [global] */
    duk_push_c_function(ctx, EvalTclCmdFromJS, DUK_VARARGS);       /* => [global] [function] */
    duk_push_lstring(ctx, lambdaString, lambdaStringLength);       /* => [global] [function] [lambda] */
    duk_put_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("lambda"));     /* => [global] [function] */
    duk_push_lstring(ctx, returnType, returnTypeLength);           /* => [global] [function] [returnType] */
    duk_put_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("returnType")); /* => [global] [function] */
    duk_put_prop_string(ctx, -2, functionName);                    /* => [global] */
    duk_pop(ctx);                                                  /* => */

    return(TCL_OK);
}

static int EvalLambda_Cmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    duk_context *ctx;
    Tcl_Obj *lambdaNameObj, *bytecodeObj, *result;
    const char *lambdaName, *bytecode;
    int lambdaNameLength, bytecodeLength;
    int idx;
    int retval;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 1, objv, USAGE_EVALLAMBDA);
        return(TCL_ERROR);
    }

    ctx = parse_id(cdata, interp, objv[1], 0);
    if (ctx == NULL) {
        return(TCL_ERROR);
    }

    bytecodeObj = objv[2];
    lambdaNameObj = objv[3];
    lambdaName = Tcl_GetStringFromObj(lambdaNameObj, &lambdaNameLength);

    duk_push_global_stash(ctx);                                   /* => [stash] */
    duk_get_prop_lstring(ctx, -1, lambdaName, lambdaNameLength);  /* => [stash] [function|undefined] */

    /*
     * If the lambda name cannot be found in the list of closures
     * use the bytecode
     */
    if (duk_is_undefined(ctx, -1) || lambdaNameLength == 0) {
        duk_pop(ctx);                                             /* => [stash] */

        bytecode = Tcl_GetStringFromObj(bytecodeObj, &bytecodeLength);

        duk_push_lstring(ctx, (const char *) bytecode, bytecodeLength); /* => [stash] [bytecodeString] */
        duk_base64_decode(ctx, -1);                               /* => [stash] [bytecodebuffer] */
        duk_load_function(ctx);                                   /* => [stash] [function] */
    }

    /*
     * Push each argument to the stack
     * => [stash] [function] [args...]
     */
    for (idx = 4; idx < objc; idx++) {
        Tclduk_TclToJS(interp, objv[idx], ctx, NULL);
    }

    /*
     * Call the JavaScript function
     */
    duk_call(ctx, objc - 4);                                      /* => [stash] [result] */

    retval = TCL_OK;
    if (duk_is_error(ctx, -1)) {
        retval = TCL_ERROR;
    }

    result = Tclduk_JSToTcl(ctx, -1);
    duk_pop(ctx);                                                 /* => [stash] */
    duk_pop(ctx);                                                 /* => */

    Tcl_SetObjResult(interp, result);

    return(retval);
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

    Tcl_RegisterObjType(&Tclduk_LambdaObjType);

    Tcl_CreateObjCommand(interp, NS INIT, Init_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS MAKE_SAFE, MakeSafe_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS MAKE_UNSAFE, MakeUnsafe_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS CLOSE, Close_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS EVAL, Eval_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS EVALLAMBDA, EvalLambda_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS TCL_FUNCTION, RegisterFunction_Cmd, duktape_data, NULL);
    Tcl_CreateObjCommand(interp, NS CALL_METHOD,
            CallMethod_Cmd, duktape_data, NULL);
    Tcl_CallWhenDeleted(interp, cleanup_interp, duktape_data);
    Tcl_PkgProvide(interp, PACKAGE, VERSION);

    return TCL_OK;
}
