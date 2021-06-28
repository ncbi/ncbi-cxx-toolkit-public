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

#include "vdbsequtil.h"
#include <ncbi/vdb-blast.h>
#include <math.h>


#ifdef __cplusplus
extern "C" {
#endif

// ==========================================================================//
// Definitions / Constants

/// Array of 4-character strings corresponding to NCBI-2na bytes.
static const char* SRASEQUTIL_2NA_BYTE_TO_STRING[256] =
{
    "AAAA", "AAAC", "AAAG", "AAAT", "AACA", "AACC", "AACG", "AACT",
    "AAGA", "AAGC", "AAGG", "AAGT", "AATA", "AATC", "AATG", "AATT",
    "ACAA", "ACAC", "ACAG", "ACAT", "ACCA", "ACCC", "ACCG", "ACCT",
    "ACGA", "ACGC", "ACGG", "ACGT", "ACTA", "ACTC", "ACTG", "ACTT",
    "AGAA", "AGAC", "AGAG", "AGAT", "AGCA", "AGCC", "AGCG", "AGCT",
    "AGGA", "AGGC", "AGGG", "AGGT", "AGTA", "AGTC", "AGTG", "AGTT",
    "ATAA", "ATAC", "ATAG", "ATAT", "ATCA", "ATCC", "ATCG", "ATCT",
    "ATGA", "ATGC", "ATGG", "ATGT", "ATTA", "ATTC", "ATTG", "ATTT",
    "CAAA", "CAAC", "CAAG", "CAAT", "CACA", "CACC", "CACG", "CACT",
    "CAGA", "CAGC", "CAGG", "CAGT", "CATA", "CATC", "CATG", "CATT",
    "CCAA", "CCAC", "CCAG", "CCAT", "CCCA", "CCCC", "CCCG", "CCCT",
    "CCGA", "CCGC", "CCGG", "CCGT", "CCTA", "CCTC", "CCTG", "CCTT",
    "CGAA", "CGAC", "CGAG", "CGAT", "CGCA", "CGCC", "CGCG", "CGCT",
    "CGGA", "CGGC", "CGGG", "CGGT", "CGTA", "CGTC", "CGTG", "CGTT",
    "CTAA", "CTAC", "CTAG", "CTAT", "CTCA", "CTCC", "CTCG", "CTCT",
    "CTGA", "CTGC", "CTGG", "CTGT", "CTTA", "CTTC", "CTTG", "CTTT",
    "GAAA", "GAAC", "GAAG", "GAAT", "GACA", "GACC", "GACG", "GACT",
    "GAGA", "GAGC", "GAGG", "GAGT", "GATA", "GATC", "GATG", "GATT",
    "GCAA", "GCAC", "GCAG", "GCAT", "GCCA", "GCCC", "GCCG", "GCCT",
    "GCGA", "GCGC", "GCGG", "GCGT", "GCTA", "GCTC", "GCTG", "GCTT",
    "GGAA", "GGAC", "GGAG", "GGAT", "GGCA", "GGCC", "GGCG", "GGCT",
    "GGGA", "GGGC", "GGGG", "GGGT", "GGTA", "GGTC", "GGTG", "GGTT",
    "GTAA", "GTAC", "GTAG", "GTAT", "GTCA", "GTCC", "GTCG", "GTCT",
    "GTGA", "GTGC", "GTGG", "GTGT", "GTTA", "GTTC", "GTTG", "GTTT",
    "TAAA", "TAAC", "TAAG", "TAAT", "TACA", "TACC", "TACG", "TACT",
    "TAGA", "TAGC", "TAGG", "TAGT", "TATA", "TATC", "TATG", "TATT",
    "TCAA", "TCAC", "TCAG", "TCAT", "TCCA", "TCCC", "TCCG", "TCCT",
    "TCGA", "TCGC", "TCGG", "TCGT", "TCTA", "TCTC", "TCTG", "TCTT",
    "TGAA", "TGAC", "TGAG", "TGAT", "TGCA", "TGCC", "TGCG", "TGCT",
    "TGGA", "TGGC", "TGGG", "TGGT", "TGTA", "TGTC", "TGTG", "TGTT",
    "TTAA", "TTAC", "TTAG", "TTAT", "TTCA", "TTCC", "TTCG", "TTCT",
    "TTGA", "TTGC", "TTGG", "TTGT", "TTTA", "TTTC", "TTTG", "TTTT",
};

// ==========================================================================//
// Byte array functions



void
VDBSRC_InitByteArray_Empty(TByteArray* byteArray)
{
    ASSERT(byteArray);

    byteArray->data = NULL;
    byteArray->size = 0;
    byteArray->basesPerByte = 1;
    byteArray->basesFirstByte = 0;
    byteArray->basesLastByte = 0;
    byteArray->basesTotal = 0;
}

void
VDBSRC_InitByteArray_Data(TByteArray* byteArray,
                     uint8_t* data,
                     uint64_t size,
                     uint8_t basesPerByte,
                     uint8_t basesFirstByte,
                     uint8_t basesLastByte,
                     uint64_t basesTotal)
{
    ASSERT(byteArray);
    ASSERT(data);
    ASSERT(size > 0);
    ASSERT(basesPerByte == 1 || basesPerByte == 2 || basesPerByte == 4);

    byteArray->data = data;
    byteArray->size = size;
    byteArray->basesPerByte = basesPerByte;
    byteArray->basesFirstByte = basesFirstByte;
    byteArray->basesLastByte = basesLastByte;
    byteArray->basesTotal = basesTotal;
}

// ==========================================================================//
// SRA Sequence-related functions - NCBI 2na

Boolean VDBSRC_GetSeq2na(TVDBData * vdbData,
				      TByteArray* dataSeq,
				      TNuclDataRequest* req2na,
				      TVDBErrMsg * vdbErrMsg)
{
	uint32_t status = eVdbBlastNoErr;
	TVDB2naICReader * r2na = vdbData->reader_2na;
	Packed2naRead * read;
	uint8_t * dataOut;
	uint8_t offsetBits;
	uint32_t sizeBits;
	uint8_t basesFirstByte;
	uint32_t offsetEnd;        // last bit's offset
	uint8_t bitsInLastByte;      // 1 <= bitsInLastByte <= 8
	uint8_t basesLastByte;
    uint64_t buf_size = 2;


    ASSERT(dataSeq);
    ASSERT(vdbData);

    ASSERT(r2na->current_index < r2na->max_index);
    read = r2na->buffer + r2na->current_index;

    if(req2na->readId != read->read_id)
    {
    	VDBSRC_InitErrorMsg(vdbErrMsg, status,  eVDBSRC_READ_ID_MISMATCH);
    	return FALSE;
    }

    dataOut = (uint8_t *) read->starting_byte;
    offsetBits = read->offset_to_first_bit;
    sizeBits = read->length_in_bases * 2;

    // If offsetBits is an odd number (which I'm not sure it can be,
    // but nothing in the interface or documentation says it can't)
    // there is nothing we can do to avoid copying the data over to a
    // new array, which is forbidden in this function.
    // Therefore, we will only consider offsets that are off
    // by 0, 2, 4, or 6 bits within the first byte they point to.
    if (offsetBits & 0x01)
    {
    	VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_READ_2NA_CACHE_ERROR);
    	return FALSE;
    }

    basesFirstByte = 4 - (offsetBits >> 1);
    offsetEnd = offsetBits + sizeBits - 1;        // last bit's offset
    bitsInLastByte = 1 + (offsetEnd & 0x07);      // 1 <= bitsInLastByte <= 8
    basesLastByte = bitsInLastByte >> 1;

    if((read->length_in_bases - basesFirstByte - basesLastByte) > 0)
    	buf_size +=(read->length_in_bases - basesFirstByte - basesLastByte)/4;


    // Set up the output sequence data
    VDBSRC_InitByteArray_Data (dataSeq, dataOut, buf_size,
    						   4, basesFirstByte, basesLastByte, read->length_in_bases);

    return TRUE;
}

