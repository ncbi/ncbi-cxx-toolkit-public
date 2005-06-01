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
 *   GNU extensions for long names/links only when unavoidably.
 *   Can handle no exotics like sparse files, devices, etc,
 *   but just regular files, directories, and symbolic links.
 *
 */

#include <ncbi_pch.hpp>
#include <util/compress/tar.hpp>

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "Class CTar can be defined on MS-Windows and UNIX platforms only!"
#endif

#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_system.hpp>
#include <ctype.h>
#include <memory>

#ifdef NCBI_OS_MSWIN
#  include <io.h>
typedef unsigned int mode_t;
typedef short        uid_t;
typedef short        gid_t;
#endif


BEGIN_NCBI_SCOPE


// Convert a number to an octal string padded to the left
// with [leading] zeros ('0') and having _no_ trailing '\0'.
static bool s_NumToOctal(unsigned long value, char* ptr, size_t size)
{
    do {
        ptr[--size] = '0' + char(value & 7);
        value >>= 3;
    } while (size);
    return !value ? true : false;
}


// Convert an octal number (possibly preceded by spaces) to numeric form.
// Stop either at the end of the field or at first '\0' (if any).
static bool s_OctalToNum(unsigned long& value, const char* ptr, size_t size)
{
    _ASSERT(ptr  &&  size > 0);
    bool retval = false;
    size_t i;
    for (i = *ptr ? 0 : 1; i < size  &&  ptr[i]; i++) {
        if (!isspace((unsigned char) ptr[i]))
            break;
    }
    value = 0;
    while (i < size  &&  ptr[i]) {
        if (ptr[i] < '0'  ||  ptr[i] > '7')
            return false;
        retval = true;
        value <<= 3;
        value  |= ptr[i++] - '0';
    }
    return retval;
}


static mode_t s_TarToMode(unsigned int value)
{
    mode_t mode = (
#ifdef S_ISUID
                   (value & TSUID   ? S_ISUID  : 0) |
#endif
#ifdef S_ISGID
                   (value & TSGID   ? S_ISGID  : 0) |
#endif
#ifdef S_ISVTX
                   (value & TSVTX   ? S_ISVTX  : 0) |
#endif
#if   defined(S_IRUSR)
                   (value & TUREAD  ? S_IRUSR  : 0) |
#elif defined(S_IREAD)
                   (value & TUREAD  ? S_IREAD  : 0) |
#endif
#if   defined(S_IWUSR)
                   (value & TUWRITE ? S_IWUSR  : 0) |
#elif defined(S_IWRITE)
                   (value & TUWRITE ? S_IWRITE : 0) |
#endif
#if   defined(S_IWUSR)
                   (value & TUEXEC  ? S_IXUSR  : 0) |
#elif defined(S_IEXEC)
                   (value & TUEXEC  ? S_IEXEC  : 0) |
#endif
#ifdef S_IRGRP
                   (value & TGREAD  ? S_IRGRP  : 0) |
#endif
#ifdef S_IWGRP
                   (value & TGWRITE ? S_IWGRP  : 0) |
#endif
#ifdef S_IXGRP
                   (value & TGEXEC  ? S_IXGRP  : 0) |
#endif
#ifdef S_IROTH
                   (value & TOREAD  ? S_IROTH  : 0) |
#endif
#ifdef S_IWOTH
                   (value & TOWRITE ? S_IWOTH  : 0) |
#endif
#ifdef S_IXOTH
                   (value & TOEXEC  ? S_IXOTH  : 0) |
#endif
                   0);
    return mode;
}


static unsigned int s_ModeToTar(mode_t mode)
{
    /* Foresee that the mode can be extracted on a different platform */
    unsigned int value = (
#ifdef S_ISUID
                           (mode & S_ISUID  ? TSUID   : 0) |
#endif
#ifdef S_ISGID
                           (mode & S_ISGID  ? TSGID   : 0) |
#endif
#ifdef S_ISVTX
                           (mode & S_ISVTX  ? TSVTX   : 0) |
#endif
#if   defined(S_IRUSR)
                           (mode & S_IRUSR  ? TUREAD  : 0) |
#elif defined(S_IREAD)
                           (mode & S_IREAD  ? TUREAD  : 0) |
#endif
#if   defined(S_IWUSR)
                           (mode & S_IWUSR  ? TUWRITE : 0) |
#elif defined(S_IWRITE)
                           (mode & S_IWRITE ? TUWRITE : 0) |
#endif
#if   defined(S_IXUSR)
                           (mode & S_IXUSR  ? TUEXEC  : 0) |
#elif defined(S_IEXEC)
                           (mode & S_IEXEC  ? TUEXEC  : 0) |
#endif
#if   defined(S_IRGRP)
                           (mode & S_IRGRP  ? TGREAD  : 0) |
#elif defined(S_IREAD)
                           /* emulate read permission when file is readable */
                           (mode & S_IREAD  ? TGREAD  : 0) |
#endif
#ifdef S_IWGRP
                           (mode & S_IWGRP  ? TGWRITE : 0) |
#endif
#ifdef S_IXGRP
                           (mode & S_IXGRP  ? TGEXEC  : 0) |
#endif
#if   defined(S_IROTH)
                           (mode & S_IROTH  ? TOREAD  : 0) |
#elif defined(S_IREAD)
                           /* emulate read permission when file is readable */
                           (mode & S_IREAD  ? TOREAD  : 0) |
#endif
#ifdef S_IWOTH
                           (mode & S_IWOTH  ? TOWRITE : 0) |
#endif
#ifdef S_IXOTH
                           (mode & S_IXOTH  ? TOEXEC  : 0) |
#endif
                           0);
    return value;
}


static size_t s_Length(const char* ptr, size_t size)
{
    const char* null = (const char*) memchr(ptr, '\0', size);
    return null ? (size_t)(null - ptr) : size;
}


