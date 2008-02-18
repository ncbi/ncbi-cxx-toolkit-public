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
 * Authors:  Vladimir Ivanov
 *           Anton Lavrentiev
 *
 * File Description:
 *   Tar archive API.
 *
 *   Supports subset of POSIX.1-1988 (ustar) format.
 *   Old GNU (POSIX 1003.1) and V7 formats are also supported partially.
 *   New archives are created using POSIX (genuine ustar) format, using
 *   GNU extensions for long names/links only when unavoidable.
 *   Can handle no exotics like sparse / contiguous files, special
 *   files (devices, FIFOs), multivolume / incremental archives, etc,
 *   but just regular files, directories, and links:  can extract
 *   both hard- and symlinks, but can store symlinks only.
 *
 */

#define NCBI_MODULE NCBITAR

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>
#include <util/compress/tar.hpp>
#include <util/error_codes.hpp>

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "Class CTar can be defined on MS-Windows and UNIX platforms only!"
#endif

#if   defined(NCBI_OS_UNIX)
#  include <unistd.h>
#elif defined(NCBI_OS_MSWIN)
#  include <io.h>
typedef unsigned int mode_t;
typedef short        uid_t;
typedef short        gid_t;
#endif


#define NCBI_USE_ERRCODE_X   Util_Compress


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// TAR helper routines
//

// Convert a number to an octal string padded to the left
// with [leading] zeros ('0') and having _no_ trailing '\0'.
static bool s_NumToOctal(unsigned long value, char* ptr, size_t len)
{
    _ASSERT(len > 0);
    do {
        ptr[--len] = '0' + char(value & 7);
        value >>= 3;
    } while (len);
    return !value ? true : false;
}


// Convert an octal number (possibly preceded by spaces) to numeric form.
// Stop either at the end of the field or at first '\0' (if any).
static bool s_OctalToNum(unsigned long& value, const char* ptr, size_t size)
{
    _ASSERT(ptr  &&  size > 0);
    size_t i = *ptr ? 0 : 1;
    while (i < size  &&  ptr[i]) {
        if (!isspace((unsigned char) ptr[i]))
            break;
        i++;
    }
    value = 0;
    bool retval = false;
    while (i < size  &&  ptr[i] >= '0'  &&  ptr[i] <= '7') {
        retval = true;
        value <<= 3;
        value  |= ptr[i++] - '0';
    }
    while (i < size  &&  ptr[i]) {
        if (!isspace((unsigned char) ptr[i]))
            return false;
        i++;
    }
    return retval;
}


static mode_t s_TarToMode(TTarMode value)
{
    mode_t mode = (
#ifdef S_ISUID
                   (value & fTarSetUID   ? S_ISUID  : 0) |
#endif
#ifdef S_ISGID
                   (value & fTarSetGID   ? S_ISGID  : 0) |
#endif
#ifdef S_ISVTX
                   (value & fTarSticky   ? S_ISVTX  : 0) |
#endif
#if   defined(S_IRUSR)
                   (value & fTarURead    ? S_IRUSR  : 0) |
#elif defined(S_IREAD)
                   (value & fTarURead    ? S_IREAD  : 0) |
#endif
#if   defined(S_IWUSR)
                   (value & fTarUWrite   ? S_IWUSR  : 0) |
#elif defined(S_IWRITE)
                   (value & fTarUWrite   ? S_IWRITE : 0) |
#endif
#if   defined(S_IWUSR)
                   (value & fTarUExecute ? S_IXUSR  : 0) |
#elif defined(S_IEXEC)
                   (value & fTarUExecute ? S_IEXEC  : 0) |
#endif
#ifdef S_IRGRP
                   (value & fTarGRead    ? S_IRGRP  : 0) |
#endif
#ifdef S_IWGRP
                   (value & fTarGWrite   ? S_IWGRP  : 0) |
#endif
#ifdef S_IXGRP
                   (value & fTarGExecute ? S_IXGRP  : 0) |
#endif
#ifdef S_IROTH
                   (value & fTarORead    ? S_IROTH  : 0) |
#endif
#ifdef S_IWOTH
                   (value & fTarOWrite   ? S_IWOTH  : 0) |
#endif
#ifdef S_IXOTH
                   (value & fTarOExecute ? S_IXOTH  : 0) |
#endif
                   0);
    return mode;
}


