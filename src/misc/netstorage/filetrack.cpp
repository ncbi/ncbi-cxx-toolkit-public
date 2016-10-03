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
 * Authors: Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:
 *   FileTrack API implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "filetrack.hpp"

#include <connect/services/ns_output_parser.hpp>

#include <connect/ncbi_http_session.hpp>

#include <util/random_gen.hpp>
#include <util/util_exception.hpp>

#include <corelib/rwstream.hpp>
#include <corelib/request_status.hpp>
#include <corelib/resource_info.hpp>
#include <corelib/request_ctx.hpp>

#include <time.h>
#include <sstream>
#include <functional>

#define DELEGATED_FROM_MYNCBI_ID_HEADER "Delegated-From-MyNCBI-ID"

BEGIN_NCBI_SCOPE

#define THROW_IO_EXCEPTION(err_code, message, status) \
        NCBI_THROW_FMT(CIOException, err_code, message << IO_StatusStr(status));

static EHTTP_HeaderParse s_HTTPParseHeader_SaveStatus(
        const char*, void*, int)
{
    return eHTTP_HeaderContinue;
}

const THTTP_Flags kDefaultHttpFlags =
        fHTTP_AutoReconnect |
        fHTTP_SuppressMessages |
        fHTTP_Flushable;

struct SUrl
{
    typedef CNetStorageObjectLoc TLoc;
    typedef TLoc::EFileTrackSite TSite;

    static string Get(TSite site, const char* path)
    {
        const char* const kURLs[TLoc::eNumberOfFileTrackSites] =
        {
            "https://submit.ncbi.nlm.nih.gov",
            "https://dsubmit.ncbi.nlm.nih.gov",
            "https://qsubmit.ncbi.nlm.nih.gov",
        };

        _ASSERT(path);
        return string(kURLs[site]) + path;
    }

    static string Get(TSite site, const char* path,
            const string& key, const char* sub_path)
    {
        _ASSERT(sub_path);
        return Get(site, path) + key + sub_path;
    }

    static string Get(const TLoc& loc, const char* path)
    {
        return Get(loc.GetFileTrackSite(), path);
    }

    static string Get(const TLoc& loc, const char* path, const char* sub_path)
    {
        return Get(loc.GetFileTrackSite(), path, loc.GetUniqueKey(), sub_path);
    }
};

SFileTrackRequest::SFileTrackRequest(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& url,
        SConnNetInfo* net_info,
        const string& user_header,
        THTTP_Flags flags) :
    m_Config(config),
    m_ObjectLoc(object_loc),
    m_URL(url),
    m_HTTPStream(url, net_info, user_header, s_HTTPParseHeader_SaveStatus,
            NULL, NULL, NULL, kDefaultHttpFlags | flags, &m_Config.comm_timeout)
{
}

SFileTrackDownload::SFileTrackDownload(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc) :
    SFileTrackRequest(config, object_loc,
            SUrl::Get(object_loc, "/api/2.0/files/", "/?format=attachment"))
{
}

SFileTrackUpload::SFileTrackUpload(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& user_header,
        SConnNetInfo* net_info) :
    SFileTrackRequest(config, object_loc,
            SUrl::Get(object_loc, "/api/2.0/uploads/binary/"),
            net_info, user_header, fHTTP_WriteThru)
{
}

void SFileTrackRequest::CheckIOStatus()
{
    EIO_Status status = m_HTTPStream.Status();

    if ((status != eIO_Success && status != eIO_Closed) ||
                ((status = m_HTTPStream.Status(eIO_Read)) != eIO_Success &&
                        status != eIO_Closed)) {
        THROW_IO_EXCEPTION(eRead,
                "Error while retrieving HTTP response from " <<
                m_URL << ": ", status);
    }
}

static string s_RemoveHTMLTags(const char* text)
{
    string result;

    while (isspace((unsigned char) *text))
        ++text;

    const char* text_beg = text;

    while (*text != '\0')
        if (*text != '<')
            ++text;
        else {
            if (text > text_beg)
                result.append(text_beg, text);

            while (*++text != '\0')
                if (*text == '>') {
                    text_beg = ++text;
                    break;
                }
        }

    if (text > text_beg)
        result.append(text_beg, text);

    NStr::TruncateSpacesInPlace(result, NStr::eTrunc_End);
    NStr::ReplaceInPlace(result, "\n", " ");
    NStr::ReplaceInPlace(result, "  ", " ");

    return result;
}

