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
* Author:  Ilya Dondoshansky
*
*/

/// @file seqdb_src.cpp
/// Implementation of the BlastSeqSrc interface for a C++ BLAST databases API

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/seqdb_src.hpp>
#include <algo/blast/core/blast_util.h>
#include <objtools/readers/seqdb/seqdb.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

extern "C" {

/* Function prototypes */
static Int4 SeqDbGetMaxLength(void* seqdb_handle, void* ignoreme);
static Int4 SeqDbGetNumSeqs(void* seqdb_handle, void* ignoreme);
static Int8 SeqDbGetTotLen(void* seqdb_handle, void* ignoreme);
static Int4 SeqDbGetAvgLength(void* seqdb_handle, void* ignoreme);
static char* SeqDbGetName(void* seqdb_handle, void* ignoreme);
static char* SeqDbGetDefinition(void* seqdb_handle, void* ignoreme);
static char* SeqDbGetDate(void* seqdb_handle, void* ignoreme);
static Boolean SeqDbGetIsProt(void* seqdb_handle, void* ignoreme);
static ListNode* SeqDbGetSeqLoc(void* seqdb_handle, void* args);
static ListNode* SeqDbGetError(void* seqdb_handle, void* args);

/** Retrieves the length of the longest sequence in the BlastSeqSrc.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param ignoreme Unused by this implementation [in]
 */
static Int4 SeqDbGetMaxLength(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return (*seqdb)->GetMaxLength();
}

/** Retrieves the number of sequences in the BlastSeqSrc.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param ignoreme Unused by this implementation [in]
 */
static Int4 SeqDbGetNumSeqs(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return (*seqdb)->GetNumSeqs();
}

/** Retrieves the total length of all sequences in the BlastSeqSrc.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param ignoreme Unused by this implementation [in]
 */
static Int8 SeqDbGetTotLen(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return (*seqdb)->GetTotalLength();
}

/** Retrieves the average length of sequences in the BlastSeqSrc.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param ignoreme Unused by this implementation [in]
 */
static Int4 SeqDbGetAvgLength(void* seqdb_handle, void* ignoreme)
{
   Int8 total_length = SeqDbGetTotLen(seqdb_handle, ignoreme);
   Int4 num_seqs = MAX(1, SeqDbGetNumSeqs(seqdb_handle, ignoreme));

   return (Int4) (total_length/num_seqs);
}

/** Retrieves the name of the BLAST database.
 * FIXME: RIGHT NOW THERE IS NO GETTER in CSeqDB for database name
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param ignoreme Unused by this implementation [in]
 */
static char* SeqDbGetName(void* /*seqdb_handle*/, void*)
{
#if 0 // FIXME: GetName method not implemented in CSeqDb!!!
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return strdup((*seqdb)->GetName().c_str());
#else
    return NULL;
#endif
}

/** Retrieves the definition (title) of the BLAST database.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param ignoreme Unused by this implementation [in]
 */
static char* SeqDbGetDefinition(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return strdup((*seqdb)->GetTitle().c_str());
}

/** Retrieves the date of the BLAST database.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param ignoreme Unused by this implementation [in]
 */
static char* SeqDbGetDate(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return strdup((*seqdb)->GetDate().c_str());
}

/** Retrieves the date of the BLAST database.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param ignoreme Unused by this implementation [in]
 */
static Boolean SeqDbGetIsProt(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;

    return ((*seqdb)->GetSeqType() == 'p');
}

/** Retrieves the sequence meeting the criteria defined by its second argument.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param args Pointer to GetSeqArg structure [in]
 * @return return codes defined in blast_seqsrc.h
 */
static Int2 SeqDbGetSequence(void* seqdb_handle, void* args)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    GetSeqArg* seqdb_args = (GetSeqArg*) args;
    Int4 oid = -1, len = 0;
    Boolean has_sentinel_byte;
    Boolean buffer_allocated;

    if (!seqdb || !seqdb_args)
        return BLAST_SEQSRC_ERROR;

    oid = seqdb_args->oid;
    Uint1 encoding = seqdb_args->encoding;
    has_sentinel_byte = (encoding == BLASTNA_ENCODING);

    buffer_allocated = 
       (encoding == BLASTNA_ENCODING || encoding == NCBI4NA_ENCODING);

    /* free buffers if necessary */
    if (seqdb_args->seq)
        BlastSequenceBlkClean(seqdb_args->seq);

    const char *buf;
    if (!buffer_allocated) {
        len = (*seqdb)->GetSequence(oid, &buf);
    } else {
        len = (*seqdb)->GetAmbigSeq(oid, &buf, has_sentinel_byte);
    }

    if (len <= 0)
        return BLAST_SEQSRC_ERROR;

    BlastSetUp_SeqBlkNew((Uint1*)buf, len, 0, &seqdb_args->seq, 
                         buffer_allocated);
    
    /* If there is no sentinel byte, and buffer is allocated, i.e. this is
       the traceback stage of a translated search, set "sequence" to the same 
       position as "sequence_start". */
    if (buffer_allocated && !has_sentinel_byte)
       seqdb_args->seq->sequence = seqdb_args->seq->sequence_start;

    /* For preliminary stage, even though sequence buffer points to a memory
       mapped location, we still need to call RetSequence. This can only be
       guaranteed by making the engine believe tat sequence is allocated.
    */
    if (!buffer_allocated)
        seqdb_args->seq->sequence_allocated = TRUE;

    seqdb_args->seq->oid = oid;

    return BLAST_SEQSRC_SUCCESS;
}

