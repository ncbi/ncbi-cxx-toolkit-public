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
* Author: Aleksandr Morgulis
*
*/

/// @file blast_dbindex.cpp
/// Functionality for indexed databases

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/mb_indexed_lookup.h>
#include <algo/blast/api/blast_dbindex.hpp>
#include <algo/blast/dbindex/dbindex.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */


extern "C" {

static BlastSeqSrc * s_IDbSrcNew( BlastSeqSrc * retval, void * args );

static void s_MB_IdbGetResults(
        void * idb_v,
        Int4 oid_i, Int4 chunk_i,
        BlastInitHitList * init_hitlist );
}

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blast )

USING_SCOPE( ncbi::objects );
USING_SCOPE( ncbi::blastdbindex );

static void NullPreSearch( 
        BlastSeqSrc *, LookupTableWrap *, 
        BLAST_SequenceBlk *, BlastSeqLoc *,
        LookupTableOptions *, BlastInitialWordOptions * ) {}

static DbIndexPreSearchFnType PreSearchFn = &NullPreSearch;
    
//------------------------------------------------------------------------------
struct SIndexedDbNewArgs
{
    string indexname;
    BlastSeqSrc * db;
};

//------------------------------------------------------------------------------
class CIndexedDb : public CObject
{
    private:

        struct SThreadLocal 
        {
            CRef< CIndexedDb > idb_;
        };

        BlastSeqSrc * db_;
        CRef< CDbIndex > index_;
        CConstRef< CDbIndex::CSearchResults > results_;

    public:

        typedef SThreadLocal TThreadLocal;

        explicit CIndexedDb( const string & indexname, BlastSeqSrc * db );
        ~CIndexedDb();

        BlastSeqSrc * GetDb() const { return db_; }

        void * GetSeqDb() const 
        { return _BlastSeqSrcImpl_GetDataStructure( db_ ); }

        bool CheckOid( Int4 oid ) const
        { return index_->CheckResults( results_, (CDbIndex::TSeqNum)oid ); }

        void PreSearch( 
                BLAST_SequenceBlk *, BlastSeqLoc *,
                LookupTableOptions *,
                BlastInitialWordOptions * );

        void GetResults( 
                CDbIndex::TSeqNum oid,
                CDbIndex::TSeqNum chunk,
                BlastInitHitList * init_hitlist ) const;
};

//------------------------------------------------------------------------------
static void IndexedDbPreSearch( 
        BlastSeqSrc * seq_src, 
        LookupTableWrap * lt_wrap,
        BLAST_SequenceBlk * queries, 
        BlastSeqLoc * locs,
        LookupTableOptions * lut_options, 
        BlastInitialWordOptions * word_options )
{
    CIndexedDb::TThreadLocal * idb_tl = 
        static_cast< CIndexedDb::TThreadLocal * >(
                _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
    CIndexedDb * idb = idb_tl->idb_.GetPointerOrNull();
    lt_wrap->lut = (void *)idb;
    lt_wrap->read_indexed_db = (void *)(&s_MB_IdbGetResults);
    idb->PreSearch( queries, locs, lut_options, word_options );
}

//------------------------------------------------------------------------------
CIndexedDb::CIndexedDb( const string & indexname, BlastSeqSrc * db )
    : db_( db ), index_( CDbIndex::Load( indexname ) ), results_( null )
{
    if( index_ == 0 ) {
        throw std::runtime_error(
                "CIndexedDb: could not load index" );
    }

    PreSearchFn = &IndexedDbPreSearch;
}

//------------------------------------------------------------------------------
CIndexedDb::~CIndexedDb()
{
    PreSearchFn = &NullPreSearch;
    BlastSeqSrcFree( db_ );
}

void CIndexedDb::PreSearch( 
        BLAST_SequenceBlk * queries, BlastSeqLoc * locs,
        LookupTableOptions * lut_options , 
        BlastInitialWordOptions * word_options )
{
    CDbIndex::SSearchOptions sopt;
    sopt.word_size = lut_options->word_size;
    sopt.template_type = lut_options->mb_template_type;
    sopt.two_hits = (word_options->window_size > 0);
    results_ = index_->Search( queries, locs, sopt );
}

//------------------------------------------------------------------------------
void CIndexedDb::GetResults( 
        CDbIndex::TSeqNum oid, CDbIndex::TSeqNum chunk, 
        BlastInitHitList * init_hitlist ) const
{
    BlastInitHitList * res = 0;

    if( (res = index_->ExtractResults( results_, oid, chunk )) != 0 ) {
        BlastInitHitListMove( init_hitlist, res );
    }else {
        BlastInitHitListReset( init_hitlist );
    }
}

//------------------------------------------------------------------------------
BlastSeqSrc * DbIndexSeqSrcInit( const string & indexname, BlastSeqSrc * db )
{
    BlastSeqSrcNewInfo bssn_info;
    SIndexedDbNewArgs idb_args = { indexname, db };
    bssn_info.constructor = &s_IDbSrcNew;
    bssn_info.ctor_argument = (void *)&idb_args;
    return BlastSeqSrcNew( &bssn_info );
}

//------------------------------------------------------------------------------
DbIndexPreSearchFnType GetDbIndexPreSearchFn() { return PreSearchFn; }

END_SCOPE( blast )
END_NCBI_SCOPE

USING_SCOPE( ncbi );
USING_SCOPE( ncbi::blast );

extern "C" {

//------------------------------------------------------------------------------
static BlastSeqSrc * s_GetForwardSeqSrc( void * handle )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    return idb_handle->idb_->GetDb();
}

//------------------------------------------------------------------------------
void * s_GetForwardSeqDb( void * handle )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    return idb_handle->idb_->GetSeqDb();
}