const auto kAuthHeader = "Authorization";
const auto kAuthPrefix = "Token ";

static void s_ApplyMyNcbiId(function<void(const string&)> apply)
{
    auto my_ncbi_id(CRequestContext_PassThrough().Get("my_ncbi_id"));
    if (!my_ncbi_id.empty()) apply(my_ncbi_id);
}

CRef<SFileTrackUpload> SFileTrackAPI::StartUpload(
        const CNetStorageObjectLoc& object_loc)
{
    string user_header;

    user_header.append("Content-Type: plain/text\r\n");
    user_header.append(kAuthHeader).append(": ").append(config.token);
    user_header.append("\r\nFile-ID: ").append(object_loc.GetUniqueKey());
    user_header.append("\r\nFile-Editable: true\r\n");

    s_ApplyMyNcbiId([&user_header](const string& my_ncbi_id)
    {
        user_header.append(DELEGATED_FROM_MYNCBI_ID_HEADER ": ");
        user_header.append(my_ncbi_id);
        user_header.append("\r\n");
    });

    AutoPtr<SConnNetInfo, CDeleter<SConnNetInfo> > net_info(ConnNetInfo_Create(0));
    net_info->version = 1;
    net_info->req_method = eReqMethod_Post;

    return CRef<SFileTrackUpload>(
            new SFileTrackUpload(config, object_loc, user_header, net_info.get()));
}

void SFileTrackUpload::Write(const void* buf,
        size_t count, size_t* bytes_written)
{
    if (!m_HTTPStream.write(static_cast<const char*>(buf), count)) {
        THROW_IO_EXCEPTION(eWrite, "Error while sending data to " << m_URL,
                m_HTTPStream.Status());
    }

    if (bytes_written != NULL)
        *bytes_written = count;
}

void SFileTrackUpload::FinishUpload()
{
    string expected_key = m_ObjectLoc.GetUniqueKey();

    CJsonNode upload_result = GetFileInfo();

    string actual_key = upload_result.GetString("key");

    if (actual_key != expected_key) {
        RenameFile(actual_key, expected_key);
    }
}

void SFileTrackUpload::RenameFile(const string& from, const string& to)
{
    string err;

    try {
        CHttpSession session;
        session.SetHttpFlags(kDefaultHttpFlags);

        SUrl::TSite site(m_ObjectLoc.GetFileTrackSite());
        const string url(SUrl::Get(site, "/api/2.0/files/", from, "/"));

        CHttpRequest req = session.NewRequest(url, CHttpSession::ePut);
        auto& timeout(m_Config.comm_timeout);
        req.SetTimeout(CTimeout(timeout.sec, timeout.usec));

        CHttpHeaders& headers = req.Headers();
        headers.SetValue(CHttpHeaders::eContentType, "application/json");
        headers.SetValue(kAuthHeader, m_Config.token);

        s_ApplyMyNcbiId([&headers](const string& my_ncbi_id)
        {
            headers.SetValue(DELEGATED_FROM_MYNCBI_ID_HEADER, my_ncbi_id);
        });

        req.ContentStream() << "{\"key\": \"" << to << "\"}";

        auto status = req.Execute().GetStatusCode();

        if (status == 200) return;

        err.append("status: ").append(to_string(status));
    }
    catch (CException& e) {
        err.append("exception: ").append(e.what());
    }

    NCBI_THROW_FMT(CNetStorageException, eUnknown,
            "Error while uploading \"" << m_ObjectLoc.GetLocator() <<
            "\" to FileTrack: the file has been stored as \"" <<
            from << "\" instead of \"" << to << "\" as requested. "
            "Renaming failed, " << err);
}

