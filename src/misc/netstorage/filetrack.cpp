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
 *   FileTrack API implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "filetrack.hpp"

#include <connect/services/ns_output_parser.hpp>

#include <connect/ncbi_gnutls.h>
#include <connect/ncbi_http_session.hpp>

#include <util/random_gen.hpp>
#include <util/util_exception.hpp>

#include <corelib/rwstream.hpp>
#include <corelib/request_status.hpp>
#include <corelib/resource_info.hpp>
#include <corelib/request_ctx.hpp>

#include <time.h>
#include <sstream>

#define FILETRACK_SIDCOOKIE "SubmissionPortalSID"

#define CONTENT_LENGTH_HEADER "Content-Length: "

BEGIN_NCBI_SCOPE

#define THROW_IO_EXCEPTION(err_code, message, status) \
        NCBI_THROW_FMT(CIOException, err_code, message << IO_StatusStr(status));

namespace { // Anonymous namespace for CNullWriter

class CNullWriter : public IWriter
{
public:
    virtual ERW_Result Write(const void* buf, size_t count, size_t* written);
    virtual ERW_Result Flush();
};

ERW_Result CNullWriter::Write(const void*, size_t count, size_t* written)
{
    if (written)
        *written = count;
    return eRW_Success;
}

ERW_Result CNullWriter::Flush()
{
    return eRW_Success;
}

} // Anonymous namespace for CNullWriter ends here.

static EHTTP_HeaderParse s_HTTPParseHeader_SaveStatus(
        const char* /*http_header*/, void* user_data, int server_error)
{
    reinterpret_cast<SFileTrackRequest*>(
            user_data)->m_HTTPStatus = server_error;

    return eHTTP_HeaderComplete;
}

static EHTTP_HeaderParse s_HTTPParseHeader_GetContentLength(
        const char* http_header, void* user_data, int server_error)
{
    SFileTrackRequest* http_request =
            reinterpret_cast<SFileTrackPostRequest*>(user_data);

    http_request->m_HTTPStatus = server_error;

    const char* content_length_begin =
        strstr(http_header, CONTENT_LENGTH_HEADER);

    if (content_length_begin != NULL) {
        size_t content_length = NStr::StringToSizet(
                content_length_begin + sizeof(CONTENT_LENGTH_HEADER) - 1,
                NStr::fConvErr_NoThrow | NStr::fAllowTrailingSymbols);
        if (content_length != 0 || errno == 0)
            http_request->m_ContentLength = content_length;
    }

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
    if (m_Config.chunked_upload) {
        SConnNetInfo* net_info(ConnNetInfo_Create(0));
        net_info->version = 1;
        return net_info;
    }

    return 0;
}

THTTP_Flags SFileTrackRequest::GetUploadFlags() const
{
    return m_Config.chunked_upload ? 
        kDefaultHttpFlags | fHTTP_WriteThru : kDefaultHttpFlags;
}

void SFileTrackRequest::SetTimeout()
{
    m_HTTPStream.SetTimeout(eIO_Close, &m_Config.read_timeout);
    m_HTTPStream.SetTimeout(eIO_Read, &m_Config.read_timeout);
}

SFileTrackRequest::SFileTrackRequest(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& url,
        FHTTP_ParseHeader parse_header) :
    m_Config(config),
    m_ObjectLoc(object_loc),
    m_URL(url),
    m_HTTPStream(url, NULL, kEmptyStr, parse_header, this, NULL,
            NULL, kDefaultHttpFlags,
            &m_Config.write_timeout)
{
    SetTimeout();
}

SFileTrackRequest::SFileTrackRequest(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& url,
        const string& user_header,
        FHTTP_ParseHeader parse_header) :
    m_Config(config),
    m_NetInfo(GetNetInfo()),
    m_ObjectLoc(object_loc),
    m_URL(url),
    m_HTTPStream(url, m_NetInfo.get(), user_header, parse_header, this, NULL,
            NULL, GetUploadFlags(),
            &m_Config.write_timeout)
{
    SetTimeout();
}