//////////////////////////////////////////////////////////////////////////////
//
// Constants / macros / typedefs
//

/// Tar block size
static const size_t kBlockSize = 512;

/// Round up to the nearest multiple of kBlockSize
#define ALIGN_SIZE(size) ((((size) + kBlockSize-1) / kBlockSize) * kBlockSize)


/// POSIX tar file header
struct SHeader {        // byte offset
    char name[100];     //   0
    char mode[8];       // 100
    char uid[8];        // 108
    char gid[8];        // 116
    char size[12];      // 124
    char mtime[12];     // 136
    char checksum[8];   // 148
    char typeflag;      // 156
    char linkname[100]; // 157
    char magic[6];      // 257
    char version[2];    // 263
    char uname[32];     // 265
    char gname[32];     // 297
    char devmajor[8];   // 329
    char devminor[8];   // 337
    char prefix[155];   // 345 (not valid with old GNU format)
};                      // 500


/// Block as a header.
union TBlock {
    char    buffer[kBlockSize];
    SHeader header;
};


static void s_TarChecksum(TBlock* block)
{
    SHeader* h = &block->header;

    // Compute the checksum
    memset(h->checksum, ' ', sizeof(h->checksum));
    unsigned long checksum = 0;
    unsigned char* p = (unsigned char*) block->buffer;
    for (size_t i = 0; i < sizeof(block->buffer); i++) {
        checksum += *p++;
    }
    // Checksum (Special: 6 digits, then '\0', then a space [already in place])
    if (!s_NumToOctal((unsigned short) checksum/*cut it!*/,
                      h->checksum, sizeof(h->checksum) - 2)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store checksum");
    }
    h->checksum[6] = '\0';
}


/// Structure with additional info about entries being processed.
/// Each archive action interprets fields in this structure differently.
struct SProcessData {
    CTar::TEntries entries;
    string         dir;
};



//////////////////////////////////////////////////////////////////////////////
//
// CTarEntryInfo
//


unsigned int CTarEntryInfo::GetMode(void) const
{
    // Raw tar mode gets returned here (as kept in the info)
    return (unsigned int) m_Stat.st_mode;
}


void CTarEntryInfo::GetMode(CDirEntry::TMode*            usr_mode,
                            CDirEntry::TMode*            grp_mode,
                            CDirEntry::TMode*            oth_mode
                            /*,CDirEntry::TSpecialModeBits* special_bits*/) const
{
    mode_t mode = s_TarToMode(m_Stat.st_mode);
    // User
    if (usr_mode) {
        *usr_mode = (
#if   defined(S_IRUSR)
                     (mode & S_IRUSR     ? CDirEntry::fRead    : 0) |
#elif defined(S_IREAD)
                     (mode & S_IREAD     ? CDirEntry::fRead    : 0) |
#endif
#if   defined(S_IWUSR)
                     (mode & S_IWUSR     ? CDirEntry::fWrite   : 0) |
#elif defined(S_IWRITE)
                     (mode & S_IWRITE    ? CDirEntry::fWrite   : 0) |
#endif
#if   defined(S_IXUSR)
                     (mode & S_IXUSR     ? CDirEntry::fExecute : 0) |
#elif defined(S_IEXEC)
                     (mode & S_IEXEC     ? CDirEntry::fExecute : 0) |
#endif
                     0);
    }
    // Group
    if (grp_mode) {
        *grp_mode = (
#ifdef S_IRGRP
                     (mode & S_IRGRP     ? CDirEntry::fRead    : 0) |
#endif
#ifdef S_IWGRP
                     (mode & S_IWGRP     ? CDirEntry::fWrite   : 0) |
#endif
#ifdef S_IXGRP
                     (mode & S_IXGRP     ? CDirEntry::fExecute : 0) |
#endif
                     0);
    }
    // Others
    if (oth_mode) {
        *oth_mode = (
#ifdef S_IROTH
                     (mode & S_IROTH     ? CDirEntry::fRead    : 0) |
#endif
#ifdef S_IWOTH
                     (mode & S_IWOTH     ? CDirEntry::fWrite   : 0) |
#endif
#ifdef S_IXOTH
                     (mode & S_IXOTH     ? CDirEntry::fExecute : 0) |
#endif
                     0);
    }
#if 0
    // Special bits
    if (special_bits) {
        *special_bits = (
#ifdef S_ISUID
                         (mode & S_ISUID ? CDirEntry::fSetUID  : 0) |
#endif
#ifdef S_ISGID
                         (mode & S_ISGID ? CDirEntry::fSetGID  : 0) |
#endif
#ifdef S_ISVTX
                         (mode & S_ISVTX ? CDirEntry::fSticky  : 0) |
#endif
                         0);
    }
#endif

    return;
}


static string s_ModeAsString(const CTarEntryInfo& info)
{
    // Take raw tar mode
    unsigned int mode = info.GetMode();

    string usr("---");
    string grp("---");
    string oth("---");
    if (mode & TUREAD) {
        usr[0] = 'r';
    }
    if (mode & TUWRITE) {
        usr[1] = 'w';
    }
    if (mode & TUEXEC) {
        usr[2] = mode & TSUID ? 's' : 'x';
    } else if (mode & TSUID) {
        usr[2] = 'S';
    }
    if (mode & TGREAD) {
        grp[0] = 'r';
    }
    if (mode & TGWRITE) {
        grp[1] = 'w';
    }
    if (mode & TGEXEC) {
        grp[2] = mode & TSGID ? 's' : 'x';
    } else if (mode & TSGID) {
        grp[2] = 'S';
    }
    if (mode & TOREAD) {
        oth[0] = 'r';
    }
    if (mode & TOWRITE) {
        oth[1] = 'w';
    }
    if (mode & TOEXEC) {
        oth[2] = mode & TSVTX ? 't' : 'x';
    } else if (mode & TSVTX) {
        oth[2] = 'T';
    }

    char type;
    switch (info.GetType()) {
    case CTarEntryInfo::eFile:
        type = '-';
        break;
    case CTarEntryInfo::eLink:
        type = 'l';
        break;
    case CTarEntryInfo::eDir:
        type = 'd';
        break;
    default:
        type = '?';
        break;
    }

    return type + usr + grp + oth;
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
    os << s_ModeAsString(info)                            << ' '
       << setw(17) << s_UserGroupAsString(info)           << ' '
       << setw(10) << NStr::UInt8ToString(info.GetSize()) << ' '
       << mtime.ToLocalTime().AsString("Y-M-D h:m:s")     << ' '
       << info.GetName();
    if (info.GetType() == CTarEntryInfo::eLink) {
        os << " -> " << info.GetLinkName();
    }
    os << endl;
    return os;
}


