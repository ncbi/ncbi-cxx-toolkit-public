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
 *
 * File Description:  TAR archive API
 *
 */

#include <ncbi_pch.hpp>
#include <util/compress/tar.hpp>
#include <memory>

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "Class CTar defined only for MS Windows and UNIX platforms"
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_system.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  endif


BEGIN_NCBI_SCOPE


// Convert number to octal string
void s_NumToOctal(unsigned long value, char* ptr, size_t size)
{
    memset(ptr, ' ', size);
    do {
        ptr[--size] = '0' + char(value & 7);
        value >>= 3;
    } while (size);
}

// Macro for s_NumToOctal() calls simplification 
#define NUM_TO_OCTAL(value, field) \
    s_NumToOctal((unsigned long)value, field, sizeof(field)-1);



//////////////////////////////////////////////////////////////////////////////
//
// Constants / macro / typedefs
//

/// TAR block size
static const streamsize kBlockSize = 512;

/// Default buffer size (must be multiple kBlockSize)
static const streamsize kDefaultBufferSize = 16*1024;

// Special chars
static const char kNull  = '\0';
static const char kSlash = '/';

/// Macro to compute size that is multiple to kBlockSize
#define ALIGN_BLOCK_SIZE(size) \
    if ( size % kBlockSize != 0 ) { \
        size = (size / kBlockSize + 1) * kBlockSize; \
    }

/// Macro to check that all mask bits are set in flags
#define F_ISSET(mask) ((GetFlags() & (mask)) == (mask))

/// Macro to check that at least one of mask bits is set in flags
#define F_ISSET_ONEOF(mask) ((GetFlags() & (mask)) > 0)


/// TAR POSIX file header
struct SHeader {          // byte offset
    char name[100];       //   0
    char mode[8];         // 100
    char uid[8];          // 108
    char gid[8];          // 116
    char size[12];        // 124
    char mtime[12];       // 136
    char checksum[8];     // 148
    char typeflag;        // 156
    char linkname[100];   // 157
    char magic[6];        // 257
    char version[2];      // 263
    char uname[32];       // 265
    char gname[32];       // 297
    char devmajor[8];     // 329
    char devminor[8];     // 337
    char prefix[155];     // 345
};                        // 500

/// Block to read
union TBlock {
  char     buffer[kBlockSize];
  SHeader  header;
};

/// Name field size
static const size_t kNameFieldSize   = 100;
/// Name prefix field size
static const size_t kPrefixFieldSize = 155;
/// User/group name field size
static const size_t kAccountFieldSize = 32;


//////////////////////////////////////////////////////////////////////////////
//
// CTarEntryInfo
//

void CTarEntryInfo::GetMode(CDirEntry::TMode* user_mode,
                            CDirEntry::TMode* group_mode,
                            CDirEntry::TMode* other_mode) const
{
    unsigned int mode = (unsigned int)m_Stat.st_mode;
    // Other
    if (other_mode) {
        *other_mode = mode & 0007;
    }
    mode >>= 3;
    // Group
    if (group_mode) {
        *group_mode = mode & 0007;
    }
    mode >>= 3;
    // User
    if (user_mode) {
        *user_mode = mode & 0007;
    }
    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// CTar
//

CTar::CTar(const string& file_name)
    : m_FileName(file_name),
      m_Stream(0),
      m_FileStreamMode(eUndefined),
      m_StreamPos(0),
      m_BufferSize(kDefaultBufferSize),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership),
      m_MaskUseCase(NStr::eCase)
{
    // Create new file stream
    m_FileStream = new CNcbiFstream();
    if ( !m_FileStream ) {
        NCBI_THROW(CTarException, eMemory, "Cannot create file stream");
    }
    // Allocate I/O bufer
    m_Buffer = new char[m_BufferSize];
    if ( !m_Buffer ) {
        NCBI_THROW(CTarException, eMemory,
                   "Cannot allocate memory for I/O buffer");
    }
    return;
}


