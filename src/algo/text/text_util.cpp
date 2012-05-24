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
#include <algo/text/text_util.hpp>
#include <algo/text/vector.hpp>
#include <algo/text/vector_serial.hpp>
#include <util/dictionary_util.hpp>
#include <util/static_set.hpp>
#include <math.h>


BEGIN_NCBI_SCOPE


/**
static bool sc_IsTokenStop[256];
static char sc_Lower[256];
static const string sc_ClauseStop(".?!;:\"{}[]()");
**/


enum {
    eAlpha        = 0x0100,
    eNumeric      = 0x0200,
    ePunctuation  = 0x0400,
    ePrintable    = 0x0800,
    eSpace        = 0x1000,
    eClauseEnd    = 0x2000
};
static Uint2 sc_Tokens[256];

struct SLoadTokens
{
    SLoadTokens()
    {
        /**
        int isalnum(int c);
        int isalpha(int c);
        int isascii(int c);
        int isblank(int c);
        int iscntrl(int c);
        int isdigit(int c);
        int isgraph(int c);
        int islower(int c);
        int isprint(int c);
        int ispunct(int c);
        int isspace(int c);
        int isupper(int c);
        int isxdigit(int c);
        **/



        /// seed our tokens with their lower-case equivalents
        /// by default all tokens are considered clause stops
        for (int i = 0;  i < 256;  ++i) {
            sc_Tokens[i] = tolower(i);

            /// add additional properties
            if (isprint(i)) {
                sc_Tokens[i] |= ePrintable;
            }
            if (isalpha(i)) {
                sc_Tokens[i] |= eAlpha;
            }
            if (isdigit(i)) {
                sc_Tokens[i] |= eNumeric;
            }
            if (ispunct(i)) {
                sc_Tokens[i] |= ePunctuation;
            }
            if (isspace(i)) {
                sc_Tokens[i] |= eSpace;
            }
        }

        /// other special pieces: clause ends
        string clause_ends(".?!;:\"{}[]()");
        ITERATE (string, it, clause_ends) {
            sc_Tokens[(unsigned char)*it] |= eClauseEnd;
        }

        /// funkyness: "'" counts as an alphanumeric
        sc_Tokens[(int)'\''] |= eAlpha;
    }
};
static SLoadTokens s_ForceTokenLoad;


//////////////////////////////////////////////////////////////////////////////


static bool s_IsAlphaNumeric(unsigned char c)
{
    return (sc_Tokens[c] & (eAlpha|eNumeric));
}

/*static bool s_IsAlpha(unsigned char c)
{
    return (sc_Tokens[c] & (eAlpha));
}*/

static bool s_IsNumeric(unsigned char c)
{
    return (sc_Tokens[c] & (eNumeric));
}

static bool s_IsNumeric(const string& s)
{
    bool is_alpha = false;
    ITERATE (string, i, s) {
        is_alpha |= !s_IsNumeric(*i);
        if (is_alpha) {
            break;
        }
    }
    return !is_alpha;
}

/*static bool s_IsPrintable(unsigned char c)
{
    return (sc_Tokens[c] & (ePrintable));
}*/

static char s_ToLower(unsigned char c)
{
    return sc_Tokens[c] & 0xff;
}

static string::size_type s_NextTokenStart(const string& s,
                                          string::size_type i)
{
    for ( ;  i < s.size();  ++i) {
        if (s_IsAlphaNumeric(s[i])) {
            break;
        }
    }
    return (i == s.size() ? string::npos : i);
}

static string::size_type s_NextTokenStop(const string& s,
                                         string::size_type i)
{
    for ( ;  i < s.size();  ++i) {
        if ( !s_IsAlphaNumeric(s[i]) ) {
            break;
        }
    }
    return (i == s.size() ? string::npos : i);
}

static string::size_type s_NextClauseStop(const string& s,
                                          string::size_type i)
{
    for ( ;  i < s.size();  ++i) {
        if (sc_Tokens[(unsigned char)s[i]] & eClauseEnd) {
            break;
        }
    }
    return (i == s.size() ? string::npos : i);
}

//////////////////////////////////////////////////////////////////////////////