static TTarMode s_ModeToTar(mode_t mode)
{
    /* Foresee that the mode can be extracted on a different platform */
    TTarMode value = (
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
                      /* emulate read permission when file is readable */
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
                      /* emulate read permission when file is readable */
                      (mode & S_IREAD  ? fTarORead    : 0) |
#endif
#ifdef S_IWOTH
                      (mode & S_IWOTH  ? fTarOWrite   : 0) |
#endif
#ifdef S_IXOTH
                      (mode & S_IXOTH  ? fTarOExecute : 0) |
#endif
                      0);
    return value;
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

/// Round up to the nearest multiple of kBlockSize
#define ALIGN_SIZE(size) (((size) + (kBlockSize-1)) & ~(kBlockSize-1))
#define OFFSET_OF(size)  ( (size)                   &  (kBlockSize-1))
#define BLOCK_OF(pos)    ((pos) >> 9)
#define SIZE_OF(blk)     ((blk) << 9)

/// Tar block size (512 bytes)
static const size_t kBlockSize = SIZE_OF(1);


/// Recognized TAR formats
enum ETar_Format {
    eTar_Unknown = 0,
    eTar_Legacy,
    eTar_OldGNU,
    eTar_Ustar
};


/// POSIX "ustar" tar archive member header
struct SHeader {             // byte offset
    char name[100];          //   0
    char mode[8];            // 100
    char uid[8];             // 108
    char gid[8];             // 116
    char size[12];           // 124
    char mtime[12];          // 136
    char checksum[8];        // 148
    char typeflag[1];        // 156
    char linkname[100];      // 157
    char magic[6];           // 257
    char version[2];         // 263
    char uname[32];          // 265
    char gname[32];          // 297
    char devmajor[8];        // 329
    char devminor[8];        // 337
    union {                  // 345
        char prefix[155];    // NB: not valid with old GNU format
        struct {             // NB: old GNU format only
            char atime[12];
            char ctime[12];  // 357  
        } gt;
    };
};


/// Block as a header.
union TBlock {
    char    buffer[kBlockSize];
    SHeader header;
};


static bool s_TarChecksum(TBlock* block, bool isgnu = false)
{
    SHeader* h = &block->header;
    size_t len = sizeof(h->checksum) - (isgnu ? 2 : 1);

    // Compute the checksum
    memset(h->checksum, ' ', sizeof(h->checksum));
    unsigned long checksum = 0;
    const unsigned char* p = (const unsigned char*) block->buffer;
    for (size_t i = 0; i < sizeof(block->buffer); i++) {
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
    TTarMode mode = GetMode();

    // User
    if (usr_mode) {
        *usr_mode = ((mode & fTarURead    ? CDirEntry::fRead    : 0) |
                     (mode & fTarUWrite   ? CDirEntry::fWrite   : 0) |
                     (mode & fTarUExecute ? CDirEntry::fExecute : 0));
    }

    // Group
    if (grp_mode) {
        *grp_mode = ((mode & fTarGRead    ? CDirEntry::fRead    : 0) |
                     (mode & fTarGWrite   ? CDirEntry::fWrite   : 0) |
                     (mode & fTarGExecute ? CDirEntry::fExecute : 0));
    }

    // Others
    if (oth_mode) {
        *oth_mode = ((mode & fTarORead    ? CDirEntry::fRead    : 0) |
                     (mode & fTarOWrite   ? CDirEntry::fWrite   : 0) |
                     (mode & fTarOExecute ? CDirEntry::fExecute : 0));
    }

    // Special bits
    if (special_bits) {
        *special_bits = ((mode & fTarSetUID ? CDirEntry::fSetUID : 0) |
                         (mode & fTarSetGID ? CDirEntry::fSetGID : 0) |
                         (mode & fTarSticky ? CDirEntry::fSticky : 0));
    }

    return;
}


static string s_ModeAsString(TTarMode mode)
{
    string usr("---");
    string grp("---");
    string oth("---");
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

    return usr + grp + oth;
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
    return user + "/" + group;
}


ostream& operator << (ostream& os, const CTarEntryInfo& info)
{
    CTime mtime(info.GetModificationTime());
    os << s_TypeAsChar(info.GetType())
       << s_ModeAsString(info.GetMode())        << ' '
       << setw(17) << s_UserGroupAsString(info) << ' '
       << setw(10) << NStr::UInt8ToString(info.GetSize())
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
                                 const string* entry)
{
    _ASSERT(!OFFSET_OF(pos));
    _ASSERT(!OFFSET_OF(recsize));
    _ASSERT(recsize >= kBlockSize);
    string result;
    if (!file.empty()) {
        CDirEntry temp(file);
        result = temp.GetName() + ": ";
    }
    result += "At record " + NStr::UInt8ToString(pos / recsize);
    if (recsize != kBlockSize) {
        result +=
            ", block " + NStr::UInt8ToString(BLOCK_OF(pos % recsize)) +
            " [thru #" + NStr::UInt8ToString(BLOCK_OF(pos),
                                             NStr::fWithCommas) + ']';
    }
    if (entry  &&  !entry->empty()) {
        result += ", while in '" + *entry + '\'';
    }
    return result + ":\n";
}


static string s_AddressAsString(size_t offset)
{
    // All addresses but the base are greater than 100:
    // do the quick and dirty formatting here
    return offset ? NStr::UIntToString((unsigned long) offset) : "000";
}


static bool memcchr(const char* s, char c, size_t len)
{
    for (size_t i = 0;  i < len;  i++) {
        if (s[i] != c)
            return true;
    }
    return false;
}


static string s_Printable(const char* field, size_t maxsize, bool text)
{
    if (!text  &&  maxsize > 1  &&  !*field)
        field++, maxsize--;
    size_t len = s_Length(field, maxsize);
    return NStr::PrintableString(string(field,
                                        memcchr(field + len, '\0',
                                                maxsize - len)
                                        ? maxsize
                                        : len));
}


#if !defined(__GNUC__)  &&  !defined(offsetof)
#  define offsetof(T, F) ((char*) &(((T*) 0)->F) - (char*) 0)
#endif


#define _STR(s) #s

#define TAR_PRINTABLE(field, text)                                      \
    "@" + s_AddressAsString((size_t) offsetof(SHeader, field)) +        \
    '[' + _STR(field) + "]:" + string(10 - sizeof(_STR(field)), ' ') +  \
    '"' + s_Printable(h->field, sizeof(h->field), text  ||  ex) + '"'

static string s_DumpHeader(const SHeader* h, ETar_Format fmt, bool ex = false)
{
    unsigned long val;
    string dump;
    bool ok;

    dump += TAR_PRINTABLE(name, true) + '\n';

    ok = s_OctalToNum(val, h->mode, sizeof(h->mode));
    dump += TAR_PRINTABLE(mode, !ok);
    if (ok  &&  val) {
        dump += " [" + s_ModeAsString((TTarMode) val) + ']';
    }
    dump += '\n';

    ok = s_OctalToNum(val, h->uid, sizeof(h->uid));
    dump += TAR_PRINTABLE(uid, !ok);
    if (ok  &&  val > 7) {
        dump += " [" + NStr::UIntToString(val) + ']';
    }
    dump += '\n';
    
    ok = s_OctalToNum(val, h->gid, sizeof(h->gid));
    dump += TAR_PRINTABLE(gid, !ok);
    if (ok  &&  val > 7) {
        dump += " [" + NStr::UIntToString(val) + ']';
    }
    dump += '\n';

    ok = s_OctalToNum(val, h->size, sizeof(h->size));
    dump += TAR_PRINTABLE(size, !ok);
    if (ok  &&  val > 7) {
        dump += " [" + NStr::UIntToString(val) + ']';
    }
    dump += '\n';

    ok = s_OctalToNum(val, h->mtime, sizeof(h->mtime));
    dump += TAR_PRINTABLE(mtime, !ok);
    if (ok  &&  val) {
        CTime mtime((time_t) val);
        dump += (" [" + NStr::UIntToString(val)
                 + mtime.ToLocalTime().AsString(", Y-M-D h:m:s]"));
    }
    dump += '\n';

    ok = s_OctalToNum(val, h->checksum, sizeof(h->checksum));
    dump += TAR_PRINTABLE(checksum, !ok) + '\n';

    // Classify to the extent possible to help debug the problem (if any)
    dump += TAR_PRINTABLE(typeflag, true);
    ok = false;
    const char* tname = 0;
    switch (h->typeflag[0]) {
    case '\0':
    case '0':
        ok = true;
        if (fmt != eTar_Ustar  &&  fmt != eTar_OldGNU) {
            size_t namelen = s_Length(h->name, sizeof(h->name));
            if (namelen  &&  h->name[namelen - 1] == '/') {
                tname = "legacy regular entry (dir)" + (h->typeflag[0]? 7 : 0);
                break;
            }
        }
        tname = "legacy regular entry (file)" + (h->typeflag[0] ? 7 : 0);
        break;
    case '1':
        ok = true;
#ifdef NCBI_OS_UNIX
        tname = "hard link";
#else
        tname = "hard link - not FULLY supported";
#endif // NCBI_OS_UNIX
        break;
    case '2':
        ok = true;
#ifdef NCBI_OS_UNIX
        tname = "symbolic link";
#else
        tname = "symbolic link - not FULLY supported";
#endif // NCBI_OS_UNIX
        break;
    case '3':
        tname = "character device";
        break;
    case '4':
        tname = "block device";
        break;
    case '5':
        ok = true;
        tname = "directory";
        break;
    case '6':
        tname = "FIFO";
        break;
    case '7':
        tname = "contiguous file";
        break;
    case 'g':
        tname = "global extended header";
        break;
    case 'x':
        if (fmt == eTar_Ustar) {
            ok = true;
            tname = "extended (PAX) header - not FULLY supported";
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
        tname = "inode only";
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
        if (fmt == eTar_OldGNU) {
            tname = "GNU extension: multivolume archive";
        }
        break;
    case 'N':
        if (fmt == eTar_OldGNU) {
            tname = "GNU extension: long filename";
        }
        break;
    case 'S':
        if (fmt == eTar_OldGNU) {
            tname = "GNU extension: sparse file";
        }
        break;
    case 'V':
        if (fmt == eTar_OldGNU) {
            tname = "GNU extension: volume header";
        }
        break;
    case 'X':
        tname = "extended (POSIX 1003.1-2001) header";
        break;
    default:
        break;
    }
    if (!tname  &&  h->typeflag[0] >= 'A'  &&  h->typeflag[0] <= 'Z') {
        tname = "user-defined extension";
    }
    dump += (" [" + string(tname ? tname : "reserved") +
             (ok
              ? "]\n"
              : " -- NOT SUPPORTED]\n"));

    dump += TAR_PRINTABLE(linkname, true) + '\n';

    switch (fmt) {
    case eTar_Legacy:
        tname = "legacy";
        break;
    case eTar_OldGNU:
        tname = "old GNU";
        break;
    case eTar_Ustar:
        tname = "ustar";
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

    dump += TAR_PRINTABLE(version, true) + '\n';

    if (fmt != eTar_Legacy) {
        dump += TAR_PRINTABLE(uname, true) + '\n';

        dump += TAR_PRINTABLE(gname, true) + '\n';

        ok = s_OctalToNum(val, h->devmajor, sizeof(h->devmajor));
        dump += TAR_PRINTABLE(devmajor, !ok);
        if (ok  &&  val > 7) {
            dump += " [" + NStr::UIntToString(val) + ']';
        }
        dump += '\n';

        ok = s_OctalToNum(val, h->devminor, sizeof(h->devminor));
        dump += TAR_PRINTABLE(devminor, !ok);
        if (ok  &&  val > 7) {
            dump += " [" + NStr::UIntToString(val) + ']';
        }
        dump += '\n';

        if (fmt == eTar_OldGNU) {
            ok = s_OctalToNum(val, h->gt.atime, sizeof(h->gt.atime));
            dump += TAR_PRINTABLE(gt.atime, !ok);
            if (ok  &&  val) {
                CTime mtime((time_t) val);
                dump += (" [" + NStr::UIntToString(val)
                         + mtime.ToLocalTime().AsString(", Y-M-D h:m:s]"));
            }
            dump += '\n';

            ok = s_OctalToNum(val, h->gt.ctime, sizeof(h->gt.ctime));
            dump += TAR_PRINTABLE(gt.ctime, !ok);
            if (ok  &&  val) {
                CTime mtime((time_t) val);
                dump += (" [" + NStr::UIntToString(val)
                         + mtime.ToLocalTime().AsString(", Y-M-D h:m:s]"));
            }
            dump += '\n';
            tname = h->gt.ctime + sizeof(h->gt.ctime);
        } else {
            dump += TAR_PRINTABLE(prefix, true) + '\n';
            tname = h->prefix + sizeof(h->prefix);
        }
    } else
        tname = h->version + sizeof(h->version);

    for (size_t n = 0;  &tname[n] < (const char*) h + kBlockSize;  n++) {
        if (tname[n]) {
            size_t offset = (size_t)(&tname[n] - (const char*) h);
            dump += "@" + s_AddressAsString(offset) + ':' + string(11, ' ')
                + '"' + NStr::PrintableString(string(&tname[n],
                                                     kBlockSize - offset))
                + "\"\n";
            break;
        }
    }

    return dump;
}

#undef TAR_PRINTABLE

#undef _STR


//////////////////////////////////////////////////////////////////////////////
//
// CTar
//

CTar::CTar(const string& filename, size_t blocking_factor)
    : m_FileName(filename),
      m_FileStream(new CNcbiFstream),
      m_OpenMode(eNone),
      m_Stream(0),
      m_BufferSize(SIZE_OF(blocking_factor)),
      m_BufferPos(0),
      m_StreamPos(0),
      m_BufPtr(0),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership),
      m_IsModified(false)
{
    x_Init();
}


CTar::CTar(CNcbiIos& stream, size_t blocking_factor)
    : m_FileName(kEmptyStr),
      m_FileStream(0),
      m_OpenMode(eNone),
      m_Stream(&stream),
      m_BufferSize(SIZE_OF(blocking_factor)),
      m_BufferPos(0),
      m_StreamPos(0),
      m_BufPtr(0),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership),
      m_IsModified(false)
{
    x_Init();
}


CTar::~CTar()
{
    // Close stream(s)
    Close();
    delete m_FileStream;

    // Delete owned file name masks
    UnsetMask();

    // Delete buffer
    delete[] m_BufPtr;
}


#define TAR_THROW(errcode, message)                                     \
    NCBI_THROW(CTarException, errcode,                                  \
               s_PositionAsString(m_FileName, m_StreamPos,              \
                                  m_BufferSize, m_Current) + (message))

#define TAR_THROW_EX(errcode, message, h, fmt)                          \
    TAR_THROW(errcode,                                                  \
              m_Flags & fDumpBlockHeaders                               \
              ? string(message) + ":\n" + s_DumpHeader(h, fmt, true)    \
              : string(message))

#define TAR_POST(subcode, severity, message)                            \
    ERR_POST_X(subcode, severity <<                                     \
               s_PositionAsString(m_FileName, m_StreamPos,              \
                                  m_BufferSize, m_Current) + (message))


void CTar::x_Init(void)
{
    size_t pagesize = (size_t) GetVirtualMemoryPageSize();
    if (!pagesize) {
        pagesize = 4096; // reasonable default
    }
    size_t pagemask = pagesize - 1;
    // Assume that the page size is a power of 2
    _ASSERT((pagesize & pagemask) == 0);
    m_BufPtr = new char[m_BufferSize + pagemask];
    // Make m_Buffer page-aligned
    m_Buffer = m_BufPtr +
        ((((size_t) m_BufPtr + pagemask) & ~pagemask) - (size_t) m_BufPtr);
}


void CTar::x_Flush(void)
{
    m_Current = 0;
    if (!m_Stream  ||  !m_OpenMode  ||  !m_IsModified) {
        return;
    }
    _ASSERT(m_OpenMode == eRW);
    _ASSERT(m_BufferSize >= m_BufferPos);
    size_t pad = m_BufferSize - m_BufferPos;
    // Assure proper blocking factor and pad the archive as necessary
    memset(m_Buffer + m_BufferPos, 0, pad);
    x_WriteArchive(pad);
    _ASSERT(m_BufferPos == 0);
    if (pad < kBlockSize  ||  pad - OFFSET_OF(pad) < (kBlockSize << 1)) {
        // Write EOT (two zero blocks), if have not already done so by padding
        memset(m_Buffer, 0, m_BufferSize);
        x_WriteArchive(m_BufferSize);
        _ASSERT(m_BufferPos == 0);
        if (m_BufferSize == kBlockSize) {
            x_WriteArchive(kBlockSize);
            _ASSERT(m_BufferPos == 0);
        }
    }
    if (m_Stream->rdbuf()->PUBSYNC() != 0) {
        int x_errno = errno; 
        TAR_THROW(eWrite, "Archive flush failed" + s_OSReason(x_errno));
    }
    m_IsModified = false;
}


void CTar::x_Close(void)
{
    if (!m_OpenMode)
        return;
    if (m_FileStream  &&  m_FileStream->is_open()) {
        m_FileStream->close();
        m_Stream = 0;
    }
    m_OpenMode  = eNone;
    m_BufferPos = 0;
    m_StreamPos = 0;
}


auto_ptr<CTar::TEntries> CTar::x_Open(EAction action)
{
    m_Current = 0;
    _ASSERT(action);
    // We can only open a named file here, and if an external stream
    // is being used as an archive, it must be explicitly repositioned by
    // user's code (outside of this class) before each archive operation.
    if (!m_FileStream) {
        if (m_IsModified  &&  action != eAppend) {
            TAR_POST(1, Warning,
                     "Pending changes may be discarded"
                     " upon reopen of in-stream archive");
            m_IsModified = false;
            m_BufferPos = 0;
            m_StreamPos = 0;
        }
        if (!m_Stream  ||  !m_Stream->good()  ||  !m_Stream->rdbuf()) {
            m_OpenMode = eNone;
            TAR_THROW(eOpen, "Bad IO stream provided for archive");
        } else {
            m_OpenMode = EOpenMode(int(action) & eRW);
        }
    } else {
        EOpenMode mode = EOpenMode(int(action) & eRW);
        _ASSERT(mode != eNone);
        if (mode != eWO  &&  action != eAppend) {
            x_Flush();
        }
        if (mode == eWO  ||  m_OpenMode < mode) {
            x_Close();
            switch (mode) {
            case eWO:
                // WO access
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::out    |
                                   IOS_BASE::binary | IOS_BASE::trunc);
                break;
            case eRO:
                // RO access
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::in     |
                                   IOS_BASE::binary);
                break;
            case eRW:
                // RW access
                m_FileStream->open(m_FileName.c_str(),
                                   IOS_BASE::in     | IOS_BASE::out |
                                   IOS_BASE::binary);
                break;
            default:
                _TROUBLE;
                break;
            }
            if (!m_FileStream->good()) {
                int x_errno = errno;
                TAR_THROW(eOpen,
                          "Cannot open archive '" + m_FileName + '\''
                          + s_OSReason(x_errno));
            }
            m_Stream = m_FileStream;
        }

        if (m_OpenMode) {
            _ASSERT(action != eCreate);
            if (action != eAppend) {
                m_BufferPos = 0;
                m_StreamPos = 0;
                m_FileStream->seekg(0, IOS_BASE::beg);
            }
        } else {
            if (action == eAppend  &&  !m_IsModified) {
                // There may be an extra and unnecessary archive scanning
                // if Append() follows Update() that caused no modifications;
                // but there is no way to distinguish this, currently :-/
                // Also, this sequence should be a real rarity in practice.
                x_ReadAndProcess(eAppend);  // positions at logical EOF
            }
            m_OpenMode = mode;
        }
    }
    _ASSERT(m_Stream);
    _ASSERT(m_Stream->rdbuf());

    if (int(action) & ((eList | eExtract | eInternal) & ~eRW)) {
        return x_ReadAndProcess(action);
    } else {
        return auto_ptr<TEntries>(0);
    }
}


