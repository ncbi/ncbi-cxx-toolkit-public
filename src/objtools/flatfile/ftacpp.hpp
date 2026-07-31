/* $Id$
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
 * File Name: ftacpp.hpp
 *
 * Author: Serge Bazhin
 *
 * File Description:
 *    Contains common C++ Toolkit header files
 *
 */

#ifndef FTACPP_HPP
#define FTACPP_HPP

#include <cstring>
#include <charconv>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

inline char* StringNew(size_t sz)
{
    char* p = new char[sz + 1];
    std::memset(p, 0, sz + 1);
    return p;
}
inline void MemSet(void* p, int n, size_t sz) { std::memset(p, n, sz); }
inline void MemCpy(void* p, const void* q, size_t sz)
{
    if (q)
        std::memcpy(p, q, sz);
}
inline void MemFree(char* p)
{
    delete[] p;
}

inline size_t StringLen(const char* s) { return s ? std::strlen(s) : 0; }
inline char*  StringSave(const char* s)
{
    if (! s)
        return nullptr;
    const size_t n = std::strlen(s) + 1;
    char*        p = new char[n];
    std::memcpy(p, s, n);
    return p;
}
inline char* StringSave(string_view s)
{
    const size_t n = s.length();
    char*        p = new char[n + 1];
    std::memcpy(p, s.data(), n);
    p[n] = '\0';
    return p;
}
inline char* StringSave(unique_ptr<string> s)
{
    if (! s)
        return nullptr;
    return StringSave(*s);
}


inline const char* StringStr(const char* s1, const char* s2) { return std::strstr(s1, s2); }
inline char*       StringStr(char* s1, const char* s2) { return std::strstr(s1, s2); }
inline void        StringCat(char* d, const char* s) { std::strcat(d, s); }
inline void        StringCpy(char* d, const char* s) { std::strcpy(d, s); }
inline void        StringNCpy(char* d, const char* s, size_t n) { std::strncpy(d, s, n); }
inline const char* StringChr(const char* s, const char c) { return std::strchr(s, c); }
inline char*       StringChr(char* s, const char c) { return std::strchr(s, c); }
inline char*       StringRChr(char* s, const char c) { return std::strrchr(s, c); }

inline int StringCmp(const char* s1, const char* s2)
{
    if (s1) {
        if (s2) {
            return std::strcmp(s1, s2);
        } else {
            return 1;
        }
    } else {
        if (s2) {
            return -1;
        } else {
            return 0;
        }
    }
}
inline bool StringEqu(const char* s1, const char* s2)
{
    if (s1 && s2)
        return (std::strcmp(s1, s2) == 0);

    if (! s1 && ! s2)
        return true;

    return false;
}
inline bool StringEquN(const char* s1, const char* s2, size_t n)
{
    if (s1 && s2)
        return (std::strncmp(s1, s2, n) == 0);

    if (! s1 && ! s2)
        return true;

    return false;
}
inline bool fta_StartsWith(const char* s1, string_view s2)
{
    return string_view(s1).starts_with(s2);
}
inline bool StringEquNI(const char* s1, const char* s2, size_t n)
{
    const string S1(s1), S2(s2);
    return NStr::EqualNocase(S1.substr(0, n), S2.substr(0, n));
}

inline bool StringHasNoText(const char* s)
{
    if (s)
        while (*s)
            if ((unsigned char)(*s++) > ' ')
                return false;
    return true;
}

inline bool StringDoesHaveText(const char* s) { return ! StringHasNoText(s); }

inline int fta_atoi(string_view sv) {
    int n = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), n);
    return n;
}

END_NCBI_SCOPE

#endif // FTACPP_HPP
