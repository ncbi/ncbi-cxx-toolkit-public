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

#define NOT_DEFERRED 1

#include "vdb_priv.h"
#include <ncbi/vdb-blast-priv.h>
#include <ncbi/vdb-blast.h>

#include <corelib/ncbi_limits.h>
#include <algo/blast/core/blast_gapalign.h>
#include "vdbsequtil.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Int4 s_VDBSRC_GetSeqLen(void* vdbDataHandle, void* args);

// ==========================================================================//
// SRA Data functions

uint64_t
VDBSRC_GetMaxSeqLen(TVDBData* vdbData)
{
    ASSERT(vdbData);

    if (!vdbData->isInitialized || NULL == vdbData->runSet) {
        return 0;
	}

    if(vdbData->refSet == NULL) {
    	return VdbBlastRunSetGetMaxSeqLen(vdbData->runSet);
	}
    else {
    	return 10000;
	}
}

uint64_t
VDBSRC_GetAvgSeqLen(TVDBData* vdbData)
{
    ASSERT(vdbData);

    if (!vdbData->isInitialized || NULL == vdbData->runSet) {
    	return 0;
	}

    if(vdbData->refSet == NULL) {
    	return VdbBlastRunSetGetAvgSeqLen(vdbData->runSet);
	}
    else {
    	return 200;
	}
}

uint64_t
VDBSRC_GetTotSeqLen(TVDBData* vdbData)
{
    uint32_t status = eVdbBlastNoErr;
    uint64_t len = 0;
    ASSERT(vdbData);

    if (!vdbData->isInitialized || NULL == vdbData->runSet) {
        return 0;
	}

    if(vdbData->refSet == NULL){
    	len = VdbBlastRunSetGetTotalLength(vdbData->runSet, &status);
    	if(status == eVdbBlastTooExpensive) {
        	len = VdbBlastRunSetGetTotalLengthApprox(vdbData->runSet);
    	}
    }
    else {
    	len = VdbBlastReferenceSetGetTotalLength(vdbData->refSet, &status);
    }

    return len;
}

uint64_t
VDBSRC_GetSeqLen(TVDBData* vdbData, uint64_t oid)
{
    uint32_t status = eVdbBlastNoErr;
    uint64_t len = 0;
    ASSERT(vdbData);

    if (!vdbData->isInitialized || NULL == vdbData->runSet)
        return 0;

    if(vdbData->refSet == NULL){
   		TVDBErrMsg errMsg;
       	char* cstrSeq = 0;
       	bool rc = false;
       	VDBSRC_InitEmptyErrorMsg(&errMsg);
       	rc = VDBSRC_Get4naSequenceAsString(vdbData, oid, &cstrSeq, &errMsg);
       	if (!cstrSeq || strlen(cstrSeq) == 0 || !rc) {
       	       return 0;
       	}
       	len = strlen(cstrSeq);
    }
    else {
    	uint64_t readId = oid | REF_SEQ_ID_MASK;
    	len = VdbBlastReferenceSetGetReadLength(vdbData->refSet, readId, &status);
    }

    if ((status != eVdbBlastNoErr) && (status != eVdbBlastCircularSequence) ) {
    	return 0;
    }
    return len;
}


TVDBData*
VDBSRC_NewData(TVDBErrMsg* vdbErrMsg)
{
    TVDBData* vdbData = (TVDBData*)calloc(1, sizeof(TVDBData));
    ASSERT(vdbErrMsg);
    if (vdbData) {
        vdbData->isInitialized = FALSE;
        vdbData->reader_2na = NULL;
        vdbData->reader_4na = NULL;
        vdbData->runSet = NULL;
        vdbData->refSet = NULL;
        vdbData->range_list = NULL;
    }
    else {
        VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_NO_MEM_FOR_VDBDATA);
    }
    return vdbData;
}

