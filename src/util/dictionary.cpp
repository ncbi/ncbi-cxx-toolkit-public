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
#include <algorithm>


BEGIN_NCBI_SCOPE


// maximum internal size for metaphone computation
// this is used to determine a heap vs stack allocation cutoff in the exact
// edit distance method; as CSimpleDictionary relies heavily on edit distance
// computations, the size of its internal metaphone keys is tuned by this
// parameter.
static const size_t kMaxMetaphoneStack = 10;


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



void CDictionaryUtil::GetMetaphone(const string& in, string* out,
                                   size_t max_chars)
{
    out->erase();
    if (in.empty()) {
        return;
    }

    ITERATE (string, iter, in) {
        size_t prev_len = iter - in.begin();
        size_t remaining = in.length() - prev_len - 1;

        if (prev_len  &&
            tolower(*iter) == tolower(*(iter - 1))  &&
            tolower(*iter) != 'c') {
            continue;
        }
        switch (tolower(*iter)) {
        case 'a':
        case 'e':
        case 'i':
        case 'o':
        case 'u':
            if ( !prev_len ) {
                *out += 'a';
                ++max_chars;
            }
            break;

        case 'b':
            if (remaining != 0  ||  *(iter - 1) != 'm') {
                *out += 'p';
            }
            break;

        case 'f':
        case 'j':
        case 'l':
        case 'n':
        case 'r':
            *out += tolower(*iter);
            break;

        case 'c':
            if (remaining > 2  &&
                *(iter + 1) == 'i'  &&
                *(iter + 2) == 'a') {
                *out += 'x';
                iter += 2;
                break;
            }

            if (remaining > 1  &&  *(iter + 1) == 'h') {
                *out += 'x';
                ++iter;
                break;
            }

            if (remaining  &&
                ( *(iter + 1) == 'e'  ||
                  *(iter + 1) == 'i'  ||
                  *(iter + 1) == 'y' ) ) {
                *out += 's';
                ++iter;
                break;
            }

            if (remaining  &&  *(iter + 1) == 'k') {
                ++iter;
            }
            *out += 'k';
            break;

        case 'd':
            if (remaining >= 2  &&  prev_len) {
                if ( *(iter + 1) == 'g'  &&
                     ( *(iter + 2) == 'e'  ||
                       *(iter + 2) == 'i'  ||
                       *(iter + 2) == 'y' ) ) {
                    *out += 'j';
                    iter += 2;
                    break;
                }
            }
            *out += 't';
            break;

        case 'g':
            if (remaining == 1  &&  *(iter + 1) == 'h') {
                if (prev_len > 2  &&  ( *(iter - 3) == 'b'  ||
                                        *(iter - 3) == 'd') ) {
                    *out += 'k';
                    ++iter;
                    break;
                }

                if (prev_len > 3  &&  *(iter - 3) == 'h') {
                    *out += 'k';
                    ++iter;
                    break;
                }
                if (prev_len > 4  &&  *(iter - 4) == 'h') {
                    *out += 'k';
                    ++iter;
                    break;
                }

                *out += 'f';
                ++iter;
                break;
            }

            if (remaining == 1  &&
                (*(iter + 1) == 'n'  ||  *(iter + 1) == 'm')) {
                ++iter;
                break;
            }

            if (remaining  &&  !prev_len  &&  *(iter + 1) == 'n') {
                ++iter;
                *out += 'n';
                break;
            }

            if (remaining == 3  &&
                *(iter + 1) == 'n'  &&
                *(iter + 1) == 'e'  &&
                *(iter + 1) == 'd') {
                iter += 3;
                break;
            }

            if ( (remaining > 1  &&  *(iter + 1) == 'e')  ||
                 (remaining  &&  ( *(iter + 1) == 'i'  ||
                                   *(iter + 1) == 'y' ) ) ) {
                *out += 'j';
                ++iter;
                break;
            }

            *out += 'k';
            break;

        case 'h':
            if (remaining  &&  prev_len  &&
                ( *(iter + 1) == 'a'  ||
                  *(iter + 1) == 'e'  ||
                  *(iter + 1) == 'i'  ||
                  *(iter + 1) == 'o'  ||
                  *(iter + 1) == 'u') &&
                *(iter - 1) != 'c'  &&
                *(iter - 1) != 'g'  &&
                *(iter - 1) != 'p'  &&
                *(iter - 1) != 's'  &&
                *(iter - 1) != 't') {
                *out += tolower(*iter);
                ++iter;
            }
            break;

        case 'm':
        case 'k':
            if (!prev_len  &&  remaining  &&  *(iter + 1) == 'n') {
                ++iter;
                *out += 'n';
                break;
            }
            *out += tolower(*iter);
            break;

        case 'p':
            if (prev_len == 0  &&  remaining  &&  *(iter + 1) == 'n') {
                ++iter;
                *out += 'n';
                break;
            }
            if (remaining  &&  *(iter + 1) == 'h') {
                *out += 'f';
                break;
            }
            *out += tolower(*iter);
            break;

        case 'q':
            *out += 'k';
            break;

        case 's':
            if (remaining > 2  &&
                *(iter + 1) == 'i'  &&
                ( *(iter + 2) == 'o'  ||
                  *(iter + 2) == 'a' ) ) {
                iter += 2;
                *out += 'x';
                break;
            }
            if (remaining  &&  *(iter + 1) == 'h') {
                *out += 'x';
                ++iter;
                break;
            }
            if (remaining > 2  &&
                *(iter + 1) == 'c'  &&
                ( *(iter + 2) == 'e'  ||
                  *(iter + 2) == 'i'  ||
                  *(iter + 2) == 'y' ) ) {
                iter += 2;
            }
            *out += 's';
            break;

        case 't':
            if (remaining > 2  &&
                *(iter + 1) == 'i'  &&
                ( *(iter + 2) == 'o'  ||
                  *(iter + 2) == 'a' ) ) {
                iter += 2;
                *out += 'x';
                break;
            }
            if (remaining  &&  *(iter + 1) == 'h') {
                *out += 'o';
                ++iter;
                break;
            }
            *out += tolower(*iter);
            break;

        case 'v':
            *out += 'f';
            break;

        case 'w':
            if ( !prev_len ) {
                if (*(iter + 1) == 'h'  ||
                    *(iter + 1) == 'r') {
                    *out += *(iter + 1);
                    ++iter;
                    break;
                }
                *out += tolower(*iter);
                break;
            }

            if ( *(iter - 1) == 'a'  ||
                 *(iter - 1) == 'e'  ||
                 *(iter - 1) == 'i'  ||
                 *(iter - 1) == 'o'  ||
                 *(iter - 1) == 'u') {
                *out += tolower(*iter);
            }
            break;

        case 'x':
            *out += "ks";
            break;

        case 'y':
            if ( *(iter + 1) == 'a'  ||
                 *(iter + 1) == 'e'  ||
                 *(iter + 1) == 'i'  ||
                 *(iter + 1) == 'o'  ||
                 *(iter + 1) == 'u') {
                break;
            }
            if ( *(iter + 1) != 'a'  &&
                 *(iter + 1) != 'e'  &&
                 *(iter + 1) != 'i'  &&
                 *(iter + 1) != 'o'  &&
                 *(iter + 1) != 'u'  &&

                 *(iter - 1) != 'a'  &&
                 *(iter - 1) != 'e'  &&
                 *(iter - 1) != 'i'  &&
                 *(iter - 1) != 'o'  &&
                 *(iter - 1) != 'u') {
                break;
            }
            *out += tolower(*iter);
            break;

        case 'z':
            *out += 's';
            break;
        }

        if (out->length() == max_chars) {
            break;
        }

    }

    //_TRACE("GetMetaphone(): " << in << " -> " << *out);
}


