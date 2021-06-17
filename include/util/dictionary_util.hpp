#ifndef UTIL___DICTIONARY_UTIL__HPP
#define UTIL___DICTIONARY_UTIL__HPP

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

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE


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

    /// Compute the Porter stem for a given word.
    /// Porter's stemming algorithm is one of many automated stemming
    /// algorithms; unlike most, Porter's stemming algorithm is a widely
    /// accepted standard algorithm for generating word stems.
    ///
    /// A description of the algorithm is available at
    ///
    ///   http://www.tartarus.org/~martin/PorterStemmer/def.txt
    ///
    /// The essence of the algorithm is to repeatedly strip likely word
    /// suffixes such as -ed, -es, -s, -ess, -ness, -ability, -ly, and so
    /// forth, leaving a residue of a word that can be compared with other stem
    /// sources.  The goal is to permit comparison of socuh words as:
    ///
    ///    compare
    ///    comparable
    ///    comparability
    ///    comparably
    ///
    /// since they all contain approximately the same meaning.
    ///
    /// This algorithm assumes that word case has already been adjusted to
    /// lower case.
    static void Stem(const string& in_str, string* out_str);

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

    /// Compute a nearness score for two different words or phrases
    static int Score(const string& word1, const string& word2,
                     size_t max_metaphone = eMaxMetaphone);

    /// Compute a nearness score based on metaphone as well as raw distance
    static int Score(const string& word1, const string& meta1,
                     const string& word2, const string& meta2,
                     EDistanceMethod method = eEditDistance_Similar);

};




END_NCBI_SCOPE

#endif  // UTIL___DICTIONARY_UTIL__HPP
