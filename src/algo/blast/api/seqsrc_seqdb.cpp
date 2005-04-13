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

/// @file seqsrc_seqdb.cpp
/// Implementation of the BlastSeqSrc interface for a C++ BLAST databases API

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/core/blast_util.h>
#include <objtools/readers/seqdb/seqdb.hpp>
#include "blast_setup.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

/** @addtogroup AlgoBlast
 *
 * @{
 */

extern "C" {

/// Retrieves the length of the longest sequence in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int4 
s_SeqDbGetMaxLength(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return (*seqdb)->GetMaxLength();
}

/// Retrieves the number of sequences in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int4 
s_SeqDbGetNumSeqs(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return (*seqdb)->GetNumSeqs();
}

/// Retrieves the total length of all sequences in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int8 
s_SeqDbGetTotLen(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return (*seqdb)->GetTotalLength();
}

/// Retrieves the average length of sequences in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param ignoreme Unused by this implementation [in]
static Int4 
s_SeqDbGetAvgLength(void* seqdb_handle, void* ignoreme)
{
   Int8 total_length = s_SeqDbGetTotLen(seqdb_handle, ignoreme);
   Int4 num_seqs = MAX(1, s_SeqDbGetNumSeqs(seqdb_handle, ignoreme));

   return (Int4) (total_length/num_seqs);
}

/// Retrieves the name of the BLAST database.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static const char* 
s_SeqDbGetName(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    return (*seqdb)->GetDBNameList().c_str();
}

/// Retrieves the date of the BLAST database.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Boolean 
s_SeqDbGetIsProt(void* seqdb_handle, void*)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;

    return ((*seqdb)->GetSeqType() == 'p');
}

/// Retrieves the sequence meeting the criteria defined by its second argument.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param args Pointer to BlastSeqSrcGetSeqArg structure [in]
/// @return return codes defined in blast_seqsrc.h
static Int2 
s_SeqDbGetSequence(void* seqdb_handle, void* args)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    BlastSeqSrcGetSeqArg* seqdb_args = (BlastSeqSrcGetSeqArg*) args;
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
       mapped location, we still need to call ReleaseSequence. This can only be
       guaranteed by making the engine believe tat sequence is allocated.
    */
    if (!buffer_allocated)
        seqdb_args->seq->sequence_allocated = TRUE;

    seqdb_args->seq->oid = oid;

    return BLAST_SEQSRC_SUCCESS;
}

/// Returns the memory allocated for the sequence buffer to the CSeqDB 
/// interface.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param args Pointer to the BlastSeqSrcGetSeqArgs structure, 
/// containing sequence block with the buffer that needs to be deallocated. [in]
static void
s_SeqDbReleaseSequence(void* seqdb_handle, void* args)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    BlastSeqSrcGetSeqArg* seqdb_args = (BlastSeqSrcGetSeqArg*) args;

    ASSERT(seqdb);
    ASSERT(seqdb_args);
    ASSERT(seqdb_args->seq);

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
}

/// Retrieve length of a given database sequence.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param args Pointer to integer indicating ordinal id [in]
/// @return Length of the database sequence or BLAST_SEQSRC_ERROR.
static Int4 
s_SeqDbGetSeqLen(void* seqdb_handle, void* args)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;
    Int4* oid = (Int4*) args;

    if (!seqdb || !oid)
       return BLAST_SEQSRC_ERROR;

    return (*seqdb)->GetSeqLength(*oid);
}

/// Assigns next chunk of the database to the sequence source iterator.
/// @param seqdb_handle Reference to the database object, cast to void* to 
///                     satisfy the signature requirement. [in]
/// @param itr Iterator over the database sequence source. [in|out]
static Int2 
s_SeqDbGetNextChunk(void* seqdb_handle, BlastSeqSrcIterator* itr)
{
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*) seqdb_handle;

    if (!seqdb || !itr)
        return BLAST_SEQSRC_ERROR;

    vector<int> oid_list(itr->chunk_sz);

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
        itr->chunk_sz = (Uint4) oid_list.size();
        if (itr->chunk_sz == 0)
            return BLAST_SEQSRC_EOF;
        itr->current_pos = 0;
        Uint4 index;
        for (index = 0; index < itr->chunk_sz; ++index)
            itr->oid_list[index] = oid_list[index];
    }

    return BLAST_SEQSRC_SUCCESS;
}

