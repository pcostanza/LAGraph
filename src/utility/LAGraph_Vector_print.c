//------------------------------------------------------------------------------
// LAGraph_Vector_print:  pretty-print a vector
//------------------------------------------------------------------------------

// LAGraph, (c) 2021 by The LAGraph Contributors, All Rights Reserved.
// SPDX-License-Identifier: BSD-2-Clause
// Contributed by Tim Davis, Texas A&M University.

//------------------------------------------------------------------------------

// LAGraph_Vector_print:  pretty-print a vector.  The type is either derived
// from GxB_Vector_type (if available) or assumed to be GrB_FP64 otherwise,
// or passed in as a parameter.
// Contributed by Tim Davis, Texas A&M.

#include "LG_internal.h"

#undef  LAGraph_FREE_WORK
#define LAGraph_FREE_WORK           \
{                                   \
    LAGraph_Free ((void **) &I) ;   \
    LAGraph_Free ((void **) &X) ;   \
}

#undef  LAGraph_FREE_ALL
#define LAGraph_FREE_ALL LAGraph_FREE_WORK

//------------------------------------------------------------------------------
// LG_Vector_print_TYPE: print with the specified type
//------------------------------------------------------------------------------

#define LG_VECTOR_PRINT(suffix,ctype,gtype,fmt1,fmt2)                       \
int LG_Vector_print_ ## suffix                                              \
(                                                                           \
    GrB_Vector v, int pr, FILE *f, char *msg                                \
)                                                                           \
{                                                                           \
    LG_CLEAR_MSG ;                                                          \
    ctype *X = NULL ;                                                       \
    GrB_Index *I = NULL ;                                                   \
    LG_ASSERT (v != NULL, GrB_NULL_POINTER) ;                               \
    LG_ASSERT (f != NULL, GrB_NULL_POINTER) ;                               \
    if (pr < 0) return (GrB_SUCCESS) ;                                      \
    /* get basic properties */                                              \
    GrB_Index n, nvals ;                                                    \
    GrB_TRY (GrB_Vector_size  (&n, v)) ;                                    \
    GrB_TRY (GrB_Vector_nvals (&nvals, v)) ;                                \
    /* print header line */                                                 \
    FPRINTF (f, "%s vector: n: %" PRIu64 " entries: %" PRIu64               \
        "\n", LG_XSTR (gtype), n, nvals) ;                                  \
    if (pr <= 1) return (GrB_SUCCESS) ;                                     \
    /* extract tuples */                                                    \
    I = LAGraph_Malloc (nvals, sizeof (GrB_Index)) ;                        \
    X = LAGraph_Malloc (nvals, sizeof (ctype)) ;                            \
    LG_ASSERT (I != NULL && X != NULL, GrB_OUT_OF_MEMORY) ;                 \
    GrB_Info info = GrB_Vector_extractTuples (I, X, &nvals, v) ;            \
    LG_ASSERT_MSG (info != GrB_DOMAIN_MISMATCH, GrB_NOT_IMPLEMENTED,        \
        "user-defined types not supported") ;                               \
    GrB_TRY (info) ;                                                        \
    /* determine the format */                                              \
    char *format = (pr <= 3) ? fmt1 : fmt2 ;                                \
    bool summary = (pr == 2 || pr == 4) && (nvals > 30) ;                   \
    for (int64_t k = 0 ; k < nvals ; k++)                                   \
    {                                                                       \
        /* print the kth tuple */                                           \
        GrB_Index i = I [k] ;                                               \
        ctype     x = X [k] ;                                               \
        FPRINTF (f, "    (%" PRIu64 ")   ", i) ;                            \
        FPRINTF (f, format, x) ;                                            \
        FPRINTF (f, "\n") ;                                                 \
        if (summary && k >= 29)                                             \
        {                                                                   \
            /* quit early if a only a summary is requested */               \
            FPRINTF (f, "    ...\n") ;                                      \
            break ;                                                         \
        }                                                                   \
    }                                                                       \
    LAGraph_FREE_WORK ;                                                     \
    return (GrB_SUCCESS) ;                                                  \
}

LG_VECTOR_PRINT (BOOL  , bool    , GrB_BOOL  , "%d"  , "%d"    ) ;
LG_VECTOR_PRINT (INT8  , int8_t  , GrB_INT8  , "%d"  , "%d"    ) ;
LG_VECTOR_PRINT (INT16 , int16_t , GrB_INT16 , "%d"  , "%d"    ) ;
LG_VECTOR_PRINT (INT32 , int32_t , GrB_INT32 , "%" PRId32, "%" PRId32  ) ;
LG_VECTOR_PRINT (INT64 , int64_t , GrB_INT64 , "%" PRId64, "%" PRId64  ) ;
LG_VECTOR_PRINT (UINT8 , uint8_t , GrB_UINT8 , "%d"  , "%d"    ) ;
LG_VECTOR_PRINT (UINT16, uint16_t, GrB_UINT16, "%d"  , "%d"    ) ;
LG_VECTOR_PRINT (UINT32, uint32_t, GrB_UINT32, "%" PRIu32, "%" PRIu32  ) ;
LG_VECTOR_PRINT (UINT64, uint64_t, GrB_UINT64, "%" PRIu64, "%" PRIu64  ) ;
LG_VECTOR_PRINT (FP32  , float   , GrB_FP32  , "%g"  , "%0.7g" ) ;
LG_VECTOR_PRINT (FP64  , double  , GrB_FP64  , "%g"  , "%0.15g") ;
#if 0
// would need to pass in an iscomplex flag to print creal(x) and cimag(x)
LG_VECTOR_PRINT (FC32  , GxB_FC32_t, GxB_FC32, ...) ;
LG_VECTOR_PRINT (FC64  , GxB_FC64_t, GxB_FC64, ...) ;
#endif

