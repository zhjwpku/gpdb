/*-------------------------------------------------------------------------
 *
 * fmgr.h
 *	  Definitions for the Postgres function manager and function-call
 *	  interface.
 *
 * This file must be included by all Postgres modules that either define
 * or call fmgr-callable functions.
 *
 *
 * Portions Copyright (c) 1996-2001, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $Id: fmgr.h,v 1.15 2001/10/06 23:21:44 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef FMGR_H
#define FMGR_H


/*
 * All functions that can be called directly by fmgr must have this signature.
 * (Other functions can be called by using a handler that does have this
 * signature.)
 */

typedef struct FunctionCallInfoData *FunctionCallInfo;

typedef Datum (*PGFunction) (FunctionCallInfo fcinfo);

/*
 * This struct holds the system-catalog information that must be looked up
 * before a function can be called through fmgr.  If the same function is
 * to be called multiple times, the lookup need be done only once and the
 * info struct saved for re-use.
 */
typedef struct FmgrInfo
{
	PGFunction	fn_addr;		/* pointer to function or handler to be
								 * called */
	Oid			fn_oid;			/* OID of function (NOT of handler, if
								 * any) */
	short		fn_nargs;		/* 0..FUNC_MAX_ARGS, or -1 if variable arg
								 * count */
	bool		fn_strict;		/* function is "strict" (NULL in => NULL
								 * out) */
	bool		fn_retset;		/* function returns a set (over multiple
								 * calls) */
	void	   *fn_extra;		/* extra space for use by handler */
	MemoryContext fn_mcxt;		/* memory context to store fn_extra in */
} FmgrInfo;

/*
 * This struct is the data actually passed to an fmgr-called function.
 */
typedef struct FunctionCallInfoData
{
	FmgrInfo   *flinfo;			/* ptr to lookup info used for this call */
	struct Node *context;		/* pass info about context of call */
	struct Node *resultinfo;	/* pass or return extra info about result */
	bool		isnull;			/* function must set true if result is
								 * NULL */
	short		nargs;			/* # arguments actually passed */
	Datum		arg[FUNC_MAX_ARGS];		/* Arguments passed to function */
	bool		argnull[FUNC_MAX_ARGS]; /* T if arg[i] is actually NULL */
} FunctionCallInfoData;

/*
 * This routine fills a FmgrInfo struct, given the OID
 * of the function to be called.
 */
extern void fmgr_info(Oid functionId, FmgrInfo *finfo);

/*
 * Same, when the FmgrInfo struct is in a memory context longer-lived than
 * CurrentMemoryContext.  The specified context will be set as fn_mcxt
 * and used to hold all subsidiary data of finfo.
 */
extern void fmgr_info_cxt(Oid functionId, FmgrInfo *finfo,
						  MemoryContext mcxt);

/*
 * Copy an FmgrInfo struct
 */
extern void fmgr_info_copy(FmgrInfo *dstinfo, FmgrInfo *srcinfo,
						   MemoryContext destcxt);

/*
 * This macro invokes a function given a filled-in FunctionCallInfoData
 * struct.	The macro result is the returned Datum --- but note that
 * caller must still check fcinfo->isnull!	Also, if function is strict,
 * it is caller's responsibility to verify that no null arguments are present
 * before calling.
 */
#define FunctionCallInvoke(fcinfo)	((* (fcinfo)->flinfo->fn_addr) (fcinfo))


/*-------------------------------------------------------------------------
 *		Support macros to ease writing fmgr-compatible functions
 *
 * A C-coded fmgr-compatible function should be declared as
 *
 *		Datum
 *		function_name(PG_FUNCTION_ARGS)
 *		{
 *			...
 *		}
 *
 * It should access its arguments using appropriate PG_GETARG_xxx macros
 * and should return its result using PG_RETURN_xxx.
 *
 *-------------------------------------------------------------------------
 */

/* Standard parameter list for fmgr-compatible functions */
#define PG_FUNCTION_ARGS	FunctionCallInfo fcinfo

/*
 * If function is not marked "proisstrict" in pg_proc, it must check for
 * null arguments using this macro.  Do not try to GETARG a null argument!
 */
#define PG_ARGISNULL(n)  (fcinfo->argnull[n])

/*
 * Support for fetching detoasted copies of toastable datatypes (all of
 * which are varlena types).  pg_detoast_datum() gives you either the input
 * datum (if not toasted) or a detoasted copy allocated with palloc().
 * pg_detoast_datum_copy() always gives you a palloc'd copy --- use it
 * if you need a modifiable copy of the input.	Caller is expected to have
 * checked for null inputs first, if necessary.
 *
 * Note: it'd be nice if these could be macros, but I see no way to do that
 * without evaluating the arguments multiple times, which is NOT acceptable.
 */
