#ifndef CORELIB___STR_UTIL__HPP
#define CORELIB___STR_UTIL__HPP

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
 * Authors:  Eugene Vasilchenko, Aaron Ucko, Denis Vakatov, Anatoliy Kuznetsov
 *
 * File Description:
 *      String algorithms
 *
 */

/// @file ncbistr_util.hpp
/// Algorithms for string processing

#include <corelib/ncbitype.h>

#include <string>

BEGIN_NCBI_SCOPE


/** @addtogroup String
 *
 * @{
 */


/// Do-nothing token position container
///
struct CStrDummyTokenPos
{
    void push_back(SIZE_TYPE /*pos*/) {}
    void reserve(SIZE_TYPE) {}
};

/// Base class for string splitting 
///
struct CStrTokenizeBase
{
    /// Whether to merge adjacent delimiters
    enum EMergeDelims {
        eNoMergeDelims,     ///< No merging of delimiters
        eMergeDelims        ///< Merge the delimiters
    };
};


/// Do nothing token counter 
///
template<class TStr>
struct CStrDummyTokenCount
{
    static
    size_t Count(const TStr&     /*str*/,
                 const TStr&     /*delim*/,
                 CStrTokenizeBase::EMergeDelims /*merge*/) 
    {
        return 0;
    }
};

/// Do nothing target reservation trait 
///
/// applies for list<> or cases when reservation only makes things slower
/// (may be the case if we use deque<> as a target container and tokenize 
/// large text)
///
template<class TStr, class TV, class TP, class TCount>
struct CStrDummyTargetReserve
{
    static
    void Reserve(const TStr&     /*str*/, 
                 const TStr&     /*delim*/,
                 TV&             /*target*/,
                 CStrTokenizeBase::EMergeDelims /*merge*/,
                 TP&             /*token_pos*/)
    {}
};


/// Main tokenization algorithm
///
/// TStr    - string type (must follow std::string interface)
/// TV      - container type for tokens (std::vector)
/// TP      - target container type for token positions (vector<size_t>)
/// TCount  - token count trait for the target container space reservation
/// TReserve- target space reservation trait
///
template<class TStr, 
         class TV, 
         class TP = CStrDummyTokenPos, 
         class TCount = CStrDummyTokenCount<TStr>,
         class TReserve = CStrDummyTargetReserve<TStr, TV, TP, TCount> > 
class CStrTokenize : public CStrTokenizeBase
{
public:
    typedef TStr     TString;
    typedef TV       TContainer;
    typedef TP       TPosContainer;
    typedef TCount   TCountTrait;
    typedef TReserve TReserveTrait;
public:

    /// Tokenize a string using the specified set of char delimiters.
    ///
    /// @param str
    ///   String to be tokenized.
    /// @param delim
    ///   Set of char delimiters used to tokenize string "str".
    ///   If delimiter is empty, then input string is appended to "arr" as is.
    /// @param target
    ///   Output container. 
    ///   Tokens defined in "str" by using symbols from "delim" are added
    ///   to the list "target".
    /// @param merge
    ///   Whether to merge the delimiters or not. 
    ///   eNoMergeDelims means that delimiters that immediately follow each
    ///    other are treated as separate delimiters - empty string(s) appear 
    ///    in the target output.
    /// @param token_pos
    ///   Container for the tokens' positions in "str".
    /// @param empty_str
    ///   String added to target when there are no other characters between
    ///   delimiters
    ///
    static
    void Do(const TString&     str,
            const TString&     delim,
            TContainer&        target,
            EMergeDelims       merge,
            TPosContainer&     token_pos,
            const TString&     empty_str = TString())
    {
        // Special cases
        if (str.empty()) {
            return;
        } else if (delim.empty()) {
            target.push_back(str);
            token_pos.push_back(0);
            return;
        }

        // Do target space reservation (if applicable)
        TReserveTrait::Reserve(str, delim, target, merge, token_pos);

        // Tokenization
        //
        SIZE_TYPE pos, prev_pos;
        for (pos = 0;;) {
            prev_pos = (merge == eMergeDelims ? 
                            str.find_first_not_of(delim, pos) : pos);
            if (prev_pos == TString::npos) {
                break;
            }
            pos = str.find_first_of(delim, prev_pos);
            if (pos == TString::npos) {
                // Avoid using temporary objects
                // ~ arr.push_back(str.substr(prev_pos));
                target.push_back(empty_str);
                target.back().assign(str, prev_pos, str.length() - prev_pos);
                token_pos.push_back(prev_pos);
                break;
            } else {
                // Avoid using temporary objects
                // ~ arr.push_back(str.substr(prev_pos, pos - prev_pos));
                target.push_back(empty_str);
                target.back().assign(str, prev_pos, pos - prev_pos);
                token_pos.push_back(prev_pos);
                ++pos;
            }
        } // for        
    }
};


/// token count trait for std::string
///
struct CStringTokenCount
{
    static
    size_t Count(const string&                  str,
                 const string&                  delim,
                 CStrTokenizeBase::EMergeDelims merge) 
    {
        SIZE_TYPE pos, prev_pos;
        size_t tokens = 0;

        // Count number of tokens         
        for (pos = 0;;) {
            prev_pos = (merge == CStrTokenizeBase::eMergeDelims ? 
                            str.find_first_not_of(delim, pos) : pos);
            if (prev_pos == NPOS) {
                break;
            } 
            pos = str.find_first_of(delim, prev_pos);
            ++tokens;
            if (pos == NPOS) {
                break;
            }
            ++pos;
        }
        return tokens;
    }
};


/// Target reservation trait (applies for vector<>)
///
template<class TStr, class TV, class TP, class TCount>
struct CStrTargetReserve
{
    static
    void Reserve(const TStr&     str, 
                 const TStr&     delim,
                 TV&             target,
                 CStrTokenizeBase::EMergeDelims  merge,
                 TP&             token_pos)
    {
        // Reserve vector size only for empty vectors.
        // For vectors which already have items this usualy works slower.
        if ( target.empty() ) {
            size_t tokens = TCount::Count(str, delim, merge);
            if (tokens) { 
                token_pos.reserve(tokens);
                target.reserve(tokens);
            }
        }
    }
};


/// Adapter for token position container pointer(NULL legal)
/// Makes pointer to a container look as a legal container
///
template<class TPosContainer>
class CStrTokenPosAdapter
{
public:
    /// If token_pos construction parameter is NULL all calls are ignored
    CStrTokenPosAdapter(TPosContainer* token_pos)
        : m_TokenPos(token_pos)
    {}

    void push_back(SIZE_TYPE pos)
    {
        if (m_TokenPos) m_TokenPos->push_back(pos);
    }
    void reserve(SIZE_TYPE capacity)
    {
        if (m_TokenPos) m_TokenPos->reserve(capacity);
    }
private:
    TPosContainer* m_TokenPos;
};



 /*
 * @}
 */

/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE

#endif  // CORELIB___STR_UTIL__HPP
