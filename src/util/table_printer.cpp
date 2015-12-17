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
 * Author:  Michael Kornbluh
 *
 * File Description:  Prints out neatly-aligned ASCII tables.
 *
 */

#include <ncbi_pch.hpp>

#include <iterator>

#include <util/table_printer.hpp>

BEGIN_NCBI_SCOPE

void CTablePrinter::SColInfoVec::AddCol(
    const string & sColName, 
    Uint4 iColWidth,
    EJustify eJustify,
    EDataTooLong eDataTooLong )
{
    m_colInfoVec.push_back(SColInfo(
        sColName, 
        iColWidth,
        eJustify,
        eDataTooLong));
}

CTablePrinter::CTablePrinter(
    const SColInfoVec & vecColInfo, 
    ostream & ostrm,
    const string & m_sColumnSeparator)
    : m_eState(eState_Initial), 
      m_vecColInfo(vecColInfo), m_ostrm(ostrm), m_iNextCol(0),
      m_sColumnSeparator(m_sColumnSeparator)
{
    // if any column width is less than the length of the name of the column,
    // expand it
    NON_CONST_ITERATE(
        SColInfoVec::TColInfoVec, col_it, m_vecColInfo.m_colInfoVec) 
    {
        col_it->m_iColWidth =
            max<Uint4>(col_it->m_iColWidth, col_it->m_sColName.length());
    }
}

void CTablePrinter::FinishTable(void)
{
    switch(m_eState) {
    case eState_Initial:
        // nothing written, so nothing to finish
        break;
    case eState_PrintingRows:
        // close rows with dashes
        x_PrintDashes();
        m_eState = eState_Initial;
        break;
    default:
        NCBI_USER_THROW_FMT("Bad state: " << static_cast<int>(m_eState));
    }
}

void CTablePrinter::x_PrintDashes(void) {
    const string * pSep = &kEmptyStr;
    ITERATE(
        SColInfoVec::TColInfoVec, col_it, m_vecColInfo.m_colInfoVec) 
    {
        m_ostrm << *pSep;
        pSep = &m_sColumnSeparator;

        m_ostrm << string(col_it->m_iColWidth, '-');
    }
    m_ostrm << endl;
}

void CTablePrinter::x_PrintColumnNames(void) {
    const string * pSep = &kEmptyStr;
    ITERATE(
        SColInfoVec::TColInfoVec, col_it, m_vecColInfo.m_colInfoVec) 
    {
        m_ostrm << *pSep;
        pSep = &m_sColumnSeparator;

        m_ostrm << setw(col_it->m_iColWidth) << left
            << col_it->m_sColName;
    }
    m_ostrm << endl;
}

void CTablePrinter::x_AddCellValue(
    const string & sValue) 
{
    if( m_eState == eState_Initial ) {
        x_PrintDashes();
        x_PrintColumnNames();
        x_PrintDashes();
        m_eState = eState_PrintingRows;
    }

    const SColInfo & colInfo = 
        m_vecColInfo.m_colInfoVec[m_iNextCol];

    // write the value
    m_ostrm << setw(colInfo.m_iColWidth) 
        << ( colInfo.m_eJustify == eJustify_Left ? left : right);
    if( sValue.length() > colInfo.m_iColWidth ) {
        // handle a cell value that's too long
        switch(colInfo.m_eDataTooLong) {
        case eDataTooLong_ShowErrorInColumn: {
            static const char kErrMsg[] = "**ERROR**";
            static const size_t kErrMsgLen = sizeof(kErrMsg) - 1;
            if( colInfo.m_iColWidth >= kErrMsgLen ) {
                m_ostrm << kErrMsg;
            } else {
                m_ostrm << string(colInfo.m_iColWidth, '?');
            }
            break;
        }
        case eDataTooLong_TruncateWithEllipses:  {
            const static string kEllipses = "...";
            
            if( colInfo.m_iColWidth > kEllipses.length() ) {
                string::const_iterator value_end_it = sValue.end();
                value_end_it -= kEllipses.length();
                m_ostrm << setw(1);
                copy( sValue.begin(), value_end_it,
                    ostream_iterator<char>(m_ostrm) );
                m_ostrm << kEllipses;
            } else {
                // even ellipses won't fit
                m_ostrm << string(colInfo.m_iColWidth, '?');
            }
            break;
        }
        
        case eDataTooLong_ShowWholeData:
            // recklessly print the whole string, regardless of how
            // it may corrupt this row's formatting and regardless of
            // how long it might be.
            m_ostrm << sValue;
            break;
        case eDataTooLong_ThrowException:
        default:
            NCBI_USER_THROW_FMT(
                "CTablePrinter cannot fit cell data into allotted space.  "
                "Column name: " << colInfo.m_sColName 
                << ", Column width: " << colInfo.m_iColWidth
                << ", Length of oversized data: " << sValue.length() 
                << "Oversized data starts with: " 
                << sValue.substr(0, colInfo.m_iColWidth) << "...[snip]...");
        }
    } else {
        // no problem, so just write it
        m_ostrm << sValue;
    }

    // go to the next column
    ++m_iNextCol;
    if( m_iNextCol >= m_vecColInfo.m_colInfoVec.size() ) {
        m_iNextCol = 0;
        m_ostrm << endl;
    } else {
        m_ostrm << m_sColumnSeparator;
    }
}

END_NCBI_SCOPE

