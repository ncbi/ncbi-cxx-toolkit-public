#ifndef UTIL_NCBITABLE__HPP
#define UTIL_NCBITABLE__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Anatoliy Kuznetsov
*
* File Description:
*   NCBI table
*
*/
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

template<class TValue, class TRow, class TColumn>
class CNcbiTable
{
public:
    typedef map<TRow,    unsigned int>    TRowMap;
    typedef map<TColumn, unsigned int>    TColumnMap;
    typedef TValue                        TValueType;

    typedef vector<TValue>    TRowType;

public:
    CNcbiTable();
    CNcbiTable(unsigned int rows, unsigned int cols);
    ~CNcbiTable();

    unsigned int Rows() const;
    unsigned int Cols() const;

    void AddColumn(const TColumn& col);
    void AddRow(const TRow& row);

    void AssociateRow(const TRow& row, unsigned int row_idx);
    void AssociateColumn(const TColumn& col, unsigned int col_idx);

    void Resize(unsigned int rows, 
                unsigned int cols);

    void Resize(unsigned int rows, 
                unsigned int cols,
                const TValue& v);

    /// Get table row
    const TRowType& GetRow(const TRow& row) const;
    /// Get table row
    const TRowType& GetRowVector(unsigned int row_idx) const;
    TRowType& GetRowVector(unsigned int row_idx);


    const TValueType& GetElement(const TRow& row, const TColumn& col) const;
    TValueType& GetElement(const TRow& row, const TColumn& col);


    const TValue& operator()(const TRow& row, const TColumn& col) const
    {
        return GetElement(row, col);
    }
    TValue& operator()(const TRow& row, const TColumn& col)
    {
        return GetElement(row, col);
    }

    unsigned int ColumnIdx(const TColumn& col) const;
    unsigned int RowIdx(const TRow& row) const;

    /// Access table element by index
    const TValueType& GetCell(unsigned int row_idx, unsigned int col_idx) const;

    /// Access table element by index
    TValueType& GetCell(unsigned int row_idx, unsigned int col_idx);

protected:
    typedef vector<TRowType*>   TRowCollection;

protected:
    unsigned int    m_Rows;      ///< Number of rows
    unsigned int    m_Cols;      ///< Number of columns

    TRowMap         m_RowMap;     ///< Row name to index
    TColumnMap      m_ColumnMap;  ///< Column name to index

    TRowCollection  m_Table;
};

/////////////////////////////////////////////////////////////////////////////
//
//  CNcbiTable<TValue, TRow, TColumn>
//

template<class TValue, class TRow, class TColumn>
CNcbiTable<TValue, TRow, TColumn>::CNcbiTable()
: m_Rows(0),
  m_Cols(0)
{
}

template<class TValue, class TRow, class TColumn>
CNcbiTable<TValue, TRow, TColumn>::CNcbiTable(unsigned int rows, 
                                              unsigned int cols)
: m_Rows(rows),
  m_Cols(cols)
{
    m_Table.reserve(m_Rows);
    for (unsigned int i = 0; i < m_Rows; ++i) {
        TRowType* r = new TRowType(m_Cols);
        m_Table.push_back(r);  
    }
}

template<class TValue, class TRow, class TColumn>
CNcbiTable<TValue, TRow, TColumn>::~CNcbiTable()
{
    NON_CONST_ITERATE(typename TRowCollection, it, m_Table) {
        TRowType* r = *it;
        delete r;
    }
}

template<class TValue, class TRow, class TColumn>
unsigned int CNcbiTable<TValue, TRow, TColumn>::Rows() const
{
    return m_Rows;
}

template<class TValue, class TRow, class TColumn>
unsigned int CNcbiTable<TValue, TRow, TColumn>::Cols() const
{
    return m_Cols;
}

template<class TValue, class TRow, class TColumn>
void CNcbiTable<TValue, TRow, TColumn>::AddColumn(const TColumn& col)
{
    unsigned int cidx = m_Cols;
    AssociateColumn(col, cidx);
    NON_CONST_ITERATE(typename TRowCollection, it, m_Table) {
        TRowType* r = *it;
        r->push_back(TValue());
    }
    ++m_Cols;
}

template<class TValue, class TRow, class TColumn>
void CNcbiTable<TValue, TRow, TColumn>::AddRow(const TRow& row)
{
    unsigned int ridx = m_Rows;
    TRowType* r = new TRowType(m_Cols);
    m_Table.push_back(r);

    AssociateRow(row, ridx);
    ++m_Rows;
}

template<class TValue, class TRow, class TColumn>
void CNcbiTable<TValue, TRow, TColumn>::AssociateRow(const TRow&  row, 
                                                     unsigned int row_idx)
{
    typename TRowMap::const_iterator it = m_RowMap.find(row);
    if (it == m_RowMap.end()) {
        m_RowMap.insert(pair<TRow, unsigned int>(row, row_idx));
    } else {
        _ASSERT(0); // Row key already exists
    }
}