extern struct varlena *pg_detoast_datum(struct varlena * datum);
extern struct varlena *pg_detoast_datum_copy(struct varlena * datum);

#define PG_DETOAST_DATUM(datum) \
	pg_detoast_datum((struct varlena *) DatumGetPointer(datum))
#define PG_DETOAST_DATUM_COPY(datum) \
	pg_detoast_datum_copy((struct varlena *) DatumGetPointer(datum))

/*
 * Support for cleaning up detoasted copies of inputs.	This must only
 * be used for pass-by-ref datatypes, and normally would only be used
 * for toastable types.  If the given pointer is different from the
 * original argument, assume it's a palloc'd detoasted copy, and pfree it.
 * NOTE: most functions on toastable types do not have to worry about this,
 * but we currently require that support functions for indexes not leak
 * memory.
 */
#define PG_FREE_IF_COPY(ptr,n) \
	do { \
		if ((Pointer) (ptr) != PG_GETARG_POINTER(n)) \
			pfree(ptr); \
	} while (0)

/* Macros for fetching arguments of standard types */

#define PG_GETARG_DATUM(n)	 (fcinfo->arg[n])
#define PG_GETARG_INT32(n)	 DatumGetInt32(PG_GETARG_DATUM(n))
#define PG_GETARG_UINT32(n)  DatumGetUInt32(PG_GETARG_DATUM(n))
#define PG_GETARG_INT16(n)	 DatumGetInt16(PG_GETARG_DATUM(n))
#define PG_GETARG_UINT16(n)  DatumGetUInt16(PG_GETARG_DATUM(n))
#define PG_GETARG_CHAR(n)	 DatumGetChar(PG_GETARG_DATUM(n))
#define PG_GETARG_BOOL(n)	 DatumGetBool(PG_GETARG_DATUM(n))
#define PG_GETARG_OID(n)	 DatumGetObjectId(PG_GETARG_DATUM(n))
#define PG_GETARG_POINTER(n) DatumGetPointer(PG_GETARG_DATUM(n))
#define PG_GETARG_CSTRING(n) DatumGetCString(PG_GETARG_DATUM(n))
#define PG_GETARG_NAME(n)	 DatumGetName(PG_GETARG_DATUM(n))
/* these macros hide the pass-by-reference-ness of the datatype: */
#define PG_GETARG_FLOAT4(n)  DatumGetFloat4(PG_GETARG_DATUM(n))
#define PG_GETARG_FLOAT8(n)  DatumGetFloat8(PG_GETARG_DATUM(n))
#define PG_GETARG_INT64(n)	 DatumGetInt64(PG_GETARG_DATUM(n))
/* use this if you want the raw, possibly-toasted input datum: */
#define PG_GETARG_RAW_VARLENA_P(n)	((struct varlena *) PG_GETARG_POINTER(n))
/* use this if you want the input datum de-toasted: */
#define PG_GETARG_VARLENA_P(n) PG_DETOAST_DATUM(PG_GETARG_DATUM(n))
/* DatumGetFoo macros for varlena types will typically look like this: */
#define DatumGetByteaP(X)			((bytea *) PG_DETOAST_DATUM(X))
#define DatumGetTextP(X)			((text *) PG_DETOAST_DATUM(X))
#define DatumGetBpCharP(X)			((BpChar *) PG_DETOAST_DATUM(X))
#define DatumGetVarCharP(X)			((VarChar *) PG_DETOAST_DATUM(X))
/* And we also offer variants that return an OK-to-write copy */
#define DatumGetByteaPCopy(X)		((bytea *) PG_DETOAST_DATUM_COPY(X))
#define DatumGetTextPCopy(X)		((text *) PG_DETOAST_DATUM_COPY(X))
#define DatumGetBpCharPCopy(X)		((BpChar *) PG_DETOAST_DATUM_COPY(X))
#define DatumGetVarCharPCopy(X)		((VarChar *) PG_DETOAST_DATUM_COPY(X))
/* GETARG macros for varlena types will typically look like this: */
#define PG_GETARG_BYTEA_P(n)		DatumGetByteaP(PG_GETARG_DATUM(n))
#define PG_GETARG_TEXT_P(n)			DatumGetTextP(PG_GETARG_DATUM(n))
#define PG_GETARG_BPCHAR_P(n)		DatumGetBpCharP(PG_GETARG_DATUM(n))
#define PG_GETARG_VARCHAR_P(n)		DatumGetVarCharP(PG_GETARG_DATUM(n))
/* And we also offer variants that return an OK-to-write copy */
#define PG_GETARG_BYTEA_P_COPY(n)	DatumGetByteaPCopy(PG_GETARG_DATUM(n))
#define PG_GETARG_TEXT_P_COPY(n)	DatumGetTextPCopy(PG_GETARG_DATUM(n))
#define PG_GETARG_BPCHAR_P_COPY(n)	DatumGetBpCharPCopy(PG_GETARG_DATUM(n))
#define PG_GETARG_VARCHAR_P_COPY(n) DatumGetVarCharPCopy(PG_GETARG_DATUM(n))