void
VDBSRC_InitData(TVDBData* vdbData,
                TVDBNewArgs* vdbArgs,
                TVDBErrMsg* vdbErrMsg,
                Boolean getStats)
{
    uint32_t status = eVdbBlastNoErr;
    uint32_t iRun;

    ASSERT(vdbData);
    ASSERT(vdbArgs);
    ASSERT(vdbArgs->vdbRunAccessions);
    ASSERT(vdbArgs->isRunExcluded);
    ASSERT(vdbArgs->numRuns > 0);
    ASSERT(vdbErrMsg);

    // Init vdb mgr
    vdbArgs->status = 0;
    vdbData->hdlMgr = VDBSRC_GetVDBManager(&status);

    if ((status != eVdbBlastNoErr) || (NULL == vdbData->hdlMgr)) {
        VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_MGR_LOAD_ERROR);
        vdbArgs->status = -1;
        return;
    }

    // Init RunSet
    vdbData->runSet = VdbBlastMgrMakeRunSet(vdbData->hdlMgr, &status,
    		                                VDBSRC_MIN_READ_LENGTH, vdbArgs->isProtein);

    if ((status != eVdbBlastNoErr) || (NULL == vdbData->runSet)) {
        VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_RUNSET_LOAD_ERROR);
        vdbArgs->status = -1;
        return;
    }

   	vdbData->numRuns = 0;

    for (iRun = 0; iRun < vdbArgs->numRuns; iRun++) {
    	const char* runAccession = vdbArgs->vdbRunAccessions[iRun];
        status = VdbBlastRunSetAddRun(vdbData->runSet, runAccession);

       if(status != eVdbBlastNoErr) {
    	   (vdbArgs->isRunExcluded)[iRun] = TRUE;
    	   vdbArgs->status ++;
       }
       else {
    	   (vdbArgs->isRunExcluded)[iRun] = FALSE;
    	   vdbData->numRuns ++;
       }
    }

    if(vdbData->numRuns == 0) {
    	vdbArgs->status = -1;
        VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_ADD_RUNS_ERROR);
    	return;
    }

    {
    	char tmp[1];
    	uint32_t tmp_s = 0;
    	// First try to get buffer size
    	size_t buf_sz = VdbBlastRunSetGetName(vdbData->runSet, &tmp_s, tmp, 1)+ 1;
    	size_t final_sz;

    	vdbData->names = (char *) malloc(buf_sz);
    	if(NULL == vdbData->names) {
    		vdbArgs->status = -1;
    		VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_NO_MEM_FOR_RUNS);
    	}
    	final_sz = VdbBlastRunSetGetName(vdbData->runSet, &status, vdbData->names, buf_sz);
    	if(status != eVdbBlastNoErr) {
    		vdbArgs->status = -1;
    		VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_GET_RUNSET_NAME_ERROR);
    	}
    	ASSERT(final_sz < buf_sz);

    }

    if(getStats){
    	vdbData->numSeqs = VdbBlastRunSetGetNumSequences(vdbData->runSet, &status);
    	if(status == eVdbBlastTooExpensive) {
    		vdbData->numSeqs = VdbBlastRunSetGetNumSequencesApprox(vdbData->runSet);
		}
    	else if(status != eVdbBlastNoErr) {
    		vdbArgs->status = -1;
    		VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_GET_RUNSET_NUM_SEQ_ERROR);
    	}

    	if(vdbData->numSeqs < 0) {
    		vdbArgs->status = -1;
    		VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_GET_RUNSET_NUM_SEQ_ERROR);
    	}
    }

    if (!vdbErrMsg->isError) {
        vdbData->isInitialized = TRUE;
   }
   return ;
}
void
VDBSRC_ResetReader_2na(TVDBData * vdbData)
{
	if(NULL != vdbData->reader_2na) {
		if(NULL != vdbData->reader_2na->reader){
			VdbBlast2naReaderRelease(vdbData->reader_2na->reader);
			vdbData->reader_2na->reader = NULL;
		}

		if(NULL != vdbData->reader_2na->chunked_buf)
		{
			free (vdbData->reader_2na->chunked_buf);
			vdbData->reader_2na->chunked_buf = NULL;
			vdbData->reader_2na->chunked_buf_size = 0;
		}

		if(NULL != vdbData->reader_2na->buffer)
		{
			free (vdbData->reader_2na->buffer);
			vdbData->reader_2na->buffer = NULL;
		}

		free(vdbData->reader_2na);
	}
	vdbData->reader_2na = NULL;
}

