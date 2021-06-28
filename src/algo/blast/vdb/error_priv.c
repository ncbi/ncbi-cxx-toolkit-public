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

#include "error_priv.h"
#include <klib/writer.h>    /* for RCExplain */

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================//
// Error message strings

#define VDBSRC_NO_ERROR	"No errors reported"
#define VDBSRC_MGR_LOAD_ERROR "Failed to create the VDB manager object"
#define VDBSRC_MGR_FREE_ERROR "Failed to release the VDB manager object"
#define VDBSRC_RUNSET_LOAD_ERROR "Failed to create VDB runset"
#define VDBSRC_REFSET_LOAD_ERROR "Failed to create VDB ref set"
#define VDBSRC_RUNSET_FREE_ERROR "Failed to release VDB runset"
#define VDBSRC_UNINIT_VDB_DATA_ERROR "VDB Data not initialized"
#define VDBSRC_ADD_RUNS_ERROR "Failed to add any run to VDB runset"
#define VDBSRC_GET_RUNSET_NAME_ERROR "Failed to get run set name"
#define VDBSRC_GET_RUNSET_NUM_SEQ_ERROR "Failed to get num of sequences"
#define VDBSRC_READER_2NA_ERROR	"2na reader error"
#define VDBSRC_READER_4NA_ERROR	"4na reader error"
#define VDBSRC_READER_2NA_CACHE_ERROR "Failed to read 2na seq from cache"
#define VDBSRC_READER_2NA_COPY_ERROR "Failed to read 2na seq using copy buffer"
#define VDBSRC_READER_4NA_CACHE_ERROR "Failed to read 4na seq from cache"
#define VDBSRC_READ_4NA_COPY_ERROR "Failed to read 4na seq using copy buffer"
#define VDBSRC_NO_MEM_FOR_VDBDATA "Failed to allocate memory for VDB data"
#define VDBSRC_NO_MEM_FOR_RUNS "Failed to allocate memory for the list of VDB Runs"
#define VDBSRC_NO_MEM_FOR_CHUNK_SEQ "Failed to allocate memory for chunk seq"
#define VDBSRC_SEQ_LENGTH_ERROR "Failed to get seq length"
#define VDBSRC_READ_ID_MISMATCH "Mismatch read id"
#define VDBSRC_4NA_SEQ_STRING_ERROR   "4na seq convert to string error"
#define VDBSRC_2NA_SEQ_STRING_ERROR   "2na seq convert to string error"
#define VDBSRC_NUM_SEQ_OVERFLOW_ERROR   "Num of seqs overflow"
#define VDBSRC_FILTERED_READ "Id correpsonds to filtered read"
#define VDBSRC_ID_OUT_OF_RANGE	"Id is out of range"
#define VDBSRC_4NA_REF_SEQ_BUF_OVERFLOW "4NA ref seq buffer overflow"

// ==========================================================================//
// Error message functions

void
VDBSRC_InitErrorMsg(TVDBErrMsg* vdbErrMsg,
               uint32_t rc,
               TVDBErrCode localCode)
{
    ASSERT(vdbErrMsg);

    if (vdbErrMsg->msgContext)
        free(vdbErrMsg->msgContext);

    vdbErrMsg->isError = (localCode != eVDBSRC_NO_ERROR);
    vdbErrMsg->resultCode = rc;
    vdbErrMsg->localCode = localCode;
    vdbErrMsg->msgContext = NULL;
}

void
VDBSRC_InitErrorMsgWithContext(TVDBErrMsg* vdbErrMsg,
                          uint32_t rc,
                          TVDBErrCode localCode,
                          const char* msgContext)
{
    ASSERT(vdbErrMsg);
    ASSERT(msgContext);

    if (vdbErrMsg->msgContext)
    {
        free(vdbErrMsg->msgContext);
        vdbErrMsg->msgContext = NULL;
    }

    vdbErrMsg->isError = 1;
    vdbErrMsg->resultCode = rc;
    vdbErrMsg->localCode = localCode;

    if (msgContext)
        vdbErrMsg->msgContext = strdup(msgContext);
    else
        vdbErrMsg->msgContext = NULL;
}

void
VDBSRC_InitLocalErrorMsg(TVDBErrMsg* vdbErrMsg,
                           TVDBErrCode localCode)
{
    ASSERT(vdbErrMsg);

    if (vdbErrMsg->msgContext)
    {
        free(vdbErrMsg->msgContext);
        vdbErrMsg->msgContext = NULL;
    }

    vdbErrMsg->isError = (localCode != eVDBSRC_NO_ERROR);
    vdbErrMsg->resultCode = 0;
    vdbErrMsg->localCode = localCode;
    vdbErrMsg->msgContext = NULL;
}

void
VDBSRC_InitEmptyErrorMsg(TVDBErrMsg* vdbErrMsg)
{
    ASSERT(vdbErrMsg);

    vdbErrMsg->isError = 0;
    vdbErrMsg->resultCode = 0;
    vdbErrMsg->localCode = eVDBSRC_NO_ERROR;
    vdbErrMsg->msgContext = NULL;
}

void
VDBSRC_ReleaseErrorMsg(TVDBErrMsg* vdbErrMsg)
{
    ASSERT(vdbErrMsg);

    if (vdbErrMsg->msgContext)
    {
        free(vdbErrMsg->msgContext);
        vdbErrMsg->msgContext = NULL;
    }
}

void
VDBSRC_FormatErrorMsg(char** errMsg,
                        const TVDBErrMsg* vdbErrMsg)
{
	const char* msgPrefix = 0;
	const char* msgBody = 0;
	 size_t lengthTotal = 32;
    ASSERT(vdbErrMsg);

    // get the message summary / prefix
    switch (vdbErrMsg->localCode)
    {
    case eVDBSRC_NO_ERROR: msgPrefix = VDBSRC_NO_ERROR; break;
    case eVDBSRC_MGR_LOAD_ERROR: msgPrefix = VDBSRC_MGR_LOAD_ERROR; break;
    case eVDBSRC_MGR_FREE_ERROR: msgPrefix = VDBSRC_MGR_FREE_ERROR; break;
    case eVDBSRC_RUNSET_LOAD_ERROR: msgPrefix = VDBSRC_RUNSET_LOAD_ERROR; break;
    case eVDBSRC_REFSET_LOAD_ERROR: msgPrefix = VDBSRC_REFSET_LOAD_ERROR; break;
    case eVDBSRC_RUNSET_FREE_ERROR: msgPrefix = VDBSRC_RUNSET_FREE_ERROR; break;
    case eVDBSRC_UNINIT_VDB_DATA_ERROR: msgPrefix = VDBSRC_UNINIT_VDB_DATA_ERROR; break;
    case eVDBSRC_ADD_RUNS_ERROR: msgPrefix = VDBSRC_ADD_RUNS_ERROR; break;
    case eVDBSRC_GET_RUNSET_NAME_ERROR: msgPrefix = VDBSRC_GET_RUNSET_NAME_ERROR; break;
    case eVDBSRC_GET_RUNSET_NUM_SEQ_ERROR: msgPrefix = VDBSRC_GET_RUNSET_NUM_SEQ_ERROR; break;
    case eVDBSRC_READER_2NA_ERROR: msgPrefix = VDBSRC_READER_2NA_ERROR; break;
    case eVDBSRC_READER_4NA_ERROR: msgPrefix = VDBSRC_READER_4NA_ERROR; break;
    case eVDBSRC_READ_2NA_CACHE_ERROR: msgPrefix = VDBSRC_READER_2NA_CACHE_ERROR; break;
    case eVDBSRC_READ_2NA_COPY_ERROR: msgPrefix = VDBSRC_READER_2NA_COPY_ERROR; break;
    case eVDBSRC_READ_4NA_CACHE_ERROR: msgPrefix = VDBSRC_READER_4NA_CACHE_ERROR; break;
    case eVDBSRC_READ_4NA_COPY_ERROR: msgPrefix = VDBSRC_READ_4NA_COPY_ERROR; break;
    case eVDBSRC_NO_MEM_FOR_VDBDATA: msgPrefix = VDBSRC_NO_MEM_FOR_VDBDATA; break;
    case eVDBSRC_NO_MEM_FOR_RUNS: msgPrefix = VDBSRC_NO_MEM_FOR_RUNS; break;
    case eVDBSRC_NO_MEM_FOR_CHUNK_SEQ: msgPrefix = VDBSRC_NO_MEM_FOR_CHUNK_SEQ; break;
    case eVDBSRC_SEQ_LENGTH_ERROR: msgPrefix = VDBSRC_SEQ_LENGTH_ERROR; break;
    case eVDBSRC_READ_ID_MISMATCH: msgPrefix = VDBSRC_READ_ID_MISMATCH; break;
    case eVDBSRC_4NA_SEQ_STRING_ERROR: msgPrefix = VDBSRC_4NA_SEQ_STRING_ERROR; break;
    case eVDBSRC_2NA_SEQ_STRING_ERROR: msgPrefix = VDBSRC_2NA_SEQ_STRING_ERROR; break;
    case eVDBSRC_NUM_SEQ_OVERFLOW_ERROR: msgPrefix = VDBSRC_NUM_SEQ_OVERFLOW_ERROR; break;
    case eVDBSRC_FILTERED_READ:	msgPrefix = VDBSRC_FILTERED_READ; break;
    case eVDBSRC_ID_OUT_OF_RANGE: msgPrefix = VDBSRC_ID_OUT_OF_RANGE; break;
    case eVDBSRC_4NA_REF_SEQ_BUF_OVERFLOW: msgPrefix = VDBSRC_4NA_REF_SEQ_BUF_OVERFLOW; break;
    }
    ASSERT(msgPrefix);

    // generate the message body if the error came from SRA libs
    // (i.e. error is not local, which is indicated by a non-zero resultCode)

    if (vdbErrMsg->resultCode != 0)
    {
        char buffer[512] = { '\0' };
        size_t num_writ = 0;
        rc_t rc = RCExplain(vdbErrMsg->resultCode, buffer, sizeof(buffer),
                            &num_writ);
        ASSERT(rc == 0);
        msgBody = strdup(buffer);
        ASSERT(msgBody);
    }

    // calculate the total length of the error message
    // (overestimate, to be safe and avoid off-by-one errors)

    lengthTotal += strlen(msgPrefix) + 1;
    if (vdbErrMsg->msgContext)
    {
        lengthTotal += strlen(vdbErrMsg->msgContext) + 3;
    }
    if (msgBody)
    {
        lengthTotal += strlen(msgBody) + 2;
    }

    // allocate the error message string
    *errMsg = (char*)calloc(lengthTotal, sizeof(char));
    if (*errMsg)
    {
        // add the prefix
        strcpy(*errMsg, msgPrefix);

        if (vdbErrMsg->msgContext)
        {
            // add the context
            strcat(*errMsg, " (");
            strcat(*errMsg, vdbErrMsg->msgContext);
            strcat(*errMsg, ")");
        }

        if (msgBody)
        {
            // add the message body
            strcat(*errMsg, ": ");
            strcat(*errMsg, msgBody);
        }
    }
}

// ==========================================================================//

#ifdef __cplusplus
}
#endif

