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
* Authors:  Chris Lanczycki
*
* File Description:
*      
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <algorithm>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbitime.hpp>
#include <algo/structure/struct_util/su_block_multiple_alignment.hpp>

#include <algo/structure/bma_refine/RowSelector.hpp>

BEGIN_SCOPE(align_refine)

const unsigned int CRowSelector::INVALID_ROW = kMax_UInt;

CRowSelector::CRowSelector(unsigned int nRows, bool unique, unsigned int seed) : m_nRows(0), m_nSelections(0), m_nSelected(0), m_au(NULL), m_unique(unique), m_rng(NULL) {

    if (nRows >= 0) {
        m_nRows = nRows;
    }
    Init(m_nRows, seed);
}


CRowSelector::CRowSelector(unsigned int nRows, unsigned int nTotal, bool unique, unsigned int seed) : m_nRows(0), m_nSelections(0), m_nSelected(0), m_au(NULL), m_unique(unique), m_rng(NULL) {

    if (nRows >= 0) {
        m_nRows = nRows;
    }
    Init((nTotal > 0) ? nTotal : m_nRows, seed);
}


CRowSelector::CRowSelector(const AlignmentUtility* au, bool unique, unsigned int seed) : m_nRows(0), m_nSelections(0), m_nSelected(0), m_au(au), m_unique(unique), m_rng(NULL) {
    if (m_au && const_cast<AlignmentUtility*>(m_au)->GetBlockMultipleAlignment()) {
        m_nRows = const_cast<AlignmentUtility*>(m_au)->GetBlockMultipleAlignment()->NRows();
    }
    Init(m_nRows, seed);
}

CRowSelector::CRowSelector(const AlignmentUtility* au, unsigned int nTotal, bool unique, unsigned int seed) : m_nRows(0), m_nSelections(0), m_nSelected(0), m_au(au), m_unique(unique), m_rng(NULL) {
    if (m_au && const_cast<AlignmentUtility*>(m_au)->GetBlockMultipleAlignment()) {
        m_nRows = const_cast<AlignmentUtility*>(m_au)->GetBlockMultipleAlignment()->NRows();
    }
    Init((nTotal > 0) ? nTotal : m_nRows, seed);
}

void CRowSelector::Init(unsigned int nSelections, unsigned int seed) {
    m_nSelections = nSelections;
    m_origNSelections = nSelections;

    m_rng = (seed > 0) ? new CRandom(seed) : new CRandom(CurrentTime().GetTimeT());
        
    SetSequence();
}


bool CRowSelector::HasNext(void) {
    return (m_nSelected < m_nSelections && m_nSelections > 0);
}

unsigned int  CRowSelector::GetNext(void) {

    unsigned int result = INVALID_ROW;

    if (HasNext()) {
        result = (m_unique) ? m_sequence[m_nSelected] : INVALID_ROW;
        ++m_nSelected;
    }

    return result;
}

string CRowSelector::PrintSequence(unsigned int first, unsigned int last, bool sorted) const {
    string s = "Number of Selectable Rows = " + NStr::IntToString(m_sequence.size()) + "\n";
    vector<unsigned int> sortedSequence;
    unsigned int n = m_sequence.size();

    //  when not defaults, inputs are one-based.
    --first;
    --last;

    if (first < 0 || first >= n) first = 0;
    if (last   < 0 || last   >= n) last   = n-1;
    if (first > last) last = n;

    if (sorted) {
        sortedSequence.insert(sortedSequence.begin(), m_sequence.begin(), m_sequence.end());
        sort(sortedSequence.begin(), sortedSequence.end());
    }

    for (unsigned int i = first; i <= last; ++i) {
        s.append("  " + NStr::UIntToString((sorted) ? sortedSequence[i]+1 : m_sequence[i]+1));
        if (i > first && (i-first)%10 == 0) s.append("\n");
    }
    s.append("\n");
    return s;
}

string CRowSelector::Print(void) const {

    string s = "\nRow Selector State:\n";

    s.append("Total Selections = " + NStr::UIntToString(m_nSelections) + "\n");
    s.append("Num Selections = " + NStr::UIntToString(m_nSelected) + "\n");
    s.append("Unique = " + NStr::BoolToString(m_unique) + "\n");
    s.append(PrintSequence());
    s.append("Excluded rows:");
    for (unsigned int i = 0; i < m_excluded.size(); ++i) {
        if (i%10 == 0) s.append("\n");
        s.append("  " + NStr::UIntToString(m_excluded[i]+1));
    }
    s.append("\n");
    return s;
}

void CRowSelector::Reset() {
    m_nSelected = 0;
}

void CRowSelector::Shuffle(bool clearExcludedRows) {
    m_nSelected = 0;
    if (clearExcludedRows) ClearExclusions();
    SetSequence();
}

unsigned int CRowSelector::GetNumRows(void) const {
    return m_nRows;
}