void
VDBSRC_ResetReader_4na(TVDBData * vdbData)
{
	if(NULL != vdbData->reader_4na) {
		VdbBlast4naReaderRelease(vdbData->reader_4na);
	}
	vdbData->reader_4na = NULL;
}

void
VDBSRC_ResetReader(TVDBData * vdbData)
{
	VDBSRC_ResetReader_2na(vdbData);
	VDBSRC_ResetReader_4na(vdbData);
}

void VDBSRC_ResetPartialFetchingList(TVDBData * vdbData)
{
	if(vdbData == NULL) {
		return;
	}
	TVDBPartialFetchingRanges * range_list = vdbData->range_list;
    if(range_list) {
    	range_list->oid = 0;
    	range_list->num_ranges = 0;
    	if(range_list->ranges) {
    		free (range_list->ranges);
    		range_list->ranges = NULL;
    	}
       	free(range_list);
       	vdbData->range_list = NULL;
    }
}

void
VDBSRC_FillPartialFetchingList(TVDBData* vdbData, Int4 num_ranges)
{
	ASSERT(vdbData);
	VDBSRC_ResetPartialFetchingList(vdbData);
	if(num_ranges  <= 0) {
		return;
	}
	vdbData->range_list = (TVDBPartialFetchingRanges *) malloc(sizeof(TVDBPartialFetchingRanges));
	vdbData->range_list->num_ranges = num_ranges;
	vdbData->range_list->ranges = (Int4 *) malloc(2*num_ranges*sizeof(Int4));
}


void
VDBSRC_FreeData(TVDBData* vdbData)
{
	// If err Null then, this is called for error cleanup
    if (!vdbData)
        return;

    VDBSRC_ResetReader(vdbData);
    VDBSRC_ResetPartialFetchingList(vdbData);
    if (vdbData->names) {
        free(vdbData->names);
        vdbData->names = NULL;
    }

    if(vdbData->runSet) {
    	VdbBlastRunSetRelease(vdbData->runSet);
    	vdbData->runSet = NULL;
    }

    if(vdbData->refSet) {
       	VdbBlastReferenceSetRelease(vdbData->refSet);
       	vdbData->refSet = NULL;
    }

    if (vdbData->hdlMgr) {
        vdbData->hdlMgr = NULL;
    }

    free(vdbData);
}

Boolean
VDBSRC_GetReadNameForOID(TVDBData* vdbData,
                     Int4 oid,
                     char * name_buffer,
                     size_t buf_size)
{
	size_t actual_size;
    ASSERT(vdbData);
    ASSERT(vdbData->runSet);
    if(vdbData->refSet == NULL) {
    	actual_size = VdbBlastRunSetGetReadName(vdbData->runSet, oid, name_buffer, buf_size -1);
    }
    else {
    	actual_size =  VdbBlastReferenceSetGetReadName (vdbData->refSet, (oid | REF_SEQ_ID_MASK), name_buffer, buf_size -1);
    	//printf("oid: %d, %s\n", oid, name_buffer);
    }

    if(actual_size >= buf_size - 1 )
    	return FALSE;

    return TRUE;
}

uint64_t s_GetChunkedSeqBufferSize(void)
{
	char * buf_size = getenv("VDB_2NA_BUFFER_SIZE");
	if(buf_size) {
		char * endptr;
		uint64_t sz = strtoul(buf_size, &endptr, 10);
		if (sz > 0) {
			return sz;
		}
	}
	return VDB_2NA_CHUNK_BUF_SIZE;
}

