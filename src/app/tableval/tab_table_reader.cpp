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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for structured comments for sequences
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <util/line_reader.hpp>

#include "tab_table_reader.hpp"

#include <misc/xmlwrapp/xmlwrapp.hpp>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void CTabDelimitedValidator::_Validate(int col_number, const CTempString& value)
{
    //const CTempString& datatype = m_col_defs[col_number];
}

void CTabDelimitedValidator::_ReportError(int col_number, const CTempString& error)
{
    CTabDelimitedValidatorError rec;
    rec.m_col = col_number;
    rec.m_row = m_current_row_number;
    rec.m_msg = error;
    m_errors.push_back(rec);
}

void CTabDelimitedValidator::ValidateInput(ILineReader& reader, const CTempString& default_collumns,
    const CTempString& required, const CTempString& ignored)
{
    m_current_row_number = 0;
    m_delim = (m_flags & e_tab_comma_delim) ? "," : "\t";

    if (_ProcessHeader(reader, default_collumns))
    {
        // preprocess headers & required & ignored
        if (_MakeColumns("Required", required, m_required_cols) &&
            _MakeColumns("Ignored", ignored, m_ignored_cols))
            _OperateRows(reader);
    }

    _GenerateReport();
}

void CTabDelimitedValidator::_ReportTab()
{
    if (!m_errors.empty())
        *m_out_stream << "Row" << "\t" << "Column" << "\t" << "Error" << endl;

    ITERATE(list<CTabDelimitedValidatorError>, it, m_errors)
    {
        *m_out_stream << it->m_row << "\t" << it->m_col << "\t" << it->m_msg.c_str() << endl;
    }
}

void CTabDelimitedValidator::_ReportXML()
{
    if (m_errors.empty())
        return;

    xml::document xmldoc("tab_delimited_validator");
    xml::node& root = xmldoc.get_root_node();

    int i=0;
    ITERATE(list<CTabDelimitedValidatorError>, it, m_errors)
    {
        xml::node new_node("error");
        new_node.get_attributes().insert("row", NStr::IntToString(it->m_row).c_str());
        new_node.get_attributes().insert("column", NStr::IntToString(it->m_row).c_str());
        new_node.get_attributes().insert("message", it->m_msg.c_str()); // will be handled correctly
        root.insert(new_node);

        i++;
        if (i>100) 
            break;
    }

    xmldoc.set_is_standalone(true);
    xmldoc.set_encoding("utf-8");
    *m_out_stream << xmldoc;

}

void CTabDelimitedValidator::_GenerateReport()
{
    if ( (m_flags & CTabDelimitedValidator::e_tab_tab_report) == CTabDelimitedValidator::e_tab_tab_report)
    {
        _ReportTab();
    }
    else
    if ( (m_flags & CTabDelimitedValidator::e_tab_xml_report) == CTabDelimitedValidator::e_tab_xml_report)
    {
        _ReportXML();
    }
}

bool CTabDelimitedValidator::_MakeColumns(const string& message, const CTempString& collumns, vector<bool>& col_defs)
{
    col_defs.resize(m_col_defs.size(), false); // all values are not required
    vector<CTempStringEx> names;
    NStr::Tokenize(collumns, ",", names);
    bool can_process = true;
    for (size_t i=0; i<names.size(); i++)
    {
        int index = NStr::StringToInt(names[i], NStr::fConvErr_NoThrow);
        if (index == 0)
        {
            vector<CTempStringEx>::const_iterator col_it = find(m_col_defs.begin(), m_col_defs.end(), names[i]);
            if (col_it == m_col_defs.end())
            {
                _ReportError(i, message + " column does not exist");
                // stop processing
                can_process = false;
            }                   
            else
                col_defs[index = col_it - m_col_defs.begin()] = true;
        }
        else
        if (index >= m_col_defs.size() || index<0)
        {
            _ReportError(index, message + " column does not exist");
            // stop processing
            can_process = false;
        }
        else
            col_defs[index + 1] = true;
    }
    return can_process;
}

bool CTabDelimitedValidator::_ProcessHeader(ILineReader& reader, const CTempString& default_collumns)
{
    if (!reader.AtEOF())
    {
        if (m_flags & e_tab_noheader)
        {
            NStr::Tokenize(default_collumns, ",", m_col_defs); //using comma separator always
        }
        else
        {
            reader.ReadLine();
            // First line is a collumn definitions
            m_current_row_number = reader.GetLineNumber();

            NStr::Tokenize(reader.GetCurrentLine(), m_delim, m_col_defs);
        }

        if (m_col_defs.size()<1)
        {
            _ReportError(1, "No columns specified");            
            return false;
        }

        return true;
    }
    return false;
}

void CTabDelimitedValidator::_OperateRows(ILineReader& reader)
{
    while (!reader.AtEOF())
    {
        reader.ReadLine();
        // First line is a collumn definitions
        CTempString current = reader.GetCurrentLine();
        m_current_row_number = reader.GetLineNumber();

        if (current.empty())
        {
            if (m_flags & e_tab_ignore_empty_rows)
                continue;
            else
                _ReportError(0, "Empty rows not allowed");
        }
        else
        {
            vector<CTempStringEx> values;
            NStr::Tokenize(current, m_delim, values);

            if (values.size() > m_col_defs.size())
                _ReportError(m_col_defs.size(), "To many values");

            for (size_t i=0; i<m_col_defs.size(); i++)
            {
                if (i>=values.size() || values[i].empty())
                {
                    if (m_required_cols[i])
                        _ReportError(i, "Missing required value");
                }
                else
                if (!m_ignored_cols[i])
                {
                   _Validate(i, values[i]);
                }
            }
        }
    }
}

END_NCBI_SCOPE