Boolean VDBSRC_Convert2naToString(TByteArray* dataSeq,
                                  char** strData)
{
	uint32_t sizeStr;
    uint32_t iByteIndex = 0;
    uint32_t iBaseIndex = 0;
    char * str = NULL;

    ASSERT(dataSeq);

    if (!dataSeq->data || dataSeq->size == 0) {
        return FALSE;
    }

    sizeStr = dataSeq->size * 4 + 32;
     str = (char*)calloc(sizeStr, sizeof(char));
    if (str == NULL) {
        return FALSE;
    }
    *strData = str;
    // Convert the first (partial) byte to string
    if(dataSeq->basesFirstByte != 4) {
    	unsigned int tmp = 4 - dataSeq->basesFirstByte;
    	iByteIndex =1;
    	iBaseIndex = dataSeq->basesFirstByte;
    	const char * tmp_ptr = SRASEQUTIL_2NA_BYTE_TO_STRING[dataSeq->data[0]] + tmp;
    	memcpy(str, tmp_ptr, dataSeq->basesFirstByte);
    }
    // Convert the array of (full) bytes to string

    for (; iByteIndex < (dataSeq->size - 1); iByteIndex++) {
    	memcpy(&str[iBaseIndex], SRASEQUTIL_2NA_BYTE_TO_STRING[dataSeq->data[iByteIndex]], 4);
    	iBaseIndex +=4;
    }

    // Convert the last (partial) byte to string
   	memcpy(&str[iBaseIndex], SRASEQUTIL_2NA_BYTE_TO_STRING[dataSeq->data[iByteIndex]], dataSeq->basesLastByte);
    iBaseIndex += dataSeq->basesLastByte;

    str[iBaseIndex] ='\0';
    return TRUE;
}

