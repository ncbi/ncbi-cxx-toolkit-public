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
 * Authors:  Mike Dicuccio, Anatoliy Kuznetsov
 *
 * File Description:
 *     BLOB (image) fetch from NetCache.
 *     Takes two CGI arguments:
 *       key=NC_KEY
 *       fmt=FORMAT  (default: "image/png")
 *
 */

#include <ncbi_pch.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <connect/services/netcache_api.hpp>
#include <connect/services/grid_app_version_info.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbiargs.hpp>

#define GRID_APP_NAME "ncfetch.cgi"

USING_NCBI_SCOPE;

/// NetCache BLOB/image fetch application
///
class CNetCacheBlobFetchApp : public CCgiApplication
{
protected:
    virtual void Init();
    virtual int ProcessRequest(CCgiContext& ctx);

private:
    enum {
        eNoPassword,
        ePasswordFromParam,
        ePasswordFromCookie
    } m_PasswordSource;

    string m_PasswordSourceName;
};

void CNetCacheBlobFetchApp::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    GetConfig().Set("netcache_api", "enable_mirroring", "on_read");

    string password_source = GetConfig().GetString("ncfetch",
        "password_source", kEmptyStr);
    if (password_source.empty())
        m_PasswordSource = eNoPassword;
    else {
        string source_type;
        if (NStr::SplitInTwo(password_source, ":",
                source_type, m_PasswordSourceName))
            m_PasswordSource = m_PasswordSourceName.empty() ?
                eNoPassword : source_type == "cookie" ?
                    ePasswordFromCookie : ePasswordFromParam;
        else if (source_type.empty())
            m_PasswordSource = eNoPassword;
        else {
            m_PasswordSource = ePasswordFromParam;
            m_PasswordSourceName = source_type;
        }
    }
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
    if (is_found && !filename.empty())
        reply.SetHeaderValue("Content-Disposition",
            "attachment; filename=" + filename);

    reply.SetContentType(fmt);

    CNetCacheAPI cli("ncfetch");
    size_t blob_size = 0;
    auto_ptr<IReader> reader;
    if (m_PasswordSource == eNoPassword)
        reader.reset(cli.GetReader(key, &blob_size,
            CNetCacheAPI::eCaching_Disable));
    else {
        string password;
        if (m_PasswordSource == ePasswordFromCookie) {
            const CCgiCookie* cookie =
                request.GetCookies().Find(m_PasswordSourceName, "", "");
            if (!cookie) {
                ERR_POST("Cookie '" << m_PasswordSourceName << "' not found");
                return 2;
            }
            password = cookie->GetValue();
        } else {
            bool password_found = false;
            password = request.GetEntry(m_PasswordSourceName, &password_found);
            if (!password_found) {
                ERR_POST("CGI entry '" << m_PasswordSourceName << "' not found");
                return 2;
            }
        }
        reader.reset(CNetCachePasswordGuard(cli, password)->GetReader(key,
            &blob_size, CNetCacheAPI::eCaching_Disable));
    }
    if (!reader.get()) {
        ERR_POST("Could not retrieve blob " << key);
        return 3;
    }

    reply.WriteHeader();

    LOG_POST(Info << "retrieved data: " << blob_size << " bytes");

    CRStream in_stream(reader.get());
    NcbiStreamCopy(reply.out(), in_stream);

    return 0;
}

int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();

    SetSplitLogFile(true);
    GetDiagContext().SetOldPostFormat(false);

    return CNetCacheBlobFetchApp().AppMain(argc, argv, 0, eDS_Default);
}
