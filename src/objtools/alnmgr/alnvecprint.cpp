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
*   CAlnVec printer.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnvec.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);


CAlnVecPrinter::CAlnVecPrinter(const CAlnVec& aln_vec,
                               CNcbiOstream&  out)
    : CAlnMapPrinter(aln_vec, out),
      m_AlnVec(aln_vec)
{
}


void
CAlnVecPrinter::x_SetChars()
{
    CAlnVec& aln_vec = const_cast<CAlnVec&>(m_AlnVec);

    m_OrigSetGapChar = aln_vec.IsSetGapChar();
    if (m_OrigSetGapChar) {
        m_OrigGapChar = aln_vec.GetGapChar(0);
    }
    aln_vec.SetGapChar('-');

    m_OrigSetEndChar = aln_vec.IsSetEndChar();
    if (m_OrigSetEndChar) {
        m_OrigEndChar = aln_vec.GetEndChar();
    }
    aln_vec.SetEndChar('-');
}


void
CAlnVecPrinter::x_UnsetChars()
{
    CAlnVec& aln_vec = const_cast<CAlnVec&>(m_AlnVec);

    if (m_OrigSetGapChar) {
        aln_vec.SetGapChar(m_OrigGapChar);
    } else {
        aln_vec.UnsetGapChar();
    }

    if (m_OrigSetEndChar) {
        aln_vec.SetEndChar(m_OrigEndChar);
    } else {
        aln_vec.UnsetEndChar();
    }
}


void CAlnVecPrinter::PopsetStyle(int scrn_width,
                                 EAlgorithm algorithm)
{
    x_SetChars();

    switch(algorithm) {
    case eUseSeqString:
        {
            TSeqPos aln_len = m_AlnVec.GetAlnStop() + 1;
            const CAlnMap::TNumrow nrows = m_NumRows;
            const CAlnMap::TNumseg nsegs = m_AlnVec.GetNumSegs();
            const CDense_seg::TStarts& starts = m_AlnVec.GetDenseg().GetStarts();
            const CDense_seg::TLens& lens = m_AlnVec.GetDenseg().GetLens();
            
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
                    
                        m_AlnVec.GetSeqString(buff,
                                              row,
                                              start,
                                              start + len * m_AlnVec.GetWidth(row) - 1);
                        buffer[row] += buff;
                    } else {
                        // add appropriate number of gap/end chars
                        char* ch_buff = new char[len+1];
                        char fill_ch;
                        if (left_seg < 0  ||  seg > right_seg  &&  right_seg > 0) {
                            fill_ch = m_AlnVec.GetEndChar();
                        } else {
                            fill_ch = m_AlnVec.GetGapChar(row);
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
                    PrintNumRow(row);
                    PrintId(row);
                    PrintSeqPos(m_AlnVec.GetSeqPosFromAlnPos(row, pos, CAlnMap::eLeft));
                    *m_Out << buffer[row].substr(pos, scrn_width)
                           << "  "
                           << m_AlnVec.GetSeqPosFromAlnPos(row, pos + scrn_width - 1,
                                                           CAlnMap::eLeft)
                           << endl;
                }
                *m_Out << endl;
                pos += scrn_width;
                if (pos + scrn_width > aln_len) {
                    scrn_width = aln_len - pos;
                }
            } while (pos < aln_len);
            break;
        }
    case eUseAlnSeqString:
        {
            TSeqPos aln_pos = 0;
            CAlnMap::TSignedRange rng;
            
            do {
                // create range
                rng.Set(aln_pos, aln_pos + scrn_width - 1);
                
                string aln_seq_str;
                aln_seq_str.reserve(scrn_width + 1);
                // for each sequence
                for (CAlnMap::TNumrow row = 0; row < m_NumRows; row++) {
                    PrintNumRow(row);
                    PrintId(row);
                    PrintSeqPos(m_AlnVec.GetSeqPosFromAlnPos(row, rng.GetFrom(),
                                                             CAlnMap::eLeft));
                    *m_Out << m_AlnVec.GetAlnSeqString(aln_seq_str, row, rng)
                           << " "
                           << m_AlnVec.GetSeqPosFromAlnPos(row, rng.GetTo(),
                                                           CAlnMap::eLeft)
                           << endl;
                }
                *m_Out << endl;
                aln_pos += scrn_width;
            } while (aln_pos < m_AlnVec.GetAlnStop());
            break;
        }
    case eUseWholeAlnSeqString:
        {
            CAlnMap::TNumrow row, nrows = m_NumRows;

            vector<string> buffer(nrows);
            vector<CAlnMap::TSeqPosList> insert_aln_starts(nrows);
            vector<CAlnMap::TSeqPosList> insert_starts(nrows);
            vector<CAlnMap::TSeqPosList> insert_lens(nrows);
            vector<CAlnMap::TSeqPosList> scrn_lefts(nrows);
            vector<CAlnMap::TSeqPosList> scrn_rights(nrows);
        
            // Fill in the vectors for each row
            for (row = 0; row < nrows; row++) {
                m_AlnVec.GetWholeAlnSeqString
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
            TSeqPos pos = 0, aln_len = m_AlnVec.GetAlnStop() + 1;
            do {
                for (row = 0; row < nrows; row++) {
                    PrintNumRow(row);
                    PrintId(row);
                    PrintSeqPos(scrn_lefts[row].front());
                    *m_Out << buffer[row].substr(pos, scrn_width)
                           << " "
                           << scrn_rights[row].front()
                           << endl;
                    scrn_lefts[row].pop_front();
                    scrn_rights[row].pop_front();
                }
                *m_Out << endl;
                pos += scrn_width;
                if (pos + scrn_width > aln_len) {
                    scrn_width = aln_len - pos;
                }
            } while (pos < aln_len);

            break;
        }
    }
    x_UnsetChars();
}