void CDictionaryUtil::GetSoundex(const string& in, string* out,
                                 size_t max_chars, char pad_char)
{
    static const char sc_SoundexLut[256] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, '1',  '2',  '3',  0x00, '1',  '2', 
        0x00, 0x00, '2',  '2',  '4',  '5',  '5',  0x00, 
        '1',  '2',  '6',  '2',  '3',  0x00, '1',  0x00, 
        '2',  0x00, '2',  0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, '1',  '2',  '3',  0x00, '1',  '2', 
        0x00, 0x00, '2',  '2',  '4',  '5',  '5',  0x00, 
        '1',  '2',  '6',  '2',  '3',  0x00, '1',  0x00, 
        '2',  0x00, '2',  0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    // basic sanity
    out->erase();
    if (in.empty()) {
        return;
    }

    // preserve the first character, in upper case
    string::const_iterator iter = in.begin();
    *out += toupper(*iter);

    // now, iterate substituting codes, using no more than four characters
    // total
    ITERATE (string, iter2, in) {
        char c = sc_SoundexLut[(int)(unsigned char)*iter2];
        if (c  &&  *(out->end() - 1) != c) {
            *out += c;
            if (out->length() == max_chars) {
                break;
            }
        }
    }

    // pad with our pad character
    if (out->length() < max_chars) {
        *out += string(max_chars - out->length(), pad_char);
    }
}