string s_GetUploadURL(const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc)
{
    auto path = config.chunked_upload ? "/api/2.0/uploads/" : "/ft/upload/";
    return s_GetURL(object_loc, path);
}

SFileTrackPostRequest::SFileTrackPostRequest(
        const SFileTrackConfig& config,
        const CNetStorageObjectLoc& object_loc,
        const string& boundary,
        const string& cookie,
        const string& user_header,
        FHTTP_ParseHeader parse_header) :
    SFileTrackRequest(config, object_loc,
            s_GetUploadURL(config, object_loc), user_header, parse_header),
    m_Boundary(boundary),
    m_Cookie(cookie)
{
}

void SFileTrackPostRequest::SendContentDisposition(const char* input_name)
{
    m_HTTPStream << "--" << m_Boundary << "\r\n"
        "Content-Disposition: form-data; name=\"" << input_name << "\"\r\n"
        "\r\n";
}

void SFileTrackPostRequest::SendFormInput(
        const char* input_name, const string& value)
{
    SendContentDisposition(input_name);

    m_HTTPStream << value << "\r\n";
}

void SFileTrackPostRequest::SendEndOfFormData()
{
    m_HTTPStream << "--" << m_Boundary << "--\r\n" << NcbiFlush;

    static const STimeout kZeroTimeout = {0};

    EIO_Status status = CONN_Wait(m_HTTPStream.GetCONN(),
            eIO_Read, &kZeroTimeout);

    if (status != eIO_Success && (status != eIO_Timeout ||
            (status = m_HTTPStream.Status(eIO_Write)) != eIO_Success)) {
        THROW_IO_EXCEPTION(eWrite,
                "Error while sending HTTP request to " <<
                m_URL << ": ", status);
    }
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

CRef<SFileTrackPostRequest> SFileTrackAPI::StartUpload(
        const CNetStorageObjectLoc& object_loc)
{
    string cookie(FILETRACK_SIDCOOKIE "=" + LoginAndGetSessionKey(object_loc));

    string boundary(GenerateUniqueBoundary());

    string user_header(MakeMutipartFormDataHeader(boundary));

    user_header.append("Cookie: ");
    user_header.append(cookie);
    user_header.append("\r\n", 2);

    const string& my_ncbi_id(CRequestContext_PassThrough().Get("my_ncbi_id"));

    if (!my_ncbi_id.empty()) {
        user_header.append("Delegated-From-MyNCBI-ID: ");
        user_header.append(my_ncbi_id);
    }

    CRef<SFileTrackPostRequest> new_request(
            new SFileTrackPostRequest(config, object_loc,
            boundary, cookie, user_header,
            s_HTTPParseHeader_SaveStatus));

    new_request->SendContentDisposition("file\"; filename=\"contents");

    return new_request;
}

void SFileTrackPostRequest::Write(const void* buf,
        size_t count, size_t* bytes_written)
{
    if (m_HTTPStream.write(reinterpret_cast<const char*>(buf), count).bad()) {
        THROW_IO_EXCEPTION(eWrite, "Error while sending data to " << m_URL,
                m_HTTPStream.Status());
    }

    if (bytes_written != NULL)
        *bytes_written = count;
}

static const STimeout kZeroTimeout = {0};

void SFileTrackPostRequest::FinishUpload()
{
    m_HTTPStream << "\r\n";

    string unique_key = m_ObjectLoc.GetUniqueKey();

    SendFormInput("expires", kEmptyStr);
    SendFormInput("owner_id", kEmptyStr);
    SendFormInput("file_id", unique_key);
    SendFormInput("editable", "true");
    SendEndOfFormData();

    try {
        EIO_Status status = CONN_Wait(m_HTTPStream.GetCONN(),
                eIO_Read, &kZeroTimeout);
        if (status != eIO_Success) {
            if (status != eIO_Timeout ||
                    (status = m_HTTPStream.Status(eIO_Write)) != eIO_Success) {
                THROW_IO_EXCEPTION(eWrite,
                        "Error while sending HTTP request to " <<
                        m_URL << ": ", status);
            }
        }
    }
    catch (CException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while uploading \"" << m_ObjectLoc.GetLocator() <<
                "\" (storage key \"" << m_ObjectLoc.GetUniqueKey() <<
                "\"); HTTP status " << m_HTTPStatus);
    }

    CJsonNode upload_result = ReadJsonResponse();

    string filetrack_file_id = upload_result.GetAt(0).GetString("file_id");

    if (filetrack_file_id != unique_key) {
        RenameFile(filetrack_file_id, unique_key);
    }
}

