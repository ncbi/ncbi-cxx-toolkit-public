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
 *
 */

#include <ncbi_pch.hpp>
#include <util/dictionary.hpp>
#include <util/dictionary_util.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE



CSimpleDictionary::CSimpleDictionary(size_t meta_key_size)
    : m_MetaphoneKeySize(meta_key_size)
{
}


CSimpleDictionary::CSimpleDictionary(const string& file,
                                     size_t meta_key_size)
    : m_MetaphoneKeySize(meta_key_size)
{
    CNcbiIfstream istr(file.c_str());
    Read(istr);
}


CSimpleDictionary::CSimpleDictionary(CNcbiIstream& istr,
                                     size_t meta_key_size)
    : m_MetaphoneKeySize(meta_key_size)
{
    Read(istr);
}


void CSimpleDictionary::Read(CNcbiIstream& istr)
{
    string line;
    string metaphone;
    string word;
    while (NcbiGetlineEOL(istr, line)) {
        string::size_type pos = line.find_first_of("|");
        if (pos == string::npos) {
            word = line;
            CDictionaryUtil::GetMetaphone(word, &metaphone, m_MetaphoneKeySize);
        } else {
            metaphone = line.substr(0, pos);
            word = line.substr(pos + 1, line.length() - pos - 1);
        }

        // insert forward and reverse dictionary pairings
        m_ForwardDict.insert(m_ForwardDict.end(), word);
        TStringSet& word_set = m_ReverseDict[metaphone];
        word_set.insert(word_set.end(), word);
    }
}

void CSimpleDictionary::Write(CNcbiOstream& ostr) const
{
    ITERATE (TReverseDict, iter, m_ReverseDict) {
        ITERATE (TStringSet, word_iter, iter->second) {
            ostr << iter->first << "|" << *word_iter << endl;
        }
    }
}


void CSimpleDictionary::AddWord(const string& word)
{
    if (word.empty()) {
        return;
    }

    // compute the metaphone code
    string metaphone;
    CDictionaryUtil::GetMetaphone(word, &metaphone, m_MetaphoneKeySize);

    // insert forward and reverse dictionary pairings
    m_ForwardDict.insert(word);
    m_ReverseDict[metaphone].insert(word);
}


bool CSimpleDictionary::CheckWord(const string& word) const
{
    TForwardDict::const_iterator iter = m_ForwardDict.find(word);
    return (iter != m_ForwardDict.end());
}


void CSimpleDictionary::SuggestAlternates(const string& word,
                                          TAlternates& alternates,
                                          size_t max_alts) const
{
    string metaphone;
    CDictionaryUtil::GetMetaphone(word, &metaphone, m_MetaphoneKeySize);
    list<TReverseDict::const_iterator> keys;
    x_GetMetaphoneKeys(metaphone, keys);

    typedef set<SAlternate, SAlternatesByScore> TAltSet;
    TAltSet words;

    SAlternate alt;
    size_t count = 0;
    ITERATE (list<TReverseDict::const_iterator>, key_iter, keys) {

        bool used = false;
        ITERATE (TStringSet, set_iter, (*key_iter)->second) {
            // score:
            // start with edit distance
            alt.score = 
                CDictionaryUtil::Score(word, metaphone,
                                       *set_iter, (*key_iter)->first);
            if (alt.score <= 0) {
                continue;
            }

            _TRACE("  hit: "
                   << metaphone << " <-> " << (*key_iter)->first
                   << ", " << word << " <-> " << *set_iter
                   << " (" << alt.score << ")");
            used = true;
            alt.alternate = *set_iter;
            words.insert(alt);
        }
        count += used ? 1 : 0;
    }

    _TRACE(word << ": "
           << keys.size() << " keys searched "
           << count << " keys used");

    if ( !words.empty() ) {
        TAlternates alts;
        TAltSet::const_iterator iter = words.begin();
        alts.push_back(*iter);
        TAltSet::const_iterator prev = iter;
        for (++iter;
             iter != words.end()  &&
             (alts.size() < max_alts  ||  prev->score == iter->score);
             ++iter) {
            alts.push_back(*iter);
            prev = iter;
        }

        alternates.insert(alternates.end(), alts.begin(), alts.end());
    }
}


void CSimpleDictionary::x_GetMetaphoneKeys(const string& metaphone,
                                           list<TReverseDict::const_iterator>& keys) const
{
    if ( !metaphone.length() ) {
        return;
    }

    const size_t max_meta_edit_dist = 1;
    const CDictionaryUtil::EDistanceMethod method =
        CDictionaryUtil::eEditDistance_Similar;

    string::const_iterator iter = metaphone.begin();
    string::const_iterator end  = iter + max_meta_edit_dist + 1;

    size_t count = 0;
    _TRACE("meta key: " << metaphone);
    for ( ;  iter != end;  ++iter) {
        string seed(1, *iter);
        _TRACE("  meta key seed: " << seed);
        TReverseDict::const_iterator lower = m_ReverseDict.lower_bound(seed);
        for ( ;
              lower != m_ReverseDict.end()  &&  lower->first[0] == *iter;
              ++lower, ++count) {

            size_t dist =
                CDictionaryUtil::GetEditDistance(lower->first, metaphone,
                                                 method);
            if (dist > max_meta_edit_dist) {
                continue;
            }

            keys.push_back(lower);
        }
    }

    _TRACE("exmained " << count << " keys, returning " << keys.size());
}


/////////////////////////////////////////////////////////////////////////////
///
/// CMultiDictionary
///

struct SDictByPriority {
    bool operator()(const CMultiDictionary::SDictionary& d1,
                    const CMultiDictionary::SDictionary& d2) const
    {
        return (d1.priority < d2.priority);
    }
};

void CMultiDictionary::RegisterDictionary(IDictionary& dict, int priority)
{
    SDictionary d;
    d.dict.Reset(&dict);
    d.priority = priority;

    m_Dictionaries.push_back(d);
    std::sort(m_Dictionaries.begin(), m_Dictionaries.end(), SDictByPriority());
}


bool CMultiDictionary::CheckWord(const string& word) const
{
    ITERATE (TDictionaries, iter, m_Dictionaries) {
        if ( iter->dict->CheckWord(word) ) {
            return true;
        }
    }

    return false;
}


void CMultiDictionary::SuggestAlternates(const string& word,
                                         TAlternates& alternates,
                                         size_t max_alts) const
{
    TAlternates alts;
    ITERATE (TDictionaries, iter, m_Dictionaries) {
        iter->dict->SuggestAlternates(word, alts, max_alts);
    }

    std::sort(alts.begin(), alts.end(), SAlternatesByScore());
    if (alts.size() > max_alts) {
        TAlternates::iterator prev = alts.begin() + max_alts;
        TAlternates::iterator iter = prev;
        ++iter;
        for ( ;  iter != alts.end()  && iter->score == prev->score;  ++iter) {
            prev = iter;
        }
        alts.erase(iter, alts.end());
    }

    alternates.swap(alts);
}


CCachedDictionary::CCachedDictionary(IDictionary& dict)
    : m_Dict(&dict)
{
}


bool CCachedDictionary::CheckWord(const string& word) const
{
    return m_Dict->CheckWord(word);
}


void CCachedDictionary::SuggestAlternates(const string& word,
                                          TAlternates& alternates,
                                          size_t max_alts) const
{
    TAltCache::iterator iter = m_Misses.find(word);
    if (iter != m_Misses.end()) {
        alternates = iter->second;
        return;
    }

    m_Dict->SuggestAlternates(word, m_Misses[word], max_alts);
    alternates = m_Misses[word];
}


END_NCBI_SCOPE