//////////////////////////////////////////////////////////////////////////////
//
// CTar
//

CTar::CTar(const string& file_name, size_t blocking_factor)
    : m_FileName(file_name),
      m_FileStream(new CNcbiFstream),
      m_OpenMode(eUndefined),
      m_Stream(0),
      m_BufferSize(blocking_factor * kBlockSize),
      m_BufferPos(0),
      m_BufPtr(0),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership),
      m_MaskUseCase(NStr::eCase),
      m_IsModified(false)
{
    x_Init();
}


CTar::CTar(CNcbiIos& stream, size_t blocking_factor)
    : m_FileName(kEmptyStr),
      m_FileStream(0),
      m_OpenMode(eUndefined),
      m_Stream(&stream),
      m_BufferSize(blocking_factor * kBlockSize),
      m_BufferPos(0),
      m_BufPtr(0),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership),
      m_MaskUseCase(NStr::eCase),
      m_IsModified(false)
{
    x_Init();
}


CTar::~CTar()
{
    // Close stream(s)
    x_Close();
    if ( m_FileStream ) {
        delete m_FileStream;
        m_FileStream = 0;
    }

    // Delete owned file name masks
    UnsetMask();

    // Delete buffer
    if ( m_BufPtr ) {
        delete[] m_BufPtr;
        m_BufPtr = 0;
    }
    m_Buffer = 0;
}


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
    // m_Buffer is page-aligned
    m_Buffer = m_BufPtr +
        ((((size_t) m_BufPtr + pagemask) & ~pagemask) - (size_t) m_BufPtr);
}


CTar::TEntries CTar::List(void)
{
    SProcessData data;
    x_ReadAndProcess(eList, &data);
    return data.entries;
}


void CTar::x_Close(void)
{
    x_Flush();
    if (m_FileStream  &&  m_FileStream->is_open()) {
        m_FileStream->close();
        m_Stream = 0;
    }
    m_OpenMode = eUndefined;
    m_BufferPos = 0;
}


void CTar::x_Flush(void)
{
    if (!m_Stream || m_OpenMode != eUpdate || !m_IsModified || !m_BufferPos) {
        return;
    }
    size_t pad = m_BufferSize - m_BufferPos;
    // Assure proper blocking factor and pad the archive as necessary
    memset(m_Buffer + m_BufferPos, 0, pad);
    x_WriteArchive(pad);
    if (m_Stream->rdbuf()->PUBSYNC() != 0) {
        NCBI_THROW(CTarException, eWrite, "Archive flush error");
    }
    m_IsModified = false;
}


void CTar::x_Open(EOpenMode mode)
{
    // We can only open a named file here, and if an external stream
    // is being used as an archive, it must be explicitly repositioned by
    // user's code (outside of this class) before each archive operation.
    if ( !m_FileStream ) {
        x_Close();
        if (!m_Stream  ||  !m_Stream->good()  ||  !m_Stream->rdbuf()) {
            NCBI_THROW(CTarException, eOpen,
                       "Cannot open archive from bad IO stream");
        } else {
            m_OpenMode = mode;
        }
        return;
    }

    if (m_OpenMode == mode  ||  (m_OpenMode == eUpdate  &&
                                 mode == eRead          &&
                                 /*FIXME*/!m_IsModified)) {
        if (mode == eRead) {
            x_Flush();
            m_FileStream->seekg(0, IOS_BASE::beg);
            m_BufferPos = 0;
        }
        // Do nothing for eUpdate mode because the file put position
        // is already at the end of file.
    } else {
        // Make sure that the file is properly closed first
        x_Close();

        // Open file using specified mode
        switch (mode) {
        case eCreate:
            m_FileStream->open(m_FileName.c_str(),
                               IOS_BASE::out    |
                               IOS_BASE::binary | IOS_BASE::trunc);
            break;
        case eRead:
            m_FileStream->open(m_FileName.c_str(),
                               IOS_BASE::in     |
                               IOS_BASE::binary);
            break;
        case eUpdate:
            m_FileStream->open(m_FileName.c_str(),
                               IOS_BASE::in     |  IOS_BASE::out  |
                               IOS_BASE::binary);
            if (m_FileStream->seekp(0, IOS_BASE::end)) {
                m_BufferPos = (size_t)(m_FileStream->tellp() % m_BufferSize);
                memset(m_Buffer, 0, m_BufferPos);
            }
            break;
        default:
            _TROUBLE;
            break;
        }
        m_OpenMode = mode;
    }
    // Check result
    if ( !m_FileStream->good() ) {
        NCBI_THROW(CTarException, eOpen,
                   "Cannot open archive '" + m_FileName + '\'');
    } else {
        m_Stream = m_FileStream;
    }
}


void CTar::Update(const string& name)
{
    x_Open(eUpdate);

    // Get list of all entries in the archive
    SProcessData data;
    x_ReadAndProcess(eList, &data, eIgnoreMask);

    // Update entries (recursively for dirs)
    x_Append(name, &data.entries);
}