void SFileTrackPostRequest::RenameFile(const string& from, const string& to)
{
    string err;

    try {
        CHttpSession session;
        session.SetHttpFlags(kDefaultHttpFlags);

        string url(s_URLs[m_ObjectLoc.GetFileTrackSite()]);
        url.append("/api/2.0/files/").append(from).append("/");

        CHttpRequest req = session.NewRequest(url, CHttpSession::ePut);
        auto& timeout(m_Config.read_timeout);
        req.SetTimeout(CTimeout(timeout.sec, timeout.usec));
        req.Headers().SetValue(CHttpHeaders::eContentType, "application/json");
        req.Headers().SetValue(CHttpHeaders::eCookie, m_Cookie);
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
    }

    CJsonNode root;

    try {
        CNetScheduleStructuredOutputParser json_parser;
        root = json_parser.ParseJSON(http_response);
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
            url, s_HTTPParseHeader_GetContentLength));

    new_request->m_FirstRead = true;

    return new_request;
}

void SFileTrackAPI::Remove(const CNetStorageObjectLoc& object_loc)
{
    const string url(s_GetURL(object_loc, "/ftmeta/files/", "/__delete__"));

    SFileTrackRequest new_request(config, object_loc,
            url, s_HTTPParseHeader_GetContentLength);

    new_request.m_HTTPStream << NcbiEndl;

    CNullWriter null_writer;
    CWStream null_stream(&null_writer);

    NcbiStreamCopy(null_stream, new_request.m_HTTPStream);

    if (new_request.m_HTTPStatus == CRequestStatus::e404_NotFound) {
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Cannot remove \"" << new_request.m_ObjectLoc.GetLocator() <<
                "\" (HTTP status " << new_request.m_HTTPStatus << ").");
    }
}

ERW_Result SFileTrackRequest::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (m_HTTPStream.read(reinterpret_cast<char*>(buf), count).bad()) {
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
    m_Random.Randomize();

#ifdef HAVE_LIBGNUTLS
    DEFINE_STATIC_FAST_MUTEX(s_TLSSetupMutex);

    static bool s_TLSSetupIsDone = false;

    if (!s_TLSSetupIsDone) {
        CFastMutexGuard guard(s_TLSSetupMutex);
        if (!s_TLSSetupIsDone)
            SOCK_SetupSSL(NcbiSetupGnuTls);
        s_TLSSetupIsDone = true;
    }
#else
    NCBI_USER_THROW("SSL support is not available in this build");
#endif /*HAVE_LIBGNUTLS*/
}

static EHTTP_HeaderParse s_HTTPParseHeader_GetSID(const char* http_header,
        void* user_data, int /*server_error*/)
{
    const char* sid_begin = strstr(http_header, FILETRACK_SIDCOOKIE);

    if (sid_begin != NULL) {
        sid_begin += sizeof(FILETRACK_SIDCOOKIE) - 1;
        while (*sid_begin == '=' || *sid_begin == ' ')
            ++sid_begin;
        const char* sid_end = sid_begin;
        while (*sid_end != '\0' && *sid_end != ';' &&
                *sid_end != '\r' && *sid_end != '\n' &&
                *sid_end != ' ' && *sid_end != '\t')
            ++sid_end;
        reinterpret_cast<string*>(user_data)->assign(sid_begin, sid_end);
    }

    return eHTTP_HeaderComplete;
}