// ==========================================================================//
// SRA Sequence-related functions - NCBI 4na

Boolean s_GetSeq4naChunkSeq(TVDBData * vdbData, TByteArray* dataSeq,
						   TNuclDataRequest* req4na, TVDBErrMsg * vdbErrMsg)
{
	uint32_t status = eVdbBlastNoErr;
	uint64_t seqLength = 0;
	uint8_t * data = NULL;
	uint64_t  sizeOut=0;
	uint8_t* start_byte = NULL;
	Boolean sentinel = req4na->hasSentinelBytes;
	uint64_t  readId = req4na->readId;

	ASSERT(dataSeq);
	ASSERT(req4na);
	ASSERT(req4na->read4na);
	ASSERT(vdbData->reader_4na != NULL);

	data = (uint8_t *) calloc(VDB_4NA_CHUNK_BUF_SIZE + (sentinel? 2:0), sizeof(uint8_t));
	if(data == NULL) {
	   	VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_NO_MEM_FOR_CHUNK_SEQ);
	   	return FALSE;
	}
	if (vdbData->refSet!= NULL) {
		readId |= REF_SEQ_ID_MASK;
	}
	start_byte = sentinel ? &(data[1]) : data;

	seqLength = VdbBlast4naReaderRead(vdbData->reader_4na, &status,
	   		    					  readId , 0, start_byte, VDB_4NA_CHUNK_BUF_SIZE);
	if(status != eVdbBlastNoErr) {
	    	VDBSRC_InitErrorMsg(vdbErrMsg, status,  eVDBSRC_READ_4NA_COPY_ERROR);
	    	return FALSE;
	}

	if(seqLength == VDB_4NA_CHUNK_BUF_SIZE) {
	   	uint8_t tmp[2];
	   	uint64_t tl = VdbBlast4naReaderRead(vdbData->reader_4na, &status,
	   	    		    					readId, seqLength, tmp, 2);
	   	if(tl > 0) {
	   		VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_4NA_REF_SEQ_BUF_OVERFLOW);
	   		return FALSE;
	   	}
	}

	if((eVdbBlastNoErr != status)  || (seqLength == 0)) {
	   	if((status == eVdbBlastErr) && (seqLength == 0))
			VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_ID_OUT_OF_RANGE);
	   	else
			VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_READ_4NA_COPY_ERROR);

	   	return FALSE;
	}

    sizeOut = seqLength;

    // Add sentinel byte
	if (sentinel) {
	   	sizeOut+=2;
	    data[0] = kNuclSentinel;
	    data[sizeOut-1] = kNuclSentinel;
	}

	if(req4na->convertDataToBlastna) {
	   	for(int i=0; i< seqLength; i++ )
	   		start_byte[i] = NCBI4NA_TO_BLASTNA[start_byte[i]];
	}

	// Set up the sub-sequence data based on the newly created array
	VDBSRC_InitByteArray_Data (dataSeq, data, sizeOut, 1, 1, 1, seqLength);

	return TRUE;
}

