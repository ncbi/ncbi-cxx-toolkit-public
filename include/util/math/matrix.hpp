#ifndef UTIL_MATH___MATRIX__HPP
#define UTIL_MATH___MATRIX__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <util/math/promote.hpp>
#include <vector>


BEGIN_NCBI_SCOPE

template <class T> class CNcbiMatrix;


template <class T>
class CNcbiMatrix
{
public:
    typedef vector<T>                      TData;
    typedef typename TData::iterator       iterator;
    typedef typename TData::const_iterator const_iterator;

    CNcbiMatrix();
    CNcbiMatrix(size_t r, size_t c, T val = T(0));

    // make this matrix an identity matrix of a given size
    void Identity(size_t size);

    // make this matrix an identity matrix
    void Identity();

    // make this matrix a diagonal matrix of a given size, with a given value
    // on the diagonal
    void Diagonal(size_t size, T val);

    // make this matrix a diagonal matrix, with a given value on the diagonal
    void Diagonal(T val);

    // transpose this matrix
    void Transpose();

    // resize this matrix, filling the empty cells with a known value
    void Resize(size_t i, size_t j, T val = T(0));

    // swap two rows in the matrix
    void SwapRows(size_t i, size_t j);

    // get the number of rows in this matrix
    size_t GetRows() const;

    // get the number of columns in this matrix
    size_t GetCols() const;

    // retrieve the data associated with this matrix
    TData&         GetData();
    const TData&   GetData() const;

    // iterators
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    // set all values in the matrix to a known value
    void Set(T val);

    // swap two matrices efficiently
    void Swap(CNcbiMatrix<T>& M);

    // operator[] for raw data indexing
    const T&    operator[] (size_t i) const;
    T&          operator[] (size_t i);

    // operator() for row/column indexing
    const T&    operator() (size_t i, size_t j) const;
    T&          operator() (size_t i, size_t j);

protected:

    // the data strip we use
    TData  m_Data;

    // size of this matrix
    size_t m_Rows;
    size_t m_Cols;
};



///
/// global operators
///

///
/// stream input.
///
/// One line per row, with entries readable by successive calls
/// to operator>>.  For doubles, this just means they're separated
/// by whitespace.
template <class T>
inline CNcbiIstream&
operator>> (CNcbiIstream& is, CNcbiMatrix<T>& M);

///
/// stream output.
///
/// One line per row, with entries separated by a single space
template <class T>
inline CNcbiOstream&
operator<< (CNcbiOstream& os, const CNcbiMatrix<T>& M);

///
/// global addition: matrix + matrix
///
template <class T, class U>
inline CNcbiMatrix< NCBI_PROMOTE(T,U) >
operator+ (const CNcbiMatrix<T>&, const CNcbiMatrix<U>&);

///
/// global subtraction: matrix - matrix
///
template <class T, class U>
inline CNcbiMatrix< NCBI_PROMOTE(T,U) >
operator- (const CNcbiMatrix<T>&, const CNcbiMatrix<U>&);


///
/// global multiplication: matrix * scalar
/// this is a hack, as MSVC doesn't support partial template specialization
/// we provide implementations for a number of popular types
///
template <class T>
inline CNcbiMatrix< NCBI_PROMOTE(T, size_t) >
operator* (const CNcbiMatrix<T>&, size_t);

template <class U>
inline CNcbiMatrix< NCBI_PROMOTE(size_t, U) >
operator* (size_t, const CNcbiMatrix<U>&);


template <class T>
inline CNcbiMatrix< NCBI_PROMOTE(T, float) >
operator* (const CNcbiMatrix<T>&, float);

template <class U>
inline CNcbiMatrix< NCBI_PROMOTE(float, U) >
operator* (float, const CNcbiMatrix<U>&);


template <class T>
inline CNcbiMatrix< NCBI_PROMOTE(T, double) >
operator* (const CNcbiMatrix<T>&, double);

template <class U>
inline CNcbiMatrix< NCBI_PROMOTE(double, U) >
operator* (double, const CNcbiMatrix<U>&);


