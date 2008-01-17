#ifndef OBJTOOLS_ALNMGR___ALN_SERIAL__HPP
#define OBJTOOLS_ALNMGR___ALN_SERIAL__HPP
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignments Serialization
*
* ===========================================================================
*/


#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_explorer.hpp>
#include <objtools/alnmgr/aln_stats.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CPairwiseAln::TRng& rng);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const IAlnSegment::ESegTypeFlags& flags);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const IAlnSegment& aln_seg);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CPairwiseAln::TAlnRng& aln_rng);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CPairwiseAln::EFlags& flags);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const TAlnSeqIdIRef& aln_seq_id_iref);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CPairwiseAln& pairwise_aln);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const CAnchoredAln& anchored_aln);

NCBI_XALNMGR_EXPORT
ostream& operator<<(ostream& out, const TAlnStats& aln_stats);


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_SERIAL__HPP
