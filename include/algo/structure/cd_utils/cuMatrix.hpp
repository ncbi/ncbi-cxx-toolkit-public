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
 *   part of CDTree app
 *
 *
 * ===========================================================================
 */

#ifndef CU_MATRIX_HPP
#define CU_MATRIX_HPP
#include <corelib/ncbistl.hpp>
//#include <ncbierr.h>
#include <cassert>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

class AMatrix_base {

  protected:
    double**  m_Array;
    bool*     m_ColumnFlags;
    int       m_NumRows, m_NumCols;
    int       m_RowIndex;

  public:
    AMatrix_base() {
      m_NumRows = 0;
      m_NumCols = 0;
      m_Array   = 0;
      m_ColumnFlags = 0;
    }

    AMatrix_base(const int NumRows, const int NumCols) {
      assert(NumRows >= 0);
      assert(NumCols >= 0);
      Allocate(NumRows, NumCols);
    }

    AMatrix_base(const AMatrix_base& Matrix) {
      Copy(Matrix);
    }

    AMatrix_base& operator= (const AMatrix_base& Matrix) {
      Copy(Matrix);
      return(*this);
    }
    
    ~AMatrix_base() {
      DeAllocate();
    }

    void GetSize(int& NumRows, int& NumCols) {
      NumRows = m_NumRows;
      NumCols = m_NumCols;
    }

    int GetNumRows() const {return(m_NumRows);}
    int GetNumCols() const {return(m_NumCols);}

    // for speed.  no error checking!
    double FastGet(const int RowIndex, const int ColIndex) const{
      return(m_Array[RowIndex][ColIndex]);
    }

    bool IsColSet(int ColIndex) const {
      if (ColIndex < m_NumCols) {
        if (m_ColumnFlags) {
          return(m_ColumnFlags[ColIndex]);
        }
      }
      return(false);
    }

    bool Shrink(const int NumRows, const int NumCols);

    void ReSize(const int NumRows, const int NumCols) {
      MakeSureArrayIsBigEnough(NumRows-1, NumCols-1);
    }

    // in combination with AMatrix::operator[], returns m_Array[i][j].
    // return ref to double so this operator can be used for setting.
    double& operator[] (int ColIndex) {
        assert(ColIndex >= 0);
        MakeSureArrayIsBigEnough(m_RowIndex, ColIndex);
        m_ColumnFlags[ColIndex] = true;
        return(m_Array[m_RowIndex][ColIndex]);
    }

    double Get(int RowIndex, int ColIndex) {
        assert((RowIndex >= 0) && (RowIndex < m_NumRows));
        assert((ColIndex >= 0) && (ColIndex < m_NumCols));
        return(m_Array[RowIndex][ColIndex]);
    }

    void Set(int RowIndex, int ColIndex, double Val) {
        assert(RowIndex >= 0);
        assert(ColIndex >= 0);
        MakeSureArrayIsBigEnough(RowIndex, ColIndex);
        m_ColumnFlags[ColIndex] = true;
        m_Array[RowIndex][ColIndex] = Val;
    }

	//  Apply linear transformation to all values y(new) = mx(old) + b
	void LinearTransform(double b, double m, bool ignoreDiagonal=false);
	void GetExtremalEntries(double& max, double& min, bool ignoreDiagonal=false);

    // null out the matrix
    void DeAllocate();

  private:
    void Allocate(const int NumRows, const int NumCols);
    void Copy(const AMatrix_base& Matrix);
    void SlowCopy(const AMatrix_base& Matrix);
    void MakeArrayBigger(const int RowIndex, const int ColIndex);
    bool MakeSureArrayIsBigEnough(const int RowIndex, const int ColIndex) {
      if ((m_NumRows > RowIndex) && (m_NumCols > ColIndex)) return(true);
      MakeArrayBigger(RowIndex, ColIndex);
      return(false);
    }

  protected:    
    int Max(int Val1, int Val2) {
      return(Val1 > Val2 ? Val1 : Val2);
    }
};


class AMatrix : public AMatrix_base {
//---------------------------------------------------------------
//  I've introduced this class so that I can peform the
//  [][] operator.  (e.g. Matrix[3][5])
//---------------------------------------------------------------

  public:

    AMatrix() : AMatrix_base() {}
    
    AMatrix(const int NumRows, const int NumCols) : AMatrix_base(NumRows, NumCols) {}

    AMatrix(const AMatrix& Matrix) : AMatrix_base(Matrix) {}
    
    // see AMatrix_base::operator[]
    AMatrix_base& operator[] (int RowIndex) {
        assert(RowIndex >= 0);
        m_RowIndex = RowIndex;
        return(*this);
    }
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif   //  ALGMATRIX_HPP

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