//------------------------------------------------------------------------------
static Int4 s_IDbGetNumSeqs( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetNumSeqs( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static Int4 s_IDbGetMaxLength( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetMaxSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static Int4 s_IDbGetAvgLength( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetAvgSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static Int8 s_IDbGetTotLen( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetTotLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static const char * s_IDbGetName( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetName( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static Boolean s_IDbGetIsProt( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetIsProt( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static Int2 s_IDbGetSequence( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetSequence( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static Int4 s_IDbGetSeqLen( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static Int4 s_IDbIteratorNext( void * handle, BlastSeqSrcIterator * itr )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    CIndexedDb * idb = idb_handle->idb_.GetPointerOrNull();
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    Int4 oid = BLAST_SEQSRC_EOF;

    do {
        oid = _BlastSeqSrcImpl_GetIterNext( fw_seqsrc )( fw_handle, itr );
    }while( oid != BLAST_SEQSRC_EOF && !idb->CheckOid( oid ) );

    return oid;
}

//------------------------------------------------------------------------------
static void s_IDbReleaseSequence( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    _BlastSeqSrcImpl_GetReleaseSequence( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
static BlastSeqSrc * s_IDbSrcFree( BlastSeqSrc * seq_src )
{
    if( seq_src ) {
        CIndexedDb::TThreadLocal * idb = 
            static_cast< CIndexedDb::TThreadLocal * >( 
                    _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
        delete idb;
        sfree( seq_src );
    }

    return 0;
}

//------------------------------------------------------------------------------
static BlastSeqSrc * s_IDbSrcCopy( BlastSeqSrc * seq_src )
{
    if( !seq_src ) {
        return 0;
    }else {
        CIndexedDb::TThreadLocal * idb =
            static_cast< CIndexedDb::TThreadLocal * >(
                    _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
        try {
            CIndexedDb::TThreadLocal * idb_copy = 
                new CIndexedDb::TThreadLocal( *idb );
            _BlastSeqSrcImpl_SetDataStructure( seq_src, (void *)idb_copy );
            return seq_src;
        }catch( ... ) {
            return 0;
        }
    }
}

//------------------------------------------------------------------------------
static void s_IDbSrcInit( BlastSeqSrc * retval, CIndexedDb::TThreadLocal * idb )
{
    ASSERT( retval );
    ASSERT( idb );

    _BlastSeqSrcImpl_SetDeleteFnPtr   (retval, & s_IDbSrcFree);
    _BlastSeqSrcImpl_SetCopyFnPtr     (retval, & s_IDbSrcCopy);
    _BlastSeqSrcImpl_SetDataStructure (retval, (void*) idb);
    _BlastSeqSrcImpl_SetGetNumSeqs    (retval, & s_IDbGetNumSeqs);
    _BlastSeqSrcImpl_SetGetMaxSeqLen  (retval, & s_IDbGetMaxLength);
    _BlastSeqSrcImpl_SetGetAvgSeqLen  (retval, & s_IDbGetAvgLength);
    _BlastSeqSrcImpl_SetGetTotLen     (retval, & s_IDbGetTotLen);
    _BlastSeqSrcImpl_SetGetName       (retval, & s_IDbGetName);
    _BlastSeqSrcImpl_SetGetIsProt     (retval, & s_IDbGetIsProt);
    _BlastSeqSrcImpl_SetGetSequence   (retval, & s_IDbGetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen     (retval, & s_IDbGetSeqLen);
    _BlastSeqSrcImpl_SetIterNext      (retval, & s_IDbIteratorNext);
    _BlastSeqSrcImpl_SetReleaseSequence   (retval, & s_IDbReleaseSequence);
}

//------------------------------------------------------------------------------
static BlastSeqSrc * s_IDbSrcNew( BlastSeqSrc * retval, void * args )
{
    ASSERT( retval );
    ASSERT( args );
    SIndexedDbNewArgs * idb_args = (SIndexedDbNewArgs *)args;
    CIndexedDb::TThreadLocal * idb = 0;
    bool success = false;

    try {
        idb = new CIndexedDb::TThreadLocal;
        idb->idb_.Reset( new CIndexedDb( idb_args->indexname, idb_args->db ) );
        success = true;
    } catch (const ncbi::CException& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
                        strdup(e.ReportThis(eDPF_ErrCodeExplanation).c_str()));
    } catch (const std::exception& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(e.what()));
    } catch (...) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
             strdup("Caught unknown exception from CSeqDB constructor"));
    }

    if( success ) {
        s_IDbSrcInit( retval, idb );
    }else {
        ERR_POST( "Index load operation failed." );
        delete idb;
        sfree( retval );
    }

    return retval;
}

static void s_MB_IdbGetResults(
        void * idb_v,
        Int4 oid_i, Int4 chunk_i,
        BlastInitHitList * init_hitlist )
{
    ASSERT( idb_v != 0 );
    ASSERT( oid_i >= 0 );
    ASSERT( chunk_i >= 0 );
    ASSERT( init_hitlist != 0 );

    CIndexedDb * idb = (CIndexedDb *)idb_v;
    CDbIndex::TSeqNum oid = (CDbIndex::TSeqNum)oid_i;
    CDbIndex::TSeqNum chunk = (CDbIndex::TSeqNum)chunk_i;

    idb->GetResults( oid, chunk, init_hitlist );
}

} /* extern "C" */

/* @} */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/10/05 15:59:21  papadopo
 * GetResults is static now
 *
 * Revision 1.1  2006/10/04 19:19:45  papadopo
 * interface for indexed blast databases
 *
 * ===========================================================================
 */
