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
#include <string>
#include <vector>
using namespace std;

BEGIN_NCBI_SCOPE

 
//============================================================================//
//                            BoyerMooreMatcher                               //
//============================================================================//

//
// This implemetation uses the Boyer-Moore alg. in oreder to search a single
// pattern over varying texts.

class BoyerMooreMatcher 
{
public:
    // Initialize a matcher with the pattern to be matched.
    BoyerMooreMatcher(const string &pattern);

    // Search for the pattern over text starting at position pos.
    // Returns the position at which the pattern was found, -1 otherwise.
    int search(const string &text, unsigned int pos = 0) const;
    
private:
    static const int ALPHABET_SIZE;

    string m_pattern;  
    unsigned int m_patlen;
    vector<unsigned int> m_last_occurance;

};  // class BoyerMooreMatcher 


//============================================================================//
//                          Finite State Automata                             //
//============================================================================//



template <typename MatchType>
class CTextFsm
{
public:
    typedef map<char, int> TransitionTbl;
    static const int FAIL_STATE;

    // Constructor:
    CTextFsm(void);

    // FSM Construction
    void AddWord(const string &word);
    void AddWord(const string &word, const MatchType &match);
    void Prime(void);

    int GetInitialState(void) const { return 0; }
    int GetNextState(int state, char letter) const;

    bool IsMatchFound(int state) const;
    const vector<MatchType> &GetMatches(int state) const;

    // Destructor
    ~CTextFsm(void);
    
private:
    class CState
    {
    public:
        CState(void) : onfailure(0) {}

        void AddTransition (char letter, int to) { transitions[letter] = to; }
        int GetNextState (char letter) const {
           	TransitionTbl::const_iterator it = transitions.find(letter);
            return it != transitions.end() ?  it->second : FAIL_STATE;
        }

        
        bool IsMatchFound (void) const { return !matches.empty(); }
        void AddMatch (const MatchType &match) { matches.push_back(match); }

        vector<MatchType> &GetMatches(void) { return matches; }
        const vector<MatchType> &GetMatches(void) const { return matches; }

        const TransitionTbl &GetTransitions (void) const { return transitions; };

        void SetOnFailure(int state) { onfailure = state; }
        int GetOnFailure(void) const { return onfailure; }

        ~CState(void) {};
        
    private:
        TransitionTbl transitions;
        vector<MatchType> matches;
        int onfailure;
    };  // class CState

    int GetNextState(const CState &from, char letter) const;
    CState *GetState(int state);
    void ComputeFail(void);
    void FindFail (int state, int new_state, char ch);
    void QueueAdd(vector<int> &queue, int qbeg, int val);
      
    bool primed;
    vector< CState > states;
};  // class CTextFsm


typedef CTextFsm<string> CTextFsa;


// Public:
// =======

template <typename MatchType>
const int CTextFsm<MatchType>::FAIL_STATE = -1;


template <typename MatchType>
CTextFsm<MatchType>::CTextFsm(void) :
	primed(false)
{
	CState initial;
	states.push_back(initial);
}


template <typename MatchType>	
void CTextFsm<MatchType>::AddWord(const string &word, const MatchType &match) 
{
	int state = 0,
		next, i, word_len = word.length();

	// try to overlay beginning of word onto existing table 
	for ( i = 0; i < word_len; ++i ) {
		next = states[state].GetNextState(word[i]);
		if ( next == FAIL_STATE ) break;
		state = next;
	}

	// now create new states for remaining characters in word 
	for ( ;i < word_len; ++ i ) {
		CState new_state;

		states.push_back(new_state);
		states[state].AddTransition(word.at(i), states.size() - 1);
		state = states[state].GetNextState(word.at(i));
	}

	// add match information 
	states[state].AddMatch(match);
}


template <typename MatchType>
void CTextFsm<MatchType>::AddWord(const string &word) {
    AddWord(word, word);
}


template <typename MatchType>
void CTextFsm<MatchType>::Prime(void)
{
	if ( primed ) return;

	ComputeFail();

	primed = true;
}


template <typename MatchType>
typename CTextFsm<MatchType>::CState *CTextFsm<MatchType>::GetState(int state) 
{
    if ( (state < 0) || (state > states.size()) ) {
        return 0;
    }
    return &states[state];
}


template <typename MatchType>
int CTextFsm<MatchType>::GetNextState(const CState &from, char letter) const {
    return from.GetNextState(letter);
}


template <typename MatchType>
int CTextFsm<MatchType>::GetNextState(int state, char letter) const
{
    if ( state < 0 || state >= states.size() ) {
        return FAIL_STATE;
    }

	int next = GetNextState(states[state], letter);
    if ( next == FAIL_STATE ) {
        next = GetInitialState();
    }

    return next;
}


template <typename MatchType>
void CTextFsm<MatchType>::QueueAdd(vector<int> &queue, int qbeg, int val)
{
  int  q;

  q = queue [qbeg];
  if (q == 0) {
    queue [qbeg] = val;
  } else {
    for ( ; queue [q] != 0; q = queue [q]) continue;
    queue [q] = val;
  }
  queue [val] = 0;
}


template <typename MatchType>
void CTextFsm<MatchType>::ComputeFail(void) 
{
    int	qbeg, r, s, state;
    vector<int> queue(states.size());
    
    qbeg = 0;
    queue [0] = 0;
    
    // queue up states reached directly from state 0 (depth 1) 
    
    iterate ( TransitionTbl, it, states[GetInitialState()].GetTransitions() ) {
        s = it->second;
        states[s].SetOnFailure(0);
        QueueAdd (queue, qbeg, s);
    }
    
    while (queue [qbeg] != 0) {
        r = queue [qbeg];
        qbeg = r;
        
        // depth 1 states beget depth 2 states, etc. 
        
        iterate ( TransitionTbl, it, states[r].GetTransitions() ) {
            s = it->second;
            QueueAdd (queue, qbeg, s);
            
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
            
            state = states[r].GetOnFailure();
            FindFail(state, s, it->first);
        }
    }
}


template <typename MatchType>
void CTextFsm<MatchType>::FindFail (int state, int new_state, char ch)
{
    int        next;

    // traverse existing failure path 

    while ( (next = GetNextState(state, ch)) == FAIL_STATE) {
        if ( state == 0 ) {
            next = 0; break;
        }
        state = states[state].GetOnFailure();
    }

    // add new failure state 

    states[new_state].SetOnFailure(next);

    // add matches of substring at new state 

    copy( states[next].GetMatches().begin(), 
	   	states[next].GetMatches().end(),
	    back_inserter(states[new_state].GetMatches()) );
}


template <typename MatchType>
const vector<MatchType> &CTextFsm<MatchType>::GetMatches(int state) const {
    return states[state].GetMatches();
}


template <typename MatchType>
bool CTextFsm<MatchType>::IsMatchFound(int state) const
{
    return states[state].IsMatchFound();
}


template <typename MatchType>
CTextFsm<MatchType>::~CTextFsm(void) 
{}


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2002/10/29 16:32:52  kans
* initial checkin - Boyer-Moore string search and templated text search finite state machine (MS)
*
*
* ===========================================================================
*/


#endif   // UTIL___STRSEARCH__HPP