CTar::CTar(CNcbiIos& stream)
    : m_FileName(kEmptyStr),
      m_Stream(&stream),
      m_FileStream(0),
      m_FileStreamMode(eUndefined),
      m_StreamPos(0),
      m_BufferSize(kDefaultBufferSize),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership),
      m_MaskUseCase(NStr::eCase)
{
    // Allocate I/O bufer
    m_Buffer = new char[m_BufferSize];
    if ( !m_Buffer ) {
        NCBI_THROW(CTarException, eMemory,
                   "Cannot allocate memory for I/O buffer");
    }
    return;
}


CTar::~CTar()
{
    // Close open streams
    x_Close();
    if ( m_FileStream ) {
        delete m_FileStream;
        m_FileStream = 0;
    }

    // Delete owned file name masks
    UnsetMask();

    // Delete buffer
    if ( m_Buffer ) {
        delete m_Buffer;
    }
    return;
}


void CTar::Update(const string& entry_name)
{
    // Get list of all entries in the archive
    SProcessData data;
    x_ReadAndProcess(eList, &data, eIgnoreMask);
    
    // Update entries (recursively for dirs)
    x_Open(eUpdate);
    x_Append(entry_name, &data.entries);
}


void CTar::Extract(const string& dst_dir)
{
    SProcessData data;
    data.dir = dst_dir;
    // Extract 
    x_ReadAndProcess(eExtract, &data);
    // Restore attributes for "delayed" entries (usually for directories)
    ITERATE(TEntries, i, data.entries) {
        x_RestoreAttrs(**i);
    }
}


void CTar::x_Open(EOpenMode mode)
{
    m_StreamPos = 0;

    // We should open only file streams,
    // all external streams must be reposition outside,
    // before any operations under archive.
    if ( !m_FileStream ) {
        return;
    }

    if ( m_FileStreamMode == mode  && 
         (mode == eRead  || mode == eUpdate) ) {
        // Reset stream postion
        if ( mode == eRead ) {
            m_FileStream->seekg(0, IOS_BASE::beg);
        }
        // Do nothing for eUpdate mode, because the file position
        // already in the end of file
    } else {
        // For sure that the file is closed
        x_Close();

        // Open file using specified mode
        switch(mode) {
        case eCreate:
            m_FileStream->open(m_FileName.c_str(),
                               IOS_BASE::binary | IOS_BASE::out |
                               IOS_BASE::trunc);
            break;
        case eRead:
            m_FileStream->open(m_FileName.c_str(),
                               IOS_BASE::binary | IOS_BASE::in);
            break;
        case eUpdate:
            m_FileStream->open(m_FileName.c_str(),
                               IOS_BASE::binary | IOS_BASE::in |
                               IOS_BASE::out);
            m_FileStream->seekg(0, IOS_BASE::end);
            m_StreamPos = m_FileStream->tellg();
            break;
        default:
            _TROUBLE;
        }
        m_FileStreamMode = mode;
        m_Stream = m_FileStream;
    }

    // Check result
    if ( !m_Stream->good() ) {
        NCBI_THROW(CTarException, eOpen,
                   "Unable to open archive '" + m_FileName + "'");
    }
}


void CTar::x_Close(void)
{
    // If we have file stream...
    if ( m_FileStream ) {
        if ( m_FileStream->is_open() ) {
            m_FileStream->close();
        }
    }
    m_Stream = 0;
}