/// split a set of word frequencies into phrase and non-phrase frequencies
/// this is done to treat the two separately
void CTextUtil::SplitWordFrequencies(const TWordFreq& wf_in,
                                      TWordFreq& wf_out, TWordFreq& phrase_out)
{
    ITERATE (TWordFreq, iter, wf_in) {
        if (iter->first[0] == 'p'  &&  iter->first.find("phrase: ") == 0) {
            phrase_out.insert(phrase_out.end(), *iter);
        } else {
            wf_out.insert(wf_out.end(), *iter);
        }
    }
}


/// add a set of frequencies into another set
void CTextUtil::AddWordFrequencies(TWordFreq& freq, const TWordFreq& wf,
                                    TFlags flags)
{
    ITERATE (TWordFreq, iter, wf) {
        if ((flags & fNoNumeric)  &&  s_IsNumeric(iter->first)) {
            continue;
        }
        freq.Add(iter->first, iter->second);
    }
}


void CTextUtil::AddWordFrequencies(TWordFreq& freq, const TWordFreq& wf,
                                    const string& prefix,
                                    TFlags flags)
{
    ITERATE (TWordFreq, iter, wf) {
        if (iter->first.find_first_of(":") != string::npos) {
            continue;
        }
        if ((flags & fNoNumeric)  &&  s_IsNumeric(iter->first)) {
            continue;
        }
        freq.Add(prefix + ": " + iter->first, iter->second);
    }
}


string s_ValToString(Int4 i)
{
    return NStr::IntToString(i);
}

string s_ValToString(Int8 i)
{
    return NStr::Int8ToString(i);
}

string s_ValToString(Uint4 i)
{
    return NStr::UIntToString(i);
}

string s_ValToString(Uint8 i)
{
    return NStr::UInt8ToString(i);
}

string s_ValToString(double i)
{
    return NStr::DoubleToString(i);
}

string s_ValToString(float i)
{
    return NStr::DoubleToString(i);
}

string s_ValToString(const string& i)
{
    return i;
}


template <class T>
void s_NumericToFreq(const T& val, CTextUtil::TWordFreq& freq)
{
    freq.Add(s_ValToString(val), 1);

    /**
    const size_t kNumericStringLimit = 40;
    string str = s_ValToString(val);
    if (str.size() <= kNumericStringLimit) {
        freq.Add(str, 1);
    } else {
        string::const_iterator it1 = str.begin();
        string::const_iterator it2 = str.begin() + kNumericStringLimit;
        for ( ;  ;  ++it1, ++it2) {
            string s(it1, it2);
            freq.Add(s, 1);
            if (it2 == str.end()) {
                break;
            }
        }
    }
    **/
}


void CTextUtil::GetWordFrequencies(Int4 i, TWordFreq& freq)
{
    s_NumericToFreq(i, freq);
}


void CTextUtil::GetWordFrequencies(Int8 i, TWordFreq& freq)
{
    s_NumericToFreq(i, freq);
}


void CTextUtil::GetWordFrequencies(Uint4 i, TWordFreq& freq)
{
    s_NumericToFreq(i, freq);
}


void CTextUtil::GetWordFrequencies(Uint8 i, TWordFreq& freq)
{
    s_NumericToFreq(i, freq);
}


void CTextUtil::GetWordFrequencies(double i, TWordFreq& freq)
{
    s_NumericToFreq(i, freq);
}


void CTextUtil::GetWordFrequencies(float i, TWordFreq& freq)
{
    s_NumericToFreq(i, freq);
}


