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
*   Various alignment viewers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnvwr.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);


void CAlnVwr::CsvTable(const CAlnMap& alnmap,
                       CNcbiOstream& out)
{
    out << ",";
    for (int seg = 0; seg < alnmap.GetNumSegs(); seg++) {
        out << "," << alnmap.GetLen(seg) << ",";
    }
    out << endl;
    for (int row = 0; row < alnmap.GetNumRows(); row++) {
        out << row << ",";
        for (int seg = 0; seg < alnmap.GetNumSegs(); seg++) {
            out << alnmap.GetStart(row, seg) << "," 
                 << alnmap.GetStop(row, seg) << ",";
        }
        out << endl;
    }
}


void CAlnVwr::Segments(const CAlnMap& alnmap,
                       CNcbiOstream& out)
{
    CAlnMap::TNumrow row;

    for (row=0; row<alnmap.GetNumRows(); row++) {
        out << "Row: " << row << endl;
        for (int seg=0; seg<alnmap.GetNumSegs(); seg++) {
            
            // seg
            out << "\t" << seg << ": ";

            // aln coords
            out << alnmap.GetAlnStart(seg) << "-"
                 << alnmap.GetAlnStop(seg) << " ";


            // type
            CAlnMap::TSegTypeFlags type = alnmap.GetSegType(row, seg);
            if (type & CAlnMap::fSeq) {
                // seq coords
                out << alnmap.GetStart(row, seg) << "-" 
                     << alnmap.GetStop(row, seg) << " (Seq)";
            } else {
                out << "(Gap)";
            }

            if (type & CAlnMap::fNotAlignedToSeqOnAnchor) out << "(NotAlignedToSeqOnAnchor)";
            if (CAlnMap::IsTypeInsert(type)) out << "(Insert)";
            if (type & CAlnMap::fUnalignedOnRight) out << "(UnalignedOnRight)";
            if (type & CAlnMap::fUnalignedOnLeft) out << "(UnalignedOnLeft)";
            if (type & CAlnMap::fNoSeqOnRight) out << "(NoSeqOnRight)";
            if (type & CAlnMap::fNoSeqOnLeft) out << "(NoSeqOnLeft)";
            if (type & CAlnMap::fEndOnRight) out << "(EndOnRight)";
            if (type & CAlnMap::fEndOnLeft) out << "(EndOnLeft)";

            out << NcbiEndl;
        }
    }
}


void CAlnVwr::Chunks(const CAlnMap& alnmap,
                     CAlnMap::TGetChunkFlags flags,
                     CNcbiOstream& out)
{
    CAlnMap::TNumrow row;

    CAlnMap::TSignedRange range(-1, alnmap.GetAlnStop()+1);

    for (row=0; row<alnmap.GetNumRows(); row++) {
        out << "Row: " << row << endl;
        //CAlnMap::TSignedRange range(alnmap.GetSeqStart(row) -1,
        //alnmap.GetSeqStop(row) + 1);
        CRef<CAlnMap::CAlnChunkVec> chunk_vec = alnmap.GetAlnChunks(row, range, flags);
    
        for (int i=0; i<chunk_vec->size(); i++) {
            CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];

            out << "[row" << row << "|" << i << "]";
            out << chunk->GetAlnRange().GetFrom() << "-"
                 << chunk->GetAlnRange().GetTo() << " ";

            if (!chunk->IsGap()) {
                out << chunk->GetRange().GetFrom() << "-"
                    << chunk->GetRange().GetTo();
            } else {
                out << "(Gap)";
            }

            if (chunk->GetType() & CAlnMap::fSeq) out << "(Seq)";
            if (chunk->GetType() & CAlnMap::fNotAlignedToSeqOnAnchor) out << "(NotAlignedToSeqOnAnchor)";
            if (CAlnMap::IsTypeInsert(chunk->GetType())) out << "(Insert)";
            if (chunk->GetType() & CAlnMap::fUnalignedOnRight) out << "(UnalignedOnRight)";
            if (chunk->GetType() & CAlnMap::fUnalignedOnLeft) out << "(UnalignedOnLeft)";
            if (chunk->GetType() & CAlnMap::fNoSeqOnRight) out << "(NoSeqOnRight)";
            if (chunk->GetType() & CAlnMap::fNoSeqOnLeft) out << "(NoSeqOnLeft)";
            if (chunk->GetType() & CAlnMap::fEndOnRight) out << "(EndOnRight)";
            if (chunk->GetType() & CAlnMap::fEndOnLeft) out << "(EndOnLeft)";
            out << NcbiEndl;
        }
    }
}


