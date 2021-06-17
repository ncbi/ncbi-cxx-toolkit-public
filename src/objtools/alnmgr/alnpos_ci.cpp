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
*   Alignment position iterator. Uses CAlnMap.
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnpos_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnPos_CI::CAlnPos_CI(const CAlnMap& alnmap,
                       TSeqPos aln_pos)
    : m_AlnMap(alnmap),
      m_AlnPos(aln_pos),
      m_Valid(true),
      m_Anchor(alnmap.GetAnchor())
{
    // set iteration limits for the aln pos
    m_AlnStart = m_AlnMap.GetAlnStart();
    m_AlnStop  = m_AlnMap.GetAlnStop();
    _ASSERT(m_AlnStart < m_AlnStop);

    // adjust m_AlnPos in case it is out of range
    if (m_AlnPos < m_AlnStart) {
        m_AlnPos = m_AlnStart;
    } else if (m_AlnPos > m_AlnStop) {
        m_AlnPos = m_AlnStop;
    }

    // set m_AlnSeg, m_RDelta, m_LDelta
    m_AlnSeg = m_AlnMap.GetSeg(m_AlnPos);
    m_LDelta = aln_pos - m_AlnMap.GetAlnStart(m_AlnSeg);
    m_RDelta = m_AlnMap.GetAlnStop(m_AlnSeg) - aln_pos;

    // resize & initialize cache
    m_SeqStartsCache.resize(m_AlnMap.GetNumRows(), -2);
};


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
