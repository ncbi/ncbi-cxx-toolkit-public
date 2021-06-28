#ifndef VDBSRC_ERROR_PRIV__H
#define VDBSRC_ERROR_PRIV__H

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Vahram Avagyan
 *
 */

/// @file error_priv.h
/// File contains definitions of error messages and error handling functions.
///
/// This header file includes most common files used internally by the library,
/// including the SRA headers and Blast core headers. It defines constants
/// used for retrieving and storing data from the SRAs, including relevant
/// column names and data types, thresholds for dropping tiny reads, maximum
/// allowed storage space for names, etc.

#include "common_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================//
// Error codes

/// Local error codes that the library can generate.
enum VDBSRC_ErrCode
{
   eVDBSRC_NO_ERROR,             	///< No errors
   eVDBSRC_MGR_LOAD_ERROR,       	///< VDB Manager could not be loaded
   eVDBSRC_MGR_FREE_ERROR,       	///< VDB Manager could not be released
   eVDBSRC_RUNSET_LOAD_ERROR,       ///< VDB RunSet could not be loaded
   eVDBSRC_REFSET_LOAD_ERROR,       ///< VDB RunSet could not be loaded
   eVDBSRC_RUNSET_FREE_ERROR,       ///< VDB RunSet could not be released
   eVDBSRC_UNINIT_VDB_DATA_ERROR,  	///< VDB Data has not be initialized
   eVDBSRC_ADD_RUNS_ERROR, 			///< Failed to add any run to RunSet
   eVDBSRC_GET_RUNSET_NAME_ERROR, 	///< Failed get run set name
   eVDBSRC_GET_RUNSET_NUM_SEQ_ERROR,///< Failed get num of seq
   eVDBSRC_READER_2NA_ERROR,       	///< Failed to process the NCBI-2na data
   eVDBSRC_READER_4NA_ERROR,       	///< Failed to process the NCBI-4na data
   eVDBSRC_READ_2NA_CACHE_ERROR,    ///< Failed to read 2na (cache)
   eVDBSRC_READ_2NA_COPY_ERROR,     ///< Failed to read 2na (copy buffer)
   eVDBSRC_READ_4NA_CACHE_ERROR,    ///< Failed to read 4na (cache)
   eVDBSRC_READ_4NA_COPY_ERROR,     ///< Failed to read 4na (copy buffer)
   eVDBSRC_NO_MEM_FOR_VDBDATA,     	///< No memory for the VDB data
   eVDBSRC_NO_MEM_FOR_RUNS,      	///< No memory for an VDB accession
   eVDBSRC_NO_MEM_FOR_CHUNK_SEQ,    ///< No memory for chunk seq
   eVDBSRC_SEQ_LENGTH_ERROR,      	///< VDB returns invalid seq length
   eVDBSRC_READ_ID_MISMATCH,      	///< Read id mismatch (requested id != retrieved id)
   eVDBSRC_4NA_SEQ_STRING_ERROR,    ///< 4na convert to string error
   eVDBSRC_2NA_SEQ_STRING_ERROR,    ///< 2na convert to string error
   eVDBSRC_NUM_SEQ_OVERFLOW_ERROR,  ///< Num of Seqs overflow
   eVDBSRC_FILTERED_READ,			///< oid correpsond to filtered read
   eVDBSRC_ID_OUT_OF_RANGE,			///< oid is out of range
   eVDBSRC_4NA_REF_SEQ_BUF_OVERFLOW	///< 4na seq overflow
};
typedef enum VDBSRC_ErrCode TVDBErrCode;

// ==========================================================================//
// Error message structure

/// Structure describing the error messages the library can generate.
struct SVDBSRC_ErrMsg
{
    Boolean isError;            ///< True if the object describes an error
    uint32_t resultCode;            ///< SRA error, rcNoErr if the error is local
    TVDBErrCode localCode;    ///< Local error code, set by this library
    char* msgContext;           ///< Optional context string for the error
};
typedef struct SVDBSRC_ErrMsg TVDBErrMsg;

// ==========================================================================//
// Error message functions

/// Initialize an Error message.
/// @param sraErrMsg Pointer to an existing Error message object. [out]
/// @param rc SRA library error code (usually returned from an SRA call). [in]
/// @param localCode Local error code, provided in addition to the SRA code. [in]
void
VDBSRC_InitErrorMsg(TVDBErrMsg* sraErrMsg,
                      uint32_t rc,
                      TVDBErrCode localCode);

/// Initialize an Error message with a context string.
/// @param sraErrMsg Pointer to an existing Error message object. [out]
/// @param rc SRA library error code (usually returned from an SRA call). [in]
/// @param localCode Local error code, provided in addition to the SRA code.  [in]
/// @param msgContext Properly initialized non-empty context string. [in]
void
VDBSRC_InitErrorMsgWithContext(TVDBErrMsg* sraErrMsg,
                                 uint32_t rc,
                                 TVDBErrCode localCode,
                                 const char* msgContext);

/// Initialize an Error message that is local to this library.
///
/// This function assumes no SRA library errors have occurred,
/// so it only needs to store the local error code.
///
/// @param sraErrMsg Pointer to an existing Error message object. [out]
/// @param localCode Local error code. [in]
void
VDBSRC_InitLocalErrorMsg(TVDBErrMsg* sraErrMsg,
                           TVDBErrCode localCode);

/// Initialize an empty Error message (No Error).
/// @param sraErrMsg Pointer to an existing Error message object. [out]
void
VDBSRC_InitEmptyErrorMsg(TVDBErrMsg* sraErrMsg);

/// Release the Error message.
///
/// This function should be called when an Error message is no longer
/// used. It frees any allocated members and calls all other
/// finalization routines.
///
/// @param sraErrMsg Pointer to an existing Error message object. [out]
void
VDBSRC_ReleaseErrorMsg(TVDBErrMsg* sraErrMsg);

/// Format the error message as a single human-readable string.
/// @param errMsg Pointer to a string to be allocated and populated. [out]
/// @param sraErrMsg Pointer to an existing Error message object. [out]
void
VDBSRC_FormatErrorMsg(char** errMsg, const TVDBErrMsg* vdbErrMsg);

// ==========================================================================//

#ifdef __cplusplus
}
#endif

#endif /* VDBSRC_ERROR_PRIV__H */