CTar::EStatus CTar::x_ReadEntryInfo(CTarEntryInfo& info)
{
    // Read block
    TBlock block;
    streamsize nread = x_ReadStream(block.buffer, kBlockSize);
    if ( !nread ) {
        return eEOF;
    }   
    if ( nread != kBlockSize ) {
        NCBI_THROW(CTarException, eRead, "Unexpected EOF in archive");
    }
    SHeader* h = &block.header;

    // Get checksum from header
    int checksum = 0;
    if ( h->checksum[0] ) {
        checksum = NStr::StringToInt(NStr::TruncateSpaces(h->checksum), 8,
                                     NStr::eCheck_Skip);
    }
    // Compute real checksum
    if ( checksum != 0 ) {
        memset(h->checksum, ' ', sizeof(h->checksum));
    }
    int sum = 0;
    char* p = block.buffer;
    for (int i = 0; i< kBlockSize; i++)  {
      sum += (unsigned char) *p;
      p++;
    }
    if ( sum == 0 ) {
        // Empty block found
        return eZeroBlock;
    }

    // Compare checksums
    if ( checksum != sum ) {
        NCBI_THROW(CTarException, eCRC, "Header CRC check failed");
    }
    // Check file format
    if ( memcmp(h->magic, "ustar", 5) != 0 ) {
        NCBI_THROW(CTarException, eUnsupportedEntryType,
                   "Unknown archive format");
    }

    // Mode 
    info.SetMode(NStr::StringToUInt(NStr::TruncateSpaces(h->mode),
                                    8, NStr::eCheck_Skip));

    // Name is null-terminated unless every character is non-null.
    h->mode[0] = kNull;
    if ( h->prefix[0] ) {
        info.SetName(CDirEntry::ConcatPath(h->prefix, h->name));
    } else {
        info.SetName(h->name);
    }
    // Size
    info.SetSize(NStr::StringToInt8(NStr::TruncateSpaces(h->size),
                                    8, NStr::eCheck_Skip));

    // Modification time.
    // This field is terminated with a space only.
    info.SetModificationTime(NStr::StringToULong(
        NStr::TruncateSpaces(h->mtime), 8, NStr::eCheck_Skip));

    // Analize entry type 
    switch(h->typeflag) {
        case kNull:
        case '0':
            info.SetType(CTarEntryInfo::eFile);
            break;
        case '1':
            info.SetType(CTarEntryInfo::eLink);
            break;
        case '5':
            info.SetType(CTarEntryInfo::eDir);
            break;
        case 'L':
            info.SetType(CTarEntryInfo::eGNULongName);
            {{
                // Read long name
                streamsize name_size = (streamsize)info.GetSize();
                ALIGN_BLOCK_SIZE(name_size);
                streamsize n = x_ReadStream(m_Buffer, name_size);
                if ( n != name_size ) {
                    NCBI_THROW(CTarException, eRead,
                               "Unexpected EOF in archive");
                }
                // Save name
                m_LongName.assign(m_Buffer, (SIZE_TYPE)info.GetSize()); 
            }}
            break;
        case 'K':
            info.SetType(CTarEntryInfo::eGNULongLink);
            break;
        default:
            info.SetType(CTarEntryInfo::eUnknown);
    }

    // The link name field is valid only if "type" field is eLink
    // It is null-terminated unless every character is non-null.
    if ( info.GetType() == CTarEntryInfo::eLink ) {
        h->magic[0] = kNull;
        info.SetLinkName(h->linkname);
    }

    // User Id
    info.SetUserId(NStr::StringToUInt(NStr::TruncateSpaces(h->uid),
                                      8, NStr::eCheck_Skip));
    // Group Id
    info.SetGroupId(NStr::StringToUInt(NStr::TruncateSpaces(h->gid),
                                       8, NStr::eCheck_Skip));
    // User name
    info.SetUserName(h->uname);

    // Group name
    info.SetGroupName(h->gname);

    return eSuccess;
}


