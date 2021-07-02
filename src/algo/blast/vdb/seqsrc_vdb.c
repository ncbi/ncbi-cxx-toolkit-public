/*  $Id:
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

// Local includes
#include "vdbsequtil.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================//
// Definitions / Constants

/// Database name string returned by BlastSeqSrc (normally empty).
static const char *const kEmptyVDBName = "";

// ==========================================================================//
// BlastSeqSrc interface - Global properties

/// Get the number of sequences in the sequence source.
static Int4
s_VDBSRC_GetNumSeqs(void* vdbDataHandle, void* dummy)
{
    TVDBData* vdbData = (TVDBData*)vdbDataHandle;
    uint64_t retval = 0;

    ASSERT(vdbDataHandle);
    if (vdbData->isInitialized)
    	retval = vdbData->numSeqs;

    if(retval > kMax_I4)
    	retval = VDBSRC_OVERFLOW_RV;

    return (Int4) retval;
}

/// Get the maximum sequence length in the sequence source.
static Int4
s_VDBSRC_GetMaxLength(void* vdbDataHandle, void* dummy)
{
    TVDBData* vdbData = (TVDBData*)vdbDataHandle;
    uint64_t retval = 0;

    ASSERT(vdbDataHandle);
    if (vdbData->isInitialized)
        retval = VDBSRC_GetMaxSeqLen(vdbData);

    if(retval > kMax_I4)
    	retval = VDBSRC_OVERFLOW_RV;

    return (Int4) retval;

}

/// Get the average sequence length in the sequence source.
static Int4
s_VDBSRC_GetAvgLength(void* vdbDataHandle, void* dummy)
{
    TVDBData* vdbData = (TVDBData*)vdbDataHandle;
    uint64_t retval = 0;

    ASSERT(vdbDataHandle);
    if (vdbData->isInitialized)
        retval = VDBSRC_GetAvgSeqLen(vdbData);

    if(retval > kMax_I4)
    	retval = VDBSRC_OVERFLOW_RV;

    return (Int4) retval;

}

/// Get the total sequence length in the sequence source.
static Int8
s_VDBSRC_GetTotLen(void* vdbDataHandle, void* dummy)
{
    TVDBData* vdbData = (TVDBData*)vdbDataHandle;
    uint64_t retval = 0;

    ASSERT(vdbDataHandle);
    if (vdbData->isInitialized)
        retval = VDBSRC_GetTotSeqLen(vdbData);

    if(retval > kMax_I8)
    	retval = VDBSRC_OVERFLOW_RV;

    return (Int8) retval;
}

/// Get the sequence source name (VDB run accessions).
static const char*
s_VDBSRC_GetName(void* vdbDataHandle, void* dummy)
{
    TVDBData* vdbData = (TVDBData*)vdbDataHandle;
    const char* retval = kEmptyVDBName;

    ASSERT(vdbDataHandle);
    if (vdbData->isInitialized)
    {
        retval = vdbData->names;
    }

    return retval;
}

// ==========================================================================// */
// BlastSeqSrc interface - Sequence retrieval and properties