#undef  LAGraph_FREE_WORK
#define LAGraph_FREE_WORK ;
#undef  LAGraph_FREE_ALL
#define LAGraph_FREE_ALL ;

//------------------------------------------------------------------------------
// LAGraph_Vector_print_type: print with a specified type
//------------------------------------------------------------------------------

int LAGraph_Vector_print_type
(
    GrB_Vector v,       // vector to pretty-print to the file
    GrB_Type type,      // type to print
    int pr,             // print level: -1 nothing, 0: one line, 1: terse,
                        //      2: summary, 3: all,
                        //      4: as 2 but with %0.15g for float/double
                        //      5: as 3 but with %0.15g for float/double
    FILE *f,            // file to write it to, must be already open; use
                        // stdout or stderr to print to those locations.
    char *msg
)
{

    LG_CLEAR_MSG ;
    if (type == GrB_BOOL)
    {
        return (LG_Vector_print_BOOL (v, pr, f, msg)) ;
    }
    else if (type == GrB_INT8) 
    {
        return (LG_Vector_print_INT8 (v, pr, f, msg)) ;
    }
    else if (type == GrB_INT16) 
    {
        return (LG_Vector_print_INT16 (v, pr, f, msg)) ;
    }
    else if (type == GrB_INT32) 
    {
        return (LG_Vector_print_INT32 (v, pr, f, msg)) ;
    }
    else if (type == GrB_INT64) 
    {
        return (LG_Vector_print_INT64 (v, pr, f, msg)) ;
    }
    else if (type == GrB_UINT8) 
    {
        return (LG_Vector_print_UINT8 (v, pr, f, msg)) ;
    }
    else if (type == GrB_UINT16) 
    {
        return (LG_Vector_print_UINT16 (v, pr, f, msg)) ;
    }
    else if (type == GrB_UINT32) 
    {
        return (LG_Vector_print_UINT32 (v, pr, f, msg)) ;
    }
    else if (type == GrB_UINT64) 
    {
        return (LG_Vector_print_UINT64 (v, pr, f, msg)) ;
    }
    else if (type == GrB_FP32) 
    {
        return (LG_Vector_print_FP32 (v, pr, f, msg)) ;
    }
    else if (type == GrB_FP64) 
    {
        return (LG_Vector_print_FP64 (v, pr, f, msg)) ;
    }
    #if 0
    else if (type == GxB_FC32)
    {
        return (LG_Vector_print_FC32 (v, pr, f, msg)) ;
    }
    else if (type == GxB_FC32)
    {
        return (LG_Vector_print_FC64 (v, pr, f, msg)) ;
    }
    #endif
    else
    {
        LG_ASSERT_MSG (false, GrB_NOT_IMPLEMENTED,
            "user-defined types not supported") ;
        return (GrB_SUCCESS) ;
    }
}

//------------------------------------------------------------------------------
// LAGraph_Vector_print: automatically determine the type
//------------------------------------------------------------------------------

int LAGraph_Vector_print
(
    GrB_Vector v,       // vector to pretty-print to the file
    int pr,             // print level: -1 nothing, 0: one line, 1: terse,
                        //      2: summary, 3: all,
                        //      4: as 2 but with %0.15g for float/double
                        //      5: as 3 but with %0.15g for float/double
    FILE *f,            // file to write it to, must be already open; use
                        // stdout or stderr to print to those locations.
    char *msg
)
{

    //--------------------------------------------------------------------------
    // check inputs
    //--------------------------------------------------------------------------

    LG_CLEAR_MSG ;
    LG_ASSERT (v != NULL, GrB_NULL_POINTER) ;
    LG_ASSERT (f != NULL, GrB_NULL_POINTER) ;

    //--------------------------------------------------------------------------
    // determine the type
    //--------------------------------------------------------------------------

    GrB_Type type ;
    #if LG_SUITESPARSE
        // SuiteSparse:GraphBLAS: query the type and print accordingly
        GrB_TRY (GxB_Vector_type (&type, v)) ;
    #else
        // no way to determine the type with pure GrB*; print as if FP64
        type = GrB_FP64 ;
    #endif

    //--------------------------------------------------------------------------
    // print the matrix
    //--------------------------------------------------------------------------

    return (LAGraph_Vector_print_type (v, type, pr, f, msg)) ;
}