void
VDBSRC_Init2naReader(TVDBData* vdbData, TVDBErrMsg* vdbErrMsg)
{
    uint32_t status = 0;
	ASSERT(vdbData);
    ASSERT(vdbData->runSet);

    VDBSRC_ResetReader_2na(vdbData);

    vdbData->reader_2na = (TVDB2naICReader *) malloc (sizeof(TVDB2naICReader));
    if(NULL == vdbData->reader_2na)  {
    	 VDBSRC_InitLocalErrorMsg(vdbErrMsg,  eVDBSRC_NO_MEM_FOR_VDBDATA);
    	 return;
    }
    Packed2naRead * p2na_ptr = NULL;
    if(vdbData->refSet == NULL) {
    	 p2na_ptr = (Packed2naRead *) calloc(VDBSRC_CACHE_ITER_BUF_SIZE, sizeof(Packed2naRead));
    }
    else {
    	 p2na_ptr = (Packed2naRead *) calloc(1, sizeof(Packed2naRead));
    }
    if(NULL == p2na_ptr) {
       	   		VDBSRC_InitLocalErrorMsg(vdbErrMsg,  eVDBSRC_NO_MEM_FOR_VDBDATA);
       	   		return;
    }

   	vdbData->reader_2na->buffer = p2na_ptr;
    vdbData->reader_2na->chunked_buf = NULL;
    //Have read id always start from 0 for now
    if(vdbData->refSet == NULL) {
    	vdbData->reader_2na->reader = VdbBlastRunSetMake2naReader(vdbData->runSet, &status, 0);
    	if(status != eVdbBlastNoErr) {
    	   	 VDBSRC_InitErrorMsg(vdbErrMsg, status,  eVDBSRC_READER_2NA_ERROR);
    	   	 return;
    	}
    }
    else {
    	vdbData->reader_2na->reader = VdbBlastReferenceSetMake2naReader(vdbData->refSet, &status, 0);
    	if(status != eVdbBlastNoErr) {
    		VDBSRC_ResetReader_2na(vdbData);
    		VDBSRC_InitErrorMsg(vdbErrMsg, status,  eVDBSRC_READER_2NA_ERROR);
    		return;
    	}
    	uint32_t buf_size = s_GetChunkedSeqBufferSize();
    	uint8_t *buf = (uint8_t *) calloc(buf_size + 1, sizeof(uint8_t));
    	if(buf == NULL) {
    	   	VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_NO_MEM_FOR_CHUNK_SEQ);
    	   	return;
    	}
    	vdbData->reader_2na->chunked_buf = buf;
    	vdbData->reader_2na->chunked_buf_size = buf_size +1;
    }

    vdbData->reader_2na->max_index = 0;
    vdbData->reader_2na->current_index = 0;
    return;
}

void
VDBSRC_Init4naReader(TVDBData* vdbData, TVDBErrMsg* vdbErrMsg)
{
    uint32_t status = 0;
	ASSERT(vdbData);
    ASSERT(vdbData->runSet);

    VDBSRC_ResetReader_4na(vdbData);

    //Have read id always start from 0 for now
    if(vdbData->refSet == NULL) {
    	vdbData->reader_4na = VdbBlastRunSetMake4naReader(vdbData->runSet, &status);
    }
    else {
    	vdbData->reader_4na = VdbBlastReferenceSetMake4naReader(vdbData->refSet, &status);
    }
    if(status != eVdbBlastNoErr)
    {
    	 VDBSRC_InitErrorMsg(vdbErrMsg, status,  eVDBSRC_READER_4NA_ERROR);
    }

    return;
}

