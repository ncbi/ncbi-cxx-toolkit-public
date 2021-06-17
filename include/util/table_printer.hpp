#ifndef UTIL___TABLE_PRINTER__HPP
#define UTIL___TABLE_PRINTER__HPP

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
* Author: Michael Kornbluh
*
* File Description:
*   Prints out neatly-aligned ASCII tables.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiobj.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE

/// This can be used to lay out neat ASCII data.
/// Example:<br>
/// <pre>
/// --------------------   --------
/// Name                   ZIP code
/// --------------------   --------
/// Pat Doe                22801   
/// Sammy Smith            20852   
/// Chris Doe              08361   
/// --------------------   --------
/// </pre>
/// Here is example code that would let you lay out such a table
/// into cout:
/// <pre>
/// CTablePrinter::SColInfoVec vecColInfo;
/// vecColInfo.AddCol("Name", 20);
/// vecColInfo.AddCol("ZIP code", 5);
/// 
/// typedef CTablePrinter::SEndOfCell SEndOfCell;
///
/// CTablePrinter table_printer(vecColInfo, cout);
/// 
/// table_printer << "Pat" << ' ' << "Doe" << SEndOfCell();
/// table_printer << "22801" << SEndOfCell();
/// 
/// table_printer << "Sammy Smith" << SEndOfCell();
/// table_printer << "20852" << SEndOfCell();
/// 
/// table_printer << "Chris Doe" << SEndOfCell();
/// table_printer << "08361" << SEndOfCell();
/// 
/// table_printer.FinishTable();
/// </pre>
class NCBI_XUTIL_EXPORT CTablePrinter {
public:

    /// controls how table should behave when a data cell
    /// is too long to fit into its column.
    enum EDataTooLong {
        /// The data will be replaced with some sort of
        /// error message, or at least question marks if the error
        /// message wouldn't fit.
        eDataTooLong_ShowErrorInColumn,
        /// The data is truncated with ellipses (that is, "...").
        /// This is NOT recommended for numeric
        /// data because a truncated number can look like a
        /// different number if the viewer doesn't notice the
        /// ellipses.
        eDataTooLong_TruncateWithEllipses,
        /// This recklessly prints the whole table data, regardless of
        /// formatting corruption and regardless of how long
        /// the data is.  
        eDataTooLong_ShowWholeData,
        /// Throws an exception when the data won't fit.
        eDataTooLong_ThrowException,

        /// Default behavior.
        eDataTooLong_Default = eDataTooLong_ShowErrorInColumn
    };
    /// controls how the data in each column is justified.  
    /// As a rough rule of thumb, numeric data should be right-justified
    /// and non-numeric data should be left-justified.
    enum EJustify {
        eJustify_Left,
        eJustify_Right
    };

    /// This structure contains info about a given column
    struct NCBI_XUTIL_EXPORT SColInfo {
        SColInfo(const string & sColName,
        Uint4 iColWidth,
        EJustify eJustify = eJustify_Left,
        EDataTooLong eDataTooLong = eDataTooLong_ShowErrorInColumn ) :
        m_sColName(sColName), m_iColWidth(iColWidth),
        m_eJustify(eJustify), m_eDataTooLong(eDataTooLong) { }

        /// The name of the column, which is shown in the header
        string       m_sColName;
        /// How many characters wide the column is
        Uint4        m_iColWidth;
        /// Justification of data cells in this column
        /// (the name of the column is always left-justified, though)
        EJustify     m_eJustify;
        /// How data cells should behave in this column behave when
        /// there is too much data to fit in them.
        EDataTooLong m_eDataTooLong;
    };
    /// This holds the info about all columns for the table.
    /// Call AddCol() until all the columns you want are added, then
    /// use it to create a CTablePrinter.
    struct NCBI_XUTIL_EXPORT SColInfoVec {
        void AddCol(const string & sColName, 
            Uint4 iColWidth = 0,
            EJustify eJustify = eJustify_Left,
            EDataTooLong eDataTooLong = eDataTooLong_Default );