CJsonNode SFileTrackRequest::GetFileInfo()
{
    string http_response;

    try {
        ostringstream sstr;
        NcbiStreamCopy(sstr, m_HTTPStream);
        sstr << NcbiEnds;
        http_response = sstr.str();
    }
    catch (CException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while accessing \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() <<
                "\"); HTTP status " << m_HTTPStream.GetStatusCode());
    }

    if (m_HTTPStream.GetStatusCode() == CRequestStatus::e404_NotFound) {
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Error while accessing \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() << "\")");

    } else if (m_HTTPStream.GetStatusCode() == CRequestStatus::e403_Forbidden) {
        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Error while accessing \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() << "\")");

    } else if (m_HTTPStream.GetStatusCode() >= CRequestStatus::e500_InternalServerError &&
            m_HTTPStream.GetStatusCode() <= CRequestStatus::e505_HTTPVerNotSupported) {
        NCBI_THROW_FMT(CNetStorageException, eUnknown,
                "Error while accessing \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() << "\"): " <<
                " (HTTP status " << m_HTTPStream.GetStatusCode() << ')');
    }

    CJsonNode root;

    try {
        root = CJsonNode::ParseJSON(http_response);
    }
    catch (CStringException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while accessing \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() << "\"): " <<
                s_RemoveHTMLTags(http_response.c_str()) <<
                " (HTTP status " << m_HTTPStream.GetStatusCode() << ')');
    }

    CheckIOStatus();

    return root;
}

CRef<SFileTrackDownload> SFileTrackAPI::StartDownload(
        const CNetStorageObjectLoc& object_loc)
{
    return CRef<SFileTrackDownload>(
            new SFileTrackDownload(config, object_loc));
}

void SFileTrackAPI::Remove(const CNetStorageObjectLoc& object_loc)
{
    CHttpSession session;
    session.SetHttpFlags(kDefaultHttpFlags);

    const string url(SUrl::Get(object_loc, "/api/2.0/files/", "/"));
    CHttpRequest req = session.NewRequest(url, CHttpSession::eDelete);
    auto& timeout(config.comm_timeout);
    req.SetTimeout(CTimeout(timeout.sec, timeout.usec));

    CHttpHeaders& headers = req.Headers();
    headers.SetValue(CHttpHeaders::eContentType, "application/json");
    headers.SetValue(kAuthHeader, config.token);

    CHttpResponse response(req.Execute());

    switch (response.GetStatusCode()) {
    case CRequestStatus::e200_Ok:
    case CRequestStatus::e204_NoContent:
        return;

    case CRequestStatus::e404_NotFound:
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Cannot remove \"" << object_loc.GetLocator() << "\"");
        break; // Not reached

    default:
        NCBI_THROW_FMT(CNetStorageException, eUnknown,
                "Cannot remove \"" << object_loc.GetLocator() <<
                "\": " << response.GetStatusText());
    }
}

ERW_Result SFileTrackDownload::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (m_HTTPStream.read(static_cast<char*>(buf), count).bad()) {
        THROW_IO_EXCEPTION(eWrite, "Error while reading data from " << m_URL,
                m_HTTPStream.Status());
    }

    if (m_FirstRead) {
        if (m_HTTPStream.GetStatusCode() >= 400) {
            // Cannot use anything except EErrCode value as the second argument
            // (as exception is added to it inside macro), so had to copy-paste
            if (m_HTTPStream.GetStatusCode() == CRequestStatus::e404_NotFound) {
                NCBI_THROW_FMT(CNetStorageException, eNotExists,
                        "Cannot open \"" << m_ObjectLoc.GetLocator() <<
                        "\" for reading (HTTP status " << m_HTTPStream.GetStatusCode() << ").");
            }
            NCBI_THROW_FMT(CNetStorageException, eIOError,
                    "Cannot open \"" << m_ObjectLoc.GetLocator() <<
                    "\" for reading (HTTP status " << m_HTTPStream.GetStatusCode() << ").");
        }
        m_FirstRead = false;
    }

    if (bytes_read != NULL)
        *bytes_read = (size_t) m_HTTPStream.gcount();

    return m_HTTPStream.eof() ? eRW_Eof : eRW_Success;
}

bool SFileTrackDownload::Eof() const
{
    return m_HTTPStream.eof();
}

void SFileTrackDownload::FinishDownload()
{
    CheckIOStatus();
}

SFileTrackAPI::SFileTrackAPI(const SFileTrackConfig& c)
    : config(c)
{
}

CJsonNode SFileTrackAPI::GetFileInfo(const CNetStorageObjectLoc& object_loc)
{
    const string url(SUrl::Get(object_loc, "/api/2.0/files/", "/"));

    SFileTrackRequest request(config, object_loc, url);

    return request.GetFileInfo();
}