Int2 VDBSRC_Load2naSeqToBuffer(TVDBData * vdbData, TVDBErrMsg * vdbErrMsg)
{
    uint32_t status = eVdbBlastNoErr;
    TVDB2naICReader * r2na = vdbData->reader_2na;
    ASSERT(r2na);

    if(vdbData->refSet == NULL) {
    	r2na->max_index = VdbBlast2naReaderData (r2na->reader, &status, r2na->buffer, VDBSRC_CACHE_ITER_BUF_SIZE);
    }

    if((vdbData->refSet != NULL) || (status == eVdbBlastChunkedSequence)) {
    	uint64_t seqLength;
    	uint64_t chunk_seq_id = 0;
   		Packed2naRead * fillIn;

    	r2na->max_index = 0;

    	seqLength = VdbBlast2naReaderRead(r2na->reader, &status, &chunk_seq_id, r2na->chunked_buf, r2na->chunked_buf_size -1);
    	if( ((r2na->chunked_buf_size -1) * 4) == seqLength) {
    		uint64_t  t_len = VdbBlastReferenceSetGetReadLength ( vdbData->refSet , chunk_seq_id, &status);
    		//printf("total ref length %ld\n", t_len);
    		if (t_len > seqLength) {
    			r2na->chunked_buf_size = (t_len + COMPRESSION_RATIO)/COMPRESSION_RATIO + 1;
    			r2na->chunked_buf = (uint8_t *) realloc(r2na->chunked_buf, r2na->chunked_buf_size * sizeof(uint8_t));
    			if(r2na->chunked_buf == NULL) {
    				r2na->chunked_buf_size = 0;
    				VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_NO_MEM_FOR_CHUNK_SEQ);
    				return BLAST_SEQSRC_ERROR;
    			}
    			uint64_t left_over_len = VdbBlast2naReaderRead(r2na->reader, &status, &chunk_seq_id, r2na->chunked_buf + (seqLength/4),
    					                                       (t_len-seqLength + COMPRESSION_RATIO)/COMPRESSION_RATIO );
    			ASSERT((left_over_len + seqLength) == t_len);
    			seqLength = t_len;
    		}
    	}
    	// Discard circular repeat for now
    	if(status == eVdbBlastCircularSequence) {
    		//printf("Chunk seq id %lu, length %ld\n", chunk_seq_id, seqLength);
    		uint64_t  t_sz = (seqLength + COMPRESSION_RATIO)/COMPRESSION_RATIO + 1;
    		uint64_t t_seq_id = 0;
    		uint64_t t_seqLength = 0;
    		if((t_sz * 2) <= r2na->chunked_buf_size) {
    			 t_seqLength = VdbBlast2naReaderRead(r2na->reader, &status, &t_seq_id, r2na->chunked_buf+t_sz, t_sz -1);
    		}
    		else {
    			uint8_t * t_buf = (uint8_t *) malloc(t_sz * sizeof(uint8_t));
    			if (t_buf == NULL) {
    				VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_NO_MEM_FOR_CHUNK_SEQ);
    				return BLAST_SEQSRC_ERROR;
    			}
    			t_seqLength = VdbBlast2naReaderRead(r2na->reader, &status, &t_seq_id, t_buf, t_sz -1);
    			free (t_buf);
    		}
    		if (status != eVdbBlastNoErr) {
	    		VDBSRC_InitErrorMsg(vdbErrMsg, status,  eVDBSRC_READ_2NA_COPY_ERROR);
    		   	return BLAST_SEQSRC_ERROR;
    		}

    		ASSERT(t_seq_id == chunk_seq_id);
    		ASSERT(t_seqLength == seqLength);
    	}

    	if(seqLength == 0) {
    		if (status == eVdbBlastNoErr) {
    			return BLAST_SEQSRC_EOF;
    		}

    		VDBSRC_InitErrorMsg(vdbErrMsg, status,  eVDBSRC_READ_2NA_COPY_ERROR);
    		return BLAST_SEQSRC_ERROR;
    	}

    	r2na->max_index = 1;
    	fillIn = &(r2na->buffer[0]);
    	fillIn->read_id = chunk_seq_id & (~REF_SEQ_ID_MASK);
    	fillIn->starting_byte = (void *) r2na->chunked_buf;
    	fillIn->offset_to_first_bit = 0;
    	fillIn->length_in_bases = seqLength;
    }
    else if(status !=eVdbBlastNoErr)
	{
		VDBSRC_InitErrorMsg(vdbErrMsg, status,  eVDBSRC_READ_2NA_CACHE_ERROR);
		return BLAST_SEQSRC_ERROR;
	}

	if(0 == r2na->max_index) {
	    return BLAST_SEQSRC_EOF;
	}

    r2na->current_index = 0;
    return BLAST_SEQSRC_SUCCESS;
}

