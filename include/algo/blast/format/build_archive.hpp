#ifndef ALGO_BLAST_API___BUILD_ARCHIVE__HPP
#define ALGO_BLAST_API___BUILD_ARCHIVE__HPP

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
 * Authors:  Tom Madden
 *
 */

/// @file build_archive.hpp
/// build_archive declarations

#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_options_builder.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    /// forward declaration of ASN.1 object containing PSSM (scoremat.asn)
    class CPssmWithParameters;
    class CBioseq_set;
    class CSeq_loc;
    class CSeq_id;
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// Returns a blast archive object.
/// @param queries factory to provide queries
/// @param options_handle BLAST options
/// @param results set of BLAST results
/// @param CSearchDatabase  search database
/// @param num_iters psi iteration number
NCBI_XBLASTFORMAT_EXPORT
CRef<objects::CBlast4_archive> BlastBuildArchive(blast::IQueryFactory& queries,
                          blast::CBlastOptionsHandle& options_handle,
                          const CSearchResultSet& results,
                          CRef<CSearchDatabase> search_db,
                          unsigned int num_iters = 0);

/// Returns a blast archive object.
/// @param queries factory to provide queries
/// @param options_handle BLAST options
/// @param results set of BLAST results
/// @param subjects factory to fetch subject sequences.
NCBI_XBLASTFORMAT_EXPORT
CRef<objects::CBlast4_archive> BlastBuildArchive(blast::IQueryFactory& queries, 
                          blast::CBlastOptionsHandle& options_handle,
                          const CSearchResultSet& results,
                          blast::IQueryFactory& subjects);

/// Returns a blast archive object.
/// @param queries factory to provide queries
/// @param options_handle BLAST options
/// @param results set of BLAST results
/// @param CSearchDatabase  search database
/// @param num_iters psi iteration number
NCBI_XBLASTFORMAT_EXPORT
CRef<objects::CBlast4_archive> BlastBuildArchive(objects::CPssmWithParameters & pssm,
                  	  	  	  	  	  	  	     blast::CBlastOptionsHandle& options_handle,
                  	  	  	  	  	  	  	     const CSearchResultSet& results,
                  	  	  	  	  	  	  	     CRef<CSearchDatabase>  search_db,
                  	  	  	  	  	  	  	     unsigned int num_iters = 0);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BUILD_ARCHIVE__HPP */
