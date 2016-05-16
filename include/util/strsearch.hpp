#ifndef UTIL___STRSEARCH__HPP
#define UTIL___STRSEARCH__HPP

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
*          Anatoliy Kuznetsov
*
* File Description:
*   String search utilities.
*
*/

/// @file strsearch.hpp
/// String search utilities.
///
///   Currently there are two search utilities:
/// 1. An implementation of the Boyer-Moore algorithm.
/// 2. A finite state automata.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <string>
#include <vector>
#include <iterator>


/** @addtogroup StringSearch
 *
 * @{
 */


BEGIN_NCBI_SCOPE


//============================================================================//
//                            CBoyerMooreMatcher                               //
//============================================================================//


/// This implemetation uses the Boyer-Moore alg. in order to search a single
/// pattern over varying texts.


class NCBI_XUTIL_EXPORT CBoyerMooreMatcher 
{
public:
    enum EWordMatch
    {
        eSubstrMatch = 0,
        ePrefixMatch = (1 << 0),
        eSuffixMatch = (1 << 1),
        eWholeWordMatch = (ePrefixMatch | eSuffixMatch)
    };
public:
    /// Initialize a matcher with the pattern to be matched.
    ///
    /// @param pattern
    ///    Pattern to be matched
    /// @param case_sensitive
    ///    should the search be case sensitive (false by default).
    /// @param whole_word 
    ///    a match is found ony if the pattern was found to 
    ///    be between delimiting characters (whitespaces)
    CBoyerMooreMatcher(const string& pattern,        
                       NStr::ECase   case_sensitive = NStr::eNocase,
                       unsigned int  whole_word = eSubstrMatch);

    
    /// Initialize a matcher with the pattern to be matched.
    ///
    /// @param pattern
    ///    Pattern to be matched
    /// @param word_delimeters 
    ///    a match is found ony if the pattern was found to 
    ///    be between delimiting characters like whitespaces
    ///    and punctuation marks
    /// @param case_sensitive
    ///    should the search be case sensitive (false by default).
    /// @param invert_delimiters
    ///    when TRUE means all characters NOT belonging to
    ///    word_delimeters are to be used
    CBoyerMooreMatcher(const string& pattern,
                       const string& word_delimeters,
                       NStr::ECase   case_sensitive = NStr::eNocase,
                       bool          invert_delimiters = false);


    /// Set word delimiting characters
    ///
    /// @param word_delimeters 
    ///    string of characters used as word delimiters
    /// @param invert_delimiters
    ///    when TRUE means all characters NOT belonging to
    ///    word_delimeters are to be used
    void SetWordDelimiters(const string& word_delimeters,
                           bool          invert_delimiters = false);

    /// Add new word delimiters
    ///
    /// @param word_delimeters 
    ///    string of characters used as word delimiters
    ///
    void AddDelimiters(const string& word_delimeters);

    /// Add new word delimiter charracter
    ///
    /// @param word_delimeters 
    ///    string of characters used as word delimiters
    ///
    void AddDelimiters(char ch);

    /// Init delimiters most common for the English language,
    /// (whitespaces, punctuations, etc)
    void InitCommonDelimiters();
    

    /// Set word matching mode
    ///
    /// @param whole_word 
    ///    word matching mode. Can be OR combination of EWordMatch values
    void SetWordMatching(unsigned int whole_word = eWholeWordMatch)
    {
        m_WholeWord = whole_word;
    }

    /// Search for the pattern over text starting at position pos.
    ///
    /// @param text
    ///    Text to search in.
    /// @param pos
    ///    Position shift in text
    /// @return 
    ///    the position at which the pattern was found,
    ///    size_t(-1) (NPOS) otherwise.
    size_t Search(const string& text, size_t pos = 0) const
    {
        return Search(text.c_str(), pos, text.length());
    }


    /// Search for the pattern over text starting at position pos.
    ///
    /// @param text
    ///    Text memory taken as NON ASCIIZ string.
    /// @param pos 
    ///    String offset position from the start
    /// @param text_len
    ///    Length of text
    /// @return 
    ///    the position at which the pattern was found,
    ///    SIZE_TYPE(-1) (NPOS) otherwise.
    SIZE_TYPE Search(const char*  text, 
                     SIZE_TYPE pos,
                     SIZE_TYPE text_len) const;
    
private:
    // Constants
    static const int sm_AlphabetSize;
    
    // Member Functions:
    
    /// Check if the pattern at position pos in the text lies on a
    /// whole word boundry.
    bool IsWholeWord(const char*   text,
                     SIZE_TYPE     pos,
                     SIZE_TYPE     text_len) const;