void CAlnVwr::PopsetStyle(const CAlnVec& alnvec,
                          int scrn_width,
                          EAlgorithm algorithm,
                          CNcbiOstream& out)
{
    switch(algorithm) {
    case eUseSegments:
        {
            int aln_pos = 0;
            CAlnMap::TSignedRange rng;
            
            do {
                // create range
                rng.Set(aln_pos, aln_pos + scrn_width - 1);
                
                string aln_seq_str;
                aln_seq_str.reserve(scrn_width + 1);
                // for each sequence
                for (CAlnMap::TNumrow row = 0; row < alnvec.GetNumRows(); row++) {
                    out << alnvec.GetSeqId(row).AsFastaString()
                        << "\t" 
                        << alnvec.GetSeqPosFromAlnPos(row, rng.GetFrom(),
                                                      CAlnMap::eLeft)
                        << "\t"
                        << alnvec.GetAlnSeqString(aln_seq_str, row, rng)
                        << "\t"
                        << alnvec.GetSeqPosFromAlnPos(row, rng.GetTo(),
                                                      CAlnMap::eLeft)
                        << endl;
                }
                out << endl;
                aln_pos += scrn_width;
            } while (aln_pos < alnvec.GetAlnStop());
            break;
        }
    case eUseChunks:
        {
            TSeqPos aln_len = alnvec.GetAlnStop() + 1;
            const CAlnMap::TNumrow nrows = alnvec.GetNumRows();
            const CAlnMap::TNumseg nsegs = alnvec.GetNumSegs();
            const CDense_seg::TStarts& starts = alnvec.GetDenseg().GetStarts();
            const CDense_seg::TLens& lens = alnvec.GetDenseg().GetLens();
            
            vector<string> buffer(nrows);
            for (CAlnMap::TNumrow row = 0; row < nrows; row++) {
            
                // allocate space for the row
                buffer[row].reserve(aln_len + 1);
                string buff;
                
                int seg, pos, left_seg = -1, right_seg = -1;
                TSignedSeqPos start;
                TSeqPos len;
            
                // determine the ending right seg
                for (seg = nsegs - 1, pos = seg * nrows + row;
                     seg >= 0; --seg, pos -= nrows) {
                    if (starts[pos] >= 0) {
                        right_seg = seg;
                        break;
                    }
                }
            
                for (seg = 0, pos = row;  seg < nsegs; ++seg, pos += nrows) {
                    len = lens[seg];
                    if ((start = starts[pos]) >= 0) {
                    
                        left_seg = seg; // ending left seg is at most here
                    
                        alnvec.GetSeqString(buff, row, start, start + len - 1);
                        buffer[row] += buff;
                    } else {
                        // add appropriate number of gap/end chars
                        char* ch_buff = new char[len+1];
                        char fill_ch;
                        if (left_seg < 0  ||  seg > right_seg  &&  right_seg > 0) {
                            fill_ch = alnvec.GetEndChar();
                        } else {
                            fill_ch = alnvec.GetGapChar(row);
                        }
                        memset(ch_buff, fill_ch, len);
                        ch_buff[len] = 0;
                        buffer[row] += ch_buff;
                        delete[] ch_buff;
                    }
                }
            }
        
            TSeqPos pos = 0;
            do {
                for (CAlnMap::TNumrow row = 0; row < nrows; row++) {
                    out << alnvec.GetSeqId(row).AsFastaString()
                        << "\t"
                        << alnvec.GetSeqPosFromAlnPos(row, pos, CAlnMap::eLeft)
                        << "\t"
                        << buffer[row].substr(pos, scrn_width)
                        << "\t"
                        << alnvec.GetSeqPosFromAlnPos(row, pos + scrn_width - 1,
                                                      CAlnMap::eLeft)
                        << endl;
                }
                out << endl;
                pos += scrn_width;
                if (pos + scrn_width > aln_len) {
                    scrn_width = aln_len - pos;
                }
            } while (pos < aln_len);
            break;
        }
    case eSpeedOptimized:
        {
            CAlnMap::TNumrow row, nrows = alnvec.GetNumRows();

            vector<string> buffer(nrows);
            vector<CAlnMap::TSeqPosList> insert_aln_starts(nrows);
            vector<CAlnMap::TSeqPosList> insert_starts(nrows);
            vector<CAlnMap::TSeqPosList> insert_lens(nrows);
            vector<CAlnMap::TSeqPosList> scrn_lefts(nrows);
            vector<CAlnMap::TSeqPosList> scrn_rights(nrows);
        
            // Fill in the vectors for each row
            for (row = 0; row < nrows; row++) {
                alnvec.GetWholeAlnSeqString
                    (row,
                     buffer[row],
                     &insert_aln_starts[row],
                     &insert_starts[row],
                     &insert_lens[row],
                     scrn_width,
                     &scrn_lefts[row],
                     &scrn_rights[row]);
            }
        
            // Visualization
            TSeqPos pos = 0, aln_len = alnvec.GetAlnStop() + 1;
            do {
                for (row = 0; row < nrows; row++) {
                    out << row 
                        << "\t"
                        << alnvec.GetSeqId(row).AsFastaString()
                        << "\t" 
                        << scrn_lefts[row].front()
                        << "\t"
                        << buffer[row].substr(pos, scrn_width)
                        << "\t"
                        << scrn_rights[row].front()
                        << endl;
                    scrn_lefts[row].pop_front();
                    scrn_rights[row].pop_front();
                }
                out << endl;
                pos += scrn_width;
                if (pos + scrn_width > aln_len) {
                    scrn_width = aln_len - pos;
                }
            } while (pos < aln_len);

            break;
        }
    }
}


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2004/08/27 20:55:00  todorov
 * initial revision
 *
 *
 * ===========================================================================
 */
