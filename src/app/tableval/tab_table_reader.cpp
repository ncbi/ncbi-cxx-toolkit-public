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
#include "col_validator.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

bool CTabDelimitedValidator::_Validate(int col_number, const CTempString& value)
{
    const string& datatype = m_col_defs[col_number];
    if (datatype.empty())
        return false; 

    string error;
    bool isfatal = CColumnValidatorRegistry::GetInstance().
        DoValidate(datatype, value, error);

    if (!error.empty())
        _ReportError(col_number, error, datatype);

    return isfatal;
}

void CTabDelimitedValidator::_ReportWarning(int col_number, const CTempString& warning, const CTempString& colname)
{
    _ReportError(col_number, warning, colname, true);
}

void CTabDelimitedValidator::_ReportError(int col_number, const CTempString& error, const CTempString& colname, bool warning)
{
    CTabDelimitedValidatorMessage rec;
    rec.m_col = col_number;
    rec.m_row = m_current_row_number;
    rec.m_msg = error;
    rec.m_warning = warning;
    rec.m_colname = colname;
    m_errors.push_back(rec);
}

bool CTabDelimitedValidator::_CheckHeader(const string& discouraged, const vector<string>& require_one)
{
    bool columns_ok = true;
    vector<string> discouraged_cols;
    if (!discouraged.empty())
    {
        NStr::Split(discouraged, ",", discouraged_cols);
    }

    ITERATE(std::vector<std::string>, it, m_col_defs)
    {
        if (CColumnValidator::IsDiscouraged(*it) ||
            find(discouraged_cols.begin(), discouraged_cols.end(), *it) != discouraged_cols.end())
        {
            int id = it - m_col_defs.begin();
            _ReportError(id, "Column is discouraged", *it);
            m_ignored_cols.resize(m_col_defs.size());
            m_ignored_cols[id] = true;
        }
    }

    m_require_one_cols.resize(require_one.size());
    ITERATE(vector<string>, it, require_one)
    {
        set<string>& require_one_cols = m_require_one_cols[it - require_one.begin()];
        vector<string> cols;
        NStr::Split(*it, ",", cols);
        int found(0);
        ITERATE(vector<string>, it_col, cols)
        {
            require_one_cols.insert(*it_col);
            if (find(m_col_defs.begin(), m_col_defs.end(), *it_col) != m_col_defs.end())
            {
                found++;
            }
        }
        if (found == 0)
        {
           _ReportError(0, "Not found any of require-one columns", *it);
        }
    }

    return columns_ok;
}

void CTabDelimitedValidator::ValidateInput(ILineReader& reader, 
    const string& default_columns,
    const string& required, const string& ignored,
    const string& unique, const string& discouraged,
    const vector<string>& require_one)
{
    m_current_row_number = 0;
    m_delim = (m_flags & e_tab_comma_delim) ? "," : "\t";

    if (_ProcessHeader(reader, default_columns) && _CheckHeader(discouraged, require_one))
    {
        // preprocess headers & required & ignored
        if (_MakeColumns("Required", required, m_required_cols) &&
            _MakeColumns("Ignored", ignored, m_ignored_cols) &&
            _MakeColumns("Unique", unique, m_unique_cols))
        {          
            if (!unique.empty())
                m_unique_values.resize(m_col_defs.size());

            bool ignore_unknown = (m_flags & e_tab_ignore_unknown_types) == e_tab_ignore_unknown_types;
            map<string, int> types;
            bool fatal = false;
            for(size_t i=0; i<m_col_defs.size(); ++i)
            {
                if (m_ignored_cols[i])
                    continue;

                if (!CColumnValidatorRegistry::GetInstance().IsSupported(m_col_defs[i]))
                {
                    _ReportError(i, "Datatype is not supported", m_col_defs[i], ignore_unknown);
                    if (ignore_unknown)
                    {
                       m_ignored_cols[i] = true;
                       continue;
                    }
                    else
                       fatal = true;
                }

                int count = ++types[m_col_defs[i]];
                if (count == 2)
                {
                    // report only first occurance
                    _ReportError(i, "Column is not unique", m_col_defs[i]);
                    fatal = true;
                }
                
            }
            if (!fatal)
               _OperateRows(reader);
        }
    }
}

void CTabDelimitedValidator::_ReportTab(CNcbiOstream* out_stream)
{
    if (!m_errors.empty())
        *out_stream << "Row\tColumn\tError\tWarning" << endl;

    ITERATE(list<CTabDelimitedValidatorMessage>, it, m_errors)
    {
        *out_stream << it->m_row << "\t" << it->m_col + 1 << "\t"
            << it->m_colname << "\t"
            << (it->m_warning?"\t":it->m_msg.c_str())
            << (it->m_warning?it->m_msg.c_str(): "\t")
            << endl;
    }
}

void CTabDelimitedValidator::_ReportXML(CNcbiOstream* out_stream, bool no_headers)
{
    if (m_errors.empty())
        return;

    xml::document xmldoc("tab_delimited_validator");
    xml::node& root = xmldoc.get_root_node();

    int i=0;
    ITERATE(list<CTabDelimitedValidatorMessage>, it, m_errors)
    {
        xml::node new_node(it->m_warning?"warning":"error");
        new_node.get_attributes().insert("row", NStr::IntToString(it->m_row).c_str());
        new_node.get_attributes().insert("column", NStr::IntToString(it->m_col + 1).c_str());
        new_node.get_attributes().insert("message", it->m_msg.c_str()); 
        if (!it->m_colname.empty())
        {
            new_node.get_attributes().insert("colname", it->m_colname.c_str());
        }
        // will be handled correctly
        root.insert(new_node);

        i++;
        if (i>100) 
            break;
    }

    xmldoc.set_is_standalone(true);
    xmldoc.set_encoding("utf-8");
    *out_stream << xmldoc;

}

