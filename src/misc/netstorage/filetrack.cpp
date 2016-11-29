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
#include <map>
#include <atomic>

BEGIN_NCBI_SCOPE

#define CHECK_HTTP_STATUS(http, object_loc, ...)                            \
    do {                                                                    \
        int http_code = http.GetStatusCode();                               \
                                                                            \
        switch (http_code) {                                                \
        case CRequestStatus::e200_Ok:                                       \
        case CRequestStatus::e201_Created:                                  \
        case CRequestStatus::e204_NoContent:                                \
            break;                                                          \
                                                                            \
        default:                                                            \
            s_ThrowHttpStatus(DIAG_COMPILE_INFO, object_loc.GetLocator(),   \
                    http_code, http.GetStatusText(), __VA_ARGS__);          \
        }                                                                   \
    } while (0)

#define CHECK_IO_STATUS(io, object_loc, when, already_failed)               \
    do {                                                                    \
        EIO_Status io_code = io.Status();                                   \
                                                                            \
        switch (io_code) {                                                  \
        case eIO_Success:                                                   \
        case eIO_Closed:                                                    \
            io_code = io.Status(eIO_Read);                                  \
        default:                                                            \
            break;                                                          \
        }                                                                   \
                                                                            \
        switch (io_code) {                                                  \
        case eIO_Success:                                                   \
        case eIO_Closed:                                                    \
            if (!already_failed) break;                                     \
        default:                                                            \
            s_ThrowIoStatus(DIAG_COMPILE_INFO, object_loc.GetLocator(),     \
                    io_code, when);                                         \
        }                                                                   \
    } while (0)

CNetStorageException::EErrCode s_HttpStatusToErrCode(int http_code)
{
    switch (http_code) {
    case CRequestStatus::e403_Forbidden:
        return CNetStorageException::eAuthError;

    case CRequestStatus::e404_NotFound:
        return CNetStorageException::eNotExists;
    }

    return CNetStorageException::eUnknown;
}

void s_ThrowHttpStatus(const CDiagCompileInfo& info, const string object_loc,
        int http_code, const string& http_text, const string when)
{
    ostringstream os;
    CNetStorageException::EErrCode err_code = s_HttpStatusToErrCode(http_code);

    os << "'" << http_code << "|" << http_text << "' " << when << " " << object_loc;

    throw CNetStorageException(info, 0, err_code, os.str());
}

void s_ThrowIoStatus(const CDiagCompileInfo& info, const string object_loc,
        EIO_Status io_code, const string when)
{
    ostringstream os;
    auto io_text = IO_StatusStr(io_code);

    os << "'" << io_code << "|" << io_text << "' " << when << " " << object_loc;

    throw CNetStorageException(info, 0, CNetStorageException::eIOError, os.str());
}

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
    enum EType
    {
        eDownload,
        eUpload,
        eLocks,
        eMeta,
    };

    static string Get(const SFileTrackConfig& config,
            const CNetStorageObjectLoc& loc, const char* when, EType type)
    {
        return GetRoot(config, loc, when) + GetPath(type, loc.GetUniqueKey());
    }

    static string Get(const SFileTrackConfig& config,
            const CNetStorageObjectLoc& loc, const char* when, EType type, string key)
    {
        return GetRoot(config, loc, when) + GetPath(type, key);
    }

private:
    static string GetRoot(const SFileTrackConfig& config,
            const CNetStorageObjectLoc& loc, const char* when)
    {
        const auto site = loc.GetFileTrackSite();

        if (config.check_locator_site && site != config.site) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    "'foreign site objects are forbidden' " <<
                    when << " " << loc.GetLocator());
        }

        switch (site) {
        case CNetStorageObjectLoc::eFileTrack_ProdSite:
            return "https://submit.ncbi.nlm.nih.gov";

        case CNetStorageObjectLoc::eFileTrack_QASite:
            return "https://qsubmit.ncbi.nlm.nih.gov";

        default:
            return "https://dsubmit.ncbi.nlm.nih.gov";
        }
    }

    static string GetPath(EType type, string key)
    {
        switch (type) {
        case eDownload: return "/api/2.0/files/" + key + "/?format=attachment";
        case eUpload:   return "/api/2.0/uploads/binary/";
        case eLocks:    return "/api/2.0/pins/";
        default:        return "/api/2.0/files/" + key + "/";
        }
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
            SUrl::Get(config, object_loc, "on downloading", SUrl::eDownload))
{
}