void CAlnVecPrinter::ClustalStyle(int scrn_width,
                                  EAlgorithm algorithm)
{
    x_SetChars();

    *m_Out << "CLUSTAL W (1.83) multiple sequence alignment" << endl << endl;

    switch(algorithm) {
    case eUseSeqString:
        {
            TSeqPos aln_len = m_AlnVec.GetAlnStop() + 1;
            const CAlnMap::TNumseg nsegs = m_AlnVec.GetNumSegs();
            const CDense_seg::TStarts& starts = m_AlnVec.GetDenseg().GetStarts();
            const CDense_seg::TLens& lens = m_AlnVec.GetDenseg().GetLens();
            CAlnMap::TNumrow row;
            
            vector<string> buffer(m_NumRows+1);
            for (row = 0; row < m_NumRows; row++) {
            
                // allocate space for the row
                buffer[row].reserve(aln_len + 1);
                string buff;
                
                int seg, pos, left_seg = -1, right_seg = -1;
                TSignedSeqPos start;
                TSeqPos len;
            
                // determine the ending right seg
                for (seg = nsegs - 1, pos = seg * m_NumRows + row;
                     seg >= 0; --seg, pos -= m_NumRows) {
                    if (starts[pos] >= 0) {
                        right_seg = seg;
                        break;
                    }
                }
            
                for (seg = 0, pos = row;  seg < nsegs; ++seg, pos += m_NumRows) {
                    len = lens[seg];
                    if ((start = starts[pos]) >= 0) {
                    
                        left_seg = seg; // ending left seg is at most here
                    
                        m_AlnVec.GetSeqString(buff,
                                              row,
                                              start,
                                              start + len * m_AlnVec.GetWidth(row) - 1);
                        buffer[row] += buff;
                    } else {
                        // add appropriate number of gap/end chars
                        char* ch_buff = new char[len+1];
                        char fill_ch;
                        if (left_seg < 0  ||  seg > right_seg  &&  right_seg > 0) {
                            fill_ch = m_AlnVec.GetEndChar();
                        } else {
                            fill_ch = m_AlnVec.GetGapChar(row);
                        }
                        memset(ch_buff, fill_ch, len);
                        ch_buff[len] = 0;
                        buffer[row] += ch_buff;
                        delete[] ch_buff;
                    }
                }
            }
            // Find identities
            buffer[m_NumRows].resize(aln_len);
            for (TSeqPos pos = 0; pos < aln_len; pos++) {
                bool identity = true;
                char residue = buffer[0][pos];
                for (row = 1; row < m_NumRows; row++) {
                    if (buffer[row][pos] != residue) {
                        identity = false;
                        break;
                    }
                }
                buffer[m_NumRows][pos] = (identity ? '*' : ' ');
            }

        
            TSeqPos aln_pos = 0;
            do {
                for (CAlnMap::TNumrow row = 0; row < m_NumRows; row++) {
                    PrintId(row);
                    *m_Out << buffer[row].substr(aln_pos, scrn_width)
                           << endl;
                }
                m_Out->width(m_IdFieldLen);
                *m_Out << "";
                *m_Out << buffer[m_NumRows].substr(aln_pos, scrn_width)
                       << endl << endl;

                aln_pos += scrn_width;
                if (aln_pos + scrn_width > aln_len) {
                    scrn_width = aln_len - aln_pos;
                }
            } while (aln_pos < aln_len);
            break;
        }
    case eUseAlnSeqString:
        {
            TSeqPos aln_pos = 0;
            TSeqPos aln_stop = m_AlnVec.GetAlnStop();
            CAlnMap::TSignedRange rng;
            
            string identities_str;
            identities_str.reserve(scrn_width + 1);

            do {
                // create range
                rng.Set(aln_pos, min(aln_pos + scrn_width - 1, aln_stop));
                
                string aln_seq_str;
                aln_seq_str.reserve(scrn_width + 1);

                // for each sequence
                for (CAlnMap::TNumrow row = 0; row < m_NumRows; row++) {
                    PrintId(row);
                    *m_Out << m_AlnVec.GetAlnSeqString(aln_seq_str, row, rng)
                           << endl;

                    if (row == 0) {
                        identities_str = aln_seq_str;
                    } else {
                        for (size_t i = 0; i < aln_seq_str.length(); i++) {
                            if (aln_seq_str[i] != identities_str[i]) {
                                identities_str[i] = ' ';
                            }
                        }
                    }
                }
                for (size_t i = 0; i < identities_str.length(); i++) {
                    if (identities_str[i] != ' ') {
                        identities_str[i] = '*';
                    }
                }
                m_Out->width(m_IdFieldLen);
                *m_Out << "";
                *m_Out << identities_str
                       << endl << endl;
                aln_pos += scrn_width;
            } while (aln_pos < m_AlnVec.GetAlnStop());
            break;
        }
    case eUseWholeAlnSeqString:
        {
            CAlnMap::TNumrow row;

            vector<string> buffer(m_NumRows+1);
        
            // Fill in the vectors for each row
            for (row = 0; row < m_NumRows; row++) {
                m_AlnVec.GetWholeAlnSeqString(row, buffer[row]);
            }
        
            TSeqPos pos = 0;
            const TSeqPos aln_len = m_AlnVec.GetAlnStop() + 1;
            
            // Find identities
            buffer[m_NumRows].resize(aln_len);
            for (pos = 0; pos < aln_len; pos++) {
                bool identity = true;
                char residue = buffer[0][pos];
                for (row = 1; row < m_NumRows; row++) {
                    if (buffer[row][pos] != residue) {
                        identity = false;
                        break;
                    }
                }
                buffer[m_NumRows][pos] = (identity ? '*' : ' ');
            }


            // Visualization
            pos = 0;
            do {
                for (row = 0; row < m_NumRows; row++) {
                    PrintId(row);
                    *m_Out << buffer[row].substr(pos, scrn_width)
                        << endl;
                }
                m_Out->width(m_IdFieldLen);
                *m_Out << "";
                *m_Out << buffer[m_NumRows].substr(pos, scrn_width)
                       << endl << endl;
                
                pos += scrn_width;
                if (pos + scrn_width > aln_len) {
                    scrn_width = aln_len - pos;
                }
            } while (pos < aln_len);

            break;
        }
    }
    x_UnsetChars();
}
