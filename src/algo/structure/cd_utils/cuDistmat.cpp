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
* Author:  Chris Lanczycki
*
* File Description:  cdt_distmat.cpp
*
*      Representation of a distance matrix for phylogenetic calculations.
*      Note that the base class AMatrix is explicitly a matrix of doubles;
*      templatize if becomes necessary.
*
*/

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuDistmat.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const bool   DistanceMatrix::USE_ALIGNED_DEFAULT = true;
const int    DistanceMatrix::OUTPUT_PRECISION    = 3;
const int    DistanceMatrix::NO_EXTENSION        = 0;
const int    DistanceMatrix::INITIAL_SCORE_BOUND = (int)1e10;
const double DistanceMatrix::TINY_DISTANCE       = 0.0003;
const double DistanceMatrix::HUGE_DISTANCE       = kMax_Double;

//
//  Utility Functions
//
string DistanceMatrix::GetDistMethodName(EDistMethod method)
{
	return DISTANCE_METHOD_NAMES[method];
}

bool DistanceMatrix::DistMethodUsesScoringMatrix(EDistMethod method)
{
	return ((method == ePercentIdentity || method == ePercIdWithKimura) ? false : true);
}

bool DistanceMatrix::ExtensionsAllowed(EDistMethod method) {

	//return ((method != eNoDistMethod) ? true : false);
	return (method == eScoreBlastFoot ||
			method == eScoreAlignedOptimal);
}

bool DistanceMatrix::RequireAlignedBlocks(EDistMethod method)
{
	return		(method == ePercentIdentity		|| 
				 method == ePercIdWithKimura	||
				 method == eScoreAligned		||
				 method == eScoreAlignedOptimal); /* ||
                 method == eScoreBlastFoot);       */   //  added CJL; 4/14/04
}

//
//  DistanceMatrix class methods
//

DistanceMatrix::~DistanceMatrix() {
    delete m_scoreMatrix;
}

void DistanceMatrix::initialize() {
    //m_cd = NULL;
	m_aligns = NULL;
    m_scoreMatrix = NULL;
    m_useAligned = USE_ALIGNED_DEFAULT;
    m_nTermExt = NO_EXTENSION;
    m_cTermExt = NO_EXTENSION;
    m_dMethod  = eNoDistMethod;
    m_ConvertedSequences.clear();
}
/*
void DistanceMatrix::SetCDD(const CCd* cddref, CCd::AlignmentUsage alignUse) 
{
	m_cd = const_cast<CCd*>(cddref);
	AlignmentCollection aligns(m_cd, alignUse);
	int nrows = aligns.GetNumRows();
	ReSize(nrows, nrows);
}*/

void DistanceMatrix::SetData(AlignmentCollection* aligns)
{
	//m_cd = aligns.getFirstCD();
	m_aligns = aligns;
	ReSize(m_aligns->GetNumRows(), m_aligns->GetNumRows());
}

double DistanceMatrix::GetMaxEntry() {
	
	double mx, mn;
	GetExtremalEntries(mx, mn, true);
	return mx;
}
double DistanceMatrix::GetMinEntry() {

	double mx, mn;
	GetExtremalEntries(mx, mn, true);
	return mn;
}

void DistanceMatrix::EnforceSymmetry() {

    int i, j;

    for (i=1; i<GetNumRows(); ++i) {
        for (j=0; j<i; ++j) {
            if (m_Array[i][j] != m_Array[j][i]) {
                m_Array[i][j] = 0.5*(m_Array[i][j] + m_Array[j][i]);
                m_Array[j][i] = m_Array[i][j];
            }
        }
    }
}

void DistanceMatrix::ReplaceZeroWithTinyValue(const double tiny) {

    int i, j;

    for (i=1; i<GetNumRows(); ++i) {
        for (j=0; j<i; ++j) {
            if (m_Array[i][j] == 0.0) {
                m_Array[i][j] = tiny;
                m_Array[j][i] = tiny;
            }
        }
    }	
}

//  Terminal extension related methods
void DistanceMatrix::SetNTermExt(int ext) {
    m_nTermExt = ext;
    m_useAligned = (m_nTermExt == 0) ? true : false;
}
int  DistanceMatrix::GetNTermExt() {
    return m_nTermExt;
}
void DistanceMatrix::SetCTermExt(int ext) {
    m_cTermExt = ext;
    m_useAligned = (m_cTermExt == 0) ? true : false;
}
int  DistanceMatrix::GetCTermExt() {
    return m_cTermExt;
}

