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
 * Authors:  Vladimir Ivanov
 *           Anton Lavrentiev
 *
 * File Description:
 *   Tar archive API.
 *
 *   Supports subsets of POSIX.1-1988 (ustar), POSIX 1003.1-2001 (posix),
 *   old GNU (POSIX 1003.1), and V7 formats (all partially but reasonably).
 *   New archives are created using POSIX (genuine ustar) format, using
 *   GNU extensions for long names/links only when unavoidable.  It cannot,
 *   however, handle all the exotics like sparse / contiguous files (yet still
 *   can work around them gracefully, if needed), multivolume / incremental
 *   archives, etc. but just regular files, devices (character or block),
 *   FIFOs, directories, and limited links:  can extract both hard- and
 *   symlinks, but can store symlinks only.  Also, this implementation is
 *   only minimally PAX(Portable Archive eXchange)-aware for file extractions
 *   (but cannot use PAX extensions to store the files).
 *
 */

#include <ncbi_pch.hpp>
// Cancel __wur (warn unused result) ill effects in GCC
#ifdef   _FORTIFY_SOURCE
#  undef _FORTIFY_SOURCE
#endif /*_FORTIFY_SOURCE*/
#define  _FORTIFY_SOURCE 0
#include <corelib/ncbi_system.hpp>
#include <util/compress/tar.hpp>
#include <util/error_codes.hpp>

#if !defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_MSWIN)
#  error "Class CTar can be defined on UNIX and MS-Windows platforms only!"
#endif

#ifdef NCBI_OS_UNIX
#  include "../../../corelib/ncbi_os_unix_p.hpp"
#  include <grp.h>
#  include <pwd.h>
#  include <unistd.h>
#  ifdef NCBI_OS_IRIX
#    include <sys/mkdev.h>
#    if !defined(major)  ||  !defined(minor)  ||  !defined(makedev)
#      error "Device macros undefined in this UNIX build!"
#    endif
#  endif
#endif //NCBI_OS_UNIX
#ifdef NCBI_OS_MSWIN
#  include "../../../corelib/ncbi_os_mswin_p.hpp"
#  include <io.h>
typedef unsigned int mode_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
#endif //NCBI_OS_MSWIN


#define NCBI_USE_ERRCODE_X  Util_Compress
#define NCBI_MODULE         NCBITAR


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// TAR helper routines
//

// Convert a number to an octal string padded to the left
// with [leading] zeros ('0') and having _no_ trailing '\0'.
static bool s_NumToOctal(Uint8 val, char* ptr, size_t len)
{
    _ASSERT(len > 0);
    do {
        ptr[--len] = '0' + char(val & 7);
        val >>= 3;
    } while (len);
    return val ? false : true;
}


// Convert an octal number (possibly preceded by spaces) to numeric form.
// Stop either at the end of the field or at first '\0' (if any).
static bool s_OctalToNum(Uint8& val, const char* ptr, size_t len)
{
    _ASSERT(ptr  &&  len > 0);
    size_t i = *ptr ? 0 : 1;
    while (i < len  &&  ptr[i]) {
        if (!isspace((unsigned char) ptr[i]))
            break;
        i++;
    }
    val = 0;
    bool okay = false;
    while (i < len  &&  ptr[i] >= '0'  &&  ptr[i] <= '7') {
        okay  = true;
        val <<= 3;
        val  |= ptr[i++] - '0';
    }
    while (i < len  &&  ptr[i]) {
        if (!isspace((unsigned char) ptr[i]))
            return false;
        i++;
    }
    return okay;
}


static bool s_NumToBase256(Uint8 val, char* ptr, size_t len)
{
    _ASSERT(len > 0);
    do {
        ptr[--len] = (unsigned char)(val & 0xFF);
        val >>= 8;
    } while (len);
    *ptr |= '\x80';  // set base-256 encoding flag
    return val ? false : true;
}


// Return 0 (false) if conversion failed; 1 if the value converted to
// conventional octal representation (perhaps, with terminating '\0'
// sacrificed), or -1 if the value converted using base-256.
static int s_EncodeUint8(Uint8 val, char* ptr, size_t len)
{                                           // Max file size (for len == 12):
    if (s_NumToOctal  (val, ptr,   len)) {  //   8GiB-1
        return  1/*okay*/;
    }
    if (s_NumToOctal  (val, ptr, ++len)) {  //   64GiB-1
        return  1/*okay*/;
    }
    if (s_NumToBase256(val, ptr,   len)) {  //   up to 2^94-1
        return -1/*okay, base-256*/;
    }
    return 0/*failure*/;
}


// Return true if conversion succeeded;  false otherwise.
static bool s_Base256ToNum(Uint8& val, const char* ptr, size_t len)
{
    const Uint8 lim = kMax_UI8 >> 8;
    if (*ptr & '\x40') {  // negative base-256?
        return false;
    }
    val = *ptr++ & '\x3F';
    while (--len) {
        if (val > lim) {
            return false;
        }
        val <<= 8;
        val  |= (unsigned char)(*ptr++);
    }
    return true;
}


// Return 0 (false) if conversion failed; 1 if the value was read into
// as a conventional octal string (perhaps, without the terminating '\0');
// or -1 if base-256 representation used.
static int s_DecodeUint8(Uint8& val, const char* ptr, size_t len)
{
    if (*ptr & '\x80') {
        return s_Base256ToNum(val, ptr, len) ? -1/*okay*/ : 0/*failure*/;
    } else {
        return s_OctalToNum  (val, ptr, len) ?  1/*okay*/ : 0/*failure*/;
    }
}


static void s_TarToMode(TTarMode                     perm,
                        CDirEntry::TMode*            usr_mode,
                        CDirEntry::TMode*            grp_mode,
                        CDirEntry::TMode*            oth_mode,
                        CDirEntry::TSpecialModeBits* special_bits)
{
    // User
    if (usr_mode) {
        *usr_mode = ((perm & fTarURead    ? CDirEntry::fRead    : 0) |
                     (perm & fTarUWrite   ? CDirEntry::fWrite   : 0) |
                     (perm & fTarUExecute ? CDirEntry::fExecute : 0));
    }

    // Group
    if (grp_mode) {
        *grp_mode = ((perm & fTarGRead    ? CDirEntry::fRead    : 0) |
                     (perm & fTarGWrite   ? CDirEntry::fWrite   : 0) |
                     (perm & fTarGExecute ? CDirEntry::fExecute : 0));
    }

    // Others
    if (oth_mode) {
        *oth_mode = ((perm & fTarORead    ? CDirEntry::fRead    : 0) |
                     (perm & fTarOWrite   ? CDirEntry::fWrite   : 0) |
                     (perm & fTarOExecute ? CDirEntry::fExecute : 0));
    }

    // Special bits
    if (special_bits) {
        *special_bits = ((perm & fTarSetUID ? CDirEntry::fSetUID : 0) |
                         (perm & fTarSetGID ? CDirEntry::fSetGID : 0) |
                         (perm & fTarSticky ? CDirEntry::fSticky : 0));
    }
}


static mode_t s_TarToMode(TTarMode perm)
{
    mode_t mode = (
#ifdef S_ISUID
                   (perm & fTarSetUID   ? S_ISUID  : 0) |
#endif
#ifdef S_ISGID
                   (perm & fTarSetGID   ? S_ISGID  : 0) |
#endif
#ifdef S_ISVTX
                   (perm & fTarSticky   ? S_ISVTX  : 0) |
#endif
#if   defined(S_IRUSR)
                   (perm & fTarURead    ? S_IRUSR  : 0) |
#elif defined(S_IREAD)
                   (perm & fTarURead    ? S_IREAD  : 0) |
#endif
#if   defined(S_IWUSR)
                   (perm & fTarUWrite   ? S_IWUSR  : 0) |
#elif defined(S_IWRITE)
                   (perm & fTarUWrite   ? S_IWRITE : 0) |
#endif
#if   defined(S_IWUSR)
                   (perm & fTarUExecute ? S_IXUSR  : 0) |
#elif defined(S_IEXEC)
                   (perm & fTarUExecute ? S_IEXEC  : 0) |
#endif
#ifdef S_IRGRP
                   (perm & fTarGRead    ? S_IRGRP  : 0) |
#endif
#ifdef S_IWGRP
                   (perm & fTarGWrite   ? S_IWGRP  : 0) |
#endif
#ifdef S_IXGRP
                   (perm & fTarGExecute ? S_IXGRP  : 0) |
#endif
#ifdef S_IROTH
                   (perm & fTarORead    ? S_IROTH  : 0) |
#endif
#ifdef S_IWOTH
                   (perm & fTarOWrite   ? S_IWOTH  : 0) |
#endif
#ifdef S_IXOTH
                   (perm & fTarOExecute ? S_IXOTH  : 0) |
#endif
                   0);
    return mode;
}


static TTarMode s_ModeToTar(mode_t mode)
{
    // Keep in mind that the mode may be extracted on a different platform
    TTarMode perm = (
#ifdef S_ISUID
                     (mode & S_ISUID  ? fTarSetUID   : 0) |
#endif
#ifdef S_ISGID
                     (mode & S_ISGID  ? fTarSetGID   : 0) |
#endif
#ifdef S_ISVTX
                     (mode & S_ISVTX  ? fTarSticky   : 0) |
#endif
#if   defined(S_IRUSR)
                     (mode & S_IRUSR  ? fTarURead    : 0) |
#elif defined(S_IREAD)
                     (mode & S_IREAD  ? fTarURead    : 0) |
#endif
#if   defined(S_IWUSR)
                     (mode & S_IWUSR  ? fTarUWrite   : 0) |
#elif defined(S_IWRITE)
                     (mode & S_IWRITE ? fTarUWrite   : 0) |
#endif
#if   defined(S_IXUSR)
                     (mode & S_IXUSR  ? fTarUExecute : 0) |
#elif defined(S_IEXEC)
                     (mode & S_IEXEC  ? fTarUExecute : 0) |
#endif
#if   defined(S_IRGRP)
                     (mode & S_IRGRP  ? fTarGRead    : 0) |
#elif defined(S_IREAD)
                     // emulate read permission when file is readable
                     (mode & S_IREAD  ? fTarGRead    : 0) |
#endif
#ifdef S_IWGRP
                     (mode & S_IWGRP  ? fTarGWrite   : 0) |
#endif
#ifdef S_IXGRP
                     (mode & S_IXGRP  ? fTarGExecute : 0) |
#endif
#if   defined(S_IROTH)
                     (mode & S_IROTH  ? fTarORead    : 0) |
#elif defined(S_IREAD)
                     // emulate read permission when file is readable
                     (mode & S_IREAD  ? fTarORead    : 0) |
#endif
#ifdef S_IWOTH
                     (mode & S_IWOTH  ? fTarOWrite   : 0) |
#endif
#ifdef S_IXOTH
                     (mode & S_IXOTH  ? fTarOExecute : 0) |
#endif
                     0);
#if defined(S_IFMT)  ||  defined(_S_IFMT)
    TTarMode mask = (TTarMode) mode;
#  ifdef S_IFMT
    mask &=  S_IFMT;
#  else
    mask &= _S_IFMT;
#  endif
    if (!(mask & 07777)) {
        perm |= mask;
    }
#endif
    return perm;
}


static size_t s_Length(const char* ptr, size_t maxsize)
{
    const char* pos = (const char*) memchr(ptr, '\0', maxsize);
    return pos ? (size_t)(pos - ptr) : maxsize;
}


//////////////////////////////////////////////////////////////////////////////
//
// Constants / macros / typedefs
//

/// Round up to the nearest multiple of BLOCK_SIZE:
//#define ALIGN_SIZE(size)   SIZE_OF(BLOCK_OF(size + (BLOCK_SIZE-1)))
#define ALIGN_SIZE(size) (((size) + (BLOCK_SIZE-1)) & ~(BLOCK_SIZE-1))
#define OFFSET_OF(size)  ( (size)                   &  (BLOCK_SIZE-1))
#define BLOCK_OF(pos)    ((pos) >> 9)
#define SIZE_OF(blk)     ((blk) << 9)

/// Tar block size (512 bytes)
#define BLOCK_SIZE       SIZE_OF(1)


/// Recognized TAR formats
enum ETar_Format {
    eTar_Unknown = 0,
    eTar_Legacy  = 1,
    eTar_OldGNU  = 2,
    eTar_Ustar   = 4,
    eTar_Posix   = 5,
    eTar_Star    = 6
};


/// POSIX "ustar" tar archive member header
typedef struct SHeader {      // byte offset
    char name[100];           //   0
    char mode[8];             // 100
    char uid[8];              // 108
    char gid[8];              // 116
    char size[12];            // 124
    char mtime[12];           // 136
    char checksum[8];         // 148
    char typeflag[1];         // 156
    char linkname[100];       // 157
    char magic[6];            // 257
    char version[2];          // 263
    char uname[32];           // 265
    char gname[32];           // 297
    char devmajor[8];         // 329
    char devminor[8];         // 337
    union {                   // 345
        char prefix[155];     // NB: not valid with old GNU format (no need)
        struct {              // NB:                old GNU format only
            char atime[12];
            char ctime[12];   // 357
            char unused[17];  // 369
            char sparse[96];  // 386 sparse map: [12] offset + [12] gapsize
            char extend[1];   // 482 non-zero if to continue in the next header
            char extent[12];  // 483 true file size
        } gnu;
        struct {
            char prefix[131]; // NB: prefix + 107 == extent (char[12]) for 'S'
            char atime[12];   // 476
            char ctime[12];   // 488
        } star;
    };                        // 500
    // NCBI in last 4 bytes   // 508
} SHeader;


/// Block as a header.
union TBlock {
    char    buffer[BLOCK_SIZE];
    SHeader header;
};


