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
*   CAlnMap printer.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objects/seqloc/Seq_id.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);


CAlnMapPrinter::CAlnMapPrinter(const CAlnMap& aln_map,
                               CNcbiOstream&  out)
    : m_AlnMap(aln_map),
      m_IdFieldLen(28),
      m_RowFieldLen(0),
      m_NumRows(m_NumRows),
      m_Out(&out)
{
    m_Ids.resize(m_NumRows);
    for (CAlnMap::TNumrow row = 0; row < m_NumRows; row++) {
        m_Ids[row] = m_AlnMap.GetSeqId(row).AsFastaString();
        if (m_Ids[row].length() > m_IdFieldLen) {
            m_IdFieldLen = m_Ids[row].length();
        }
    }
    m_IdFieldLen += 2;
    m_RowFieldLen = NStr::IntToString(m_NumRows).length() + 2;
}


void
CAlnMapPrinter::PrintId(CAlnMap::TNumrow row) const
{
    m_Out->width(m_IdFieldLen);
    m_Out->setf(ios_base::left, ios_base::adjustfield);
    *m_Out << GetId(row);
}


void
CAlnMapPrinter::PrintRow(CAlnMap::TNumrow row) const
{
    _ASSERT(row <= m_NumRows);
    m_Out->width(m_RowFieldLen);
    m_Out->setf(ios_base::right, ios_base::adjustfield);
    *m_Out << row;
}


void CAlnMapPrinter::CsvTable()
{
    *m_Out << ",";
    for (int seg = 0; seg < m_AlnMap.GetNumSegs(); seg++) {
        *m_Out << "," << m_AlnMap.GetLen(seg) << ",";
    }
    *m_Out << endl;
    for (int row = 0; row < m_NumRows; row++) {
        *m_Out << row << ",";
        for (int seg = 0; seg < m_AlnMap.GetNumSegs(); seg++) {
            *m_Out << m_AlnMap.GetStart(row, seg) << "," 
                 << m_AlnMap.GetStop(row, seg) << ",";
        }
        *m_Out << endl;
    }
}


void CAlnMapPrinter::Segments()
{
    CAlnMap::TNumrow row;

    for (row=0; row<m_NumRows; row++) {
        *m_Out << "Row: " << row << endl;
        for (int seg=0; seg<m_AlnMap.GetNumSegs(); seg++) {
            
            // seg
            *m_Out << "\t" << seg << ": ";

            // aln coords
            *m_Out << m_AlnMap.GetAlnStart(seg) << "-"
                 << m_AlnMap.GetAlnStop(seg) << " ";


            // type
            CAlnMap::TSegTypeFlags type = m_AlnMap.GetSegType(row, seg);
            if (type & CAlnMap::fSeq) {
                // seq coords
                *m_Out << m_AlnMap.GetStart(row, seg) << "-" 
                     << m_AlnMap.GetStop(row, seg) << " (Seq)";
            } else {
                *m_Out << "(Gap)";
            }

            if (type & CAlnMap::fNotAlignedToSeqOnAnchor) *m_Out << "(NotAlignedToSeqOnAnchor)";
            if (CAlnMap::IsTypeInsert(type)) *m_Out << "(Insert)";
            if (type & CAlnMap::fUnalignedOnRight) *m_Out << "(UnalignedOnRight)";
            if (type & CAlnMap::fUnalignedOnLeft) *m_Out << "(UnalignedOnLeft)";
            if (type & CAlnMap::fNoSeqOnRight) *m_Out << "(NoSeqOnRight)";
            if (type & CAlnMap::fNoSeqOnLeft) *m_Out << "(NoSeqOnLeft)";
            if (type & CAlnMap::fEndOnRight) *m_Out << "(EndOnRight)";
            if (type & CAlnMap::fEndOnLeft) *m_Out << "(EndOnLeft)";

            *m_Out << NcbiEndl;
        }
    }
}


void CAlnMapPrinter::Chunks(CAlnMap::TGetChunkFlags flags)
{
    CAlnMap::TNumrow row;

    CAlnMap::TSignedRange range(-1, m_AlnMap.GetAlnStop()+1);

    for (row=0; row<m_NumRows; row++) {
        *m_Out << "Row: " << row << endl;
        //CAlnMap::TSignedRange range(m_AlnMap.GetSeqStart(row) -1,
        //m_AlnMap.GetSeqStop(row) + 1);
        CRef<CAlnMap::CAlnChunkVec> chunk_vec = m_AlnMap.GetAlnChunks(row, range, flags);
    
        for (int i=0; i<chunk_vec->size(); i++) {
            CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];

            *m_Out << "[row" << row << "|" << i << "]";
            *m_Out << chunk->GetAlnRange().GetFrom() << "-"
                 << chunk->GetAlnRange().GetTo() << " ";

            if (!chunk->IsGap()) {
                *m_Out << chunk->GetRange().GetFrom() << "-"
                    << chunk->GetRange().GetTo();
            } else {
                *m_Out << "(Gap)";
            }

            if (chunk->GetType() & CAlnMap::fSeq) *m_Out << "(Seq)";
            if (chunk->GetType() & CAlnMap::fNotAlignedToSeqOnAnchor) *m_Out << "(NotAlignedToSeqOnAnchor)";
            if (CAlnMap::IsTypeInsert(chunk->GetType())) *m_Out << "(Insert)";
            if (chunk->GetType() & CAlnMap::fUnalignedOnRight) *m_Out << "(UnalignedOnRight)";
            if (chunk->GetType() & CAlnMap::fUnalignedOnLeft) *m_Out << "(UnalignedOnLeft)";
            if (chunk->GetType() & CAlnMap::fNoSeqOnRight) *m_Out << "(NoSeqOnRight)";
            if (chunk->GetType() & CAlnMap::fNoSeqOnLeft) *m_Out << "(NoSeqOnLeft)";
            if (chunk->GetType() & CAlnMap::fEndOnRight) *m_Out << "(EndOnRight)";
            if (chunk->GetType() & CAlnMap::fEndOnLeft) *m_Out << "(EndOnLeft)";
            if (chunk->GetType() & CAlnMap::fUnaligned) *m_Out << "(Unaligned)";
            *m_Out << NcbiEndl;
        }
    }
}



/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2005/03/15 19:17:16  todorov
 * + PrintRow
 *
 * Revision 1.1  2005/03/15 17:43:36  todorov
 * Initial revision
 *
 * ===========================================================================
 */
