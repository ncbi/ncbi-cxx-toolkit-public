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
* Author:  Mati Shomrat
*
* File Description:
*   String search utilities.
*
*   Currently there are two search utilities:
*   1. An implementation of the Boyer-Moore algorithm.
*   2. A finite state automaton.
*
*/

#include <ncbi_pch.hpp>
#include <util/strsearch.hpp>
#include <algorithm>
#include <vector>


BEGIN_NCBI_SCOPE


//==============================================================================
//                            CBoyerMooreMatcher
//==============================================================================

// Public:
// =======

CBoyerMooreMatcher::CBoyerMooreMatcher(const string& pattern, 
                                       NStr::ECase   case_sensitive,
                                       unsigned int  whole_word)
: m_Pattern(pattern), 
  m_PatLen(pattern.length()), 
  m_CaseSensitive(case_sensitive), 
  m_WholeWord(whole_word),
  m_LastOccurrence(sm_AlphabetSize),
  m_WordDelimiters(sm_AlphabetSize)
{
    x_InitPattern();
    // Init the word deimiting alphabet
    if (m_WholeWord) {
        for (int i = 0; i < sm_AlphabetSize; ++i) {
            m_WordDelimiters[i] = (isspace((unsigned char) i) != 0);
        }
    }
}

CBoyerMooreMatcher::CBoyerMooreMatcher(const string& pattern,
                                       const string& word_delimeters,
                                       NStr::ECase   case_sensitive,
                                       bool          invert_delimiters)
: m_Pattern(pattern), 
  m_PatLen(pattern.length()), 
  m_CaseSensitive(case_sensitive), 
  m_WholeWord(true),
  m_LastOccurrence(sm_AlphabetSize),
  m_WordDelimiters(sm_AlphabetSize)
{
    x_InitPattern();
    SetWordDelimiters(word_delimeters, invert_delimiters);
}

void CBoyerMooreMatcher::SetWordDelimiters(const string& word_delimeters,
                                           bool          invert_delimiters)
{
    m_WholeWord = eWholeWordMatch;

    string word_d = word_delimeters;
    if (m_CaseSensitive == NStr::eNocase) {
        NStr::ToUpper(word_d);
    }

    // Init the word delimiting alphabet
    for (int i = 0; i < sm_AlphabetSize; ++i) {
        char ch = (char)(m_CaseSensitive ? i : toupper((unsigned char)i));
        string::size_type n = word_d.find_first_of(ch);
        m_WordDelimiters[i] = (!invert_delimiters) == (n != string::npos);
    }
}

void CBoyerMooreMatcher::AddDelimiters(const string& word_delimeters)
{
    if (m_WholeWord == 0) {
        m_WholeWord = eWholeWordMatch;
    }

    string word_d = word_delimeters;
    if (m_CaseSensitive == NStr::eNocase) {
        NStr::ToUpper(word_d);
    }

    for (int i = 0; i < sm_AlphabetSize; ++i) {
        char ch = (char)(m_CaseSensitive ? i : toupper((unsigned char)i));
        string::size_type n = word_d.find_first_of(ch);
        if (n != NPOS) {
            m_WordDelimiters[i] = true;
        }
    }
}

void CBoyerMooreMatcher::AddDelimiters(char ch)
{
    if (m_WholeWord == 0) {
        m_WholeWord = eWholeWordMatch;
    }
    m_WordDelimiters[(unsigned char)ch] = true;

    if (m_CaseSensitive == NStr::eNocase) {
        m_WordDelimiters[toupper((unsigned char) ch)] = true;
    }
}

void CBoyerMooreMatcher::InitCommonDelimiters()
{
    if (m_WholeWord == 0) {
        m_WholeWord = eWholeWordMatch;
    }

    for (int i = 0; i < sm_AlphabetSize; ++i) {
        char ch = (char)(m_CaseSensitive ? i : toupper((unsigned char)i));
        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            (ch == '_')){
        } else {
            m_WordDelimiters[i] = true;
        }
    }
}

void CBoyerMooreMatcher::x_InitPattern(void)
{
    if ( m_CaseSensitive == NStr::eNocase) {
        NStr::ToUpper(m_Pattern);
    }
    
    // For each character in the alpahbet compute its last occurrence in 
    // the pattern.
    
    // Initilalize vector
    size_t size = m_LastOccurrence.size();
    for ( size_t i = 0;  i < size;  ++i ) {
        m_LastOccurrence[i] = m_PatLen;
    }
    
    // compute right-most occurrence
    for ( int j = 0;  j < (int)m_PatLen - 1;  ++j ) {
        /* int lo = */
        m_LastOccurrence[(unsigned char)m_Pattern[j]] = m_PatLen - j - 1;
   }
}


SIZE_TYPE CBoyerMooreMatcher::Search(const char*  text, 
                                     SIZE_TYPE shift,
                                     SIZE_TYPE text_len) const
{
    // Implementation note.
    // Case sensitivity check has been taken out of loop. 
    // Code size for performance optimization. (We generally choose speed).
    // (Anatoliy)
    if (m_CaseSensitive == NStr::eCase) {
        while (shift + m_PatLen <= text_len) {
            int j = (int)m_PatLen - 1;

            for( ; j >= 0 && m_Pattern[j]==text[shift + j]; --j )
                {}

            if ( (j == -1)  &&  IsWholeWord(text, shift, text_len) ) {
                return  shift;
            } else {
                shift += (unsigned int)m_LastOccurrence[(unsigned char)text[shift + (int)m_PatLen - 1]];
            }
        }
    } else { // case insensitive NStr::eNocase
        while (shift + m_PatLen <= text_len) {
            int j = (int)m_PatLen - 1;

            for( ; j >= 0 && m_Pattern[j]==char(toupper((unsigned char) text[shift + j])); --j )
                {}

            if ( (j == -1)  &&  IsWholeWord(text, shift, text_len) ) {
                return  shift;
            } else {
                shift += 
                    (unsigned int)m_LastOccurrence[toupper((unsigned char) text[shift + (int)m_PatLen - 1])];
            }
        }
    }
    return (SIZE_TYPE)-1;
}


// Private:
// ========

// Constants
const int CBoyerMooreMatcher::sm_AlphabetSize = 256;     // assuming ASCII


// Member Functions
bool CBoyerMooreMatcher::IsWholeWord(const char*  text, 
                                     SIZE_TYPE    pos,
                                     SIZE_TYPE    text_len) const
{
    SIZE_TYPE left, right;
    left = right = 1;

    // Words at the beginning and end of text are also considered "whole"

    // check on the left  
    if (m_WholeWord & ePrefixMatch) {
        left = (pos == 0) ||
            ((pos > 0) && m_WordDelimiters[(unsigned char)text[pos - 1]]);
    }

    // check on the right
    if (m_WholeWord & eSuffixMatch) {
        pos += (unsigned int)m_PatLen;
        right = (pos == text_len) || 
            ((pos < text_len) && m_WordDelimiters[(unsigned char)text[pos]]);
    }


    return (left && right);
}


END_NCBI_SCOPE