static bool s_TarChecksum(TBlock* block, bool isgnu)
{
    SHeader* h = &block->header;
    size_t len = sizeof(h->checksum) - (isgnu ? 2 : 1);

    // Compute the checksum
    memset(h->checksum, ' ', sizeof(h->checksum));
    unsigned long checksum = 0;
    const unsigned char* p = (const unsigned char*) block->buffer;
    for (size_t i = 0;  i < sizeof(block->buffer);  ++i) {
        checksum += *p++;
    }
    // ustar:       '\0'-terminated checksum
    // GNU special: 6 digits, then '\0', then a space [already in place]
    if (!s_NumToOctal(checksum, h->checksum, len)) {
        return false;
    }
    h->checksum[len] = '\0';
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CTarEntryInfo
//

TTarMode CTarEntryInfo::GetMode(void) const
{
    // Raw tar mode gets returned here (as kept in the info)
    return (TTarMode)(m_Stat.st_mode & 07777);
}


void CTarEntryInfo::GetMode(CDirEntry::TMode*            usr_mode,
                            CDirEntry::TMode*            grp_mode,
                            CDirEntry::TMode*            oth_mode,
                            CDirEntry::TSpecialModeBits* special_bits) const
{
    s_TarToMode(GetMode(), usr_mode, grp_mode, oth_mode, special_bits);
}


unsigned int CTarEntryInfo::GetMajor(void) const
{
#ifdef major
    if (m_Type == eCharDev  ||  m_Type == eBlockDev) {
        return major(m_Stat.st_rdev);
    }
#else
    if (sizeof(int) >= 4  &&  sizeof(m_Stat.st_rdev) >= 4) {
        return (*((unsigned int*) &m_Stat.st_rdev) >> 16) & 0xFFFF;
    }
#endif //major
    return (unsigned int)(-1);
}


unsigned int CTarEntryInfo::GetMinor(void) const
{
#ifdef minor
    if (m_Type == eCharDev  ||  m_Type == eBlockDev) {
        return minor(m_Stat.st_rdev);
    }
#else
    if (sizeof(int) >= 4  &&  sizeof(m_Stat.st_rdev) >= 4) {
        return *((unsigned int*) &m_Stat.st_rdev) & 0xFFFF;
    }
#endif //minor
    return (unsigned int)(-1);
}


bool CTarEntryInfo::operator == (const CTarEntryInfo& info) const
{
    return (m_Type       == info.m_Type                        &&
            m_Name       == info.m_Name                        &&
            m_LinkName   == info.m_LinkName                    &&
            m_UserName   == info.m_UserName                    &&
            m_GroupName  == info.m_GroupName                   &&
            m_HeaderSize == info.m_HeaderSize                  &&
            memcmp(&m_Stat,&info.m_Stat, sizeof(m_Stat)) == 0  &&
            m_Pos        == info.m_Pos ? true : false);
}


static string s_ModeAsString(TTarMode mode)
{
    char buf[9];
    memset(buf, '-', sizeof(buf));

    char* usr = buf;
    char* grp = usr + 3;
    char* oth = grp + 3;

    if (mode & fTarURead) {
        usr[0] = 'r';
    }
    if (mode & fTarUWrite) {
        usr[1] = 'w';
    }
    if (mode & fTarUExecute) {
        usr[2] = mode & fTarSetUID ? 's' : 'x';
    } else if (mode & fTarSetUID) {
        usr[2] = 'S';
    }
    if (mode & fTarGRead) {
        grp[0] = 'r';
    }
    if (mode & fTarGWrite) {
        grp[1] = 'w';
    }
    if (mode & fTarGExecute) {
        grp[2] = mode & fTarSetGID ? 's' : 'x';
    } else if (mode & fTarSetGID) {
        grp[2] = 'S';
    }
    if (mode & fTarORead) {
        oth[0] = 'r';
    }
    if (mode & fTarOWrite) {
        oth[1] = 'w';
    }
    if (mode & fTarOExecute) {
        oth[2] = mode & fTarSticky ? 't' : 'x';
    } else if (mode & fTarSticky) {
        oth[2] = 'T';
    }

    return string(buf, sizeof(buf));
}


static char s_TypeAsChar(CTarEntryInfo::EType type)
{
    switch (type) {
    case CTarEntryInfo::eFile:
    case CTarEntryInfo::eHardLink:
        return '-';
    case CTarEntryInfo::eSymLink:
        return 'l';
    case CTarEntryInfo::eDir:
        return 'd';
    case CTarEntryInfo::ePipe:
        return 'p';
    case CTarEntryInfo::eCharDev:
        return 'c';
    case CTarEntryInfo::eBlockDev:
        return 'b';
    default:
        break;
    }
    return '?';
}


static string s_UserGroupAsString(const CTarEntryInfo& info)
{
    string user(info.GetUserName());
    if (user.empty()) {
        NStr::UIntToString(user, info.GetUserId());
    }
    string group(info.GetGroupName());
    if (group.empty()) {
        NStr::UIntToString(group, info.GetGroupId());
    }
    return user + '/' + group;
}


static string s_MajorMinor(unsigned int n)
{
    return n != (unsigned int)(-1) ? NStr::UIntToString(n) : string(1, '?');
}


static string s_SizeOrMajorMinor(const CTarEntryInfo& info)
{
    if (info.GetType() == CTarEntryInfo::eCharDev  ||
        info.GetType() == CTarEntryInfo::eBlockDev) {
        unsigned int major = info.GetMajor();
        unsigned int minor = info.GetMinor();
        return s_MajorMinor(major) + ',' + s_MajorMinor(minor);
    } else if (info.GetType() == CTarEntryInfo::eDir  ||
               info.GetType() == CTarEntryInfo::eSymLink) {
        return string("-");
    }
    return NStr::UInt8ToString(info.GetSize());
}


CNcbiOstream& operator << (CNcbiOstream& os, const CTarEntryInfo& info)
{
    CTime mtime(info.GetModificationTime());
    os << s_TypeAsChar(info.GetType())
       << s_ModeAsString(info.GetMode())        << ' '
       << setw(17) << s_UserGroupAsString(info) << ' '
       << setw(10) << s_SizeOrMajorMinor(info)  << ' '
       << mtime.ToLocalTime().AsString(" Y-M-D h:m:s ")
       << info.GetName();
    if (info.GetType() == CTarEntryInfo::eSymLink  ||
        info.GetType() == CTarEntryInfo::eHardLink) {
        os << " -> " << info.GetLinkName();
    }
    return os;
}



//////////////////////////////////////////////////////////////////////////////
//
// Debugging utilities
//

static string s_OSReason(int x_errno)
{
    const char* strerr = x_errno ? strerror(x_errno) : 0;
    return strerr  &&  *strerr ? string(": ") + strerr : kEmptyStr;
}


static string s_PositionAsString(const string& file, Uint8 pos, size_t recsize,
                                 const string& entryname)
{
    _ASSERT(!OFFSET_OF(pos));
    _ASSERT(!OFFSET_OF(recsize));
    _ASSERT(recsize >= BLOCK_SIZE);
    string result;
    if (!file.empty()) {
        CDirEntry temp(file);
        result = temp.GetName() + ": ";
    }
    result += "At record " + NStr::UInt8ToString(pos / recsize);
    if (recsize != BLOCK_SIZE) {
        result +=
            ", block " + NStr::UInt8ToString(BLOCK_OF(pos % recsize)) +
            " [thru #" + NStr::UInt8ToString(BLOCK_OF(pos),
                                             NStr::fWithCommas) + ']';
    }
    if (!entryname.empty()) {
        result += ", while in '" + entryname + '\'';
    }
    return result + ":\n";
}


static string s_OffsetAsString(size_t offset)
{
    char buf[20];
    _ASSERT(offset < 1000);
    _VERIFY(sprintf(buf, "%03u", (unsigned int) offset));
    return buf;
}


static bool memcchr(const char* s, char c, size_t len)
{
    for (size_t i = 0;  i < len;  ++i) {
        if (s[i] != c)
            return true;
    }
    return false;
}


static string s_Printable(const char* field, size_t maxsize, bool text)
{
    bool check = false;
    if (!text  &&  maxsize > 1  &&  !*field) {
        field++, maxsize--;
        check = true;
    }
    size_t len = s_Length(field, maxsize);
    string retval = NStr::PrintableString(string(field,
                                                 memcchr(field + len, '\0',
                                                         maxsize - len)
                                                 ? maxsize
                                                 : len));
    return check  &&  !retval.empty() ? "\\0" + retval : retval;
}


#if !defined(__GNUC__)  &&  !defined(offsetof)
#  define offsetof(T, F) ((char*) &(((T*) 0)->F) - (char*) 0)
#endif


#define _STR(s) #s

#define TAR_PRINTABLE_EX(field, text, size)                             \
    "@" + s_OffsetAsString((size_t) offsetof(SHeader, field)) +         \
    "[" _STR(field) "]:" + string(13 - sizeof(_STR(field)), ' ') +      \
    '"' + s_Printable(h->field, size, text  ||  ex) + '"'

#define TAR_PRINTABLE(field, text)                                      \
    TAR_PRINTABLE_EX(field, text, sizeof(h->field))


static string s_DumpSparseMap(const SHeader* h, const char* sparse,
                              const char* extend, bool ex = false)
{
    string dump;
    size_t offset;
    bool done = false;

    do {
        if (memcchr(sparse, '\0', 24)) {
            offset = (size_t)(sparse - (const char*) h);
            if (!dump.empty())
                dump += '\n';
            dump += '@' + s_OffsetAsString(offset);
            if (!done) {
                Uint8 off, gap;
                int ok_off = s_DecodeUint8(off, sparse,      12);
                int ok_gap = s_DecodeUint8(gap, sparse + 12, 12);
                if (ok_off & ok_gap) {
                    dump += "[gnu.map]:" + string(5, ' ');
                    if (ok_off > 0) {
                        dump += '"';
                        dump += s_Printable(sparse, 12, ex);
                        dump += "\" ";
                    } else {
                        dump += string(14, ' ');
                    }
                    sparse += 12;
                    if (ok_gap > 0) {
                        dump += '"';
                        dump += s_Printable(sparse, 12, ex);
                        dump += "\" ";
                    } else {
                        dump += string(14, ' ');
                    }
                    sparse += 12;
                    dump += " [";
                    dump += NStr::UInt8ToString(off);
                    dump += ", ";
                    dump += NStr::UInt8ToString(gap);
                    dump += ']';
                    continue;
                }
                done = true;
            }
            dump += ':' + string(14, ' ')
                + '"' + NStr::PrintableString(string(sparse, 24)) + '"';
        } else {
            done = true;
        }
        sparse += 24;
    } while (sparse < extend);
    if (!dump.empty()) {
        dump += '\n';
    }
    offset = (size_t)(extend - (const char*) h);
    dump += '@' + s_OffsetAsString(offset) + "[gnu.extend]:" + string(2, ' ')
        + '"' + NStr::PrintableString(string(extend, 1)) + '"'
        + string(*extend ? " [continued]" : " [last]");
    return dump;
}


static string s_DumpHeader(const SHeader* h, ETar_Format fmt, bool ex = false)
{
    string dump;
    Uint8 val;
    int ok;

    dump += TAR_PRINTABLE(name, true);
    dump += '\n';

    ok = s_OctalToNum(val, h->mode, sizeof(h->mode));
    dump += TAR_PRINTABLE(mode, !ok);
    if (ok  &&  val) {
        dump += " [" + s_ModeAsString((TTarMode) val) + ']';
    }
    dump += '\n';

    ok = s_DecodeUint8(val, h->uid, sizeof(h->uid));
    dump += TAR_PRINTABLE(uid, ok <= 0);
    if (ok  &&  (ok < 0  ||  val > 7)) {
        dump += " [" + NStr::UInt8ToString(val) + ']';
        if (ok < 0) {
            dump += " (base-256)";
        }
    }
    dump += '\n';
    
    ok = s_DecodeUint8(val, h->gid, sizeof(h->gid));
    dump += TAR_PRINTABLE(gid, ok <= 0);
    if (ok  &&  (ok < 0  ||  val > 7)) {
        dump += " [" + NStr::UInt8ToString(val) + ']';
        if (ok < 0) {
            dump += " (base-256)";
        }
    }
    dump += '\n';

    ok = s_DecodeUint8(val, h->size, sizeof(h->size));
    dump += TAR_PRINTABLE(size, ok <= 0);
    if (ok  &&  (ok < 0  ||  val > 7)) {
        dump += " [" + NStr::UInt8ToString(val) + ']';
        if (ok  &&  h->typeflag[0] == 'S'  &&  fmt == eTar_OldGNU) {
            dump += " w/o map!";
        }
        if (ok < 0) {
            dump += " (base-256)";
        }
    }
    dump += '\n';

    ok = s_OctalToNum(val, h->mtime, sizeof(h->mtime));
    dump += TAR_PRINTABLE(mtime, !ok);
    if (ok  &&  val) {
        CTime mtime((time_t) val);
        ok = (Uint8) mtime.GetTimeT() == val ? true : false;
        if (ok  ||  val > 7) {
            dump += (" ["
                     + (val > 7 ? NStr::UInt8ToString(val) + ", "        : "")
                     + (ok ? mtime.ToLocalTime().AsString("Y-M-D h:m:s") : "")
                     + ']');
        }
    }
    dump += '\n';

    ok = s_OctalToNum(val, h->checksum, sizeof(h->checksum));
    dump += TAR_PRINTABLE(checksum, !ok);
    dump += '\n';

    // Classify to the extent possible to help debug the problem (if any)
    dump += TAR_PRINTABLE(typeflag, true);
    ok = false;
    const char* tname = 0;
    switch (h->typeflag[0]) {
    case '\0':
    case '0':
        ok = true;
        if (!(fmt & eTar_Ustar)  &&  fmt != eTar_OldGNU) {
            size_t namelen = s_Length(h->name, sizeof(h->name));
            if (namelen  &&  h->name[namelen - 1] == '/')
                tname = "legacy regular entry (dir)";
        }
        if (!tname)
            tname = "legacy regular entry (file)";
        tname += h->typeflag[0] ? 7 : 0;
        break;
    case '\1':
    case '1':
        ok = true;
#ifdef NCBI_OS_UNIX
        tname = "legacy hard link";
#else
        tname = "legacy hard link - not FULLY supported";
#endif //NCBI_OS_UNIX
        tname += h->typeflag[0] != '\1' ? 7 : 0;
        break;
    case '\2':
    case '2':
        ok = true;
#ifdef NCBI_OS_UNIX
        tname = "legacy symbolic link";
#else
        tname = "legacy symbolic link - not FULLY supported";
#endif //NCBI_OS_UNIX
        tname += h->typeflag[0] != '\2' ? 7 : 0;
        break;
    case '3':
#ifdef NCBI_OS_UNIX
        ok = true;
#endif //NCBI_OS_UNIX
        tname = "character device";
        break;
    case '4':
#ifdef NCBI_OS_UNIX
        ok = true;
#endif //NCBI_OS_UNIX
        tname = "block device";
        break;
    case '5':
        ok = true;
        tname = "directory";
        break;
    case '6':
#ifdef NCBI_OS_UNIX
        ok = true;
#endif //NCBI_OS_UNIX
        tname = "FIFO";
        break;
    case '7':
        tname = "contiguous";
        break;
    case 'g':
        tname = "global extended header";
        break;
    case 'x':
    case 'X':
        if (fmt & eTar_Ustar) {
            ok = true;
            if (h->typeflag[0] == 'x') {
                tname = "extended (POSIX 1003.1-2001 [PAX]) header"
                    " - not FULLY supported";
            } else {
                tname = "extended (POSIX 1003.1-2001 [PAX] by Sun) header"
                    " - not FULLY supported";
            }
        } else {
            tname = "extended header";
        }
        break;
    case 'A':
        tname = "Solaris ACL";
        break;
    case 'D':
        if (fmt == eTar_OldGNU) {
            tname = "GNU extension: directory dump";
        }
        break;
    case 'E':
        tname = "Solaris extended attribute file";
        break;
    case 'I':
        // CAUTION:  Entry size shows actual file size in the filesystem but
        // no actual data blocks stored in the archive following the header!
        tname = "Inode metadata only";
        break;
    case 'K':
        if (fmt == eTar_OldGNU) {
            ok = true;
            tname = "GNU extension: long link";
        }
        break;
    case 'L':
        if (fmt == eTar_OldGNU) {
            ok = true;
            tname = "GNU extension: long name";            
        }
        break;
    case 'M':
        switch (fmt) {
        case eTar_OldGNU:
            tname = "GNU extension: multi-volume entry";
            break;
        case eTar_Star:
            tname = "STAR extension: multi-volume entry";
            break;
        default:
            break;
        }
        break;
    case 'N':
        if (fmt == eTar_OldGNU) {
            tname = "GNU extension (obsolete): long filename(s)";
        }
        break;
    case 'S':
        switch (fmt) {
        case eTar_OldGNU:
            // CAUTION:  Entry size does not include sparse entry map stored in
            // additional (non-standard) headers that may follow this header!
            tname = "GNU extension: sparse file";
            break;
        case eTar_Star:
            // Entry size already includes size of additional sparse file maps
            // that may follow this header before the actual file data.
            tname = "STAR extension: sparse file";
            break;
        default:
            break;
        }
        break;
    case 'V':
        tname = "Volume header";
        break;
    default:
        break;
    }
    if (!tname  &&  h->typeflag[0] >= 'A'  &&  h->typeflag[0] <= 'Z') {
        tname = "local vendor enhancement / user-defined extension";
    }
    dump += (" [" + string(tname ? tname : "reserved") +
             (ok
              ? "]\n"
              : " -- NOT SUPPORTED]\n"));

    dump += TAR_PRINTABLE(linkname, true);
    dump += '\n';

    switch (fmt) {
    case eTar_Legacy:  // NCBI never writes this header
        tname = "legacy (V7)";
        break;
    case eTar_OldGNU:
        if (!NStr::strncasecmp((const char*) h + BLOCK_SIZE - 4, "NCBI", 4)) {
            tname = "old GNU (NCBI)";
        } else {
            tname = "old GNU";
        }
        break;
    case eTar_Ustar:
        if (!NStr::strncasecmp((const char*) h + BLOCK_SIZE - 4, "NCBI", 4)) {
            tname = "ustar (NCBI)";
        } else {
            tname = "ustar";
        }
        break;
    case eTar_Posix:  // aka "pax"
        if (!NStr::strncasecmp((const char*) h + BLOCK_SIZE - 4, "NCBI", 4)) {
            tname = "posix (NCBI)";
        } else {
            tname = "posix";
        }
        break;
    case eTar_Star:  // NCBI never writes this header
        tname = "star";
        break;
    default:
        tname = 0;
        break;
    }
    dump += TAR_PRINTABLE(magic, true);
    if (tname) {
        dump += " [" + string(tname) + ']';
    }
    dump += '\n';

    dump += TAR_PRINTABLE(version, true);

    if (fmt != eTar_Legacy) {
        dump += '\n';

        dump += TAR_PRINTABLE(uname, true);
        dump += '\n';

        dump += TAR_PRINTABLE(gname, true);
        dump += '\n';

        ok = s_OctalToNum(val, h->devmajor, sizeof(h->devmajor));
        dump += TAR_PRINTABLE(devmajor, !ok);
        if (ok  &&  val > 7) {
            dump += " [" + NStr::UInt8ToString(val) + ']';
        }
        dump += '\n';

        ok = s_OctalToNum(val, h->devminor, sizeof(h->devminor));
        dump += TAR_PRINTABLE(devminor, !ok);
        if (ok  &&  val > 7) {
            dump += " [" + NStr::UInt8ToString(val) + ']';
        }
        dump += '\n';

        switch (fmt) {
        case eTar_Star:
            if (h->typeflag[0] == 'S') {
                dump += TAR_PRINTABLE_EX(star.prefix, true, 107);
                const char* extent = h->star.prefix + 107;
                ok = s_DecodeUint8(val, extent, 12);
                dump += "@"
                    + s_OffsetAsString((size_t)(extent - (const char*) h))
                    + "[star.extent]: \""
                    + s_Printable(extent, 12, !ok  ||  ex) + '"';
                if (ok  &&  (ok < 0  ||  val > 7)) {
                    dump += " [" + NStr::UInt8ToString(val) + ']';
                    if (ok < 0) {
                        dump += " (base-256)";
                    }
                }
            } else {
                dump += TAR_PRINTABLE(star.prefix, true);
            }
            dump += '\n';

            ok = s_OctalToNum(val, h->star.atime, sizeof(h->star.atime));
            dump += TAR_PRINTABLE(star.atime, !ok);
            if (ok  &&  val) {
                CTime atime((time_t) val);
                ok = (Uint8) atime.GetTimeT() == val ? true : false;
                if (ok  ||  val > 7) {
                    dump += (" ["
                             + (val > 7 ? NStr::UInt8ToString(val) + ", " : "")
                             + (ok
                                ? atime.ToLocalTime().AsString("Y-M-D h:m:s")
                                : "")
                             + ']');
                }
            }
            dump += '\n';

            ok = s_OctalToNum(val, h->star.ctime, sizeof(h->star.ctime));
            dump += TAR_PRINTABLE(star.ctime, !ok);
            if (ok  &&  val) {
                CTime ctime((time_t) val);
                ok = (Uint8) ctime.GetTimeT() == val ? true : false;
                if (ok  ||  val > 7) {
                    dump += (" ["
                             + (val > 7 ? NStr::UInt8ToString(val) + ", " : "")
                             + (ok
                                ? ctime.ToLocalTime().AsString("Y-M-D h:m:s")
                                : "")
                             + ']');
                }
            }
            tname = (const char*) &h->star + sizeof(h->star);
            break;

        case eTar_OldGNU:
            ok = s_OctalToNum(val, h->gnu.atime, sizeof(h->gnu.atime));
            dump += TAR_PRINTABLE(gnu.atime, !ok);
            if (ok  &&  val) {
                CTime atime((time_t) val);
                ok = (Uint8) atime.GetTimeT() == val ? true : false;
                if (ok  ||  val > 7) {
                    dump += (" ["
                             + (val > 7 ? NStr::UInt8ToString(val) + ", " : "")
                             + (ok
                                ? atime.ToLocalTime().AsString("Y-M-D h:m:s")
                                : "")
                             + ']');
                }
            }
            dump += '\n';

            ok = s_OctalToNum(val, h->gnu.ctime, sizeof(h->gnu.ctime));
            dump += TAR_PRINTABLE(gnu.ctime, !ok);
            if (ok  &&  val) {
                CTime ctime((time_t) val);
                ok = (Uint8) ctime.GetTimeT() == val ? true : false;
                if (ok  ||  val > 7) {
                    dump += (" ["
                             + (val > 7 ? NStr::UInt8ToString(val) + ", " : "")
                             + (ok
                                ? ctime.ToLocalTime().AsString("Y-M-D h:m:s")
                                : "")
                             + ']');
                }
            }

            if (h->typeflag[0] == 'S') {
                if (memcchr(h->gnu.unused, '\0', sizeof(h->gnu.unused))) {
                    dump += '\n';
                    dump += TAR_PRINTABLE(gnu.unused, true);
                }
                dump += '\n' + s_DumpSparseMap(h, h->gnu.sparse,
                                               h->gnu.extend, ex);
                if (memcchr(h->gnu.extent, '\0', sizeof(h->gnu.extent))) {
                    ok = s_DecodeUint8(val,
                                       h->gnu.extent, sizeof(h->gnu.extent));
                    dump += '\n';
                    dump += TAR_PRINTABLE(gnu.extent, ok <= 0);
                    if (ok  &&  (ok < 0  ||  val > 7)) {
                        dump += " [" + NStr::UInt8ToString(val) + ']';
                    }
                    if (ok < 0) {
                        dump += " (base-256)";
                    }
                }
                tname = (const char*) &h->gnu + sizeof(h->gnu);
            } else {
                tname = h->gnu.ctime + sizeof(h->gnu.ctime);
            }
            break;

        default:
            dump += TAR_PRINTABLE(prefix, true);
            tname = h->prefix + sizeof(h->prefix);
            break;
        }
    } else {
        tname = h->version + sizeof(h->version);
    }

    size_t n = 0;
    while (&tname[n] < (const char*) h + BLOCK_SIZE) {
        if (tname[n]) {
            size_t offset = (size_t)(&tname[n] - (const char*) h);
            size_t len = BLOCK_SIZE - offset;
            if (len & ~0xF) {  // len > 16
                len = 0x10;    // len = 16
            }
            const char* e = (const char*) memchr(&tname[n], '\0', len);
            if (e) {
                len = (size_t)(e - &tname[n]);
                ok = s_DecodeUint8(val, &tname[n], len);
            } else {
                if (len  > (offset & 0xF)) {
                    len -= (offset & 0xF);
                }
                ok = false;
            }
            _ASSERT(len);
            dump += "\n@" + s_OffsetAsString(offset) + ':' + string(14, ' ')
                + '"' + NStr::PrintableString(string(&tname[n], len)) + '"';
            if (ok) {
                CTime time((time_t) val);
                bool okaytime = (Uint8) time.GetTimeT() == val;
                if (ok < 0  ||  val > 7  ||  okaytime) {
                    dump += " [";
                    if (ok < 0  ||  val > 7) {
                        dump += NStr::UInt8ToString(val);
                    }
                    if (ok < 0) {
                        dump += "] (base-256)";
                    } else if (okaytime) {
                        if (val > 7) {
                            dump += ", ";
                        }
                        dump += time.ToLocalTime().AsString("Y-M-D h:m:s]");
                    } else {
                        dump += ']';
                    }
                }
            }
            n += len;
        } else {
            n++;
        }
    }

    return dump;
}

#undef TAR_PRINTABLE

#undef _STR


inline void s_SetStateSafe(CNcbiIos& ios, IOS_BASE::iostate state) throw()
{
    try {
        ios.setstate(state);
    } catch (IOS_BASE::failure&) {
        ;
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// CTar
//

CTar::CTar(const string& filename, size_t blocking_factor)
    : m_FileName(filename),
      m_FileStream(new CNcbiFstream),
      m_Stream(*m_FileStream),
      m_ZeroBlockCount(0),
      m_BufferSize(SIZE_OF(blocking_factor)),
      m_BufferPos(0),
      m_StreamPos(0),
      m_BufPtr(0),
      m_Buffer(0),
      m_OpenMode(eNone),
      m_Modified(false),
      m_Bad(false),
      m_Flags(fDefault)
{
    x_Init();
}


CTar::CTar(CNcbiIos& stream, size_t blocking_factor)
    : m_FileName(kEmptyStr),
      m_FileStream(0),
      m_Stream(stream),
      m_ZeroBlockCount(0),
      m_BufferSize(SIZE_OF(blocking_factor)),
      m_BufferPos(0),
      m_StreamPos(0),
      m_BufPtr(0),
      m_Buffer(0),
      m_OpenMode(eNone),
      m_Modified(false),
      m_Bad(false),
      m_Flags(fDefault)
{
    x_Init();
}


CTar::~CTar()
{
    // Close stream(s)
    x_Close(x_Flush(true/*no_throw*/));
    delete m_FileStream;
    m_FileStream = 0;

    // Delete owned masks
    for (size_t i = 0;  i < sizeof(m_Mask) / sizeof(m_Mask[0]);  ++i) {
        SetMask(0, eNoOwnership, EMaskType(i));
    }

    // Delete buffer
    delete[] m_BufPtr;
    m_BufPtr = 0;
}


#define TAR_THROW(who, errcode, message)                                \
    NCBI_THROW(CTarException, errcode,                                  \
               s_PositionAsString(who->m_FileName, who->m_StreamPos,    \
                                  who->m_BufferSize,                    \
                                  who->m_Current.GetName()) + (message))

#define TAR_THROW_EX(who, errcode, message, h, fmt)                     \
    TAR_THROW(who, errcode,                                             \
              who->m_Flags & fDumpEntryHeaders                          \
              ? string(message) + ":\n" + s_DumpHeader(h, fmt, true)    \
              : string(message))

#define TAR_POST(subcode, severity, message)                            \
    ERR_POST_X(subcode, (severity) <<                                   \
               s_PositionAsString(m_FileName, m_StreamPos, m_BufferSize,\
                                  m_Current.GetName()) + (message))


void CTar::x_Init(void)
{
    _ASSERT(!OFFSET_OF(m_BufferSize));
    size_t pagesize = (size_t) GetVirtualMemoryPageSize();
    if (!pagesize) {
        pagesize = 4096;  // reasonable default
    }
    size_t pagemask = pagesize - 1;
    // Assume that the page size is a power of 2
    _ASSERT((pagesize & pagemask) == 0);
    m_BufPtr = new char[m_BufferSize + pagemask];
    // Make m_Buffer page-aligned
    m_Buffer = m_BufPtr +
        ((((size_t) m_BufPtr + pagemask) & ~pagemask) - (size_t) m_BufPtr);
}


bool CTar::x_Flush(bool no_throw)
{
    m_Current.m_Name.erase();
    if (m_Bad  ||  !m_OpenMode) {
        return false;
    }
    if (!m_Modified  &&
        (m_FileStream  ||  !(m_Flags & fStreamPipeThrough)  ||  !m_StreamPos)){
        return false;
    }

    _ASSERT(m_BufferPos < m_BufferSize);
    if (m_BufferPos  ||  m_ZeroBlockCount < 2) {
        // Assure proper blocking factor and pad the archive as necessary
        size_t zbc = m_ZeroBlockCount;
        size_t pad = m_BufferSize - m_BufferPos;
        memset(m_Buffer + m_BufferPos, 0, pad);
        x_WriteArchive(pad, no_throw ? (const char*)(-1L) : 0);
        _ASSERT(!(m_BufferPos % m_BufferSize) // m_BufferSize if write error
                &&  !m_Bad == !m_BufferPos);
        if (!m_Bad  &&  (zbc += BLOCK_OF(pad)) < 2) {
            // Write EOT (two zero blocks), if have not padded enough already
            memset(m_Buffer, 0, m_BufferSize - pad);
            x_WriteArchive(m_BufferSize, no_throw ? (const char*)(-1L) : 0);
            _ASSERT(!(m_BufferPos % m_BufferSize)
                    &&  !m_Bad == !m_BufferPos);
            if (!m_Bad  &&  (zbc += BLOCK_OF(m_BufferSize)) < 2) {
                _ASSERT(zbc == 1  &&  m_BufferSize == BLOCK_SIZE);
                x_WriteArchive(BLOCK_SIZE, no_throw ? (const char*)(-1L) : 0);
                _ASSERT(!(m_BufferPos % m_BufferSize)
                        &&  !m_Bad == !m_BufferPos);
            }
        }
    }
    _ASSERT(!OFFSET_OF(m_BufferPos));

    if (!m_Bad  &&  m_Stream.rdbuf()->PUBSYNC() != 0) {
        m_Bad = true;
        int x_errno = errno;
        s_SetStateSafe(m_Stream, NcbiBadbit);
        if (!no_throw) {
            TAR_THROW(this, eWrite,
                      "Archive flush failed" + s_OSReason(x_errno));
        }
        TAR_POST(83, Error,
                 "Archive flush failed" + s_OSReason(x_errno));
    }
    if (!m_Bad) {
        m_Modified = false;
    }
    return true;
}


void CTar::x_Close(bool truncate)
{
    if (m_FileStream  &&  m_FileStream->is_open()) {
        m_FileStream->close();
        if (!m_Bad  &&  !(m_Flags & fTarfileNoTruncate)  &&  truncate) {
#ifdef NCBI_OS_UNIX
            ::truncate(m_FileName.c_str(), (off_t) m_StreamPos);
#endif //NCBI_OS_UNIX
#ifdef NCBI_OS_MSWIN
            TXString filename(_T_XSTRING(m_FileName));
            HANDLE handle = ::CreateFile(filename.c_str(), GENERIC_WRITE,
                                         0/*sharing*/, NULL, OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL, NULL);
            if (handle != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER filesize;
                filesize.QuadPart = m_StreamPos;
                if (::SetFilePointerEx(handle, filesize, NULL, FILE_BEGIN)) {
                    ::SetEndOfFile(handle);
                }
                ::CloseHandle(handle);
            }
#endif //NCBI_OS_MSWIN
        }
    }
    m_OpenMode  = eNone;
    m_Modified  = false;
    m_BufferPos = 0;
    m_Bad = false;
}


void CTar::x_Open(EAction action)
{
    _ASSERT(action);
    bool toend = false;
    // We can only open a named file here, and if an external stream is being
    // used as an archive, it must be explicitly repositioned by user's code
    // (outside of this class) before each archive operation.
    if (!m_FileStream) {
        if (!m_Modified) {
            // Check if Create() is followed by Append()
            if (m_OpenMode != eWO  &&  action == eAppend
                &&  (m_Flags & fStreamPipeThrough)) {
                toend = true;
            }
        } else if (action != eAppend) {
            _ASSERT(m_OpenMode != eWO);  // NB: Prev action != eCreate
            if (m_Flags & fStreamPipeThrough) {
                x_Flush();  // NB: resets m_Modified to false if successful
            }
            if (m_Modified) {
                if (!m_Bad) {
                    TAR_POST(1, Warning,
                             "Pending changes may be discarded"
                             " upon reopen of in-stream archive");
                }
                m_Modified = false;
            }
        }
        m_Current.m_Name.erase();
        if (m_Bad || (m_Stream.rdstate() & ~NcbiEofbit) || !m_Stream.rdbuf()) {
            TAR_THROW(this, eOpen,
                      "Archive IO stream is in bad state");
        } else {
            m_OpenMode = EOpenMode(int(action) & eRW);
            _ASSERT(m_OpenMode != eNone);
        }
        if (action != eAppend  &&  action != eInternal) {
            m_BufferPos = 0;
            m_StreamPos = 0;
        }
#ifdef NCBI_OS_MSWIN
        if (&m_Stream == &cin) {
            HANDLE handle = (HANDLE) _get_osfhandle(_fileno(stdin));
            if (GetFileType(handle) != FILE_TYPE_DISK) {
                m_Flags |= fSlowSkipWithRead;
            }
        }
#endif //NCBI_OS_MSWIN
    } else {
        _ASSERT(&m_Stream == m_FileStream);
        EOpenMode mode = EOpenMode(int(action) & eRW);
        _ASSERT(mode != eNone);
        if (action != eAppend  &&  action != eCreate/*mode == eWO*/) {
            x_Flush();
        } else {
            m_Current.m_Name.erase();
        }
        if (mode == eWO  ||  m_OpenMode < mode) {
            // Need to (re-)open the archive file
            if (m_OpenMode != eWO  &&  action == eAppend) {
                toend = true;
            }
            x_Close(false);  // NB: m_OpenMode = eNone; m_Modified = false
            m_StreamPos = 0;
            switch (mode) {
            case eWO:
                // WO access
                _ASSERT(action == eCreate);
                // Note that m_Modified is untouched
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::out    |
                                   IOS_BASE::binary | IOS_BASE::trunc);
                break;
            case eRO:
                // RO access
                _ASSERT(action != eCreate);
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::in     |
                                   IOS_BASE::binary);
                break;
            case eRW:
                // RW access
                _ASSERT(action != eCreate);
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::in     | IOS_BASE::out |
                                   IOS_BASE::binary);
                break;
            default:
                _TROUBLE;
                break;
            }
            if (!m_FileStream->is_open()  ||  !m_FileStream->good()) {
                int x_errno = errno;
                TAR_THROW(this, eOpen,
                          "Cannot open archive" + s_OSReason(x_errno));
            } else {
                m_OpenMode = mode;
            }
        } else {
            // No need to reopen the archive file
            _ASSERT(m_OpenMode > eWO  &&  action != eCreate);
            if (m_Bad) {
                TAR_THROW(this, eOpen,
                          "Archive is in bad state");
            }
            if (action != eAppend  &&  action != eInternal) {
                m_BufferPos = 0;
                m_StreamPos = 0;
                m_FileStream->seekg(0, IOS_BASE::beg);
            }
        }
    }
    if (toend) {
        _ASSERT(!m_Modified  &&  action == eAppend);
        // There may be an extra and unnecessary archive file scanning
        // if Append() follows Update() that caused no modifications;
        // but there is no way to distinguish this, currently :-/
        // Also, this sequence should be a real rarity in practice.
        x_ReadAndProcess(eAppend);  // to position at logical EOF
    }
    _ASSERT(!(m_Stream.rdstate() & ~NcbiEofbit));
    _ASSERT(m_Stream.rdbuf());
}