/** Returns the memory allocated for the sequence buffer to the CSeqDB 
 * interface.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param args Pointer to the GetSeqArgs structure, containing sequence block
 *             with the buffer that needs to be deallocated. [in]
 * @return return codes defined in blast_seqsrc.h
 */
static Int2 SeqDbRetSequence(void* seqdb_handle, void* args)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    GetSeqArg* seqdb_args = (GetSeqArg*) args;

    if (!seqdb || !seqdb_args)
        return BLAST_SEQSRC_ERROR;

    if (!seqdb_args->seq) {
        // Nothing to return, just return successful status
        return BLAST_SEQSRC_SUCCESS;
    }

    if (seqdb_args->seq->sequence_start_allocated) {
        (*seqdb)->RetSequence((const char**)&seqdb_args->seq->sequence_start);
        seqdb_args->seq->sequence_start_allocated = FALSE;
        seqdb_args->seq->sequence_start = NULL;
    }
    if (seqdb_args->seq->sequence_allocated) {
        (*seqdb)->RetSequence((const char**)&seqdb_args->seq->sequence);
        seqdb_args->seq->sequence_allocated = FALSE;
        seqdb_args->seq->sequence = NULL;
    }

    return BLAST_SEQSRC_SUCCESS;
}

/** Retrieves the sequence identifier meeting the criteria defined by its 
 * second argument. Currently it is an ordinal id (integer value).
 * Client code is responsible for deallocating the return value. 
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param args Pointer to integer indicating ordinal id [in]
 * @return Sequence id structure generated from ASN.1 spec, 
 *         cast to a void pointer.
 */
static ListNode* SeqDbGetSeqId(void* seqdb_handle, void* args)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    Uint4* oid = (Uint4*) args;
    ListNode* seqid_wrap;

    if (!seqdb || !oid)
        return NULL;

    list< CRef<CSeq_id> > seqid_list;
    seqid_list = (*seqdb)->GetSeqIDs(*oid);

    CRef<CSeq_id>* seqid_ref = new CRef<CSeq_id>(seqid_list.front());
    seqid_wrap = ListNodeAddPointer(NULL, BLAST_SEQSRC_CPP_SEQID_REF, 
                                    (void*) seqid_ref);

    return seqid_wrap;
}

/** Retrieves the sequence identifier meeting the criteria defined by its 
 * second argument. Currently it is an ordinal id (integer value).
 * @todo Need a way to request difference sequence identifiers in redundant
 * databases.
 * Client code is responsible for deallocating the return value. 
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param args Pointer to integer indicating ordinal id [in]
 */
static char* SeqDbGetSeqIdStr(void* seqdb_handle, void* args)
{
    char* seqid_str = NULL;
    ListNode* seqid_wrap = NULL;

    seqid_wrap = SeqDbGetSeqId(seqdb_handle, args);
    if (seqid_wrap->choice != BLAST_SEQSRC_CPP_SEQID_REF)
        return NULL;
    CRef<CSeq_id>* seqid_ref = (CRef<CSeq_id>*) seqid_wrap->ptr;

    ListNodeFree(seqid_wrap);
    if (seqid_ref)
        seqid_str = strdup((*seqid_ref)->GetSeqIdString().c_str());

    delete seqid_ref;
    return seqid_str;
}

/* There is no need to return locations from seqdb, since we always search
   whole sequences in the database */
static ListNode* SeqDbGetSeqLoc(void*, void*)
{
   return NULL;
}

/** Retrieve length of a given database sequence.
 * @param seqdb_handle Pointer to initialized CSeqDB object [in]
 * @param args Pointer to integer indicating ordinal id [in]
 * @return Length of the database sequence or BLAST_SEQSRC_ERROR.
 */