auto_ptr<CTar::TEntries> CTar::Extract(void)
{
    // Extract
    auto_ptr<TEntries> done = x_Open(eExtract);

    // Restore attributes of "postponed" directory entries
    if (m_Flags & fPreserveAll) {
        ITERATE(TEntries, i, *done.get()) {
            if (i->GetType() == CTarEntryInfo::eDir) {
                x_RestoreAttrs(*i);
            }
        }
    }

    return done;
}


Uint8 CTar::EstimateArchiveSize(const TFiles& files) const
{
    Uint8 result = 0;

    ITERATE(TFiles, it, files) {
        // Count in the file size
        result += kBlockSize/*header*/ + ALIGN_SIZE(it->second);

        // Count in the long name (if any)
        string path = x_ToFilesystemPath(it->first);
        string name = x_ToArchiveName(path);
        size_t namelen = name.length() + 1;
        if (namelen > sizeof(((SHeader*) 0)->name)) {
            result += kBlockSize/*long name header*/ + ALIGN_SIZE(namelen);
        }
    }
    if (result) {
        result += kBlockSize << 1; // EOT
        Uint8 incomplete = result % m_BufferSize;
        if (incomplete) {
            result += m_BufferSize - incomplete;
        }
    }

    return result;
}


void CTar::SetBaseDir(const string& dirname)
{
    m_BaseDir = CDirEntry::AddTrailingPathSeparator(dirname);
#ifdef NCBI_OS_MSWIN
    // Replace backslashes with forward slashes
    NStr::ReplaceInPlace(m_BaseDir, "\\", "/");
#endif // NCBI_OS_MSWIN
}


// Return a pointer to buffer, which is always block-aligned,
// and reflect the number of bytes available via the parameter.
const char* CTar::x_ReadArchive(size_t& n)
{
    _ASSERT(!OFFSET_OF(m_BufferPos));
    _ASSERT(n != 0);
    size_t nread;
    if (!m_BufferPos) {
        nread = 0;
        do {
#ifdef NCBI_COMPILER_MIPSPRO
            // Work around a bug in MIPSPro 7.3's streambuf::xsgetn()
            istream* is = dynamic_cast<istream*>(m_Stream);
            _ASSERT(is);
            is->read(m_Buffer + nread, m_BufferSize - nread);
            long xread = (long) is->gcount();
            is->clear();
#else
            long xread =
                (long) m_Stream->rdbuf()->sgetn(m_Buffer     + nread,
                                                m_BufferSize - nread);
#endif // NCBI_COMPILER_MIPSPRO
            if (xread <= 0) {
                break;
            }
            nread += xread;
        } while (nread < m_BufferSize);
        size_t gap = m_BufferSize - nread;
        if (gap < m_BufferSize) {
            memset(m_Buffer + nread, 0, gap);
        } else {
            return 0/*EOF*/;
        }
    } else {
        nread = m_BufferSize - m_BufferPos;
    }
    if (n > nread) {
        n = nread;
    }
    size_t xpos = m_BufferPos;
    m_BufferPos += ALIGN_SIZE(n);
    m_BufferPos %= m_BufferSize;
    return m_Buffer + xpos;
}


// All partial internal (i.e. in-buffer) block writes are _not_ block-aligned.
void CTar::x_WriteArchive(size_t nwrite, const char* src)
{
    if (!nwrite) {
        return;
    }
    do {
        size_t avail = m_BufferSize - m_BufferPos;
        _ASSERT(avail != 0);
        if (avail > nwrite) {
            avail = nwrite;
        }
        size_t advance = avail;
        if (src) {
            memcpy(m_Buffer + m_BufferPos, src, avail);
            size_t pad = ALIGN_SIZE(avail) - avail;
            memset(m_Buffer + m_BufferPos + avail, 0, pad);
            advance += pad;
        }
        m_BufferPos += advance;
        if (m_BufferPos == m_BufferSize) {
            size_t nwritten = 0;
            do {
                long xwritten =
                    (long) m_Stream->rdbuf()->sputn(m_Buffer     + nwritten,
                                                    m_BufferSize - nwritten);
                if (xwritten <= 0) {
                    int x_errno = errno;
                    TAR_THROW(eWrite,
                              "Archive write failed" + s_OSReason(x_errno));
                }
                nwritten += xwritten;
            } while (nwritten < m_BufferSize);
            m_BufferPos = 0;
        }
        m_StreamPos += advance;
        nwrite -= avail;
    } while (nwrite);
    m_IsModified = true;
}