void CTar::Extract(const string& dst_dir)
{
    SProcessData data;
    data.dir = dst_dir;

    // Extract
    x_ReadAndProcess(eExtract, &data);

    // Restore attributes of "delayed" entries (usually directories)
    ITERATE(TEntries, i, data.entries) {
        x_RestoreAttrs(**i);
    }
}


// Return a pointer to buffer, which is always block-aligned,
// and reflect the number of bytes available via the parameter.
char* CTar::x_ReadArchive(size_t& n)
{
    _ASSERT(n != 0);
    _ASSERT(m_BufferPos % kBlockSize == 0);
    size_t nread;
    if (!m_BufferPos) {
        nread = (size_t) m_Stream->rdbuf()->sgetn(m_Buffer, m_BufferSize);
        if (!nread) {
            return 0/*EOF*/;
        }
        size_t gap = m_BufferSize - nread;
        memset(m_Buffer + nread, 0, gap);
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


// All partial internal block writes are _not_ block-aligned.
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
            if ((size_t) m_Stream->rdbuf()->sputn(m_Buffer, m_BufferSize)
                != m_BufferSize) {
                NCBI_THROW(CTarException, eWrite, "Archive write error");
            }
            m_BufferPos = 0;
        }
        nwrite -= avail;
    } while (nwrite);
    m_IsModified = true;
}


CTar::EStatus CTar::x_ReadEntryInfo(CTarEntryInfo& info)
{
    // Read block
    TBlock* block;
    size_t nread = kBlockSize;
    _ASSERT(sizeof(block->buffer) == nread);
    if (!(block = (TBlock*) x_ReadArchive(nread))) {
        return eEOF;
    }
    if (nread != kBlockSize) {
        NCBI_THROW(CTarException, eRead, "Unexpected EOF in archive");
    }
    SHeader* h = &block->header;

    bool ustar = false, oldgnu = false;
    // Check header format
    if (memcmp(h->magic, "ustar", 6) == 0) {
        ustar = true;
    } else if (memcmp(h->magic, "ustar  ", 8) == 0) {
        // NB: Here, the magic protruded into the adjacent version field
        oldgnu = true;
    } else if (memcmp(h->magic, "\0\0\0\0\0", 6) != 0) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat,
                   "Unknown archive format");
    }

    // Get checksum from header
    unsigned long value;
    if (!s_OctalToNum(value, h->checksum, sizeof(h->checksum))) {
        // We must allow all zero bytes here in case of pad/zero blocks
        for (size_t i = 0; i < sizeof(block->buffer); i++) {
            if (block->buffer[i]) {
                NCBI_THROW(CTarException,eUnsupportedTarFormat,"Bad checksum");
            }
        }
        return eZeroBlock;
    }
    int checksum = value;

    // Compute both signed and unsigned checksums (for compatibility)
    int ssum = 0;
    unsigned int usum = 0;
    char* p = block->buffer;
    memset(h->checksum, ' ', sizeof(h->checksum));
    for (size_t i = 0; i < sizeof(block->buffer); i++)  {
        ssum += *p;
        usum += (unsigned char) *p;
        p++;
    }

    // Compare checksum(s)
    if (checksum != ssum   &&  (unsigned int) checksum != usum) {
        NCBI_THROW(CTarException, eChecksum, "Header checksum failed");
    }

    // Set info members now

    // Name
    if (ustar  &&  !oldgnu  &&  h->prefix[0]) {
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

    // Mode
    if (!s_OctalToNum(value, h->mode, sizeof(h->mode))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat, "Bad file mode");
    }
    // Quietly strip unknown bits
    info.m_Stat.st_mode = (mode_t)(value & TMODEMASK);

    // User Id
    if (!s_OctalToNum(value, h->uid, sizeof(h->uid))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat, "Bad user ID");
    }
    info.m_Stat.st_uid = (uid_t) value;

    // Group Id
    if (!s_OctalToNum(value, h->gid, sizeof(h->gid))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat, "Bad group ID");
    }
    info.m_Stat.st_gid = (gid_t) value;

    // Size
    if (!s_OctalToNum(value, h->size, sizeof(h->size))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat, "Bad file size");
    }
    info.m_Stat.st_size = value;

    // Modification time
    if (!s_OctalToNum(value, h->mtime, sizeof(h->mtime))) {
        NCBI_THROW(CTarException, eUnsupportedTarFormat,
                   "Bad modification time");
    }
    info.m_Stat.st_mtime = value;

    // Entry type
    switch (h->typeflag) {
    case '\0':
    case '0':
        if (ustar  ||  oldgnu) {
            info.m_Type = CTarEntryInfo::eFile;
        } else {
            size_t name_size = s_Length(h->name, sizeof(h->name));
            if (!name_size  ||  h->name[name_size - 1] != '/') {
                info.m_Type = CTarEntryInfo::eFile;
            } else {
                info.m_Type = CTarEntryInfo::eDir;
                info.m_Stat.st_size = 0;
            }
        }
        break;
    case '2':
        info.m_Type = CTarEntryInfo::eLink;
        info.m_LinkName.assign(h->linkname,
                               s_Length(h->linkname, sizeof(h->linkname)));
        info.m_Stat.st_size = 0;
        break;
    case '5':
        if (ustar  ||  oldgnu) {
            info.m_Type = CTarEntryInfo::eDir;
            info.m_Stat.st_size = 0;
            break;
        }
        /*FALLTHRU*/
    case 'K':
    case 'L':
        if (oldgnu) {
            info.m_Type = h->typeflag == 'K'
                ? CTarEntryInfo::eGNULongLink
                : CTarEntryInfo::eGNULongName;
            // Read the long name in
            info.m_Name.erase();
            size_t size = (streamsize) info.GetSize();
            while (size) {
                size_t nread = size;
                char* xbuf = x_ReadArchive(nread);
                if (!xbuf) {
                    NCBI_THROW(CTarException, eRead,
                               "Unexpected EOF while reading long name");
                }
                info.m_Name.append(xbuf, nread);
                size -= nread;
            }
            info.m_Stat.st_size = 0;
            // NB: Input buffers may be trashed, so return right here
            return eSuccess;
        }
        /*FALLTHRU*/
    default:
        info.m_Type = CTarEntryInfo::eUnknown;
        break;
    }

    if (ustar  ||  oldgnu) {
        // User name
        info.m_UserName.assign(h->uname, s_Length(h->uname, sizeof(h->uname)));

        // Group name
        info.m_GroupName.assign(h->gname, s_Length(h->gname,sizeof(h->gname)));
    }

    return eSuccess;
}