SFileTrackUpload::SFileTrackUpload(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& user_header,
        SConnNetInfo* net_info) :
    SFileTrackRequest(config, object_loc,
            SUrl::Get(config, object_loc, "on uploading", SUrl::eUpload),
            net_info, user_header, fHTTP_WriteThru)
{
}

const auto kMyNcbiIdHeader = "Delegated-From-MyNCBI-ID";
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
        user_header.append(kMyNcbiIdHeader).append(": ");
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
        CHECK_IO_STATUS(m_HTTPStream, m_ObjectLoc, "on writing", true);
    }

    if (bytes_written != NULL)
        *bytes_written = count;
}

void SFileTrackUpload::FinishUpload()
{
    string expected_key = m_ObjectLoc.GetUniqueKey();

    CJsonNode upload_result = GetFileInfo("on getting key for");

    string actual_key = upload_result.GetString("key");

    if (actual_key != expected_key) {
        RenameFile(actual_key, expected_key);
    }
}

// Special overload for SFileTrackUpload::RenameFile
void s_ThrowHttpStatus(const CDiagCompileInfo& info, const string object_loc,
        int http_code, const string& http_text, const string& from, const string&)
{
    ostringstream os;

    os << "on renaming " << from << " for";

    s_ThrowHttpStatus(info, object_loc, http_code, http_text, os.str());
}

void SFileTrackUpload::RenameFile(const string& from, const string& to)
{
    CHttpSession session;
    session.SetHttpFlags(kDefaultHttpFlags);

    const auto url = SUrl::Get(m_Config, m_ObjectLoc, "on renaming", SUrl::eMeta, from);

    CHttpRequest req = session.NewRequest(url, CHttpSession::ePut);
    auto& timeout(m_Config.comm_timeout);
    req.SetTimeout(CTimeout(timeout.sec, timeout.usec));

    CHttpHeaders& headers = req.Headers();
    headers.SetValue(CHttpHeaders::eContentType, "application/json");
    headers.SetValue(kAuthHeader, m_Config.token);

    s_ApplyMyNcbiId([&headers](const string& my_ncbi_id)
    {
        headers.SetValue(kMyNcbiIdHeader, my_ncbi_id);
    });

    req.ContentStream() << "{\"key\": \"" << to << "\"}";

    CHttpResponse response(req.Execute());
    CHECK_HTTP_STATUS(response, m_ObjectLoc, from, to);
}

CJsonNode SFileTrackRequest::GetFileInfo(const char* const when)
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
                "'" << e.GetType() << "|" << e.GetErrCodeString() << "' " <<
                when << " " << m_ObjectLoc.GetLocator());
    }

    CHECK_HTTP_STATUS(m_HTTPStream, m_ObjectLoc, when);
    CHECK_IO_STATUS(m_HTTPStream, m_ObjectLoc, when, false);

    CJsonNode root;

    try {
        root = CJsonNode::ParseJSON(http_response);
    }
    catch (CStringException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "'" << e.GetType() << "|" << e.GetErrCodeString() << "' " <<
                when << " " << m_ObjectLoc.GetLocator());
    }

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

    const auto url = SUrl::Get(config, object_loc, "on removing", SUrl::eMeta);
    CHttpRequest req = session.NewRequest(url, CHttpSession::eDelete);
    auto& timeout(config.comm_timeout);
    req.SetTimeout(CTimeout(timeout.sec, timeout.usec));

    CHttpHeaders& headers = req.Headers();
    headers.SetValue(CHttpHeaders::eContentType, "application/json");
    headers.SetValue(kAuthHeader, config.token);

    CHttpResponse response(req.Execute());
    CHECK_HTTP_STATUS(response, object_loc, "on removing");
}