void CTar::x_WriteEntryInfo(const string& entry_name, CTarEntryInfo& info)
{
    // Prepare block info
    TBlock block;
    memset(&block, 0, sizeof(block));
    SHeader* h = &block.header;

    CTarEntryInfo::EType type = info.GetType();
    string iname = info.GetName();

    // --- Name ---
    if ( iname.length() <= kNameFieldSize ) {
        memcpy(&h->name, iname.c_str(), iname.length());
    } else {
        if ( iname.length() > kNameFieldSize + kPrefixFieldSize ) {
            NCBI_THROW(CTarException, eLongName,
                       "Entry name '" + entry_name + "' is to long");
        }
        // Split long name to prefix and short name
        size_t i = iname.length();
        if ( i > kPrefixFieldSize ) {
            i = kPrefixFieldSize;
        }
        while ( i > 0  &&  iname[i] != kSlash ) i--;
        if ( !i  ||  iname.length() - i > kNameFieldSize ) {
            NCBI_THROW(CTarException, eLongName,
                       "Entry name '" + entry_name + "' is to long");
        }
        memcpy(&h->name, iname.c_str() + i + 1, iname.length() - i - 1);
        memcpy(&h->prefix, iname.c_str(), i);
    }

    // --- Mode ---
    NUM_TO_OCTAL(info.m_Stat.st_mode, h->mode);

    // --- User ID ---
    NUM_TO_OCTAL(info.m_Stat.st_uid, h->uid);

    // --- Group ID ---
    NUM_TO_OCTAL(info.m_Stat.st_gid, h->gid);

    // --- Size ---
    unsigned long size = 0;
    if ( type == CDirEntry::eFile ) {
        size = info.m_Stat.st_size;
    }
    NUM_TO_OCTAL(size, h->size);

    // --- Modification time ---
    NUM_TO_OCTAL(info.m_Stat.st_mtime, h->mtime);

    // --- Typeflag ---
    switch(type) {
        case CDirEntry::eFile:
            h->typeflag = '0'; 
            break;
        case CDirEntry::eLink:
            h->typeflag = '1';
            break;
        case CDirEntry::eDir:
            h->typeflag = '5';
            break;
        default:
            ;
    }

    // --- Link name ---
    // not supported yet

    // --- Magic ---
    strcpy(h->magic, "ustar");

    // --- Version ---
    strncpy(h->version, "00", 2);

    // --- User name ---
    // --- Group name ---
    string user, group;
    if ( CDirEntry(entry_name).GetOwner(&user, &group, eIgnoreLinks) ) {
        if ( user.length() <= kAccountFieldSize ) {
            memcpy(&h->uname, user.c_str(), user.length());
        }
        if ( group.length() <= kAccountFieldSize ) {
            memcpy(&h->gname, group.c_str(), group.length());
        }
    }

    // --- Device ---
    // not supported yet

    // --- Check sum ---
    memset(h->checksum, ' ', sizeof(h->checksum));
    int checksum = 0;
    char* p = block.buffer;
    for (int i = 0; i< kBlockSize; i++)  {
      checksum += (unsigned char) *p;
      p++;
    }
    // Checksumm field have 6 digits, a null, and a space
    s_NumToOctal((unsigned long)checksum, h->checksum, 6);
    h->checksum[6] = kNull;

    // Write header
    x_WriteStream(block.buffer, kBlockSize);
}


void CTar::x_AddEntryInfoToList(const CTarEntryInfo& info,
                                TEntries& entries) const
{
    CTarEntryInfo* pinfo = new CTarEntryInfo(info);
    if ( !pinfo ) {
        NCBI_THROW(CTarException, eMemory,
                    "CTar::x_AddEntryInfoToList(): " \
                    "Cannot allocate memory for entry info");
    }
    entries.push_back(pinfo);
}