template<class TValue, class TRow, class TColumn>
void CNcbiTable<TValue, TRow, TColumn>::AssociateColumn(const TColumn& col, 
                                                        unsigned int   col_idx)
{
    typename TColumnMap::const_iterator it = m_ColumnMap.find(col);
    if (it == m_ColumnMap.end()) {
        m_ColumnMap.insert(pair<TColumn, unsigned int>(col, col_idx));
    } else {
        _ASSERT(0); // Column key already exists
    }

}

template<class TValue, class TRow, class TColumn>
void CNcbiTable<TValue, TRow, TColumn>::Resize(unsigned int rows, 
                                               unsigned int cols)
{
    m_Rows = rows;

    if (rows < m_Rows) {
        m_Table.resize(rows);
        if (m_Cols == cols)
            return; // nothing to do
    } else {
        m_Table.resize(rows, 0);
    }
    m_Cols = cols;

    NON_CONST_ITERATE(typename TRowCollection, it, m_Table) {
        TRowType* r = *it;
        if (r == 0) {  // new row
            r = new TRowType();
            r->resize(cols);
            *it = r;
        } else {
            if (r->size() != cols) { // resize required
                r->resize(cols);
            }
        }
    }
}

template<class TValue, class TRow, class TColumn>
void CNcbiTable<TValue, TRow, TColumn>::Resize(unsigned int  rows, 
                                               unsigned int  cols,
                                               const TValue& v)
{
    m_Rows = rows;

    if (rows < m_Rows) {
        m_Table.resize(rows);
        if (m_Cols == cols)
            return; // nothing to do
    } else {
        m_Table.resize(rows, 0);
    }
    m_Cols = cols;

    NON_CONST_ITERATE(typename TRowCollection, it, m_Table) {
        TRowType* r = *it;
        if (r == 0) {  // new row
            r = new TRowType();
            r->resize(cols, v);
            *it = r;
        } else {
            if (r->size() != cols) { // resize required
                r->resize(cols, v);
            }
        }
    }
}


template<class TValue, class TRow, class TColumn>
const typename CNcbiTable<TValue, TRow, TColumn>::TRowType& 
CNcbiTable<TValue, TRow, TColumn>::GetRow(const TRow& row) const
{
    typename TRowMap::const_iterator it = m_RowMap.find(row);
    if (it == m_RowMap.end()) {
        _ASSERT(0);
    } 
    unsigned int idx = it->second;
    return *(m_Table[idx]);
}

template<class TValue, class TRow, class TColumn>
const typename CNcbiTable<TValue, TRow, TColumn>::TRowType& 
CNcbiTable<TValue, TRow, TColumn>::GetRowVector(unsigned int row_idx) const
{
    return *(m_Table[row_idx]);
}

template<class TValue, class TRow, class TColumn>
typename CNcbiTable<TValue, TRow, TColumn>::TRowType& 
CNcbiTable<TValue, TRow, TColumn>::GetRowVector(unsigned int row_idx)
{
    return *(m_Table[row_idx]);
}


template<class TValue, class TRow, class TColumn>
const typename CNcbiTable<TValue, TRow, TColumn>::TValueType& 
CNcbiTable<TValue, TRow, TColumn>::GetElement(const TRow& row, const TColumn& col) const
{
    unsigned int ridx = RowIdx(row);
    unsigned int cidx = ColumnIdx(col);

    return GetCell(ridx, cidx);
}

template<class TValue, class TRow, class TColumn>
typename CNcbiTable<TValue, TRow, TColumn>::TValueType& 
CNcbiTable<TValue, TRow, TColumn>::GetElement(const TRow& row, const TColumn& col)
{
    unsigned int ridx = RowIdx(row);
    unsigned int cidx = ColumnIdx(col);

    return GetCell(ridx, cidx);
}


template<class TValue, class TRow, class TColumn>
unsigned int 
CNcbiTable<TValue, TRow, TColumn>::ColumnIdx(const TColumn& col) const
{
    typename TColumnMap::const_iterator it = m_ColumnMap.find(col);
    if (it == m_ColumnMap.end()) {
        _ASSERT(0);
    }
    return it->second;
}

template<class TValue, class TRow, class TColumn>
unsigned int 
CNcbiTable<TValue, TRow, TColumn>::RowIdx(const TRow& row) const
{
    typename TRowMap::const_iterator it = m_RowMap.find(row);
    if (it == m_RowMap.end()) {
        _ASSERT(0);
    }
    return it->second;
}

template<class TValue, class TRow, class TColumn>
const typename CNcbiTable<TValue, TRow, TColumn>::TValueType& 
CNcbiTable<TValue, TRow, TColumn>::GetCell(unsigned int row_idx, 
                                           unsigned int col_idx) const
{
    const TRowType& r = *(m_Table[row_idx]);
    return r[col_idx];
}

template<class TValue, class TRow, class TColumn>
typename CNcbiTable<TValue, TRow, TColumn>::TValueType& 
CNcbiTable<TValue, TRow, TColumn>::GetCell(unsigned int row_idx, 
                                           unsigned int col_idx)
{
    TRowType& r = *(m_Table[row_idx]);
    return r[col_idx];
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/09/01 13:04:03  kuznets
 * Use typename to make GCC happy
 *
 * Revision 1.1  2004/09/01 12:27:54  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif  /* UTIL_NCBITABLE__HPP */