Boolean s_GetSeq4naChunkSeq_PartialFetching(TVDBData * vdbData, TByteArray* dataSeq,
						   TNuclDataRequest* req4na, TVDBErrMsg * vdbErrMsg)
{
	uint32_t status = eVdbBlastNoErr;
	uint8_t * data = NULL;
	uint8_t* seq_start = NULL;
	Boolean sentinel = req4na->hasSentinelBytes;
	uint64_t  readId = req4na->readId;
    TVDBPartialFetchingRanges *pf_list = vdbData->range_list;
    int64_t num_ranges = pf_list->num_ranges;
    int64_t region_end = pf_list->ranges[num_ranges * 2 -1];
    uint64_t buffer_length = 0;
    uint64_t seq_length = 0;

    ASSERT(dataSeq);
    ASSERT(req4na);
    ASSERT(req4na->read4na);
    ASSERT(vdbData->reader_4na != NULL);

    if (vdbData->refSet!= NULL) {
    		readId |= REF_SEQ_ID_MASK;
    }
    seq_length = VDBSRC_GetSeqLen(vdbData, readId);
    ASSERT(seq_length);
    region_end = MIN (seq_length, pf_list->ranges[num_ranges * 2 -1]);
    //buffer_length =  region_end + (sentinel ? 2:0);
    buffer_length =  seq_length + (sentinel ? 2:0);
    pf_list->ranges[num_ranges * 2 -1] = region_end;
    data = (uint8_t *) calloc(buffer_length, sizeof(uint8_t));
    //printf("Paritial fetching add %ld, oid %d\n", data,req4na->readId);
	if(data == NULL) {
	   	VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_NO_MEM_FOR_CHUNK_SEQ);
	   	return FALSE;
	}

	seq_start = data + (sentinel ? 1:0);

	/* Place the fence sentinel aorund ranges first; so the range data is freeto reaplce the
	 * fence byte if adjacent rnages overlap, skipping first 'from' and last 'to'
	 */
	for(unsigned int i =0; i < num_ranges * 2; i+=2) {
		Int4 begin = pf_list->ranges[i];
		Int4 end = pf_list->ranges[i+1];
		if (begin > 0) {
			seq_start[begin-1] =(char) FENCE_SENTRY;
		}
		if(end < seq_length) {
		seq_start[end] =(char) FENCE_SENTRY;
		}
	}

	for(unsigned int i =0; i < num_ranges * 2; i+=2) {
		int64_t range_start = pf_list->ranges[i];
		uint64_t range_length = pf_list->ranges[i+1] - range_start;
		uint8_t* buf_start = &(seq_start[range_start]);
		uint64_t rl = VdbBlast4naReaderRead(vdbData->reader_4na, &status, readId,
											range_start, buf_start, range_length);
		//printf("Range start %ld, end %ld, length %ld\n", range_start, range_start +range_length, range_length);
		if(((eVdbBlastNoErr != status) && (eVdbBlastCircularSequence != status)) || (rl == 0)) {
		   	if((status == eVdbBlastErr) && (rl == 0)){
				VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_ID_OUT_OF_RANGE);
		   	}
		   	else {
				VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_READ_4NA_COPY_ERROR);
		   	}
		   	return FALSE;
		}
		if(rl != range_length){
				VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_READ_4NA_COPY_ERROR);
				return FALSE;
		}

		if(req4na->convertDataToBlastna) {
	    	for(int i=0; i< range_length; i++ ) {
	    		buf_start[i] = NCBI4NA_TO_BLASTNA[buf_start[i]];
	    	}
	    }
	}

    // Add sentinel byte
    if (sentinel) {
        data[0] = kNuclSentinel;
        data[buffer_length-1] = kNuclSentinel;
    }

    // Set up the sub-sequence data based on the newly created array
    VDBSRC_InitByteArray_Data (dataSeq, data, buffer_length, 1, 1, 1, seq_length);

    return TRUE;
}