void CTar::x_WriteEntryInfo(const string& name, const CTarEntryInfo& info)
{
    // Prepare block info
    TBlock block;
    _ASSERT(sizeof(block.buffer) == kBlockSize);
    memset(block.buffer, 0, sizeof(block.buffer));
    SHeader* h = &block.header;

    CTarEntryInfo::EType type = info.GetType();

    // Name(s) ('\0'-terminated if fit, otherwise not)
    if (!x_PackName(h, info, false)) {
        NCBI_THROW(CTarException, eNameTooLong,
                   "Name '" + info.GetName() + "' too long in"
                   " entry '" + name + '\'');
    }
    if (type == CTarEntryInfo::eLink  &&  !x_PackName(h, info, true)) {
        NCBI_THROW(CTarException, eNameTooLong,
                   "Link '" + info.GetLinkName() + "' too long in"
                   " entry '" + name + '\'');
    }

    // Mode ('\0'-terminated)
    if (!s_NumToOctal(info.m_Stat.st_mode, h->mode, sizeof(h->mode) - 1)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store file mode");
    }

    // User ID ('\0'-terminated)
    if (!s_NumToOctal(info.m_Stat.st_uid, h->uid, sizeof(h->uid) - 1)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store user ID");
    }

    // Group ID ('\0'-terminated)
    if (!s_NumToOctal(info.m_Stat.st_gid, h->gid, sizeof(h->gid) - 1)) {
        NCBI_THROW(CTarException, eMemory, "Cannot store group ID");
    }

    // Size (not '\0'-terminated)
    if (!s_NumToOctal((unsigned long)
                      (type == CTarEntryInfo::eFile ? info.GetSize() : 0),
                      h->size, sizeof(h->size))) {
        NCBI_THROW(CTarException, eMemory, "Cannot store file size");
    }

    // Modification time (not '\0'-terminated)
    if (!s_NumToOctal(info.GetModificationTime(), h->mtime, sizeof(h->mtime))){
        NCBI_THROW(CTarException, eMemory, "Cannot store modification time");
    }

    // Type (GNU extension for SymLink)
    switch (type) {
    case CDirEntry::eFile:
        h->typeflag = '0';
        break;
    case CDirEntry::eLink:
        h->typeflag = '2';
        break;
    case CDirEntry::eDir:
        h->typeflag = '5';
        break;
    default:
        NCBI_THROW(CTarException, eUnsupportedEntryType,
                   "Don't know how to store entry type " +
                   NStr::IntToString(int(type)) + " in archive");
        break;
    }

    // User and group names (must be '\0'-terminated)
    string user, group;
    if ( CDirEntry(name).GetOwner(&user, &group, eIgnoreLinks) ) {
        if (user.length() < sizeof(h->uname)) {
            strcpy(h->uname, user.c_str());
        }
        if (group.length() < sizeof(h->gname)) {
            strcpy(h->gname, group.c_str());
        }
    }

    // Magic ('\0'-terminated)
    strcpy(h->magic,   "ustar");
    // Version (not '\0' terminated)
    memcpy(h->version, "00", 2);

    s_TarChecksum(&block);

    // Write header
    x_WriteArchive(sizeof(block.buffer), block.buffer);
}


bool CTar::x_PackName(SHeader* h, const CTarEntryInfo& info, bool link)
{
    char*      storage = link ? h->linkname         : h->name;
    size_t        size = link ? sizeof(h->linkname) : sizeof(h->name);
    const string& name = link ? info.GetLinkName( ) : info.GetName();
    size_t         len = name.length();

    if (len <= size) {
        // Name fits!
        memcpy(storage, name.c_str(), len);
        return true;
    }

    if (!link  &&  len <= sizeof(h->prefix) + 1 + sizeof(h->name)) {
        // Try to split the long name into a prefix and a short name (POSIX)
        size_t i = len;
        if (i > sizeof(h->prefix)) {
            i = sizeof(h->prefix);
        }
        while (i > 0  &&  !CDirEntry::IsPathSeparator(name[--i])) {
            if (len - i > sizeof(h->name))
                break;
        }
        if (i  &&  len - i - 1  &&  len - i <= sizeof(h->name)) {
            memcpy(&h->prefix, name.c_str(),         i);
            memcpy(&h->name,   name.c_str() + i + 1, len - i - 1);
            return true;
        }
    }

    // Still, store the initial part in the original header
    memcpy(storage, name.c_str(), size);

    // Prepare extended block header with the long name info (GNU style)
    _ASSERT(m_BufferPos % kBlockSize == 0  &&  m_BufferPos < m_BufferSize);
    TBlock* block = (TBlock*)(m_Buffer + m_BufferPos);
    _ASSERT(sizeof(block->buffer) == kBlockSize);
    memset(block->buffer, 0, sizeof(block->buffer));
    h = &block->header;

    // See above for comments about header filling
    strcpy(h->name, "././@LongLink");
    s_NumToOctal(0,        h->mode,  sizeof(h->mode) - 1);
    s_NumToOctal(0,        h->uid,   sizeof(h->uid)  - 1);
    s_NumToOctal(0,        h->gid,   sizeof(h->gid)  - 1);
    if (!s_NumToOctal(len, h->size,  sizeof(h->size))) {
        return false;
    }
    s_NumToOctal(0,        h->mtime, sizeof(h->mtime));
    h->typeflag = link ? 'K' : 'L';

    // NB: Old GNU magic protrudes into adjacent verion field
    memcpy(h->magic, "ustar  ", 8); // 2 spaces and '\0'-terminated

    s_TarChecksum(block);

    // Write the header
    x_WriteArchive(sizeof(block->buffer));

    // Store the full name in the extended block
    AutoPtr< char, ArrayDeleter<char> > buf_ptr(new char[len]);
    storage = buf_ptr.get();
    memcpy(storage, name.c_str(), len);

    // Write the extended block (will be aligned as necessary)
    x_WriteArchive(len, storage);

    return true;
}