auto_ptr<CTar::TEntries> CTar::Extract(void)
{
    x_Open(eExtract);
    auto_ptr<TEntries> entries = x_ReadAndProcess(eExtract);

    // Restore attributes of "postponed" directory entries
    if (m_Flags & fPreserveAll) {
        ITERATE(TEntries, e, *entries) {
            if (e->GetType() == CTarEntryInfo::eDir) {
                x_RestoreAttrs(*e, m_Flags);
            }
        }
    }

    return entries;
}


const CTarEntryInfo* CTar::GetNextEntryInfo(void)
{
    if (m_Bad) {
        return 0;
    }
    if (m_OpenMode & eRO) {
        x_Skip(BLOCK_OF(m_Current.GetPosition(CTarEntryInfo::ePos_Data)
                        + ALIGN_SIZE(m_Current.GetSize()) - m_StreamPos));
    } else {
        x_Open(eInternal);
    }
    auto_ptr<TEntries> temp = x_ReadAndProcess(eInternal);
    _ASSERT(temp.get()  &&  temp->size() < 2);
    if (temp->size() < 1) {
        return 0;
    }
    _ASSERT(m_Current == temp->front());
    return &m_Current;
}


// Return a pointer to buffer, which is always block-aligned,
// and reflect the number of bytes available via the parameter.
// Return NULL when unable to read (either EOF or other read error).
const char* CTar::x_ReadArchive(size_t& n)
{
    _ASSERT(!OFFSET_OF(m_BufferPos)  &&  m_BufferPos < m_BufferSize);
    _ASSERT(!OFFSET_OF(m_StreamPos));
    _ASSERT(n != 0);
    size_t nread;
    if (!m_BufferPos) {
        nread = 0;
        do {
            streamsize xread;
            IOS_BASE::iostate iostate = m_Stream.rdstate();
            if (!iostate) {  // NB: good()
#ifdef NCBI_COMPILER_MIPSPRO
                try {
                    // Work around a bug in MIPSPro 7.3's streambuf::xsgetn()
                    CNcbiIstream* is = dynamic_cast<CNcbiIstream*>(&m_Stream);
                    _ASSERT(is);
                    is->read(m_Buffer                  + nread,
                             (streamsize)(m_BufferSize - nread));
                    xread = is->gcount();
                    if (xread > 0) {
                        is->clear();
                    }
                } catch (IOS_BASE::failure&) {
                    xread = m_Stream.rdstate() == NcbiEofbit ? 0 : -1;
                }
#else
                xread = m_Stream.rdbuf()
                    ->sgetn (m_Buffer                  + nread,
                             (streamsize)(m_BufferSize - nread));
#  ifdef NCBI_COMPILER_WORKSHOP
                if (xread < 0) {
                    xread = 0;  // NB: WS6 is known to return -1 :-/
                }
#  endif //NCBI_COMPILER_WORKSHOP
#endif //NCBI_COMPILER_MIPSPRO
            } else {
                xread = iostate == NcbiEofbit ? 0 : -1;
            }
            if (xread <= 0) {
                if (nread  &&  (m_Flags & fDumpEntryHeaders)) {
                    TAR_POST(57, xread ? Error : Warning,
                             "Short read (" + NStr::NumericToString(nread)
                             + (xread ? ")" : "): EOF"));
                }
                s_SetStateSafe(m_Stream, xread < 0 ? NcbiBadbit : NcbiEofbit);
                if (nread) {
                    break;
                }
                return 0;
            }
            nread += (size_t) xread;
        } while (nread < m_BufferSize);
        memset(m_Buffer + nread, 0, m_BufferSize - nread);
    } else {
        nread = m_BufferSize - m_BufferPos;
    }
    if (n > nread) {
        n = nread;
    }
    size_t xpos = m_BufferPos;
    m_BufferPos += ALIGN_SIZE(n);
    _ASSERT(xpos < m_BufferPos  &&  m_BufferPos <= m_BufferSize);
    if (m_BufferPos == m_BufferSize) {
        m_BufferPos  = 0;
        if (!m_FileStream  &&  (m_Flags & fStreamPipeThrough)) {
            size_t zbc = m_ZeroBlockCount;
            x_WriteArchive(m_BufferSize);
            m_StreamPos -= m_BufferSize;
            _ASSERT(m_BufferPos == 0);
            m_ZeroBlockCount = zbc;
        }
    }
    _ASSERT(!OFFSET_OF(m_BufferPos)  &&  m_BufferPos < m_BufferSize);
    return m_Buffer + xpos;
}


