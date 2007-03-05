#ifndef NCBI_OS_MAC__HPP
#define NCBI_OS_MAC__HPP

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Mac specifics
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistr.hpp>

#if !defined(NCBI_OS_MAC)
#  error "ncbi_os_mac.hpp must be used on MAC platforms only"
#endif

#include <cassert>
#include <cstring>

#include <Files.h>
#include <MacTypes.h>

// If this variable gets set to true, then we should throw rather than exit() or abort().
extern bool g_Mac_SpecialEnvironment;
// Enable the coding style used by the special environment in question.
#define gMacSpecialEnvironment g_Mac_SpecialEnvironment

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//   COSErrException_Mac
//

class COSErrException_Mac : public exception
{
    OSErr m_OSErr;
    
public:
    // Report description of "os_err" (along with "what" if specified)
    COSErrException_Mac(const OSErr&  os_err,
                        const string& what = kEmptyStr) THROWS_NONE;
    ~COSErrException_Mac(void) THROWS_NONE;
    const OSErr& GetOSErr(void) const THROWS_NONE { return m_OSErr; }
};


/* Copy a C string (up to a limit) onto a Pascal string. */
inline
unsigned char*
Pstrncpy(unsigned char *dest, const char *src, size_t len)
{
    assert(len <= 255);
    if (len > 255) {
        len = 255;
    }
    dest[0] = static_cast<unsigned char>(len);
    memcpy(dest + 1, src, len);

    return dest;
}

/* Copy a C string to a Pascal string. */
inline
unsigned char*
Pstrcpy(unsigned char *dest, const char *src)
{
    return Pstrncpy(dest, src, std::strlen(src));
}

/* Copy a Pascal string to a Pascal string. */
inline
unsigned char*
PstrPcpy(unsigned char *dest, const unsigned char *src)
{
    return static_cast<unsigned char *>(
        std::memcpy(dest, src, static_cast<size_t>(src[0]+1) )
    );
}

class PString {
public:
    PString(const unsigned char* str) { PstrPcpy(m_Str, str); }
    friend bool operator==(const PString& one, const PString& other) {
        return std::memcmp(one.m_Str, other.m_Str, one.m_Str[0]) == 0;
    }
private:
    Str255 m_Str;
};


extern OSErr FSpGetDirectoryID(const FSSpec *spec, long *theDirID, Boolean *isDirectory);
extern OSErr FSpCheckObjectLock(const FSSpec *spec);
extern OSErr FSpGetFileSize(const FSSpec *spec, long *dataSize, long *rsrcSize);
extern OSErr GetDirItems(short vRefNum, long dirID, ConstStr255Param name,
    Boolean getFiles, Boolean getDirectories, FSSpecPtr items,
    short reqItemCount, short *actItemCount, short *itemIndex);
extern OSErr MacPathname2FSSpec(const char *inPathname, FSSpec *outFSS);
extern OSErr MacFSSpec2FullPathname(const FSSpec *inFSS, char **outPathname);


END_NCBI_SCOPE

#endif  /* NCBI_OS_MAC__HPP */
