/* ftacpp.hpp
 *
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
 * File Name:  ftacpp.hpp
 *
 * Author: Serge Bazhin
 *
 * File Description:
 * -----------------
 *    Contains common C++ Toolkit header files
 *
 */

#ifndef FTACPP_HPP
#define FTACPP_HPP

#include <cstring>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

inline void* MemNew(size_t sz) { void* p = new char[sz]; std::memset(p, 0, sz); return p; }
inline void* MemSet(void* p, int n, size_t sz) { return std::memset(p, n, sz); }
inline void* MemCpy(void* p, void* q, size_t sz) { return std::memcpy(p, q, sz); }
inline void* MemFree(void* p) { delete[](p); return 0; }

inline size_t StringLen(const char* s) { return s ? std::strlen(s) : 0; }
inline char* StringSave(const char* s) { if (!s) return 0; const size_t n = std::strlen(s) + 1; char* p = new char[n]; std::memcpy(p, s, n); return p; }
inline char* StringStr(const char* s1, const char* s2) { return const_cast<char*>(std::strstr(s1, s2)); }
inline char* StringCat(char* d, const char* s) { return std::strcat(d, s); }
inline char* StringCpy(char* d, const char* s) { return std::strcpy(d, s); }
inline char* StringNCpy(char* d, const char* s, size_t n) { return std::strncpy(d, s, n); }
inline char* StringChr(const char* s, const int c) { return const_cast<char*>(std::strchr(s, c)); }
inline char* StringRChr(char* s, const int c) { return std::strrchr(s, c); }
inline int StringCmp(const char* s1, const char* s2) { return std::strcmp(s1, s2); }
inline int StringNCmp(const char* s1, const char* s2, size_t n) { return std::strncmp(s1, s2, n); }
inline int StringNICmp(const char* s1, const char* s2, size_t n) { const string S1(s1), S2(s2); return NStr::CompareNocase(S1.substr(0, n), S2.substr(0, n)); }

inline char* StringMove(char* d, const char* s) { return s && d ? std::strcpy(d, s) + std::strlen(s) : d; }

inline bool StringHasNoText(const char* s) {
    if (s) while (*s) if ((unsigned char)(*s++) > ' ') return false;
    return true;
}

inline bool StringDoesHaveText(const char* s) { return !StringHasNoText(s); }

END_NCBI_SCOPE

#endif // FTACPP_HPP