// All partial internal (i.e. in-buffer) block writes are _not_ block-aligned;
// but all external writes (i.e. when "src" is provided) _are_ block-aligned.
void CTar::x_WriteArchive(size_t nwrite, const char* src)
{
    if (!nwrite  ||  m_Bad) {
        return;
    }
    m_Modified = true;
    m_ZeroBlockCount = 0;
    do {
        _ASSERT(m_BufferPos < m_BufferSize);
        size_t avail = m_BufferSize - m_BufferPos;
        if (avail > nwrite) {
            avail = nwrite;
        }
        size_t advance = avail;
        if (src  &&  src != (const char*)(-1L)) {
            memcpy(m_Buffer + m_BufferPos, src, avail);
            size_t pad = ALIGN_SIZE(avail) - avail;
            memset(m_Buffer + m_BufferPos + avail, 0, pad);
            advance += pad;
            src += avail;
        }
        m_BufferPos += advance;
        _ASSERT(m_BufferPos <= m_BufferSize);
        if (m_BufferPos == m_BufferSize) {
            size_t nwritten = 0;
            do {
                int x_errno;
                streamsize xwritten;
                IOS_BASE::iostate iostate = m_Stream.rdstate();
                if (!(iostate & ~NcbiEofbit)) {  // NB: good() OR eof()
                    xwritten = m_Stream.rdbuf()
                        ->sputn(m_Buffer                  + nwritten,
                                (streamsize)(m_BufferSize - nwritten));
                    if (xwritten > 0) {
                        if (iostate) {
                            m_Stream.clear();
                        }
                        x_errno = 0;
                    } else {
                        x_errno = errno;
                    }
                } else {
                    xwritten = -1;
                    x_errno = 0;
                }
                if (xwritten <= 0) {
                    m_Bad = true;
                    s_SetStateSafe(m_Stream, NcbiBadbit);
                    if (src != (const char*)(-1L)) {
                        TAR_THROW(this, eWrite,
                                  "Archive write failed" +s_OSReason(x_errno));
                    }
                    TAR_POST(84, Error,
                             "Archive write failed" + s_OSReason(x_errno));
                    return;
                }
                nwritten += (size_t) xwritten;
            } while (nwritten < m_BufferSize);
            m_BufferPos = 0;
        }
        m_StreamPos += advance;
        nwrite      -= avail;
    } while (nwrite);
    _ASSERT(m_BufferPos < m_BufferSize);
}


// PAX (Portable Archive Interchange) extraction support

// Define bitmasks for extended numeric information
typedef enum {
    fPAXNone   = 0,
    fPAXSparse = 1 << 0,
    fPAXMtime  = 1 << 1,
    fPAXAtime  = 1 << 2,
    fPAXCtime  = 1 << 3,
    fPAXSize   = 1 << 4,
    fPAXUid    = 1 << 5,
    fPAXGid    = 1 << 6
} EPAXBit;
typedef unsigned int TPAXBits;  // Bitwise-OR of EPAXBit(s)


static bool s_ParsePAXInt(Uint8* valp, const char* str, size_t len, bool dot)
{
    _ASSERT(str[len] == '\n');
    if (!isdigit((unsigned char)(*str))) {
        return false;
    }
    const char* p = (const char*) memchr(str, '.', len);
    if (!p) {
        p = str + len;
    } else if (!dot) {
        return false;
    }
    Uint8 val;
    try {
        val = NStr::StringToUInt8(CTempString(str, (size_t)(p - str)));
    } catch (...) {
        return false;
    }
    if (*p == '.') {
        if (++p != str + len) {
            if (!isdigit((unsigned char)(*p)))
                return false;
            len -= (size_t)(p - str);
            try {
                (void) NStr::StringToUInt8(CTempString(p, len));
            } catch (...) {
                return false;
            }
        }
    }  // else (*p == '\n')
    *valp = val;
    return true;
}


static bool s_AllLowerCase(const char* str, size_t len)
{
    for (size_t i = 0;  i < len;  ++i) {
        unsigned char c = (unsigned char) str[i];
        if (!isalpha(c)  ||  !islower(c))
            return false;
    }
    return true;
}


CTar::EStatus CTar::x_ParsePAXData(const string& buffer)
{
    Uint8 size = 0, sparse = 0, uid = 0, gid = 0;
    Uint8 mtime = 0, atime = 0, ctime = 0, dummy;
    string path, linkpath, name, uname, gname;
    string* nodot = (string*)(-1L);
    const struct SPAXParseTable {
        const char* key;
        Uint8*      val;  // non-null for numeric, else do as string
        string*     str;  // null for check only (numeric: non-null for no '.')
        EPAXBit     bit;  // for numerics only
    } parser[] = {
        { "mtime",    &mtime, 0,         fPAXMtime },  // numeric w/dot: assign
        { "atime",    &atime, 0,         fPAXAtime },
        { "ctime",    &ctime, 0,         fPAXCtime },
        { "size",     &size,  nodot,     fPAXSize  },  // num.-no-dot: assign
        { "uid",      &uid,   nodot,     fPAXUid   },
        { "gid",      &gid,   nodot,     fPAXGid   },
      /*{ "dummy",    &dummy, nodot,     fPAXNone  },*/// num.-no-dot: ck.only
        { "path",     0,      &path,     fPAXNone  },  // string: assign
        { "linkpath", 0,      &linkpath, fPAXNone  },
        { "uname",    0,      &uname,    fPAXNone  },
        { "gname",    0,      &gname,    fPAXNone  },
        { "comment",  0,      0,         fPAXNone  },  // string: check only
        { "charset",  0,      0,         fPAXNone  },  // string: check only
        // GNU sparse extensions (NB: .size and .realsize don't go together)
        { "GNU.sparse.realsize", &sparse, nodot, fPAXSparse },
        { "GNU.sparse.major",    &dummy,  nodot, fPAXSparse },
        { "GNU.sparse.minor",    &dummy,  nodot, fPAXSparse },
        { "GNU.sparse.size",     &sparse, nodot, fPAXSparse },
        { "GNU.sparse.name",     0,       &name, fPAXNone   },
        // Other
        { "SCHILY.realsize",     &size,   nodot, fPAXSize   }
    };
    const char* str = buffer.c_str();
    TPAXBits parsed = fPAXNone;

    do {
        unsigned long len;
        size_t klen, vlen;
        char *k, *e, *v;

        errno = 0;
        if (!isdigit((unsigned char)(*str)) || !(e = (char*) strchr(str, '\n'))
            ||  !(len = strtoul(str, &k, 10))  ||  errno  ||  str + len-1 != e
            ||  (*k != ' '  &&  *k != '\t')
            ||  !(v = (char*) memchr(k, '=', (size_t)(e - k)))
            ||  !(klen = (size_t)(v++ - ++k))
            ||  memchr(k, ' ', klen)  ||  memchr(k, '\t', klen)
            ||  !(vlen = (size_t)(e - v))) {
            TAR_POST(74, Error, "Skipping malformed PAX data");
            return eFailure;
        }
        bool done = false;
        for (size_t n = 0;  n < sizeof(parser) / sizeof(parser[0]);  ++n) {
            if (strlen(parser[n].key) == klen
                &&  strncmp(parser[n].key, k, klen) == 0) {
                if (!parser[n].val) {
                    if (parser[n].str)
                        parser[n].str->assign(v, vlen);
                } else if (!s_ParsePAXInt(parser[n].val, v, vlen,
                                          !parser[n].str ? true : false)) {
                    TAR_POST(75, Warning,
                             "Ignoring bad numeric '" + string(v, vlen)
                             + "' in PAX value '" + string(k, klen) + '\'');
                } else {
                    parsed |= parser[n].bit;
                }
                done = true;
                break;
            }
        }
        if (!done  &&  s_AllLowerCase(k, klen)/*&&  !memchr(k, '.', klen)*/) {
            TAR_POST(76, Warning,
                     "Ignoring unrecognized PAX value '"
                     + string(k, klen) + '\'');
        }
        str = ++e;
    } while (*str);

    if (!name.empty()) {
        path.swap(name);
        parsed |= fPAXSparse;
    }
    if ((parsed & fPAXSparse)  &&  sparse) {
        size = sparse;
    }
    m_Current.m_Name.swap(path);
    m_Current.m_LinkName.swap(linkpath);
    m_Current.m_UserName.swap(uname);
    m_Current.m_GroupName.swap(gname);
    m_Current.m_Stat.st_mtime = (time_t) mtime;
    m_Current.m_Stat.st_atime = (time_t) atime;
    m_Current.m_Stat.st_ctime = (time_t) ctime;
    m_Current.m_Stat.st_size  = (off_t)  size;
    m_Current.m_Stat.st_uid   = (uid_t)  uid;
    m_Current.m_Stat.st_gid   = (gid_t)  gid;
    m_Current.m_Pos           = parsed;

    return eContinue;
}


static void s_Dump(const string& file, Uint8 pos, size_t recsize,
                   const string& entryname, const SHeader* h,
                   ETar_Format fmt, Uint8 datasize)
{
    EDiagSev level = SetDiagPostLevel(eDiag_Info);
    Uint8 blocks = BLOCK_OF(ALIGN_SIZE(datasize));
    ERR_POST(Info << '\n' + s_PositionAsString(file, pos, recsize, entryname)
             + s_DumpHeader(h, fmt) + '\n'
             + (blocks
                ? "Blocks of data:    " + NStr::UInt8ToString(blocks) + '\n'
                : kEmptyStr));
    SetDiagPostLevel(level);
}


static void s_DumpSparse(const string& file, Uint8 pos, size_t recsize,
                         const string& entryname, const SHeader* h,
                         const char* extend)
{
    EDiagSev level = SetDiagPostLevel(eDiag_Info);
    ERR_POST(Info << '\n' + s_PositionAsString(file, pos, recsize, entryname)
             + "GNU sparse file map header:\n"
             + s_DumpSparseMap(h, (const char*) h, extend) + '\n');
    SetDiagPostLevel(level);
}


static void s_DumpZero(const string& file, Uint8 pos, size_t recsize,
                       size_t zeroblock_count, bool eot = false)
{
    EDiagSev level = SetDiagPostLevel(eDiag_Info);
    ERR_POST(Info << '\n' + s_PositionAsString(file, pos, recsize, kEmptyStr)
             + (zeroblock_count
                ? "Zero block " + NStr::UInt8ToString((Uint8) zeroblock_count)
                : (eot ? "End-Of-Tape" : "End-Of-File")) + '\n');
    SetDiagPostLevel(level);
}


static inline bool s_IsOctal(char c)
{
    return '0' <= c  &&  c <= '7' ? true : false;
}