/// Get the sequence from the sequence source given the OID and encoding.
static Int2
s_VDBSRC_GetSequence(void* vdbDataHandle, BlastSeqSrcGetSeqArg* args)
{
    // Init the error message and error return value
    TVDBErrMsg vdbErrMsg;
    Int2 retval = BLAST_SEQSRC_ERROR;

    TVDBData* vdbData = (TVDBData*)vdbDataHandle;
    oid_t oid;
    EBlastEncoding encoding;
    TNuclDataRequest req;
    Boolean bufferAlloc;
    TByteArray byteArray;
    Int2 resSetUpBlk;

    // Note 1: We might need to return an empty sequence in this function
    // instead of returning an error (also applies to OIDs that map to
    // empty or tiny reads).
    //
    // Note 2: Currently the 2na format doesn't take care of the partial
    // terminal bytes (basically it reports extra bases on the edges).
    // These should be ignored at the traceback stage anyway, so we won't
    // worry about it here.

    if (!vdbDataHandle || !args)
        return BLAST_SEQSRC_ERROR;

    // Init the error message and error return value
    VDBSRC_InitEmptyErrorMsg(&vdbErrMsg);

    // Read the arguments
    oid = args->oid;
    encoding = args->encoding;

    // Clean up the existing sequence
    if (args->seq)
        BlastSequenceBlkClean(args->seq);

    // Prepare the nucleotide data request structure

    req.read4na = (encoding == eBlastEncodingNucleotide ||
                   encoding == eBlastEncodingNcbi4na);
    req.copyData = req.read4na;
    req.hasSentinelBytes = (encoding == eBlastEncodingNucleotide);
    req.convertDataToBlastna = (encoding == eBlastEncodingNucleotide);
    req.readId = oid;

    bufferAlloc = req.copyData;
    VDBSRC_InitByteArray_Empty(&byteArray);

   //Get the sequence buffer and length
   if (req.read4na)
   {
	   if(vdbData->reader_4na == NULL)
	   {
		   VDBSRC_Init4naReader(vdbData, &vdbErrMsg);

		   if(vdbErrMsg.isError)
		   {
			   VDBSRC_ResetReader(vdbData);
			   return retval;
		   }
	   }

	   if(!VDBSRC_GetSeq4naCopy(vdbData, &byteArray, &req, &vdbErrMsg))
		   return retval;
   }
   else
   {
	   if(vdbData->reader_2na == NULL)
	   {
		   VDBSRC_InitErrorMsg(&vdbErrMsg, retval,  eVDBSRC_READER_2NA_ERROR);
		   return retval;
	   }

	  if(!VDBSRC_GetSeq2na(vdbData, &byteArray, &req, &vdbErrMsg))
		  return retval;
   }

   // Set up the sequence data
   resSetUpBlk =
   BlastSetUp_SeqBlkNew(byteArray.data, byteArray.basesTotal, &args->seq, bufferAlloc);

   if (resSetUpBlk == 0)
   {
	   // Set the sequence oid
       args->seq->oid = oid;

       // Final adjustments to the sequence data
       // (similar to the SeqDB BlastSeqSrc implementation)
       if (bufferAlloc) {
            if (!req.hasSentinelBytes)
                args->seq->sequence = args->seq->sequence_start;
        }
        else {
        	if(! req.read4na)
        	{
        		args->seq->bases_offset =  4- byteArray.basesFirstByte ;
		 	// Increase length to correct for seq end
                	args->seq->length += args->seq->bases_offset;
        	}
        }
        // Sequence data was successfully initialized
        retval = BLAST_SEQSRC_SUCCESS;
    }

    VDBSRC_ReleaseErrorMsg(&vdbErrMsg);
    return retval;
}

/// Get the sequence length from the sequence source given the OID.
Int4
s_VDBSRC_GetSeqLen(void* vdbDataHandle, void* oid)
{
	// Access the SRA data
    TVDBData* vdbData = (TVDBData*)vdbDataHandle;

    // Read the sequence as string
    Int4 * id = (Int4 *) oid;
    uint64_t retval = 0;

    if (!vdbData || oid == NULL)
      	return 0;


   	retval = VDBSRC_GetSeqLen(vdbData, (uint64_t) (*id));

   	if(retval > kMax_I4) {
   	   retval = VDBSRC_OVERFLOW_RV;
   	}

   	return (Int4) retval;

}

/// Release the sequence from the sequence source given the OID.
static void
s_VDBSRC_ReleaseSequence(void* vdbDataHandle, BlastSeqSrcGetSeqArg* args)
{
	TVDBData* vdbData = (TVDBData*)vdbDataHandle;
	if (args->seq->sequence_start_allocated) {
		sfree(args->seq->sequence_start);
	    args->seq->sequence_start_allocated = FALSE;
	    args->seq->sequence_start = NULL;
	}
	if (args->seq->sequence_allocated) {
		sfree(args->seq->sequence);
	    args->seq->sequence_allocated = FALSE;
	    args->seq->sequence = NULL;
	}
	if(( vdbData->range_list != NULL)  && (args->oid == vdbData->range_list->oid)) {
		VDBSRC_ResetPartialFetchingList(vdbData);
	}
	return;
}

// ==========================================================================//
// BlastSeqSrc interface - Iteration

/// Get the next sequence OID and increment the BlastSeqSrc iterator.
static Int4
s_VDBSRC_IteratorNext(void* vdbDataHandle, BlastSeqSrcIterator* itr)
{
	Int4 retval = BLAST_SEQSRC_ERROR;
	TVDBErrMsg vdbErrMsg;
	TVDBData* vdbData = (TVDBData*)vdbDataHandle;
	TVDB2naICReader * r2na;
	uint64_t read_id;

    if (!vdbDataHandle || !itr)
        return retval;
    
    if (!vdbData->isInitialized)
        return retval;

    VDBSRC_InitEmptyErrorMsg(&vdbErrMsg);

    if(vdbData->reader_2na == NULL)
    {
    	VDBSRC_Init2naReader(vdbData, &vdbErrMsg);

    	if(vdbErrMsg.isError)
    	{
    	   VDBSRC_ResetReader(vdbData);
    	   return retval;
    	}
    }

    r2na = vdbData->reader_2na;

    r2na->current_index ++;
    if(r2na->current_index >= r2na->max_index)
    {
    	r2na->max_index = 0;
    	r2na->current_index = 0;

   		retval = VDBSRC_Load2naSeqToBuffer(vdbData, &vdbErrMsg);
    	if((vdbErrMsg.isError) || (retval == BLAST_SEQSRC_EOF))
    	{
    		return retval;
    	}
    }

   	read_id = r2na->buffer[r2na->current_index].read_id;
    if(read_id > kMax_I4)
    {
        retval = BLAST_SEQSRC_ERROR;
    }

    itr->current_pos = read_id;

    return read_id;
}