void CTextUtil::GetWordFrequencies(const string& text,
                                   TWordFreq& freq,
                                   TFlags flags)
{
    _TRACE("CTextUtil::GetWordFrequencies(): text = " << text);
    string::size_type clause_start = 0;
    string::size_type clause_end   = text.size();

    list<string> prev_words;
    string phrase;
    string word;
    string stem;

    /// we process one clause at a time
    while (clause_start != clause_end) {
        clause_end = text.size();
        string::size_type pos = s_NextClauseStop(text, clause_start);
        if (pos != string::npos) {
            clause_end = pos;
        }

        prev_words.clear();
        _TRACE("clause: |" << text.substr(clause_start, clause_end - clause_start) << "|");
        for ( ;  clause_start != clause_end;  clause_start = pos) {
            /// find the current starting point
            clause_start =
                min(clause_end, s_NextTokenStart(text, clause_start));
            if (clause_start == clause_end) {
                break;
            }

            /// determine the next word end boundary
            pos = min(clause_end,
                      s_NextTokenStop(text, clause_start));

            ///
            /// we've found a word
            ///

            /// extract it and lower-case it
            word.assign(text, clause_start, pos - clause_start);

            /// make sure it's not entirely numeric
            string::size_type pos1 =
                word.find_first_not_of("0123456789");
            if (pos1 == string::npos) {
                if ((flags & fNoNumeric) == 0) {
                    s_NumericToFreq(word, freq);
                }
                continue;
            }


            /// remove single-quotes
            string::iterator copy_to = word.begin();
            NON_CONST_ITERATE (string, copy_from, word) {
                if (*copy_from == '\'') {
                    continue;
                }
                *copy_to++ = s_ToLower(*copy_from);
            }
            if (copy_to != word.end()) {
                word.erase(copy_to, word.end());
            }

            if ((flags & fTrimStops)  &&  IsStopWord(word)) {
                continue;
            }

            if (word.empty()) {
                continue;
            }

            _TRACE("  word: " << word);

            /// we may want to fix common british diphthongs
            if (flags & fDiphthongReplace) {
                typedef pair<string, string> TDiphPair;
                static const TDiphPair sc_DiphPairs[] = {
                    TDiphPair("oe", "e"),
                    TDiphPair("ae", "e")
                };

                string s;
                for (size_t i = 0;  i < sizeof(sc_DiphPairs) / sizeof(TDiphPair);  ++i) {
                    if (word.find(sc_DiphPairs[i].first) != string::npos) {
                        NStr::Replace(word,
                                      sc_DiphPairs[i].first,
                                      sc_DiphPairs[i].second,
                                      s);
                        word.swap(s);
                    }
                }
            }

            ///
            /// add the word
            ///
            freq.Add(word, 1);

            if (flags & fIncludePhrases) {
                ///
                /// add a phrase including the previous word
                ///
                if (flags & fPhrase_NoStems) {
                    prev_words.push_back(word);
                } else {
                    CDictionaryUtil::Stem(word, &stem);
                    prev_words.push_back(stem);
                }

                /// we permit a phrase to be a proximity search, looking for
                /// wors within two words of the current
                while (prev_words.size() > 3) {
                    prev_words.pop_front();
                }
                if (prev_words.size() > 1) {
                    list<string>::iterator pit = prev_words.begin();
                    list<string>::iterator end = prev_words.end();
                    --end;
                    for ( ;  pit != end;  ++pit) {
                        /// this looks odd, but we want to make sure our
                        /// phrases are not things like 'cancer cancer'
                        if (*pit == *end) {
                            continue;
                        }

                        phrase.erase();
                        if ( !(flags & fPhrase_NoPrefix) ) {
                            phrase  = "phrase: ";
                        }
                        if (*pit < *end) {
                            phrase += *pit;
                            phrase += " ";
                            phrase += *end;
                        } else {
                            phrase += *end;
                            phrase += " ";
                            phrase += *pit;
                        }
                        _TRACE("    phrase: |" << phrase << "|");

                        freq.Add(phrase, 1);
                        /**
                        TWordFreq::iterator it = freq.find(phrase);
                        if (it == freq.end()) {
                            freq[phrase] = 1;
                        } else {
                            ++it->second;
                        }
                        **/
                    }
                }
            }
        }

        _TRACE("clause end");

        clause_start = s_NextTokenStart(text, clause_start);
        if (clause_start == string::npos) {
            break;
        }
    }

#ifdef _DEBUG
    ITERATE (TWordFreq, it, freq) {
        _TRACE("  word: " << it->first << "  count: " << it->second);
    }
#endif
}


void CTextUtil::GetStemFrequencies(const TWordFreq& freq,
                                    TWordFreq& stem_freq,
                                    TFlags flags)
{
    string stem;
    ITERATE (TWordFreq, iter, freq) {
        if (iter->first.find_first_of(":") != string::npos) {
            stem = iter->first;
        } else {
            CDictionaryUtil::Stem(iter->first, &stem);
        }

        TWordFreq::iterator it = stem_freq.find(stem);
        if (it != stem_freq.end()) {
            it->second += iter->second;
        } else {
            stem_freq.insert
                (stem_freq.end(),
                 CTextUtil::TWordFreq::value_type(stem, iter->second));
        }
    }

    if (flags & fTrimStops) {
        TrimStopWords(stem_freq);
    }
}


