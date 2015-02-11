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
 * Authors:  Mike Dicuccio, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *     BLOB (image) fetch from NetCache.
 *     Takes three CGI arguments:
 *       key=NETCACHE_KEY_OR_NETSTORAGE_LOCATOR
 *       fmt=mime/type  (default: "image/png")
 *       filename=Example.pdf
 *
 */

#include <ncbi_pch.hpp>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_exception.hpp>

#include <connect/services/netstorage.hpp>
#include <connect/services/grid_app_version_info.hpp>

#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbiargs.hpp>

#define GRID_APP_NAME "ncfetch.cgi"
#define CONFIG_SECTION "ncfetch"

USING_NCBI_SCOPE;

#define NETSTORAGE_IO_BUFFER_SIZE (64 * 1024)

/// NetCache BLOB/image fetch application
///
class CNetCacheBlobFetchApp : public CCgiApplication
{
public:
    CNetCacheBlobFetchApp() : m_NetStorage(eVoid) {}

protected:
    virtual void Init();
    virtual int ProcessRequest(CCgiContext& ctx);
    virtual int OnException(std::exception& e, CNcbiOstream& os);

private:
    CCompoundIDPool m_CompoundIDPool;
    CNetStorage m_NetStorage;
};

void CNetCacheBlobFetchApp::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    m_NetStorage = CNetStorage(GetConfig().GetString(CONFIG_SECTION,
            "netstorage", kEmptyStr));
}

int CNetCacheBlobFetchApp::ProcessRequest(CCgiContext& ctx)
{
    const CCgiRequest& request = ctx.GetRequest();
    CCgiResponse&      reply   = ctx.GetResponse();

    bool is_found;

    string key = request.GetEntry("key", &is_found);
    if (key.empty() || !is_found) {
        NCBI_THROW(CArgException, eNoArg, "CGI entry 'key' is missing");
    }

    string fmt = request.GetEntry("fmt", &is_found);
    if (fmt.empty() || !is_found)
        fmt = "image/png";

    string filename(request.GetEntry("filename", &is_found));
    if (is_found && !filename.empty()) {
        string is_inline(request.GetEntry("inline", &is_found));

        reply.SetHeaderValue("Content-Disposition",
                (is_found && NStr::StringToBool(is_inline) ?
                        "inline; filename=" : "attachment; filename=") +
                filename);
    }

    reply.SetContentType(fmt);

    reply.WriteHeader();

    CNetStorageObject netstorage_object(m_NetStorage.Open(key));

    char buffer[NETSTORAGE_IO_BUFFER_SIZE];

    size_t total_bytes_written = 0;

    while (!netstorage_object.Eof()) {
        size_t bytes_read = netstorage_object.Read(buffer, sizeof(buffer));

        reply.out().write(buffer, bytes_read);
        total_bytes_written += bytes_read;

        if (bytes_read < sizeof(buffer))
            break;
    }

    netstorage_object.Close();

    LOG_POST(Info << "retrieved data: " << total_bytes_written << " bytes");

    return 0;
}

int CNetCacheBlobFetchApp::OnException(std::exception& e, CNcbiOstream& os)
{
    union {
        CArgException* arg_exception;
        CNetCacheException* nc_exception;
    };

    string status_str;
    string message;

    if ((arg_exception = dynamic_cast<CArgException*>(&e)) != NULL) {
        status_str = "400 Bad Request";
        message = arg_exception->GetMsg();
        SetHTTPStatus(CRequestStatus::e400_BadRequest);
    } else if ((nc_exception = dynamic_cast<CNetCacheException*>(&e)) != NULL) {
        switch (nc_exception->GetErrCode()) {
        case CNetCacheException::eAccessDenied:
            status_str = "403 Forbidden";
            message = nc_exception->GetMsg();
            SetHTTPStatus(CRequestStatus::e403_Forbidden);
            break;
        case CNetCacheException::eBlobNotFound:
            status_str = "404 Not Found";
            message = nc_exception->GetMsg();
            SetHTTPStatus(CRequestStatus::e404_NotFound);
            break;
        default:
            return CCgiApplication::OnException(e, os);
        }
    } else
        return CCgiApplication::OnException(e, os);

    // Don't try to write to a broken output
    if (!os.good()) {
        return -1;
    }

    try {
        // HTTP header
        os << "Status: " << status_str << HTTP_EOL <<
                "Content-Type: text/plain" HTTP_EOL HTTP_EOL <<
                "ERROR:  " << status_str << " " HTTP_EOL HTTP_EOL <<
                message << HTTP_EOL;

        // Check for problems in sending the response
        if (!os.good()) {
            ERR_POST("CNetCacheBlobFetchApp::OnException() "
                    "failed to send error page back to the client");
            return -1;
        }
    }
    catch (exception& e) {
        NCBI_REPORT_EXCEPTION("(CGI) CNetCacheBlobFetchApp::OnException", e);
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();

    SetSplitLogFile(true);
    GetDiagContext().SetOldPostFormat(false);

    return CNetCacheBlobFetchApp().AppMain(argc, argv, 0, eDS_Default);
}