static Int4 SeqDbGetSeqLen(void* seqdb_handle, void* args)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    Int4* oid = (Int4*) args;

    if (!seqdb || !oid)
       return BLAST_SEQSRC_ERROR;

    return (*seqdb)->GetSeqLength(*oid);
}

/* There are no error messages saved in the SeqdbFILE structure, so the 
 * following getter function is implemented as always returning NULL.
 */
static ListNode* SeqDbGetError(void*, void*)
{
    /* FIXME: error handling should not be provided by the seqsrc interface */
   return NULL;
}

static Int2 SeqDbGetNextChunk(void* seqdb_handle, BlastSeqSrcIterator* itr)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;

    if (!seqdb || !itr)
        return BLAST_SEQSRC_ERROR;

    vector<Uint4> oid_list(itr->chunk_sz);

    CSeqDB::EOidListType chunk_type = 
        (*seqdb)->GetNextOIDChunk(itr->oid_range[0], itr->oid_range[1], 
                                  oid_list);
    
    if (chunk_type == CSeqDB::eOidRange) {
        if (itr->oid_range[1] <= itr->oid_range[0])
            return BLAST_SEQSRC_EOF;
        itr->itr_type = eOidRange;
        itr->current_pos = itr->oid_range[0];
    } else if (chunk_type == CSeqDB::eOidList) {
        itr->itr_type = eOidList;
        itr->chunk_sz = oid_list.size();
        if (itr->chunk_sz == 0)
            return BLAST_SEQSRC_EOF;
        itr->current_pos = 0;
        Uint4 index;
        for (index = 0; index < itr->chunk_sz; ++index)
            itr->oid_list[index] = oid_list[index];
    }

    return BLAST_SEQSRC_SUCCESS;
}

static Int4 SeqDbIteratorNext(void* seqsrc, BlastSeqSrcIterator* itr)
{
    BlastSeqSrc* bssp = (BlastSeqSrc*) seqsrc;
    Int4 retval = BLAST_SEQSRC_EOF;
    Int4 status = BLAST_SEQSRC_SUCCESS;

    ASSERT(bssp);
    ASSERT(itr);

    /* If internal iterator is uninitialized/invalid, retrieve the next chunk 
       from the BlastSeqSrc */
    if (itr->current_pos == UINT4_MAX) {
        status = BLASTSeqSrcGetNextChunk(bssp, itr);
        if (status == BLAST_SEQSRC_ERROR || status == BLAST_SEQSRC_EOF) {
            return status;
        }
    }

    Uint4 last_pos = 0;

    if (itr->itr_type == eOidRange) {
        retval = itr->current_pos;
        last_pos = itr->oid_range[1];
    } else if (itr->itr_type == eOidList) {
        retval = itr->oid_list[itr->current_pos];
        last_pos = itr->chunk_sz;
    } else {
        /* Unsupported/invalid iterator type! */
        fprintf(stderr, "Invalid iterator type: %d\n", itr->itr_type);
        abort();
    }

    ++itr->current_pos;
    if (itr->current_pos >= last_pos) {
        itr->current_pos = UINT4_MAX;  /* invalidate internal iteration */
    }

    return retval;
}

}

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

extern "C" {

BlastSeqSrc* SeqDbSrcNew(BlastSeqSrc* retval, void* args)
{
    SSeqDbSrcNewArgs* rargs = (SSeqDbSrcNewArgs*) args;

    if (!retval)
        return NULL;

    ASSERT(rargs);

    string db_name(rargs->dbname);
    char db_type = static_cast<char>((rargs->is_protein ? 'p' : 'n'));

    CSeqDB* seqdb = new CSeqDB(db_name, db_type, (Uint4)rargs->first_db_seq, 
                               (Uint4)rargs->final_db_seq, true);

    CRef<CSeqDB>* seqdb_ref = new CRef<CSeqDB>(seqdb);

    /* Initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and seqdb */
    SetDeleteFnPtr(retval, &SeqDbSrcFree);
    SetCopyFnPtr(retval, &SeqDbSrcCopy);
    SetDataStructure(retval, (void*) seqdb_ref);
    SetGetNumSeqs(retval, &SeqDbGetNumSeqs);
    SetGetMaxSeqLen(retval, &SeqDbGetMaxLength);
    SetGetAvgSeqLen(retval, &SeqDbGetAvgLength);
    SetGetTotLen(retval, &SeqDbGetTotLen);
    SetGetAvgSeqLen(retval, &SeqDbGetAvgLength);
    SetGetName(retval, &SeqDbGetName);
    SetGetDefinition(retval, &SeqDbGetDefinition);
    SetGetDate(retval, &SeqDbGetDate);
    SetGetIsProt(retval, &SeqDbGetIsProt);
    SetGetSequence(retval, &SeqDbGetSequence);
    SetGetSeqIdStr(retval, &SeqDbGetSeqIdStr);
    SetGetSeqId(retval, &SeqDbGetSeqId);
    SetGetSeqLoc(retval, &SeqDbGetSeqLoc);
    SetGetSeqLen(retval, &SeqDbGetSeqLen);
    SetGetNextChunk(retval, &SeqDbGetNextChunk);
    SetIterNext(retval, &SeqDbIteratorNext);
    SetGetError(retval, &SeqDbGetError);
    SetRetSequence(retval, &SeqDbRetSequence);

    return retval;
}

BlastSeqSrc* SeqDbSrcFree(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*)(GetDataStructure(seq_src));
    delete seqdb;
    sfree(seq_src);
    return NULL;
}

