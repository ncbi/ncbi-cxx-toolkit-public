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
 * Authors:  Colleen Bollin
 */


#ifndef _STRING_CONSTRAINT_H_
#define _STRING_CONSTRAINT_H_

#include <corelib/ncbistd.hpp>

#include <objmgr/scope.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


enum EExistingText {
    eExistingText_replace_old = 0,
    eExistingText_append_semi,
    eExistingText_append_space,
    eExistingText_append_colon,
    eExistingText_append_comma,
    eExistingText_append_none,
    eExistingText_prefix_semi,
    eExistingText_prefix_space,
    eExistingText_prefix_colon,
    eExistingText_prefix_comma,
    eExistingText_prefix_none,
    eExistingText_leave_old,
    eExistingText_add_qual,
    eExistingText_cancel
};


/// Add text to an existing string, using the "existing_text" directive to combine 
/// new text with existing text
NCBI_XOBJEDIT_EXPORT bool AddValueToString (string& str, const string& value, EExistingText existing_text);


class NCBI_XOBJEDIT_EXPORT CStringConstraint : public CObject
{
public:
    enum EMatchType {
        eMatchType_Contains = 0,
        eMatchType_Equals,
        eMatchType_StartsWith,
        eMatchType_EndsWith,
        eMatchType_IsOneOf
    };

    CStringConstraint(const string& match_text,
                      EMatchType match_type = eMatchType_Contains, 
                      bool ignore_case = false,
                      bool ignore_space = false,
                      bool negation = false)
          : m_MatchText(match_text),
            m_MatchType(match_type),
            m_IgnoreCase(ignore_case), 
            m_IgnoreSpace(ignore_space), 
            m_NotPresent(negation) {}
    ~CStringConstraint() {}

    void SetNegation(bool val) { m_NotPresent = val; }
    void SetIgnoreCase(bool val) { m_IgnoreCase = val; }
    void SetIgnoreSpace(bool val) { m_IgnoreSpace = val; }
    bool GetNegation() { return m_NotPresent; }
    bool GetIgnoreCase() { return m_IgnoreCase; }
    bool GetIgnoreSpace() { return m_IgnoreSpace; }
    bool DoesTextMatch(const string& text);
    bool DoesListMatch(const vector<string>& vals);
    EMatchType GetMatchType() { return m_MatchType; }
    void SetMatchType(EMatchType match_type) { m_MatchType = match_type; }
    const string& GetMatchText() { return m_MatchText; }
    void SetMatchText(const string& match_text) { m_MatchText = match_text; }
    void Assign(const CStringConstraint& other);

private:
    string m_MatchText;
    EMatchType m_MatchType;
    bool m_IgnoreCase;
    bool m_IgnoreSpace;
    bool m_NotPresent;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif