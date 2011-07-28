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
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XUTIL_EXPORT CNcbiTable_Exception : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// Exception types
    enum EErrCode {
        eRowNotFound,          ///< Row not found
        eColumnNotFound,       ///< Column not found
        eRowAlreadyExists,     ///< Row id has been assigned before
        eColumnAlreadyExists   ///< Column id has been assigned before
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CNcbiTable_Exception, CException);
};



/// Template class to create a table with custom row-column access
///
/// Table provides two access modes one is using custom types 
/// associated with rows and columns and another using rows and columns
/// integer indexes.
///
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
    /// Construction.
    /// Creates row x cols table without any row-column association
    CNcbiTable(unsigned int rows, unsigned int cols);

    CNcbiTable(const CNcbiTable& table);
    CNcbiTable& operator=(const CNcbiTable& table)
    {
        x_Free();
        x_Copy();
    }

    ~CNcbiTable();


    /// Number of rows
    unsigned int Rows() const;
    /// Number of column
    unsigned int Cols() const;

    /// Add column to the table, column recieves name "col"
    void AddColumn(const TColumn& col);
    /// Add row to the table, column recieves name "row"
    void AddRow(const TRow& row);

    /// Set up row name
    void AssociateRow(const TRow& row, unsigned int row_idx);
    /// Set up column name
    void AssociateColumn(const TColumn& col, unsigned int col_idx);

    /// Change table size
    void Resize(unsigned int rows, 
                unsigned int cols);

    /// Change table size, new table elements initialized with value
    void Resize(unsigned int rows, 
                unsigned int cols,
                const TValue& v);

    /// Get table row
    const TRowType& GetRow(const TRow& row) const;
    /// Get table row by row index
    const TRowType& GetRowVector(unsigned int row_idx) const;
    /// Get table row by row index
    TRowType& GetRowVector(unsigned int row_idx);

    /// Get table element
    const TValueType& GetElement(const TRow& row, const TColumn& col) const;
    /// Get table element
    TValueType& GetElement(const TRow& row, const TColumn& col);

    /// Get table element
    const TValue& operator()(const TRow& row, const TColumn& col) const
    {
        return GetElement(row, col);
    }
    /// Get table element
    TValue& operator()(const TRow& row, const TColumn& col)
    {
        return GetElement(row, col);
    }

    /// Get column index
    unsigned int ColumnIdx(const TColumn& col) const;
    /// Get row index
    unsigned int RowIdx(const TRow& row) const;

    /// Get column name
    const TColumn& Column(unsigned int idx) const;
    /// Get row name
    const TRow& Row(unsigned int idx) const;

    /// Access table element by index
    const TValueType& GetCell(unsigned int row_idx, unsigned int col_idx) const;

    /// Access table element by index
    TValueType& GetCell(unsigned int row_idx, unsigned int col_idx);
protected:

    void x_Free();
    void x_Copy(const CNcbiTable& table);

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
CNcbiTable<TValue, TRow, TColumn>::CNcbiTable(
                 const CNcbiTable<TValue, TRow, TColumn>& table)
{
    x_Copy(table);
}

template<class TValue, class TRow, class TColumn>
void CNcbiTable<TValue, TRow, TColumn>::x_Copy(const CNcbiTable& table)
{
    m_Rows = table.Rows();
    m_Cols = table.Cols();

    m_Table.resize(0);
    m_Table.reserve(m_Rows);
    for (unsigned int i = 0; i < m_Rows; ++i) {
        const TRowType& src_row = table.GetRowVector(i);
        TRowType* r = new TRowType(src_row);
        m_Table.push_back(r);  
    }

    m_RowMap = table.m_RowMap;
    m_ColumnMap = table.m_ColumnMap;
}


template<class TValue, class TRow, class TColumn>
CNcbiTable<TValue, TRow, TColumn>::~CNcbiTable()
{
    x_Free();
}

template<class TValue, class TRow, class TColumn>
void CNcbiTable<TValue, TRow, TColumn>::x_Free()
{
    NON_CONST_ITERATE(typename TRowCollection, it, m_Table) {
        TRowType* r = *it;
        delete r;
    }
    m_Table.resize(0);
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
        NCBI_THROW(
          CNcbiTable_Exception, 
          eRowAlreadyExists, "Cannot assign row key (already assigned).");
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
        NCBI_THROW(
          CNcbiTable_Exception, 
          eRowAlreadyExists, "Cannot assign column key (already assigned).");
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
        NCBI_THROW(
          CNcbiTable_Exception, 
          eRowNotFound, "Row not found.");
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
        NCBI_THROW(
          CNcbiTable_Exception, 
          eColumnNotFound, "Column not found.");
    }
    return it->second;
}

template<class TValue, class TRow, class TColumn>
unsigned int 
CNcbiTable<TValue, TRow, TColumn>::RowIdx(const TRow& row) const
{
    typename TRowMap::const_iterator it = m_RowMap.find(row);
    if (it == m_RowMap.end()) {
        NCBI_THROW(
          CNcbiTable_Exception, 
          eRowNotFound, "Row not found.");
    }
    return it->second;
}

template<class TValue, class TRow, class TColumn>
const TColumn&
CNcbiTable<TValue, TRow, TColumn>::Column(unsigned int idx) const
{
    typename TColumnMap::const_iterator it = m_ColumnMap.begin();
    for(it=m_ColumnMap.begin(); it!=m_ColumnMap.end(); ++it) {
        if ( (*it).second == idx )
            return (*it).first;
    }

    NCBI_THROW(CNcbiTable_Exception, eColumnNotFound, "Column not found.");   
}

template<class TValue, class TRow, class TColumn>
const TRow&
CNcbiTable<TValue, TRow, TColumn>::Row(unsigned int idx) const
{
    typename TRowMap::const_iterator it = m_RowMap.begin();
    for(it=m_RowMap.begin(); it!=m_RowMap.end(); ++it) {
        if ( (*it).second == idx )
            return (*it).first;
    }

    NCBI_THROW(CNcbiTable_Exception, eRowNotFound, "Row not found.");  
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

#endif  /* UTIL_NCBITABLE__HPP */