/* Entering Work zone
 *
 */
TVDBData*
VDBSRC_CopyData(TVDBData* vdbData,
                TVDBErrMsg* vdbErrMsg)
{
    TVDBData* vdbDataNew = NULL;

    ASSERT(vdbData);
    ASSERT(vdbErrMsg);

    if (!vdbData->isInitialized)
    {
    	VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_UNINIT_VDB_DATA_ERROR);
    	return vdbDataNew;
    }

    vdbDataNew = VDBSRC_NewData(vdbErrMsg);
    if (vdbDataNew)
    {
        vdbDataNew->hdlMgr = vdbData->hdlMgr;
        if (NULL == vdbDataNew->hdlMgr)
        {
            VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_MGR_LOAD_ERROR);
        	VDBSRC_FreeData(vdbDataNew);
            return NULL;
        }

        vdbDataNew->runSet = VdbBlastRunSetAddRef(vdbData->runSet);

        if (NULL == vdbDataNew->runSet)
        {
        	VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_MGR_LOAD_ERROR);
        	VDBSRC_FreeData(vdbDataNew);
            return NULL;
        }

        if(vdbData->refSet)
        {
        	vdbDataNew->refSet = VdbBlastReferenceSetAddRef(vdbData->refSet);
        	if (NULL == vdbDataNew->refSet)
        	{
        	   	VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_MGR_LOAD_ERROR);
        	   	VDBSRC_FreeData(vdbDataNew);
        	    return NULL;
        	}
        }


        // Don't copy readers
        vdbDataNew->reader_2na = NULL;
        vdbDataNew->reader_4na = NULL;

        // copy misc data
        vdbDataNew->numRuns = vdbData->numRuns;
      	vdbDataNew->numSeqs = vdbData->numSeqs;
        vdbDataNew->names = strdup(vdbData->names);

        // Copy partial fetching info
        if(vdbData->range_list && (vdbData->range_list->num_ranges > 0) ) {
        	VDBSRC_FillPartialFetchingList(vdbDataNew, vdbData->range_list->num_ranges);
        	memcpy(vdbDataNew->range_list->ranges, vdbData->range_list->ranges, 2*vdbData->range_list->num_ranges*sizeof(Int4));
        	vdbDataNew->range_list->oid = vdbData->range_list->oid;
        }
        // complete success
        vdbDataNew->isInitialized = TRUE;
    }
    else
    {
        VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_NO_MEM_FOR_VDBDATA);
    }
    return vdbDataNew;
}

bool
VDBSRC_GetIsProtein(TVDBData* vdbData)
{
    ASSERT(vdbData);

    if (!vdbData->isInitialized || NULL == vdbData->runSet) {
        return false;
	}

    return VdbBlastRunSetIsProtein ( vdbData->runSet );
}

Boolean
VDBSRC_GetOIDFromReadName(TVDBData* vdbData,
                          const char* nameRun,
                          Int4* oid)
{
	uint32_t status;
	uint64_t read_id = 0;
	size_t nameLength = strlen(nameRun);

	ASSERT(vdbData);
	ASSERT(nameRun);
    if (!vdbData->isInitialized || NULL == vdbData->runSet) {
        return false;
	}

	*oid = 0;

	if(nameLength == 0) {
		return false;
	}
	if(vdbData->refSet == NULL) {
		status = VdbBlastRunSetGetReadId(vdbData->runSet, nameRun, nameLength, &read_id);
	}
	else {
		status = VdbBlastReferenceSetGetReadId( vdbData->refSet, nameRun, nameLength, &read_id);
		read_id &=(~REF_SEQ_ID_MASK);
	}

	if(status != eVdbBlastNoErr) {
		return false;
	}

	if(read_id > kMax_I4) {
		return false;
	}
	*oid = read_id;

	return TRUE;
}