// PAX (Portable Archive Interchange) extraction support

// Define bitmasks for extended numeric information
typedef enum {
    fPAXNone  = 0,
    fPAXMtime = 1 << 0,
    fPAXAtime = 1 << 1,
    fPAXCtime = 1 << 2,
    fPAXSize  = 1 << 3,
    fPAXUid   = 1 << 4,
    fPAXGid   = 1 << 5
} EPAXBit;
typedef unsigned int TPAXBits;  // Bitwise-OR of EPAXBit(s)


static bool s_ParsePAXInt(Uint8* valp, const char* str, size_t len, bool dot)
{
    char* e;
    errno = 0;
    unsigned long val = strtoul(str, &e, 10);
    if (errno  ||  (size_t)(e - str) > len  ||  (*e != '.'  &&  *e != '\n')) {
        return false;
    }
    if (*e++ == '.') {
        if (!dot) {
            return false;
        }
        strtoul(e, &e, 10);
        if (errno  ||  (size_t)(e - str) != len  ||  *e != '\n') {
            return false;
        }
    }
    *valp = val;
    return true;
}


static bool s_AllLowerCase(const char* str, size_t len)
{
    for (size_t i = 0;  i < len;  i++) {
        if (!islower(str[i]))
            return false;
    }
    return true;
}