void CTextUtil::GetWordFrequencies(CNcbiIstream& istr,
                                    TWordFreq& freq,
                                    TFlags flags)
{
    string line;
    while (NcbiGetlineEOL(istr, line)) {
        GetWordFrequencies(line, freq, flags);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// Stop Word Pruning
///
///

static const char* const sc_StopWordArray[] = {
    "a",
    "about",
    "again",
    "am",
    "an",
    "and",
    "any",
    "anybody",
    "anyhow",
    "anyone",
    "anything",
    "anyway",
    "are",
    "as",
    "at",
    "be",
    "because",
    "been",
    "being",
    "but",
    "by",
    "did",
    "do",
    "does",
    "doing",
    "done",
    "for",
    "from",
    "had",
    "has",
    "have",
    "having",
    "he",
    "her",
    "him",
    "his",
    "how",
    "i",
    "if",
    "in",
    "into",
    "is",
    "isnt",
    "it",
    "make",
    "me",
    "my",
    "myself",
    "no",
    "none",
    "not",
    "now",
    "of",
    "off",
    "on",
    "or",
    "our",
    "than",
    "that",
    "the",     
    "their",
    "them",
    "then",
    "there",
    "these",
    "they",
    "this",
    "those",
    "to",
    "too",
    "two",
    "very",
    "was",
    "we",
    "went",
    "were",
    "what",
    "whatever",
    "whats",
    "when",
    "where",
    "who",
    "whom",
    "whose",
    "why",
    "will",
    "with",
    "wont",
    "would",
    "wouldnt",
    "yet",
    "you",
    "your",
};
typedef CStaticArraySet<const char*, PCase_CStr> TStopWords;
DEFINE_STATIC_ARRAY_MAP(TStopWords, sc_StopWords, sc_StopWordArray);


bool CTextUtil::IsStopWord(const string& str)
{
    TStopWords::const_iterator iter = sc_StopWords.find(str.c_str());
    return (iter != sc_StopWords.end());
}


void CTextUtil::TrimStopWords(TWordFreq& freq)
{
    /// eliminate stop words
    TStopWords::const_iterator stop_it  = sc_StopWords.begin();
    TStopWords::const_iterator stop_end = sc_StopWords.end();

    CTextUtil::TWordFreq::iterator it  = freq.begin();
    CTextUtil::TWordFreq::iterator end = freq.end();

    for ( ;  stop_it != stop_end  &&  it != end;  ) {
        if (it->first == *stop_it) {
            freq.erase(it++);
            ++stop_it;
        } else {
            if (it->first < *stop_it) {
                ++it;
            } else {
                ++stop_it;
            }
        }
    }
}


void CTextUtil::CleanJournalTitle(string& title)
{
    string::size_type pos = 0;
    while ( (pos = title.find_first_of(".,[](){};:'\"/?<>", pos)) != string::npos) {
        title.erase(pos, 1);
    }
    title = NStr::ToLower(title);
}


/// encode word frequencies in a serializable blob of data
void CTextUtil::EncodeFreqs(const CTextUtil::TWordFreq& freq,
                            vector<unsigned char>& data)
{
    Encode(freq, data);
}

void CTextUtil::EncodeFreqs(const CTextUtil::TWordFreq& freq,
                            vector<char>& data)
{
    Encode(freq, data);
}


void CTextUtil::EncodeFreqs(const CTextUtil::TWordFreq& freq,
                            CSimpleBuffer& data)
{
    Encode(freq, data);
}


/// decode from a serializable blob of data
void CTextUtil::DecodeFreqs(CTextUtil::TWordFreq& freq,
                             const vector<unsigned char>& data)
{
    Decode(data, freq);
}


void CTextUtil::DecodeFreqs(CTextUtil::TWordFreq& freq,
                             const vector<char>& data)
{
    Decode(data, freq);
}


void CTextUtil::DecodeFreqs(CTextUtil::TWordFreq& freq,
                            const CSimpleBuffer& data)
{
    Decode(data, freq);
}


void CTextUtil::DecodeFreqs(CTextUtil::TWordFreq& freq,
                             const void* data, size_t data_len)
{
    Decode(data, data_len, freq);
}



END_NCBI_SCOPE
