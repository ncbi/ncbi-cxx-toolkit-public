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

#include <sys/types.h>
#if defined(HAVE_SYS_STAT_H)
#  include <sys/stat.h>
#endif

//#if defined(NCBI_OS_MSWIN)
//#  include <windows.h>
//#endif

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

/// Name field size
static const size_t kNameFieldSize   = 100;
/// Name prefix field size
static const size_t kPrefixFieldSize = 155;

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

/// Macro to check bits
#define F_ISSET(mask) ((GetFlags() & (mask)) == (mask))


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


//////////////////////////////////////////////////////////////////////////////
//
// CTar
//

CTar::CTar(const string& file_name)
    : m_FileName(file_name),
      m_Stream(0),
      m_FileStreamMode(eUnknown),
      m_StreamPos(0),
      m_BufferSize(kDefaultBufferSize),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership)
{
    // Create new file stream
    m_FileStream = new CNcbiFstream();
    if ( !m_FileStream ) {
        NCBI_THROW(CTarException, eMemory, "Cannot create file stream");
    }
    // Allocate I/O bufer
    m_Buffer = new char[m_BufferSize];
    if ( !m_Buffer ) {
        NCBI_THROW(CTarException, eMemory, "Unable to allocate memory");
    }
    return;
}


CTar::CTar(CNcbiIos& stream)
    : m_FileName(kEmptyStr),
      m_Stream(&stream),
      m_FileStream(0),
      m_FileStreamMode(eUnknown),
      m_StreamPos(0),
      m_BufferSize(kDefaultBufferSize),
      m_Buffer(0),
      m_Flags(fDefault),
      m_Mask(0),
      m_MaskOwned(eNoOwnership)
{
    // Allocate I/O bufer
    m_Buffer = new char[m_BufferSize];
    if ( !m_Buffer ) {
        NCBI_THROW(CTarException, eMemory, "Unable to allocate memory");
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
        return F_ISSET(fIgnoreZeroBlocks) ? eZeroBlock : eEOF;
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
    //...

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
                streamsize name_size = info.m_Size;
                ALIGN_BLOCK_SIZE(name_size);
                streamsize n = x_ReadStream(m_Buffer, name_size);
                if ( n != name_size ) {
                    NCBI_THROW(CTarException, eRead,
                               "Unexpected EOF in archive");
                }
                // Save name
                m_LongName.assign(m_Buffer, info.m_Size); 
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

    return eSuccess;
}


void CTar::x_WriteEntryInfo(const string& entry_name, CDirEntry::EType type)
{
    // Prepare block info
    TBlock block;
    memset(&block, 0, sizeof(block));
    SHeader* h = &block.header;

    // Get status information
    struct stat st;
    int errcode = -1;

#  if defined(NCBI_OS_MSWIN)
    errcode = stat(entry_name.c_str(), &st);
#  elif defined(NCBI_OS_UNIX)
    errcode = lstat(entry_name.c_str(), &st);
#  endif
    if (errcode != 0) {
        NCBI_THROW(CTarException, eOpen,
                   "Unable to get status information for '" +
                   entry_name + "'");
    }

    // Get internal archive name for entry
    string iname = x_ToArchiveName(entry_name);
    if ( iname.empty() ) {
        NCBI_THROW(CTarException, eBadName, "Empty entry name not allowed");
    }
    if ( type == CDirEntry::eDir ) {
        iname += kSlash;
    }

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
    NUM_TO_OCTAL(st.st_mode, h->mode);

    // --- User ID ---
    NUM_TO_OCTAL(st.st_uid, h->uid);

    // --- Group ID ---
    NUM_TO_OCTAL(st.st_gid, h->gid);

    // --- Size ---
    unsigned long size = 0;
    if ( type == CDirEntry::eFile ) {
        size = st.st_size;
    }
    NUM_TO_OCTAL(size, h->size);

    // --- Modification time ---
    NUM_TO_OCTAL(st.st_mtime, h->mtime);

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
    // not supported yet - empty string

    // --- Group name ---
    // not supported yet - empty string

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


void CTar::x_ReadAndProcess(EAction action, void* data)
{
    // Open file for reading
    x_Open(eRead);

    for(;;) {
        // Next block supposed to be a header
        CTarEntryInfo info;
        EStatus status = x_ReadEntryInfo(info);
        switch(status) {
        case eEOF:
            return;
        case eZeroBlock:
            // skip zero blocks
            continue;
        case eFailure:
            NCBI_THROW(CCoreException, eCore, "Unknown error");
        case eSuccess:
            ; // processed below
        }

        // Process entry
        {{
            // Correct 'info' if long names are defined
            if ( info.GetType() == CTarEntryInfo::eFile  ||
                 info.GetType() == CTarEntryInfo::eDir) {
                if ( !m_LongName.empty() ) {
                    info.SetName(m_LongName);
                    m_LongName.erase();
                }
                if ( !m_LongLinkName.empty() ) {
                    info.SetLinkName(m_LongLinkName);
                    m_LongLinkName.erase();
                }
            }

            // Match file name by set of masks
            bool matched = true;
            if ( m_Mask ) {
                matched = m_Mask->Match(info.GetName());
            }

            // Process
            switch(action) {
                case eList:
                    {{
                    CTarEntryInfo* pinfo = new CTarEntryInfo(info);
                    if ( !pinfo ) {
                        NCBI_THROW(CTarException, eMemory,
                                   "Cannot allocate memory");
                    }
                    // Save entry info
                    if ( matched ) {
                        ((TEntries*)data)->push_back(pinfo);
                    }
                    // Skip entry contents
                    x_ProcessEntry(info, matched, eList);
                    }}
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
        }}
    }
}


void CTar::x_ProcessEntry(const CTarEntryInfo& info, bool do_process,
                          EAction action, void* data)
{
    string dst_dir;
    if ( data ) {
        dst_dir = *(string*)data;
    }
    string dst_path = CDir::ConcatPath(dst_dir, info.GetName());

    // fKeepOldFiles
    if ( action == eExtract  &&  F_ISSET(fKeepOldFiles)  &&
         CDirEntry(dst_path).Exists() )  {
        action = eList;
    }
    auto_ptr<CNcbiOfstream> os;

    // Skip entry contents
    switch(info.GetType()) {

        // File
        case CTarEntryInfo::eFile:
            {{
            // Get size of entry rounded up by kBlockSize
            streamsize aligned_size = info.m_Size;
            ALIGN_BLOCK_SIZE(aligned_size);

            if ( m_FileStream  &&  (!do_process || action == eList) ) {
                m_FileStream->seekg(aligned_size, IOS_BASE::cur);
                break;
            }
            if ( do_process  &&  action == eExtract ) {
                // Create base directory
                string base_dir = CDirEntry(dst_path).GetDir();
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
            // Read file contents
            streamsize to_read  = aligned_size;
            streamsize to_write = info.m_Size;

            while ( m_Stream->good() &&  to_read > 0 ) {
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
                if ( do_process  &&  action == eExtract ) {
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
            }
            }}
            break;

        // Directory
        case CTarEntryInfo::eDir:
            if ( do_process  &&  action == eExtract ) {
                CDir dir(dst_path);
                if ( !dir.CreatePath() ) {
                    NCBI_THROW(CTarException, eCreate,
                               "Unable to create directory '" + dst_path+"'");
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
}


string CTar::x_ToArchiveName(const string& path)
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


void CTar::x_Append(const string& entry_name)
{
    // Compose entry name for relative names
    string x_name;
    if ( CDirEntry::IsAbsolutePathEx(entry_name)  ||  m_BaseDir.empty()) {
        x_name = entry_name;
    } else {
        x_name = GetBaseDir() + entry_name;
    }

    // Get entry type
    CDir entry(x_name);

    switch (entry.GetType()) {
        case CDirEntry::eFile:
            {{
                x_AppendFile(x_name);
            }}
            break;
        case CDirEntry::eDir:
            {{
                // Append directory info
                x_WriteEntryInfo(x_name, CDirEntry::eDir);
                // Append all files in directory
                CDir::TEntries contents = 
                    entry.GetEntries("*", CDir::eIgnoreRecursive);
                ITERATE(CDir::TEntries, i, contents) {
                    string name = (*i)->GetPath();
                    x_Append(name);
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


void CTar::x_AppendFile(const string& file_name)
{
    // Open file
    CNcbiIfstream is;
    is.open(file_name.c_str(), IOS_BASE::binary | IOS_BASE::in);
    if ( !is ) {
        NCBI_THROW(CTarException, eOpen,
                   "Unable to open file '" + file_name + "'");
    }

    // Write file header
    x_WriteEntryInfo(file_name, CDirEntry::eFile);

    streamsize file_size = streamsize(CFile(file_name).GetLength());
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
