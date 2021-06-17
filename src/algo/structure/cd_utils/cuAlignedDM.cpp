#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuAlignedDM.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const double AlignedDM::FRACTIONAL_EXTRA_OFFSET = 0.005;

AlignedDM::AlignedDM()
:DistanceMatrix(), m_ppAlignedResidues(0), m_maligns(0)
{
}

void AlignedDM::setData(MultipleAlignment* malign)
{
	m_maligns = malign;
	DistanceMatrix::SetData(malign);
}

AlignedDM::~AlignedDM()
{
	 if (m_ppAlignedResidues) {
		for (int i=0; i<m_NumRows; i++) {
            delete [] m_ppAlignedResidues[i];
        }  
        delete [] m_ppAlignedResidues;
	 }
}

//  Pack the m_ppAlignedResidues array as follows:
//  [0...nAligned-1][nAligned...nAligned+m_nTermExt-1][nAligned+m_nTermExt....nAligned+m_nTermExt+m_cTermExt-1]
//  If either m_nTermExt or m_cTermExt < 0, drop the corresponding appended range and 
//  make the corresponding parts of the [0...nAligned-1] range 0 == no residue.
bool AlignedDM::GetResidueListsWithShifts() {

    int nrows       = 0;
    int resArrayDim = 0;
    int nAligned    = 0;
    int firstAligned, lastAligned;

    bool result    = true;

    if (m_aligns) {
		nrows = m_aligns->GetNumRows();
    } else {
        return false;
    }
        
    // if space is already allocated, clean up and get fresh copy
    if (m_ppAlignedResidues) {
        for (int i=0; i<m_NumRows; i++) {
            delete [] m_ppAlignedResidues[i];
        }  
        delete [] m_ppAlignedResidues;
        m_ppAlignedResidues = NULL;
    }
    m_ConvertedSequences.clear();
    m_ppAlignedResidues = new CharPtr[nrows];
    if (m_ppAlignedResidues == NULL) {
        return false;
    }

    //  Make m_ppAlignedResidues large enough to hold any positive extensions
    nAligned = m_aligns->GetAlignmentLength();
    resArrayDim = nAligned + Max(0, m_nTermExt) + Max(0, m_cTermExt);
    for (int i=0; i<nrows; i++) {
        m_ppAlignedResidues[i] = new char[resArrayDim];
        if (m_ppAlignedResidues[i] == NULL) {
            result = false;
        }
    }

    //  Get all sequences and mapping from rowIndex -> seqIndex
	//m_ConvertedSequences.clear();
    m_maligns->GetAllSequences( m_ConvertedSequences); 
    m_maligns->GetAlignedResiduesForAll( m_ppAlignedResidues, true);
//    m_cd->SetAlignedResidues();  //m_ppAlignedResidues, m_ConvertedSequences);

    if (m_nTermExt == 0 && m_cTermExt == 0) {
        return true;
    }

    //  N-terminal extension...  if negative, eliminate that many residues from front of array
    if (m_nTermExt < 0) {
        for (int i = 0; i<nrows; ++i) {
            for (int j = 0; j<-m_nTermExt; ++j) {
                m_ppAlignedResidues[i][j] = 0;
            }
        }
    //                           if positive, add after the list of aligned residues
    } else {
        for (int i = 0; i<nrows; ++i) {
            firstAligned = m_maligns->GetLowerBound(i);
            for (int j = m_nTermExt; j > 0 ; --j) {
                m_ppAlignedResidues[i][m_nTermExt - j + nAligned] = (firstAligned - j < 0) ? 0 : m_ConvertedSequences[i][firstAligned - j];
            }
        }
    }

    //  C-terminal extension...  if negative, eliminate that many residues from back of aligned residues section
    if (m_cTermExt < 0) {
        for (int i = 0; i<nrows; ++i) {
            for (int j = 0; j<-m_cTermExt; ++j) {
                m_ppAlignedResidues[i][nAligned - 1 - j] = 0;
            }
        }
    //                           if positive, add after end of n-term extensions
    } else {
        int nTermOffset = Max(0, m_nTermExt);
        int seqLen;
        for (int i = 0; i<nrows; ++i) {
            lastAligned = m_maligns->GetUpperBound(i);
            seqLen = m_ConvertedSequences[i].size();
            for (int j = 0; j < m_cTermExt; ++j) {
                m_ppAlignedResidues[i][j + nAligned + nTermOffset] = 
                    (lastAligned + 1 + j >= seqLen) ? 0 : m_ConvertedSequences[i][lastAligned + 1 + j];
            }
        }
    }

    return result;
}

int AlignedDM::GetMaxScoreForAligned() {

	int nrows;
	int rowVal;
	int maxVal = -INITIAL_SCORE_BOUND;
//	if (m_cd != null) {
	if (m_maligns) {
		nrows = m_maligns->GetNumRows();
		// if space is already allocated then safe to assume array has been filled in
		if (!m_ppAlignedResidues) {
	        m_ppAlignedResidues = new CharPtr[nrows];
	        for (int i=0; i<nrows; i++) {
	            m_ppAlignedResidues[i] = new char[m_maligns->GetAlignmentLength()];
			}
//            SetConvertedSequencesForCD(m_cd, m_ConvertedSequences); 
            m_maligns->GetAlignedResiduesForAll(m_ppAlignedResidues, true);
//	        m_cd->SetAlignedResidues(m_ppAlignedResidues, m_ConvertedSequences);
		}
		for (int i=0; i<nrows; ++i) {
			rowVal = GetMaxScore(m_ppAlignedResidues[i]);
			if (rowVal > maxVal) {
				maxVal = rowVal;
			}
		}
	}
	return maxVal;
}

//  Calculate score for an exact match to a set of residues. 
int AlignedDM::GetMaxScore(CharPtr residues) {

    char Res1;
    int score=0;
    int alignLen;
	
//	if (m_cd == null || m_scoreMatrix == NULL || m_scoreMatrix->GetType() == eInvalidMatrixType) {
	if (m_maligns == NULL || m_scoreMatrix == NULL || m_scoreMatrix->GetType() == eInvalidMatrixType) {
		return -INITIAL_SCORE_BOUND;
	}

	alignLen = m_maligns->GetAlignmentLength();
    // for each column of the alignment
    for (int i=0; i<alignLen; i++) {
        Res1 = residues[i];
        if (Res1 > 0) {
            score += m_scoreMatrix->GetScore(Res1, Res1);
        }
    }
    return score;
}


int AlignedDM::SetMinScore() {

	int minVal = INITIAL_SCORE_BOUND;
	if (m_maligns && m_scoreMatrix != NULL && m_scoreMatrix->GetType() != eInvalidMatrixType) {
		minVal = m_maligns->GetAlignmentLength() * m_scoreMatrix->GetScore('*', 'A');
	}
	return minVal;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