CTar::EStatus CTar::x_ReadEntryInfo(bool dump, bool pax)
{
    // Read block
    const TBlock* block;
    size_t nread = sizeof(block->buffer);
    _ASSERT(sizeof(*block) == BLOCK_SIZE/*== sizeof(block->buffer)*/);
    if (!(block = (const TBlock*) x_ReadArchive(nread))) {
        return eEOF;
    }
    if (nread != BLOCK_SIZE) {
        TAR_THROW(this, eRead,
                  "Unexpected EOF in archive");
    }
    const SHeader* h = &block->header;

    // Check header format
    ETar_Format fmt = eTar_Unknown;
    if (memcmp(h->magic, "ustar", 6) == 0) {
        if ((h->star.prefix[sizeof(h->star.prefix) - 1] == '\0'
             &&  s_IsOctal(h->star.atime[0])  &&  h->star.atime[0] == ' '
             &&  s_IsOctal(h->star.ctime[0])  &&  h->star.ctime[0] == ' ')
            ||  strcmp(block->buffer + BLOCK_SIZE - 4, "tar") == 0) {
            fmt = eTar_Star;
        } else {
            fmt = pax ? eTar_Posix : eTar_Ustar;
        }
    } else if (memcmp(h->magic, "ustar  ", 8) == 0) {
        // Here the magic is protruded into the adjacent version field
        fmt = eTar_OldGNU;
    } else if (memcmp(h->magic, "\0\0\0\0\0", 6) == 0) {
        fmt = eTar_Legacy;
    } else {
        TAR_THROW_EX(this, eUnsupportedTarFormat,
                     "Unrecognized format", h, fmt);
    }

    Uint8 val;
    // Get checksum from header
    if (!s_OctalToNum(val, h->checksum, sizeof(h->checksum))) {
        // We must allow all zero bytes here in case of pad/zero blocks
        for (size_t i = 0;  i < sizeof(block->buffer);  ++i) {
            if (block->buffer[i]) {
                TAR_THROW_EX(this, eUnsupportedTarFormat,
                             "Bad checksum", h, fmt);
            }
        }
        m_StreamPos += BLOCK_SIZE;  // NB: nread
        return eZeroBlock;
    }
    int checksum = int(val);

    // Compute both signed and unsigned checksums (for compatibility)
    int ssum = 0;
    unsigned int usum = 0;
    const char* p = block->buffer;
    for (size_t i = 0;  i < sizeof(block->buffer);  ++i)  {
        ssum +=                 *p;
        usum += (unsigned char)(*p);
        p++;
    }
    p = h->checksum;
    for (size_t j = 0;  j < sizeof(h->checksum);  ++j) {
        ssum -=                 *p  - ' ';
        usum -= (unsigned char)(*p) - ' ';
        p++;
    }

    // Compare checksum(s)
    if (checksum != ssum   &&  (unsigned int) checksum != usum) {
        string message = "Header checksum failed";
        if (m_Flags & fDumpEntryHeaders) {
            message += ", expected ";
            if (usum != (unsigned int) ssum) {
                message += "either ";
            }
            if (usum > 7) {
                message += "0";
            }
            message += NStr::UIntToString(usum, 0, 8);
            if (usum != (unsigned int) ssum) {
                message += " or ";
                if ((unsigned int) ssum > 7) {
                    message += "0";
                }
                message += NStr::UIntToString((unsigned int) ssum, 0, 8);
            }
        }
        TAR_THROW_EX(this, eChecksum,
                     message, h, fmt);
    }

    // Set all info members now (thus, validating the header block)

    m_Current.m_HeaderSize = BLOCK_SIZE;

    // Name
    if (m_Current.GetName().empty()) {
        if ((fmt & eTar_Ustar)  &&  h->prefix[0]
            &&  tolower((unsigned char) h->typeflag[0]) != 'x') {
            const char* prefix = fmt == eTar_Star ? h->star.prefix : h->prefix;
            size_t      pfxlen = fmt == eTar_Star
                ? s_Length(h->star.prefix, sizeof(h->star.prefix))
                : s_Length(h->prefix,      sizeof(h->prefix));
            m_Current.m_Name =
                CDirEntry::ConcatPath(string(prefix, pfxlen),
                                      string(h->name,
                                             s_Length(h->name,
                                                      sizeof(h->name))));
        } else {
            // Name prefix cannot be used
            m_Current.m_Name.assign(h->name,
                                    s_Length(h->name, sizeof(h->name)));
        }
    }

    // Mode
    if (!s_OctalToNum(val, h->mode, sizeof(h->mode))) {
        TAR_THROW_EX(this, eUnsupportedTarFormat,
                     "Bad entry mode", h, fmt);
    }
    m_Current.m_Stat.st_mode = (mode_t) val;

    // User Id
    if (!s_DecodeUint8(val, h->uid, sizeof(h->uid))) {
        TAR_THROW_EX(this, eUnsupportedTarFormat,
                     "Bad user ID", h, fmt);
    }
    m_Current.m_Stat.st_uid = (uid_t) val;

    // Group Id
    if (!s_DecodeUint8(val, h->gid, sizeof(h->gid))) {
        TAR_THROW_EX(this, eUnsupportedTarFormat,
                     "Bad group ID", h, fmt);
    }
    m_Current.m_Stat.st_gid = (gid_t) val;

    // Size
    if (!s_DecodeUint8(val, h->size, sizeof(h->size))) {
        TAR_THROW_EX(this, eUnsupportedTarFormat,
                     "Bad entry size", h, fmt);
    }
    m_Current.m_Stat.st_size = (off_t) val;
    if (m_Current.GetSize() != val) {
        ERR_POST_ONCE(Critical << "CAUTION:"
                      " ***"
                      " This run-time may not support large TAR entries"
                      " (have you built it --with-lfs?)"
                      " ***");
    }

    // Modification time
    if (!s_OctalToNum(val, h->mtime, sizeof(h->mtime))) {
        TAR_THROW_EX(this, eUnsupportedTarFormat,
                     "Bad modification time", h, fmt);
    }
    m_Current.m_Stat.st_mtime = (time_t) val;

    if (fmt == eTar_OldGNU  ||  (fmt & eTar_Ustar)) {
        // User name
        m_Current.m_UserName.assign(h->uname,
                                    s_Length(h->uname, sizeof(h->uname)));
        // Group name
        m_Current.m_GroupName.assign(h->gname,
                                     s_Length(h->gname,sizeof(h->gname)));
    }

    if (fmt == eTar_OldGNU  ||  fmt == eTar_Star) {
        // GNU times may not be valid so checks are relaxed
        const char* time;
        size_t      tlen;
        time = fmt == eTar_Star ?        h->star.atime  :        h->gnu.atime;
        tlen = fmt == eTar_Star ? sizeof(h->star.atime) : sizeof(h->gnu.atime);
        if (!s_OctalToNum(val, time, tlen)) {
            if (fmt == eTar_Star  ||  memcchr(time, '\0', tlen)) {
                TAR_THROW_EX(this, eUnsupportedTarFormat,
                             "Bad last access time", h, fmt);
            }
        } else {
            m_Current.m_Stat.st_atime = (time_t) val;
        }
        time = fmt == eTar_Star ?        h->star.ctime  :        h->gnu.ctime;
        tlen = fmt == eTar_Star ? sizeof(h->star.ctime) : sizeof(h->gnu.ctime);
        if (!s_OctalToNum(val, time, tlen)) {
            if (fmt == eTar_Star  ||  memcchr(time, '\0', tlen)) {
                TAR_THROW_EX(this, eUnsupportedTarFormat,
                             "Bad creation time", h, fmt);
            }
        } else {
            m_Current.m_Stat.st_ctime = (time_t) val;
        }
    }

    // Entry type
    switch (h->typeflag[0]) {
    case '\0':
    case '0':
        if (!(fmt & eTar_Ustar)  &&  fmt != eTar_OldGNU) {
            size_t namelen = s_Length(h->name, sizeof(h->name));
            if (namelen  &&  h->name[namelen - 1] == '/') {
                m_Current.m_Type = CTarEntryInfo::eDir;
                m_Current.m_Stat.st_size = 0;
                break;
            }
        }
        m_Current.m_Type = CTarEntryInfo::eFile;
        break;
    case '\1':
    case '\2':
    case '1':
    case '2':
        m_Current.m_Type = (h->typeflag[0] == '\2'  ||  h->typeflag[0] == '2'
                            ? CTarEntryInfo::eSymLink
                            : CTarEntryInfo::eHardLink);
        m_Current.m_LinkName.assign(h->linkname,
                                    s_Length(h->linkname,sizeof(h->linkname)));
        if (m_Current.GetSize()) {
            if (m_Current.GetType() == CTarEntryInfo::eSymLink) {
                // Mandatory to ignore
                m_Current.m_Stat.st_size = 0;
            } else if (fmt != eTar_Posix) {
                TAR_POST(77, Warning,
                         "Non-zero hard-link size ("
                         + NStr::UInt8ToString(m_Current.GetSize())
                         + ") is ignored (non-PAX)");
                m_Current.m_Stat.st_size = 0;
            } // else POSIX (re-)allowed hard links to be followed by file data
        }
        break;
    case '3':
    case '4':
        m_Current.m_Type = (h->typeflag[0] == '3'
                            ? CTarEntryInfo::eCharDev
                            : CTarEntryInfo::eBlockDev);
        if (!s_OctalToNum(val, h->devminor, sizeof(h->devminor))) {
            TAR_THROW_EX(this, eUnsupportedTarFormat,
                         "Bad device minor number", h, fmt);
        }
        usum = (unsigned int) val;  // set aside
        if (!s_OctalToNum(val, h->devmajor, sizeof(h->devmajor))) {
            TAR_THROW_EX(this, eUnsupportedTarFormat,
                         "Bad device major number", h, fmt);            
        }
#ifdef makedev
        m_Current.m_Stat.st_rdev = makedev((unsigned int) val, usum);
#else
        if (sizeof(int) >= 4  &&  sizeof(m_Current.m_Stat.st_rdev) >= 4) {
            *((unsigned int*) &m_Current.m_Stat.st_rdev) =
                (unsigned int)((val << 16) | usum);
        }
#endif //makedev
        m_Current.m_Stat.st_size = 0;
        break;
    case '5':
        m_Current.m_Type = CTarEntryInfo::eDir;
        m_Current.m_Stat.st_size = 0;
        break;
    case '6':
        m_Current.m_Type = CTarEntryInfo::ePipe;
        m_Current.m_Stat.st_size = 0;
        break;
    case '7':
        ERR_POST_ONCE(Critical << "CAUTION:"
                      " *** Contiguous TAR entries processed as regular files"
                      " ***");
        m_Current.m_Type = CTarEntryInfo::eFile;
        break;
    case 'K':
    case 'L':
    case 'S':
    case 'x':
    case 'X':
        if ((tolower((unsigned char) h->typeflag[0]) == 'x'
             &&  (fmt & eTar_Ustar))  ||
            (tolower((unsigned char) h->typeflag[0]) != 'x'
             &&  fmt == eTar_OldGNU)  ||
            (h->typeflag[0] == 'S'  &&  fmt == eTar_Star)) {
            // Assign actual type
            switch (h->typeflag[0]) {
            case 'x':
            case 'X':
                if (pax) {
                    TAR_POST(78, Warning,
                             "Duplicate PAX header encountered,"
                             " archive may be corrupt");
                }
                fmt = eTar_Posix;  // upgrade
                m_Current.m_Type = CTarEntryInfo::ePAXHeader;
                break;
            case 'S':
                m_Current.m_Type = CTarEntryInfo::eSparseFile;
                break;
            case 'K':
                m_Current.m_Type = CTarEntryInfo::eGNULongLink;
                break;
            case 'L':
                m_Current.m_Type = CTarEntryInfo::eGNULongName;
                break;
            default:
                _TROUBLE;
                break;
            }

            // Dump header
            size_t hsize = (size_t) m_Current.GetSize();
            if (dump) {
                s_Dump(m_FileName, m_StreamPos, m_BufferSize,
                       m_Current.GetName(), h, fmt, hsize);
            }
            m_StreamPos += BLOCK_SIZE;  // NB: nread

            if (m_Current.m_Type == CTarEntryInfo::eSparseFile) {
                const char* extent
                    = fmt == eTar_Star ? h->star.prefix + 107  : h->gnu.extent;
                hsize
                    = fmt != eTar_Star ? sizeof(h->gnu.extent) : 12;
                // Real file size (if present)
                if (!s_DecodeUint8(val, extent, hsize)){
                    val = (Uint8) m_Current.GetSize();
                }
                if (fmt == eTar_Star) {
                    // Archive file size includes sparse map, and already valid
                    m_Current.m_Pos = val;  // NB: real file size
                    return eContinue;
                }
                // Skip all GNU sparse file headers
                const char* extend = h->gnu.extend;
                while (*extend) {
                    _ASSERT(nread == BLOCK_SIZE);
                    if (!(block = (const TBlock*) x_ReadArchive(nread))
                        ||  nread != BLOCK_SIZE) {
                        TAR_THROW(this, eRead,
                                  "Unexpected EOF in GNU sparse file map");
                    }
                    h = &block->header;
                    extend = block->buffer + 504;
                    if (dump) {
                        s_DumpSparse(m_FileName, m_StreamPos, m_BufferSize,
                                     m_Current.GetName(), h, extend);
                    }
                    m_Current.m_HeaderSize += BLOCK_SIZE;
                    m_StreamPos            += BLOCK_SIZE;  // NB: nread
                }
                m_Current.m_Pos = val;  // NB: real file size
                return eContinue;
            }

            // Read in the extended header information
            string buffer;
            while (hsize) {
                nread = hsize;
                const char* xbuf = x_ReadArchive(nread);
                if (!xbuf) {
                    TAR_THROW(this, eRead,
                              string("Unexpected EOF in ") +
                              (m_Current.GetType()
                               == CTarEntryInfo::ePAXHeader
                               ? "PAX data" :
                               m_Current.GetType()
                               == CTarEntryInfo::eGNULongName
                               ? "long name"
                               : "long link"));
                }
                buffer.append(xbuf, nread);
                m_StreamPos += ALIGN_SIZE(nread);
                hsize       -=            nread;
            }
            if (m_Current.GetType() != CTarEntryInfo::ePAXHeader) {
                // Make sure there's no embedded '\0'(s)
                buffer.resize(strlen(buffer.c_str()));
            }
            if (dump) {
                string what(m_Current.GetType() == CTarEntryInfo::ePAXHeader
                            ? "PAX data:\n" :
                            m_Current.GetType() == CTarEntryInfo::eGNULongName
                            ? "Long name:         \""
                            : "Long link name:    \"");
                EDiagSev level = SetDiagPostLevel(eDiag_Info);
                ERR_POST(Info << Message << '\n' + what
                         + NStr::PrintableString(buffer,
                                                 m_Current.GetType()
                                                 == CTarEntryInfo::ePAXHeader
                                                 ? NStr::fNewLine_Passthru
                                                 : NStr::fNewLine_Quote) +
                         (m_Current.GetType() == CTarEntryInfo::ePAXHeader ?
                          buffer.size()  &&  buffer[buffer.size()-1] == '\n'
                          ? kEmptyStr : "\n"
                          : "\"\n"));
                SetDiagPostLevel(level);
            }
            // Reset size because the data blocks have been all read
            hsize = (size_t) m_Current.GetSize();
            m_Current.m_HeaderSize += ALIGN_SIZE(hsize);
            m_Current.m_Stat.st_size = 0;
            if (!hsize  ||  !buffer.size()) {
                TAR_POST(79, Error,
                         "Skipping " + string(hsize ? "empty" : "zero-sized")
                         + " extended header data");
                return eFailure;
            }
            switch (m_Current.GetType()) {
            case CTarEntryInfo::ePAXHeader:
                return x_ParsePAXData(buffer);
            case CTarEntryInfo::eGNULongName:
                m_Current.m_Name.swap(buffer);
                return eContinue;
            case CTarEntryInfo::eGNULongLink:
                m_Current.m_LinkName.swap(buffer);
                return eContinue;
            default:
                _TROUBLE;
                break;
            }
            return eFailure;
        }
        /*FALLTHRU*/
    case 'I':
        if (h->typeflag[0] == 'I') {
            // Safety for no data to actually follow
            m_Current.m_Stat.st_size = 0;
        }
        /*FALLTHRU*/
    default:
        m_Current.m_Type = CTarEntryInfo::eUnknown;
        break;
    }

    if (dump) {
        s_Dump(m_FileName, m_StreamPos, m_BufferSize,
               m_Current.GetName(), h, fmt, m_Current.GetSize());
    }
    m_StreamPos += BLOCK_SIZE;  // NB: nread

    return eSuccess;
}


void CTar::x_WriteEntryInfo(const string& name)
{
    // Prepare block info
    TBlock block;
    _ASSERT(sizeof(block) == BLOCK_SIZE/*== sizeof(block.buffer)*/);
    memset(block.buffer, 0, sizeof(block.buffer));
    SHeader* h = &block.header;

    CTarEntryInfo::EType type = m_Current.GetType();

    // Name(s) ('\0'-terminated if fit entirely, otherwise not)
    if (!x_PackName(h, m_Current, false)) {
        TAR_THROW(this, eNameTooLong,
                  "Name '" + m_Current.GetName() + "' too long in"
                  " entry '" + name + '\'');
    }
    if (type == CTarEntryInfo::eSymLink  &&  !x_PackName(h, m_Current, true)) {
        TAR_THROW(this, eNameTooLong,
                  "Link '" + m_Current.GetLinkName() + "' too long in"
                  " entry '" + name + '\'');
    }

    /* NOTE:  Although some sources on the Internet indicate that all but size,
     * mtime, and version numeric fields are '\0'-terminated, we could not
     * confirm that with existing tar programs, all of which we saw using
     * either '\0' or ' '-terminated values in both size and mtime fields.
     * For the ustar archive we have found a document that definitively tells
     * that _all_ numeric fields are '\0'-terminated, and that they can keep
     * up to "sizeof(field)-1" octal digits.  We follow it here.
     * However, GNU and ustar checksums seem to be different indeed, so we
     * don't use a trailing space for ustar, but for GNU only.
     */

    // Mode
    if (!s_NumToOctal(m_Current.GetMode(), h->mode, sizeof(h->mode) - 1)) {
        TAR_THROW(this, eMemory,
                  "Cannot store file mode");
    }

    // Update format as we go
    ETar_Format fmt = eTar_Ustar;
    int ok;

    // User ID
    ok = s_EncodeUint8(m_Current.GetUserId(), h->uid, sizeof(h->uid) - 1);
    if (!ok) {
        TAR_THROW(this, eMemory,
                  "Cannot store user ID");
    }
    if (ok < 0) {
        fmt = eTar_OldGNU;
    }

    // Group ID
    ok = s_EncodeUint8(m_Current.GetGroupId(), h->gid, sizeof(h->gid) - 1);
    if (!ok) {
        TAR_THROW(this, eMemory,
                  "Cannot store group ID");
    }
    if (ok < 0) {
        fmt = eTar_OldGNU;
    }

    // Size
    _ASSERT(type == CTarEntryInfo::eFile  ||  m_Current.GetSize() == 0);
    ok = s_EncodeUint8(m_Current.GetSize(), h->size, sizeof(h->size) - 1);
    if (!ok) {
        TAR_THROW(this, eMemory,
                  "Cannot store file size");
    }
    if (ok < 0) {
        fmt = eTar_OldGNU;
    }

    if (fmt != eTar_Ustar  &&  h->prefix[0]) {
        // Cannot downgrade to reflect encoding
        fmt  = eTar_Ustar;
    }

    // Modification time
    if (!s_NumToOctal(m_Current.GetModificationTime(),
                      h->mtime, sizeof(h->mtime) - 1)) {
        TAR_THROW(this, eMemory,
                  "Cannot store modification time");
    }

    bool device = false;
    // Type (GNU extension for SymLink)
    switch (type) {
    case CTarEntryInfo::eFile:
        h->typeflag[0] = '0';
        break;
    case CTarEntryInfo::eSymLink:
        h->typeflag[0] = '2';
        break;
    case CTarEntryInfo::eCharDev:
    case CTarEntryInfo::eBlockDev:
        h->typeflag[0] = type == CTarEntryInfo::eCharDev ? '3' : '4';
        if (!s_NumToOctal(m_Current.GetMajor(),
                          h->devmajor, sizeof(h->devmajor) - 1)) {
            TAR_THROW(this, eMemory,
                      "Cannot store major number");
        }
        if (!s_NumToOctal(m_Current.GetMinor(),
                          h->devminor, sizeof(h->devminor) - 1)) {
            TAR_THROW(this, eMemory,
                      "Cannot store minor number");
        }
        device = true;
        break;
    case CTarEntryInfo::eDir:
        h->typeflag[0] = '5';
        break;
    case CTarEntryInfo::ePipe:
        h->typeflag[0] = '6';
        break;
    default:
        TAR_THROW(this, eUnsupportedEntryType,
                  "Don't know how to store entry '" + name
                  + "' of type #" + NStr::IntToString(int(type))
                  + " into archive: Internal error, please report!");
    }

    // User and group
    const string& usr = m_Current.GetUserName();
    size_t len = usr.size();
    if (len < sizeof(h->uname)) {
        memcpy(h->uname, usr.c_str(), len);
    }
    const string& grp = m_Current.GetGroupName();
    len = grp.size();
    if (len < sizeof(h->gname)) {
        memcpy(h->gname, grp.c_str(), len);
    }

    // Device numbers to complete the ustar header protocol (all fields ok)
    if (!device  &&  fmt != eTar_OldGNU) {
        s_NumToOctal(0, h->devmajor, sizeof(h->devmajor) - 1);
        s_NumToOctal(0, h->devminor, sizeof(h->devminor) - 1);
    }

    if (fmt != eTar_OldGNU) {
        // Magic
        strcpy(h->magic,   "ustar");
        // Version (EXCEPTION:  not '\0' terminated)
        memcpy(h->version, "00", 2);
    } else {
        // NB: Old GNU magic protrudes into adjacent version field
        memcpy(h->magic,   "ustar  ", 8);  // 2 spaces and '\0'-terminated
    }

    // NCBI signature if allowed
    if (!(m_Flags & fStandardHeaderOnly)) {
        _ASSERT(sizeof(block.header) + 4 < sizeof(block.buffer));
        memcpy(block.buffer + sizeof(block) - 4, "NCBI", 4);
    }

    // Final step: checksumming
    if (!s_TarChecksum(&block, fmt == eTar_OldGNU ? true : false)) {
        TAR_THROW(this, eMemory,
                  "Cannot store checksum");
    }

    // Write header
    x_WriteArchive(sizeof(block.buffer), block.buffer);
    m_Current.m_HeaderSize = (streamsize)(m_StreamPos - m_Current.m_Pos);

    Checkpoint(m_Current, true/*write*/);
}


