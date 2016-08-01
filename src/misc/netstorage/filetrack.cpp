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
        const char* /*http_header*/, void* user_data, int server_error)
{
    static_cast<SFileTrackRequest*>(
            user_data)->m_HTTPStatus = server_error;

    return eHTTP_HeaderComplete;
}

const THTTP_Flags kDefaultHttpFlags =
        fHTTP_AutoReconnect |
        fHTTP_SuppressMessages |
        fHTTP_Flushable;

static const char* const s_URLs[CNetStorageObjectLoc::eNumberOfFileTrackSites] =
{
    "https://submit.ncbi.nlm.nih.gov",
    "https://dsubmit.ncbi.nlm.nih.gov",
    "https://qsubmit.ncbi.nlm.nih.gov",
};

string s_GetURL(const CNetStorageObjectLoc& object_loc,
        const char* path, const char* path_after_key = 0)
{
    _ASSERT(path);

    string url = s_URLs[object_loc.GetFileTrackSite()];
    url += path;

    if (path_after_key) {
        url += object_loc.GetUniqueKey();
        url += path_after_key;
    }

    return url;
}

SConnNetInfo* SFileTrackRequest::GetNetInfo() const
{
    SConnNetInfo* net_info(ConnNetInfo_Create(0));
    net_info->version = 1;
    net_info->req_method = eReqMethod_Post;
    return net_info;
}

string SFileTrackRequest::GetURL() const
{
    return s_GetURL(m_ObjectLoc, "/api/2.0/uploads/binary/");
}

THTTP_Flags SFileTrackRequest::GetUploadFlags() const
{
    return kDefaultHttpFlags | fHTTP_WriteThru;
}

SFileTrackRequest::SFileTrackRequest(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& url) :
    m_Config(config),
    m_ObjectLoc(object_loc),
    m_URL(url),
    m_HTTPStream(url, NULL, kEmptyStr, s_HTTPParseHeader_SaveStatus,
            this, NULL, NULL, kDefaultHttpFlags, &m_Config.comm_timeout)
{
}

SFileTrackRequest::SFileTrackRequest(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& user_header,
        int) :
    m_Config(config),
    m_NetInfo(GetNetInfo()),
    m_ObjectLoc(object_loc),
    m_URL(GetURL()),
    m_HTTPStream(m_URL, m_NetInfo.get(), user_header,
            s_HTTPParseHeader_SaveStatus, this, NULL, NULL, GetUploadFlags(),
            &m_Config.comm_timeout)
{
}

SFileTrackPostRequest::SFileTrackPostRequest(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& user_header) :
    SFileTrackRequest(config, object_loc, user_header, 0)
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

CRef<SFileTrackPostRequest> SFileTrackPostRequest::Create(
        const SFileTrackConfig& config,
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

    return CRef<SFileTrackPostRequest>(
            new SFileTrackPostRequest(config, object_loc, user_header));
}

CRef<SFileTrackPostRequest> SFileTrackAPI::StartUpload(
        const CNetStorageObjectLoc& object_loc)
{
    return SFileTrackPostRequest::Create(config, object_loc);
}

void SFileTrackPostRequest::Write(const void* buf,
        size_t count, size_t* bytes_written)
{
    if (!m_HTTPStream.write(static_cast<const char*>(buf), count)) {
        THROW_IO_EXCEPTION(eWrite, "Error while sending data to " << m_URL,
                m_HTTPStream.Status());
    }

    if (bytes_written != NULL)
        *bytes_written = count;
}

void SFileTrackPostRequest::FinishUpload()
{
    string unique_key = m_ObjectLoc.GetUniqueKey();

    CJsonNode upload_result = ReadJsonResponse();

    string filetrack_file_id = upload_result.GetString("key");

    if (filetrack_file_id != unique_key) {
        RenameFile(filetrack_file_id, unique_key, kAuthHeader, m_Config.token);
    }
}