/// Resets the internal bookmark iterator (not applicable in our case).
/// @see s_MultiSeqResetChunkIter
static void
s_VDBSRC_ResetChunkIterator(void* vdbDataHandle)
{
    // Note: Right now this is useless (we don't keep track of chunks)
    return;
}

// ==========================================================================//
// BlastSeqSrc interface - Unsupported or trivial functions

/// GetNumSeqsStats - Not supported.
static Int4
s_VDBSRC_GetNumSeqsStats(void* vdbDataHandle, void* dummy)
{
    // Not supported
    return 0;
}

/// GetTotLenStats - Not supported.
static Int8
s_VDBSRC_GetTotLenStats(void* vdbDataHandle, void* dummy)
{
    // Not supported
    return 0;
}

/// GetIsProt - always returns FALSE in our case.
static Boolean
s_VDBSRC_GetIsProt(void* vdbDataHandle, void* dummy)
{
    TVDBData* vdbData = (TVDBData*)vdbDataHandle;
    ASSERT(vdbDataHandle);
    return VDBSRC_GetIsProtein(vdbData);
}

// ==========================================================================//
// Copy

/// Copy the BlastSeqSrc and its internal SRA data access object.
static BlastSeqSrc* 
s_VDBSRC_SrcCopy(BlastSeqSrc* seqSrc)
{
	TVDBData* vdbData;
	TVDBData* vdbDataNew;
    TVDBErrMsg vdbErrMsg;
    if (!seqSrc) 
        return NULL;
    
    vdbData =
        (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(seqSrc));

    VDBSRC_InitEmptyErrorMsg(&vdbErrMsg);

    vdbDataNew = VDBSRC_CopyData(vdbData, &vdbErrMsg);

    // set the new copy of SRA data in BlastSeqSrc
    _BlastSeqSrcImpl_SetDataStructure(seqSrc, (void*)vdbDataNew);

    VDBSRC_ReleaseErrorMsg(&vdbErrMsg);
    return seqSrc;
}

// ==========================================================================//
// Destruction

/// Release the BlastSeqSrc and its internal SRA data access object.
static BlastSeqSrc* 
s_VDBSRC_SrcFree(BlastSeqSrc* seqSrc)
{
	TVDBData* vdbData;
    TVDBErrMsg vdbErrMsg;
    if (!seqSrc) 
        return NULL;
    
    vdbData =
        (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(seqSrc));

    VDBSRC_InitEmptyErrorMsg(&vdbErrMsg);
    VDBSRC_FreeData(vdbData);
    VDBSRC_ReleaseErrorMsg(&vdbErrMsg);
    return NULL;
}

static Boolean
s_VDBSRC_GetSupportsPartialFetching(void* vdbDataHandle, void* dummy)
{
	TVDBData* vdbData = (TVDBData*)vdbDataHandle;
	if(vdbData->refSet != NULL) {
		return true;
	}
	return false;
}

static void s_VDBSRC_SetSeqRange(void* vdbDataHandle, BlastSeqSrcSetRangesArg* args)
{
	TVDBData* vdbData = (TVDBData*)vdbDataHandle;
	ASSERT(vdbDataHandle);

	if(args== NULL) {
		return;
	}
	VDBSRC_FillPartialFetchingList(vdbData, args->num_ranges);
	if(vdbData->range_list && vdbData->range_list->ranges) {
		memcpy(vdbData->range_list->ranges, args->ranges, 2*args->num_ranges*sizeof(Int4));
		vdbData->range_list->oid = args->oid;
	}
	return;
}

// ==========================================================================//
// Construction / Initialization

