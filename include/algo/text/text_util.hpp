#ifndef ALGO_TEXT___QUERY_UTIL__HPP
#define ALGO_TEXT___QUERY_UTIL__HPP

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

#include <corelib/ncbiobj.hpp>
#include <corelib/hash_map.hpp>
#include <corelib/hash_set.hpp>
#include <util/simple_buffer.hpp>

#include <algo/text/vector.hpp>


BEGIN_NCBI_SCOPE



///
/// string comparison using integers instead of const char*
/// this is coded in C++ and beats an assembler-based memcmp()
/// this algorithm makes no adjustment for memory alignment and may be either
/// slower or illegal on some platforms.
///
template <typename TComp>
struct SStringLess : public binary_function<string, string, bool>
{
    bool operator() (const string& s1, const string& s2) const
    {
        const TComp* p1     = (const TComp*)s1.data();
        const TComp* p1_end = p1 + s1.size() / sizeof(TComp);
        const TComp* p2     = (const TComp*)s2.data();
        const TComp* p2_end = p2 + s2.size() / sizeof(TComp);
        for ( ;  p1 != p1_end  &&  p2 != p2_end;  ++p1, ++p2) {
            if (*p1 < *p2) {
                return true;
            }
            if (*p2 < *p1) {
                return false;
            }
        }

        const char* pc1 = (const char*)p1;
        const char* pc1_end = s1.data() + s1.size();
        const char* pc2 = (const char*)p2;
        const char* pc2_end = s2.data() + s2.size();
        for ( ; pc1 != pc1_end  &&  pc2 != pc2_end;  ++pc1, ++pc2) {
            if (*pc1 < *pc2) {
                return true;
            }
            if (*pc2 < *pc1) {
                return false;
            }
        }

        return (pc2 != pc2_end);
    }
};


///
/// This appears to be the best overall hash function for use in English
/// dictionaries.  It amounts to an iterative string walk in which the hash is
/// computed as
///
///  hval[n] = (hval[n-1] * 17) + c
/// 
/// for each c in the string
///
template<class IT>
size_t StringHash17(IT start, IT end)
{
    size_t hval = 0;
    for (; start != end; ++start) {
        hval = ((hval << 4) + hval) + *start;
    }
    return hval;
}

/// Functor-adaptor for StringHash17
struct SStringHash
{
    size_t operator()(const string& s) const
    {
        return StringHash17(s.begin(), s.end());
    }
};



class CTextUtil
{
public:

    enum EOptions {
        fDiphthongReplace    = 0x01,
        fPorterStem          = 0x02,
        fTrimStops           = 0x04,
        fNoNumeric           = 0x08,
        fIncludePhrases      = 0x10,
        fPhrase_NoStems      = 0x20,
        fPhrase_NoPrefix     = 0x40,

        fDefaults = fPorterStem | fDiphthongReplace |
                    fTrimStops | fIncludePhrases
    };
    typedef int TFlags;

    /// typedef for word frequencies
    typedef CScoreVector<string, float> TWordFreq;

    /// retrieve word frequencies for a given piece of text
    static void GetWordFrequencies(const string& text, TWordFreq& freq,
                                   TFlags flags = fDefaults);

    /// retrieve word frequencies from a file
    static void GetWordFrequencies(CNcbiIstream& istr, TWordFreq& freq,
                                   TFlags flags = fDefaults);

    /// convert an integer into a set of word frequencies
    /// this maps ints to smaller strings to compress the dictionary
    static void GetWordFrequencies(Int4 i,    TWordFreq& freq);
    static void GetWordFrequencies(Uint4 i,   TWordFreq& freq);
    static void GetWordFrequencies(Int8 i, TWordFreq& freq);
    static void GetWordFrequencies(Uint8 i, TWordFreq& freq);
    static void GetWordFrequencies(float i, TWordFreq& freq);
    static void GetWordFrequencies(double i, TWordFreq& freq);

    /// split a set of word frequencies into phrase and non-phrase frequencies
    /// this is done to treat the two separately
    static void SplitWordFrequencies(const TWordFreq& wf_in,
                                     TWordFreq& wf_out, TWordFreq& phrase_out);

    /// retrieve stem frequencies from a set of word frequencies
    static void GetStemFrequencies(const TWordFreq& freq,
                                   TWordFreq& stems,
                                   TFlags flags = fDefaults);

    /// add a set of frequencies into another set
    static void AddWordFrequencies(TWordFreq& freq,
                                   const TWordFreq& wf,
                                   TFlags flags = 0);
    static void AddWordFrequencies(TWordFreq& freq,
                                   const TWordFreq& wf,
                                   const string& prefix,
                                   TFlags flags = 0);

    /// return true if the provided word is a stop word
    static bool IsStopWord(const string& str);

    /// eliminate the stop words frm a set of word frequencies
    static void TrimStopWords(TWordFreq& freq);

    static void EncodeFreqs(const TWordFreq& freq,
                            vector<char>& data);
    static void EncodeFreqs(const TWordFreq& freq,
                            vector<unsigned char>& data);
    static void EncodeFreqs(const TWordFreq& freq,
                            CSimpleBuffer& data);
    static void DecodeFreqs(TWordFreq& freq,
                            const vector<char>& data);
    static void DecodeFreqs(TWordFreq& freq,
                            const vector<unsigned char>& data);
    static void DecodeFreqs(TWordFreq& freq,
                            const CSimpleBuffer& data);
    static void DecodeFreqs(TWordFreq& freq,
                            const void* data, size_t data_len);

    /// perform a set of punctuational clean-ups on a string suitable for a
    /// journal or book title
    static void CleanJournalTitle(string& title);
};



END_NCBI_SCOPE

#endif  // ALGO_TEXT___QUERY_UTIL__HPP