void CTar::x_ReadAndProcess(EAction action, SProcessData* data, EMask use_mask)
{
    // Open file for reading
    x_Open(eRead);
    bool prev_zeroblock = false;

    for(;;) {
        // Next block supposed to be a header
        CTarEntryInfo info;
        EStatus status = x_ReadEntryInfo(info);
        switch(status) {
            case eEOF:
                return;
            case eZeroBlock:
                // Skip zero blocks
                if ( F_ISSET(fIgnoreZeroBlocks) ) {
                    continue;
                }
                // Check previous status
                if ( prev_zeroblock ) {
                    // eEOF
                    return;
                }
                prev_zeroblock = true;
                continue;
            case eFailure:
                NCBI_THROW(CCoreException, eCore, "Unknown error");
            case eSuccess:
                ; // processed below
        }
        prev_zeroblock = false;

        //
        // Process entry
        //

        // Some preprocess work
        switch(info.GetType()) {

            // Correct 'info' if long names are defined
            case CTarEntryInfo::eFile:
            case CTarEntryInfo::eDir:
                if ( !m_LongName.empty() ) {
                    info.SetName(m_LongName);
                    m_LongName.erase();
                }
                if ( !m_LongLinkName.empty() ) {
                    info.SetLinkName(m_LongLinkName);
                    m_LongLinkName.erase();
                }
                break;

            // Do not process some dummy entries.
            // They already processed in x_ReadEntryInfo(), just skip.
            case CTarEntryInfo::eGNULongName:
            case CTarEntryInfo::eGNULongLink:
                continue;

            // Otherwise -- nothing to do
            default:
                break;
        }

        // Match file name by set of masks
        bool matched = true;
        if ( use_mask == eUseMask  &&  m_Mask ) {
            matched = m_Mask->Match(info.GetName(), m_MaskUseCase);
        }

        // Process
        switch(action) {
            case eList:
                // Save entry info
                if ( matched ) {
                    x_AddEntryInfoToList(info, data->entries);
                }
                // Skip entry contents
                x_ProcessEntry(info, false, eList);
                break;

            case eExtract:
                // Create entry
                x_ProcessEntry(info, matched, eExtract, data);
                break;

            case eTest:
                // Test entry integrity
                x_ProcessEntry(info, matched, eTest);
                break;
        }
    }
}