bool CTabDelimitedValidator::_MakeColumns(const string& message, const CTempString& columns, vector<bool>& col_defs)
{
    col_defs.resize(m_col_defs.size(), false); // all values are not required
    vector<CTempStringEx> names;
    NStr::Split(columns, ",", names);
    bool can_process = true;
    for (size_t i=0; i<names.size(); i++)
    {
        int index = NStr::StringToInt(names[i], NStr::fConvErr_NoThrow);
        if (index == 0)
        {
            vector<string>::const_iterator col_it = find(m_col_defs.begin(), m_col_defs.end(), names[i]);
            if (col_it == m_col_defs.end())
            {
                _ReportError(i, message + " column does not exist", names[i]);
                // stop processing
                can_process = false;
            }                   
            else
                col_defs[index = col_it - m_col_defs.begin()] = true;
        }
        else
        if (index > (int)m_col_defs.size() || index<1)
        {
            _ReportError(i, message + " column does not exist", names[i]);
            // stop processing
            can_process = false;
        }
        else
            col_defs[index - 1] = true;
    }
    return can_process;
}

bool CTabDelimitedValidator::_ProcessHeader(ILineReader& reader, const CTempString& default_columns)
{
    if (!default_columns.empty())
    {
        string lower = default_columns;
        NStr::ToLower(lower);
        NStr::Split(lower, ",", m_col_defs); //using comma separator always

        return true;
    }
    else
    {
        while (!reader.AtEOF())
        {
            // skip all comment lines
            reader.ReadLine();
            // First line is a column definitions
            m_current_row_number = reader.GetLineNumber();
            string lower = reader.GetCurrentLine();
            if (lower[0] == '#') continue;
            NStr::ToLower(lower);
            NStr::Split(lower, m_delim, m_col_defs);
            break;
        }

        if (m_col_defs.size()<1)
        {
            _ReportError(1, "No columns specified", "");            
            return false;
        }
    }
    return true;
}

void CTabDelimitedValidator::_OperateRows(ILineReader& reader)
{
    while (!reader.AtEOF())
    {
        reader.ReadLine();
        // First line is a column definitions
        CTempString current = reader.GetCurrentLine();
        m_current_row_number = reader.GetLineNumber();

        if (current.empty())
        {
            if (m_flags & e_tab_ignore_empty_rows)
                continue;
            else
                _ReportError(0, "Empty rows not allowed", "");
        }
        else
        {
            if (current[0] == '#') continue; // skip all comment lines
            vector<CTempStringEx> values; values.reserve(m_col_defs.size());
            NStr::Split(current, m_delim, values);

            if (values.size() > m_col_defs.size())
                _ReportError(m_col_defs.size(), "To many values", "");

            ITERATE(vector< set<string> >, req_one_it, m_require_one_cols)
            {     
                int count = 0;
                for (size_t i=0; i<m_col_defs.size(); i++)
                {                    
                    if (i<values.size() && !values[i].empty())
                    {
                        if (req_one_it->find(m_col_defs[i]) != req_one_it->end())
                        {
                            count++;
                        }
                    }
                }
                if (count==0)
                {
                    string colname = NStr::Join(*req_one_it, ",");
                    _ReportError(-1, "None of require-one columns specified", colname);
                }
            }

            for (size_t i=0; i<m_col_defs.size(); i++)
            {
                if (i>=values.size() || values[i].empty())
                {
                    if (m_required_cols[i])
                        _ReportError(i, "Missing required value", "");
                    else
                        continue;
                }
                else
                if (m_ignored_cols[i])
                    continue;

                bool isfatal = _Validate(i, values[i]);
                if (isfatal)
                {
                    _ReportError(i, "Fatal error occured, stopping", "");
                    return;
                }
                if (!values[i].empty() && m_unique_cols[i])
                {
                    int& count = m_unique_values[i][values[i]];
                    if (count++)
                    {
                        _ReportError(i, "Non unique value", "");
                    }
                    
                }
            } // iterate over cols
        }
    }
}

void CTabDelimitedValidator::GenerateOutput(CNcbiOstream* out_stream, bool no_headers)
{
    if ( (m_flags & CTabDelimitedValidator::e_tab_tab_report) == CTabDelimitedValidator::e_tab_tab_report)
    {
        _ReportTab(out_stream);
    }
    else
    if ( (m_flags & CTabDelimitedValidator::e_tab_xml_report) == CTabDelimitedValidator::e_tab_xml_report)
    {
        _ReportXML(out_stream, no_headers);
    }
}

void CTabDelimitedValidator::RegisterAliases(CNcbiIstream* in_stream)
{
    CColumnValidatorRegistry& r = CColumnValidatorRegistry::GetInstance();

    // default aliases
    r.Register("germline", "boolean");
    r.Register("metagenomic", "boolean");
    r.Register("rearranged", "boolean");
    r.Register("transgenic", "boolean");

    if (in_stream)
    {
        CRef<ILineReader> reader(ILineReader::New(*in_stream));
        while (!reader->AtEOF())
        {
            reader->ReadLine();
            CTempString line = reader->GetCurrentLine();
            if (line.empty())
                continue;
            if (line[0] == '#' || line[0] == ';')
                continue;
            CTempString name, alias;
            NStr::SplitInTwo(line, "\t ", name, alias, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
            if (name.empty() || alias.empty())
                continue;
            r.Register(name, alias);
        }
    }
}

END_NCBI_SCOPE