/// Finds the next not searched ordinal id in the iteration over BLAST database.
/// @param ptr BlastSeqSrc pointer, cast to void* to satisfy uniform signature
///            requirement. [in]
/// @param itr Iterator of the BlastSeqSrc pointed by ptr. [in]
/// @return Next ordinal id.
static Int4 
s_SeqDbIteratorNext(void* ptr, BlastSeqSrcIterator* itr)
{
    BlastSeqSrc* seq_src = (BlastSeqSrc*) ptr;
    Int4 retval = BLAST_SEQSRC_EOF;
    Int4 status = BLAST_SEQSRC_SUCCESS;

    ASSERT(seq_src);
    ASSERT(itr);

    /* If internal iterator is uninitialized/invalid, retrieve the next chunk 
       from the BlastSeqSrc */
    if (itr->current_pos == UINT4_MAX) {
        status = BLASTSeqSrcGetNextChunk(seq_src, itr);
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

/// Encapsulates the arguments needed to initialize CSeqDB.
class CSeqDbSrcNewArgs {
public:
    /// Constructor
    CSeqDbSrcNewArgs(const string& db, bool is_prot,
                     Uint4 first_oid = 0, Uint4 final_oid = 0)
        : m_DbName(db), m_IsProtein(is_prot), 
          m_FirstDbSeq(first_oid), m_FinalDbSeq(final_oid)
    {}

    /// Getter functions for the private fields
    const string GetDbName() const { return m_DbName; }
    /// Returns database type: protein or nucleotide
    char GetDbType() const { return m_IsProtein ? 'p' : 'n'; }
    /// Returns first database ordinal id covered by this BlastSeqSrc
    Uint4 GetFirstOid() const { return m_FirstDbSeq; }
    /// Returns last database ordinal id covered by this BlastSeqSrc
    Uint4 GetFinalOid() const { return m_FinalDbSeq; }
    /// Should memory mapping be used? Always returns true.
    bool GetUseMmap() const { return true; }

private:
    string m_DbName;        ///< Database name
    bool m_IsProtein;       ///< Is this database protein?
    Uint4 m_FirstDbSeq;     ///< Ordinal id of the first sequence to search
    Uint4 m_FinalDbSeq;     ///< Ordinal id of the last sequence to search
};

extern "C" {

/// SeqDb sequence source destructor: frees its internal data structure and the
/// BlastSeqSrc structure itself.
/// @param seq_src BlastSeqSrc structure to free [in]
/// @return NULL
static BlastSeqSrc* 
s_SeqDbSrcFree(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*)(GetDataStructure(seq_src));
    delete seqdb;
    sfree(seq_src);
    return NULL;
}

/// SeqDb sequence source copier: creates a new reference to the CSeqDB object
/// and copies the rest of the BlastSeqSrc structure.
/// @param seq_src BlastSeqSrc structure to copy [in]
/// @return Pointer to the new BlastSeqSrc.
static BlastSeqSrc* 
s_SeqDbSrcCopy(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CRef<CSeqDB>* seqdb = (CRef<CSeqDB>*)(GetDataStructure(seq_src));
    CRef<CSeqDB>* new_seqdb = new CRef<CSeqDB>(*seqdb);

    SetDataStructure(seq_src, (void*) new_seqdb);
    
    return seq_src;
}

/// Initializes the data structure and function pointers in a SeqDb based 
/// BlastSeqSrc.
/// @param retval Structure to populate [in] [out]
/// @param seqdb Reference to a CSeqDB object [in]
static void 
s_InitNewSeqDbSrc(BlastSeqSrc* retval, CRef<CSeqDB> * seqdb)
{
    ASSERT(retval);
    ASSERT(seqdb);
    
    /* Initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and seqdb */
    SetDeleteFnPtr   (retval, & s_SeqDbSrcFree);
    SetCopyFnPtr     (retval, & s_SeqDbSrcCopy);
    SetDataStructure (retval, (void*) seqdb);
    SetGetNumSeqs    (retval, & s_SeqDbGetNumSeqs);
    SetGetMaxSeqLen  (retval, & s_SeqDbGetMaxLength);
    SetGetAvgSeqLen  (retval, & s_SeqDbGetAvgLength);
    SetGetTotLen     (retval, & s_SeqDbGetTotLen);
    SetGetName       (retval, & s_SeqDbGetName);
    SetGetIsProt     (retval, & s_SeqDbGetIsProt);
    SetGetSequence   (retval, & s_SeqDbGetSequence);
    SetGetSeqLen     (retval, & s_SeqDbGetSeqLen);
    SetGetNextChunk  (retval, & s_SeqDbGetNextChunk);
    SetIterNext      (retval, & s_SeqDbIteratorNext);
    SetReleaseSequence   (retval, & s_SeqDbReleaseSequence);
}

/// Populates a BlastSeqSrc, creating a new reference to the already existing 
/// SeqDb object.
/// @param retval Original BlastSeqSrc [in]
/// @param args Pointer to a reference to CSeqDB object [in]
/// @return retval
static BlastSeqSrc* 
s_SeqDbSrcSharedNew(BlastSeqSrc* retval, void* args)
{
    if ( !retval ) {
        return (BlastSeqSrc*) NULL;
    }
    
    CRef<CSeqDB> * seqdb = (CRef<CSeqDB> *) args;
    CRef<CSeqDB> * cref  = new CRef<CSeqDB>(*seqdb);
    
    s_InitNewSeqDbSrc(retval, cref);
    
    return retval;
}

/// SeqDb sequence source constructor 
/// @param retval BlastSeqSrc structure (already allocated) to populate [in]
/// @param args Pointer to internal CSeqDbSrcNewArgs structure (@sa
/// CSeqDbSrcNewArgs) [in]
/// @return Updated seq_src structure (with all function pointers initialized
static BlastSeqSrc* 
s_SeqDbSrcNew(BlastSeqSrc* retval, void* args)
{
    if ( !retval ) {
        return (BlastSeqSrc*) NULL;
    }
    
    CSeqDbSrcNewArgs* seqdb_args = (CSeqDbSrcNewArgs*) args;
    ASSERT(seqdb_args);
    
    CRef<CSeqDB>* seqdb = new CRef<CSeqDB>(NULL);
    try {
        seqdb->Reset(new CSeqDB(seqdb_args->GetDbName(), 
                                seqdb_args->GetDbType(), 
                                seqdb_args->GetFirstOid(),
                                seqdb_args->GetFinalOid(),
                                seqdb_args->GetUseMmap()));
    } catch (const ncbi::CException& e) {
        SetInitErrorStr(retval, 
                        strdup(e.ReportThis(eDPF_ErrCodeExplanation).c_str()));
    } catch (const std::exception& e) {
        SetInitErrorStr(retval, strdup(e.what()));
    } catch (...) {
        SetInitErrorStr(retval, strdup("Caught unknown exception from CSeqDB"
                                       " constructor"));
    }
    
    /* Initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and seqdb */
    
    s_InitNewSeqDbSrc(retval, seqdb);
    
    return retval;
}

}

BlastSeqSrc* 
SeqDbBlastSeqSrcInit(const string& dbname, bool is_prot, 
                     Uint4 first_seq, Uint4 last_seq, void*)
{
    BlastSeqSrcNewInfo bssn_info;
    BlastSeqSrc* seq_src = NULL;
    CSeqDbSrcNewArgs seqdb_args(dbname, is_prot, first_seq, last_seq);

    bssn_info.constructor = &s_SeqDbSrcNew;
    bssn_info.ctor_argument = (void*) &seqdb_args;
    seq_src = BlastSeqSrcNew(&bssn_info);
    return seq_src;
}

BlastSeqSrc* 
SeqDbBlastSeqSrcInit(CSeqDB * seqdb)
{
    BlastSeqSrcNewInfo bssn_info;
    BlastSeqSrc * seq_src = NULL;
    CRef<CSeqDB> db(seqdb);
    
    bssn_info.constructor = & s_SeqDbSrcSharedNew;
    bssn_info.ctor_argument = (void*) & db;
    seq_src = BlastSeqSrcNew(& bssn_info);
    return seq_src;
}


END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.31  2005/04/13 22:34:47  camacho
 * Renamed BlastSeqSrc RetSequence to ReleaseSequence
 *
 * Revision 1.30  2005/03/31 16:15:03  dondosha
 * Some doxygen fixes
 *
 * Revision 1.29  2005/02/09 21:03:36  dondosha
 * Minor doxygen fixes
 *
 * Revision 1.28  2005/02/08 18:50:29  bealer
 * - Fix type truncation warnings.
 *
 * Revision 1.27  2005/02/07 20:57:23  bealer
 * - Changes for Uint4->int usage in SeqDB.
 *
 * Revision 1.26  2005/02/02 05:04:47  dondosha
 * Doxygen comments
 *
 * Revision 1.25  2005/01/26 21:03:29  dondosha
 * Made internal functions static, moved internal class from .hpp file
 *
 * Revision 1.24  2005/01/07 14:02:09  camacho
 * doxygen fixes
 *
 * Revision 1.23  2004/12/20 17:02:10  bealer
 * - New SeqSrc construction function to share existing SeqDB object.
 *
 * Revision 1.22  2004/11/24 00:31:40  camacho
 * Construct error initialization string with error code explanation only
 *
 * Revision 1.21  2004/11/17 20:26:01  camacho
 * 1. Implemented error handling at initialization time.
 * 2. Removed user-defined unimplemented error function as it is no longer needed.
 * 3. Made initialization function name consistent with other BlastSeqSrc
 *    implementations
 *
 * Revision 1.20  2004/10/06 14:56:13  dondosha
 * Remove methods that are no longer supported by interface
 *
 * Revision 1.19  2004/09/23 15:46:48  dondosha
 * Use GetDBNameList method of CSeqDb class to get database name
 *
 * Revision 1.18  2004/07/19 14:58:47  dondosha
 * Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
 *
 * Revision 1.17  2004/07/19 13:55:04  dondosha
 * Removed GetSeqLoc method
 *
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
 * Added implementation of BLASTSeqSrcReleaseSequence function
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