void CTar::x_ProcessEntry(CTarEntryInfo& info, bool do_process, EAction action,
                          SProcessData* data)
{
    // Next variables have a value only when extracting is possible.
    string dst_path;
    auto_ptr<CDirEntry> dst;

    //
    // Check on existent destination entry
    //

    if ( do_process  &&  action == eExtract ) {
        _ASSERT(data);
        dst_path = CDir::ConcatPath(data->dir, info.GetName());
        dst.reset(CDirEntry::CreateObject(
            CDirEntry::EType(info.GetType()), dst_path));

        // Dereference link
        if ( info.GetType() != CTarEntryInfo::eLink ) {
            if ( F_ISSET(fFollowLinks) ) {
                dst->DereferenceLink();
                dst_path = dst->GetPath();
            }
        }
        CDirEntry::EType dst_type = dst->GetType();
        bool dst_exists = (dst_type != CDirEntry::eUnknown);
        if ( dst_exists )  {
            try {
                // Can overwrite it?
                if ( !F_ISSET(fOverwrite) ) {
                    // Entry already exists, and cannot be changed
                    throw(0);
                }
                // Can update?
                if ( F_ISSET(fUpdate) ) {
                    time_t dst_mtime;
                    if ( !dst->GetTimeT(&dst_mtime)  ||
                         dst_mtime >= info.GetModificationTime()) {
                        throw(0);
                    }
                }
                // Backup destination entry first
                if ( F_ISSET(fBackup) ) {
                    if ( !CDirEntry(*dst).Backup(kEmptyStr,
                                                 CDirEntry::eBackup_Rename)) {
                        NCBI_THROW(CTarException, eBackup,
                                   "Backup existing entry '" +
                                   dst->GetPath() + "' failed");
                    }
                }
                // Overwrite destination entry
                if ( F_ISSET(fOverwrite) ) {
                    dst->Remove();
                } 
            } catch (int) {
                // Just skip current extracting entry
                do_process = false;
            }
        }
    }

    //
    // Read/skip entry content
    //

    auto_ptr<CNcbiOfstream> os;
    bool need_restore_attrs = false;

    // Read entry contents
    switch(info.GetType()) {

        // File
        case CTarEntryInfo::eFile:
            {{
            // Get size of entry rounded up by kBlockSize
            streamsize aligned_size = (streamsize)info.GetSize();
            ALIGN_BLOCK_SIZE(aligned_size);

            // Just skip current entry if possible
            if ( m_FileStream  &&  !do_process ) {
                m_FileStream->seekg(aligned_size, IOS_BASE::cur);
                break;
            }
            // Otherwise, read it
            if ( do_process  &&  action == eExtract ) {
                // Create base directory
                string base_dir = dst->GetDir();
                CDir dir(base_dir);
                if ( !dir.CreatePath() ) {
                    NCBI_THROW(CTarException, eCreate,
                               "Unable to create directory '" +
                               base_dir + "'");
                }
                // Create output file
                auto_ptr<CNcbiOfstream> osp(
                   new CNcbiOfstream(dst_path.c_str(),
                                     IOS_BASE::out | IOS_BASE::binary));
                os = osp;
                if ( !*os  ||  !os->good() ) {
                    NCBI_THROW(CTarException, eCreate,
                               "Unable to create file '" + dst_path + "'");
                }
            }
            // Read file content
            streamsize to_read  = aligned_size;
            streamsize to_write = (streamsize)info.GetSize();

            while (m_Stream->good()  &&  to_read > 0) {
                // Read archive
                streamsize x_nread = (to_read > m_BufferSize) ? m_BufferSize :
                                                                to_read;
                streamsize nread = x_ReadStream(m_Buffer, x_nread);
                if ( !nread ) {
                    NCBI_THROW(CTarException, eRead, "Cannot read TAR file '"+
                               info.GetName() + "'");
                }
                to_read -= nread;

                // Write file to disk
                if (do_process  &&  action == eExtract) {
                    if (to_write > 0) {
                        os->write(m_Buffer, min(nread, to_write));
                        if ( !*os ) {
                            NCBI_THROW(CTarException, eWrite,
                                       "Cannot write file '" + dst_path +"'");
                        }
                        to_write -= nread;
                    }
                }
            }
            // Clean up
            if ( do_process  &&  action == eExtract  &&  *os) {
                os->close();
                // Enable to set attrs for entry
                need_restore_attrs = F_ISSET_ONEOF(fPreserveAll);
            }
            }}
            break;

        // Directory
        case CTarEntryInfo::eDir:
            if ( do_process  &&  action == eExtract ) {
                if ( !CDir(dst_path).CreatePath() ) {
                    NCBI_THROW(CTarException, eCreate,
                               "Unable to create directory '" + dst_path+"'");
                }
                // Attributes for directories must be set only when all
                // its files already extracted. Now add it to the waiting
                // list to set attributes later.
                if ( F_ISSET_ONEOF(fPreserveAll) ) {
                    info.SetName(dst_path);
                    x_AddEntryInfoToList(info, data->entries);
                }
            }
            break;
            
        // Long name/link -- already processed in x_ReadEntryInfo(), just skip
        case CTarEntryInfo::eGNULongName:
        case CTarEntryInfo::eGNULongLink:
            break;

        // Link
        case CTarEntryInfo::eLink:
        // Other
        default:
            ERR_POST(Warning << "Unsupported entry type. Skipped.");
    }

    // Restore attributes
    if ( need_restore_attrs ) {
        x_RestoreAttrs(info, dst.get());
    }
}