//  Scoring matrix methods
string DistanceMatrix::GetMatrixName() {
    string s = kEmptyStr;
    if (m_scoreMatrix) {
        s = m_scoreMatrix->GetName();
    }
    return s;
}

EScoreMatrixType DistanceMatrix::GetMatrixType() {
    EScoreMatrixType myType = eInvalidMatrixType;
    if (m_scoreMatrix) {
        myType = m_scoreMatrix->GetType();
    }
    return myType;
}

//  discards old matrix even if of the same type
bool DistanceMatrix::ResetMatrixType(EScoreMatrixType newType) {
    if (m_scoreMatrix) {
        delete m_scoreMatrix;
    }
    m_scoreMatrix = new ScoreMatrix(newType);
    return (m_scoreMatrix != NULL) ? true : false;
}


//  Distance matrix I/O methods
//////////////////////////////////////////////////////
//  File format:  
//  line 1:  integer dimension
//  lines 2 - (dim+1):  space-delimited doubles
void DistanceMatrix::readMat(ifstream& is, DistanceMatrix& dm, bool triangular) {

    int dim;
    int row, col, count, nexpect;
    double value;
    string seqId;

    if (is >> dim) {
        if (dim <= 0) {
            cerr << "Error:  " << dim << " is an invalidof matrix dimensionality.\n";
            exit(1);
        }
        dm.ReSize(dim, dim);
    }

    row   = 0;
    col   = 0;
    count = 0;
    if (triangular) {  //  symmetric matrix with zero diagonal elements
        nexpect = dim*(dim-1)/2;
        dm[0][0] = 0.0;
        for (row = 0; row < dim; ++row) {
            col = 0;
            dm[row][row] = 0.0;
            is >> seqId;  //  strip off the seqId
            while (col < row && is >> value) {
                count++;
                //cout << row << " " << col << " " << value << endl;
                dm[row][col] = value;
                dm[col][row] = value;
                col++;
            }
        }
    } else {      // full matrix
        nexpect = dim*dim;
        is >> seqId;  // strip off first row's seqId
        while (row != dim && is >> value) {
            count++;
            dm[row][col] = value;
            col++;
            if (col == dim) {
                col = 0;
                row++;
                if (row != dim) is >> seqId;  // strip off next row's seqId
            }
        }
    }
    if (count != nexpect) {
        cerr << "Error:  " << nexpect << " values expected; " << count << " found.\n";
    }
        
}

void DistanceMatrix::writeMat(ofstream& ofs, const DistanceMatrix& dm, bool triangular) {
    dm.writeMat(ofs, triangular);
}

void DistanceMatrix::printMat(bool triangular) {
    writeMat(cout, triangular);
}
    
void DistanceMatrix::writeMat(ostream& os, bool triangular) const {
    
    int nrows = GetNumRows();
    int prec  = os.precision();
    string seqId, notFound = "<not found>";

    //cout << "precision to start:  " << prec << endl;

    os << nrows << endl;
    std::ios_base::fmtflags initFlags = os.setf(ios::scientific, ios::floatfield);
//    os.setf(0, ios::floatfield);
    os.precision(OUTPUT_PRECISION);
    if (triangular) {  //  symmetric matrix with zero diagonal elements
        for (int row=0; row<nrows; ++row) {
            seqId.erase();
            if (!m_aligns->Get_GI_or_PDB_String_FromAlignment(row, seqId)) seqId = notFound;
            os << setw(12) << left << seqId << " ";
            for (int col=0; col<row; ++col) {
                os << setw(10) << (*(const_cast<DistanceMatrix*>(this)))[row][col]  << "  ";
            }
            os << endl;
        }

    } else {      //  full matrix 
        for (int row=0; row<nrows; ++row) {
            seqId.erase();
            if (!m_aligns->Get_GI_or_PDB_String_FromAlignment(row, seqId)) seqId = notFound;
            os << setw(12) << left << seqId << " ";
            for (int col=0; col<nrows; ++col) {
                os << setw(10) << (*(const_cast<DistanceMatrix*>(this)))[row][col]  << "  ";
            }
            os << endl;
        }
    }
    os.setf(initFlags, ios::floatfield);
    os.precision(prec);
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/04/19 14:27:18  lanczyck
* initial version under algo/structure
*
*
*/
