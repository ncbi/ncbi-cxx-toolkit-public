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
#include <util/static_map.hpp>
#include <util/dictionary_util.hpp>

BEGIN_NCBI_SCOPE


// maximum internal size for metaphone computation
// this is used to determine a heap vs stack allocation cutoff in the exact
// edit distance method; as CSimpleDictionary relies heavily on edit distance
// computations, the size of its internal metaphone keys is tuned by this
// parameter.
static const size_t kMaxMetaphoneStack = 10;


void CDictionaryUtil::GetMetaphone(const string& in, string* out,
                                   size_t max_chars)
{
    out->erase();
    if (in.empty()) {
        return;
    }

    CTempString vowels("aeiou");

    ITERATE (string, iter, in) {
        size_t prev_len = iter - in.begin();
        size_t remaining = in.length() - prev_len - 1;

        if (prev_len  &&
            tolower((unsigned char)(*iter)) == tolower((unsigned char)(*(iter - 1)))  &&
            tolower((unsigned char)(*iter)) != 'c') {
            continue;
        }
        switch (tolower((unsigned char)(*iter))) {
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
            if (remaining != 0  ||
                !prev_len  ||
                *(iter - 1) != 'm') {
                *out += 'p';
            }

            break;

        case 'f':
        case 'j':
        case 'l':
        case 'n':
        case 'r':
            *out += tolower((unsigned char)(*iter));
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
                vowels.find(*(iter + 1)) != string::npos  &&
                CTempString("cgpst").find(*(iter - 1)) == string::npos) {
                *out += tolower((unsigned char)(*iter));
                ++iter;
            }
            else if ( !prev_len ) {
                *out += tolower((unsigned char)(*iter));
            }
            break;

        case 'm':
        case 'k':
            if (!prev_len  &&  remaining  &&  *(iter + 1) == 'n') {
                ++iter;
                *out += 'n';
                break;
            }
            *out += tolower((unsigned char)(*iter));
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
            *out += tolower((unsigned char)(*iter));
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
            *out += tolower((unsigned char)(*iter));
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
                *out += tolower((unsigned char)(*iter));
                break;
            }

            if ( *(iter - 1) == 'a'  ||
                 *(iter - 1) == 'e'  ||
                 *(iter - 1) == 'i'  ||
                 *(iter - 1) == 'o'  ||
                 *(iter - 1) == 'u') {
                *out += tolower((unsigned char)(*iter));
            }
            break;

        case 'x':
            *out += "ks";
            break;

        case 'y':
            if (remaining  &&  prev_len  &&
                ( *(iter + 1) == 'a'  ||
                  *(iter + 1) == 'e'  ||
                  *(iter + 1) == 'i'  ||
                  *(iter + 1) == 'o'  ||
                  *(iter + 1) == 'u')) {
                break;
            }
            *out += tolower((unsigned char)(*iter));
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
    *out += toupper((unsigned char)(*iter));

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
                char c1_0 = tolower((unsigned char)(*iter1));
                char c2_0 = tolower((unsigned char)(*iter2));
                if (c1_0 == c2_0) {
                    /// identity: easy out
                    ++iter1;
                    ++iter2;
                    continue;
                }

                /// we scan for a match, starting from the corner formed
                /// as we march forward a few letters.  We use a maximum
                /// of 3 letters as our limit
                int max_radius = (int)min(pstr1->end() - iter1,
                                          string::difference_type(3));

                string::const_iterator best_iter1 = iter1 + 1;
                string::const_iterator best_iter2 = iter2 + 1;
                size_t cost = 1;

                for (int radius = 1;  radius <= max_radius;  ++radius) {

                    char corner1 = *(iter1 + radius);
                    char corner2 = *(iter2 + radius);
                    bool match = false;
                    for (int i = radius;  i >= 0;  --i) {
                        c1_0 = tolower((unsigned char)(*(iter1 + i)));
                        c2_0 = tolower((unsigned char)(*(iter2 + i)));
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
            if (long_str->size() < short_str->size()) {
                swap(short_str, long_str);
            }
            size_t short_size = short_str->size();
            size_t long_size = long_str->size();

            size_t* row0_ptr = buf0;
            size_t* row1_ptr = buf1;
            if (short_size > kMaxMetaphoneStack) {
                row0.resize(short_size + 1);
                row1.resize(short_size + 1);
                row0_ptr = &row0[0];
                row1_ptr = &row1[0];
            }

            size_t i;
            size_t j;

            //cout << "   ";
            for (i = 0;  i <= short_size;  ++i) {
                //cout << (*short_str)[i] << "  ";
                row0_ptr[i] = i;
                row1_ptr[i] = i;
            }
            //cout << endl;

            for (i = 0;  i < long_size;  ++i) {
                row1_ptr[0] = i + 1;
                //cout << (*long_str)[i] << " ";
                for (j = 0;  j < short_size;  ++j) {
                    int c0 = tolower((unsigned char) (*short_str)[j]);
                    int c1 = tolower((unsigned char) (*long_str)[i]);
                    size_t cost = (c0 == c1 ? 0 : 1);
                    row1_ptr[j + 1] =
                        min(row0_ptr[j] + cost,
                            min(row0_ptr[j + 1] + 1, row1_ptr[j] + 1));
                    //cout << setw(2) << row1_ptr[j + 1] << " ";
                }

                //cout << endl;

                swap(row0_ptr, row1_ptr);
            }

            return row0_ptr[short_size];
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
    //score += (tolower((unsigned char) word1[0]) == tolower((unsigned char) word2[0]));

    // one point for identical lengths of words
    //score += (word1.length() == word2.length());

    return (int)score;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Porter's Stemming Algorithm
///

enum ECharType {
    eOther,
    eConsonant,
    eVowel
};

class CFillTypes
{
public:
    CFillTypes()
    {
        // This cycle is processed in backward order to avoid buggy
        // optimization by ICC 9.1 on 64-bit platforms.
        // The optimizer calls buggy intel_fast_mem(cpy|set) even with
        // -fno-builtin-memset -fno-builtin-memcpy.
        for (int i = 256;  i--; ) {
            s_char_type[i] = eOther;
        }

        for (int i = 0;  i < 26;  ++i) {
            s_char_type[i + 'a'] = eConsonant;
            s_char_type[i + 'A'] = eConsonant;
        }

        s_char_type[(int)'a'] = eVowel;
        s_char_type[(int)'e'] = eVowel;
        s_char_type[(int)'i'] = eVowel;
        s_char_type[(int)'o'] = eVowel;
        s_char_type[(int)'u'] = eVowel;
    }

    ECharType GetChar(int c) {
        return s_char_type[c];
    }

private:
    ECharType s_char_type[256];
};


static inline ECharType s_GetCharType(int c)
{
    static CSafeStatic<CFillTypes> fill_types;
    _ASSERT(c < 256  &&  c >= 0);
    return fill_types->GetChar(c);
}

static inline int s_MeasureWord(string::const_iterator iter,
                                string::const_iterator end)
{
    int m = 0;

    /**
    {{
         ECharType first_char_type = s_GetCharType(*iter);
         while (iter != end  &&  s_GetCharType(*iter) == first_char_type) {
             ++iter;
         }
     }}
     **/


    // skip leading entities
    ECharType prev_type = s_GetCharType(*iter);
    for ( ;  iter != end;  ++iter) {
        ECharType type = s_GetCharType(*iter);
        if (type != prev_type) {
            prev_type = type;
            break;
        }
    }

    //prev_type = s_GetCharType(*iter);
    for ( ;  iter != end;  ++iter) {
        ECharType type = s_GetCharType(*iter);
        if (type != prev_type) {
            prev_type = type;
            ++m;
        }
    }
    /**
    for ( ;  iter != end;  ++m) {
        string::const_iterator prev(iter);
        ECharType prev_type = s_GetCharType(*prev);
        for (++iter;  iter != end;  ) {
            ECharType type = s_GetCharType(*iter);
            if (type != prev_type) {
                break;
            }
            prev_type = type;
            prev = iter++;
        }
        if (iter != end) {
            prev = iter;
            prev_type = s_GetCharType(*prev);
            for (++iter;  iter != end;  ) {
                ECharType type = s_GetCharType(*iter);
                if (type != prev_type) {
                    break;
                }
                prev_type = type;
                prev = iter++;
            }
        }
    }
    **/

    return m;
}


static inline bool s_EndsWith(const string& str1, const string& str2)
{
    string::const_reverse_iterator iter1(str1.end());
    string::const_reverse_iterator end1 (str1.begin());
    string::const_reverse_iterator iter2(str2.end());
    string::const_reverse_iterator end2 (str2.begin());
    for ( ;  iter1 != end1  &&  iter2 != end2;  ++iter1, ++iter2) {
        if (*iter1 != *iter2) {
            return false;
        }
    }
    return true;
}

static inline bool s_EndsWith(const string& str1, const char* p)
{
    string::const_reverse_iterator iter1(str1.end());
    string::const_reverse_iterator end1 (str1.begin());
    const char* iter2 = p + strlen(p) - 1;
    const char* end2  = p - 1;
    for ( ;  iter1 != end1  &&  iter2 != end2;  ++iter1, --iter2) {
        if (*iter1 != *iter2) {
            return false;
        }
    }
    return true;
}

static string::size_type s_FindFirstVowel(const string& str)
{
    for (string::size_type i = 0;  i < str.size();  ++i) {
        if (s_GetCharType(str[i]) == eVowel) {
            return i;
        }
    }
    return string::npos;
}

static inline bool s_ReplaceEnding(string& word,
                                   const string& match,
                                   const string& substitute,
                                   int min_measure = 0)
{
    if (word.length() < match.length()) {
        return false;
    }

    if ( !s_EndsWith(word, match) ) {
        return false;
    }

    if (s_MeasureWord(word.begin(),
                      word.end() - match.length()) <= min_measure) {
        return false;
    }

    word.erase(word.length() - match.length());
    word += substitute;
    return true;
}


static inline bool s_ReplaceEnding(string& word,
                                   const char* match,
                                   const char* substitute,
                                   int min_measure = 0)
{
    size_t match_len = strlen(match);
    if (word.length() < match_len) {
        return false;
    }

    if ( !s_EndsWith(word, match) ) {
        return false;
    }

    if (s_MeasureWord(word.begin(),
                      word.end() - match_len) <= min_measure) {
        return false;
    }

    word.erase(word.length() - match_len);
    word += substitute;
    return true;
}


static inline bool s_TruncateEnding(string& word,
                                    const char* match,
                                    size_t new_ending_size,
                                    int min_measure = 0)
{
    size_t match_len = strlen(match);
    if (word.length() < match_len) {
        return false;
    }

    if ( !s_EndsWith(word, match) ) {
        return false;
    }

    if (s_MeasureWord(word.begin(),
                      word.end() - match_len) <= min_measure) {
        return false;
    }

    word.erase(word.length() - match_len + new_ending_size);
    return true;
}


void CDictionaryUtil::Stem(const string& in_str, string* out_str)
{
    *out_str = in_str;
    string& str = *out_str;

    // the steps outlined below follow the general scheme at:
    //
    //  http://snowball.tartarus.org/algorithms/porter/stemmer.html
    //

    // step 1a: common 's' endings
    //
    // sses -> ss
    // ies  -> i
    // ss   -> ss
    // s    ->
    if (str[ str.length()-1 ] == 's') {
        do {
            //if (s_ReplaceEnding(str, "sses", "ss")) {
            if (s_TruncateEnding(str, "sses", 2)) {
                break;
            }

            //if (s_ReplaceEnding(str, "ies", "i")) {
            if (s_TruncateEnding(str, "ies", 1)) {
                break;
            }

            if ( !s_EndsWith(str, "ss") ) {
                //s_ReplaceEnding(str, "s", "");
                s_TruncateEnding(str, "s", 0);
            }
        }
        while (false);
    }

    // step 1b: ed/ing
    //
    // eed -> ed (not .eed)
    // ed  ->    (*v*)
    // ing ->    (*v*)
    if (s_EndsWith(str, "eed")  &&  str.length() > 4) {
        str.erase(str.length() - 1);
    } else {
        bool extra = false;
        if (s_EndsWith(str, "ed")  &&  
            s_FindFirstVowel(str) < str.length() - 3) {
            str.erase(str.length() - 2);
            extra = true;
        } else if (s_EndsWith(str, "ing")  &&
                   s_FindFirstVowel(str) < str.length() - 3) {
            str.erase(str.length() - 3);
            extra = true;
        }

        if (extra) {
            if (s_EndsWith(str, "at")  ||
                s_EndsWith(str, "bl")  ||
                s_EndsWith(str, "iz")) {
                str += 'e';
            } else if (str[str.length() - 1] != 'l'  &&
                       str[str.length() - 1] != 's'  &&
                       str[str.length() - 1] != 'z'  &&
                       str[str.length() - 1]  == str[str.length() - 2]) {
                str.erase(str.length() - 1);
            } else if (str.length() == 3  &&
                       s_GetCharType(str[0]) == eConsonant  &&
                       s_GetCharType(str[1]) == eVowel  &&
                       s_GetCharType(str[2]) == eConsonant) {
                str += 'e';
            }
        }
    }

    // step 1c: y -> i
    if (str[str.length() - 1] == 'y'  &&
        s_FindFirstVowel(str) < str.length() - 1) {
        str[str.length() - 1] = 'i';
    }

    // step 2

    if (str.length() > 3) {
        switch (str[ str.length() - 2 ]) {
        case 'a':
            if ( !s_ReplaceEnding(str, "ational", "ate") ) {
                s_ReplaceEnding(str, "tional", "tion");
                //s_TruncateEnding(str, "tional", 4);
            }
            break;

        case 'c':
            if ( !s_ReplaceEnding(str, "enci", "ence") ) {
                s_ReplaceEnding(str, "anci", "ance");
            }
            break;

        case 'e':
            s_ReplaceEnding(str, "izer", "ize");
            //s_TruncateEnding(str, "izer", 3);
            break;

        case 'l':
            if (str[ str.length()-1 ] == 'i'  &&
                !s_ReplaceEnding(str, "abli", "able")  &&
                !s_ReplaceEnding(str, "alli", "al")  &&
                !s_ReplaceEnding(str, "entli", "ent")  &&
                !s_ReplaceEnding(str, "eli", "e") ) {
                s_ReplaceEnding(str, "ousli", "ous");
            }
            /**
            if (str[ str.length()-1 ] == 'i'  &&
                !s_ReplaceEnding(str, "abli", "able")  &&
                !s_TruncateEnding(str, "alli", 2)  &&
                !s_TruncateEnding(str, "entli", 3)  &&
                !s_TruncateEnding(str, "eli", 1) ) {
                s_TruncateEnding(str, "ousli", 3);
            }
            **/
            break;

        case 'o':
            if ( !s_ReplaceEnding(str, "ization", "ize")  &&
                 !s_ReplaceEnding(str, "ation", "ate") ) {
                s_ReplaceEnding(str, "ator", "ate");
            }
            break;

        case 's':
            if ( !s_ReplaceEnding(str, "alism", "al")  &&
                 !s_ReplaceEnding(str, "iveness", "ive")  &&
                 !s_ReplaceEnding(str, "fulness", "ful") ) {
                s_ReplaceEnding(str, "ousness", "ous");
            }
            /**
            if ( !s_TruncateEnding(str, "alism", 2)  &&
                 !s_TruncateEnding(str, "iveness", 3)  &&
                 !s_TruncateEnding(str, "fulness", 3) ) {
                s_TruncateEnding(str, "ousness", 3);
            }
            **/
            break;

        case 't':
            if ( !s_ReplaceEnding(str, "aliti", "al")  &&
                 !s_ReplaceEnding(str, "iviti", "ive") ) {
                s_ReplaceEnding(str, "biliti", "ble");
            }
            break;

        default:
            break;
        }
    }

    // 'us' endings
    //s_ReplaceEnding(str, "u", "us");

    // step 3
	typedef SStaticPair<const char*, const char*> TReplace;
	static const TReplace rep_step3[] = {
        { "icate",  "ic" },
        { "ative",  ""   },
        { "alize", "al"  },
        { "iciti", "ic"  },
        { "ical",  "ic"  },
        { "ful",   ""    },
        { "ness",  ""    },
        { NULL, NULL }  /// end
    };
    {{
         static const char* s_Step3_Endings("eils");
         if (CTempString(s_Step3_Endings).find(str[str.length()-1]) != string::npos) {
             for (const TReplace* p = rep_step3;  p->first;  ++p) {
                 if (s_ReplaceEnding(str, p->first, p->second)) {
                     break;
                 }
             }
         }
     }}

    // step 4
    if (str.length() > 2) {
        switch (str[ str.length() - 2]) {
        case 'a':
            if (str[ str.length()-1 ] == 'l') {
                if (s_ReplaceEnding(str, "ual", "", 1)) {
                    break;
                }
                if (s_ReplaceEnding(str, "ial", "", 1)) {
                    break;
                }
                s_ReplaceEnding(str, "al", "", 1);
            }
            break;

        case 'c':
            if (str[ str.length()-1 ] == 'e') {
                if ( !s_ReplaceEnding(str, "ance", "", 1) ) {
                    s_ReplaceEnding(str, "ence", "", 1);
                }
            }
            break;

        case 'e':
            s_ReplaceEnding(str, "er", "", 1);
            break;

        case 'i':
            if (s_ReplaceEnding(str, "ix", "ic", 0)) {
                break;
            }
            s_ReplaceEnding(str, "ic", "", 1);
            break;

        case 'l':
            if ( !s_ReplaceEnding(str, "able", "", 1) ) {
                s_ReplaceEnding(str, "ible", "", 1);
            }
            break;

        case 'n':
            if ( !s_ReplaceEnding(str, "ant", "", 1) ) {
                if ( !s_ReplaceEnding(str, "ement", "", 1) ) {
                    if ( !s_ReplaceEnding(str, "ment", "", 1) ) {
                        s_ReplaceEnding(str, "ent", "", 1);
                    }
                }
            }
            break;

        case 'o':
            if ( !s_ReplaceEnding(str, "sion", "s", 1) ) {
                if ( !s_ReplaceEnding(str, "tion", "t", 1) ) {
                    s_ReplaceEnding(str, "ou", "", 1);
                }
            }
            break;

        case 's':
            s_ReplaceEnding(str, "ism", "", 1);
            break;

        case 't':
            if ( !s_ReplaceEnding(str, "ate", "", 1) ) {
                s_ReplaceEnding(str, "iti", "", 1);
            }
            break;

        case 'u':
            s_ReplaceEnding(str, "ous", "", 1);
            break;

        case 'v':
            s_ReplaceEnding(str, "ive", "", 1);
            break;

        case 'z':
            s_ReplaceEnding(str, "ize", "", 1);
            break;
        }
    }

    // step 5a
    //s_ReplaceEnding(str, "e", "", 1);
    s_TruncateEnding(str, "e", 0, 1);

    // step 5b
    if (s_MeasureWord(str.begin(), str.end()) > 1  &&
        str[str.length() - 1] == 'l'  &&
        str[str.length() - 2] == 'l') {
        str.erase(str.length() - 1);
    }

}


END_NCBI_SCOPE