/* To return a NULL do this: */
#define PG_RETURN_NULL()  \
	do { fcinfo->isnull = true; return (Datum) 0; } while (0)

/* A few internal functions return void (which is not the same as NULL!) */
#define PG_RETURN_VOID()	 return (Datum) 0

/* Macros for returning results of standard types */

#define PG_RETURN_DATUM(x)	 return (x)
#define PG_RETURN_INT32(x)	 return Int32GetDatum(x)
#define PG_RETURN_UINT32(x)  return UInt32GetDatum(x)
#define PG_RETURN_INT16(x)	 return Int16GetDatum(x)
#define PG_RETURN_CHAR(x)	 return CharGetDatum(x)
#define PG_RETURN_BOOL(x)	 return BoolGetDatum(x)
#define PG_RETURN_OID(x)	 return ObjectIdGetDatum(x)
#define PG_RETURN_POINTER(x) return PointerGetDatum(x)
#define PG_RETURN_CSTRING(x) return CStringGetDatum(x)
#define PG_RETURN_NAME(x)	 return NameGetDatum(x)
/* these macros hide the pass-by-reference-ness of the datatype: */
#define PG_RETURN_FLOAT4(x)  return Float4GetDatum(x)
#define PG_RETURN_FLOAT8(x)  return Float8GetDatum(x)
#define PG_RETURN_INT64(x)	 return Int64GetDatum(x)
/* RETURN macros for other pass-by-ref types will typically look like this: */
#define PG_RETURN_BYTEA_P(x)   PG_RETURN_POINTER(x)
#define PG_RETURN_TEXT_P(x)    PG_RETURN_POINTER(x)
#define PG_RETURN_BPCHAR_P(x)  PG_RETURN_POINTER(x)
#define PG_RETURN_VARCHAR_P(x) PG_RETURN_POINTER(x)


/*-------------------------------------------------------------------------
 *		Support for detecting call convention of dynamically-loaded functions
 *
 * Dynamically loaded functions may use either the version-1 ("new style")
 * or version-0 ("old style") calling convention.  Version 1 is the call
 * convention defined in this header file; version 0 is the old "plain C"
 * convention.	A version-1 function must be accompanied by the macro call
 *
 *		PG_FUNCTION_INFO_V1(function_name);
 *
 * Note that internal functions do not need this decoration since they are
 * assumed to be version-1.
 *
 *-------------------------------------------------------------------------
 */

typedef struct
{
	int			api_version;	/* specifies call convention version
								 * number */
	/* More fields may be added later, for version numbers > 1. */
} Pg_finfo_record;

/* Expected signature of an info function */
typedef Pg_finfo_record *(*PGFInfoFunction) (void);

/* Macro to build an info function associated with the given function name */

#define PG_FUNCTION_INFO_V1(funcname) \
extern Pg_finfo_record * CppConcat(pg_finfo_,funcname) (void); \
Pg_finfo_record * \
CppConcat(pg_finfo_,funcname) (void) \
{ \
	static Pg_finfo_record my_finfo = { 1 }; \
	return &my_finfo; \
}


/*-------------------------------------------------------------------------
 *		Support routines and macros for callers of fmgr-compatible functions
 *-------------------------------------------------------------------------
 */

/* These are for invocation of a specifically named function with a
 * directly-computed parameter list.  Note that neither arguments nor result
 * are allowed to be NULL.
 */
extern Datum DirectFunctionCall1(PGFunction func, Datum arg1);
extern Datum DirectFunctionCall2(PGFunction func, Datum arg1, Datum arg2);
extern Datum DirectFunctionCall3(PGFunction func, Datum arg1, Datum arg2,
					Datum arg3);
extern Datum DirectFunctionCall4(PGFunction func, Datum arg1, Datum arg2,
					Datum arg3, Datum arg4);
extern Datum DirectFunctionCall5(PGFunction func, Datum arg1, Datum arg2,
					Datum arg3, Datum arg4, Datum arg5);
extern Datum DirectFunctionCall6(PGFunction func, Datum arg1, Datum arg2,
					Datum arg3, Datum arg4, Datum arg5,
					Datum arg6);
extern Datum DirectFunctionCall7(PGFunction func, Datum arg1, Datum arg2,
					Datum arg3, Datum arg4, Datum arg5,
					Datum arg6, Datum arg7);