void CTar::x_ReadAndProcess(EAction action, SProcessData* data, EMask use_mask)
{
    // Open file for reading (won't affect unmodified eUpdate)
    x_Open(eRead);

    string nextLongName, nextLongLink;
    bool prev_zeroblock = false;

    for (;;) {
        // Next block supposed to be a header
        CTarEntryInfo info;
        EStatus status = x_ReadEntryInfo(info);
        switch (status) {
        case eEOF:
            return;
        case eZeroBlock:
            // FIXME
            // Skip zero blocks
            if (m_Flags & fIgnoreZeroBlocks) {
                continue;
            }
            // Check previous status
            if (prev_zeroblock) {
                // Two zero blocks -> eEOF
                return;
            }
            prev_zeroblock = true;
            continue;
        case eSuccess:
            // processed below
            break;
        default:
            NCBI_THROW(CCoreException, eCore, "Unknown error");
            break;
        }
        prev_zeroblock = false;

        //
        // Process entry
        //
        switch (info.GetType()) {
        case CTarEntryInfo::eGNULongName:
            // Latch next long name here then just skip
            nextLongName = info.GetName();
            continue;
        case CTarEntryInfo::eGNULongLink:
            // Latch next long link here then just skip
            nextLongLink = info.GetName();
            continue;
        default:
            // Otherwise process the entry as usual
            break;
        }

        // Correct 'info' if long names have been previously defined
        if (!nextLongName.empty()) {
            info.m_Name = nextLongName;
            nextLongName.erase();
        }
        if (!nextLongLink.empty()) {
            if (info.GetType() == CTarEntryInfo::eLink) {
                info.m_LinkName = nextLongLink;
            }
            nextLongLink.erase();
        }

        // Match file name by set of masks
        bool match = true;
        if (action != eTest  &&  use_mask == eUseMask  &&  m_Mask) {
            match = m_Mask->Match(info.GetName(), m_MaskUseCase);
        }

        // Process
        switch (action) {
        case eList:
            // Save entry info
            if ( match ) {
                data->entries.push_back(new CTarEntryInfo(info));
            }
            // Skip the entry
            x_ProcessEntry(info, false, eList);
            break;

        case eExtract:
            // Extract an entry (if)
            x_ProcessEntry(info, match, eExtract, data);
            break;

        case eTest:
            // Test the entry integrity
            x_ProcessEntry(info, true, eTest);
            break;

        default:
            _TROUBLE;
            break;
        }
    }
}


void CTar::x_ProcessEntry(const CTarEntryInfo& info, bool process,
                          EAction action, SProcessData* data)
{
    // Remaining entry size in the archive
    streamsize size;

    if (process  &&  action == eExtract) {
        _ASSERT(data);
        size = x_ExtractEntry(info, *data);
    } else {
        size = (streamsize) info.GetSize();
    }

    while (size) {
        size_t nskip = size;
        if (!x_ReadArchive(nskip)) {
            NCBI_THROW(CTarException, eRead, "Archive read error");
        }
        size -= nskip;
    }
}


streamsize CTar::x_ExtractEntry(const CTarEntryInfo& info, SProcessData& data)
{
    bool extract = true;

    CTarEntryInfo::EType type = info.GetType();
    streamsize           size = (streamsize) info.GetSize();

    // Destination for extraction
    auto_ptr<CDirEntry>
        dst(CDirEntry::CreateObject(CDirEntry::EType(type),
                                    CDir::ConcatPath(data.dir,
                                                     info.GetName())));
    // Dereference sym.link if requested
    if (type != CTarEntryInfo::eLink  &&  (m_Flags & fFollowLinks)) {
        dst->DereferenceLink();
    }

    // Look if backup is necessary
    if (dst->Exists()) {
        // Can overwrite it?
        if (!(m_Flags & fOverwrite)) {
            // Entry already exists, and cannot be changed
            extract = false;
        } else if ((m_Flags & fUpdate)  &&
                   dst->IsNewer(info.GetModificationTime())) {
            extract = false;
        } else if ((m_Flags & fEqualTypes)  &&
                   CDirEntry::EType(type) != dst->GetType()) {
            extract = false;
        } else if (m_Flags & fBackup) {
            // Backup destination entry
            try {
                if (!CDirEntry(*dst).Backup(kEmptyStr,
                                            CDirEntry::eBackup_Rename)) {
                    NCBI_THROW(CTarException, eBackup,
                               "Cannot backup existing destination '" +
                               dst->GetPath() + '\'');
                } else if (m_Flags & fOverwrite) {
                    dst->Remove();
                }
            } catch (...) {
                extract = false;
            }
        }
    }

    if (!extract) {
        // Full size of the entry to skip
        return size;
    }

    // Really extract the entry
    switch (info.GetType()) {
    case CTarEntryInfo::eFile:
        {{
            // Create base directory
            CDir dir(dst->GetDir());
            if (!dir.CreatePath()) {
                NCBI_THROW(CTarException, eCreate,
                           "Cannot create directory '" + dir.GetPath() + '\'');
            }
            // Create file
            ofstream file(dst->GetPath().c_str(),
                          IOS_BASE::out | IOS_BASE::binary | IOS_BASE::trunc);
            if ( !file ) {
                NCBI_THROW(CTarException, eCreate,
                           "Cannot create file '" + dst->GetPath() + '\'');
            }

            while (size) {
                // Read from the archive
                size_t nread = size;
                char* xbuf = x_ReadArchive(nread);
                if (!xbuf) {
                    NCBI_THROW(CTarException, eRead, "Cannot extract '"
                               + info.GetName() + "' from archive");
                }
                // Write file to disk
                if (!file.write(xbuf, nread)) {
                    NCBI_THROW(CTarException, eWrite,
                               "Error writing file '" + dst->GetPath() + '\'');
                }
                size -= nread;
            }

            _ASSERT(file.good());
            file.close();

            if (m_Flags & fPreserveAll) {
                // Restore attributes
                x_RestoreAttrs(info, dst.get());
            }
        }}
        break;

    case CTarEntryInfo::eDir:
        if (!CDir(dst->GetPath()).CreatePath()) {
            NCBI_THROW(CTarException, eCreate,
                       "Cannot create directory '" + dst->GetPath() + '\'');
        }
        // Attributes for directories must be set only when all
        // its files have been already extracted. At this point add
        // the directory to the waiting list to set the attributes later.
        if (m_Flags & fPreserveAll) {
            AutoPtr<CTarEntryInfo> waiting(new CTarEntryInfo(info));
            waiting->m_Name = dst->GetPath();
            data.entries.push_back(waiting);
        }
        _ASSERT(size == 0);
        break;

    case CTarEntryInfo::eLink:
        {{
            CSymLink symlink(dst->GetPath());
            if (!symlink.Create(info.GetLinkName())) {
                ERR_POST(Error << "Cannot create symlink '" + dst->GetPath()
                         + "' -> '" + info.GetLinkName() + '\'');
            }
            _ASSERT(size == 0);
        }}
        break;

    case CTarEntryInfo::eGNULongName:
    case CTarEntryInfo::eGNULongLink:
        // Long name/link -- already processed and should not be here
        _ASSERT(0);
        break;

    default:
        ERR_POST(Warning << "Skipping unsupported entry '" +
                 info.GetName() + "' of type " + NStr::IntToString(int(type)));
        break;
    }

    return size;
}