CTar::EStatus CTar::x_ParsePAXHeader(CTarEntryInfo& info, const string& buffer)
{
    Uint8 mtime = 0, atime = 0, ctime = 0, size = 0, uid = 0, gid = 0;
    string path, linkpath, uname, gname;
    string* dummy = (string*)(~0UL);
    const struct SPAXParseTable {
        const char* key;
        EPAXBit     bit;
        string*     str;
        Uint8*      val;
    } parser[] = {
        { "mtime",    fPAXMtime, 0,         &mtime },
        { "atime",	  fPAXAtime, 0,         &atime },
        { "ctime",	  fPAXCtime, 0,         &ctime },
        { "size",	  fPAXSize,  dummy,     &size  },
        { "uid",      fPAXUid,   dummy,     &uid   },
        { "gid",      fPAXGid,   dummy,     &gid   },
        { "path",     fPAXNone,  &path,     0      },
        { "linkpath", fPAXNone,  &linkpath, 0      },
        { "uname",    fPAXNone,  &uname,    0      },
        { "gname",    fPAXNone,  &gname,    0      },
        { "comment",  fPAXNone,  0,         0      },
        { "charset",  fPAXNone,  0,         0      }
    };
    const char* str = buffer.c_str();
    TPAXBits parsed = fPAXNone;

    do {
        unsigned long len;
        size_t klen, vlen;
        char *k, *e, *v;

        errno = 0;
        if (!(e = (char*) strchr(str, '\n'))  ||  !(len = strtoul(str, &k, 10))
            ||  str + len - 1 != e  ||  *k++ != ' '
            ||  !(v = (char*) memchr(k, '=', (size_t)(e - k)))
            ||  !(klen = (size_t)(v++ - k))  ||  memchr(k, ' ', klen)
            ||  !(vlen = (size_t)(e - v))) {
            TAR_POST(74, Error, "Skipping malformed PAX header");
            return eFailure;
        }
        bool done = false;
        for (size_t n = 0;  n < sizeof(parser) / sizeof(parser[0]);  n++) {
            if (strlen(parser[n].key) == klen
                &&  strncmp(parser[n].key, k, klen) == 0) {
                if (parser[n].bit == fPAXNone) {
                    if (parser[n].str) {
                        parser[n].str->assign(v, vlen);
                    }
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

    info.m_Name.swap(path);
    info.m_LinkName.swap(linkpath);
    info.m_UserName.swap(uname);
    info.m_GroupName.swap(gname);
    info.m_Stat.st_mtime = (time_t) mtime;
    info.m_Stat.st_atime = (time_t) atime;
    info.m_Stat.st_ctime = (time_t) ctime;
    info.m_Stat.st_size  = /*(?)*/  size;
    info.m_Stat.st_uid   = (uid_t)  uid;
    info.m_Stat.st_gid   = (gid_t)  gid;
    info.m_Pos = parsed;

    return eContinue;
}


static void s_Dump(const string& file, Uint8 pos, size_t recsize,
                   const string* entry, const SHeader* h, ETar_Format fmt,
                   Uint8 datasize)
{
    unsigned long blocks = (unsigned long) BLOCK_OF(datasize + (kBlockSize-1));
    LOG_POST_X(2, s_PositionAsString(file, pos, recsize, entry)
             + s_DumpHeader(h, fmt)
             + (blocks
                ? "Blocks of data: " + NStr::UIntToString(blocks) + '\n'
                : kEmptyStr));
}


CTar::EStatus CTar::x_ReadEntryInfo(CTarEntryInfo& info, bool dump)
{
    // Read block
    const TBlock* block;
    size_t nread = sizeof(block->buffer);
    if (!(block = (const TBlock*) x_ReadArchive(nread))) {
        return eEOF;
    }
    if (nread != kBlockSize) {
        TAR_THROW(eRead, "Unexpected EOF in archive");
    }
    const SHeader* h = &block->header;

    // Check header format
    ETar_Format fmt = eTar_Unknown;
    if (memcmp(h->magic, "ustar", 6) == 0) {
        fmt = eTar_Ustar;
    } else if (memcmp(h->magic, "ustar  ", 8) == 0) {
        // NB: Here, the magic protruded into the adjacent version field
        fmt = eTar_OldGNU;
    } else if (memcmp(h->magic, "\0\0\0\0\0", 6) == 0) {
        fmt = eTar_Legacy;
    } else {
        TAR_THROW_EX(eUnsupportedTarFormat, "Unrecognized format", h, fmt);
    }

    unsigned long value;
    // Get checksum from header
    if (!s_OctalToNum(value, h->checksum, sizeof(h->checksum))) {
        // We must allow all zero bytes here in case of pad/zero blocks
        for (size_t i = 0;  i < sizeof(block->buffer);  i++) {
            if (block->buffer[i]) {
                TAR_THROW_EX(eUnsupportedTarFormat, "Bad checksum", h, fmt);
            }
        }
        return eZeroBlock;
    }
    int checksum = int(value);

    // Compute both signed and unsigned checksums (for compatibility)
    int ssum = 0;
    unsigned int usum = 0;
    const char* p = block->buffer;
    for (size_t i = 0;  i < sizeof(block->buffer);  i++)  {
        ssum +=                 *p;
        usum += (unsigned char)(*p);
        p++;
    }
    p = h->checksum;
    for (size_t j = 0;  j < sizeof(h->checksum);  j++) {
        ssum -=                 *p  - ' ';
        usum -= (unsigned char)(*p) - ' ';
        p++;
    }

    // Compare checksum(s)
    if (checksum != ssum   &&  (unsigned int) checksum != usum) {
        string message = "Header checksum failed";
        if (m_Flags & fDumpBlockHeaders) {
            message += ", expected ";
            if (usum != (unsigned int) ssum) {
                message += "either ";
            }
            message += NStr::UIntToString(usum, 0, 8);
            if (usum != (unsigned int) ssum) {
                message += " or " +
                    NStr::UIntToString((unsigned int) ssum, 0, 8);
            }
        }
        TAR_THROW_EX(eChecksum, message, h, fmt);
    }

    // Set info members now (thus, validating the header block)

    // Name
    if (fmt == eTar_Ustar  &&  h->prefix[0]  &&  h->typeflag[0] != 'x') {
        info.m_Name =
            CDirEntry::ConcatPath(string(h->prefix,
                                         s_Length(h->prefix,
                                                  sizeof(h->prefix))),
                                  string(h->name,
                                         s_Length(h->name,
                                                  sizeof(h->name))));
    } else {
        info.m_Name.assign(h->name, s_Length(h->name, sizeof(h->name)));
    }
    if (!m_Current) {
        m_Current = &info.GetName();
    }

    // Mode
    if (!s_OctalToNum(value, h->mode, sizeof(h->mode))) {
        TAR_THROW_EX(eUnsupportedTarFormat, "Bad entry mode", h, fmt);
    }
    info.m_Stat.st_mode = (mode_t) value;

    // User Id
    if (!s_OctalToNum(value, h->uid, sizeof(h->uid))) {
        TAR_THROW_EX(eUnsupportedTarFormat, "Bad user ID", h, fmt);
    }
    info.m_Stat.st_uid = (uid_t) value;

    // Group Id
    if (!s_OctalToNum(value, h->gid, sizeof(h->gid))) {
        TAR_THROW_EX(eUnsupportedTarFormat, "Bad group ID", h, fmt);
    }
    info.m_Stat.st_gid = (gid_t) value;

    // Size
    if (!s_OctalToNum(value, h->size, sizeof(h->size))) {
        TAR_THROW_EX(eUnsupportedTarFormat, "Bad entry size", h, fmt);
    }
    info.m_Stat.st_size = value;

    // Modification time
    if (!s_OctalToNum(value, h->mtime, sizeof(h->mtime))) {
        TAR_THROW_EX(eUnsupportedTarFormat, "Bad modification time", h, fmt);
    }
    info.m_Stat.st_mtime = value;

    if (fmt == eTar_Ustar  ||  fmt == eTar_OldGNU) {
        // User name
        info.m_UserName.assign(h->uname, s_Length(h->uname, sizeof(h->uname)));

        // Group name
        info.m_GroupName.assign(h->gname, s_Length(h->gname,sizeof(h->gname)));
    }

    if (fmt == eTar_OldGNU) {
        // Name prefix cannot be used because there are times, and other stuff
        // NB: times are valid for incremental archive only, so checks relaxed
        if (!s_OctalToNum(value, h->gt.atime, sizeof(h->gt.atime))) {
            if (memcchr(h->gt.atime, '\0', sizeof(h->gt.atime))) {
                TAR_THROW_EX(eUnsupportedTarFormat,
                             "Bad last access time", h, fmt);
            }
        } else
            info.m_Stat.st_atime = value;
        if (!s_OctalToNum(value, h->gt.ctime, sizeof(h->gt.ctime))) {
            if (memcchr(h->gt.ctime, '\0', sizeof(h->gt.ctime))) {
                TAR_THROW_EX(eUnsupportedTarFormat,
                             "Bad creation time", h, fmt);
            }
        } else
            info.m_Stat.st_ctime = value;
    }

    // Entry type
    switch (h->typeflag[0]) {
    case '\0':
    case '0':
        if (fmt != eTar_Ustar  &&  fmt != eTar_OldGNU) {
            size_t namelen = s_Length(h->name, sizeof(h->name));
            if (namelen  &&  h->name[namelen - 1] == '/') {
                info.m_Type = CTarEntryInfo::eDir;
                info.m_Stat.st_size = 0;
                break;
            }
        }
        info.m_Type = CTarEntryInfo::eFile;
        break;
    case '1':
    case '2':
        info.m_Type = (h->typeflag[0] == '1'
                       ? CTarEntryInfo::eHardLink
                       : CTarEntryInfo::eSymLink);
        info.m_LinkName.assign(h->linkname,
                               s_Length(h->linkname, sizeof(h->linkname)));
        info.m_Stat.st_size = 0;
        break;
    case '5':
        info.m_Type = CTarEntryInfo::eDir;
        info.m_Stat.st_size = 0;
        break;
    case 'x':
    case 'K':
    case 'L':
        if ((h->typeflag[0] == 'x'  &&  fmt == eTar_Ustar)  ||
            (h->typeflag[0] != 'x'  &&  fmt == eTar_OldGNU)) {
            size_t size = (size_t) info.GetSize();
            if (dump) {
                s_Dump(m_FileName, m_StreamPos, m_BufferSize, m_Current,
                       h, fmt, size);
            }
            m_StreamPos += ALIGN_SIZE(nread);
            switch (h->typeflag[0]) {
            case 'x':
                info.m_Type = CTarEntryInfo::ePAXHeader;
                break;
            case 'K':
                info.m_Type = CTarEntryInfo::eGNULongLink;
                break;
            case 'L':
                info.m_Type = CTarEntryInfo::eGNULongName;
                break;
            }
            // Read in the extended information
            string buffer;
            while (size) {
                nread = size;
                const char* xbuf = x_ReadArchive(nread);
                if (!xbuf) {
                    TAR_THROW(eRead,
                              string("Unexpected EOF in ") +
                              (info.GetType() == CTarEntryInfo::ePAXHeader
                               ? "PAX header" :
                               info.GetType() == CTarEntryInfo::eGNULongName
                               ? "long name"
                               : "long link"));
                }
                buffer.append(xbuf, nread);
                m_StreamPos += ALIGN_SIZE(nread);
                size -= nread;
            }
            // Make sure there's no embedded '\0'(s)
            buffer.resize(strlen(buffer.c_str()));
            if (dump) {
                string what(info.GetType() == CTarEntryInfo::ePAXHeader
                            ? "PAX header:\n" :
                            info.GetType() == CTarEntryInfo::eGNULongName
                            ? "Long name:      \""
                            : "Long link name: \"");
                LOG_POST_X(3, what +
                           NStr::PrintableString(buffer,
                                                 info.GetType()
                                                 == CTarEntryInfo::ePAXHeader
                                                 ? NStr::fNewLine_Passthru
                                                 : NStr::fNewLine_Quote) +
                           (info.GetType() == CTarEntryInfo::ePAXHeader ?
                            buffer.size()  &&  buffer[buffer.size()-1] == '\n'
                            ? kEmptyStr : "\n"
                            : "\"\n"));
            }
            // Reset size because the data blocks have been all read
            size = (size_t) info.GetSize();
            info.m_Stat.st_size = 0;
            if (!size  ||  !buffer.size()) {
                TAR_POST(77, Error,
                         "Skipping " + string(size ? "empty" : "zero-sized")
                         + " extended header");
                return eFailure;
            }
            if (info.GetType() == CTarEntryInfo::ePAXHeader) {
                if (size != buffer.size()) {
                    TAR_POST(78, Error,
                             "Skipping truncated ("
                             + NStr::UInt8ToString(info.GetSize()) + "->"
                             + NStr::UInt8ToString((Uint8) buffer.size())
                             + ") PAX header");
                    return eFailure;
                }
                return x_ParsePAXHeader(info, buffer);
            }
            info.m_Name.swap(buffer);
            return eContinue;
        }
        /*FALLTHRU*/
    default:
        info.m_Type = CTarEntryInfo::eUnknown;
        break;
    }

    if (dump) {
        s_Dump(m_FileName, m_StreamPos, m_BufferSize, m_Current,
               h, fmt, info.GetSize());
    }
    m_StreamPos += ALIGN_SIZE(nread);

    return eSuccess;
}


void CTar::x_WriteEntryInfo(const string& name, const CTarEntryInfo& info)
{
    // Prepare block info
    TBlock block;
    memset(block.buffer, 0, sizeof(block.buffer));
    SHeader* h = &block.header;

    CTarEntryInfo::EType type = info.GetType();

    // Name(s) ('\0'-terminated if fits entirely, otherwise not)
    if (!x_PackName(h, info, false)) {
        TAR_THROW(eNameTooLong,
                  "Name '" + info.GetName() + "' too long in"
                  " entry '" + name + '\'');
    }
    if (type == CTarEntryInfo::eSymLink  &&  !x_PackName(h, info, true)) {
        TAR_THROW(eNameTooLong,
                  "Link '" + info.GetLinkName() + "' too long in"
                  " entry '" + name + '\'');
    }

    /* NOTE:  Although some sources on the Internet indicate that
     * all but size, mtime and version numeric fields are '\0'-terminated,
     * we could not confirm that with existing tar programs, all of
     * which we saw using either '\0' or ' '-terminated values in both size
     * and mtime fields.  For ustar archive we have found a document
     * that definitively tells that _all_ numeric fields are '\0'-terminated,
     * and can keep maximum sizeof(field)-1 octal digits.  We follow it here.
     * However, GNU and ustar checksums seem to be different indeed,
     * so we don't use a trailing space for ustar, but for GNU only.
     */

    // Mode
    if (!s_NumToOctal(info.GetMode(), h->mode, sizeof(h->mode) - 1)) {
        TAR_THROW(eMemory, "Cannot store file mode");
    }

    // User ID
    if (!s_NumToOctal(info.GetUserId(), h->uid, sizeof(h->uid) - 1)) {
        TAR_THROW(eMemory, "Cannot store user ID");
    }

    // Group ID
    if (!s_NumToOctal(info.GetGroupId(), h->gid, sizeof(h->gid) - 1)) {
        TAR_THROW(eMemory, "Cannot store group ID");
    }

    // Size
    if (!s_NumToOctal((unsigned long)
                      (type == CTarEntryInfo::eFile ? info.GetSize() : 0),
                      h->size, sizeof(h->size) - 1)) {
        TAR_THROW(eMemory, "Cannot store file size");
    }

    // Modification time
    if (!s_NumToOctal((unsigned long) info.GetModificationTime(),
                      h->mtime, sizeof(h->mtime) - 1)){
        TAR_THROW(eMemory, "Cannot store modification time");
    }

    // Type (GNU extension for SymLink)
    switch (type) {
    case CTarEntryInfo::eFile:
        h->typeflag[0] = '0';
        break;
    case CTarEntryInfo::eSymLink:
        h->typeflag[0] = '2';
        break;
    case CTarEntryInfo::eDir:
        h->typeflag[0] = '5';
        break;
    default:
        TAR_THROW(eUnsupportedEntryType,
                  "Don't know how to store entry '" + name + "' w/type #" +
                  NStr::IntToString(int(type)) + " into archive -- internal"
                  " error, please report!");
    }

    // User and group
    const string& usr = info.GetUserName();
    size_t len = usr.length();
    if (len < sizeof(h->uname)) {
        memcpy(h->uname, usr.c_str(), len);
    }
    const string& grp = info.GetGroupName();
    len = grp.length();
    if (len < sizeof(h->gname)) {
        memcpy(h->gname, grp.c_str(), len);
    }

    // Magic
    strcpy(h->magic,   "ustar");
    // Version (EXCEPTION:  not '\0' terminated)
    memcpy(h->version, "00", 2);

    if (!s_TarChecksum(&block)) {
        TAR_THROW(eMemory, "Cannot store checksum");
    }

    // Write header
    x_WriteArchive(sizeof(block.buffer), block.buffer);
}


bool CTar::x_PackName(SHeader* h, const CTarEntryInfo& info, bool link)
{
    char*      storage = link ? h->linkname         : h->name;
    size_t        size = link ? sizeof(h->linkname) : sizeof(h->name);
    const string& name = link ? info.GetLinkName()  : info.GetName();
    size_t         len = name.length();
    const char*    src = name.c_str();

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
    len++; // write terminating '\0' as it can always be made to fit in
    strcpy(h->name, "././@LongLink");
    s_NumToOctal(0,        h->mode,  sizeof(h->mode) - 1);
    s_NumToOctal(0,        h->uid,   sizeof(h->uid)  - 1);
    s_NumToOctal(0,        h->gid,   sizeof(h->gid)  - 1);
    if (!s_NumToOctal(len, h->size,  sizeof(h->size) - 1)) {
        return false;
    }
    s_NumToOctal(0,        h->mtime, sizeof(h->mtime)- 1);
    h->typeflag[0] = link ? 'K' : 'L';

    // NB: Old GNU magic protrudes into adjacent version field
    memcpy(h->magic, "ustar  ", 8); // 2 spaces and '\0'-terminated

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


void CTar::x_Backspace(EAction action, size_t blocks)
{
    m_Current = 0;
    if (!blocks  ||  (action != eAppend  &&  action != eUpdate)) {
        return;
    }
    if (!m_FileStream) {
        TAR_POST(4, Warning,
                 "In-stream update may result in gapped tar archive");
        return;
    }

    CT_POS_TYPE pos = m_FileStream->tellg();  // Current read position
    if (pos == (CT_POS_TYPE)(-1)) {
        int x_errno = errno;
        TAR_THROW(eRead, "Archive backspace failed" + s_OSReason(x_errno));
    }
    size_t      gap = SIZE_OF(blocks);        // Size of zero-filled area read
    CT_POS_TYPE rec = 0;                      // Record number (0-based)

    if (pos > (CT_POS_TYPE) gap) {
        pos -= 1;                             // NB: pos > 0 here
        rec = pos / m_BufferSize;
        if (!m_BufferPos)
            m_BufferPos = m_BufferSize;
        if (gap > m_BufferPos) {
            gap -= m_BufferPos;
            size_t n = gap / m_BufferSize;
            rec -= (CT_OFF_TYPE) n;           // Backup this many records
            gap %= m_BufferSize;              // Gap size remaining
            m_BufferPos = 0;
            if (gap) {
                rec -= 1;                     // Compensation for partial rec.
                m_FileStream->seekg(rec * m_BufferSize);
                m_StreamPos = (Uint8) rec * m_BufferSize;
                n = kBlockSize;
                x_ReadArchive(n);             // Refetch the record
                m_BufferPos = m_BufferSize - gap;
            }
        } else {
            m_BufferPos -= gap;               // Entirely within this buffer
        }
    } else {
        m_BufferPos = 0;                      // File had nothing but the gap
    }

    m_FileStream->seekp(rec * m_BufferSize);  // Always set put position here
    m_StreamPos = (Uint8) rec * m_BufferSize + m_BufferPos;
}


auto_ptr<CTar::TEntries> CTar::x_ReadAndProcess(EAction action)
{
    auto_ptr<TEntries> done(new TEntries);
    size_t zeroblock_count = 0;
    Uint8 pos = m_StreamPos;
    CTarEntryInfo xinfo;

    xinfo.m_Type = CTarEntryInfo::eUnknown;
    for (;;) {
        // Next block is supposed to be a header
        CTarEntryInfo info(pos);
        m_Current = xinfo.GetName().empty() ? 0 : &xinfo.GetName();
        EStatus status = x_ReadEntryInfo(info, action == eTest
                                         &&  (m_Flags & fDumpBlockHeaders));
        switch (status) {
        case eFailure:
        case eSuccess:
        case eContinue:
            if (zeroblock_count  &&  !(m_Flags & fIgnoreZeroBlocks)) {
                TAR_POST(5, Error, "Interspersing single zero block ignored");
            }
            /*FALLTHRU*/
            break;

        case eZeroBlock:
            m_Current = 0;
            zeroblock_count++;
            if ((m_Flags & fIgnoreZeroBlocks)  ||  zeroblock_count < 2) {
                continue;
            }
            // Two zero blocks -> eEOF
            /*FALLTHRU*/

        case eEOF:
            if (xinfo.GetType() != CTarEntryInfo::eUnknown) {
                TAR_POST(6, Error, "Orphaned extended information ignored");
            }
            x_Backspace(action, zeroblock_count);
            return done;
        }
        zeroblock_count = 0;

        //
        // Process entry
        //
        if (status == eContinue) {
            // Extended header information has been read
            switch (info.GetType()) {
            case CTarEntryInfo::ePAXHeader:
                if (xinfo.GetType() != CTarEntryInfo::eUnknown) {
                    TAR_POST(7, Error, "Unused extended header replaced");
                }
                xinfo.m_Type = CTarEntryInfo::ePAXHeader;
                xinfo.m_Name.swap(info.m_Name);
                xinfo.m_LinkName.swap(info.m_LinkName);
                xinfo.m_UserName.swap(info.m_UserName);
                xinfo.m_GroupName.swap(info.m_GroupName);
                xinfo.m_Stat = info.m_Stat;
                xinfo.m_Pos = info.m_Pos;
                break;

            case CTarEntryInfo::eGNULongName:
                if (xinfo.GetType() == CTarEntryInfo::ePAXHeader
                    ||  !xinfo.GetName().empty()) {
                    TAR_POST(8, Error,
                             "Unused long name '" + xinfo.GetName()
                             + "' replaced");
                }
                // Latch next long name here then just skip
                xinfo.m_Type = CTarEntryInfo::eGNULongName;
                xinfo.m_Name.swap(info.m_Name);
                break;

            case CTarEntryInfo::eGNULongLink:
                if (xinfo.GetType() == CTarEntryInfo::ePAXHeader
                    ||  !xinfo.GetLinkName().empty()) {
                    TAR_POST(9, Error,
                             "Unused long link '" + xinfo.GetLinkName()
                             + "' replaced");
                }
                // Latch next long link here then just skip
                xinfo.m_Type = CTarEntryInfo::eGNULongLink;
                xinfo.m_LinkName.swap(info.m_Name);
                break;

            default:
                NCBI_THROW(CCoreException, eCore, "Internal error");
            }
            continue;
        }

        // Fixup current 'info' with extended information obtained previously
        if (!xinfo.GetName().empty()) {
            xinfo.m_Name.swap(info.m_Name);
            xinfo.m_Name.erase();
        }
        if (!xinfo.GetLinkName().empty()) {
            xinfo.m_LinkName.swap(info.m_LinkName);
            xinfo.m_LinkName.erase();
        }
        if (xinfo.GetType() == CTarEntryInfo::ePAXHeader) {
            TPAXBits parsed = (TPAXBits) xinfo.m_Pos;
            if (!xinfo.GetUserName().empty()) {
                xinfo.m_UserName.swap(info.m_UserName);
                xinfo.m_UserName.erase();
            }
            if (!xinfo.GetGroupName().empty()) {
                xinfo.m_GroupName.swap(info.m_GroupName);
                xinfo.m_GroupName.erase();
            }
            if (parsed & fPAXMtime) {
                info.m_Stat.st_mtime = xinfo.m_Stat.st_mtime;
            }
            if (parsed & fPAXAtime) {
                info.m_Stat.st_atime = xinfo.m_Stat.st_atime;
            }
            if (parsed & fPAXCtime) {
                info.m_Stat.st_ctime = xinfo.m_Stat.st_ctime;
            }
            if (parsed & fPAXSize) {
                info.m_Stat.st_size = xinfo.m_Stat.st_size;
            }
            if (parsed & fPAXUid) {
                info.m_Stat.st_uid = xinfo.m_Stat.st_uid;
            }
            if (parsed & fPAXGid) {
                info.m_Stat.st_gid = xinfo.m_Stat.st_gid;
            }
        }
        xinfo.m_Type = CTarEntryInfo::eUnknown;
        _ASSERT(status == eFailure  ||  status == eSuccess);

        // Match file name with the set of masks
        bool match = (status == eFailure ? false :
                      m_Mask  &&  (action == eList     ||
                                   action == eExtract  ||
                                   action == eInternal)
                      ? m_Mask->Match(info.GetName(),
                                      m_Flags & fMaskNocase
                                      ? NStr::eNocase
                                      : NStr::eCase)
                      : true);

        // NB: match is 'false' when processing a failing entry
        if ((match  &&  action == eInternal)  ||
            x_ProcessEntry(info, match  &&  action == eExtract, done.get())  ||
            (match  &&  !(int(action) & ((eExtract | eInternal) & ~eRW)))) {
            _ASSERT(status == eSuccess);
            done->push_back(info);
            if (action == eInternal) {
                break;
            }
        }

        pos = m_StreamPos;
    }

    m_Current = 0;
    return done;
}


bool CTar::x_ProcessEntry(const CTarEntryInfo& info, bool extract,
                          const CTar::TEntries* done)
{
    Uint8                size = info.GetSize();
    CTarEntryInfo::EType type = info.GetType();

    if (extract) {
        // Destination for extraction
        auto_ptr<CDirEntry> dst
            (CDirEntry::CreateObject(CDirEntry::EType(type),
                                     CDirEntry::NormalizePath
                                     (CDirEntry::ConcatPath
                                      (m_BaseDir, info.GetName()))));
        // Source for extraction
        auto_ptr<CDirEntry> src;

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
            if (done) {
                ITERATE(TEntries, i, *done) {
                    if (i->GetName() == info.GetName()  &&
                        i->GetType() == info.GetType()) {
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
                            &&  dst_time >= info.GetModificationTime()) {
                            extract = false;
                        }
                    }
                    // Have equal types?
                    if (extract  &&  (m_Flags & fEqualTypes)) {
                        if (type == CTarEntryInfo::eHardLink) {
                            src.reset(new CDirEntry(CDirEntry::NormalizePath
                                                    (CDirEntry::ConcatPath
                                                     (m_BaseDir,
                                                      info.GetLinkName()))));
                            if (dst_type != src->GetType())
                                extract = false;
                        } else if (dst_type != CDirEntry::EType(type)) {
                            extract = false;
                        }
                    }
                }
            }
            if (extract) {
                // Need to backup the existing destination?
                if (!found  &&  (m_Flags & fBackup) == fBackup) {
                    CDirEntry dst_tmp(*dst);
                    if (!dst_tmp.Backup(kEmptyStr,
                                        CDirEntry::eBackup_Rename)) {
                        TAR_THROW(eBackup,
                                  "Failed to backup '" + dst->GetPath() +'\'');
                    }
                } else if (type != CTarEntryInfo::eDir
                           ||  (!found  &&  (m_Flags & fUpdate) != fUpdate)) {
                    dst->Remove();
                }
            }
        }
        if (extract) {
            extract = x_ExtractEntry(info, size, dst.get(), src.get());
        }
    }

    while (size) {
        size_t nskip = size < m_BufferSize
            ? (size_t) size : m_BufferSize;
        if (!x_ReadArchive(nskip)) {
            int x_errno = errno;
            TAR_THROW(eRead, "Archive read failed" + s_OSReason(x_errno));
        }
        m_StreamPos += ALIGN_SIZE(nskip);
        size -= nskip;
    }

    return extract;
}


bool CTar::x_ExtractEntry(const CTarEntryInfo& info, Uint8&           size,
                          const CDirEntry*     dst,  const CDirEntry* src)
{
    auto_ptr<CDirEntry> src_ptr;  // deleter
    bool result = true;  // assume best

    switch (info.GetType()) {
    case CTarEntryInfo::eHardLink:
    case CTarEntryInfo::eFile:
        {{
            // Create base directory
            CDir dir(dst->GetDir());
            if (!dir.CreatePath()) {
                int x_errno = errno;
                TAR_THROW(eCreate,
                          "Cannot create directory '" + dir.GetPath() + '\''
                          + s_OSReason(x_errno));
            }

            if (info.GetType() == CTarEntryInfo::eFile) {
                // Create file
                ofstream ofs(dst->GetPath().c_str(),
                             IOS_BASE::out    |
                             IOS_BASE::binary |
                             IOS_BASE::trunc);
                if (!ofs) {
                    int x_errno = errno;
                    TAR_THROW(eCreate,
                              "Cannot create file '" + dst->GetPath() + '\''
                              + s_OSReason(x_errno));
                }

                while (size) {
                    // Read from the archive
                    size_t nread = size < m_BufferSize
                        ? (size_t) size : m_BufferSize;
                    const char* xbuf = x_ReadArchive(nread);
                    if (!xbuf) {
                        TAR_THROW(eRead, "Unexpected EOF");
                    }
                    // Write file to disk
                    if (!ofs.write(xbuf, nread)) {
                        int x_errno = errno;
                        TAR_THROW(eWrite,
                                  "Error writing file '" +dst->GetPath()+ '\''
                                  + s_OSReason(x_errno));
                    }
                    m_StreamPos += ALIGN_SIZE(nread);
                    size -= nread;
                }

                _ASSERT(ofs.good());
                ofs.close();
            } else {
                _ASSERT(size == 0);
                if (!src) {
                    src_ptr.reset(new CDirEntry(CDirEntry::NormalizePath
                                                (CDirEntry::ConcatPath
                                                 (m_BaseDir,
                                                  info.GetLinkName()))));
                    src = src_ptr.get();
                }
#ifdef NCBI_OS_UNIX
                if (link(src->GetPath().c_str(),
                         dst->GetPath().c_str()) == 0) {
                    if (m_Flags & fPreserveAll) {
                        x_RestoreAttrs(info, dst);
                    }
                    break;
                }
                int x_errno = errno;
                TAR_POST(10, Warning,
                         "Cannot hard-link '" + src->GetPath()
                         + "' and '" + dst->GetPath() + '\''
                         + s_OSReason(x_errno) + ", trying to copy");
#endif // NCBI_OS_UNIX
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
                x_RestoreAttrs(info, dst);
            }
        }}
        break;

    case CTarEntryInfo::eDir:
        if (!CDir(dst->GetPath()).CreatePath()) {
            int x_errno = errno;
            TAR_THROW(eCreate,
                      "Cannot create directory '" + dst->GetPath() + '\''
                      + s_OSReason(x_errno));
        }
        // Attributes for directories must be set only when all
        // its files have been already extracted.
        _ASSERT(size == 0);
        break;

    case CTarEntryInfo::eSymLink:
        {{
            CSymLink symlink(dst->GetPath());
            if (!symlink.Create(info.GetLinkName())) {
                int x_errno = errno;
                TAR_POST(12, Error,
                         "Cannot create symlink '" + dst->GetPath()
                         + "' -> '" + info.GetLinkName() + '\''
                         + s_OSReason(x_errno));
                result = false;
            }
            _ASSERT(size == 0);
        }}
        break;

    case CTarEntryInfo::ePAXHeader:
    case CTarEntryInfo::eGNULongName:
    case CTarEntryInfo::eGNULongLink:
        // Extended headers should have already been processed and not be here
        _TROUBLE;
        /*FALLTHRU*/

    default:
        TAR_POST(13, Warning,
                 "Skipping unsupported entry '" + info.GetName()
                 + "' w/type #" + NStr::IntToString(int(info.GetType())));
        result = false;
        break;
    }

    return result;
}


void CTar::x_RestoreAttrs(const CTarEntryInfo& info,
                          const CDirEntry*     dst) const
{
    auto_ptr<CDirEntry> dst_ptr;  // deleter
    if (!dst) {
        dst_ptr.reset(CDirEntry::CreateObject
                      (CDirEntry::EType(info.GetType()),
                       CDirEntry::NormalizePath
                       (CDirEntry::ConcatPath
                        (m_BaseDir, info.GetName()))));
        dst = dst_ptr.get();
    }

    // Date/time.
    // Set the time before permissions because on some platforms
    // this setting can also affect file permissions.
    if (m_Flags & fPreserveTime) {
        time_t modification(info.GetModificationTime());
        time_t last_access(info.GetLastAccessTime());
        time_t creation(info.GetCreationTime());
        if (!dst->SetTimeT(&modification, &last_access, &creation)) {
            int x_errno = errno;
            TAR_THROW(eRestoreAttrs,
                      "Cannot restore date/time for '" + dst->GetPath() + '\''
                      + s_OSReason(x_errno));
        }
    }

    // Owner.
    // This must precede changing permissions because on some
    // systems chown() clears the set[ug]id bits for non-superusers
    // thus resulting in incorrect permissions.
    if (m_Flags & fPreserveOwner) {
        // 2-tier trial:  first using the names, then using numeric IDs.
        // Note that it is often impossible to restore the original owner
        // without super-user rights so no error checking is done here.
        if (!dst->SetOwner(info.GetUserName(),
                           info.GetGroupName(),
                           eIgnoreLinks)) {
            dst->SetOwner(NStr::IntToString(info.GetUserId()),
                          NStr::IntToString(info.GetGroupId()),
                          eIgnoreLinks);
        }
    }

    // Mode.
    // Set them last.
    if (m_Flags & fPreserveMode) {
        bool failed = false;
#ifdef NCBI_OS_UNIX
        // We cannot change permissions for sym.links because lchmod()
        // is not portable and is not implemented on majority of platforms.
        if (info.GetType() != CTarEntryInfo::eSymLink) {
            // Use raw mode here to restore most of bits
            mode_t mode = s_TarToMode(info.m_Stat.st_mode);
            if (chmod(dst->GetPath().c_str(), mode) != 0) {
                // May fail due to setuid/setgid bits -- strip'em and try again
                mode &= ~(S_ISUID | S_ISGID);
                failed = chmod(dst->GetPath().c_str(), mode) != 0;
            }
        }
#else
        CDirEntry::TMode user, group, other;
        CDirEntry::TSpecialModeBits special_bits;
        info.GetMode(&user, &group, &other, &special_bits);
        failed = !dst->SetMode(user, group, other, special_bits);
#endif // NCBI_OS_UNIX
        if (failed) {
            int x_errno = errno;
            TAR_THROW(eRestoreAttrs,
                      "Cannot restore mode bits for '" + dst->GetPath() + '\''
                      + s_OSReason(x_errno));
        }
    }
}


string CTar::x_ToFilesystemPath(const string& name) const
{
    string path(CDirEntry::IsAbsolutePath(name)  ||  m_BaseDir.empty()
                ? name : CDirEntry::ConcatPath(m_BaseDir, name));
    return CDirEntry::NormalizePath(path);
}


string CTar::x_ToArchiveName(const string& path) const
{
    // NB: Path assumed to have been normalized
    string retval = CDirEntry::AddTrailingPathSeparator(path);

#ifdef NCBI_OS_MSWIN
    // Convert to Unix format with forward slashes
    NStr::ReplaceInPlace(retval, "\\", "/");
    const NStr::ECase how = NStr::eNocase;
#else
    const NStr::ECase how = NStr::eCase;
#endif // NCBI_OS_MSWIN

    bool absolute;
    // Remove leading base dir from the path
    if (!m_BaseDir.empty()  &&  NStr::StartsWith(retval, m_BaseDir, how)) {
        if (retval.length() > m_BaseDir.length()) {
            retval.erase(0, m_BaseDir.length()/*separator too*/);
        } else {
            retval.assign(".", 1);
        }
        absolute = false;
    } else {
        absolute = CDirEntry::IsAbsolutePath(retval);
    }

    SIZE_TYPE pos = 0;

#ifdef NCBI_OS_MSWIN
    _ASSERT(!retval.empty());
    // Remove disk name if present
    if (isalpha((unsigned char) retval[0])  &&  retval.find(":") == 1) {
        pos = 2;
    }
#endif // NCBI_OS_MSWIN

    // Remove any leading and trailing slashes
    while (pos < retval.length()  &&  retval[pos] == '/') {
        pos++;
    }
    if (pos) {
        retval.erase(0, pos);
    }
    pos = retval.length();
    while (pos > 0  &&  retval[pos - 1] == '/') {
        --pos;
    }
    if (pos < retval.length()) {
        retval.erase(pos);
    }

    // Check on '..'
    if (retval == ".."  ||  NStr::StartsWith(retval, "../")  ||
        NStr::EndsWith(retval, "/..")  ||  retval.find("/../") != NPOS) {
        TAR_THROW(eBadName,
                  "Name '" + retval + "' embeds parent directory ('..')");
    }

    if (absolute) {
        retval.insert((SIZE_TYPE) 0, 1, '/');
    }
    return retval;
}


auto_ptr<CTar::TEntries> CTar::x_Append(const string&   name,
                                        const TEntries* toc)
{
    auto_ptr<TEntries> entries(new TEntries);

    const EFollowLinks follow_links = (m_Flags & fFollowLinks ?
                                       eFollowLinks : eIgnoreLinks);

    // Compose entry name for relative names
    string path = x_ToFilesystemPath(name);

    // Get direntry information
    CDirEntry entry(path);
    CDirEntry::SStat st;
    if (!entry.Stat(&st, follow_links)) {
        int x_errno = errno;
        TAR_THROW(eOpen,
                  "Cannot get status of '" + path + '\''+ s_OSReason(x_errno));
    }
    CDirEntry::EType type = CDirEntry::GetType(st.orig);

    // Create the entry info
    CTarEntryInfo info(m_StreamPos);
    info.m_Name = x_ToArchiveName(path);
    if (type == CDirEntry::eDir  &&  info.m_Name != "/") {
        info.m_Name += '/';
    }
    if (info.GetName().empty()) {
        TAR_THROW(eBadName, "Empty entry name not allowed");
    }
    m_Current = &info.GetName();
    info.m_Type = CTarEntryInfo::EType(type);
    if (info.GetType() == CTarEntryInfo::eSymLink) {
        _ASSERT(!follow_links);
        info.m_LinkName = entry.LookupLink();
        if (info.GetLinkName().empty()) {
            TAR_THROW(eBadName, "Empty link name not allowed");
        }
    }
    entry.GetOwner(&info.m_UserName, &info.m_GroupName);
    info.m_Stat = st.orig;
    // Fixup for mode bits
    info.m_Stat.st_mode = (mode_t) s_ModeToTar(st.orig.st_mode);

    // Check if we need to update this entry in the archive
    bool update = true;
    if (toc) {
        bool found = false;

        if (type != CDirEntry::eUnknown) {
            // Start searching from the end of the list, to find
            // the most recent entry (if any) first
            string temp;
            REVERSE_ITERATE(TEntries, i, *toc) {
                if (!temp.empty()) {
                    if (i->GetType() == CTarEntryInfo::eHardLink
                        ||  temp != x_ToFilesystemPath(i->GetName())) {
                        continue;
                    }
                } else if (path == x_ToFilesystemPath(i->GetName())) {
                    found = true;
                    if (i->GetType() == CTarEntryInfo::eHardLink) {
                        temp = x_ToFilesystemPath(i->GetLinkName());
                        continue;
                    }
                } else
                    continue;
                if (info.GetType() != i->GetType()) {
                    if (m_Flags & fEqualTypes)
                        goto out;
                } else if (info.GetType() == CTarEntryInfo::eSymLink
                           &&  info.GetLinkName() == i->GetLinkName()) {
                    goto out;
                }
                if (info.GetModificationTime() <= i->GetModificationTime()) {
                    update = false; // same(or older), no update
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

    // Append entry
    switch (type) {
    case CDirEntry::eLink:
        info.m_Stat.st_size = 0;
        /*FALLTHRU*/
    case CDirEntry::eFile:
        _ASSERT(update);
        x_AppendFile(path, info);
        entries->push_back(info);
        break;

    case CDirEntry::eDir:
        if (update) {
            x_WriteEntryInfo(path, info);
            entries->push_back(info);
            m_Current = 0;
        }
        {{
            // Append/Update all files from that directory
            CDir::TEntries dir = CDir(path).GetEntries("*",
                                                       CDir::eIgnoreRecursive);
            ITERATE(CDir::TEntries, i, dir) {
                auto_ptr<TEntries> e = x_Append((*i)->GetPath(), toc);
                entries->splice(entries->end(), *e.get());
            }
        }}
        break;

    case CDirEntry::eUnknown:
        TAR_THROW(eBadName, "Unable to handle '" + path + '\'');
        /*FALLTHRU*/

    default:
        TAR_POST(14, Warning,
                 "Skipping unsupported source '" + path
                 + "' w/type #" + NStr::IntToString(int(type)));
        break;
    }

 out:
    m_Current = 0;
    return entries;
}


// Works for both regular files and symbolic links
void CTar::x_AppendFile(const string& file, const CTarEntryInfo& info)
{
    CNcbiIfstream ifs;

    if (info.GetType() == CTarEntryInfo::eFile) {
        // Open file
        ifs.open(file.c_str(), IOS_BASE::binary | IOS_BASE::in);
        if (!ifs) {
            int x_errno = errno;
            TAR_THROW(eOpen,
                      "Cannot open file '" + file + '\''+ s_OSReason(x_errno));
        }
    }

    // Write file header
    x_WriteEntryInfo(file, info);
    Uint8 size = info.GetSize();

    while (size) {
        // Write file contents
        size_t avail = m_BufferSize - m_BufferPos;
        if (avail > (size_t) size) {
            avail = (size_t) size;
        }
        // Read file
        if (!ifs.read(m_Buffer + m_BufferPos, avail)) {
            int x_errno = errno;
            TAR_THROW(eRead,
                      "Error reading file '" +file+ '\''+ s_OSReason(x_errno));
        }
        avail = (size_t) ifs.gcount();
        // Write buffer to the archive
        x_WriteArchive(avail);
        size -= avail;
    }

    // Write zeros to get the written size a multiple of kBlockSize
    size_t zero = ALIGN_SIZE(m_BufferPos) - m_BufferPos;
    memset(m_Buffer + m_BufferPos, 0, zero);
    x_WriteArchive(zero);
    _ASSERT(!OFFSET_OF(m_BufferPos));
}


class CTarReader : public IReader,
                   protected CTar
{
    friend class CTar;

protected:
    CTarReader(istream& is, size_t blocking_factor);

public:
    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* count);

protected:
    CTarEntryInfo m_Info;
    Uint8         m_Read;
    bool          m_Eof;
    bool          m_Bad;
};


CTarReader::CTarReader(istream&is, size_t blocking_factor)
    : CTar(is, blocking_factor), m_Read(0), m_Eof(false), m_Bad(false)
{
}


ERW_Result CTarReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    _ASSERT(m_BufferSize == kBlockSize);

    if (m_Bad  ||  !count) {
        if (bytes_read)
            *bytes_read = 0;
        return m_Bad ? eRW_Error :
            (m_Read < m_Info.GetSize()  ||  !m_Eof) ? eRW_Success : eRW_Eof;
    }

    size_t read;
    if (m_Read >= m_Info.GetSize()) {
        m_Eof = true;
        read = 0;
    } else {
        if (count > (size_t)(m_Info.GetSize() - m_Read)) {
            count = (size_t)(m_Info.GetSize() - m_Read);
        }
        size_t left = OFFSET_OF(m_Read);
        if (left) {
            read = kBlockSize - left;
            if (read > count) {
                read = count;
            }
        } else if (!x_ReadArchive(count)) {
            read = 0;
            m_Bad = true;
            TAR_THROW(eRead, "Unexpected EOF while streaming");
        } else {
            read = count;
            m_StreamPos += ALIGN_SIZE(read);
        }
        if (!m_Bad) {
            memcpy(buf, m_Buffer + left, read);
            m_Read += read;
        }
    }

    if (bytes_read) {
        *bytes_read = read;
    }
    return m_Bad ? eRW_Error : m_Eof ? eRW_Eof : eRW_Success;
}


ERW_Result CTarReader::PendingCount(size_t* count)
{
    if (m_Bad)
        return eRW_Error;
    Uint8 left = m_Info.GetSize() - m_Read;
    if (!left  &&  m_Eof) {
        return eRW_Eof;
    }
    *count = (size_t) left;
    return eRW_Success;
}


IReader* CTar::Extract(istream& is, const string& name, CTar::TFlags flags)
{
    auto_ptr<CTarReader> retval(new CTarReader(is, 1));

    retval->SetFlags(flags);

    auto_ptr<CMaskFileName> mask(new CMaskFileName);
    mask->Add(name);
    retval->SetMask(mask.release(), eTakeOwnership);

    auto_ptr<TEntries> toc = retval->x_Open(eInternal);
    _ASSERT(toc->size() < 2);
    if (toc->size() < 1) {
        return 0;
    }

    const CTarEntryInfo& info = *toc->begin();
    if (info.GetType() != CTarEntryInfo::eFile) {
        return 0;
    }
    retval->m_Info    = info;
    retval->m_Current = &retval->m_Info.GetName();

    return retval.release();
}


END_NCBI_SCOPE