///
/// global multiplication: matrix * matrix
///
template <class T, class U>
inline CNcbiMatrix< NCBI_PROMOTE(T,U) >
operator* (const CNcbiMatrix<T>&, const CNcbiMatrix<U>&);

///
/// global multiplication: matrix * vector
///
template <class T, class U>
inline vector< NCBI_PROMOTE(T,U) >
operator* (const CNcbiMatrix<T>&, const vector<U>&);

///
/// global multiplication: vector * matrix
///
template <class T, class U>
inline vector< NCBI_PROMOTE(T,U) >
operator* (const vector<T>&, const CNcbiMatrix<U>&);

///
/// global addition: matrix += matrix
///
template <class T, class U>
inline CNcbiMatrix<T>&
operator+= (CNcbiMatrix<T>&, const CNcbiMatrix<U>&);

///
/// global subtraction: matrix -= matrix
///
template <class T, class U>
inline CNcbiMatrix<T>&
operator-= (CNcbiMatrix<T>&, const CNcbiMatrix<U>&);

///
/// global multiplication: matrix *= matrix
///
template <class T, class U>
inline CNcbiMatrix<T>&
operator*= (CNcbiMatrix<T>&, const CNcbiMatrix<U>&);

///
/// global multiplication: matrix *= vector
///
template <class T, class U>
inline vector<T>&
operator*= (CNcbiMatrix<T>&, const vector<U>&);

///
/// global multiplication: matrix *= scalar
///
template <class T>
inline CNcbiMatrix<T>&
operator*= (CNcbiMatrix<T>&, T);

///
/// global division: matrix /= matrix
///
template <class T>
inline CNcbiMatrix<T>&
operator/= (CNcbiMatrix<T>&, T);

///
/// global comparison: matrix == matrix
///
template <class T, class U>
inline bool
operator== (const CNcbiMatrix<T>&, const CNcbiMatrix<U>&);

///
/// global comparison: matrix != matrix
///
template <class T, class U>
inline bool
operator!= (const CNcbiMatrix<T>&, const CNcbiMatrix<U>&);


/////////////////////////////////////////////////////////////////////////////
//
// Inline Methods
//

///
/// default ctor
template <class T>
inline CNcbiMatrix<T>::CNcbiMatrix()
    : m_Rows(0),
      m_Cols(0)
{
}


template <class T>
inline CNcbiMatrix<T>::CNcbiMatrix(size_t r, size_t c, T val)
    : m_Rows(r),
      m_Cols(c)
{
    m_Data.resize(r*c, val);
}


template <class T>
inline size_t CNcbiMatrix<T>::GetRows() const
{
    return m_Rows;
}


template <class T>
inline size_t CNcbiMatrix<T>::GetCols() const
{
    return m_Cols;
}


template <class T>
inline typename CNcbiMatrix<T>::TData& CNcbiMatrix<T>::GetData()
{
    return m_Data;
}


template <class T>
inline const typename CNcbiMatrix<T>::TData& CNcbiMatrix<T>::GetData() const
{
    return m_Data;
}

template <class T>
inline typename CNcbiMatrix<T>::iterator CNcbiMatrix<T>::begin()
{
    return m_Data.begin();
}


template <class T>
inline typename CNcbiMatrix<T>::iterator CNcbiMatrix<T>::end()
{
    return m_Data.end();
}


template <class T>
inline typename CNcbiMatrix<T>::const_iterator CNcbiMatrix<T>::begin() const
{
    return m_Data.begin();
}


template <class T>
inline typename CNcbiMatrix<T>::const_iterator CNcbiMatrix<T>::end() const
{
    return m_Data.end();
}


template <class T>
inline const T&
CNcbiMatrix<T>::operator[] (size_t i) const
{
    _ASSERT(i >= 0  &&  i < m_Data.size());
    return m_Data[i];
}


template <class T>
inline T& CNcbiMatrix<T>::operator[] (size_t i)
{
    _ASSERT(i >= 0  &&  i < m_Data.size());
    return m_Data[i];
}