size_t CDictionaryUtil::GetEditDistance(const string& str1,
                                        const string& str2,
                                        EDistanceMethod method)
{
    switch (method) {
    case eEditDistance_Similar:
        {{
            /// it lgically makes no difference which string
            /// we look at as the master; we choose the shortest to make
            /// some of the logic work better (also, it yields more accurate
            /// results)
            const string* pstr1 = &str1;
            const string* pstr2 = &str2;
            if (pstr1->length() > pstr2->length()) {
                swap(pstr1, pstr2);
            }
            size_t dist = 0;
            string::const_iterator iter1 = pstr1->begin();
            string::const_iterator iter2 = pstr2->begin();
            for ( ;  iter1 != pstr1->end()  &&  iter2 != pstr2->end();  ) {
                char c1_0 = tolower(*iter1);
                char c2_0 = tolower(*iter2);
                if (c1_0 == c2_0) {
                    /// identity: easy out
                    ++iter1;
                    ++iter2;
                    continue;
                }

                /// we scan for a match, starting from the corner formed
                /// as we march forward a few letters.  We use a maximum
                /// of 3 letters as our limit
                int max_radius = min(pstr1->end() - iter1,
                                     string::difference_type(3));

                string::const_iterator best_iter1 = iter1 + 1;
                string::const_iterator best_iter2 = iter2 + 1;
                size_t cost = 1;

                for (int radius = 1;  radius <= max_radius;  ++radius) {

                    char corner1 = *(iter1 + radius);
                    char corner2 = *(iter2 + radius);
                    bool match = false;
                    for (int i = radius;  i >= 0;  --i) {
                        c1_0 = tolower(*(iter1 + i));
                        c2_0 = tolower(*(iter2 + i));
                        if (c1_0 == corner2) {
                            match = true;
                            cost = radius;
                            best_iter1 = iter1 + i;
                            best_iter2 = iter2 + radius;
                            break;
                        }
                        if (c2_0 == corner1) {
                            match = true;
                            cost = radius;
                            best_iter1 = iter1 + radius;
                            best_iter2 = iter2 + i;
                            break;
                        }
                    }
                    if (match) {
                        break;
                    }
                }

                dist += cost;
                iter1 = best_iter1;
                iter2 = best_iter2;
            }
            dist += (pstr1->end() - iter1) + (pstr2->end() - iter2);
            return dist;
         }}

    case eEditDistance_Exact:
        {{
            size_t buf0[kMaxMetaphoneStack + 1];
            size_t buf1[kMaxMetaphoneStack + 1];
            vector<size_t> row0;
            vector<size_t> row1;

            const string* short_str = &str1;
            const string* long_str = &str2;
            if (str2.length() < str1.length()) {
                swap(short_str, long_str);
            }

            size_t* row0_ptr = buf0;
            size_t* row1_ptr = buf1;
            if (short_str->size() > kMaxMetaphoneStack) {
                row0.resize(short_str->size() + 1);
                row1.resize(short_str->size() + 1);
                row0_ptr = &row0[0];
                row1_ptr = &row1[0];
            }

            size_t i;
            size_t j;

            //cout << "   ";
            for (i = 0;  i < short_str->size() + 1;  ++i) {
                //cout << (*short_str)[i] << "  ";
                row0_ptr[i] = i;
                row1_ptr[i] = i;
            }
            //cout << endl;

            for (i = 0;  i < long_str->size();  ++i) {
                row1_ptr[0] = i + 1;
                //cout << (*long_str)[i] << " ";
                for (j = 0;  j < short_str->size();  ++j) {
                    int c0 = tolower((*short_str)[j]);
                    int c1 = tolower((*long_str)[i]);
                    size_t cost = (c0 == c1 ? 0 : 1);
                    row1_ptr[j + 1] =
                        min(row0_ptr[j] + cost,
                            min(row0_ptr[j + 1] + 1, row1_ptr[j] + 1));
                    //cout << setw(2) << row1_ptr[j + 1] << " ";
                }

                //cout << endl;

                swap(row0_ptr, row1_ptr);
            }

            return row0_ptr[short_str->size()];
         }}
    }

    // undefined
    return (size_t)-1;
}


