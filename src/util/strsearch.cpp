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
//                            BoyerMooreMatcher
//==============================================================================

// Public:
// =======

BoyerMooreMatcher::BoyerMooreMatcher(const string &pattern, 
                                     bool caseSensitive,
                                     bool wholeWord) :
    m_pattern(pattern), m_patlen(pattern.length()), 
    m_caseSensitive(caseSensitive), m_wholeWord(wholeWord),
    m_last_occurance(ALPHABET_SIZE)
{
    if ( !m_caseSensitive ) {
        NStr::ToUpper(m_pattern);
    }

    // For each character in the alpahbet compute its last occurance in the pattern.
    int i;
    
    // Initilalize vector
    unsigned int size = m_last_occurance.size();
    for ( i = 0; i < size; ++i ) {
        m_last_occurance[i] = m_patlen;
    }

    // compute right-most occurance
    for ( i = 0; i < (int)m_patlen - 1; ++i ) {
        m_last_occurance[(int)m_pattern[i]] = m_patlen - i - 1;
    }
}


int BoyerMooreMatcher::search (const string &text, unsigned int shift) const
{
    unsigned int text_len = text.length();

    while ( shift + m_patlen <= text_len ) {
        int j = m_patlen - 1;

        for ( char text_char = m_caseSensitive ? text[shift + j] : toupper(text[shift + j]);
              j >= 0 && m_pattern[j] == text_char;
              text_char = m_caseSensitive ? text[shift + j] : toupper(text[shift + j]) ) {
            --j;
        }
        if ( j == -1 && IsWholeWord(text, shift) ) {
            return  shift;
        } else {
            shift += m_last_occurance[text[shift + j]];
        }
    }

    return -1;
}


// Private:
// ========

const int BoyerMooreMatcher::ALPHABET_SIZE = 256;     // assuming ASCII

bool BoyerMooreMatcher::IsWholeWord (const string &text, int pos) const
{
    bool left = true, right = true;
    if ( !m_wholeWord ) {
        return true;
    }

    // check on the right
    if ( pos > 0 ) {
        right = isspace(text[pos - 1]) != 0;
    }

    // check on the left
    pos += m_patlen;
    if ( pos < text.length() ) {
        left = isspace(text[pos]) != 0;
    }

    return right && left;
}
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2002/11/03 21:59:14  kans
* BoyerMoore takes caseSensitive, wholeWord parameters (MS)
*
* Revision 1.1  2002/10/29 16:33:11  kans
* initial checkin - Boyer-Moore string search and templated text search finite state machine (MS)
*
*
* ===========================================================================
*/

