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
* Author: Colleen Bollin, NCBI
*
* File Description:
*   String Constraint for editing
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/edit/string_constraint.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

bool AddValueToString (string& str, const string& value, EExistingText existing_text)
{
    bool rval = false;
    if (NStr::IsBlank(value)) {
        // no change to value
        rval = false;
    } else if (existing_text == eExistingText_replace_old || NStr::IsBlank (str)) {
        str = value;
        rval = true;
    } else {
        rval = true;
        switch (existing_text) {
            case eExistingText_append_semi:
                str = str + "; " + value;
                break;
            case eExistingText_append_space:
                str = str + " " + value;
                break;
            case eExistingText_append_colon:
                str = str + ":" + value;
                break;
            case eExistingText_append_comma:
                str = str + ", " + value;
                break;
            case eExistingText_append_none:
                str = str + value;
                break;            
            case eExistingText_prefix_semi:
                str = value + "; " + str;
                break;            
            case eExistingText_prefix_space:
                str = value + " " + str;
                break;            
            case eExistingText_prefix_colon:
                str = value + ":" + str;
                break; 
            case eExistingText_prefix_comma:
                str = value + ", " + str;
                break; 
            case eExistingText_prefix_none:
                str = value + str;
                break; 
            default:
                rval = false;
                break;
        }
    }
    return rval;
}


bool CStringConstraint::DoesTextMatch (const string& text)
{
    string match = m_MatchText;
    if (match.length() == 0) {
        return true;
    }
        
    bool rval = false;
    
    string tmp = text;
    if (m_IgnoreSpace) {
        NStr::ReplaceInPlace(match, " ", "");
        NStr::ReplaceInPlace(tmp, " ", "");
    }
    if (m_IgnoreCase) {
        NStr::ToLower(match);
        NStr::ToLower(tmp);
    }
    
    switch (m_MatchType) {
        case eMatchType_Contains: 
            if (NStr::Find(tmp, match) != string::npos) {
                rval = true;
            }
            break;
        case eMatchType_Equals:
            if (NStr::Equal(tmp, match)) {
                rval = true;
            }
            break;
        case eMatchType_StartsWith:
            if (NStr::StartsWith(tmp, match)) {
                rval = true;
            }
            break;
        case eMatchType_EndsWith:
            if (NStr::EndsWith(tmp, match)) {
                rval = true;
            }
            break;
        case eMatchType_IsOneOf:
            {
                vector<string> tokens;
                NStr::Tokenize(match, ",; ", tokens);
                ITERATE(vector<string>, it, tokens) {
                    if (NStr::Equal(*it, tmp)) {
                        rval = true;
                        break;
                    }
                }
            }
            break;
    }
    if (m_NotPresent) {
        rval = !rval;
    }
    return rval;    
}


bool CStringConstraint::DoesListMatch(const vector<string>& vals)
{
    bool any_match = false;
    bool invert = false;
    if (m_NotPresent) {
        invert = true;
        m_NotPresent = false;
    }

    ITERATE(vector<string>, s, vals) {
        any_match = DoesTextMatch(*s);
        if (any_match) {
            break;
        }
    }
    if (invert) {
        m_NotPresent = true;
        any_match = !any_match;
    }
    return any_match;

}


void CStringConstraint::Assign(const CStringConstraint& other)
{
    m_MatchText = other.m_MatchText;
    m_MatchType = other.m_MatchType;
    m_IgnoreCase = other.m_IgnoreCase;
    m_IgnoreSpace = other.m_IgnoreSpace;
    m_NotPresent = other.m_NotPresent;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