        typedef vector<SColInfo> TColInfoVec;
        /// The underlying column info vector for which SColInfoVec
        /// is a wrapper.
        TColInfoVec m_colInfoVec;
    };

    /// @param vecColInfo
    ///   This holds information about all the columns that will
    ///   be shown in the table.
    /// @param ostrm
    ///   This is the output stream to which the table will be written
    /// @param sColumnSeparator
    ///   Default should be fine for most purposes, but the caller
    ///   can separate columns with something else (for example, " | ").
    CTablePrinter(
        const SColInfoVec & vecColInfo, ostream & ostrm,
        const string & sColumnSeparator = "   ");

    /// Destructor finishes the table if it's not already finished.
    /// See FinishTable().
    ~CTablePrinter(void) {
        FinishTable();
    }

    /// Stream an instance of this object into the CTablePrinter
    /// to have it write out the current table cell and prepare for the
    /// next one.  See the documentation of this class for more details.
    struct SEndOfCell {
        int dummy; // in case compiler doesn't like empty classes
    };

    /// This is just a helper for the global "operator <<" function
    /// for writing into the table.
    template<class TValue>
    CTablePrinter & StreamToCurrentCell (
        const TValue & value)
    {
        m_NextCellContents << value;
        return *this;
    }

    /// This writes the contents of the current cell and
    /// prepares for the next one.  This is really just a helper
    /// for the "operator <<" that accepts SEndOfCell.
    CTablePrinter & EndOfCurrentCell(void)
    {
        // reset m_NextCellContents early in case
        // x_AddCellValue throws.
        const string sNextCellContents( m_NextCellContents.str() );
        m_NextCellContents.str(kEmptyStr);

        // might throw
        x_AddCellValue(sNextCellContents);

        return *this;
    }

    /// If the table is not already finished, this
    /// finishes it by writing the closing row (which
    /// is usually a series of dashes).
    /// If cells are added after this point, a new header will
    /// be printed and a new table will be written.
    void FinishTable(void);

private:
    /// This keeps track of the state of the table writer.
    enum EState {
        /// This means the header row has NOT been printed yet.
        eState_Initial,
        /// This means the header row HAS been printed, and data
        /// rows are being printed.
        eState_PrintingRows,
    };
    /// This keeps track of the state of the table writer.
    EState m_eState;
    
    /// The info about columns of the table
    SColInfoVec  m_vecColInfo;
    /// The ostream to which the table is written
    ostream &    m_ostrm;
    /// The 0-based index of the column that the next AddCell will use.
    Uint4        m_iNextCol;
    /// The text that separates columns (both in the header as well as dat).
    const string m_sColumnSeparator;

    /// The contents of the current table cell are accumulated
    /// in here.
    stringstream m_NextCellContents;

    /// forbid copy 
    CTablePrinter(const CTablePrinter &);
    /// forbid assignment
    CTablePrinter & operator = (const CTablePrinter &);

    /// This writes a row of dashes which starts the table,
    /// separates the header from the data, and ends the table.
    void x_PrintDashes(void);
    /// This prints the column names, appropriately spaced and separated.
    void x_PrintColumnNames(void);

    /// This is the underlying logic to add another cell to the table data.
    void x_AddCellValue(const string & sValue);
};

// inline functions

    /// Writes object to current table cell.
    template<typename TValue>
    inline
    CTablePrinter & operator << (
        CTablePrinter & table_printer, const TValue & value) 
    {
        return table_printer.StreamToCurrentCell(value);
    }

    /// Flushes table cell contents and prepares for the next cell.
    template<>
    inline
    CTablePrinter & operator << <CTablePrinter::SEndOfCell>(
        CTablePrinter & table_printer, const CTablePrinter::SEndOfCell & /*end_of_cell*/) 
    {
        return table_printer.EndOfCurrentCell();
    }


END_NCBI_SCOPE

#endif /* UTIL___TABLE_PRINTER__HPP */
