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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <util/strsearch.hpp>
#include <algorithm>
#include <vector>
#include <ctype.h>

NCBI_USING_NAMESPACE_STD;

BEGIN_NCBI_SCOPE

//==============================================================================
//                            CBoyerMooreMatcher
//==============================================================================

// Public:
// =======

CBoyerMooreMatcher::CBoyerMooreMatcher
(const string& pattern, 
 bool case_sensitive,
 bool whole_word) :
m_Pattern(pattern), 
m_PatLen(pattern.length()), 
m_CaseSensitive(case_sensitive), 
m_WholeWord(whole_word),
m_LastOccurance(sm_AlphabetSize)
{
    if ( !m_CaseSensitive ) {
        NStr::ToUpper(m_Pattern);
    }
    
    // For each character in the alpahbet compute its last occurance in 
    // the pattern.
    
    // Initilalize vector
    size_t size = m_LastOccurance.size();
    for ( size_t i = 0;  i < size;  ++i ) {
        m_LastOccurance[i] = m_PatLen;
    }
    
    // compute right-most occurance
    for ( int j = 0;  j < (int)m_PatLen - 1;  ++j ) {
        m_LastOccurance[(int)m_Pattern[j]] = m_PatLen - j - 1;
    }
}


int CBoyerMooreMatcher::Search(const string& text, unsigned int shift) const
{
    size_t text_len = text.length();
    
    while ( shift + m_PatLen <= text_len ) {
        int j = (int)m_PatLen - 1;
        
        for ( char text_char = 
                m_CaseSensitive ? text[shift + j] : toupper(text[shift + j]);
              j >= 0  &&  m_Pattern[j] == text_char;
              text_char = 
                m_CaseSensitive ? text[shift + j] : toupper(text[shift + j]) ) {
            --j;
        }
        if ( (j == -1)  &&  IsWholeWord(text, shift) ) {
            return  shift;
        } else {
            shift += (unsigned int)m_LastOccurance[text[shift + j]];
        }
    }
    
    return -1;
}


// Private:
// ========

// Constants
const int CBoyerMooreMatcher::sm_AlphabetSize = 256;     // assuming ASCII


// Member Functions
bool CBoyerMooreMatcher::IsWholeWord(const string& text, unsigned int pos) const
{
    if ( !m_WholeWord ) {
        return true;
    }
    
    bool left  = true, 
         right = true;
    
    // check on the left
    if ( pos > 0 ) {
        left = isspace(text[pos - 1]) != 0;
    }
    
    // check on the right
    pos += (unsigned int)m_PatLen;
    if ( pos < text.length() ) {
        right = isspace(text[pos]) != 0;
    }
    
    return (right  &&  left);
}


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2003/11/07 17:16:23  ivanov
* Fixed  warnings on 64-bit Workshop compiler
*
* Revision 1.4  2003/02/04 20:16:15  shomrat
* Change signed to unsigned to eliminate compilation warning
*
* Revision 1.3  2002/11/05 23:01:13  shomrat
* Coding style changes
*
* Revision 1.2  2002/11/03 21:59:14  kans
* BoyerMoore takes caseSensitive, wholeWord parameters (MS)
*
* Revision 1.1  2002/10/29 16:33:11  kans
* initial checkin - Boyer-Moore string search and templated text search finite state machine (MS)
*
*
* ===========================================================================
*/