ERW_Result SFileTrackDownload::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (m_HTTPStream.read(static_cast<char*>(buf), count).bad()) {
        CHECK_IO_STATUS(m_HTTPStream, m_ObjectLoc, "on reading", true);
    }

    if (m_FirstRead) {
        CHECK_HTTP_STATUS(m_HTTPStream, m_ObjectLoc, "on reading");
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
    CHECK_IO_STATUS(m_HTTPStream, m_ObjectLoc, "after reading", false);
}

SFileTrackAPI::SFileTrackAPI(const SFileTrackConfig& c)
    : config(c)
{
}

CJsonNode SFileTrackAPI::GetFileInfo(const CNetStorageObjectLoc& object_loc)
{
    const auto url = SUrl::Get(config, object_loc, "on getting info for", SUrl::eMeta);

    SFileTrackRequest request(config, object_loc, url);

    return request.GetFileInfo("on getting info for");
}

string SFileTrackAPI::GetPath(const CNetStorageObjectLoc& object_loc)
{
    CHttpSession session;
    session.SetHttpFlags(kDefaultHttpFlags);

    const auto url = SUrl::Get(config, object_loc, "on locking", SUrl::eLocks);
    CHttpRequest req = session.NewRequest(url, CHttpSession::ePost);
    auto& timeout(config.comm_timeout);
    req.SetTimeout(CTimeout(timeout.sec, timeout.usec));

    CHttpHeaders& headers = req.Headers();
    headers.SetValue(CHttpHeaders::eContentType, "application/json");
    headers.SetValue(kAuthHeader, config.token);

    req.ContentStream() << "{\"file_key\": \"" <<
            object_loc.GetUniqueKey() << "\"}";

    CHttpResponse response(req.Execute());
    CHECK_HTTP_STATUS(response, object_loc, "on locking");

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
                "'" << e.GetType() << "|" << e.GetErrCodeString() <<
                "' on locking " << object_loc.GetLocator());
    }

    NCBI_THROW_FMT(CNetStorageException, eUnknown,
            "'no path in response' on locking " << object_loc.GetLocator());
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
    check_locator_site(reg.GetBool(s_GetSection(section), "check_locator_site", false)),
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
        site = value;
    } else if (name == "ft_token") {
        token = kAuthPrefix + NStr::URLDecode(s_GetDecryptedKey(value));
    } else {
        return false;
    }

    enabled = true;
    return true;
}

SFileTrackConfig::SSite SFileTrackConfig::site;

const auto kUninitialized = CNetStorageObjectLoc::eNumberOfFileTrackSites;

SFileTrackConfig::SSite::SSite() :
    value(kUninitialized)
{
}

SFileTrackConfig::SSite::operator TValue()
{
    if (value == kUninitialized) *this = CDiagContext::GetHostRole();
    return value;
}

void SFileTrackConfig::SSite::operator=(const string& ft_site_name)
{
    map<CTempString, TValue, PNocase> p =
    {
        { "production",  TValue::eFileTrack_ProdSite },
        { "prod",        TValue::eFileTrack_ProdSite },
        { "submit",      TValue::eFileTrack_ProdSite },
        { "development", TValue::eFileTrack_DevSite  },
        { "dev",         TValue::eFileTrack_DevSite  },
        { "dsubmit",     TValue::eFileTrack_DevSite  },
        { "qa",          TValue::eFileTrack_QASite   },
        { "qsubmit",     TValue::eFileTrack_QASite   },
    };

    auto i = p.find(ft_site_name);

    if (i != p.end()) {
        value = i->second;
    } else {
        NCBI_THROW_FMT(CArgException, eInvalidArg,
                "unrecognized FileTrack site '" << ft_site_name << '\'');
    }
}

END_NCBI_SCOPE