template <class T>
inline const T& CNcbiMatrix<T>::operator() (size_t i, size_t j) const
{
    _ASSERT(i >= 0  &&  i < m_Rows);
    _ASSERT(j >= 0  &&  j < m_Cols);

    return m_Data[i * m_Cols + j];
} 


template <class T>
inline T& CNcbiMatrix<T>::operator() (size_t i, size_t j)
{
    _ASSERT(i >= 0  &&  i < m_Rows);
    _ASSERT(j >= 0  &&  j < m_Cols);

    return m_Data[i * m_Cols + j];
}


template <class T>
inline void CNcbiMatrix<T>::Resize(size_t new_rows, size_t new_cols, T val)
{

    if (new_cols == m_Cols && new_rows >= m_Rows) {
        // common special case that can easily be handled efficiently
        m_Data.resize(new_rows * new_cols, val);
    } else {
        /// hack: we just make a new strip and copy things correctly
        /// there is a faster way to do this
        TData new_data(new_rows * new_cols, val);
        size_t i = min(new_rows, m_Rows);
        size_t j = min(new_cols, m_Cols);
        
        for (size_t r = 0;  r < i;  ++r) {
            for (size_t c = 0;  c < j;  ++c) {
                new_data[r * new_cols + c] = m_Data[r * m_Cols + c];
            }
        }

        new_data.swap(m_Data);
    }
    m_Rows = new_rows;
    m_Cols = new_cols;
}


template <class T>
inline void CNcbiMatrix<T>::Set(T val)
{
    m_Data.clear();
    m_Data.resize(m_Rows * m_Cols, val);
}


template <class T>
inline void CNcbiMatrix<T>::Identity()
{
    _ASSERT(m_Rows == m_Cols);
    Set(T(0));

    for (size_t i = 0;  i < m_Rows;  ++i) {
        m_Data[i * m_Cols + i] = T(1);
    }
}


template <class T>
inline void CNcbiMatrix<T>::Identity(size_t size)
{
    Resize(size, size);
    Set(T(0));

    for (size_t i = 0;  i < m_Rows;  ++i) {
        m_Data[i * m_Cols + i] = T(1);
    }
}


template <class T>
inline void CNcbiMatrix<T>::Diagonal(T val)
{
    _ASSERT(m_Rows == GetCols());
    Set(T(0));

    for (size_t i = 0;  i < m_Rows;  ++i) {
        m_Data[i * m_Cols + i] = val;
    }
}


template <class T>
inline void CNcbiMatrix<T>::Diagonal(size_t size, T val)
{
    Resize(size, size);
    Set(T(0));

    for (size_t i = 0;  i < m_Rows;  ++i) {
        m_Data[i * m_Cols + i] = val;
    }
}


template <class T>
inline void CNcbiMatrix<T>::Transpose()
{
    TData new_data(m_Cols * m_Rows);

    for (size_t i = 0;  i < m_Rows;  ++i) {
        for (size_t j = 0;  j < m_Cols;  ++j) {
            new_data[j * m_Cols + i] = m_Data[i * m_Cols + j];
        }
    }

    m_Data.swap(new_data);
    swap(m_Rows, m_Cols);
}


template <class T>
inline void CNcbiMatrix<T>::SwapRows(size_t i, size_t j)
{
    size_t i_offs = i * m_Cols;
    size_t j_offs = j * m_Cols;

    for (size_t c = 0;  c < m_Cols;  ++c) {
        swap(m_Data[i_offs + c], m_Data[j_offs + c] );
    }
}


template <class T>
inline void CNcbiMatrix<T>::Swap(CNcbiMatrix<T>& M)
{
    m_Data.swap(M.m_Data);
    swap(m_Cols, M.m_Cols);
    swap(m_Rows, M.m_Rows);
}


///
/// global operators
///


///
/// addition
///