unsigned int CRowSelector::GetNumSelected(void) const {
    return m_nSelected;
}

unsigned int CRowSelector::GetSequenceSize(void) const {
    return m_sequence.size();
}

void CRowSelector::ExcludeStructureRows() {

    if (m_au) {
        
    }
}

bool CRowSelector::ExcludeRow(unsigned int row) {

    bool isExcluded = false;
    unsigned int nTimes = 0;

    //cout << "Excluding row " << row+1 << " in sequence." << endl;
    //  'row' must be in range, in m_sequence and not already excluded.
    if (row < m_nRows && find(m_excluded.begin(), m_excluded.end(), row) == m_excluded.end()) {
        ITERATE (vector<unsigned int>, it, m_sequence) {
            if (*it == row) {
                ++nTimes;
            }
        }
        if (nTimes > 0) {
            //  If the row was previously selected, decrease # selected as it will subsequently
            //  be removed from the actual sequence.
            for (unsigned int i = 0; i < m_nSelected; ++i) {
                if (m_sequence[i] == row) --m_nSelected;
            }
            m_nSelections -= nTimes;
            m_sequence.erase(remove(m_sequence.begin(), m_sequence.end(), row), m_sequence.end());
        }
        m_excluded.push_back(row);
        isExcluded = true;
    }
    return isExcluded;
    //cout << "excluded row " << row << " in sequence:  " << PrintSequence();
}

void CRowSelector::ClearExclusions(void) {
    m_excluded.clear();
    m_nSelections = m_origNSelections;
}

unsigned int CRowSelector::GetNumExcluded(void) const {
    return m_excluded.size();
}

void CRowSelector::SetSequence(void) {

    vector<unsigned int> allowedValues(m_nRows);
    unsigned int randomSelection, nExcluded = m_excluded.size();
    unsigned int i;
    int ran, tmp;
    int maxIndex = m_nRows-1;
    bool isExcluded;

    m_sequence.clear();

    //  When maximum integer exceeds number of selections requested, the possibility exists 
    //  to use each integer once.  Otherwise, simply draw m_nSelections integers on [0, m_nRows)
    if (m_nSelections <= m_nRows) {
        for (i = 0; i < m_nRows; ++i) allowedValues[i] = i;

        i = 0;
        while (i < m_nSelections) {
            ran = m_rng->GetRand(0, maxIndex);
            randomSelection = allowedValues[ran];
            isExcluded = (nExcluded > 0 && find(m_excluded.begin(), m_excluded.end(), randomSelection) != m_excluded.end());

            if (!isExcluded) {
                m_sequence.push_back(randomSelection);
                ++i;
            }

            if (m_unique || isExcluded) {
                //  Swap the used/excluded integer w/ the last unused element of the allowed 
                //  values list.  If it's the last unused index already, don't bother.
                if (ran < maxIndex) {
                    tmp = allowedValues[maxIndex];
                    allowedValues[maxIndex] = ran;
                    allowedValues[ran] = tmp;
                }
                --maxIndex;
            }
        }
            
    } else {
        i = 0;
        while (i < m_nSelections) {
            randomSelection = (unsigned)m_rng->GetRand(0, m_nRows-1);
            if (nExcluded == 0 || find(m_excluded.begin(), m_excluded.end(), randomSelection) == m_excluded.end()) {
                m_sequence.push_back(randomSelection);
                ++i;
            }
        }
    }
}

END_SCOPE(align_refine)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2005/06/29 01:32:33  ucko
* Rework ExcludeRow to avoid count(), as WorkShop's STL implementation
* doesn't support the standard syntax. :-/
*
* Revision 1.1  2005/06/28 13:44:23  lanczyck
* block multiple alignment refiner code from internal/structure/align_refine
*
* Revision 1.9  2004/11/02 23:52:08  lanczyck
* remove CCdCore in favor of CCdd & AlignmentUtility anticipating deployment w/i cn3d
*
* Revision 1.8  2004/10/22 16:56:42  lanczyck
* add block score analysis; modify output formatting and default ERR_POST flags
*
* Revision 1.7  2004/10/12 20:06:10  lanczyck
* minor change to output text
*
* Revision 1.6  2004/10/04 19:11:44  lanczyck
* add shift analysis; switch to use of cd_utils vs my own class; rename output CDs w/ trial number included; default T = 3e6
*
* Revision 1.5  2004/07/27 16:53:35  lanczyck
* give RowSelector a CRandom member; change seed parameter handling in main app
*
* Revision 1.3  2004/07/20 18:56:32  lanczyck
* change CDUtils api to use const refernce vs pointer
*
* Revision 1.2  2004/06/22 19:53:01  lanczyck
* fix message reporting; add a couple cmd options
*
* Revision 1.1  2004/06/21 20:09:02  lanczyck
* Initial version w/ LOO & scoring vs. PSSM
*
*
*/


