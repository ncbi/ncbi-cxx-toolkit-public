#ifndef UTIL___DICTIONARY__HPP
#define UTIL___DICTIONARY__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *    CDicitionary -- basic dictionary interface
 *    CSimpleDictionary -- simplified dictionary, supports forward lookups and
 *                         phonetic matches
 *    CMultiDictionary -- multiplexes queries across a set of dictionaries
 *    CCachedDictionary -- supports caching results from previous lookus in any
 *                       IDictionary interface.
 *    CDictionaryUtil -- string manipulation techniques used by the dictionary
 *                       system
 */

#include <corelib/ncbiobj.hpp>
#include <set>
#include <map>


BEGIN_NCBI_SCOPE


///
/// class IDictionary defines an abstract interface for dictionaries.
/// All dictionaries must provide a means of checking whether a single word
/// exists in the dictionary; in addition, there is a mechanism for returning a
/// suggested (ranked) list of alternates to the user.
///
class IDictionary : public CObject
{
public:
    /// SAlternate wraps a word and its score.  The meaning of 'score' is
    /// intentionally vague; in practice, this is the length of the word minus
    /// the edit distance from the query, plus a few modifications to favor
    /// near phonetic matches
    struct SAlternate {
        SAlternate()
            : score(0) { }

        string alternate;
        int score;
    };
    typedef vector<SAlternate> TAlternates;

    /// functor for sorting alternates list by score
    struct SAlternatesByScore {
        PNocase nocase;

        bool operator() (const IDictionary::SAlternate& alt1,
                        const IDictionary::SAlternate& alt2) const
        {
            if (alt1.score == alt2.score) {
                return nocase.Less(alt1.alternate, alt2.alternate);
            }
            return (alt1.score > alt2.score);
        }
    };


    virtual ~IDictionary() { }

    /// Virtual requirement: check a word for existence in the dictionary.
    virtual bool CheckWord(const string& word) const = 0;

    /// Scan for a list of words similar to the indicated word
    virtual void SuggestAlternates(const string& word,
                                   TAlternates& alternates,
                                   size_t max_alternates = 20) const = 0;
};



///
/// class CSimpleDictionary implements a simple dictionary strategy, providing
/// forward and reverse phonetic look-ups.  This class has the ability to
/// suggest a list of alternates based on phonetic matches.
///
class NCBI_XUTIL_EXPORT CSimpleDictionary : public IDictionary
{
public:
    /// initialize the dictionary with a set of correctly spelled words.  The
    //words must be one word per line, and will be automatically lower-cased.
    CSimpleDictionary(const string& file, size_t metaphone_key_size = 5);

    /// Virtual requirement: check a word for existence in the dictionary.
    /// This function provides a mechanism for returning a list of alternates to
    /// the user as well.
    bool CheckWord(const string& word) const;

    /// Scan for a list of words similar to the indicated word
    void SuggestAlternates(const string& word,
                           TAlternates& alternates,
                           size_t max_alternates = 20) const;

    /// Add a word to the dictionary
    void AddWord(const string& str);

protected:

    /// forward dictionary: word -> phonetic representation
    typedef map<string, string, PNocase> TForwardDict;
    TForwardDict m_ForwardDict;

    /// reverse dictionary: soundex/metaphone -> word
    typedef set<string, PNocase> TNocaseStringSet;
    typedef map<string, TNocaseStringSet> TReverseDict;
    TReverseDict m_ReverseDict;

    /// the size of our metaphone keys
    const size_t m_MetaphoneKeySize;
};


///
/// class CMultiDictionary permits the creation of a linked, prioritized set of
/// dictionaries.
///
class NCBI_XUTIL_EXPORT CMultiDictionary : public IDictionary
{
public:
    enum EPriority {
        ePriority_Low = 100,
        ePriority_Medium = 50,
        ePrioritycwHigh_Low = 0,

        ePriority_Default = ePriority_Medium
    };

