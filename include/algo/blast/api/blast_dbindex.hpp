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

NCBI_XBLAST_EXPORT
std::string DbIndexInit( 
        const string & indexname, bool old_style, bool & partial );

NCBI_XBLAST_EXPORT
void SetUpDbIndexCallbacks( void );

NCBI_XBLAST_EXPORT
void ClearDbIndexCallbacks( void );

/** Get minimal word size accepted by indexing library.
*/
NCBI_XBLAST_EXPORT
int MinIndexWordSize();

//------------------------------------------------------------------------------
/// Index wrapper exceptions.
class CIndexedDbException : public CException
{
    public:

        /// Error types that BLAST can generate
        enum EErrCode {
            eIndexInitError,    ///< Index initialization error.
            eDBMismatch         ///< index-db inconsistency
        };

        /// Translate from the error code value to its string representation
        virtual const char* GetErrCodeString(void) const override {
            switch ( GetErrCode() ) {
                case eIndexInitError: return "eIndexInitError";
                case eDBMismatch: return "inconsistent database";
                default: return CException::GetErrCodeString();
            }
        }

        NCBI_EXCEPTION_DEFAULT( CIndexedDbException, CException );
};

//------------------------------------------------------------------------------
/// Type of a callback to set the concurrency state in the index structure.
/// @param multiple_threads 'true' if MegaBLAST is run with multiple threads
typedef void (*DbIndexSetUsingThreadsFnType)( bool multiple_threads );

/// Return the appropriate callback to set the concurrency state in the index
/// structure.
/// @return No-op function if indexing is not used;
///         otherwise - the appropriate callback.
extern DbIndexSetUsingThreadsFnType GetDbIndexSetUsingThreadsFn();

//------------------------------------------------------------------------------
/// Type of a callback to provide the number of threads to the indexing
/// library, when multi-threaded search is used.
/// @param n_threads number of threads
typedef void (*DbIndexSetNumThreadsFnType)( size_t n_threads );

/// Return the appropriate callback to set the number of threads in the 
/// index structure.
/// @return No-op function if indexing is not used;
///         otherwise - the appropriate callback.
extern DbIndexSetNumThreadsFnType GetDbIndexSetNumThreadsFn();

//------------------------------------------------------------------------------
/// Type of a callback to set the query information in the index structure.
/// @param lt_wrap lookup table wrapper object
/// @param locs_wrap lookup (unmasked) segments
typedef void (*DbIndexSetQueryInfoFnType)( 
        LookupTableWrap * lt_wrap, 
        CRef< CBlastSeqLocWrap > locs_wrap );

/// Return the appropriate callback to set query information in the index. 
/// @return No-op function if indexing is not being used;
///         otherwise - the appropriate callback.
extern DbIndexSetQueryInfoFnType GetDbIndexSetQueryInfoFn();

//------------------------------------------------------------------------------
/// Type of a callback to run the indexed seed search.
/// @param queries query information structure
/// @param lut_options lookup table parameters
/// @param word_options word options
typedef void (*DbIndexRunSearchFnType)( 
        BLAST_SequenceBlk * queries, 
        LookupTableOptions * lut_options, 
        BlastInitialWordOptions * word_options );

/// Return the appropriate callback to run indexed seed search.
/// @return No-op function if indexing is not being used;
///         otherwise - the appropriate callback.
extern DbIndexRunSearchFnType GetDbIndexRunSearchFn();

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_API___BLAST_DBINDEX__HPP */