Boolean s_GetSeq4na(TVDBData * vdbData, TByteArray* dataSeq,
					TNuclDataRequest* req4na, TVDBErrMsg * vdbErrMsg)
{
	uint32_t status = eVdbBlastNoErr;
	uint64_t seqLength = 0;
	const uint8_t * data;
	uint64_t  sizeOut;
	uint8_t* dataOut;
	uint8_t* start_byte;

    ASSERT(dataSeq);
    ASSERT(req4na);
    ASSERT(req4na->read4na);
    ASSERT(vdbData->reader_4na != NULL);


    //Note seq in 4na format but but has been expanded to 1 nucl per byte already
    data = VdbBlast4naReaderData(vdbData->reader_4na, &status, req4na->readId, &seqLength);

    if((data == NULL) && (eVdbBlastChunkedSequence == status)) {
    	return s_GetSeq4naChunkSeq(vdbData, dataSeq, req4na, vdbErrMsg);
    }

    if((eVdbBlastNoErr != status)  || (seqLength == 0))
    {
    	if((status == eVdbBlastInvalidId) && (seqLength == 0))
			VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_FILTERED_READ);
    	else if((status == eVdbBlastErr) && (seqLength == 0))
			VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_ID_OUT_OF_RANGE);
    	else
			VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_READ_4NA_COPY_ERROR);

    	return FALSE;
    }

    sizeOut = seqLength;

    if (req4na->hasSentinelBytes)
            sizeOut += 2;

    dataOut = (uint8_t*)calloc(sizeOut, sizeof(uint8_t));

    if(NULL == dataOut)
    {
    	VDBSRC_InitErrorMsg(vdbErrMsg, status, eVDBSRC_NO_MEM_FOR_VDBDATA);
    	return false;
    }

    // Add sentinel byte
    start_byte = dataOut;
    if (req4na->hasSentinelBytes)
    {
        dataOut[0] = kNuclSentinel;
        dataOut[sizeOut-1] = kNuclSentinel;
        memcpy(&dataOut[1], data, seqLength);
        start_byte = &dataOut[1];
    }

    if(req4na->convertDataToBlastna)
    {
    	int i;
    	for(i=0; i< seqLength; i++ )
    		start_byte[i] = NCBI4NA_TO_BLASTNA[data[i]];
    }
    else
    {
        memcpy(start_byte, data, seqLength);
    }

    // Set up the sub-sequence data based on the newly created array
    VDBSRC_InitByteArray_Data (dataSeq, dataOut, sizeOut, 1, 1, 1, seqLength);

    return TRUE;
}

Boolean VDBSRC_GetSeq4naCopy(TVDBData * vdbData,
						  	 TByteArray* dataSeq,
						  	 TNuclDataRequest* req4na,
						  	 TVDBErrMsg * vdbErrMsg)
{
	if(vdbData->refSet == NULL) {
		return s_GetSeq4na(vdbData, dataSeq, req4na, vdbErrMsg);
	}
	else {

		if((vdbData->range_list != NULL) &&
		   (vdbData->range_list->num_ranges > 0) &&
		   (vdbData->range_list->oid == req4na->readId)) {
			return s_GetSeq4naChunkSeq_PartialFetching(vdbData, dataSeq, req4na, vdbErrMsg);
		}
		else {
			return s_GetSeq4naChunkSeq(vdbData, dataSeq, req4na, vdbErrMsg);
		}

	}
}

