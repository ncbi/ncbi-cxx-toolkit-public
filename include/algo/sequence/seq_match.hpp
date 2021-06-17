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
 * Authors:  Josh Cherry
 *
 * File Description:  Simple pattern matching for sequences
 *
 */


#ifndef GUI_CORE_ALGO_BASIC___SEQ_MATCH__HPP
#define GUI_CORE_ALGO_BASIC___SEQ_MATCH__HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

///
/// This class provides functions for determining where
/// sequences, perhaps containing ambiguity codes,
/// must or can match patterns.
///
/// The functions are highly templatized.  The main
/// reason is to allow reuse of pattern-matching code with
/// different 'alphabets'.  A second reason is to allow use
/// of different container classes for sequences and patterns,
/// e.g., the ncbi8na matching will work with both
/// string and vector<char>.
/// This would be much nicer with template template parameters,
/// but MSVC doesn't support them.
///

class CSeqMatch
{
public:
    enum EMatch {
        eNo,
        eYes,
        eMaybe
    };
    /// determine whether ncbi8na base s is a match to q.
    static EMatch CompareNcbi8na(char s, char q);
    /// determine whether seq matches pattern pat starting at position pos.
    ///
    /// It is the caller's responsibility to ensure that there are
    /// enough 'characters' in seq, i.e., that pos + pat.size() <= seq.size()
    template<class Seq, class Pat, class Compare_fun>
    static EMatch Match(const Seq& seq, const Pat& pat, TSeqPos pos,
                        const Compare_fun compare_fun)
    {

        // for efficiency, no check is made that we're not looking
        // past the end of seq; caller must assure this

        EMatch rv = eYes;
        // check pattern positions in succession
        for (unsigned int i = 0;  i < pat.size();  i++) {
            EMatch res = compare_fun(seq[pos+i], pat[i]);
            if (res == eNo) {
                return eNo;
            }
            if (res == eMaybe) {
                rv = eMaybe;
            }
        }
        // if we got here, everybody at least could have matched
        return rv;
    }

    template<class Seq, class Pat>
    static EMatch MatchNcbi8na(const Seq& seq,
                               const Pat& pat, TSeqPos pos)
    {
        return Match(seq, pat, pos, CompareNcbi8na);
    }

    template <class Seq, class Pat>
    struct SMatchNcbi8na
    {
        EMatch operator() (const Seq& seq,
                           const Pat& pat, TSeqPos pos) const
        {
            return CSeqMatch::Match(seq, pat, pos, CompareNcbi8na);
        }
    };

    /// find all places where seq must or might match pat
    template<class Seq, class Pat, class Match_fun>
    static void FindMatches(const Seq& seq,
                            const Pat& pat,
                            vector<TSeqPos>& definite_matches,
                            vector<TSeqPos>& possible_matches,
                            Match_fun match)
    {
        for (unsigned int i = 0;  i < seq.size() - pat.size() + 1; i++) {
            EMatch res = match(seq, pat, i);
            if (res == eNo) {
                continue;
            }
            if (res == eYes) {
                definite_matches.push_back(i);
                continue;
            }
            // otherwise must be eMaybe
            possible_matches.push_back(i);
        }
    }

    template<class Seq, class Pat>
    static void FindMatchesNcbi8na(const Seq& seq,
                                   const Pat& pat,
                                   vector<TSeqPos>& definite_matches,
                                   vector<TSeqPos>& possible_matches)
    {
        FindMatches(seq, pat,
                    definite_matches, possible_matches,
                    SMatchNcbi8na<Seq, Pat>());
    }

    /// stuff for dealing with ncbi8na.
    /// doesn't really belong here, but oh well

    /// convert a single base from IUPAC to ncbi8na
    NCBI_XALGOSEQ_EXPORT static char IupacToNcbi8na(char in);
    /// convert a whole string from IUPAC to ncbi8na
    NCBI_XALGOSEQ_EXPORT static void IupacToNcbi8na(const string& in, string& out);
    /// complement an ncbi8na sequence in place
    NCBI_XALGOSEQ_EXPORT static void CompNcbi8na(string& seq8na);
    /// complement a single ncbi8na base
    NCBI_XALGOSEQ_EXPORT static char CompNcbi8na(char);
};





// works on ncbi8na
// s can match q iff they have some set bits in common
// s must match q iff it represents a subset,
// i.e., if no bits set in s are unset in q
inline
CSeqMatch::EMatch CSeqMatch::CompareNcbi8na(char s, char q)
{
    if (!(s & q)) {
        // nothing in common
        return eNo;
    }
    if (s & ~q) {
        return eMaybe;
    }
    return eYes;
}


END_NCBI_SCOPE

#endif   // GUI_CORE_ALGO_BASIC___SEQ_MATCH__HPP