/// Compute a nearness score for two different words or phrases
int CDictionaryUtil::Score(const string& word1, const string& word2,
                           size_t max_metaphone)
{
    string meta1;
    string meta2;
    GetMetaphone(word1, &meta1, max_metaphone);
    GetMetaphone(word2, &meta2, max_metaphone);
    return Score(word1, meta1, word2, meta2);
}


/// Compute a nearness score based on metaphone as well as raw distance
int CDictionaryUtil::Score(const string& word1, const string& meta1,
                           const string& word2, const string& meta2,
                           EDistanceMethod method)
{
    // score:
    // start with edit distance
    size_t score = CDictionaryUtil::GetEditDistance(word1, word2, method);

    // normalize to length of word
    // this allows negative scores to be omittied
    score = word1.length() - score;

    // down-weight for metaphone distance
    score -= CDictionaryUtil::GetEditDistance(meta1, meta2, method);

    // one point for first letter of word being same
    //score += (tolower(word1[0]) == tolower(word2[0]));

    // one point for identical lengths of words
    //score += (word1.length() == word2.length());

    return score;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2005/02/01 21:47:15  grichenk
 * Fixed warnings
 *
 * Revision 1.6  2004/10/28 18:41:47  dicuccio
 * Implemented CSimpleDictionary::Write()
 *
 * Revision 1.5  2004/08/17 17:14:56  ucko
 * Fix CDictionaryUtil::GetEditDistance to compile on 64-bit platforms.
 *
 * Revision 1.4  2004/08/17 13:26:39  dicuccio
 * Large update.  Added more constructors for CSimpleDictionary; support
 * pre-computed metaphone keys in CSimpleDictionary; fixed several bugs in edit
 * distance computation
 *
 * Revision 1.3  2004/08/02 15:09:15  dicuccio
 * Parameterized metaphone key size.  Made data members of CSimpleDictionary
 * protected (not private)
 *
 * Revision 1.2  2004/07/16 18:36:12  ucko
 * Use string::erase() rather than string::clear(), which some older
 * compilers lack.
 *
 * Revision 1.1  2004/07/16 15:33:07  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
