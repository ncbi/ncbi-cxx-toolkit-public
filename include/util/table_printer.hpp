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
/// CTablePrinter table_printer(vecColInfo, cout);
/// 
/// *table_printer.AddCell() << "Pat" << ' ' << "Doe";
/// *table_printer.AddCell() << "22801";
/// 
/// *table_printer.AddCell() << "Sammy Smith";
/// *table_printer.AddCell() << "20852";
/// 
/// *table_printer.AddCell() << "Chris Doe";
/// *table_printer.AddCell() << "08361";
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

    /// This is specifically designed to be a temporary object
    /// so that each series of ostream-related operators
    /// will store up and its destructor will then write out the table
    /// data.  Please see the example in the
    /// class comment. Outside users should not use this class.
    struct NCBI_XUTIL_EXPORT SCellStream : public CObject {
        typedef CObject TParent;
        /// Give this a reference to its parent table so it
        /// can add the cells to it.  Note the "friend" declaration
        /// farther below.
        SCellStream(CTablePrinter * pParentTable) 
            : m_pParentTable(pParentTable) { }

        /// Cell is printed after destruction of this SCellStream.
        /// Since printing can throw, we can't do the printing in
        /// the destructor per se.
        void DeleteThis(void) {
            // remember some information before this object is destroyed
            CTablePrinter * pParentTable = m_pParentTable;
            string sStringToPrint = CNcbiOstrstreamToString(m_cellValueStrm);

            // call parent's version of destructor
            // WARNING: Be careful not to reference any fields or methods
            // after this point, since the object is probably deleted.
            TParent::DeleteThis();

            // this can throw:
            pParentTable->x_AddCellValue(
                sStringToPrint );
        }

        /// SCellStream is not itself an ostream, so we
        /// have this template to return its internal m_cellValueStrm
        /// instead.
        template<typename TValue>
        ostream & operator << (const TValue & value) {
            return m_cellValueStrm << value; 
        }

        /// Pointer to parent.  Outside users should not touch this.
        CTablePrinter * m_pParentTable;
        /// This is where the cell's value is built.  Outside users
        /// should not directly touch this, and they should not need to
        /// anyway since there is an operator just above for reaching it
        /// cleanly.
        CNcbiOstrstream m_cellValueStrm;
    };
    /// Let SCellStream's dtor access x_AddCellValue
    friend SCellStream::~SCellStream();

    /// Call this once for each cell of table data you would like to add.
    /// Do NOT store the returned CRef anywhere because it's designed
    /// to be destroyed at the end of the statement so it can write
    /// its data.
    CRef<SCellStream> AddCell(void) {
        return Ref(new SCellStream(this));
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

END_NCBI_SCOPE

#endif /* UTIL___TABLE_PRINTER__HPP */

