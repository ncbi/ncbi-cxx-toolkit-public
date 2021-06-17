#ifndef ALGO_BLAST_API___BLAST_SEQINFOSRC_AUX__HPP
#define ALGO_BLAST_API___BLAST_SEQINFOSRC_AUX__HPP

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
* Author: Vahram Avagyan
*
*/

/// @file blast_seqinfosrc_aux.hpp
/// Declarations of auxiliary functions using IBlastSeqInfoSrc to
/// retrieve ids and related sequence information.
///

#include <corelib/ncbistd.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Retrieves subject sequence Seq-id and length.
/// @param seqinfo_src Source of subject sequences information [in]
/// @param oid Ordinal id (index) of the subject sequence [in]
/// @param seqid Subject sequence identifier to fill [out]
/// @param length Subject sequence length [out]
NCBI_XBLAST_EXPORT
void GetSequenceLengthAndId(const IBlastSeqInfoSrc* seqinfo_src, 
                        int oid,
                        CRef<objects::CSeq_id>& seqid, 
                        TSeqPos* length);

/// Retrieves subject sequence Seq-id and length, using the provided sequence
/// id ranking function.
/// @param seqinfo_src Source of subject sequences information [in]
/// @param oid Ordinal id (index) of the subject sequence [in]
/// @param rank_func Sequence id ranking function [in]
/// @param seqid Subject sequence identifier to fill [out]
/// @param length Subject sequence length [out]
NCBI_XBLAST_EXPORT
void GetSequenceLengthAndId(const IBlastSeqInfoSrc* seqinfo_src, 
                        int oid,
                        int (*rank_func)(const CRef<objects::CSeq_id>&),
                        CRef<objects::CSeq_id>& seqid, 
                        TSeqPos* length);
 
/// Get GIs for a sequence in a redundant database.
///
/// This function returns a list of GIs corresponding to the specified
/// OID.  This allows a GI list to be built for those GIs found by a
/// search and included in the associated database; the returned GIs
/// will be filtered by any OID and GI list filtering that is applied
/// to the database (if any).
///
/// @param sisrc Source of sequence information. [in]
/// @param oid OID for which to retrieve GIs.    [in]
/// @param gis GIs found for the specified oid.  [out]
NCBI_XBLAST_EXPORT
void GetFilteredRedundantGis(const IBlastSeqInfoSrc & sisrc,
                             int                      oid,
                             vector<TGi>            & gis);

/// Get Seqids for a sequence in a redundant database.
///
/// This function returns a list of GIs or Seqids corresponding to the specified
/// OID.  This allows a GI list to be built for those GIs found by a
/// search and included in the associated database; the returned GIs
/// will be filtered by any OID and GI list filtering that is applied
/// to the database (if any).
///
/// @param sisrc Source of sequence information. [in]
/// @param oid OID for which to retrieve GIs.    [in]
/// @param seqids SeqIds found for the specified oid.  [out]
/// @param use_gis Return only GIs seqids vector [in]
NCBI_XBLAST_EXPORT
void GetFilteredRedundantSeqids(const IBlastSeqInfoSrc & sisrc,
                             int                      oid,
                             vector<string>            & seqids,
			     bool		     use_gis=true);
END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___BLAST_SEQINFOSRC_AUX__HPP */
