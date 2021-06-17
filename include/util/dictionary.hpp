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
    CSimpleDictionary(size_t metaphone_key_size = 5);

    /// initialize the dictionary with a set of correctly spelled words.  The
    /// words must be one word per line
    CSimpleDictionary(const string& file, size_t metaphone_key_size = 5);

    /// initialize the dictionary with a set of correctly spelled words.  The
    /// words must be one word per line
    CSimpleDictionary(CNcbiIstream& file, size_t metaphone_key_size = 5);

    void Read(CNcbiIstream& istr);
    void Write(CNcbiOstream& ostr) const;

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
    typedef set<string, PNocase> TForwardDict;
    TForwardDict m_ForwardDict;

    /// reverse dictionary: soundex/metaphone -> word
    typedef set<string> TStringSet;
    typedef map<string, TStringSet> TReverseDict;
    TReverseDict m_ReverseDict;

    /// the size of our metaphone keys
    const size_t m_MetaphoneKeySize;

    void x_GetMetaphoneKeys(const string& metaphone,
                            list<TReverseDict::const_iterator>& keys) const;
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




END_NCBI_SCOPE

#endif  /// UTIL___DICTIONARY__HPP
