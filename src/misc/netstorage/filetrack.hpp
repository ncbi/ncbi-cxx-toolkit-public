#ifndef CONNECT_SERVICES___FILETRACK_HPP
#define CONNECT_SERVICES___FILETRACK_HPP

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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   FileTrack API declarations.
 *
 */

#include <misc/netstorage/netstorage.hpp>

#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_http_session.hpp>

#include <corelib/ncbimisc.hpp>
#include <corelib/reader_writer.hpp>

#include <util/random_gen.hpp>

BEGIN_NCBI_SCOPE

struct SFileTrackConfig
{
    bool enabled = false;
    CNetStorageObjectLoc::EFileTrackSite site;
    string token;
    const STimeout comm_timeout;

    SFileTrackConfig(EVoid = eVoid); // Means no FileTrack as a backend storage
    SFileTrackConfig(const IRegistry& registry, const string& section = kEmptyStr);

    bool ParseArg(const string&, const string&);

    static CNetStorageObjectLoc::EFileTrackSite GetSite(const string&);
};

struct SFileTrackRequest : public CObject
{
    SFileTrackRequest(const SFileTrackConfig& config,
            const CNetStorageObjectLoc& object_loc,
            const string& url);

    CJsonNode ReadJsonResponse();

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read);
    void FinishDownload();

    void CheckIOStatus();

protected:
    SFileTrackRequest(const SFileTrackConfig& config,
            const CNetStorageObjectLoc& object_loc,
            const string& user_header, SConnNetInfo* net_info);

    const SFileTrackConfig& m_Config;

private:
    string GetURL() const;
    THTTP_Flags GetUploadFlags() const;

public:
    const CNetStorageObjectLoc& m_ObjectLoc;
    string m_URL;
    CConn_HttpStream m_HTTPStream;
    bool m_FirstRead = false;
};

struct SFileTrackUpload : public SFileTrackRequest
{
    void Write(const void* buf, size_t count, size_t* bytes_written);
    virtual void FinishUpload();

    SFileTrackUpload(const SFileTrackConfig& config,
            const CNetStorageObjectLoc& object_loc,
            const string& user_header, SConnNetInfo* net_info);

private:
    void RenameFile(const string& from, const string& to,
            CHttpHeaders::CHeaderNameConverter header, const string& value);

    SFileTrackUpload();
};

struct SFileTrackAPI
{
    SFileTrackAPI(const SFileTrackConfig&);

    CJsonNode GetFileInfo(const CNetStorageObjectLoc& object_loc);

    CRef<SFileTrackUpload> StartUpload(const CNetStorageObjectLoc& object_loc);
    CRef<SFileTrackRequest> StartDownload(const CNetStorageObjectLoc& object_loc);

    void Remove(const CNetStorageObjectLoc& object_loc);
    string GetPath(const CNetStorageObjectLoc& object_loc);

    DECLARE_OPERATOR_BOOL(config.enabled);

    const SFileTrackConfig config;
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___FILETRACK_HPP */
