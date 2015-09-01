#ifndef _PARSE_TEXT_OPTIONS_HPP_
#define _PARSE_TEXT_OPTIONS_HPP_
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
*  and reliability of the software and data,  the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties,  express or implied,  including
*  warranties of performance,  merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Colleen Bollin, Andrea Asztalos
*
* File Description:
*   Classes that implement string parsing. 
*/

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

NCBI_XOBJEDIT_EXPORT SIZE_TYPE FindWithOptions(const string& str, const string& pattern,
                                    SIZE_TYPE start_search, bool case_insensitive, bool whole_word);

class NCBI_XOBJEDIT_EXPORT CParseTextMarker : public CObject
{
public:
    enum EMarkerType {
        eMarkerType_None = 0,
        eMarkerType_Text,
        eMarkerType_Digits,
        eMarkerType_Letters
    };

    CParseTextMarker() 
        : m_MarkerType(eMarkerType_None),
        m_Text(kEmptyStr) {}
    ~CParseTextMarker() {}

    void SetText(const string& val);
    void SetDigits() { m_MarkerType = eMarkerType_Digits; }
    void SetLetters() { m_MarkerType = eMarkerType_Letters; }
    bool FindInText(const string& val, size_t& pos, size_t& len, size_t start_search, bool case_insensitive, bool whole_word) const;
    void Reset();
    
    static void s_GetDigitsPosition(const string& str, size_t& pos, size_t& len, size_t start_search);
    static void s_GetLettersPosition(const string& str, size_t& pos, size_t& len, size_t start_search);

private:
    EMarkerType m_MarkerType;
    string m_Text;
};


class NCBI_XOBJEDIT_EXPORT CParseTextOptions : public CObject
{
public:
    CParseTextOptions() 
        : m_IncludeStartMarker(false),
        m_IncludeStopMarker(false),
        m_RemoveFromParsed(false),
        m_RemoveBeforePattern(false),
        m_RemoveAfterPattern(false),
        m_CaseInsensitive(false),
        m_WholeWord(false) {}
    ~CParseTextOptions() {}

    void SetStartText(const string& val) { m_StartMarker.SetText(val); }
    void SetStopText(const string& val) { m_StopMarker.SetText(val); }
    void SetStartDigits() { m_StartMarker.SetDigits(); }
    void SetStopDigits() { m_StopMarker.SetDigits(); }
    void SetStartLetters() { m_StartMarker.SetLetters(); }
    void SetStopLetters() { m_StopMarker.SetLetters(); }
    
    void SetIncludeStart(bool val) { m_IncludeStartMarker = val; }
    void SetIncludeStop(bool val) { m_IncludeStopMarker = val; }
    /// if true, removes the parsed text from the parsed text
    void SetShouldRemove(bool val) { m_RemoveFromParsed = val; }
    /// if true, removes the lhs delimiter from the parsed text
    void SetShouldRmvBeforePattern(bool val) { m_RemoveBeforePattern = val; }
    /// if true, removes the rhs delimiter from the parsed text
    void SetShouldRmvAfterPattern(bool val) { m_RemoveAfterPattern = val; }
    
    void SetCaseInsensitive(bool val) { m_CaseInsensitive = val; }
    void SetWholeWord(bool val) { m_WholeWord = val; }
    void Reset();

    string GetSelectedText(const string& input) const;
    void RemoveSelectedText(string& input, bool remove_first_only = true) const;
    bool ShouldRemoveFromParsed() const { return m_RemoveFromParsed; }

private:
    CParseTextMarker m_StartMarker;
    CParseTextMarker m_StopMarker;
    bool m_IncludeStartMarker;
    bool m_IncludeStopMarker;
    bool m_RemoveFromParsed;
    bool m_RemoveBeforePattern;
    bool m_RemoveAfterPattern;
    bool m_CaseInsensitive;
    bool m_WholeWord;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif    // _PARSE_TEXT_OPTIONS_HPP_