void CTar::x_RestoreAttrs(const CTarEntryInfo& info, CDirEntry* dst)
{
    auto_ptr<CDirEntry> dst_ptr;
    if (!dst) {
        dst_ptr.reset(CDirEntry::CreateObject(CDirEntry::EType(info.GetType()),
                                              info.GetName()));
        dst = dst_ptr.get();
    }

    // Date/time.
    // Set the time before permissions because on some platforms
    // this setting can also affect file permissions.
    if (m_Flags & fPreserveTime) {
        time_t modification = info.GetModificationTime();
        if ( !dst->SetTimeT(&modification) ) {
            NCBI_THROW(CTarException, eRestoreAttrs,
                       "Cannot restore date/time for '" +
                       dst->GetPath() + '\'');
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
        if ( info.GetType() != CTarEntryInfo::eLink ) {
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
#endif /*NCBI_OS_UNIX*/
        if (failed) {
            NCBI_THROW(CTarException, eRestoreAttrs,
                       "Cannot restore file mode for '" +
                       dst->GetPath() + '\'');
        }
    }
}


string CTar::x_ToArchiveName(const string& path) const
{
    string retval = CDirEntry::NormalizePath(path, eIgnoreLinks);

    // Remove leading base dir from the path
    if (!m_BaseDir.empty()  &&  NStr::StartsWith(retval, m_BaseDir)) {
        retval.erase(0, m_BaseDir.length()/*separator too*/);
    }
    if (retval.empty()) {
        retval.assign(".");
    }

    // Check on '..'
    if (retval.find("..") != NPOS) {
        NCBI_THROW(CTarException, eBadName, "Entry name contains '..'");
    }

    // Convert to Unix format with forward slashes
    retval = NStr::Replace(retval, "\\", "/");

    SIZE_TYPE pos = 0;
    // Remove disk name if present
    if (isalpha(retval[0])  &&  retval.find(":") == 1) {
        pos = 2;
    }

    // Remove leading and trailing slashes
    while (pos < retval.length()  &&  retval[pos] == '/') {
        ++pos;
    }
    if (pos) {
        retval.erase(0, pos);
    }
    if (retval.length()) {
        pos = retval.length() - 1;
        while (pos >= 0  &&  retval[pos] == '/') {
            --pos;
        }
        retval.erase(pos + 1);
    }

    return retval;
}


void CTar::x_Append(const string& entry_name, const TEntries* update_list)
{
    // Compose entry name for relative names
    string name;
    if (CDirEntry::IsAbsolutePath(entry_name)  ||  m_BaseDir.empty()) {
        name = entry_name;
    } else {
        name = CDirEntry::ConcatPath(GetBaseDir(), entry_name);
    }
    EFollowLinks follow_links = m_Flags & fFollowLinks
        ? eFollowLinks : eIgnoreLinks;

    // Get dir entry information
    CDir entry(name);
    CDirEntry::SStat st;
    if (!entry.Stat(&st, follow_links)) {
        NCBI_THROW(CTarException, eOpen,
                   "Cannot get status information on '" + entry_name + "'");
    }
    CDirEntry::EType type = entry.GetType();

    // Create the entry info
    CTarEntryInfo info;
    string x_name = x_ToArchiveName(name);
    info.m_Name = type == CDirEntry::eDir ? x_name + '/' : x_name;
    if (info.GetName().empty()) {
        NCBI_THROW(CTarException, eBadName, "Empty entry name not allowed");
    }
    info.m_Type = CTarEntryInfo::EType(type);
    if (info.GetType() == CTarEntryInfo::eLink) {
        _ASSERT(!follow_links);
        info.m_LinkName = entry.LookupLink();
        if (info.GetLinkName().empty()) {
            NCBI_THROW(CTarException, eBadName, "Empty link name not allowed");
        }
    }
    entry.GetOwner(&info.m_UserName, &info.m_GroupName);
    info.m_Stat = st.orig;
    // Fixup for mode bits
    info.m_Stat.st_mode = (mode_t)(s_ModeToTar(st.orig.st_mode) & TMODEMASK);

    // Check if we need to update this entry in the archive
    bool update_directory = true;
    if ( update_list ) {
        bool found = false;
        const CTarEntryInfo* x_info;

        // Start searching from the end of the list, to find
        // the most recently updated entry (if any) first
        REVERSE_ITERATE(CTar::TEntries, i, *update_list) {
            x_info = (*i).get();
            if (info.GetName() == x_info->GetName()  &&
                (info.GetType() == x_info->GetType()  ||
                 !(m_Flags & fEqualTypes))) {
                found = true;
                break;
            }
        }

        if (found) {
            if (!entry.IsNewer(x_info->GetModificationTime())) {
                if (type == CDirEntry::eDir) {
                    update_directory = false;
                } else {
                    return;
                }
            }
        } else {
            update_directory = false;
        }
    }

    // Append entry
    switch (type) {
    case CDirEntry::eLink:
        info.m_Stat.st_size = 0;
        /*FALLTHRU*/
    case CDirEntry::eFile:
        x_AppendFile(name, info);
        break;

    case CDirEntry::eDir:
        {{
            if (update_directory) {
                x_WriteEntryInfo(name, info);
            }
            // Append/Update all files from that directory
            CDir::TEntries dir = entry.GetEntries("*", CDir::eIgnoreRecursive);
            ITERATE(CDir::TEntries, i, dir) {
                x_Append((*i)->GetPath(), update_list);
            }
        }}
        break;

    case CDirEntry::eUnknown:
        NCBI_THROW(CTarException, eBadName, "Cannot find '" + name + '\'');
        break;

    default:
        ERR_POST(Warning << "Skipping unsupported source '" + name + '\'');
        break;
    }
}


// Works for both regular files and symbolic links
void CTar::x_AppendFile(const string& file_name, const CTarEntryInfo& info)
{
    CNcbiIfstream ifs;

    if (info.GetType() == CTarEntryInfo::eFile) {
        // Open file
        ifs.open(file_name.c_str(), IOS_BASE::binary | IOS_BASE::in);
        if ( !ifs ) {
            NCBI_THROW(CTarException, eOpen,
                       "Cannot open file '" + file_name + "'");
        }
    }

    // Write file header
    x_WriteEntryInfo(file_name, info);
    streamsize size = (streamsize) info.GetSize();

    while (size) {
        // Write file contents
        size_t avail = m_BufferSize - m_BufferPos;
        if (avail > (size_t) size) {
            avail = (size_t) size;
        }
        // Read file
        if (!ifs.read(m_Buffer + m_BufferPos, avail)) {
            NCBI_THROW(CTarException, eRead,
                       "Error reading file '" + file_name + "'");
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
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2005/06/01 20:04:31  lavr
 * Use older eBackup_Rename enum (not newer one which is not yet in CVS)
 *
 * Revision 1.20  2005/06/01 19:58:58  lavr
 * Fix previous "fix" of getting page size
 * Move tar permission bits to the header; some cosmetics
 *
 * Revision 1.19  2005/05/31 14:09:40  ivanov
 * CTar::x_Init(): use GetVirtualMemoryPageSize().
 * Ger rid of compilation warnings on MSVC.
 *
 * Revision 1.18  2005/05/30 21:04:59  ucko
 * Use erase() rather than clear() on strings for compatibility with GCC 2.95.
 *
 * Revision 1.17  2005/05/30 15:28:22  lavr
 * Comments reviewed, proper blocking factor fully implemented, other patches
 *
 * Revision 1.16  2005/05/27 21:12:55  lavr
 * Revert to use of std::ios as a main I/O stream (instead of std::iostream)
 *
 * Revision 1.15  2005/05/27 14:14:27  lavr
 * Fix MS-Windows compilation problems and heed warnings
 *
 * Revision 1.14  2005/05/27 13:55:45  lavr
 * Major revamp/redesign/fix/improvement/extension of this API
 *
 * Revision 1.13  2005/05/05 13:41:42  ivanov
 * Added const to parameters in private methods
 *
 * Revision 1.12  2005/05/05 12:40:11  ivanov
 * Fixed GCC compilation warning
 *
 * Revision 1.11  2005/05/05 12:32:45  ivanov
 * + CTar::Update()
 *
 * Revision 1.10  2005/04/27 13:53:02  ivanov
 * Added support for (re)storing permissions/owner/times
 *
 * Revision 1.9  2005/02/03 14:01:21  rsmith
 * must initialize m_StreamPos since streampos may not have a default cnstr.
 *
 * Revision 1.8  2005/01/31 20:53:09  ucko
 * Use string::erase() rather than string::clear(), which GCC 2.95
 * continues not to support.
 *
 * Revision 1.7  2005/01/31 15:31:22  ivanov
 * Lines wrapped at 79th column
 *
 * Revision 1.6  2005/01/31 15:09:55  ivanov
 * Fixed include path to tar.hpp
 *
 * Revision 1.5  2005/01/31 14:28:51  ivanov
 * CTar:: rewritten Extract() methods using universal x_ReadAndProcess().
 * Added some private auxiliary methods to support Create(), Append(), List(),
 * and Test() methods.
 *
 * Revision 1.4  2004/12/14 17:55:58  ivanov
 * Added GNU tar long name support
 *
 * Revision 1.3  2004/12/07 15:29:40  ivanov
 * Fixed incorrect file size for extracted files
 *
 * Revision 1.2  2004/12/02 18:01:13  ivanov
 * Comment out unused seekpos()
 *
 * Revision 1.1  2004/12/02 17:49:16  ivanov
 * Initial draft revision
 *
 * ===========================================================================
 */
