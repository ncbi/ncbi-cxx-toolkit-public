#ifndef UTIL___TEXT_JOINER__HPP
#define UTIL___TEXT_JOINER__HPP

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
 * Authors:  Aaron Ucko
 *
 */

/// @file text_joiner.hpp
/// Template for collecting and joining strings with a minimum of heap churn.

#include <corelib/ncbistd.hpp>
#include <util/error_codes.hpp>


/** @addtogroup String
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// CTextJoiner<> -- template for efficiently collecting and joining strings.
///
/// @param num_prealloc
///   How many strings to store locally (without turning to the heap).
/// @param TIn
///   Input string type, std::string by default.  CTempString is an option
///   too, but users will need to ensure that its data remains valid through
///   any calls to Join().
/// @param TOut
///   Output string type, normally deduced from TIn.

template <size_t num_prealloc, typename TIn = string,
          typename TOut = basic_string<typename TIn::value_type> >
class CTextJoiner
{
public:
    CTextJoiner() : m_MainStorageUsage(0) { }

    CTextJoiner& Add (const TIn& s);
    void         Join(TOut* result) const;

private:
    TIn                    m_MainStorage[num_prealloc];
    auto_ptr<vector<TIn> > m_ExtraStorage;
    size_t                 m_MainStorageUsage;
};


template<size_t num_prealloc, typename TIn, typename TOut>
inline
CTextJoiner<num_prealloc, TIn, TOut>&
CTextJoiner<num_prealloc, TIn, TOut>::Add(const TIn& s)
{
    if (s.empty()) {
        return *this;
    }

    if (m_MainStorageUsage < num_prealloc) {
        m_MainStorage[m_MainStorageUsage++] = s;
    } else if (m_ExtraStorage.get() != NULL) {
        ERR_POST_XX_ONCE(Util_TextJoiner, 1,
                         Warning << "exceeding anticipated count "
                         << num_prealloc);
        m_ExtraStorage->push_back(s);
    } else {
        m_ExtraStorage.reset(new vector<TIn>(1, s));
    }

    return *this;
}


template<size_t num_prealloc, typename TIn, typename TOut>
inline
void CTextJoiner<num_prealloc, TIn, TOut>::Join(TOut* result) const
{
    SIZE_TYPE size_needed = 0;
    for (size_t i = 0;  i < m_MainStorageUsage;  ++i) {
        size_needed += m_MainStorage[i].size();
    }
    if (m_ExtraStorage.get() != NULL) {
        ITERATE (typename vector<TIn>, it, *m_ExtraStorage) {
            size_needed += it->size();
        }
    }

    result->clear();
    result->reserve(size_needed);
    for (size_t i = 0;  i < m_MainStorageUsage;  ++i) {
        result->append(m_MainStorage[i].data(), m_MainStorage[i].size());
    }
    if (m_ExtraStorage.get() != NULL) {
        ITERATE (typename vector<TIn>, it, *m_ExtraStorage) {
            result->append(it->data(), it->size());
        }
    }
}


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL___TEXT_JOINER__HPP */
