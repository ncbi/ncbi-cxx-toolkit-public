#ifndef CORELIB___NCBI_XSTR__HPP
#define CORELIB___NCBI_XSTR__HPP

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
 * Authors:  Andrei Gourianov
 *    Template-based string comparison
 *
 *
 */

#include <corelib/ncbistd.hpp>
#include <string.h>
#ifdef NCBI_OS_OSF1
#  include <strings.h>
#endif
#include <string>


BEGIN_NCBI_SCOPE

/** @addtogroup String
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////

#if defined(_UNICODE)
#  define NCBI_TEXT(x)      L ## x
#else
#  define NCBI_TEXT(x)      x
#endif

/////////////////////////////////////////////////////////////////////////////
//

template<typename _TChar>
class CTempXStr
{
public:
    CTempXStr(void)
        : m_Data(kEmptyStr.data()), m_Length(0)
    {
    }
    CTempXStr(const CTempXStr& lw)
        : m_Data(lw.m_Data), m_Length(lw.m_Length)
    {
    }

//---------------------------------------------------------------------------
    CTempXStr(const _TChar* str)
        : m_Data(str), m_Length(NPOS)
    {
    }
    CTempXStr(const _TChar* str, size_t length)
        : m_Data(str), m_Length(length)
    {
    }
    CTempXStr(const _TChar* str, size_t pos, size_t length)
    {
        if (pos == x_npos()) {
            m_Data = str;
            m_Length = 0;    
        } else {
            m_Data = str+pos;
            m_Length = length;
        }
    }

//---------------------------------------------------------------------------
    CTempXStr(const basic_string<_TChar>& str)
        : m_Data(str.data()), m_Length(str.length())
    {
    }
    CTempXStr(const basic_string<_TChar>& str, size_t length)
        : m_Data(str.data()), m_Length(length)
    {
    }
    CTempXStr(const basic_string<_TChar>& str, size_t pos, size_t length)
        : m_Data(str.data()+pos), m_Length(length)
    {
        if (pos == x_npos()) {
            m_Data = str.data();
            m_Length = 0;
        } else {
            m_Data = str.data()+pos;
            if (length == x_npos()) {
                m_Length = str.length() - pos;
            } else {
                m_Length = length;
                if (m_Length + pos > str.length()) {
                    m_Length = str.length() - pos;
                }
            }
        }
    }
    operator basic_string<_TChar>(void) const
    {
        return basic_string<_TChar>(data(), length());
    }

//---------------------------------------------------------------------------
// data access
    const _TChar* data(void) const
    {
        return m_Data;
    }
    size_t length(void) const
    {
        return m_Length == NPOS ? x_length() : m_Length;
    }
    bool empty(void)  const
    {
        return length() == 0;
    }
    _TChar operator[] (size_t pos) const
    {
        if (length() <= pos) {
            return _TChar(0);
        }
        return *(m_Data+pos);
    }
//---------------------------------------------------------------------------

    int CompareCase(const CTempXStr<_TChar>& pattern) const
    {
        size_t n = length();
        if (pattern.length() < n) {
            n = pattern.length();
        }
        const _TChar *s = data();
        const _TChar *p = pattern.data();
        size_t c = 0;
        for ( ; n-- && (*s == *p); ++s,++p,++c )
            ;
        _TChar se = operator[](c);
        _TChar pe = pattern[c];
        if ( se == pe) {
            return 0;
        }
        return se < pe ? -1 : 1;
    }

    int CompareNocase(const CTempXStr<_TChar>& pattern) const
    {
#if defined(NCBI_COMPILER_GCC) && (NCBI_COMPILER_VERSION <= 295)
#  define CT_TOLOWER(x) tolower((unsigned)(x))
#else
        const ctype<_TChar>& ct =

#if defined(NCBI_COMPILER_WORKSHOP)
//#if !defined(_RWSTD_NOTEMPLATE_ON_RETURN_TYPE)
// Old style; on newer compilers this is deprecated
            use_facet( locale(), (ctype<_TChar>*)0);
#else
            use_facet< ctype<_TChar> >(locale());
#endif
#  define CT_TOLOWER(x) ct.tolower(x)
#endif

        size_t n = length();
        if (pattern.length() < n) {
            n = pattern.length();
        }
        const _TChar *s = data();
        const _TChar *p = pattern.data();
        size_t c = 0;
        for ( ; n-- && (CT_TOLOWER( *s ) == CT_TOLOWER( *p )); ++s,++p,++c )
            ;
        _TChar se = CT_TOLOWER( operator[](c) );
        _TChar pe = CT_TOLOWER( pattern[c] );
        if ( se == pe) {
            return 0;
        }
        return se < pe ? -1 : 1;
#undef CT_TOLOWER
    }

private:
    static size_t x_npos(void)
    {
        return (size_t)NPOS;
    }
    size_t x_length(void) const
    {
#if defined(NCBI_COMPILER_GCC) && (NCBI_COMPILER_VERSION <= 295)
        return (m_Length = string_char_traits<_TChar>::length(m_Data));
#else
        return (m_Length = char_traits<_TChar>::length(m_Data));
#endif
    }
    const _TChar* m_Data;
    mutable size_t  m_Length;
};


/////////////////////////////////////////////////////////////////////////////
///
/// XStr --
///
/// Template-based string comparison

class XStr
{
public:
    enum ECase {
        eCase,      ///< Case sensitive compare
        eNocase     ///< Case insensitive compare
    };
// --------------------------------------------------------------------------
// CompareCase
    template< typename _TChar >
    static
    int CompareCase(const basic_string<_TChar>& str, SIZE_TYPE pos, SIZE_TYPE n, const _TChar* pattern)
    {
#if defined(NCBI_COMPILER_GCC) && (NCBI_COMPILER_VERSION <= 295)
        return CTempXStr<_TChar>(str,pos,n).CompareCase(CTempXStr<_TChar>(pattern));
#else
        return str.compare(pos,n,pattern);
#endif
    }
    template< typename _TChar >
    static
    int CompareCase(const _TChar* str, SIZE_TYPE pos, SIZE_TYPE n, const _TChar* pattern)
    {
#if defined(NCBI_COMPILER_GCC) && (NCBI_COMPILER_VERSION <= 295)
        return CTempXStr<_TChar>(str,pos,n).CompareCase(CTempXStr<_TChar>(pattern));
#else
        return basic_string<_TChar>(str).compare(pos,n,pattern);
#endif
    }

    template< typename _TChar >
    static
    int CompareCase(const basic_string<_TChar>& str, SIZE_TYPE pos, SIZE_TYPE n, const basic_string<_TChar>& pattern)
    {
#if defined(NCBI_COMPILER_GCC) && (NCBI_COMPILER_VERSION <= 295)
        return CTempXStr<_TChar>(str,pos,n).CompareCase(CTempXStr<_TChar>(pattern));
#else
        return str.compare(pos,n,pattern);
#endif
    }
    template< typename _TChar >
    static
    int CompareCase(const _TChar* str, SIZE_TYPE pos, SIZE_TYPE n, const basic_string<_TChar>& pattern)
    {
#if defined(NCBI_COMPILER_GCC) && (NCBI_COMPILER_VERSION <= 295)
        return CTempXStr<_TChar>(str,pos,n).CompareCase(CTempXStr<_TChar>(pattern));
#else
        return basic_string<_TChar>(str).compare(pos,n,pattern);
#endif
    }
// --------------------------------------------------------------------------
// CompareNocase
    template< typename _TChar >
    static
    int CompareNocase(const basic_string<_TChar>& str, SIZE_TYPE pos, SIZE_TYPE n, const _TChar* pattern)
    {
        return CTempXStr<_TChar>(str,pos,n).CompareNocase(CTempXStr<_TChar>(pattern));
    }
    template< typename _TChar >
    static
    int CompareNocase(const _TChar* str, SIZE_TYPE pos, SIZE_TYPE n, const _TChar* pattern)
    {
        return CTempXStr<_TChar>(str,pos,n).CompareNocase(CTempXStr<_TChar>(pattern));
    }

    template< typename _TChar >
    static
    int CompareNocase(const basic_string<_TChar>& str, SIZE_TYPE pos, SIZE_TYPE n, const basic_string<_TChar>& pattern)
    {
        return CTempXStr<_TChar>(str,pos,n).CompareNocase(CTempXStr<_TChar>(pattern));
    }
    template< typename _TChar >
    static
    int CompareNocase(const _TChar* str, SIZE_TYPE pos, SIZE_TYPE n, const basic_string<_TChar>& pattern)
    {
        return CTempXStr<_TChar>(str,pos,n).CompareNocase(CTempXStr<_TChar>(pattern));
    }
// --------------------------------------------------------------------------
// Compare with 4 args
    template< typename _TChar >
    static
    int Compare(const basic_string<_TChar>& str, SIZE_TYPE pos, SIZE_TYPE n, const _TChar* pattern, ECase use_case = eCase)
    {
        return use_case == eCase ? CompareCase(str,pos,n,pattern) : CompareNocase(str,pos,n,pattern);
    }
    template< typename _TChar >
    static
    int Compare(const _TChar* str, SIZE_TYPE pos, SIZE_TYPE n, const _TChar* pattern, ECase use_case = eCase)
    {
        return use_case == eCase ? CompareCase(str,pos,n,pattern) : CompareNocase(str,pos,n,pattern);
    }

    template< typename _TChar >
    static
    int Compare(const basic_string<_TChar>& str, SIZE_TYPE pos, SIZE_TYPE n, const basic_string<_TChar>& pattern, ECase use_case = eCase)
    {
        return use_case == eCase ? CompareCase(str,pos,n,pattern) : CompareNocase(str,pos,n,pattern);
    }
    template< typename _TChar >
    static
    int Compare(const _TChar* str, SIZE_TYPE pos, SIZE_TYPE n, const basic_string<_TChar>& pattern, ECase use_case = eCase)
    {
        return use_case == eCase ? CompareCase(str,pos,n,pattern) : CompareNocase(str,pos,n,pattern);
    }

// --------------------------------------------------------------------------
// strcmp, strncmp, strcasecmp, strncasecmp
    template< typename _TChar >
    static 
    int strcmp( const _TChar* s1, const _TChar* s2)
    {
        return CTempXStr<_TChar>(s1).CompareCase(CTempXStr<_TChar>(s2));
    }
    template< typename _TChar >
    static 
    int strncmp( const _TChar* s1, const _TChar* s2, size_t n)
    {
        return CTempXStr<_TChar>(s1,n).CompareCase(CTempXStr<_TChar>(s2,n));
    }
    template< typename _TChar >
    static 
    int strcasecmp( const _TChar* s1, const _TChar* s2)
    {
        return CTempXStr<_TChar>(s1).CompareNocase(CTempXStr<_TChar>(s2));
    }
    template< typename _TChar >
    static 
    int strncasecmp( const _TChar* s1, const _TChar* s2, size_t n)
    {
        return CTempXStr<_TChar>(s1,n).CompareNocase(CTempXStr<_TChar>(s2,n));
    }

// --------------------------------------------------------------------------
// CompareCase
    template< typename _TChar >
    static 
    int CompareCase( const _TChar* s1, const _TChar* s2)
    {
        return strcmp(s1,s2);
    }
    template< typename _TChar >
    static 
    int CompareCase( const basic_string<_TChar>& s1, const basic_string<_TChar>& s2)
    {
        return strcmp(s1.c_str(),s2.c_str());
    }

// --------------------------------------------------------------------------
// CompareNocase
    template< typename _TChar >
    static 
    int CompareNocase( const _TChar* s1, const _TChar* s2)
    {
        return strcasecmp(s1,s2);
    }
    template< typename _TChar >
    static 
    int CompareNocase( const basic_string<_TChar>& s1, const basic_string<_TChar>& s2)
    {
        return strcasecmp(s1.c_str(),s2.c_str());
    }

// --------------------------------------------------------------------------
// Compare
    template< typename _TChar >
    static 
    int Compare( const basic_string<_TChar>& s1, const _TChar* s2, ECase use_case = eCase)
    {
        return use_case == eCase ? CompareCase(s1.c_str(),s2) : CompareNocase(s1.c_str(),s2);
    }
    template< typename _TChar >
    static 
    int Compare( const _TChar* s1, const _TChar* s2, ECase use_case = eCase)
    {
        return use_case == eCase ? CompareCase(s1,s2) : CompareNocase(s1,s2);
    }
    template< typename _TChar >
    static 
    int Compare( const basic_string<_TChar>& s1, const basic_string<_TChar>& s2, ECase use_case = eCase)
    {
        return use_case == eCase ? CompareCase(s1.c_str(),s2.c_str()) : CompareNocase(s1.c_str(),s2.c_str());
    }
    template< typename _TChar >
    static 
    int Compare( const _TChar* s1, const basic_string<_TChar>& s2, ECase use_case = eCase)
    {
        return use_case == eCase ? CompareCase(s1,s2.c_str()) : CompareNocase(s1,s2.c_str());
    }
};

/* @} */

END_NCBI_SCOPE

#endif  /* CORELIB___NCBI_XSTR__HPP */
