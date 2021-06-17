#ifndef LIGHTSTR__HPP
#define LIGHTSTR__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   CLightString - class with "light" strings: char pointer + string length
*/

#if defined __GNUG__
# warning This file is deprecated and will be removed.                  \
    Please, change CLightString to CTempString with PQuickStringLess
#endif

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <string.h>


/** @addtogroup LightStr
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CLightString
{
public:
    /// Use class CTempString with PQuickStringLess comparator
    NCBI_DEPRECATED_CTOR(CLightString(void));
    /// Use class CTempString with PQuickStringLess comparator
    NCBI_DEPRECATED_CTOR(CLightString(const char* str));
    /// Use class CTempString with PQuickStringLess comparator
    NCBI_DEPRECATED_CTOR(CLightString(const char* str, size_t length));
    /// Use class CTempString with PQuickStringLess comparator
    NCBI_DEPRECATED_CTOR(CLightString(const string& str));
    /// Use class CTempString with PQuickStringLess comparator
    NCBI_DEPRECATED_CTOR(CLightString(const CTempString& str));


    const char* GetString(void) const
        {
            _ASSERT(m_String);
            return m_String;
        }
    size_t GetLength(void) const
        {
            return m_Length;
        }

    bool Empty(void) const
        {
            return m_Length == 0;
        }

    operator string(void) const
        {
            return string(GetString(), GetLength());
        }

    operator CTempString(void) const
        {
            return CTempString(GetString(), GetLength());
        }

    bool operator<(const CLightString& cmp) const
        {
            size_t len = GetLength();
            size_t cmpLen = cmp.GetLength();
            if ( len == cmpLen )
                return memcmp(GetString(), cmp.GetString(), len) < 0;
            else
                return len < cmpLen;
        }

    bool EqualTo(const CLightString& cmp) const
        {
            size_t l = cmp.GetLength();
            return GetLength() == l &&
                memcmp(GetString(), cmp.GetString(), l) == 0;
        }
    bool EqualTo(const string& cmp) const
        {
            size_t l = cmp.size();
            return GetLength() == l &&
                memcmp(GetString(), cmp.data(), l) == 0;
        }
    bool EqualTo(const char* s, size_t l) const
        {
            return GetLength() == l &&
                memcmp(GetString(), s, l) == 0;
        }

    bool operator==(const CLightString& cmp) const
        {
            return EqualTo(cmp);
        }
    bool operator==(const string& cmp) const
        {
            return EqualTo(cmp);
        }
    bool operator==(const char* cmp) const
        {
            return EqualTo(cmp, strlen(cmp));
        }
    bool operator!=(const CLightString& cmp) const
        {
            return !EqualTo(cmp);
        }
    bool operator!=(const string& cmp) const
        {
            return !EqualTo(cmp);
        }
    bool operator!=(const char* cmp) const
        {
            return !EqualTo(cmp, strlen(cmp));
        }

private:
    const char* m_String;
    size_t m_Length;
};

inline
CLightString::CLightString(void)
    : m_String(""), m_Length(0)
{
}
inline
CLightString::CLightString(const char* str)
    : m_String(str), m_Length(strlen(str))
{
}
inline
CLightString::CLightString(const char* str, size_t length)
    : m_String(str), m_Length(length)
{
}
inline
CLightString::CLightString(const string& str)
    : m_String(str.data()), m_Length(str.size())
{
}
inline
CLightString::CLightString(const CTempString& str)
    : m_String(str.data()), m_Length(str.length())
{
}

/* @} */

END_NCBI_SCOPE

#endif  /* LIGHTSTR__HPP */