template <class T, class U>
inline CNcbiMatrix< NCBI_PROMOTE(T,U) >
operator+ (const CNcbiMatrix<T>& m0, const CNcbiMatrix<U>& m1)
{
    _ASSERT(m0.GetRows() == m1.GetRows());
    _ASSERT(m0.GetCols() == m1.GetCols());

    CNcbiMatrix<NCBI_PROMOTE(T,U)> res(m0.GetRows(), m0.GetCols());

    typename CNcbiMatrix<NCBI_PROMOTE(T,U)>::iterator res_iter = res.begin();
    typename CNcbiMatrix<NCBI_PROMOTE(T,U)>::iterator res_end  = res.end();
    typename CNcbiMatrix<T>::const_iterator           m0_iter  = m0.begin();
    typename CNcbiMatrix<U>::const_iterator           m1_iter  = m1.begin();

    for ( ;  res_iter != res_end;  ++res_iter, ++m0_iter, ++m1_iter) {
        *res_iter = *m0_iter + *m1_iter;
    }

    return res;
}


template <class T, class U>
inline CNcbiMatrix<T>&
operator+= (CNcbiMatrix<T>& m0, const CNcbiMatrix<U>& m1)
{
    _ASSERT(m0.GetRows() == m1.GetRows());
    _ASSERT(m0.GetCols() == m1.GetCols());

    typename CNcbiMatrix<T>::iterator           m0_iter  = m0.begin();
    typename CNcbiMatrix<T>::iterator           m0_end   = m0.end();
    typename CNcbiMatrix<U>::const_iterator     m1_iter  = m1.begin();

    size_t mod = (m0.GetRows() * m0.GetCols()) % 4;
    for ( ;  mod;  --mod, ++m0_iter, ++m1_iter) {
        *m0_iter += *m1_iter;
    }

    for ( ;  m0_iter != m0_end;  ) {
        *m0_iter++ += *m1_iter++;
        *m0_iter++ += *m1_iter++;
        *m0_iter++ += *m1_iter++;
        *m0_iter++ += *m1_iter++;
    }

    return m0;
}


///
/// subtraction
///

template <class T, class U>
inline CNcbiMatrix< NCBI_PROMOTE(T,U) >
operator- (const CNcbiMatrix<T>& m0, const CNcbiMatrix<U>& m1)
{
    _ASSERT(m0.GetRows() == m1.GetRows());
    _ASSERT(m0.GetCols() == m1.GetCols());

    CNcbiMatrix< NCBI_PROMOTE(T,U) > res(m0.GetRows(), m0.GetCols());
    typename CNcbiMatrix<NCBI_PROMOTE(T,U)>::iterator res_iter = res.begin();
    typename CNcbiMatrix<NCBI_PROMOTE(T,U)>::iterator res_end  = res.end();
    typename CNcbiMatrix<T>::const_iterator           m0_iter  = m0.begin();
    typename CNcbiMatrix<U>::const_iterator           m1_iter  = m1.begin();

    for ( ;  res_iter != res_end;  ++res_iter, ++m0_iter, ++m1_iter) {
        *res_iter = *m0_iter - *m1_iter;
    }

    return res;
}


template <class T, class U>
inline CNcbiMatrix<T>&
operator-= (CNcbiMatrix<T>& m0, const CNcbiMatrix<U>& m1)
{
    _ASSERT(m0.GetRows() == m1.GetRows());
    _ASSERT(m0.GetCols() == m1.GetCols());

    typename CNcbiMatrix<T>::iterator           m0_iter  = m0.begin();
    typename CNcbiMatrix<T>::iterator           m0_end   = m0.end();
    typename CNcbiMatrix<U>::const_iterator     m1_iter  = m1.begin();

    for ( ;  m0_iter != m0_end;  ++m0_iter, ++m1_iter) {
        *m0_iter -= *m1_iter;
    }

    return m0;
}


