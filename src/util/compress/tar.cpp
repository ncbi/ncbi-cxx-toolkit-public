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
#include <internal/util/tar/tar.hpp>


BEGIN_NCBI_SCOPE


/// TAR block size
static const streamsize kBlockSize = 512;

/// Default buffer size (must be multiple kBlockSize)
static const streamsize kDefaultBufferSize = 4096;

/// Null char
static const char kNull = '\0';

/// Macro to compute size that is multiple to kBlockSize
#define ALIGN_BLOCK_SIZE(size) \
    if ( size % kBlockSize != 0 ) { \
        size = (size / kBlockSize + 1) * kBlockSize; \
    }


//////////////////////////////////////////////////////////////////////////////
//
// CTar
//


// TAR POSIX file header
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


// Block to read
union TBlock {
  char     buffer[kBlockSize];
  SHeader  header;
};


CTar::CTar(const string& file_name)
    : m_FileName(file_name), m_Stream(0), m_IsStreamOwned(true),
      m_BufferSize(kDefaultBufferSize)
{
    // Create new file stream
    m_FileStream = new CNcbiFstream();
    if ( !m_FileStream ) {
        NCBI_THROW(CTarException, eMemory, "Cannot create file stream");
    }
    return;
}


CTar::CTar(CNcbiIos& stream)
    : m_FileName(kEmptyStr), m_Stream(&stream), m_FileStream(0), m_IsStreamOwned(true),
      m_StreamPos(0), m_BufferSize(kDefaultBufferSize)
{
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
    return;
}


void CTar::Extract(const string& dst_dir)
{
    // Open file for reading
    if ( m_IsStreamOwned ) {
        x_Open(eRead);
    }

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
            // Assign long names if defined
            if ( info.GetType() == CTarEntryInfo::eFile  ||
                 info.GetType() == CTarEntryInfo::eDir) {
                if ( !m_LongName.empty() ) {
                    info.SetName(m_LongName);
                    m_LongName.clear();
                }
                if ( !m_LongLinkName.empty() ) {
                    info.SetLinkName(m_LongLinkName);
                    m_LongLinkName.clear();
                }
            }

            // Create entry
            switch(info.GetType()) {
                // File
                case CTarEntryInfo::eFile:
                    {{
                    // Create base path
                    string dst_path = CDirEntry::ConcatPath(dst_dir, info.GetName());
                    string base_dir = CDirEntry(dst_path).GetDir();
                    CDir dir(base_dir);
                    if ( !dir.CreatePath() ) {
                        NCBI_THROW(CTarException, eCreate, "Unable to create directory " + base_dir);
                    }
                    CNcbiOfstream os(dst_path.c_str(), IOS_BASE::out | IOS_BASE::binary);
                    if ( !os.good() ) {
                        NCBI_THROW(CTarException, eCreate, "Unable to create file " + dst_path);
                    }   

                    // Get size of entry rounded up by kBlockSize
                    streamsize aligned_size = info.m_Size;
                    ALIGN_BLOCK_SIZE(aligned_size);

                    // Allocate buffer
                    char* buffer = x_AllocateBuffer(m_BufferSize);
                    // Copy file from archive to disk
                    streamsize to_read  = aligned_size;
                    streamsize to_write = info.m_Size;
                    while ( m_Stream->good() &&  to_read > 0 ) {
                        // Read archive
                        streamsize x_nread = ( to_read > m_BufferSize) ? m_BufferSize : to_read;
                        streamsize nread = x_ReadStream(buffer, x_nread);
                        if ( !nread ) {
                            NCBI_THROW(CTarException, eRead, "Cannot read TAR file " + dst_path);
                        }
                        to_read -= nread;
                        // Write to file
                        if (to_write > 0) {
                            os.write(buffer, min(nread, to_write));
                            if ( !os ) {
                                NCBI_THROW(CTarException, eWrite, "Cannot write file " + dst_path);
                            }
                            to_write -= nread;
                        }
                    }
                    // Clean up
                    delete buffer;
                    os.close();
                    }}
                    break;

                // Directory
                case CTarEntryInfo::eDir:
                    {{
                    string dst_path = CDir::ConcatPath(dst_dir, info.GetName());
                    CDir dir(dst_path);
                    if ( !dir.CreatePath() ) {
                        NCBI_THROW(CTarException, eCreate, "Unable to create directory " + dst_path);
                    }
                    }}
                    break;

                case CTarEntryInfo::eGNULongName:

                    break;

                case CTarEntryInfo::eGNULongLink:
                    break;

                // Link
                case CTarEntryInfo::eLink:
                default:
                    ERR_POST(Warning << "Unsupported entry type. Skipped.");
            }
        }}
//      m_Stream->seekg(aligned_size, IOS_BASE::cur);
//      m_Stream->rdbuf()->PUBSEEKPOS(pos);
//???
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// CTar private methods
//


void CTar::x_Open(EOpenMode mode)
{
    if ( !m_FileStream ) {
        return;
    }

    // For sure that the file is closed
    x_Close();

    // Open file using specified mode
    switch(mode) {
    case eCreate:
        m_FileStream->open(m_FileName.c_str(), IOS_BASE::binary | IOS_BASE::out | IOS_BASE::trunc);
        break;
    case eRead:
        m_FileStream->open(m_FileName.c_str(), IOS_BASE::binary | IOS_BASE::in);
        break;
    case eUpdate:
        m_FileStream->open(m_FileName.c_str(), IOS_BASE::binary | IOS_BASE::in | IOS_BASE::out);
        break;
    }
    // TODO: use compression streams for gzip/bzip2 compressed tar archives...
    m_Stream = m_FileStream;

    // Check result
    if ( !m_Stream->good() ) {
        NCBI_THROW(CTarException, eOpen, "Unable to open file " + m_FileName);
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
    // TODO: use compression streams for gzip/bzip2 compressed tar archives...
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
        checksum = NStr::StringToInt(NStr::TruncateSpaces(h->checksum),
                                     8, NStr::eCheck_Skip);
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
        return eZeroBlock;
    }

    // Compare checksums
    if ( checksum != sum ) {
        NCBI_THROW(CTarException, eCRC, "Header CRC check failed");
    }
    // Check file format
    if ( memcmp(h->magic, "ustar", 5) != 0 ) {
        NCBI_THROW(CTarException, eFormat, "Unknown archive format");
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

    // Entry type 
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
                char* name = x_AllocateBuffer(name_size);
                streamsize n = x_ReadStream(name, name_size);
                if ( n != name_size ) {
                    NCBI_THROW(CTarException, eRead, "Unexpected EOF in archive");
                }
                // Save name
                m_LongName.assign(name, info.m_Size); 
                delete name;
            }}
            break;
        case 'K':
            info.SetType(CTarEntryInfo::eGNULongLink);
            break;
        default:
            info.SetType(CTarEntryInfo::eUnknown);
    }

    // Link name field is valid only if "type" field is eLink
    // It is null-terminated unless every character is non-null.
    if ( info.GetType() == CTarEntryInfo::eLink ) {
        h->magic[0] = kNull;
        info.SetLinkName(h->linkname);
    }

    return eSuccess;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