static VdbBlastMgr *
s_VDBManager(uint32_t * status, bool isGet)
{
	static VdbBlastMgr * vm_singleton = NULL;

	*status = eVdbBlastNoErr;
	if(isGet) {
		if(vm_singleton == NULL) {
			vm_singleton = VdbBlastInit(status);
			if(*status != eVdbBlastNoErr) {
				return NULL;
			}
		}
	}
	else {
		if(vm_singleton == NULL) {
			*status = eVdbBlastErr;
		}
		else {
			VdbBlastMgrRelease(vm_singleton);
			vm_singleton = NULL;
		}
	}

	return vm_singleton;
}

VdbBlastMgr*
VDBSRC_GetVDBManager(uint32_t * status)
{
	return s_VDBManager(status, true);
}

void
VDBSRC_ReleaseVDBManager()
{
	uint32_t status;
	s_VDBManager(&status, false);
}

int
VDBSRC_IsCSRA(const char * run)
{
	uint32_t status;
	int isCSRA = 0;
	VdbBlastMgr * mgr = s_VDBManager(&status, true);
	if(mgr == NULL || status != 0) {
		return -1;
	}

	isCSRA = VdbBlastMgrIsCSraRun(mgr, run) ? 1:0;
	return isCSRA;
}

void
VDBSRC_InitCSRAData(TVDBData* vdbData, TVDBNewArgs* vdbArgs,
                    TVDBErrMsg* vdbErrMsg, Boolean getStats)
{
    ASSERT(vdbData);
    ASSERT(vdbArgs);
    ASSERT(vdbArgs->vdbRunAccessions);
    ASSERT(vdbArgs->isRunExcluded);
    ASSERT(vdbArgs->numRuns > 0);
    ASSERT(vdbErrMsg);

    // Init vdb mgr
    vdbArgs->status = 0;

	VDBSRC_InitData(vdbData, vdbArgs, vdbErrMsg, false);

    if(vdbData->isInitialized != true) {
        vdbArgs->status = -1;
        return;
    }

    VDBSRC_MakeCSRASeqSrcFromSRASeqSrc(vdbData, vdbErrMsg, getStats);
    if(vdbData->isInitialized != true) {
         vdbArgs->status = -1;
         return;
    }
    return ;
}

void
VDBSRC_MakeCSRASeqSrcFromSRASeqSrc(TVDBData* vdbData, TVDBErrMsg* vdbErrMsg, Boolean getStats)
{
    uint32_t status = eVdbBlastNoErr;

    ASSERT(vdbData);
    ASSERT(vdbErrMsg);

    if(vdbData->isInitialized != true) {
    	status = -1;
        VDBSRC_InitErrorMsg(vdbErrMsg, status , eVDBSRC_REFSET_LOAD_ERROR);
        return;
    }

    vdbData->isInitialized = false;
   	VDBSRC_ResetReader_2na(vdbData);
   	VDBSRC_ResetReader_4na(vdbData);

    // Init RefSet
    vdbData->refSet = VdbBlastRunSetMakeReferenceSet(vdbData->runSet, &status);

    if ((status != eVdbBlastNoErr) || (NULL == vdbData->refSet)) {
        VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_REFSET_LOAD_ERROR);
        return;
    }

    if(getStats) {
    	vdbData->numSeqs = VdbBlastReferenceSetGetNumSequences ( vdbData->refSet, &status );
    	if ((status != eVdbBlastNoErr) || (0 == vdbData->numSeqs)) {
    		VDBSRC_InitErrorMsg(vdbErrMsg, status , eVDBSRC_REFSET_LOAD_ERROR);
    		return;
    	}
    }

    if (!vdbErrMsg->isError) {
       	vdbData->isInitialized = true;
    }

   return ;
}



// ==========================================================================//

#ifdef __cplusplus
}
#endif