///
/// multiplication
///
template <class T>
inline CNcbiMatrix< NCBI_PROMOTE(T, size_t) >
operator* (const CNcbiMatrix<T>& m0, size_t val)
{
    CNcbiMatrix< NCBI_PROMOTE(T, size_t) > res(m0.GetRows(), m0.GetCols());

    typename CNcbiMatrix<T>::iterator       res_iter = res.begin();
    typename CNcbiMatrix<T>::iterator       res_end  = res.end();
    typename CNcbiMatrix<T>::const_iterator m0_iter  = m0.begin();

    for ( ;  res_iter != res_end;  ++res_iter, ++m0_iter) {
        *res_iter = *m0_iter * val;
    }

    return res;
}


template <class U>
inline CNcbiMatrix< NCBI_PROMOTE(size_t, U) >
operator* (size_t val, const CNcbiMatrix<U>& m0)
{
    CNcbiMatrix< NCBI_PROMOTE(U, size_t) > res(m0.GetRows(), m0.GetCols());

    typename CNcbiMatrix<U>::iterator       res_iter = res.begin();
    typename CNcbiMatrix<U>::iterator       res_end  = res.end();
    typename CNcbiMatrix<U>::const_iterator m0_iter  = m0.begin();

    for ( ;  res_iter != res_end;  ++res_iter, ++m0_iter) {
        *res_iter = *m0_iter * val;
    }

    return res;
}


template <class T>
inline CNcbiMatrix< NCBI_PROMOTE(T, float) >
operator* (const CNcbiMatrix<T>& m0, float val)
{
    CNcbiMatrix< NCBI_PROMOTE(T, float) > res(m0.GetRows(), m0.GetCols());

    typename CNcbiMatrix<T>::iterator       res_iter = res.begin();
    typename CNcbiMatrix<T>::iterator       res_end  = res.end();
    typename CNcbiMatrix<T>::const_iterator m0_iter  = m0.begin();

    for ( ;  res_iter != res_end;  ++res_iter, ++m0_iter) {
        *res_iter = *m0_iter * val;
    }

    return res;
}


template <class U>
inline CNcbiMatrix< NCBI_PROMOTE(float, U) >
operator* (float val, const CNcbiMatrix<U>& m0)
{
    CNcbiMatrix< NCBI_PROMOTE(U, float) > res(m0.GetRows(), m0.GetCols());

    typename CNcbiMatrix<U>::iterator       res_iter = res.begin();
    typename CNcbiMatrix<U>::iterator       res_end  = res.end();
    typename CNcbiMatrix<U>::const_iterator m0_iter  = m0.begin();

    for ( ;  res_iter != res_end;  ++res_iter, ++m0_iter) {
        *res_iter = *m0_iter * val;
    }

    return res;
}


template <class T>
inline CNcbiMatrix< NCBI_PROMOTE(T, double) >
operator* (const CNcbiMatrix<T>& m0, double val)
{
    CNcbiMatrix< NCBI_PROMOTE(T, double) > res(m0.GetRows(), m0.GetCols());

    typename CNcbiMatrix<T>::iterator       res_iter = res.begin();
    typename CNcbiMatrix<T>::iterator       res_end  = res.end();
    typename CNcbiMatrix<T>::const_iterator m0_iter  = m0.begin();

    for ( ;  res_iter != res_end;  ++res_iter, ++m0_iter) {
        *res_iter = *m0_iter * val;
    }

    return res;
}


template <class U>
inline CNcbiMatrix< NCBI_PROMOTE(double, U) >
operator* (double val, const CNcbiMatrix<U>& m0)
{
    CNcbiMatrix< NCBI_PROMOTE(U, double) > res(m0.GetRows(), m0.GetCols());

    typename CNcbiMatrix<U>::iterator       res_iter = res.begin();
    typename CNcbiMatrix<U>::iterator       res_end  = res.end();
    typename CNcbiMatrix<U>::const_iterator m0_iter  = m0.begin();

    for ( ;  res_iter != res_end;  ++res_iter, ++m0_iter) {
        *res_iter = *m0_iter * val;
    }

    return res;
}