string SFileTrackAPI::GetPath(const CNetStorageObjectLoc& object_loc)
{
    CHttpSession session;
    session.SetHttpFlags(kDefaultHttpFlags);

    const string url(SUrl::Get(object_loc, "/api/2.0/pins/"));
    CHttpRequest req = session.NewRequest(url, CHttpSession::ePost);
    auto& timeout(config.comm_timeout);
    req.SetTimeout(CTimeout(timeout.sec, timeout.usec));

    CHttpHeaders& headers = req.Headers();
    headers.SetValue(CHttpHeaders::eContentType, "application/json");
    headers.SetValue(kAuthHeader, config.token);

    req.ContentStream() << "{\"file_key\": \"" <<
            object_loc.GetUniqueKey() << "\"}";

    CHttpResponse response(req.Execute());

    // If neither "200 OK" nor "201 CREATED"
    if (response.GetStatusCode() < 200 || response.GetStatusCode() > 201) {
        NCBI_THROW_FMT(CNetStorageException, eUnknown,
                "Error while locking path for \"" << object_loc.GetLocator() <<
                "\" in FileTrack: " << response.GetStatusCode() << ' ' <<
                response.GetStatusText());
    }

    ostringstream oss;
    oss << response.ContentStream().rdbuf();

    try {
        // If there is already "path" in response
        if (CJsonNode root = CJsonNode::ParseJSON(oss.str())) {
            if (CJsonNode path = root.GetByKeyOrNull("path")) {
                return path.AsString();
            }
        }
    }
    catch (CStringException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eUnknown,
                "Error while locking path for \"" << object_loc.GetLocator() <<
                "\" in FileTrack: Failed to parse response.");
    }

    NCBI_THROW_FMT(CNetStorageException, eUnknown,
            "Error while locking path for \"" << object_loc.GetLocator() <<
            "\" in FileTrack: no path in response");
    return string(); // Not reached
}

const string s_GetSection(const string& section)
{
    return section.empty() ? "filetrack" : section;
}

const unsigned kDefaultCommTimeout = 30;

const STimeout s_GetDefaultTimeout(unsigned sec = kDefaultCommTimeout)
{
    STimeout result;
    result.sec = sec;
    result.usec = 0;
    return result;
}

const string s_GetDecryptedKey(const string& key)
{
    if (CNcbiEncrypt::IsEncrypted(key)) {
        try {
            return CNcbiEncrypt::Decrypt(key);
        } catch (CException& e) {
            NCBI_RETHROW2(e, CRegistryException, eDecryptionFailed,
                    "Decryption failed for configuration value '" + key + "'.", 0);
        }
    }

    return key;
}

SFileTrackConfig::SFileTrackConfig(EVoid) :
    comm_timeout(s_GetDefaultTimeout())
{
}

SFileTrackConfig::SFileTrackConfig(const IRegistry& reg, const string& section) :
    enabled(true),
    site(GetSite(reg.GetString(s_GetSection(section), "site", "prod"))),
    token(reg.GetEncryptedString(s_GetSection(section), "token",
                IRegistry::fPlaintextAllowed)),
    comm_timeout(s_GetDefaultTimeout(reg.GetInt(
                s_GetSection(section), "communication_timeout", kDefaultCommTimeout)))
{
    if (token.size()) token.insert(0, kAuthPrefix);
}

bool SFileTrackConfig::ParseArg(const string& name, const string& value)
{
    if (name == "ft_site") {
        site = SFileTrackConfig::GetSite(value);
    } else if (name == "ft_token") {
        token = kAuthPrefix + NStr::URLDecode(s_GetDecryptedKey(value));
    } else {
        return false;
    }

    enabled = true;
    return true;
}

CNetStorageObjectLoc::EFileTrackSite
SFileTrackConfig::GetSite(const string& ft_site_name)
{
    if (ft_site_name == "submit" || ft_site_name == "prod")
        return CNetStorageObjectLoc::eFileTrack_ProdSite;
    else if (ft_site_name == "dsubmit" || ft_site_name == "dev")
        return CNetStorageObjectLoc::eFileTrack_DevSite;
    else if (ft_site_name == "qsubmit" || ft_site_name == "qa")
        return CNetStorageObjectLoc::eFileTrack_QASite;
    else {
        NCBI_THROW_FMT(CArgException, eInvalidArg,
                "unrecognized FileTrack site '" << ft_site_name << '\'');
    }
}

END_NCBI_SCOPE