string SFileTrackAPI::LoginAndGetSessionKey(const CNetStorageObjectLoc& object_loc)
{
    string api_key(config.key);

    // Nocreate mode
    if (api_key.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Not allowed in nocreate mode (no FT API key provided).");
    }

    const string url(s_GetURL(object_loc, "/accounts/api_login?key=") + api_key);
    string session_key;

    CConn_HttpStream http_stream(url,
            NULL, kEmptyStr, s_HTTPParseHeader_GetSID, &session_key, NULL, NULL,
            fHTTP_AutoReconnect, &config.write_timeout);

    string dummy;
    http_stream >> dummy;

    if (session_key.empty()) {
        // Server will report this exception to clients, so API key needs jamming
        const size_t kSize = 8;
        if (api_key.size() > kSize) {
            api_key.replace(kSize / 2, api_key.size() - kSize, "...");
        }

        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Error while uploading data to FileTrack: "
                "authentication error (API key = " << api_key <<
                ", URL = " << url << ").");
    }

    return session_key;
}

CJsonNode SFileTrackAPI::GetFileInfo(const CNetStorageObjectLoc& object_loc)
{
    const string url(s_GetURL(object_loc, "/ftmeta/files/", "/"));

    SFileTrackRequest request(config, object_loc, url,
            s_HTTPParseHeader_SaveStatus);

    return request.ReadJsonResponse();
}

string SFileTrackAPI::GenerateUniqueBoundary()
{
    string boundary("FileTrack-" + NStr::NumericToString(time(NULL)));
    boundary += '-';
    boundary += NStr::NumericToString(m_Random.GetRandUint8());

    return boundary;
}

string SFileTrackAPI::MakeMutipartFormDataHeader(const string& boundary)
{
    string header("Content-Type: multipart/form-data; boundary=" + boundary);

    header.append("\r\n", 2);

    return header;
}

string SFileTrackAPI::GetPath(const CNetStorageObjectLoc& object_loc)
{
    CHttpSession session;
    session.SetHttpFlags(kDefaultHttpFlags);

    const string url(s_GetURL(object_loc, "/api/2.0/pins/"));
    CHttpRequest req = session.NewRequest(url, CHttpSession::ePost);
    req.SetTimeout(CTimeout(config.read_timeout.sec, config.read_timeout.usec));
    req.Headers().SetValue(CHttpHeaders::eContentType, "application/json");
    req.Headers().SetValue(CHttpHeaders::eCookie, FILETRACK_SIDCOOKIE "=" +
            LoginAndGetSessionKey(object_loc));

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
    CNetScheduleStructuredOutputParser parser;

    try {
        // If there is already "path" in response
        if (CJsonNode root = parser.ParseJSON(oss.str())) {
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

const STimeout s_GetDefaultTimeout()
{
    STimeout result;
    result.sec = 30;
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
    read_timeout(s_GetDefaultTimeout()),
    write_timeout(s_GetDefaultTimeout())
{
}

SFileTrackConfig::SFileTrackConfig(const IRegistry& reg, const string& section) :
    site(GetSite(reg.GetString(s_GetSection(section), "site", "prod"))),
    key(reg.GetEncryptedString(s_GetSection(section), "api_key",
                IRegistry::fPlaintextAllowed)),
    chunked_upload(reg.GetBool(s_GetSection(section), "chunked_upload",
                false, 0, IRegistry::eReturn)),
    read_timeout(s_GetDefaultTimeout()),
    write_timeout(s_GetDefaultTimeout())
{
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
