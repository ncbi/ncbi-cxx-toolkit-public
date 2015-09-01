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
* Author:  Colleen Bollin, Andrea Asztalos
*
* File Description:
*   
*/

#include <ncbi_pch.hpp>
#include <objtools/edit/parse_text_options.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


SIZE_TYPE FindWithOptions(const string& str, const string& pattern, SIZE_TYPE start_search, bool case_insensitive, bool whole_word)
{
    SIZE_TYPE pos = start_search;
    NStr::ECase str_case = case_insensitive ? NStr::eNocase : NStr::eCase;

    bool found = false;
    while (pos != NPOS && !found) {
        pos = NStr::Find(str, pattern, pos, NPOS, NStr::eFirst, str_case);
        if (pos != NPOS) {
            if (whole_word) {
                bool whole_at_start = false;
                if (pos == 0 || !isalpha(str.c_str()[pos - 1])) {
                    whole_at_start = true;
                }
                bool whole_at_end = false;
                if (pos + pattern.length() == str.length() || !isalpha(str.c_str()[pos + pattern.length()])) {
                    whole_at_end = true;
                }
                if (whole_at_start && whole_at_end) {
                    found = true;
                }
            } else {
                found = true;
            }

            if (!found) {
                ++pos;
            }
        }
    }
    return pos;
}


void CParseTextMarker::SetText(const string& val)
{
    m_Text = val;
    if (m_Text.empty()) {
        m_MarkerType = eMarkerType_None;
    } else {
        m_MarkerType = eMarkerType_Text;
    }
}


bool CParseTextMarker::FindInText(const string& val, size_t& pos, size_t& len, size_t start_search, 
    bool case_insensitive, bool whole_word) const
{
    bool found = false;

    switch (m_MarkerType) {
    case eMarkerType_None:
        pos = 0;
        if (start_search != 0)
            pos = NPOS;
        len = 0;
        found = true;
        break;
    case eMarkerType_Text:
        pos = FindWithOptions(val, m_Text, start_search, case_insensitive, whole_word);
        if (pos != NPOS) {
            len = m_Text.length();
            found = true;
        }
        break;
    case eMarkerType_Digits:
        s_GetDigitsPosition(val, pos, len, start_search);
        if (len > 0) {
            found = true;
        }
        break;
    case eMarkerType_Letters:
        s_GetLettersPosition(val, pos, len, start_search);
        if (len > 0) {
            found = true;
        }
        break;
    }
    return found;
}


void CParseTextMarker::s_GetDigitsPosition(const string& str, size_t& pos, size_t& len, size_t start_search)
{
    pos = start_search;
    string substr = str.substr(start_search);
    const char *cp = substr.c_str();
    while (*cp != 0 && !isdigit(*cp)) {
        pos++;
        cp++;
    }
    if (*cp != 0) {
        len = 1;
        cp++;
        while (*cp != 0 && isdigit(*cp)) {
            len++;
            cp++;
        }
    }
}


void CParseTextMarker::s_GetLettersPosition(const string& str, size_t& pos, size_t& len, size_t start_search)
{
    pos = start_search;
    string substr = str.substr(start_search);
    const char *cp = substr.c_str();
    while (*cp != 0 && !isalpha(*cp)) {
        pos++;
        cp++;
    }
    if (*cp != 0) {
        len = 1;
        cp++;
        while (*cp != 0 && isalpha(*cp)) {
            len++;
            cp++;
        }
    }
}

void CParseTextMarker::Reset()
{
    m_MarkerType = eMarkerType_None;
    m_Text.clear();
}

string CParseTextOptions::GetSelectedText(const string& input) const
{
    string rval = kEmptyStr;
    size_t start_pos(0), start_len(0), stop_pos(0), stop_len(0);
    if (m_StartMarker.FindInText(input, start_pos, start_len, 0, m_CaseInsensitive, m_WholeWord)
        && m_StopMarker.FindInText(input, stop_pos, stop_len, start_pos + start_len, m_CaseInsensitive, m_WholeWord)) {
        size_t sel_start = start_pos;
        size_t sel_stop = stop_pos;
        if (!m_IncludeStartMarker) {
            sel_start += start_len;
        }
        if (m_IncludeStopMarker) {
            sel_stop += stop_len;
        }
        if (sel_start == 0 && sel_stop == 0) {
            sel_stop = NPOS;
        }
        rval = input.substr(sel_start, sel_stop - sel_start);
    }

    NStr::TruncateSpacesInPlace(rval);
    return rval;
}

void CParseTextOptions::RemoveSelectedText(string& input, bool remove_first_only) const
{
    bool found = true;
    size_t start_pos = 0;
    while (found)
    {
        found = false;
        size_t start_len(0), stop_pos(0), stop_len(0);
        if (m_StartMarker.FindInText(input, start_pos, start_len, start_pos, m_CaseInsensitive, m_WholeWord)
            && m_StopMarker.FindInText(input, stop_pos, stop_len, start_pos + start_len, m_CaseInsensitive, m_WholeWord)) {
            size_t sel_start = start_pos;
            size_t sel_stop = stop_pos;
            if (!m_IncludeStartMarker && !m_RemoveBeforePattern) {
                sel_start += start_len;
            }
            if (m_IncludeStopMarker || m_RemoveAfterPattern) {
                sel_stop += stop_len;
            }
            string new_val = kEmptyStr;
            if (sel_start > 0) {
                new_val = input.substr(0, sel_start);
            }
            if (sel_stop > 0 && sel_stop < input.length() - 1) {
                new_val += input.substr(sel_stop);
            }
            if (new_val != input)
            {
                found = true;
            }
            input = new_val;
        }
        if (remove_first_only)
        {
            found = false;
        }
        start_pos++;
    }

    NStr::TruncateSpacesInPlace(input);
}

void CParseTextOptions::Reset()
{
    m_StartMarker.Reset();
    m_StopMarker.Reset();
    m_IncludeStartMarker = false;
    m_IncludeStopMarker = false;
    m_RemoveFromParsed = false;
    m_RemoveBeforePattern = false;
    m_RemoveAfterPattern = false;
    m_CaseInsensitive = false;
    m_WholeWord = false;
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