template <class T>
inline CNcbiMatrix<T>&
operator*= (CNcbiMatrix<T>& m0, T val)
{
    typename CNcbiMatrix<T>::iterator m0_iter  = m0.begin();
    typename CNcbiMatrix<T>::iterator m0_end  = m0.end();

    for ( ;  m0_iter != m0_end;  ++m0_iter) {
        *m0_iter *= val;
    }

    return m0;
}


template <class T>
inline CNcbiMatrix<T>&
operator/= (CNcbiMatrix<T>& m0, T val)
{
    val = T(1/val);

    typename CNcbiMatrix<T>::iterator m0_iter  = m0.begin();
    typename CNcbiMatrix<T>::iterator m0_end  = m0.end();

    for ( ;  m0_iter != m0_end;  ++m0_iter) {
        *m0_iter *= val;
    }

    return m0;
}


template <class T, class U>
inline vector< NCBI_PROMOTE(T,U) >
operator* (const CNcbiMatrix<T>& m, const vector<U>& v)
{
    _ASSERT(m.GetCols() == v.size());

    typedef NCBI_PROMOTE(T,U) TPromote;
    vector< TPromote > res(m.GetRows(), TPromote(0));

    for (size_t r = 0;  r < m.GetRows();  ++r) {
        for (size_t i = 0;  i < m.GetCols();  ++i) {
            res[r] += m(r,i) * v[i];
        }
    }

    return res;
}


template <class T, class U>
inline vector< NCBI_PROMOTE(T,U) >
operator* (const vector<T>& v, const CNcbiMatrix<U>& m)
{
    _ASSERT(m.GetRows() == v.size());

    typedef NCBI_PROMOTE(T,U) TPromote;
    vector<TPromote> res(m.GetCols(), TPromote(0));

    for (size_t c = 0;  c < m.GetCols();  ++c) {
        for (size_t i = 0;  i < m.GetRows();  ++i) {
            res[c] += m(i,c) * v[i];
        }
    }

    return res;
}


template <class T, class U>
inline CNcbiMatrix< NCBI_PROMOTE(T,U) >
operator* (const CNcbiMatrix<T>& m0, const CNcbiMatrix<U>& m1)
{
    _ASSERT(m0.GetCols() == m1.GetRows());

    typedef NCBI_PROMOTE(T,U) TPromote;
    CNcbiMatrix< TPromote > res(m0.GetRows(), m1.GetCols(), TPromote(0));

#if 1
    for (size_t r = 0; r < m0.GetRows();  ++r) {
        for (size_t c = 0;  c < m1.GetCols();  ++c) {
            for (size_t i = 0;  i < m0.GetCols();  ++i) {
                res(r,c) += m0(r,i) * m1(i,c);
            }
        }
    }
#endif


#if 0
    const vector<T>& lhs = m0.GetData();
    const vector<U>& rhs = m1.GetData();
    size_t col_mod = m0.GetCols() % 4;

    for (size_t r = 0;  r < m0.GetRows();  ++r) {
        size_t r_offs = r * m0.GetCols();

        for (size_t c = 0;  c < m1.GetCols();  ++c) {
            size_t i=0;
            T t0 = 0;
            T t1 = 0;
            T t2 = 0;
            T t3 = 0;

            switch(col_mod) {
            default:
            case 0:
                break;

            case 3:
                t0 += m0.GetData()[r_offs + 2] * m1(2,c);
                ++i;
            case 2:
                t0 += m0.GetData()[r_offs + 1] * m1(1,c);
                ++i;
            case 1:
                t0 += m0.GetData()[r_offs + 0] * m1(0,c);
                ++i;
                break;
            }

            for (;  i < m0.GetCols();  i += 2) {
                t0 += lhs[r_offs + i + 0] * m1(i + 0, c);
                t1 += lhs[r_offs + i + 1] * m1(i + 1, c);
                //t2 += lhs[r_offs + i + 2] * m1(i + 2, c);
                //t3 += lhs[r_offs + i + 3] * m1(i + 3, c);
            }
            res(r,c) = t0 + t1 + t2 + t3;
        }
    }
#endif

    return res;
}


