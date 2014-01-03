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

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <util/line_reader.hpp>
#include <objtools/readers/source_mod_parser.hpp>

#include "tab_table_reader.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void CTabDelimitedValidator::_Validate(int col_number, const CTempString& datatype, const CTempString& value)
{
}

void CTabDelimitedValidator::_ReportError(int col_number, const CTempString& error)
{
}

void CTabDelimitedValidator::ValidateInput(ILineReader& reader, const CTempString& default_collumns, const CTempString& required, bool comma_separated, bool no_header, bool ignore_empty_rows)
{
    vector<CTempStringEx> cols;
    vector<bool> required_cols;
    m_current_row_number = 0;

    CTempString delim = comma_separated ? "," : "\t";

    if (!default_collumns.empty())
    {
        NStr::Tokenize(default_collumns, ",", cols); //using comma separator always
    }

    while (!reader.AtEOF())
    {
        reader.ReadLine();
        // First line is a collumn definitions
        CTempString current = reader.GetCurrentLine();
        m_current_row_number = reader.GetLineNumber();
        if (reader.GetLineNumber() == 1)
        {
            if (no_header)
                continue;

            if (default_collumns.empty())
                NStr::Tokenize(current, delim, cols);

            // preprocess headers & required
            required_cols.resize(cols.size(), false); // all values are not required
            vector<CTempStringEx> required_names;
            NStr::Tokenize(required, ",", required_names);
            for (size_t i=0; i<required_names.size(); i++)
            {
                int index = NStr::StringToInt(required_names[i], NStr::fConvErr_NoThrow);
                if (index == 0)
                {
                    vector<CTempStringEx>::iterator col_it = find(cols.begin(), cols.end(), required_names[i]);
                    if (col_it == cols.end())
                    {
                        _ReportError(i, "Required column does not exist");
                        // stop processing
                        return;
                    }                   
                    index = (col_it - cols.begin());
                }
                required_cols[index] = true;
            }         
            continue;
        }

        if (current.empty())
        {
            if (ignore_empty_rows)
                continue;
            else
                _ReportError(0, "Empty rows not allowed");
        }
        else
        {
            vector<CTempStringEx> values;
            NStr::Tokenize(current, delim, values);

            if (values.size() > cols.size())
                _ReportError(0, "To many values");

            for (size_t i=0; i<cols.size(); i++)
            {
                if (i>=values.size() || values[i].empty())
                {
                    if (required_cols[i])
                        _ReportError(i, "Missing required value");
                }
                else
                {
                   _Validate(i, cols[i], values[i]);
                }
            }
        }
    }
}

END_NCBI_SCOPE