    struct SDictionary {
        CRef<IDictionary> dict;
        int priority;
    };
    
    typedef vector<SDictionary> TDictionaries;

    bool CheckWord(const string& word) const;

    /// Scan for a list of words similar to the indicated word
    void SuggestAlternates(const string& word,
                           TAlternates& alternates,
                           size_t max_alternates = 20) const;

    void RegisterDictionary(IDictionary& dict,
                            int priority = ePriority_Default);

private:
    
    TDictionaries m_Dictionaries;
};


///
/// class CCachedDictionary provides an internalized mechanism for caching the
/// alternates returned by SuggestAlternates().  This class should be used only
/// if the number of words to be checked against an existing dictionary is
/// large; for a small number of words, there will likely be little benefit and
/// some incurred overhad for using this class.
///
class NCBI_XUTIL_EXPORT CCachedDictionary : public IDictionary
{
public:
    CCachedDictionary(IDictionary& dict);

    bool CheckWord(const string& word) const;
    void SuggestAlternates(const string& word,
                           TAlternates& alternates,
                           size_t max_alternates = 20) const;

private:
    CRef<IDictionary> m_Dict;

    typedef map<string, IDictionary::TAlternates, PNocase> TAltCache;
    mutable TAltCache m_Misses;
};



///
/// Standard dictionary utility functions
///
class NCBI_XUTIL_EXPORT CDictionaryUtil
{
public:
    enum {
        eMaxSoundex = 4,
        eMaxMetaphone = 4
    };

    /// Compute the Soundex key for a given word
    /// The Soundex key is defined as:
    ///   - the first leter of the word, capitalized
    ///   - convert the remaining letters based on simple substitutions and
    ///     groupings of similar letters
    ///   - remove duplicates
    ///   - pad with '0' to the maximum number of characters
    ///
    /// The final step is non-standard; the usual pad is ' '
    static void GetSoundex(const string& in, string* out,
                           size_t max_chars = eMaxSoundex,
                           char pad_char = '0');

    /// Compute the Metaphone key for a given word
    /// Metaphone is a more advanced algorithm than Soundex; instead of
    /// matching simple letters, Metaphone matches diphthongs.  The rules are
    /// complex, and try to match how languages are pronounced.  The
    /// implementation here borrows some options from Double Metaphone; the
    /// modifications from the traditional Metaphone algorithm include:
    ///  - all leading vowels are rendered as 'a' (from Double Metaphone)
    ///  - rules regarding substitution of dge/dgi/dgy -> j were loosened a bit
    ///  to permit such substitutions at the end of the word
    ///  - 'y' is treated as a vowel if surrounded by consonants
    static void GetMetaphone(const string& in, string* out,
                             size_t max_chars = eMaxMetaphone);

    /// Return the Levenshtein edit distance between two words.  Two possible
    /// methods of computation are supported - an exact method with quadratic
    /// complexity and a method suitable for similar words with a near-linear
    /// complexity.  The similar algorithm is suitable for almost all words we
    /// would encounter; it will render inaccuracies if the number of
    /// consecutive differences is greater than three.

    enum EDistanceMethod {
        /// This method performs an exhausive search, and has an
        /// algorithmic complexity of O(n x m), where n = length of str1 and 
        /// m = length of str2
        eEditDistance_Exact,

        /// This method performs a simpler search, looking for the distance
        /// between similar words.  Words with more than two consecutively
        /// different characters will be scored incorrectly.
        eEditDistance_Similar
    };
    static size_t GetEditDistance(const string& str1, const string& str2,
                                  EDistanceMethod method = eEditDistance_Exact);

};



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/08/02 15:09:15  dicuccio
 * Parameterized metaphone key size.  Made data members of CSimpleDictionary
 * protected (not private)
 *
 * Revision 1.1  2004/07/16 15:32:55  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /// UTIL___DICTIONARY__HPP