template <class T, class U>
inline CNcbiMatrix<T>&
operator*= (CNcbiMatrix<T>& m0, const CNcbiMatrix<U>& m1)
{
    _ASSERT(m0.GetCols() == m1.GetRows());

    m0 = m0 * m1;
    return m0;
}


///
/// comparators
///

template <class T, class U>
inline bool
operator== (const CNcbiMatrix<T>& m0, const CNcbiMatrix<U>& m1)
{
    if (m0.GetRows() != m1.GetRows()) {
        return false;
    }
    if (m0.GetCols() != m1.GetCols()) {
        return false;
    }

    for (size_t i = 0;  i < m0.GetData().size();  ++i) {
        if (m0[i] != m1[i]) {
            return false;
        }
    }

    return true;
}


template <class T, class U>
inline bool
operator!= (const CNcbiMatrix<T>& m0, const CNcbiMatrix<U>& m1)
{
    return !(m0 == m1);
}


///
/// stream output
template <class T>
inline CNcbiOstream& operator<<(CNcbiOstream& os, const CNcbiMatrix<T>& M)
{
    for (size_t i = 0;  i < M.GetRows();  ++i) {
        for (size_t j = 0;  j < M.GetCols();  ++j) {
            if (j > 0) {
                os << " ";
            }
            os << M(i, j);
        }
        os << NcbiEndl;
    }
    return os;
}


///
/// stream input
template <class T>
inline CNcbiIstream& operator>>(CNcbiIstream& is, CNcbiMatrix<T>& M)
{
    CNcbiMatrix<T> A;
    string line;
    vector<T> row;
    T entry;
    int linenum = 0;
    while(getline(is, line)) {
        linenum++;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        CNcbiIstrstream iss(line.c_str(), line.size());
        row.clear();
        while(1) {
            iss >> entry;
            if (!iss) {
                break;
            }
            row.push_back(entry);
        }
        if (A.GetCols() == 0) {
            A.Resize(A.GetCols(), row.size());
        }
        if (row.size() != A.GetCols()) {
            NCBI_THROW(CException, eUnknown,
                       "error at line " +
                       NStr::IntToString(linenum) + ": expected " +
                       NStr::IntToString(A.GetCols()) + " columns; got" +
                       NStr::IntToString(row.size()));
        }
        A.Resize(A.GetRows() + 1, A.GetCols());
        for (int i = 0;  i < A.GetCols();  ++i) {
            A(A.GetRows() - 1, i) = row[i];
        }
    }
    M.Swap(A);
    return is;
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/08/19 13:11:19  dicuccio
 * ncbistr --> ncbistd for broader coverage of NCBI definitions
 *
 * Revision 1.6  2004/03/11 17:31:42  jcherry
 * Sped up adding of rows (greatly speeds reading from streams)
 *
 * Revision 1.5  2004/03/10 14:13:29  dicuccio
 * Corrected include - use ncbistr instead of ncbistre
 *
 * Revision 1.4  2004/02/10 21:17:54  jcherry
 * Made operator>> work for a matrix of any contained type that defines
 * operator>> (not just doubles)
 *
 * Revision 1.3  2004/02/10 14:29:51  jcherry
 * Removed wayward use of protected data in operator>>
 *
 * Revision 1.2  2004/02/10 14:00:40  dicuccio
 * Added comments.  Fixed spacing issues.  Changed extraction operator to be
 * non-friend function
 *
 * Revision 1.1  2004/02/09 17:34:11  dicuccio
 * Moved from gui/math
 *
 * Revision 1.4  2004/02/02 15:55:49  jcherry
 * Added Swap() method and stream reader
 *
 * Revision 1.3  2004/01/29 22:38:03  jcherry
 * Fixed typo
 *
 * Revision 1.2  2004/01/29 21:30:17  jcherry
 * Added stream output operator
 *
 * Revision 1.1  2004/01/23 13:54:15  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // UTIL_MATH___MATRIX__HPP