/// Set the BlastSeqSrc function pointers and the SRA data access object.
static void 
s_InitVDBBlastSeqSrcFields(BlastSeqSrc* seqSrc, TVDBData* vdbData)
{
    ASSERT(seqSrc);
    ASSERT(vdbData);
    
    /* initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and SRA data */
    _BlastSeqSrcImpl_SetDeleteFnPtr         (seqSrc, & s_VDBSRC_SrcFree);
    _BlastSeqSrcImpl_SetCopyFnPtr           (seqSrc, & s_VDBSRC_SrcCopy);
    _BlastSeqSrcImpl_SetDataStructure       (seqSrc, (void*) vdbData);
    _BlastSeqSrcImpl_SetGetNumSeqs          (seqSrc, & s_VDBSRC_GetNumSeqs);
    _BlastSeqSrcImpl_SetGetNumSeqsStats     (seqSrc, & s_VDBSRC_GetNumSeqsStats);
    _BlastSeqSrcImpl_SetGetMaxSeqLen        (seqSrc, & s_VDBSRC_GetMaxLength);
    _BlastSeqSrcImpl_SetGetAvgSeqLen        (seqSrc, & s_VDBSRC_GetAvgLength);
    _BlastSeqSrcImpl_SetGetTotLen           (seqSrc, & s_VDBSRC_GetTotLen);
    _BlastSeqSrcImpl_SetGetTotLenStats      (seqSrc, & s_VDBSRC_GetTotLenStats);
    _BlastSeqSrcImpl_SetGetName             (seqSrc, & s_VDBSRC_GetName);
    _BlastSeqSrcImpl_SetGetIsProt           (seqSrc, & s_VDBSRC_GetIsProt);
    _BlastSeqSrcImpl_SetGetSequence         (seqSrc, & s_VDBSRC_GetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen           (seqSrc, & s_VDBSRC_GetSeqLen);
    _BlastSeqSrcImpl_SetIterNext            (seqSrc, & s_VDBSRC_IteratorNext);
    _BlastSeqSrcImpl_SetResetChunkIterator  (seqSrc, & s_VDBSRC_ResetChunkIterator);
    _BlastSeqSrcImpl_SetReleaseSequence     (seqSrc, & s_VDBSRC_ReleaseSequence);
    _BlastSeqSrcImpl_SetGetSupportsPartialFetching (seqSrc, & s_VDBSRC_GetSupportsPartialFetching);
    _BlastSeqSrcImpl_SetSetSeqRange         (seqSrc, & s_VDBSRC_SetSeqRange);
}

/// Initialize the SRA data access object and set up the BlastSeqSrc.
static BlastSeqSrc* 
s_VDBSRC_SrcNew(BlastSeqSrc* seqSrc, void* args)
{
    TVDBNewArgs* vdbArgs = (TVDBNewArgs*)args;
    TVDBErrMsg vdbErrMsg;
    TVDBData* vdbData;

    ASSERT(seqSrc);
    ASSERT(args);
    ASSERT(vdbArgs);

    // error message to be reported in case of a failure

    VDBSRC_InitEmptyErrorMsg(&vdbErrMsg);

    // allocate and initialize the SRA data structure
    vdbData = VDBSRC_NewData(&vdbErrMsg);
    if (!vdbErrMsg.isError)
    {
    	if(vdbArgs->isCSRA)
    		VDBSRC_InitCSRAData(vdbData, vdbArgs, &vdbErrMsg, true);
    	else
    		VDBSRC_InitData(vdbData, vdbArgs, &vdbErrMsg, true);
    }

    // set up the BlastSeqSrc structure
    if (vdbData->isInitialized)
    {
        // set the VDB data and function pointers in BlastSeqSrc
        s_InitVDBBlastSeqSrcFields(seqSrc, vdbData);
    }
    else
    {
        // format the error message
        char* errMsg = 0;
        VDBSRC_FormatErrorMsg(&errMsg, &vdbErrMsg);

        // store the error message in BlastSeqSrc
        // (it is assumed that in case of an error the client will
        //  never invoke any other BlastSeqSrc functionality,
        //  as required by the BlastSeqSrc interface in blast_seqsrc.h)
        _BlastSeqSrcImpl_SetInitErrorStr(seqSrc, errMsg);

        // clean up
        VDBSRC_FreeData(vdbData);
    }

    VDBSRC_ReleaseErrorMsg(&vdbErrMsg);
    return seqSrc;
}

BlastSeqSrc* 
SRABlastSeqSrcInit(const char** vdbRunAccessions, Uint4 numRuns,
		           Boolean isProtein, Boolean * excluded_runs,
		           Uint4* status, Boolean isCSRA)
{
    BlastSeqSrcNewInfo bssNewInfo;
    BlastSeqSrc* seqSrc = NULL;

    TVDBNewArgs vdbArgs;
    vdbArgs.vdbRunAccessions = vdbRunAccessions;
    vdbArgs.numRuns = numRuns;
    vdbArgs.isProtein = isProtein;
    vdbArgs.isRunExcluded = excluded_runs;
    vdbArgs.status = 0;
    vdbArgs.isCSRA = isCSRA;

    bssNewInfo.constructor = &s_VDBSRC_SrcNew;
    bssNewInfo.ctor_argument = (void*)&vdbArgs;
    seqSrc = BlastSeqSrcNew(&bssNewInfo);

    *status = vdbArgs.status;
    return seqSrc;
}

// ==========================================================================//

#ifdef __cplusplus
}
#endif