    void x_InitPattern(void);
private:    
    string                  m_Pattern;  
    SIZE_TYPE               m_PatLen;
    NStr::ECase             m_CaseSensitive;
    unsigned int            m_WholeWord;
    vector<size_t>          m_LastOccurrence;
    vector<unsigned char>   m_WordDelimiters;
    
};


//============================================================================//
//                          Finite State Automata                             //
//============================================================================//



template <typename MatchType>
class CTextFsm
{
public:
    // Constants (done as an enum to avoid link errors on Darwin)
    enum ESpecialStates {
        eFailState = -1
    };
    
    // Constructors and Destructors:
    CTextFsm(bool case_sensitive = false);
    ~CTextFsm(void);
    
    // Add a word to the Fsm. Store a match for later retreival.
    void AddWord(const string& word, const MatchType& match);
    
    // Prime the FSM.
    // After finishing adding all the words to the FSM it needs to be 
    // primed to enable usage.
    bool IsPrimed(void) const { return m_Primed; }
    void Prime(void);
    
    // Retreive the FSM's initial state.
    int GetInitialState(void) const { return 0; }
    
    // Get the next state, based on the currect state and a transition
    // character.
    int GetNextState(int state, char letter) const;
    
    // Are there any matches stored in state?
    bool IsMatchFound(int state) const;
    
    // Retrieve the stored matches in state.
    const vector<MatchType>& GetMatches(int state) const;
    
private:
    // Internal representation of states.
    class CState
    {
    public:
        // Ad-hoc definitions
        typedef map<char, int> TMapCharInt;
        
        // Constructors and Destructors
        CState(void) : m_OnFailure(0) {}
        ~CState(void) {};
        
        // Add a transition to the table.
        void AddTransition(char letter, int to) { m_Transitions[letter] = to; }
        
        // Retreive the transition state, give the transition character.
        int GetNextState(char letter) const {
	    TMapCharInt::const_iterator it = m_Transitions.find(letter);
	    return it != m_Transitions.end() ?  it->second : eFailState;
        }
        
        
        // Are there any matches stored in this state?
        bool IsMatchFound(void) const { return !m_Matches.empty(); }
        
        // Retreive the stored matches
        vector<MatchType>& GetMatches(void) { return m_Matches; }
        const vector<MatchType>& GetMatches(void) const { return m_Matches; }
        
        // Store a match.
        void AddMatch(const MatchType& match) { m_Matches.push_back(match); }
        
        const TMapCharInt& GetTransitions(void) const { return m_Transitions; };
        
        // Getter and Setter for failure transition.
        void SetOnFailure(int state) { m_OnFailure = state; }
        int GetOnFailure(void) const { return m_OnFailure; }
        
    private:
        
        // Member Variables:
        TMapCharInt         m_Transitions;   // Transition table
        vector<MatchType>   m_Matches;       // Stored matches
        int                 m_OnFailure;     // Transition state in failure.
        
    };  // end of class CState
    
    // Private Methods:
    CState *GetState(int state);
    int GetNextState(const CState& from, char letter) const;
    
    // Compute the transition to be performed on failure.
    void ComputeFail(void);
    void FindFail(int state, int new_state, char ch);
    void QueueAdd(vector<int>& in_queue, int qbeg, int val);
    
    // Member Variables
    bool                m_Primed;
    vector< CState >    m_States;
    bool                m_CaseSensitive;
    
};  // end of class CTextFsm


// Convenience class when the MatchType is of string type (most cases)
class NCBI_XUTIL_EXPORT CTextFsa : public CTextFsm<string>
{
public:
    CTextFsa(bool case_sensitive = false) :
        CTextFsm<string>(case_sensitive) 
    {}

    void AddWord(const string& word) {
        CTextFsm<string>::AddWord(word, word);
    }
};


//============================================================================//
//                   Finite State Automata Implementation                     //
//============================================================================//


// Public:
// =======

template <typename MatchType>
CTextFsm<MatchType>::CTextFsm(bool case_sensitive) :
m_Primed(false), m_CaseSensitive(case_sensitive)
{
    CState initial;
    m_States.push_back(initial);
}


template <typename MatchType>	
void CTextFsm<MatchType>::AddWord(const string& word, const MatchType& match) 
{
    string temp = word;
    if ( !m_CaseSensitive ) {
        NStr::ToUpper(temp);
    }

    int next, i, 
        state = 0,
        word_len = (int)temp.length();
    
    // try to overlay beginning of word onto existing table 
    for ( i = 0;  i < word_len;  ++i ) {
        next = m_States[state].GetNextState(temp[i]);
        if ( next == eFailState ) break;
        state = next;
    }
    
    // now create new states for remaining characters in word 
    for ( ;  i < word_len;  ++ i ) {
        CState new_state;
        
        m_States.push_back(new_state);
        m_States[state].AddTransition(temp[i], (int)m_States.size() - 1);
        state = m_States[state].GetNextState(temp[i]);
    }
    
    // add match information 
    m_States[state].AddMatch(match);
}