bool CTar::x_PackName(SHeader* h, const CTarEntryInfo& info, bool link)
{
    char*      storage = link ? h->linkname         : h->name;
    size_t        size = link ? sizeof(h->linkname) : sizeof(h->name);
    const string& name = link ? info.GetLinkName()  : info.GetName();
    const char*    src = name.c_str();
    size_t         len = name.size();

    if (len <= size) {
        // Name fits!
        memcpy(storage, src, len);
        return true;
    }

    if (!link  &&  len <= sizeof(h->prefix) + 1 + sizeof(h->name)) {
        // Try to split the long name into a prefix and a short name (POSIX)
        size_t i = len;
        if (i > sizeof(h->prefix)) {
            i = sizeof(h->prefix);
        }
        while (i > 0  &&  src[--i] != '/');
        if (i  &&  len - i <= sizeof(h->name) + 1) {
            memcpy(h->prefix, src,         i);
            memcpy(h->name,   src + i + 1, len - i - 1);
            return true;
        }
    }

    // Still, store the initial part in the original header
    memcpy(storage, src, size);

    // Prepare extended block header with the long name info (old GNU style)
    _ASSERT(!OFFSET_OF(m_BufferPos)  &&  m_BufferPos < m_BufferSize);
    TBlock* block = (TBlock*)(m_Buffer + m_BufferPos);
    memset(block->buffer, 0, sizeof(block->buffer));
    h = &block->header;

    // See above for comments about header filling
    len++;  // write terminating '\0' as it can always be made to fit in
    strcpy(h->name, "././@LongLink");
    s_NumToOctal(0,         h->mode,  sizeof(h->mode) - 1);
    s_NumToOctal(0,         h->uid,   sizeof(h->uid)  - 1);
    s_NumToOctal(0,         h->gid,   sizeof(h->gid)  - 1);
    if (!s_EncodeUint8(len, h->size,  sizeof(h->size) - 1)) {
        return false;
    }
    s_NumToOctal(0,         h->mtime, sizeof(h->mtime)- 1);
    h->typeflag[0] = link ? 'K' : 'L';

    // Old GNU magic protrudes into adjacent version field
    memcpy(h->magic, "ustar  ", 8);  // 2 spaces and '\0'-terminated

    s_TarChecksum(block, true);

    // Write the header
    x_WriteArchive(sizeof(block->buffer));

    // Store the full name in the extended block
    AutoPtr< char, ArrayDeleter<char> > buf_ptr(new char[len]);
    storage = buf_ptr.get();
    memcpy(storage, src, len);

    // Write the extended block (will be aligned as necessary)
    x_WriteArchive(len, storage);

    return true;
}


void CTar::x_Backspace(EAction action)
{
    _ASSERT(SIZE_OF(m_ZeroBlockCount) <= m_StreamPos);
    _ASSERT(!OFFSET_OF(m_StreamPos));
    m_Current.m_Name.erase();
    if (!m_ZeroBlockCount) {
        return;
    }

    size_t gap = SIZE_OF(m_ZeroBlockCount);
    if (!m_FileStream) {
        if (gap > m_BufferPos) {
            if (action == eAppend  ||  action == eUpdate) {
                TAR_POST(4, Warning,
                         "In-stream update may result in gapped tar archive");
            }
            gap = m_BufferPos;
            m_ZeroBlockCount -= BLOCK_OF(gap);
        }
        m_BufferPos -= gap;
        m_StreamPos -= gap;
        return;
    }

    // Tarfile here
    m_StreamPos -= gap;
    CT_POS_TYPE rec  = (CT_OFF_TYPE)(m_StreamPos / m_BufferSize);
    size_t      off  = (size_t)     (m_StreamPos % m_BufferSize);
    size_t     temp  = BLOCK_SIZE;
    if (m_BufferPos == 0) {
        m_BufferPos += m_BufferSize;
    }
    if (gap > m_BufferPos) {
        m_BufferPos  = 0;
        // Re-fetch the entire record
        if (!m_FileStream->seekg(rec * m_BufferSize)
            // NB: successful positioning guarantees the stream was !fail(),
            // which means it might have only been either good() or eof()
            ||  (m_FileStream->clear(), !x_ReadArchive(temp))
            ||  temp != BLOCK_SIZE) {
            TAR_POST(65, Error,
                     "Archive backspace error in record reget");
            s_SetStateSafe(m_Stream, NcbiBadbit);
            temp = 0;
        }
        m_BufferPos  = off;
    } else {
        m_BufferPos -= gap;
    }
    _ASSERT(!OFFSET_OF(m_BufferPos)  &&  m_BufferPos < m_BufferSize);

    // Always set put position here
    if (!m_FileStream->seekp(rec * m_BufferSize)  &&  temp) {
        TAR_POST(80, Error,
                 "Archive backspace error in record reset");
        s_SetStateSafe(m_Stream, NcbiBadbit);
    }
    m_ZeroBlockCount = 0;
}


static bool s_MatchPattern(const list<CTempString>& elems,
                           const CMask*             mask,
                           NStr::ECase              acase)
{
    _ASSERT(mask  &&  !elems.empty());
    if (elems.size() == 1) {
        return mask->Match(elems.front(), acase);
    }

    string temp;
    REVERSE_ITERATE(list<CTempString>, it, elems) {
        temp = temp.empty() ? string(*it) : string(*it) + '/' + temp;
        if (mask->Match(temp, acase)) {
            return true;
        }
    }
    return false;
}


auto_ptr<CTar::TEntries> CTar::x_ReadAndProcess(EAction action)
{
    auto_ptr<TEntries> done(new TEntries);
    _ASSERT(!OFFSET_OF(m_StreamPos));
    Uint8 pos = m_StreamPos;
    CTarEntryInfo xinfo;

    m_ZeroBlockCount = 0;
    for (;;) {
        // Next block is supposed to be a header
        m_Current = CTarEntryInfo(pos);
        m_Current.m_Name = xinfo.GetName();
        EStatus status = x_ReadEntryInfo
            (action == eTest  &&  (m_Flags & fDumpEntryHeaders),
             xinfo.GetType() == CTarEntryInfo::ePAXHeader);
        switch (status) {
        case eFailure:
        case eSuccess:
        case eContinue:
            if (m_ZeroBlockCount  &&  !(m_Flags & fIgnoreZeroBlocks)) {
                Uint8 save_pos = m_StreamPos;
                m_StreamPos   -= xinfo.m_HeaderSize + m_Current.m_HeaderSize;
                m_StreamPos   -= SIZE_OF(m_ZeroBlockCount);
                TAR_POST(5, Error, "Interspersing zero block ignored");
                m_StreamPos    = save_pos;
            }
            break;

        case eZeroBlock:
            m_ZeroBlockCount++;
            if (action == eTest  &&  (m_Flags & fDumpEntryHeaders)) {
                s_DumpZero(m_FileName, m_StreamPos - BLOCK_SIZE, m_BufferSize,
                           m_ZeroBlockCount);
            }
            if ((m_Flags & fIgnoreZeroBlocks)  ||  m_ZeroBlockCount < 2) {
                if (xinfo.GetType() == CTarEntryInfo::eUnknown) {
                    // Not yet reading an entry -- advance
                    pos += BLOCK_SIZE;
                }
                continue;
            }
            // Two zero blocks -> eEOF
            /*FALLTHRU*/

        case eEOF:
            if (action == eTest  &&  (m_Flags & fDumpEntryHeaders)) {
                s_DumpZero(m_FileName, m_StreamPos, m_BufferSize, 0,
                           status != eEOF ? true : false);
            }
            if (xinfo.GetType() != CTarEntryInfo::eUnknown) {
                TAR_POST(6, Error, "Orphaned extended information ignored");
            } else if (m_ZeroBlockCount < 2  &&  action != eAppend) {
                if (!m_StreamPos) {
                    TAR_THROW(this, eRead,
                              "Unexpected EOF in archive");
                }
                TAR_POST(58, Warning, m_ZeroBlockCount
                         ? "Incomplete EOT in archive"
                         : "Missing EOT in archive");
            }
            x_Backspace(action);
            return done;
        }
        m_ZeroBlockCount = 0;

        //
        // Process entry
        //
        if (status == eContinue) {
            // Extended header information has just been read in
            xinfo.m_HeaderSize += m_Current.m_HeaderSize;

            switch (m_Current.GetType()) {
            case CTarEntryInfo::ePAXHeader:
                xinfo.m_Pos = m_Current.m_Pos;  // NB: parse mask, not pos!
                m_Current.m_Pos = pos;
                if (xinfo.GetType() != CTarEntryInfo::eUnknown) {
                    TAR_POST(7, Error, "Unused extended header replaced");
                }
                xinfo.m_Type = CTarEntryInfo::ePAXHeader;
                xinfo.m_Name.swap(m_Current.m_Name);
                xinfo.m_LinkName.swap(m_Current.m_LinkName);
                xinfo.m_UserName.swap(m_Current.m_UserName);
                xinfo.m_GroupName.swap(m_Current.m_GroupName);
                xinfo.m_Stat = m_Current.m_Stat;
                continue;

            case CTarEntryInfo::eGNULongName:
                if (xinfo.GetType() == CTarEntryInfo::ePAXHeader
                    ||  !xinfo.GetName().empty()) {
                    TAR_POST(8, Error,
                             "Unused long name '" + xinfo.GetName()
                             + "' replaced");
                }
                // Latch next long name here then just skip
                xinfo.m_Type = CTarEntryInfo::eGNULongName;
                xinfo.m_Name.swap(m_Current.m_Name);
                continue;

            case CTarEntryInfo::eGNULongLink:
                if (xinfo.GetType() == CTarEntryInfo::ePAXHeader
                    ||  !xinfo.GetLinkName().empty()) {
                    TAR_POST(9, Error,
                             "Unused long link '" + xinfo.GetLinkName()
                             + "' replaced");
                }
                // Latch next long link here then just skip
                xinfo.m_Type = CTarEntryInfo::eGNULongLink;
                xinfo.m_LinkName.swap(m_Current.m_LinkName);
                continue;

            case CTarEntryInfo::eSparseFile:
                xinfo.m_Type = CTarEntryInfo::eSparseFile;
                xinfo.m_Stat.st_size = (off_t) m_Current.m_Pos;
                m_Current.m_Pos = pos;  // NB: real file size was stored
                status = eSuccess;
                break;

            default:
                NCBI_THROW(CCoreException, eCore, "Internal error");
                /*NOTREACHED*/
                break;
            }
        }

        // Fixup current 'info' with extended information obtained previously
        m_Current.m_HeaderSize += xinfo.m_HeaderSize;
        xinfo.m_HeaderSize = 0;
        if (!xinfo.GetName().empty()) {
            xinfo.m_Name.swap(m_Current.m_Name);
            xinfo.m_Name.erase();
        }
        if (!xinfo.GetLinkName().empty()) {
            xinfo.m_LinkName.swap(m_Current.m_LinkName);
            xinfo.m_LinkName.erase();
        }
        if (xinfo.GetType() == CTarEntryInfo::ePAXHeader) {
            TPAXBits parsed = (TPAXBits) xinfo.m_Pos;
            if (!xinfo.GetUserName().empty()) {
                xinfo.m_UserName.swap(m_Current.m_UserName);
                xinfo.m_UserName.erase();
            }
            if (!xinfo.GetGroupName().empty()) {
                xinfo.m_GroupName.swap(m_Current.m_GroupName);
                xinfo.m_GroupName.erase();
            }
            if (parsed & fPAXMtime) {
                m_Current.m_Stat.st_mtime = xinfo.m_Stat.st_mtime;
            }
            if (parsed & fPAXAtime) {
                m_Current.m_Stat.st_atime = xinfo.m_Stat.st_atime;
            }
            if (parsed & fPAXCtime) {
                m_Current.m_Stat.st_ctime = xinfo.m_Stat.st_ctime;
            }
            if (parsed & fPAXSparse) {
                // GTar does not store "size" correctly in PAX form in this
                // case: there is a real size instead of the archived size.
                xinfo.m_Type = CTarEntryInfo::eSparseFile;
            } else if (parsed & fPAXSize) {
                m_Current.m_Stat.st_size = xinfo.m_Stat.st_size;
            }
            if (parsed & fPAXUid) {
                m_Current.m_Stat.st_uid = xinfo.m_Stat.st_uid;
            }
            if (parsed & fPAXGid) {
                m_Current.m_Stat.st_gid = xinfo.m_Stat.st_gid;
            }
        }
        Uint8 size = m_Current.GetSize();  // NB: archive size of the entry
        if (xinfo.GetType() == CTarEntryInfo::eSparseFile) {
            m_Current.m_Stat.st_size = xinfo.m_Stat.st_size;  // display size
            m_Current.m_Type = CTarEntryInfo::eUnknown;
            // To unconditionally disable extractions of these totally
            // clobbered (who knows how) files, uncomment the following line:
            // m_Current.m_Type = CTarEntryInfo::eSparseFile;
        }
        xinfo.m_Type = CTarEntryInfo::eUnknown;
        _ASSERT(status == eFailure  ||  status == eSuccess);

        // User callback
        if (!Checkpoint(m_Current, false/*read*/)) {
            status = eFailure;
        }
        // Last sanity check
        if (status != eFailure  &&  m_Current.GetName().empty()) {
            TAR_THROW(this, eBadName,
                      "Empty entry name in archive");
        }

        // Match file name with the set of masks
        bool match = (status == eFailure ? false
                      : m_Mask[eExtractMask].mask  &&  (action == eList     ||
                                                        action == eExtract  ||
                                                        action == eInternal)
                      ? m_Mask[eExtractMask].mask->Match(m_Current.GetName(),
                                                         m_Mask[eExtractMask]
                                                         .acase)
                      : true);
        if (match  &&  m_Mask[eExcludeMask].mask  &&  action != eTest) {
            list<CTempString> elems;
            _ASSERT(!m_Current.GetName().empty());
            NStr::Split(m_Current.GetName(), "/", elems);
            match = !s_MatchPattern(elems,
                                    m_Mask[eExcludeMask].mask,
                                    m_Mask[eExcludeMask].acase);
        }

        // NB: match is 'false' when processing a failing entry
        if ((match  &&  action == eInternal)
            ||  x_ProcessEntry(match  &&  action == eExtract, size, done.get())
            ||  (match  &&  (action == eList  ||  action == eUpdate))) {
            _ASSERT(status == eSuccess);
            done->push_back(m_Current);
            if (action == eInternal) {
                break;
            }
        }

        _ASSERT(!OFFSET_OF(m_StreamPos));
        pos = m_StreamPos;
    }

    return done;
}


struct CTmpDirEntryDeleter {
    static void Delete(CDirEntry* entry) { entry->Remove(); delete entry; }
};


