#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file seqinfosrc_bioseq.cpp
 * Implementation of the concrete strategy for an IBlastSeqInfoSrc interface to
 * retrieve sequence identifiers and lengths from a IQueryFactory object.
 */

#include <ncbi_pch.hpp>
#include "seqinfosrc_bioseq.hpp"

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

// Defined in objmgrfree_query_data.cpp
extern CRef<CBioseq_set> x_BioseqSetFromBioseq(const CBioseq& bioseq);

CBioseqSeqInfoSrc::CBioseqSeqInfoSrc(const CBioseq& bs, bool is_prot)
    : m_DataSource(*x_BioseqSetFromBioseq(bs), is_prot)
{}

CBioseqSeqInfoSrc::CBioseqSeqInfoSrc(const CBioseq_set& bss, bool is_prot)
    : m_DataSource(bss, is_prot)
{}

list< CRef<CSeq_id> > CBioseqSeqInfoSrc::GetId(Uint4 index) const
{
    list< CRef<CSeq_id> > retval;
    CConstRef<CSeq_loc> sl(m_DataSource.GetSeqLoc(static_cast<int>(index)));
    ASSERT(sl.NotEmpty());
    CRef<CSeq_id> seqid(const_cast<CSeq_id*>(sl->GetId()));
    ASSERT(seqid.NotEmpty());
    retval.push_back(seqid);
    return retval;
}

Uint4 CBioseqSeqInfoSrc::GetLength(Uint4 index) const
{
    return static_cast<Uint4>(m_DataSource.GetLength(static_cast<int>(index)));
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