template <typename MatchType>
void CTextFsm<MatchType>::Prime(void)
{
    if ( m_Primed ) return;
    
    ComputeFail();
    
    m_Primed = true;
}


template <typename MatchType>
typename CTextFsm<MatchType>::CState *CTextFsm<MatchType>::GetState(int state) 
{
    if ( size_t(state) >= m_States.size() ) {
        return 0;
    }
    return &(m_States[state]);
}


template <typename MatchType>
int CTextFsm<MatchType>::GetNextState(const CState& from, char letter) const {
    char ch = m_CaseSensitive ? letter : (char)toupper((unsigned char) letter);
    return from.GetNextState(ch);
}


template <typename MatchType>
int CTextFsm<MatchType>::GetNextState(int state, char letter) const
{
    if ( size_t(state) >= m_States.size() ) {
        return eFailState;
    }
    
    int next;
    int initial = GetInitialState();
    while ( (next = GetNextState(m_States[state], letter)) == eFailState ) {
        if ( state == initial ) {
            next = initial;
            break;
        }
        state = m_States[state].GetOnFailure();
    }
    
    return next;
}


template <typename MatchType>
void CTextFsm<MatchType>::QueueAdd(vector<int>& in_queue, int qbeg, int val)
{
    int  q;
    
    q = in_queue [qbeg];
    if (q == 0) {
        in_queue [qbeg] = val;
    } else {
        for ( ;  in_queue [q] != 0;  q = in_queue [q]) continue;
        in_queue [q] = val;
    }
    in_queue [val] = 0;
}


template <typename MatchType>
void CTextFsm<MatchType>::ComputeFail(void) 
{
    int	qbeg, r, s, state;
    vector<int> state_queue(m_States.size());
    
    qbeg = 0;
    state_queue [0] = 0;
    
    // queue up states reached directly from state 0 (depth 1) 
    
    ITERATE ( typename CState::TMapCharInt,
              it, 
              m_States[GetInitialState()].GetTransitions() ) {
        s = it->second;
        m_States[s].SetOnFailure(0);
        QueueAdd(state_queue, qbeg, s);
    }
    
    while (state_queue [qbeg] != 0) {
        r = state_queue [qbeg];
        qbeg = r;
        
        // depth 1 states beget depth 2 states, etc. 
        
        ITERATE ( typename CState::TMapCharInt, it,
                  m_States[r].GetTransitions() ) {
            s = it->second;
            QueueAdd(state_queue, qbeg, s);
            
            //   State   Substring   Transitions   Failure
            //     2       st          a ->   3       6
            //     3       sta         l ->   4
            //     6       t           a ->   7       0
            //     7       ta          p ->   8
            //
            //   For example, r = 2 (st), if 'a' would go to s = 3 (sta).
            //   From previous computation, 2 (st) fails to 6 (t).
            //   Thus, check state 6 (t) for any transitions using 'a'.
            //   Since 6 (t) 'a' -> 7 (ta), therefore set fail [3] -> 7.
            
            state = m_States[r].GetOnFailure();
            FindFail(state, s, it->first);
        }
    }
}


template <typename MatchType>
void CTextFsm<MatchType>::FindFail(int state, int new_state, char ch)
{
    int        next;
    
    // traverse existing failure path 
    
    while ( (next = GetNextState(state, ch)) == eFailState) {
        if ( state == 0 ) {
            next = 0; break;
        }
        state = m_States[state].GetOnFailure();
    }
    
    // add new failure state 
    
    m_States[new_state].SetOnFailure(next);
    
    // add matches of substring at new state 
    
    copy( m_States[next].GetMatches().begin(), 
        m_States[next].GetMatches().end(),
        back_inserter(m_States[new_state].GetMatches()) );
}


template <typename MatchType>
const vector<MatchType>& CTextFsm<MatchType>::GetMatches(int state) const {
    return m_States[state].GetMatches();
}


template <typename MatchType>
bool CTextFsm<MatchType>::IsMatchFound(int state) const
{
    return m_States[state].IsMatchFound();
}


template <typename MatchType>
CTextFsm<MatchType>::~CTextFsm(void) 
{}


END_NCBI_SCOPE


/* @} */

#endif   // UTIL___STRSEARCH__HPP