Boolean VDBSRC_Convert4naToString(TByteArray* dataSeq, TNuclDataRequest* req4na,
                                  char** strData)
{
    uint32_t sizeStr;
    uint32_t iByteIndex=0;
    uint32_t lastIndex;
    ASSERT(dataSeq);
    
    if (!dataSeq->data ||
        dataSeq->size == 0 ||
        dataSeq->basesPerByte != 1)
    {
        return FALSE;
    }

    sizeStr = dataSeq->size + 1;
    lastIndex = dataSeq->size;
    if (req4na->hasSentinelBytes)
    {
    	sizeStr -=2;
    	iByteIndex = 1;
    	lastIndex -=1;
    }

    *strData = (char*)calloc(sizeStr, sizeof(char));
    if (!(*strData))
    {
        return FALSE;
    }

    // Convert the array of bytes to string
    for (; iByteIndex < lastIndex; iByteIndex++)
    {
        uint8_t byteCur = dataSeq->data[iByteIndex] & 0x0F;

        (*strData)[iByteIndex] = NCBI4NA_TO_IUPACNA[byteCur];
    }

    (*strData)[iByteIndex] = '\0';

    return TRUE;
}
Boolean
VDBSRC_Get4naSequenceAsString(TVDBData* vdbData,
                           uint64_t oid,
                           char** seqIupacna,
                           TVDBErrMsg * vdbErrMsg)
{
    TNuclDataRequest req;
    TByteArray dataSeq;
    ASSERT(vdbData);
    ASSERT(seqIupacna);
   // Open the read for given OID
    req.read4na = TRUE;
    req.copyData = TRUE;
    req.hasSentinelBytes = FALSE;
    req.convertDataToBlastna = FALSE;
    req.readId = oid;

    if((vdbData->reader_4na == NULL )) {
    	VDBSRC_Init4naReader(vdbData, vdbErrMsg);
    }
    if(vdbErrMsg->isError)
    {
    	return FALSE;
    }

    VDBSRC_InitByteArray_Empty(&dataSeq);
    //clock_t start = clock(), diff;
    if(!VDBSRC_GetSeq4naCopy( vdbData, &dataSeq, &req, vdbErrMsg))
    	return FALSE;

    /*
    diff = clock() - start;
    		int msec = diff * 1000 / CLOCKS_PER_SEC;
    		printf("Time taken %d seconds %d milliseconds\n", msec/1000, msec%1000);
    		*/
    if (!VDBSRC_Convert4naToString(&dataSeq, &req, seqIupacna))
    {
    	free(dataSeq.data);
    	VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_4NA_SEQ_STRING_ERROR);
        return FALSE;
    }

    free(dataSeq.data);
    return TRUE;
}

// Note that 2na reader can only read in sequential order
// The oid is provided as sanity check to make sure
// the application iterations count is in sycn with the
// internal count in the vdb lib
Boolean
VDBSRC_Get2naSequenceAsString(TVDBData* vdbData,
                  	  	  	  uint64_t oid,
                  	  	  	  char** seqIupacna,
                  	  	  	  TVDBErrMsg * vdbErrMsg)
{
	TNuclDataRequest req;
    TByteArray dataSeq;
    int8_t rv;
    char* strData = 0;
    ASSERT(vdbData);
    ASSERT(seqIupacna);

    // Open the read for given OID

    req.read4na = FALSE;
    req.copyData = FALSE;
    req.hasSentinelBytes = FALSE;
    req.convertDataToBlastna = FALSE;
    req.readId = oid;

    VDBSRC_InitByteArray_Empty(&dataSeq);
    rv = VDBSRC_GetSeq2na(vdbData, &dataSeq, &req, vdbErrMsg);

    if(rv == BLAST_SEQSRC_ERROR)
       	return FALSE;

    // Convert it to its string representation

    if (!VDBSRC_Convert2naToString(&dataSeq, &strData))
    {
      	VDBSRC_InitLocalErrorMsg(vdbErrMsg, eVDBSRC_2NA_SEQ_STRING_ERROR);
        return FALSE;
    }

    *seqIupacna = strData;
    return TRUE;
}
// ==========================================================================//

#ifdef __cplusplus
}
#endif

