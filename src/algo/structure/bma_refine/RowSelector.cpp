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

CRowSelector::CRowSelector(unsigned int nRows, bool unique) : m_unique(unique), m_nRows(0), m_nSelections(0), m_nSelected(0) {

    Init(nRows, nRows);
}


CRowSelector::CRowSelector(unsigned int nRows, unsigned int nTotal, bool unique) : m_unique(unique), m_nRows(0), m_nSelections(0), m_nSelected(0) {

    Init(nRows, (nTotal > 0) ? nTotal : nRows);
}


void CRowSelector::Init(unsigned int nRows, unsigned int nSelections) {
    m_nRows = nRows;
    m_nSelections = nSelections;
    m_origNSelections = nSelections;
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
    string s = "Number of Selectable Rows = " + NStr::SizetToString(m_sequence.size()) + "\n";
    vector<unsigned int> sortedSequence;
    unsigned int n = m_sequence.size();

    //  when not defaults, inputs are one-based.
    --first;
    --last;

    if (first >= n) first  = 0;
    if (last  >= n) last   = n-1;
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

unsigned int CRowSelector::GetNumRows(void) const {
    return m_nRows;
}

unsigned int CRowSelector::GetNumSelected(void) const {
    return m_nSelected;
}

unsigned int CRowSelector::GetSequenceSize(void) const {
    return m_sequence.size();
}

bool CRowSelector::ExcludeRow(unsigned int row) {

    bool isExcluded = false;
    unsigned int nTimes = 0;

    //cout << "Excluding row " << row+1 << " in sequence; initial sequence:  "  << Print() << endl;
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
    //cout << "excluded row " << row << " in sequence:  " << Print();
    return isExcluded;
}

void CRowSelector::ClearExclusions(void) {
    m_excluded.clear();
    m_nSelections = m_origNSelections;
}

unsigned int CRowSelector::GetNumExcluded(void) const {
    return m_excluded.size();
}


//  ==================================================
//  Random-order row selector
//  ==================================================

CRandomRowSelector::CRandomRowSelector(unsigned int nRows, bool unique, unsigned int seed) : CRowSelector(nRows, unique), m_rng(NULL) {

    InitRNG(seed);
}


CRandomRowSelector::CRandomRowSelector(unsigned int nRows, unsigned int nTotal, bool unique, unsigned int seed) : CRowSelector(nRows, nTotal, unique), m_rng(NULL) {

    InitRNG(seed);
}

void CRandomRowSelector::InitRNG(unsigned int seed) {

    m_rng = (seed > 0) ? new CRandom(seed) : new CRandom(CurrentTime().GetTimeT());
    SetSequence();
}


void CRandomRowSelector::Shuffle(bool clearExcludedRows) {
    m_nSelected = 0;
    if (clearExcludedRows) ClearExclusions();
    SetSequence();
}


void CRandomRowSelector::SetSequence(void) {

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



//  ==================================================
//  Alignment-based row selector
//  ==================================================

CAlignmentBasedRowSelector::CAlignmentBasedRowSelector(const AlignmentUtility* au, bool unique, bool bestToWorst) : CRowSelector(0, unique), m_au(NULL), m_sortBestToWorst(bestToWorst) {

    InitAU(au, 0);
}

CAlignmentBasedRowSelector::CAlignmentBasedRowSelector(const AlignmentUtility* au, unsigned int nTotal, bool unique, bool bestToWorst) : CRowSelector(0, nTotal, unique), m_au(NULL), m_sortBestToWorst(bestToWorst) {

    InitAU(au, (nTotal > 0) ? nTotal : 0);
}

void CAlignmentBasedRowSelector::InitAU(const AlignmentUtility* au, unsigned int nSelections) {

    unsigned int nRows = 0;

    if (m_au) delete m_au;

    m_au = (au) ? au->Clone() : NULL;
    nRows = (m_au) ? m_au->GetNRows() : 0;

    //  Constrain nSelections to not exceed number of rows available less
    //  the number of current exclusions.
    if (nSelections == 0 || nSelections > nRows) nSelections = nRows - m_excluded.size();

    CRowSelector::Init(nRows, nSelections);

    SetSequence();
}

/*
void CAlignmentBasedRowSelector::ExcludeStructureRows(bool skipMaster, vector<unsigned int>* excludedStructureRows) {

    if (excludedStructureRows) excludedStructureRows->clear();
    if (!m_au) return;

    unsigned int initRow = (skipMaster) ? 1 : 0;
    for (unsigned int i = initRow; i < m_nRows; ++i) {
        if (m_au->IsRowPDB(i)) {
            ExcludeRow(i);
            if (excludedStructureRows) excludedStructureRows->push_back(i);
        }
    }
}
*/

void CAlignmentBasedRowSelector::SetSequence(void) {

    vector<unsigned int> allowedValues(m_nRows);
    unsigned int thisSelection, nExcluded = m_excluded.size();
    unsigned int i;
    double rowScore;
    bool isExcluded;

    if (!m_au) return;

    m_sequence.clear();
    m_scoresToRow.clear();

    //  Get the scores for all rows.
    for (i = 0; i < m_nRows; ++i) {
        rowScore = (double) m_au->ScoreRowByPSSM(i);
        m_scoresToRow.insert(ScoreMapVT(rowScore, i));
    }

    if (m_sortBestToWorst) {
        ScoreMapRit smRit = m_scoresToRow.rbegin(), smRend = m_scoresToRow.rend();
        for (i = 0; smRit != smRend && i < m_nSelections; ++smRit) {
            thisSelection = smRit->second;
            isExcluded = (nExcluded > 0 && find(m_excluded.begin(), m_excluded.end(), thisSelection) != m_excluded.end());
            if (!isExcluded) {
                m_sequence.push_back(thisSelection);
//                cout << "row " << thisSelection << "; score " << smRit->first << endl;
                ++i;
            }

        }
    } else {
        ScoreMapIt smIt = m_scoresToRow.begin(), smEnd = m_scoresToRow.end();
        for (i = 0; smIt != smEnd && i < m_nSelections; ++smIt) {
            thisSelection = smIt->second;
            isExcluded = (nExcluded > 0 && find(m_excluded.begin(), m_excluded.end(), thisSelection) != m_excluded.end());
            if (!isExcluded) {
                m_sequence.push_back(thisSelection);
//                cout << "row " << thisSelection << "; score " << smIt->first << endl;
                ++i;
            }

        }
    }

}

bool CAlignmentBasedRowSelector::Update(const AlignmentUtility* newAU, unsigned int nSelection, bool bestToWorst) {
    if (!newAU) return false;

    m_sortBestToWorst = bestToWorst;
    Reset();
    InitAU(newAU, (nSelection > 0) ? nSelection : 0);
    return (m_au != NULL);
}

END_SCOPE(align_refine)
