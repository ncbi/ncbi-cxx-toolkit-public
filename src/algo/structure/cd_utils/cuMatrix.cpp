/* $Id$
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
 * Author:  David Hurwitz
 *
 * File Description:
 *   ported from CDTree app
 *
 */

#include <ncbi_pch.hpp>
#include <cassert>
#include <corelib/ncbi_limits.h>
#include <algo/structure/cd_utils/cuMatrix.hpp>

#include <string.h>  // for memset

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

void AMatrix_base::MakeArrayBigger(const int RowIndex, const int ColIndex) {
//-------------------------------------------------------------
// if m_Array is big enough, return true.
// if m_Array is too small, make it bigger and return false.
//-------------------------------------------------------------
  // make an array that's big enough (don't let either dimension shrink)
  int  NumRows = Max(m_NumRows, RowIndex+1);
  int  NumCols = Max(m_NumCols, ColIndex+1);
  AMatrix_base  NewMatrix(NumRows, NumCols);

  // copy old array into new array
  NewMatrix.SlowCopy(*this);

  // free the old array
  DeAllocate();

  // save new array in place of old array
  m_Array = NewMatrix.m_Array;
  m_ColumnFlags = NewMatrix.m_ColumnFlags;
  m_NumRows = NewMatrix.m_NumRows;
  m_NumCols = NewMatrix.m_NumCols;

  // pretend new array has been freed
  // (so that new array isn't freed when leaving this routine)
  NewMatrix.m_Array = NULL;
  NewMatrix.m_ColumnFlags = NULL;
  NewMatrix.m_NumRows = 0;
  NewMatrix.m_NumCols = 0;
}

//  Reduce the size of this, retaining data from the original array
//  for valid indices of the new array.
bool AMatrix_base::Shrink(const int NumRows, const int NumCols) {

    bool result = false;
    int i, j;

    if ((NumRows < m_NumRows) || (NumCols < m_NumCols)) {
        if (NumRows > 0 && NumCols > 0) {
            //  
            AMatrix_base tmpMatrix(NumRows, NumCols);
            for (i = 0; i < NumRows; ++i) {
                for (j = 0; j < NumCols; ++j) {
                    tmpMatrix.m_Array[i][j] = m_Array[i][j];
                    if (i == 0) {
                        tmpMatrix.m_ColumnFlags[j] = m_ColumnFlags[j];
                    }
                }
            }
            DeAllocate();
            Allocate(NumRows, NumCols);
            for (i = 0; i < NumRows; ++i) {
                for (j = 0; j < NumCols; ++j) {
                    m_Array[i][j] = tmpMatrix.m_Array[i][j];
                    if (i == 0) {
                        m_ColumnFlags[j] = tmpMatrix.m_ColumnFlags[j];
                    }
                }
            }
            result = true;
        }
    }
    return result;
}

void AMatrix_base::Allocate(const int NumRows, const int NumCols) {
//--------------------------------------------
// allocate memory for array
//--------------------------------------------
  typedef double*  DoublePtr;

  // allocate NumRows pointers-to-doubles
  m_Array = new DoublePtr[NumRows];
  // for each pointer-to-double
  for (int i=0; i<NumRows; i++) {
    // allocate NumCols doubles
    m_Array[i] = new double[NumCols];
  }

  // allocate space for column flags, set them to false
  m_ColumnFlags = new bool[NumCols];
  memset(m_ColumnFlags, 0, NumCols*sizeof(bool));

  m_NumRows = NumRows;
  m_NumCols = NumCols;
}


void AMatrix_base::DeAllocate() {
//--------------------------------------------
// deallocate array memory
//--------------------------------------------
  if (m_Array == NULL) return;

  // for each pointer-to-double
  for (int i=0; i<m_NumRows; i++) {
    // free NumCols doubles
    delete []m_Array[i];
    m_Array[i] = NULL;
  }
  // free NumRows pointer-to-doubles
  delete []m_Array;
  m_Array = NULL;

  // free column flags
  delete []m_ColumnFlags;
  m_ColumnFlags = NULL;

  m_NumRows = 0;
  m_NumCols = 0;
}



void AMatrix_base::Copy(const AMatrix_base& Matrix) {
//--------------------------------------------
// copy Matrix to this one
//--------------------------------------------
  DeAllocate();
  Allocate(Matrix.m_NumRows, Matrix.m_NumCols);
  memcpy(m_Array, Matrix.m_Array, m_NumRows*m_NumCols*sizeof(double));
  memcpy(m_ColumnFlags, Matrix.m_ColumnFlags, m_NumCols*sizeof(bool));
}


void AMatrix_base::SlowCopy(const AMatrix_base& Matrix) {
//------------------------------------------------------------------
// copy Matrix to this one.
// memory for Array must already be allocated.
// don't assume this matrix is the same size as Matrix.
//------------------------------------------------------------------
  // if matrices are the same size, go ahead and do a fast copy
  if ((m_NumRows == Matrix.m_NumRows) && (m_NumCols == Matrix.m_NumCols)) {
    memcpy(m_Array, Matrix.m_Array, m_NumRows*m_NumCols*sizeof(double));
  }

  // make sure this matrix is bigger than Matrix
  assert(m_NumRows >= Matrix.m_NumRows);
  assert(m_NumCols >= Matrix.m_NumCols);

  // copy Matrix into this matrix
  for (int i=0; i<Matrix.m_NumRows; i++) {
    memcpy(m_Array[i], Matrix.m_Array[i], Matrix.m_NumCols*sizeof(double));
  }

  // copy column flags
  memcpy(m_ColumnFlags, Matrix.m_ColumnFlags, Matrix.m_NumCols*sizeof(bool));
}


void AMatrix_base::LinearTransform(double b, double m, bool ignoreDiagonal) {
	int n = GetNumRows();
    for (int i=0; i<n; ++i) {
        for (int j=0; j<n; ++j) {
			if (ignoreDiagonal && i == j) {
				continue;
			} else {
				m_Array[i][j] = m*m_Array[i][j] + b;
			}
        }
    }	
}

//  Does not assume a symmetric matrix.
void AMatrix_base::GetExtremalEntries(double& max, double& min, bool ignoreDiagonal) {
	int i, j;
	int n = GetNumRows();
	double maxEntry=kMin_Double;
	double minEntry=kMax_Double;
	for (i=0; i<n; ++i) {
		for (j=0; j<n; ++j) {
			if (ignoreDiagonal && i == j) {
				continue;
			}
			if (m_Array[i][j] > maxEntry) {
				maxEntry = m_Array[i][j];
			}
			if (m_Array[i][j] < minEntry) {
				minEntry = m_Array[i][j];
			}
		}
	}
	max = maxEntry;
	min = minEntry;
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
 * ===========================================================================
 */