bool CTar::x_ProcessEntry(bool extract, Uint8 size,
                          const CTar::TEntries* entries)
{
    CTarEntryInfo::EType type = m_Current.GetType();

    if (extract) {
        // Destination for extraction
        auto_ptr<CDirEntry> dst
            (CDirEntry::CreateObject(CDirEntry::EType(type),
                                     CDirEntry::NormalizePath
                                     (CDirEntry::ConcatPath
                                      (m_BaseDir, m_Current.GetName()))));
        // Source for extraction
        auto_ptr<CDirEntry> src;
        // Direntry pending removal
        AutoPtr<CDirEntry, CTmpDirEntryDeleter> pending;

        // Dereference sym.link if requested
        if (type != CTarEntryInfo::eSymLink  &&
            type != CTarEntryInfo::eHardLink  &&  (m_Flags & fFollowLinks)) {
            dst->DereferenceLink();
        }

        // Actual type in file system (if exists)
        CDirEntry::EType dst_type = dst->GetType();

        // Look if extraction is allowed (when the destination exists)
        if (dst_type != CDirEntry::eUnknown) {
            bool found = false;  // check if ours (prev. revision extracted)
            if (entries) {
                ITERATE(TEntries, e, *entries) {
                    if (e->GetName() == m_Current.GetName()  &&
                        e->GetType() == m_Current.GetType()) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                // Can overwrite it?
                if (!(m_Flags & fOverwrite)) {
                    // File already exists, and cannot be changed
                    extract = false;
                } else { // The fOverwrite flag is set
                    // Can update?
                    if ((m_Flags & fUpdate) == fUpdate
                        &&  type != CTarEntryInfo::eDir) {
                        // Update directories always, because the archive can
                        // contain other subtree of this existing directory.
                        time_t dst_time;
                        // Make sure that dst is not older than the entry
                        if (dst->GetTimeT(&dst_time)
                            &&  m_Current.GetModificationTime() <= dst_time) {
                            extract = false;
                        }
                    }
                    // Have equal types?
                    if (extract  &&  (m_Flags & fEqualTypes)) {
                        if (type == CTarEntryInfo::eHardLink) {
                            src.reset
                                (new CDirEntry(CDirEntry::NormalizePath
                                               (CDirEntry::ConcatPath
                                                (m_BaseDir,
                                                 m_Current.GetLinkName()))));
                            if (dst_type != src->GetType()) {
                                extract = false;
                            }
                        } else if (dst_type != CDirEntry::EType(type)) {
                            extract = false;
                        }
                    }
                }
            }
            if (extract) {
                if (!found  &&  (m_Flags & fBackup) == fBackup) {
                    // Need to backup the existing destination?
                    CDirEntry tmp(*dst);
                    if (!tmp.Backup(kEmptyStr, CDirEntry::eBackup_Rename)) {
                        TAR_THROW(this, eBackup,
                                  "Failed to backup '" + dst->GetPath() +'\'');
                    }
                } else if (type != CTarEntryInfo::eDir) {
                    // Do removal safely until extraction is confirmed
                    CDirEntry tmp(*dst);
                    pending.reset(new CDirEntry(CDirEntry::GetTmpNameEx
                                                (dst->GetDir(), "xNCBItArX")));
                    errno = 0;
                    if (!tmp.Rename(pending->GetPath())  ||  dst->Exists()) {
                        // Security concern:  do not attempt data extraction
                        // into special files etc., which can harm the system.
                        int x_errno = errno ? errno : EEXIST;
                        TAR_THROW(this, eWrite,
                                  "Cannot extract '" + dst->GetPath() + '\''
                                  + s_OSReason(x_errno));
                    }
                }
            }
        }
        if (extract) {
#ifdef NCBI_OS_UNIX
            mode_t u = umask(0);
            umask(u & 077);
            try {
#endif //NCBI_OS_UNIX
                extract = x_ExtractEntry(size, dst.get(), src.get());
#ifdef NCBI_OS_UNIX
            } catch (...) {
                umask(u);
                throw;
            }
            umask(u);
#endif //NCBI_OS_UNIX
            if (!extract  &&  pending.get()/*NB: not dir*/) {
                dst->Remove();
                // Undo delete
                CDirEntry tmp(*pending);
                if (!tmp.Rename(dst->GetPath())) {
                    int x_errno = errno;
                    TAR_THROW(this, eWrite,
                              "Cannot restore '" + dst->GetPath()
                              + "' back in place" + s_OSReason(x_errno));
                }
            }
        }
    }

    x_Skip(BLOCK_OF(ALIGN_SIZE(size)));

    return extract;
}


void CTar::x_Skip(Uint8 blocks)
{
    _ASSERT(!OFFSET_OF(m_StreamPos));
    while (blocks) {
#ifndef NCBI_COMPILER_WORKSHOP
        // RogueWave RTL is buggy in seeking pipes -- it clobbers
        // (discards) streambuf data instead of leaving it alone..
        if (!(m_Flags & (fSlowSkipWithRead | fStreamPipeThrough))
            &&  m_BufferPos == 0  &&  blocks >= BLOCK_OF(m_BufferSize)) {
            CT_OFF_TYPE fskip =
                (CT_OFF_TYPE)(blocks / BLOCK_OF(m_BufferSize) * m_BufferSize);
            _ASSERT(ALIGN_SIZE(fskip) == fskip);
            if (m_Stream.rdbuf()->PUBSEEKOFF(fskip, IOS_BASE::cur)
                != (CT_POS_TYPE)((CT_OFF_TYPE)(-1))) {
                blocks      -= BLOCK_OF(fskip);
                m_StreamPos +=          fskip;
                continue;
            }
            if (m_FileStream) {
                TAR_POST(2, Warning,
                         "Cannot fast skip in file archive,"
                         " reverting to slow skip");
            }
            m_Flags |= fSlowSkipWithRead;
        }
#endif //NCBI_COMPILER_WORKSHOP
        size_t nskip = (blocks < BLOCK_OF(m_BufferSize)
                        ? (size_t) SIZE_OF(blocks)
                        : m_BufferSize);
        _ASSERT(ALIGN_SIZE(nskip) == nskip);
        if (!x_ReadArchive(nskip)) {
            TAR_THROW(this, eRead,
                      "Archive skip failed (EOF)");
        }
        nskip        = ALIGN_SIZE(nskip);
        blocks      -= BLOCK_OF(nskip);
        m_StreamPos +=          nskip;
    }
    _ASSERT(!OFFSET_OF(m_StreamPos));
}


bool CTar::x_ExtractEntry(Uint8& size,
                          const CDirEntry* dst, const CDirEntry* src)
{
    CTarEntryInfo::EType type = m_Current.GetType();
    auto_ptr<CDirEntry> src_ptr;  // deleter
    bool result = true;  // assume best

    if (type == CTarEntryInfo::eUnknown  &&  !(m_Flags & fSkipUnsupported)) {
        // Conform to POSIX-mandated behavior to extract as files
        type = CTarEntryInfo::eFile;
    }
    switch (type) {
    case CTarEntryInfo::eHardLink:
    case CTarEntryInfo::eFile:
        {{
            // Create base directory
            CDir dir(dst->GetDir());
            if (!dir.CreatePath()) {
                int x_errno = errno;
                TAR_THROW(this, eCreate,
                          "Cannot create directory '" + dir.GetPath() + '\''
                          + s_OSReason(x_errno));
            }

            if (type != CTarEntryInfo::eFile) {
                if (!src) {
                    src_ptr.reset(new CDirEntry(CDirEntry::NormalizePath
                                                (CDirEntry::ConcatPath
                                                 (m_BaseDir,
                                                  m_Current.GetLinkName()))));
                    src = src_ptr.get();
                }
                if (src->GetType() == CDirEntry::eUnknown  &&  size) {
                    // Looks like a dangling hard link but luckily we have
                    // the actual file data (POSIX extension) so use it here.
                    type = CTarEntryInfo::eFile;
                }
            }

            if (type == CTarEntryInfo::eFile) {
                // Create the file
                // FIXME:  Switch to CFileIO eventually to bypass
                // ofstream obscurity w.r.t. errors, extra buffering etc.
                // FIXME:  Should the file name match an existing device (or
                // a terminal, in particular), things may go really ugly here.
                CNcbiOfstream ofs(dst->GetPath().c_str(),
                                  IOS_BASE::trunc |
                                  IOS_BASE::out   |
                                  IOS_BASE::binary);
                if (!ofs) {
                    int x_errno = errno;
                    TAR_THROW(this, eCreate,
                              "Cannot create file '" + dst->GetPath() + '\''
                              + s_OSReason(x_errno));
                }
                if (m_Flags & fPreserveMode) {  // NB: secure
                    x_RestoreAttrs(m_Current, fPreserveMode,
                                   dst, fTarURead | fTarUWrite);
                }

                while (size) {
                    // Read from the archive
                    size_t nread = size < m_BufferSize
                        ? (size_t) size : m_BufferSize;
                    const char* xbuf = x_ReadArchive(nread);
                    if (!xbuf) {
                        TAR_THROW(this, eRead,
                                  "Unexpected EOF in archive");
                    }
                    // Write file to disk
                    bool okay;
                    if (ofs.good()) {
                        try {
                            okay = (ofs.write(xbuf, (streamsize) nread)
                                    ? true : false);
                        } catch (IOS_BASE::failure&) {
                            okay = false;
                        }
                    } else {
                        okay = false;
                    }
                    if (!okay) {
                        int x_errno = errno;
                        TAR_THROW(this, eWrite,
                                  "Error writing file '" + dst->GetPath()+ '\''
                                  + s_OSReason(x_errno));
                    }
                    m_StreamPos += ALIGN_SIZE(nread);
                    size        -=            nread;
                }

                _ASSERT(ofs.good());
                ofs.close();
            } else {
                _ASSERT(src);
#ifdef NCBI_OS_UNIX
                if (link(src->GetPath().c_str(), dst->GetPath().c_str()) == 0){
                    if (m_Flags & fPreserveAll) {
                        x_RestoreAttrs(m_Current, m_Flags, dst);
                    }
                    break;
                }
                int x_errno = errno;
                TAR_POST(10, Warning,
                         "Cannot hard-link '" + src->GetPath()
                         + "' and '" + dst->GetPath() + '\''
                         + s_OSReason(x_errno) + ", trying to copy");
#endif //NCBI_OS_UNIX
                if (!src->Copy(dst->GetPath(),
                               CDirEntry::fCF_Overwrite |
                               CDirEntry::fCF_PreserveAll)) {
                    TAR_POST(11, Error,
                             "Cannot hard-link '" + src->GetPath()
                             + "' and '" + dst->GetPath() + "\' via copy");
                    result = false;
                    break;
                }
            }

            // Restore attributes
            if (m_Flags & fPreserveAll) {
                x_RestoreAttrs(m_Current, m_Flags, dst);
            }
        }}
        break;

    case CTarEntryInfo::eDir:
        if (!CDir(dst->GetPath()).CreatePath()) {
            int x_errno = errno;
            TAR_THROW(this, eCreate,
                      "Cannot create directory '" + dst->GetPath() + '\''
                      + s_OSReason(x_errno));
        }
        // Attributes for a directory must be set only when all
        // its files have been already extracted.
        _ASSERT(size == 0);
        break;

    case CTarEntryInfo::eSymLink:
        {{
            CSymLink symlink(dst->GetPath());
            if (!symlink.Create(m_Current.GetLinkName())) {
                int x_errno = errno;
                TAR_POST(12, Error,
                         "Cannot create symlink '" + dst->GetPath()
                         + "' -> '" + m_Current.GetLinkName() + '\''
                         + s_OSReason(x_errno));
                result = false;
            }
            _ASSERT(size == 0);
        }}
        break;

    case CTarEntryInfo::ePipe:
        {{
            _ASSERT(size == 0);
#ifdef NCBI_OS_UNIX
            mode_t u = umask(0);
            if (mkfifo(dst->GetPath().c_str(), m_Current.GetMode()) != 0) {
                result = false;
            }
            umask(u);  // NB: always succeeds and does not change errno
            if (result) {
                break;
            }
            string reason = s_OSReason(errno);
#else
            string reason = ": Feature not supported by host OS";
            result = false;
#endif //NCBI_OS_UNIX
            TAR_POST(81, Error,
                     "Cannot create FIFO '" + dst->GetPath() + '\'' + reason);
        }}
        break;

    case CTarEntryInfo::eCharDev:
    case CTarEntryInfo::eBlockDev:
        {{
            _ASSERT(size == 0);
#ifdef NCBI_OS_UNIX
            mode_t u = umask(0);
            mode_t m = (m_Current.GetMode() |
                        (type == CTarEntryInfo::eCharDev ? S_IFCHR : S_IFBLK));
            if (mknod(dst->GetPath().c_str(), m, m_Current.m_Stat.st_rdev)) {
                result = false;

            }
            umask(u);  // NB: always succeeds and does not clobber errno
            if (result) {
                break;
            }
            string reason = s_OSReason(errno);
#else
            string reason = ": Feature not supported by host OS";
            result = false;
#endif //NCBI_OS_UNIX
            TAR_POST(82, Error,
                     "Cannot create " +
                     string(type == CTarEntryInfo::eCharDev
                            ? "character" : "block")
                     + " special '" + dst->GetPath() + '\'' + reason);
        }}
        break;

    case CTarEntryInfo::ePAXHeader:
    case CTarEntryInfo::eGNULongName:
    case CTarEntryInfo::eGNULongLink:
        // Extended headers should have already been processed and not be here
        _TROUBLE;
        /*FALLTHRU*/

    case CTarEntryInfo::eSparseFile:
        /*FALLTHRU*/

    default:
        TAR_POST(13, Warning,
                 "Skipping unsupported entry '" + m_Current.GetName()
                 + "' of type #" + NStr::IntToString(int(type)));
        result = false;
        break;
    }

    return result;
}


void CTar::x_RestoreAttrs(const CTarEntryInfo& info,
                          TFlags               what,
                          const CDirEntry*     path,
                          TTarMode             perm) const
{
    auto_ptr<CDirEntry> path_ptr;  // deleter
    if (!path) {
        path_ptr.reset(CDirEntry::CreateObject
                       (CDirEntry::EType(info.GetType()),
                        CDirEntry::NormalizePath
                        (CDirEntry::ConcatPath
                         (m_BaseDir, info.GetName()))));
        path = path_ptr.get();
    }

    // Date/time.
    // Set the time before permissions because on some platforms
    // this setting can also affect file permissions.
    if (what & fPreserveTime) {
        time_t modification(info.GetModificationTime());
        time_t last_access(info.GetLastAccessTime());
        time_t creation(info.GetCreationTime());
        if (!path->SetTimeT(&modification, &last_access, &creation)) {
            int x_errno = errno;
            TAR_THROW(this, eRestoreAttrs,
                      "Cannot restore date/time of '" + path->GetPath() + '\''
                      + s_OSReason(x_errno));
        }
    }

    // Owner.
    // This must precede changing permissions because on some
    // systems chown() clears the set[ug]id bits for non-superusers
    // thus resulting in incorrect permissions.
    if (what & fPreserveOwner) {
        unsigned int uid, gid;
        // 2-tier trial:  first using the names, then using numeric IDs.
        // Note that it is often impossible to restore the original owner
        // without the super-user rights so no error checking is done here.
        if (!path->SetOwner(info.GetUserName(),
                            info.GetGroupName(),
                            eIgnoreLinks, &uid, &gid)  &&
            !path->SetOwner(kEmptyStr, info.GetGroupName(), eIgnoreLinks)) {
            if (uid != info.GetUserId()  ||  gid != info.GetGroupId()) {
                string user = NStr::UIntToString(info.GetUserId());
                string group = NStr::UIntToString(info.GetGroupId());
                if (!path->SetOwner(user, group, eIgnoreLinks)) {
                    path->SetOwner(kEmptyStr, group, eIgnoreLinks);
                }
            }
        }
    }

    // Mode.
    // Set them last.
    if ((what & fPreserveMode)
        &&  info.GetType() != CTarEntryInfo::ePipe
        &&  info.GetType() != CTarEntryInfo::eCharDev
        &&  info.GetType() != CTarEntryInfo::eBlockDev) {
        bool failed = false;
#ifdef NCBI_OS_UNIX
        // We cannot change permissions for sym.links because lchmod()
        // is not portable and is not implemented on majority of platforms.
        if (info.GetType() != CTarEntryInfo::eSymLink) {
            // Use raw mode here to restore most of the bits
            mode_t mode = s_TarToMode(perm ? perm : info.m_Stat.st_mode);
            if (chmod(path->GetPath().c_str(), mode) != 0) {
                // May fail due to setuid/setgid bits -- strip'em and try again
                if (mode &   (S_ISUID | S_ISGID)) {
                    mode &= ~(S_ISUID | S_ISGID);
                    failed = chmod(path->GetPath().c_str(), mode) != 0;
                } else {
                    failed = true;
                }
            }
        }
#else
        CDirEntry::TMode user, group, other;
        CDirEntry::TSpecialModeBits special_bits;
        if (perm) {
            s_TarToMode(perm, &user, &group, &other, &special_bits);
        } else {
            info.GetMode(&user, &group, &other, &special_bits);
        }
        failed = !path->SetMode(user, group, other, special_bits);
#endif //NCBI_OS_UNIX
        if (failed) {
            int x_errno = errno;
            TAR_THROW(this, eRestoreAttrs,
                      "Cannot " + string(perm ? "change" : "restore")
                      + " mode bits of '" + path->GetPath() + '\''
                      + s_OSReason(x_errno));
        }
    }
}


static string s_BaseDir(const string& dirname)
{
    string retval = CDirEntry::AddTrailingPathSeparator(dirname);
#ifdef NCBI_OS_MSWIN
    // Replace backslashes with forward slashes
    NStr::ReplaceInPlace(retval, "\\", "/");
#endif //NCBI_OS_MSWIN
    return retval;
}


static string s_ToFilesystemPath(const string& base_dir, const string& name)
{
    string path(CDirEntry::IsAbsolutePath(name)  ||  base_dir.empty()
                ? name : CDirEntry::ConcatPath(base_dir, name));
    return CDirEntry::NormalizePath(path);
}


static string s_ToArchiveName(const string& base_dir, const string& path)
{
    // NB: Path assumed to have been normalized
    string retval = CDirEntry::AddTrailingPathSeparator(path);

#ifdef NCBI_OS_MSWIN
    // Convert to Unix format with forward slashes
    NStr::ReplaceInPlace(retval, "\\", "/");
    const NStr::ECase how = NStr::eNocase;
#else
    const NStr::ECase how = NStr::eCase;
#endif //NCBI_OS_MSWIN

    bool absolute;
    // Remove leading base dir from the path
    if (!base_dir.empty()  &&  NStr::StartsWith(retval, base_dir, how)) {
        if (retval.size() > base_dir.size()) {
            retval.erase(0, base_dir.size()/*separator too*/);
        } else {
            retval.assign(1, '.');
        }
        absolute = false;
    } else {
        absolute = CDirEntry::IsAbsolutePath(retval);
    }

    SIZE_TYPE pos = 0;

#ifdef NCBI_OS_MSWIN
    // Remove a disk name if present
    if (retval.size() > 1
        &&  isalpha((unsigned char) retval[0])  &&  retval[1] == ':') {
        pos = 2;
    }
#endif //NCBI_OS_MSWIN

    // Remove any leading and trailing slashes
    while (pos < retval.size()  &&  retval[pos] == '/') {
        pos++;
    }
    if (pos) {
        retval.erase(0, pos);
    }
    pos = retval.size();
    while (pos > 0  &&  retval[pos - 1] == '/') {
        --pos;
    }
    if (pos < retval.size()) {
        retval.erase(pos);
    }

    if (absolute) {
        retval.insert((SIZE_TYPE) 0, 1, '/');
    }
    return retval;
}


auto_ptr<CTar::TEntries> CTar::x_Append(const string&   name,
                                        const TEntries* toc)
{
    auto_ptr<TEntries>       entries(new TEntries);
    auto_ptr<CDir::TEntries> dir;

    const EFollowLinks follow_links = (m_Flags & fFollowLinks ?
                                       eFollowLinks : eIgnoreLinks);
    unsigned int uid = 0, gid = 0;
    bool update = true;

    // Create the entry info
    m_Current = CTarEntryInfo(m_StreamPos);

    // Compose entry name for relative names
    string path = s_ToFilesystemPath(m_BaseDir, name);

    // Get direntry information
    CDirEntry entry(path);
    CDirEntry::SStat st;
    if (!entry.Stat(&st, follow_links)) {
        int x_errno = errno;
        TAR_THROW(this, eOpen,
                  "Cannot get status of '" + path + '\''+ s_OSReason(x_errno));
    }
    CDirEntry::EType type = CDirEntry::GetType(st.orig);

    string temp = s_ToArchiveName(m_BaseDir, path);

    if (temp.empty()) {
        TAR_THROW(this, eBadName,
                  "Empty entry name not allowed");
    }

    list<CTempString> elems;
    NStr::Split(temp, "/", elems);
    if (find(elems.begin(), elems.end(), "..") != elems.end()) {
        TAR_THROW(this, eBadName,
                  "Name '" + temp + "' embeds parent directory ('..')");
    }
    if (m_Mask[eExcludeMask].mask
        &&  s_MatchPattern(elems,
                           m_Mask[eExcludeMask].mask,
                           m_Mask[eExcludeMask].acase)) {
        goto out;
    }
    elems.clear();
    if (type == CDirEntry::eDir  &&  temp != "/") {
        temp += '/';
    }

    m_Current.m_Name.swap(temp);
    m_Current.m_Type = CTarEntryInfo::EType(type);
    if (m_Current.GetType() == CTarEntryInfo::eSymLink) {
        _ASSERT(!follow_links);
        m_Current.m_LinkName = entry.LookupLink();
        if (m_Current.GetLinkName().empty()) {
            TAR_THROW(this, eBadName,
                      "Empty link name not allowed");
        }
    }

    entry.GetOwner(&m_Current.m_UserName, &m_Current.m_GroupName,
                   follow_links, &uid, &gid);
#ifdef NCBI_OS_UNIX
    if (NStr::UIntToString(uid) == m_Current.GetUserName()) {
        m_Current.m_UserName.erase();
    }
    if (NStr::UIntToString(gid) == m_Current.GetGroupName()) {
        m_Current.m_GroupName.erase();
    }
#endif //NCBI_OS_UNIX
#ifdef NCBI_OS_MSWIN
    // These are fake but we don't want to leave plain 0 (root) in there
    st.orig.st_uid = (uid_t) uid;
    st.orig.st_gid = (gid_t) gid;
#endif //NCBI_OS_MSWIN

    m_Current.m_Stat = st.orig;
    // Fixup for mode bits
    m_Current.m_Stat.st_mode = (mode_t) s_ModeToTar(st.orig.st_mode);

    // Check if we need to update this entry in the archive
    if (toc) {
        bool found = false;

        if (type != CDirEntry::eUnknown) {
            // Start searching from the end of the list, to find
            // the most recent entry (if any) first
            _ASSERT(temp.empty());
            REVERSE_ITERATE(TEntries, e, *toc) {
                if (!temp.empty()) {
                    if (e->GetType() == CTarEntryInfo::eHardLink  ||
                        temp != s_ToFilesystemPath(m_BaseDir, e->GetName())) {
                        continue;
                    }
                } else if (path == s_ToFilesystemPath(m_BaseDir,e->GetName())){
                    found = true;
                    if (e->GetType() == CTarEntryInfo::eHardLink) {
                        temp = s_ToFilesystemPath(m_BaseDir, e->GetLinkName());
                        continue;
                    }
                } else {
                    continue;
                }
                if (m_Current.GetType() != e->GetType()) {
                    if (m_Flags & fEqualTypes) {
                        goto out;
                    }
                } else if (m_Current.GetType() == CTarEntryInfo::eSymLink
                           &&  m_Current.GetLinkName() == e->GetLinkName()) {
                    goto out;
                }
                if (m_Current.GetModificationTime() <=
                    e->GetModificationTime()) {
                    update = false;  // same(or older), no update
                }
                break;
            }
        }

        if (!update  ||  (!found  &&  (m_Flags & (fUpdate & ~fOverwrite)))) {
            if (type != CDirEntry::eDir  &&  type != CDirEntry::eUnknown) {
                goto out;
            }
            // Directories always get recursive treatment later
            update = false;
        }
    }

    // Append the entry
    switch (type) {
    case CDirEntry::eFile:
        _ASSERT(update);
        x_AppendFile(path);
        entries->push_back(m_Current);
        break;

    case CDirEntry::eBlockSpecial:
    case CDirEntry::eCharSpecial:
    case CDirEntry::eSymLink:
    case CDirEntry::ePipe:
        _ASSERT(update);
        m_Current.m_Stat.st_size = 0;
        x_WriteEntryInfo(path);
        entries->push_back(m_Current);
        break;

    case CDirEntry::eDir:
        dir.reset(CDir(path).GetEntriesPtr(kEmptyStr, CDir::eIgnoreRecursive));
        if (!dir.get()) {
            int x_errno = errno;
            TAR_THROW(this, eRead,
                      "Cannot list directory '" + path + '\''
                      + s_OSReason(x_errno));
        }
        if (update) {
            m_Current.m_Stat.st_size = 0;
            x_WriteEntryInfo(path);
            entries->push_back(m_Current);
        }
        // Append/update all files from that directory
        ITERATE(CDir::TEntries, e, *dir) {
            auto_ptr<TEntries> add = x_Append((*e)->GetPath(), toc);
            entries->splice(entries->end(), *add);
        }
        break;

    case CDirEntry::eDoor:
    case CDirEntry::eSocket:
        // Tar does not have any provisions to store this kind of entries
        if (!(m_Flags & fSkipUnsupported)) {
            TAR_POST(3, Warning,
                     "Skipping non-archiveable "
                     + string(type == CDirEntry::eSocket ? "socket" : "door")
                     + " entry '" + path + '\'');
        }
        break;

    case CDirEntry::eUnknown:
        if (!(m_Flags & fSkipUnsupported)) {
            TAR_THROW(this, eBadName,
                      "Unable to handle '" + path + '\'');
        }
        /*FALLTHRU*/

    default:
        if (type == CDirEntry::eUnknown  ||  !(m_Flags & fSkipUnsupported)) {
            TAR_POST(14, Warning,
                     "Skipping unsupported source '" + path
                     + "' of type #" + NStr::IntToString(int(type)));
        }
        break;
    }

 out:
    return entries;
}


auto_ptr<CTar::TEntries> CTar::x_Append(const CTarUserEntryInfo& entry,
                                        CNcbiIstream& is)
{
    auto_ptr<TEntries> entries(new TEntries);

    // Create a temp entry info first
    m_Current = CTarEntryInfo(m_StreamPos);

    string temp = s_ToArchiveName(kEmptyStr, entry.GetName());

    while (NStr::EndsWith(temp, '/')) { // NB: directories are not allowed here
        temp.resize(temp.size() - 1);
    }
    if (temp.empty()) {
        TAR_THROW(this, eBadName,
                  "Empty entry name not allowed");
    }

    list<CTempString> elems;
    NStr::Split(temp, "/", elems);
    if (find(elems.begin(), elems.end(), "..") != elems.end()) {
        TAR_THROW(this, eBadName,
                  "Name '" + temp + "' embeds parent directory ('..')");
    }
    elems.clear();

    // Recreate entry info
    m_Current = entry;
    m_Current.m_Name.swap(temp);
    m_Current.m_Pos = m_StreamPos;
    m_Current.m_Type = CTarEntryInfo::eFile;

    if (!is.good()) {
        TAR_THROW(this, eRead,
                  "Bad input file stream");
    }

    m_Current.m_Stat.st_mtime
        = m_Current.m_Stat.st_atime
        = m_Current.m_Stat.st_ctime
        = CTime(CTime::eCurrent).GetTimeT();

#ifdef NCBI_OS_UNIX
    // use regular file mode, adjusted with umask()
    mode_t mode = s_TarToMode(fTarURead | fTarUWrite |
                              fTarGRead | fTarGWrite |
                              fTarORead | fTarOWrite);
    mode_t u;
#  ifdef HAVE_GETUMASK
    // NB: thread-safe
    u = getumask();
#  else
    u = umask(0);
    umask(u);
#  endif //HAVE_GETUMASK
    mode &= ~u;
    m_Current.m_Stat.st_mode = (mode_t) s_ModeToTar(mode);

    m_Current.m_Stat.st_uid = geteuid();
    m_Current.m_Stat.st_gid = getegid();

    CUnixFeature::GetUserNameByUID(m_Current.m_Stat.st_uid)
        .swap(m_Current.m_UserName);
    CUnixFeature::GetGroupNameByGID(m_Current.m_Stat.st_gid)
        .swap(m_Current.m_GroupName);
#endif //NCBI_OS_UNIX
#ifdef NCBI_OS_MSWIN
    // safe file mode
    m_Current.m_Stat.st_mode = (fTarURead | fTarUWrite |
                                fTarGRead | fTarORead);

    unsigned int uid = 0, gid = 0;
    CWinSecurity::GetObjectOwner(CProcess::GetCurrentHandle(),
                                 SE_KERNEL_OBJECT,
                                 &m_Current.m_UserName,
                                 &m_Current.m_GroupName,
                                 &uid, &gid);
    // these are fake but we don't want to leave plain 0 (Unix root) in there
    m_Current.m_Stat.st_uid = (uid_t) uid;
    m_Current.m_Stat.st_gid = (gid_t) gid;
#endif //NCBI_OS_MSWIN

    x_AppendStream(entry.GetName(), is);

    entries->push_back(m_Current);
    return entries;
}


// Regular entries only!
void CTar::x_AppendStream(const string& name, CNcbiIstream& is)
{
    _ASSERT(m_Current.GetType() == CTarEntryInfo::eFile);

    // Write entry header
    x_WriteEntryInfo(name);

    errno = 0;
    Uint8 size = m_Current.GetSize();
    while (size) {
        // Write file contents
        _ASSERT(m_BufferPos < m_BufferSize);
        size_t avail = m_BufferSize - m_BufferPos;
        if (avail > size) {
            avail = (size_t) size;
        }
        // Read file
        int x_errno = 0;
        streamsize xread;
        if (is.good()) {
            try {
                if (!is.read(m_Buffer + m_BufferPos, (streamsize) avail)) {
                    x_errno = errno;
                    xread = -1;
                } else {
                    xread = is.gcount();
                }
            } catch (IOS_BASE::failure&) {
                xread = -1;
            }
        } else {
            xread = -1;
        }
        if (xread <= 0) {
            ifstream* ifs = dynamic_cast<ifstream*>(&is);
            TAR_THROW(this, eRead,
                      "Error reading "
                      + string(ifs ? "file" : "stream")
                      + " '" + name + '\'' + s_OSReason(ifs ? x_errno : 0));
        }
        // Write buffer to the archive
        avail = (size_t) xread;
        x_WriteArchive(avail);
        size -= avail;
    }

    // Write zeros to get the written size a multiple of BLOCK_SIZE
    size_t zero = ALIGN_SIZE(m_BufferPos) - m_BufferPos;
    memset(m_Buffer + m_BufferPos, 0, zero);
    x_WriteArchive(zero);
    _ASSERT(!OFFSET_OF(m_BufferPos)  &&  !OFFSET_OF(m_StreamPos));
}


// Regular files only!
void CTar::x_AppendFile(const string& file)
{
    _ASSERT(m_Current.GetType() == CTarEntryInfo::eFile);

    // FIXME:  Switch to CFileIO eventually to avoid ifstream
    // obscurity w.r.t. errors, an extra layer of buffering etc.
    CNcbiIfstream ifs;

    // Open file
    ifs.open(file.c_str(), IOS_BASE::binary | IOS_BASE::in);
    if (!ifs) {
        int x_errno = errno;
        TAR_THROW(this, eOpen,
                  "Cannot open file '" + file + '\'' + s_OSReason(x_errno));
    }

    x_AppendStream(file, ifs);
}


void CTar::SetMask(CMask*    mask, EOwnership  own,
                   EMaskType type, NStr::ECase acase)
{
    int idx = int(type);
    if (idx < 0  ||  sizeof(m_Mask)/sizeof(m_Mask[0]) <= (size_t) idx){
        TAR_THROW(this, eMemory,
                  "Mask type is out of range: " + NStr::IntToString(idx));
    }
    if (m_Mask[idx].owned) {
        delete m_Mask[idx].mask;
    }
    m_Mask[idx].mask  = mask;
    m_Mask[idx].acase = acase;
    m_Mask[idx].owned = mask ? own : eNoOwnership;
}


void CTar::SetBaseDir(const string& dirname)
{
    string dir = s_BaseDir(dirname);
    dir.swap(m_BaseDir);
}


Uint8 CTar::EstimateArchiveSize(const TFiles& files,
                                size_t blocking_factor,
                                const string& base_dir)
{
    const size_t buffer_size = SIZE_OF(blocking_factor);
    string basedir = s_BaseDir(base_dir);
    Uint8 result = 0;

    ITERATE(TFiles, f, files) {
        // Count in the file size
        result += BLOCK_SIZE/*header*/ + ALIGN_SIZE(f->second);

        // Count in the long name (if any)
        string path    = s_ToFilesystemPath(basedir, f->first);
        string name    = s_ToArchiveName(basedir, path);
        size_t namelen = name.size() + 1;
        if (namelen > sizeof(((SHeader*) 0)->name)) {
            result += BLOCK_SIZE/*long name header*/ + ALIGN_SIZE(namelen);
        }
    }
    if (result) {
        result += BLOCK_SIZE << 1;  // EOT
        Uint8 padding = result % buffer_size;
        if (padding) {
            result += buffer_size - padding;
        }
    }

    return result;
}


class CTarReader : public IReader
{
public:
    CTarReader(CTar* tar, EOwnership own = eNoOwnership)
        : m_Read(0), m_Eof(false), m_Bad(false), m_Tar(tar, own)
    { }

    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* count);

private:
    Uint8         m_Read;
    bool          m_Eof;
    bool          m_Bad;
    AutoPtr<CTar> m_Tar;
};


