#ifndef LDS2_HANDLERS_HPP__
#define LDS2_HANDLERS_HPP__
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
 * Author: Aleksey Grichenko
 *
 * File Description: LDS v.2 URL handlers.
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objtools/lds2/lds2_db.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// Base class for URL handler.
class NCBI_LDS2_EXPORT CLDS2_UrlHandler_Base : public CObject
{
public:
    /// Create a handler with the given name.
    CLDS2_UrlHandler_Base(const string& handler_name)
        : m_Name(handler_name) {}
    virtual ~CLDS2_UrlHandler_Base(void) {}

    /// Get handler name
    const string& GetHandlerName(void) const { return m_Name; }

    /// Fill file/url information - crc, size, format etc.
    /// 'size' member must be set to >=0 if the file exists,
    /// negative values indicate non-existent files/urls which
    /// should be removed from the database.
    /// The default implementation uses GetXXXX methods to fill
    /// the information.
    virtual void FillInfo(SLDS2_File& info);

    /// Save information about chunks for the URL in the database.
    /// The default implementation does nothing.
    virtual void SaveChunks(const SLDS2_File& file_info,
                            CLDS2_Database&   db) {}

    /// Open input stream for the URL at the specified position.
    /// The stream will be deleted by the caller.
    /// Database is provided so that the handler can fetch
    /// information about chunks.
    virtual CNcbiIstream* OpenStream(const SLDS2_File& file_info,
                                     Int8              stream_pos,
                                     CLDS2_Database*   db) = 0;

protected:
    /// Allow to change handler name by derived classes.
    void SetHandlerName(const string& new_name) { m_Name = new_name; }

    /// Methods for getting file information. Most of this information
    /// (except format) is not critical and is used only to detect
    /// modifications and reindex file in the database if at least
    /// one field changes.

    /// Guess file format. The default implementation opens the
    /// stream (see OpenStream) and uses CFormatGuess on it.
    virtual SLDS2_File::TFormat GetFileFormat(const SLDS2_File& file_info);
    /// Get file size - returns 0 by default. Negative values are used
    /// to indicate non-existing files.
    virtual Int8 GetFileSize(const SLDS2_File& /*file_info*/) { return 0; }
    /// Get file CRC - returns 0 by default.
    virtual Uint4 GetFileCRC(const SLDS2_File& /*file_info*/) { return 0; }
    /// Get file timestamp - returns 0 by default.
    virtual Int8 GetFileTime(const SLDS2_File& /*file_info*/) { return 0; }

private:
    string m_Name;
};


/// Default handler for local files - registered automatically
/// by LDS2 manager and data loader.
class NCBI_LDS2_EXPORT CLDS2_UrlHandler_File : public CLDS2_UrlHandler_Base
{
public:
    CLDS2_UrlHandler_File(void);
    virtual ~CLDS2_UrlHandler_File(void) {}

    /// Open input stream for the URL at the specified position.
    /// The stream will be deleted by the caller.
    virtual CNcbiIstream* OpenStream(const SLDS2_File& file_info,
                                     Int8              stream_pos,
                                     CLDS2_Database*   db);

    static const string s_GetHandlerName(void);

protected:
    virtual Int8 GetFileSize(const SLDS2_File& file_info);
    virtual Uint4 GetFileCRC(const SLDS2_File& file_info);
    virtual Int8 GetFileTime(const SLDS2_File& file_info);
};


/// Handler for GZip local files. Not registered by default.
class NCBI_LDS2_EXPORT CLDS2_UrlHandler_GZipFile : public CLDS2_UrlHandler_File
{
public:
    /// Create GZip file handler.
    CLDS2_UrlHandler_GZipFile(void);
    virtual ~CLDS2_UrlHandler_GZipFile(void) {}

    /// Save information about chunks for the URL in the database.
    virtual void SaveChunks(const SLDS2_File& file_info,
                            CLDS2_Database&   db);

    /// Open input stream for the URL at the specified position.
    /// The stream will be deleted by the caller.
    virtual CNcbiIstream* OpenStream(const SLDS2_File& file_info,
                                     Int8              stream_pos,
                                     CLDS2_Database*   db);

    static const string s_GetHandlerName(void);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // LDS2_HPP__