void SFileTrackPostRequest::RenameFile(const string& from, const string& to,
        CHttpHeaders::CHeaderNameConverter header, const string& value)
{
    string err;

    try {
        CHttpSession session;
        session.SetHttpFlags(kDefaultHttpFlags);

        string url(s_URLs[m_ObjectLoc.GetFileTrackSite()]);
        url.append("/api/2.0/files/").append(from).append("/");

        CHttpRequest req = session.NewRequest(url, CHttpSession::ePut);
        auto& timeout(m_Config.comm_timeout);
        req.SetTimeout(CTimeout(timeout.sec, timeout.usec));

        CHttpHeaders& headers = req.Headers();
        headers.SetValue(CHttpHeaders::eContentType, "application/json");
        headers.SetValue(header, value);

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

CJsonNode SFileTrackRequest::ReadJsonResponse()
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
                "\"); HTTP status " << m_HTTPStatus);
    }

    if (m_HTTPStatus == CRequestStatus::e404_NotFound) {
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Error while accessing \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() << "\")");

    } else if (m_HTTPStatus == CRequestStatus::e403_Forbidden) {
        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Error while accessing \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() << "\")");

    } else if (m_HTTPStatus >= CRequestStatus::e500_InternalServerError &&
            m_HTTPStatus <= CRequestStatus::e505_HTTPVerNotSupported) {
        NCBI_THROW_FMT(CNetStorageException, eUnknown,
                "Error while accessing \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() << "\"): " <<
                " (HTTP status " << m_HTTPStatus << ')');
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
                " (HTTP status " << m_HTTPStatus << ')');
    }

    CheckIOStatus();

    return root;
}

CRef<SFileTrackRequest> SFileTrackAPI::StartDownload(
        const CNetStorageObjectLoc& object_loc)
{
    const string url(s_GetURL(object_loc, "/ft/byid/", "/contents"));

    CRef<SFileTrackRequest> new_request(new SFileTrackRequest(config, object_loc,
            url));

    new_request->m_FirstRead = true;

    return new_request;
}

void SFileTrackAPI::Remove(const CNetStorageObjectLoc& object_loc)
{
    const string url(s_GetURL(object_loc, "/ftmeta/files/", "/__delete__"));

    SFileTrackRequest new_request(config, object_loc, url);

    new_request.m_HTTPStream << NcbiEndl;

    ostringstream null_stream; // Not used
    NcbiStreamCopy(null_stream, new_request.m_HTTPStream);

    if (new_request.m_HTTPStatus == CRequestStatus::e404_NotFound) {
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Cannot remove \"" << new_request.m_ObjectLoc.GetLocator() <<
                "\" (HTTP status " << new_request.m_HTTPStatus << ").");
    }
}

ERW_Result SFileTrackRequest::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (m_HTTPStream.read(static_cast<char*>(buf), count).bad()) {
        THROW_IO_EXCEPTION(eWrite, "Error while reading data from " << m_URL,
                m_HTTPStream.Status());
    }

    if (m_FirstRead) {
        if (m_HTTPStatus >= 400) {
            // Cannot use anything except EErrCode value as the second argument
            // (as exception is added to it inside macro), so had to copy-paste
            if (m_HTTPStatus == CRequestStatus::e404_NotFound) {
                NCBI_THROW_FMT(CNetStorageException, eNotExists,
                        "Cannot open \"" << m_ObjectLoc.GetLocator() <<
                        "\" for reading (HTTP status " << m_HTTPStatus << ").");
            }
            NCBI_THROW_FMT(CNetStorageException, eIOError,
                    "Cannot open \"" << m_ObjectLoc.GetLocator() <<
                    "\" for reading (HTTP status " << m_HTTPStatus << ").");
        }
        m_FirstRead = false;
    }

    if (bytes_read != NULL)
        *bytes_read = (size_t) m_HTTPStream.gcount();

    return m_HTTPStream.eof() ? eRW_Eof : eRW_Success;
}

void SFileTrackRequest::FinishDownload()
{
    CheckIOStatus();
}

SFileTrackAPI::SFileTrackAPI(const SFileTrackConfig& c)
    : config(c)
{
}

CJsonNode SFileTrackAPI::GetFileInfo(const CNetStorageObjectLoc& object_loc)
{
    const string url(s_GetURL(object_loc, "/ftmeta/files/", "/"));

    SFileTrackRequest request(config, object_loc, url);

    return request.ReadJsonResponse();
}

string SFileTrackAPI::GetPath(const CNetStorageObjectLoc& object_loc)
{
    CHttpSession session;
    session.SetHttpFlags(kDefaultHttpFlags);

    const string url(s_GetURL(object_loc, "/api/2.0/pins/"));
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

// Enable this code if FileTrack starts to send "path" in response to lock
#if 0
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
#endif

    if (CJsonNode root = GetFileInfo(object_loc)) {
        if (CJsonNode path = root.GetByKeyOrNull("path")) {
            return path.AsString();
        }
    }

    return string();
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