ERW_Result CTarReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (m_Bad  ||  !count) {
        if (bytes_read) {
            *bytes_read = 0;
        }
        return m_Bad ? eRW_Error
            : (m_Read < m_Tar->m_Current.GetSize()  ||  !m_Eof) ? eRW_Success
            : eRW_Eof;
    }

    size_t read;
    _ASSERT(m_Tar->m_Current.GetSize() >= m_Read);
    Uint8  left = m_Tar->m_Current.GetSize() - m_Read;
    if (!left) {
        m_Eof = true;
        read = 0;
    } else {
        if (count > left) {
            count = (size_t) left;
        }

        size_t off = (size_t) OFFSET_OF(m_Read);
        if (off) {
            read = BLOCK_SIZE - off;
            if (m_Tar->m_BufferPos) {
                off += m_Tar->m_BufferPos  - BLOCK_SIZE;
            } else {
                off += m_Tar->m_BufferSize - BLOCK_SIZE;
            }
            if (read > count) {
                read = count;
            }
            memcpy(buf, m_Tar->m_Buffer + off, read);
            m_Read += read;
            count  -= read;
            if (!count) {
                goto out;
            }
            buf = (char*) buf + read;
        } else {
            read = 0;
        }

        off = m_Tar->m_BufferPos;  // NB: x_ReadArchive() changes m_BufferPos
        if (m_Tar->x_ReadArchive(count)) {
            memcpy(buf, m_Tar->m_Buffer + off, count);
            m_Tar->m_StreamPos += ALIGN_SIZE(count);
            _ASSERT(!OFFSET_OF(m_Tar->m_StreamPos));
            m_Read += count;
            read   += count;
        } else {
            m_Bad = true;
            _ASSERT(!m_Tar->m_Stream.good());
            // If we don't throw here, it may look like an ordinary EOF
            TAR_THROW(m_Tar, eRead,
                      "Read error while streaming");
        }
    }

 out:
    _ASSERT(!m_Bad);
    if (bytes_read) {
        *bytes_read = read;
    }
    return m_Eof ? eRW_Eof : eRW_Success;
}


ERW_Result CTarReader::PendingCount(size_t* count)
{
    if (m_Bad) {
        return eRW_Error;
    }
    _ASSERT(m_Tar->m_Current.GetSize() >= m_Read);
    Uint8 left = m_Tar->m_Current.GetSize() - m_Read;
    if (!left  &&  m_Eof) {
        return eRW_Eof;
    }
    size_t avail = BLOCK_SIZE - (size_t) OFFSET_OF(m_Read);
    _ASSERT(m_Tar->m_BufferPos < m_Tar->m_BufferSize);
    if (m_Tar->m_BufferPos) {
        avail += m_Tar->m_BufferSize - m_Tar->m_BufferPos;
    }
    *count = avail > left ? (size_t) left : avail;
    return eRW_Success;
}


IReader* CTar::Extract(CNcbiIstream& is,
                       const string& name, CTar::TFlags flags)
{
    auto_ptr<CTar> tar(new CTar(is, 1/*blocking factor*/));
    tar->SetFlags(flags & ~fStreamPipeThrough);

    auto_ptr<CMaskFileName> mask(new CMaskFileName);
    mask->Add(name);
    tar->SetMask(mask.get(), eTakeOwnership);
    mask.release();

    tar->x_Open(eInternal);
    auto_ptr<TEntries> temp = tar->x_ReadAndProcess(eInternal);
    _ASSERT(temp.get()  &&  temp->size() < 2);
    if (temp->size() < 1) {
        return 0;
    }

    _ASSERT(tar->m_Current == temp->front());
    CTarEntryInfo::EType type = tar->m_Current.GetType();
    if (type != CTarEntryInfo::eFile
        &&  (type != CTarEntryInfo::eUnknown  ||  (flags & fSkipUnsupported))){
        return 0;
    }

    IReader* ir = new CTarReader(tar.get(), eTakeOwnership);
    tar.release();
    return ir;
}


IReader* CTar::GetNextEntryData(void)
{
    CTarEntryInfo::EType type = m_Current.GetType();
    return type != CTarEntryInfo::eFile
        &&  (type != CTarEntryInfo::eUnknown  ||  (m_Flags & fSkipUnsupported))
        ? 0 : new CTarReader(this);
}


END_NCBI_SCOPE