void CTar::x_RestoreAttrs(const CTarEntryInfo& info, CDirEntry* target)
{
    string dst_path;
    CDirEntry* dst = 0;
    auto_ptr<CDirEntry> dst_ptr;

    if ( target ) {
        dst_path = target->GetPath();
        dst = target;    
    } else {
        dst_path = info.GetName();
        dst_ptr.reset(CDirEntry::CreateObject(
                                 CDirEntry::EType(info.GetType()), dst_path));
        dst = dst_ptr.get();
    }

    // Date/time.
    // Set the time before permissions because on some platforms setting
    // time can affect file permissions also.
    if ( F_ISSET(fPreserveTime) ) {
        time_t modification = info.GetModificationTime();
        if ( !dst->SetTimeT(&modification) ) {
            NCBI_THROW(CTarException, eRestoreAttrs,
                        "Restore date/time for '" + dst_path + "' failed");
        }
    }

    // Owner
    // Change owner must precede changing permissions because on some
    // systems chown clears the set[ug]id bits for non-superusers,
    // resulting in incorrect permissions.
    if ( F_ISSET(fPreserveOwner) ) {
        // We dont check result here, because often is not impossible
        // to save an original owner name without super-user rights.
        dst->SetOwner(info.GetUserName(), info.GetGroupName(), eIgnoreLinks);
    }

    // Permissions.
    // Set it after times and owner.
    if ( F_ISSET(fPreservePerm) ) {
#if defined(NCBI_OS_MSWIN)
        int mode = (int)info.GetMode();
#else
        mode_t mode = info.GetMode();
#endif
        if ( info.GetType() == CTarEntryInfo::eLink ) {
            // We cannot change permissions for sym.links.
            // lchmod() is not portable and doesn't implemented on some
            // platforms. So, do nothing.
        } else {
            if ( chmod(dst_path.c_str(), mode) != 0 ) {
                NCBI_THROW(CTarException, eRestoreAttrs,
                    "Restore permissions for '" + dst_path + "' failed");
            }
        }
    }
}


string CTar::x_ToArchiveName(const string& path) const
{
    // Assumes that "path" already in Unix format with "/" slashes.

    // Remove leading base dir from the path
    string conv_path;
    if ( !m_BaseDir.empty()  &&  NStr::StartsWith(path, m_BaseDir)) {
        conv_path = path.substr(m_BaseDir.length());
    } else {
        conv_path = path;
    }

    // Check on '..'
    SIZE_TYPE pos = conv_path.find("..");
    if ( pos != NPOS ) {
        NCBI_THROW(CTarException, eBadName, "Entry name contains '..'");
    }

    // Convert to Unix format with '/' slashes
    conv_path = NStr::Replace(conv_path, "\\", "/");

    // Remove disk name if present
    pos = conv_path.find(":");
    pos = ( pos == NPOS ) ? 0 : pos + 1;

    // Remove leading and trailing slashes
    while (pos < conv_path.length()  &&  conv_path[pos] == kSlash)
        ++pos;
    if ( pos ) {
        conv_path.erase(0, pos);
    }
    if ( conv_path.length() ) {
        pos = conv_path.length() - 1;
        while (pos >= 0  &&  conv_path[pos] == kSlash)
            --pos;
        conv_path.erase(pos+1);
    }
    return conv_path;
}


