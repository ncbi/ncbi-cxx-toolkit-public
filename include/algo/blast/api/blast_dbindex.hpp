/* $Id$
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

/// @file blast_dbindex.hpp
/// Declarations for indexed blast databases

#ifndef ALGO_BLAST_API___BLAST_DBINDEX__HPP
#define ALGO_BLAST_API___BLAST_DBINDEX__HPP

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_seqsrc_impl.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_filter.h>

#include <algo/blast/api/blast_types.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/** Wrap a BlastSeqSrc object db by another object that provides index based
    seed searching.

    @param indexname Name of the file containing index data.
    @param db        Blast database object.
    @return New BlastSeqSrc object the is a wrapper around the original one.
*/
NCBI_XBLAST_EXPORT
BlastSeqSrc * DbIndexSeqSrcInit( const string & indexname, BlastSeqSrc * db );

/** Creates a clone of a BlastSeqSrc structure.
    
    @param src Original sequence source.

    @return New BlastSeqSrc object initialized with elements of the given one.
*/
NCBI_XBLAST_EXPORT
BlastSeqSrc * CloneSeqSrcInit( BlastSeqSrc * src );

/** Copies the contents of src to dst.
    
    @param dst destination BlastSeqSrc object
    @param src source BlastSeqSrc object
*/
NCBI_XBLAST_EXPORT
void CloneSeqSrc( BlastSeqSrc * dst, BlastSeqSrc * src );

/** Get minimal word size accepted by indexing library.
*/
NCBI_XBLAST_EXPORT
int MinIndexWordSize();

/** Type of a callback that is called to invoke index based search.

    @param seq_src      Indexed database source object.
    @param lt_wrap      Wrapper for CIndexedDb.
    @param queries      Queries descriptor.
    @param locs         Unmasked intervals of queries.
    @param lut_options  Lookup table parameters, like target word size.
    @param word_options Contains window size of two-hits based search.
*/
typedef void (*DbIndexPreSearchFnType)( 
        BlastSeqSrc * seq_src, LookupTableWrap * lt_wrap, 
        BLAST_SequenceBlk * queries, BlastSeqLoc * locs,
        LookupTableOptions * lut_options, 
        BlastInitialWordOptions * word_options );

/** Return the appropriate pre-search callback. 

    @return No-op function if indexing is not being used;
            otherwise - the appropriate callback.
*/
extern DbIndexPreSearchFnType GetDbIndexPreSearchFn();

//------------------------------------------------------------------------------
/// Index wrapper exceptions.
class CIndexedDbException : public CException
{
    public:

        /// Error types that BLAST can generate
        enum EErrCode {
            eIndexInitError     ///< Index initialization error.
        };

        /// Translate from the error code value to its string representation
        virtual const char* GetErrCodeString(void) const {
            switch ( GetErrCode() ) {
                case eIndexInitError: return "eIndexInitError";
                default: return CException::GetErrCodeString();
            }
        }

        NCBI_EXCEPTION_DEFAULT( CIndexedDbException, CException );
};

//------------------------------------------------------------------------------
/// Type of a callback to set the query information in the index structure.
/// @param seq_src sequence source (index)
/// @param lt_wrap lookup table wrapper object
/// @param locs_wrap lookup (unmasked) segments
typedef void (*DbIndexSetQueryInfoFnType)( 
        BlastSeqSrc * seq_src, 
        LookupTableWrap * lt_wrap, 
        CRef< CBlastSeqLocWrap > locs_wrap );

/// Return the appropriate callback to set query information in the index. 
/// @return No-op function if indexing is not being used;
///         otherwise - the appropriate callback.
extern DbIndexSetQueryInfoFnType GetDbIndexSetQueryInfoFn();

//------------------------------------------------------------------------------
/// Type of a callback to run the indexed seed search.
/// @param seq_src sequence source (index)
/// @param queries query information structure
/// @param lut_options lookup table parameters
/// @param word_options word options
typedef void (*DbIndexRunSearchFnType)( 
        BlastSeqSrc * seq_src, BLAST_SequenceBlk * queries, 
        LookupTableOptions * lut_options, 
        BlastInitialWordOptions * word_options );

/// Return the appropriate callback to run indexed seed search.
/// @return No-op function if indexing is not being used;
///         otherwise - the appropriate callback.
extern DbIndexRunSearchFnType GetDbIndexRunSearchFn();

//------------------------------------------------------------------------------
/// Type of a callback to set the number of threads for indexed seed search.
/// @param seq_src sequence source (index)
/// @param n_threads number of threads
typedef void (*DbIndexSetNumThreadsFnType)( 
        BlastSeqSrc * seq_src, size_t n_threads );

/// Return the appropriate callback to set the number of threads for indexed
/// seed search.
/// @return No-op function if indexing is not being used;
///         otherwise - the appropriate callback.
extern DbIndexSetNumThreadsFnType GetDbIndexSetNumThreadsFn();

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_API___BLAST_DBINDEX__HPP */