BlastSeqSrc* SeqDbSrcCopy(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*)(GetDataStructure(seq_src));
    CRef<CSeqDB>* new_seqdb = new CRef<CSeqDB>(*seqdb);

    SetDataStructure(seq_src, (void*) new_seqdb);
    
    return seq_src;
}

}

BlastSeqSrc* 
SeqDbSrcInit(const char* dbname, Boolean is_prot, Int4 first_seq, 
                      Int4 last_seq, void*)
{
    BlastSeqSrcNewInfo bssn_info;
    BlastSeqSrc* seq_src = NULL;
    SSeqDbSrcNewArgs* seqdb_args = 
        (SSeqDbSrcNewArgs*) calloc(1, sizeof(SSeqDbSrcNewArgs));;
    seqdb_args->dbname = strdup(dbname);
    seqdb_args->is_protein = is_prot ? true : false;
    seqdb_args->first_db_seq = first_seq;
    seqdb_args->final_db_seq = last_seq; 
    bssn_info.constructor = &SeqDbSrcNew;
    bssn_info.ctor_argument = (void*) seqdb_args;

    seq_src = BlastSeqSrcNew(&bssn_info);
    sfree(seqdb_args->dbname);
    sfree(seqdb_args);
    return seq_src;
}


END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.16  2004/07/13 17:31:04  dondosha
 * Test sequence block pointer for null in SeqDbRetSequence
 *
 * Revision 1.15  2004/07/06 15:50:33  dondosha
 * Use GetNextOIDChunk function from CSeqDb for iteration
 *
 * Revision 1.14  2004/06/23 14:07:01  dondosha
 * Call RetSequence for memory-mapped buffer
 *
 * Revision 1.13  2004/06/15 16:24:01  dondosha
 * Use CRef to handle pointers to CSeqDb instance properly; added SeqDbSrcCopy function
 *
 * Revision 1.12  2004/06/02 15:57:57  bealer
 * - Isolate object manager dependant code.
 *
 * Revision 1.11  2004/05/24 17:26:08  camacho
 * Fix PC warning
 *
 * Revision 1.10  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.9  2004/05/19 15:26:47  dondosha
 * Removed unused variables
 *
 * Revision 1.8  2004/05/17 18:11:15  dondosha
 * Minor: eliminates strict compiler warning
 *
 * Revision 1.7  2004/05/17 15:49:07  dondosha
 * Removed local variable declaration in empty SeqDbGetName method, to eliminate compiler warning
 *
 * Revision 1.6  2004/05/17 15:43:21  dondosha
 * Return NULL from SeqDbGetName until CSeqDB gets a GetName method
 *
 * Revision 1.5  2004/04/28 19:38:20  dondosha
 * Added implementation of BLASTSeqSrcRetSequence function
 *
 * Revision 1.4  2004/04/12 14:58:42  ucko
 * Avoid placing static C functions inside namespace blocks to keep the
 * SGI MIPSpro compiler from segfaulting in Scope Setup.
 *
 * Revision 1.3  2004/04/08 14:45:48  camacho
 * 1. Added missing extern "C" declarations
 * 2. Removed compiler warnings about unused parameters.
 * 3. Removed dead code.
 * 4. Added FIXMEs.
 *
 * Revision 1.2  2004/04/07 18:45:13  bealer
 * - Update constructor for CSeqDB to omit obsolete first argument.
 *
 * Revision 1.1  2004/04/06 20:43:59  dondosha
 * Sequence source for CSeqDB BLAST database interface
 *
 * ===========================================================================
 */