void CTar::x_Append(const string& entry_name, TEntries* update_list)
{
    // Compose entry name for relative names
    string x_name;
    if ( CDirEntry::IsAbsolutePathEx(entry_name)  ||  m_BaseDir.empty()) {
        x_name = entry_name;
    } else {
        x_name = GetBaseDir() + entry_name;
    }

    // Get dir entry information
    CDir entry(x_name);
    CTarEntryInfo info;
    CDirEntry::SStat st;
    if ( entry.Stat(&st, eIgnoreLinks) != 0 ) {
        NCBI_THROW(CTarException, eOpen,
                   "Unable to get status information for '" +
                   entry_name + "'");
    }
    info.m_Stat = st.orig;
    CDirEntry::EType type = entry.GetType();
    info.SetType((CTarEntryInfo::EType)type);

    // Get internal archive name for entry
    string iname = x_ToArchiveName(x_name);
    if ( iname.empty() ) {
        NCBI_THROW(CTarException, eBadName, "Empty entry name not allowed");
    }
    if ( type == CDirEntry::eDir ) {
        iname += kSlash;
    }
    info.SetName(iname);

    // Check if we need to update entry in the archive
    bool write_dir_info = true;
    if ( update_list ) {
        // Find entry among the archive entries
        bool found = false;
        CTarEntryInfo x_info;

        // Start search from the end of the list, to find
        // already updated entry first
        REVERSE_ITERATE(CTar::TEntries, i, *update_list) {
            x_info = **i;
            if ( iname == x_info.GetName() ) {
                found = true;
                break;
            }
        }

        // If found, that compare archive entry with dir entry
        if ( found ) {
            if ( type == CDirEntry::eDir ) {
                // We can skip to update information about directory itself,
                // but should check all its entries.
                write_dir_info = (st.orig.st_mtime > x_info.GetModificationTime());
            } else {
                // Check modification time
                if ( st.orig.st_mtime <= x_info.GetModificationTime() ) {
                    // Don't need to update
                    return;
                }
            }
        }
    }

    // Append entry
    switch (type) {
        case CDirEntry::eFile:
            {{
                info.SetType(CTarEntryInfo::eFile);
                x_AppendFile(x_name, info);
            }}
            break;
        case CDirEntry::eDir:
            {{
                // Append directory info
                if ( write_dir_info ) {
                    info.SetType(CTarEntryInfo::eDir);
                    x_WriteEntryInfo(x_name, info);
                }
                // Append all files from that directory
                CDir::TEntries contents = 
                    entry.GetEntries("*", CDir::eIgnoreRecursive);
                ITERATE(CDir::TEntries, i, contents) {
                    string name = (*i)->GetPath();
                    x_Append(name, update_list);
                }
            }}
            break;
        case CDirEntry::eUnknown:
            NCBI_THROW(CTarException, eBadName,
                       "Cannot find '" + entry_name + "'");
        default:
            ERR_POST(Warning << "Unsupported entry type of '" +
                     entry_name + "'. Skipped.");
//            NCBI_THROW(CTarException, eUnsupportedEntryType,
//                       "Unsupported entry type of '" + entry_name + "'");
    }
}


void CTar::x_AppendFile(const string& file_name, CTarEntryInfo& info)
{
    // Open file
    CNcbiIfstream is;
    is.open(file_name.c_str(), IOS_BASE::binary | IOS_BASE::in);
    if ( !is ) {
        NCBI_THROW(CTarException, eOpen,
                   "Unable to open file '" + file_name + "'");
    }

    // Write file header
    x_WriteEntryInfo(file_name, info);

    streamsize file_size = (streamsize)info.GetSize();
    streamsize to_read = file_size;

    // Write file contents
    while ( is.good()  &&  to_read > 0 ) {
        // Read file
        streamsize x_nread = (to_read > m_BufferSize) ? m_BufferSize :to_read;
        is.read(m_Buffer, x_nread);
        streamsize nread = is.gcount();
        to_read -= nread;

        // Write buffer to TAR archive
        x_WriteStream(m_Buffer, nread);
    }
    // 
    if ( to_read > 0 ) {
        NCBI_THROW(CTarException, eRead,
                   "Error reading file '" + file_name + "'");
    }

    // Write zeros to have a written size be multiple of kBlockSize
    streamsize zero_size = kBlockSize - file_size % kBlockSize;
    memset(m_Buffer, 0, zero_size);
    x_WriteStream(m_Buffer, zero_size);

    // Close input file
    is.close();
}

/*
    /// Write EOF marker to archive.
    ///
    /// Many TAR implementations don't need such marker, but some require it.
    void AppendFinish(void);

void CTar::AppendFinish()
{
    // Write zero block -- end of TAR file
    memset(m_Buffer, 0, kBlockSize);
    x_WriteStream(m_Buffer, kBlockSize);
}
*/


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/05/05 12:32:45  ivanov
 * + CTar::Update()
 *
 * Revision 1.10  2005/04/27 13:53:02  ivanov
 * Added support for (re)storing permissions/owner/times
 *
 * Revision 1.9  2005/02/03 14:01:21  rsmith
 * must initialize  m_StreamPos since streampos may not have a default cnstr.
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