extern Datum DirectFunctionCall8(PGFunction func, Datum arg1, Datum arg2,
					Datum arg3, Datum arg4, Datum arg5,
					Datum arg6, Datum arg7, Datum arg8);
extern Datum DirectFunctionCall9(PGFunction func, Datum arg1, Datum arg2,
					Datum arg3, Datum arg4, Datum arg5,
					Datum arg6, Datum arg7, Datum arg8,
					Datum arg9);

/* These are for invocation of a previously-looked-up function with a
 * directly-computed parameter list.  Note that neither arguments nor result
 * are allowed to be NULL.
 */
extern Datum FunctionCall1(FmgrInfo *flinfo, Datum arg1);
extern Datum FunctionCall2(FmgrInfo *flinfo, Datum arg1, Datum arg2);
extern Datum FunctionCall3(FmgrInfo *flinfo, Datum arg1, Datum arg2,
			  Datum arg3);
extern Datum FunctionCall4(FmgrInfo *flinfo, Datum arg1, Datum arg2,
			  Datum arg3, Datum arg4);
extern Datum FunctionCall5(FmgrInfo *flinfo, Datum arg1, Datum arg2,
			  Datum arg3, Datum arg4, Datum arg5);
extern Datum FunctionCall6(FmgrInfo *flinfo, Datum arg1, Datum arg2,
			  Datum arg3, Datum arg4, Datum arg5,
			  Datum arg6);
extern Datum FunctionCall7(FmgrInfo *flinfo, Datum arg1, Datum arg2,
			  Datum arg3, Datum arg4, Datum arg5,
			  Datum arg6, Datum arg7);
extern Datum FunctionCall8(FmgrInfo *flinfo, Datum arg1, Datum arg2,
			  Datum arg3, Datum arg4, Datum arg5,
			  Datum arg6, Datum arg7, Datum arg8);
extern Datum FunctionCall9(FmgrInfo *flinfo, Datum arg1, Datum arg2,
			  Datum arg3, Datum arg4, Datum arg5,
			  Datum arg6, Datum arg7, Datum arg8,
			  Datum arg9);

/* These are for invocation of a function identified by OID with a
 * directly-computed parameter list.  Note that neither arguments nor result
 * are allowed to be NULL.	These are essentially FunctionLookup() followed
 * by FunctionCallN().	If the same function is to be invoked repeatedly,
 * do the FunctionLookup() once and then use FunctionCallN().
 */
extern Datum OidFunctionCall1(Oid functionId, Datum arg1);
extern Datum OidFunctionCall2(Oid functionId, Datum arg1, Datum arg2);
extern Datum OidFunctionCall3(Oid functionId, Datum arg1, Datum arg2,
				 Datum arg3);
extern Datum OidFunctionCall4(Oid functionId, Datum arg1, Datum arg2,
				 Datum arg3, Datum arg4);
extern Datum OidFunctionCall5(Oid functionId, Datum arg1, Datum arg2,
				 Datum arg3, Datum arg4, Datum arg5);
extern Datum OidFunctionCall6(Oid functionId, Datum arg1, Datum arg2,
				 Datum arg3, Datum arg4, Datum arg5,
				 Datum arg6);
extern Datum OidFunctionCall7(Oid functionId, Datum arg1, Datum arg2,
				 Datum arg3, Datum arg4, Datum arg5,
				 Datum arg6, Datum arg7);
extern Datum OidFunctionCall8(Oid functionId, Datum arg1, Datum arg2,
				 Datum arg3, Datum arg4, Datum arg5,
				 Datum arg6, Datum arg7, Datum arg8);
extern Datum OidFunctionCall9(Oid functionId, Datum arg1, Datum arg2,
				 Datum arg3, Datum arg4, Datum arg5,
				 Datum arg6, Datum arg7, Datum arg8,
				 Datum arg9);


/*
 * Routines in fmgr.c
 */
extern Pg_finfo_record *fetch_finfo_record(void *filehandle, char *funcname);
extern Oid	fmgr_internal_function(const char *proname);

/*
 * Routines in dfmgr.c
 */
extern char * Dynamic_library_path;

extern PGFunction load_external_function(char *filename, char *funcname,
					   bool signalNotFound, void **filehandle);
extern PGFunction lookup_external_function(void *filehandle, char *funcname);
extern void load_file(char *filename);


/*
 * !!! OLD INTERFACE !!!
 *
 * fmgr() is the only remaining vestige of the old-style caller support
 * functions.  It's no longer used anywhere in the Postgres distribution,
 * but we should leave it around for a release or two to ease the transition
 * for user-supplied C functions.  OidFunctionCallN() replaces it for new
 * code.
 */

/*
 * DEPRECATED, DO NOT USE IN NEW CODE
 */
extern char *fmgr(Oid procedureId,...);

#endif	 /* FMGR_H */
