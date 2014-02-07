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

#include <connect/services/netstorage.hpp>
#include <connect/services/ns_output_parser.hpp>

#include <misc/netstorage/netstorage.hpp>
#include <misc/netstorage/error_codes.hpp>

#include <connect/ncbi_gnutls.h>

#include <util/random_gen.hpp>
#include <util/util_exception.hpp>

#include <corelib/rwstream.hpp>
#include <corelib/request_status.hpp>

#include <time.h>

#define NCBI_USE_ERRCODE_X  NetStorage_FileTrack

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

SFileTrackRequest::SFileTrackRequest(
        SFileTrackAPI* storage_impl,
        CNetStorageObjectID* object_id,
        const string& url,
        const string& user_header,
        FHTTP_ParseHeader parse_header) :
    m_FileTrackAPI(storage_impl),
    m_ObjectID(object_id),
    m_URL(url),
    m_HTTPStream(url, NULL, user_header, parse_header, this, NULL,
            NULL, fHTTP_AutoReconnect, &storage_impl->m_WriteTimeout),
    m_HTTPStatus(0),
    m_ContentLength((size_t) -1)
{
    m_HTTPStream.SetTimeout(eIO_Close, &storage_impl->m_ReadTimeout);
    m_HTTPStream.SetTimeout(eIO_Read, &storage_impl->m_ReadTimeout);
}

SFileTrackPostRequest::SFileTrackPostRequest(
        SFileTrackAPI* storage_impl,
        CNetStorageObjectID* object_id,
        const string& url,
        const string& boundary,
        const string& user_header,
        FHTTP_ParseHeader parse_header) :
    SFileTrackRequest(storage_impl, object_id, url, user_header, parse_header),
    m_Boundary(boundary)
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
        CNetStorageObjectID* object_id)
{
    string session_key(LoginAndGetSessionKey(object_id));

    string boundary(GenerateUniqueBoundary());

    string user_header(MakeMutipartFormDataHeader(boundary));

    user_header.append("Cookie: " FILETRACK_SIDCOOKIE "=");
    user_header.append(session_key);
    user_header.append("\r\n", 2);

    CRef<SFileTrackPostRequest> new_request(
            new SFileTrackPostRequest(this, object_id,
            object_id->GetFileTrackURL() + "/ft/upload/", boundary, user_header,
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

    string unique_key = m_ObjectID->GetUniqueKey();

    SendFormInput("expires", kEmptyStr);
    SendFormInput("owner_id", kEmptyStr);
    SendFormInput("file_id", unique_key);
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
                "Error while uploading \"" << m_ObjectID->GetID() <<
                "\" (storage key \"" << m_ObjectID->GetUniqueKey() <<
                "\"); HTTP status " << m_HTTPStatus);
    }

    CJsonNode upload_result = ReadJsonResponse();

    string filetrack_file_id = upload_result.GetAt(0).GetString("file_id");

    if (filetrack_file_id != unique_key) {
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "Error while uploading \"" << m_ObjectID->GetID() <<
                "\" to FileTrack: the file has been stored as \"" <<
                filetrack_file_id << "\" instead of "
                "\"" << unique_key << "\" as requested.");
    }
}

CJsonNode SFileTrackRequest::ReadJsonResponse()
{
    string http_response;

    try {
        CNcbiOstrstream sstr;
        NcbiStreamCopy(sstr, m_HTTPStream);
        sstr << NcbiEnds;
        http_response = sstr.str();
    }
    catch (CException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while accessing \"" << m_ObjectID->GetID() <<
                "\" (storage key \"" << m_ObjectID->GetUniqueKey() <<
                "\"); HTTP status " << m_HTTPStatus);
    }

    CJsonNode root;

    try {
        CNetScheduleStructuredOutputParser json_parser;
        root = json_parser.ParseJSON(http_response);
    }
    catch (CStringException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while accessing \"" << m_ObjectID->GetID() <<
                "\" (storage key \"" << m_ObjectID->GetUniqueKey() << "\"): " <<
                s_RemoveHTMLTags(http_response.c_str()) <<
                " (HTTP status " << m_HTTPStatus << ')');
    }

    CheckIOStatus();

    return root;
}

CRef<SFileTrackRequest> SFileTrackAPI::StartDownload(
        CNetStorageObjectID* object_id)
{
    string url(object_id->GetFileTrackURL() + "/ft/byid/");
    url += object_id->GetUniqueKey();
    url += "/contents";

    CRef<SFileTrackRequest> new_request(new SFileTrackRequest(this, object_id,
            url, kEmptyStr, s_HTTPParseHeader_GetContentLength));

    new_request->m_FirstRead = true;

    return new_request;
}

void SFileTrackAPI::Remove(CNetStorageObjectID* object_id)
{
    string url(object_id->GetFileTrackURL() + "/ftmeta/files/");
    url += object_id->GetUniqueKey();
    url += "/__delete__";

    CRef<SFileTrackRequest> new_request(new SFileTrackRequest(this, object_id,
            url, kEmptyStr, s_HTTPParseHeader_GetContentLength));

    new_request->m_HTTPStream << NcbiEndl;

    CNullWriter null_writer;
    CWStream null_stream(&null_writer);

    NcbiStreamCopy(null_stream, new_request->m_HTTPStream);
}

ERW_Result SFileTrackRequest::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (m_HTTPStream.read(reinterpret_cast<char*>(buf), count).bad()) {
        THROW_IO_EXCEPTION(eWrite, "Error while reading data from " << m_URL,
                m_HTTPStream.Status());
    }

    if (m_FirstRead) {
        if (m_HTTPStatus >= 400) {
            NCBI_THROW_FMT(CNetStorageException, eNotExists,
                    "Cannot open \"" << m_ObjectID->GetID() <<
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

SFileTrackAPI::SFileTrackAPI()
{
    m_Random.Randomize();

    m_WriteTimeout.sec = m_ReadTimeout.sec = 5;
    m_WriteTimeout.usec = m_ReadTimeout.usec = 0;

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

string SFileTrackAPI::GetAPIKey()
{
    return TFileTrack_APIKey::GetDefault();
}

string SFileTrackAPI::LoginAndGetSessionKey(CNetStorageObjectID* object_id)
{
    string api_key(GetAPIKey());

    string url(object_id->GetFileTrackURL() + "/accounts/api_login?key=");
    url += api_key;

    string session_key;

    CConn_HttpStream http_stream(url, NULL, kEmptyStr,
            s_HTTPParseHeader_GetSID, &session_key, NULL, NULL,
            fHTTP_AutoReconnect, &m_WriteTimeout);

    string dummy;
    http_stream >> dummy;

    if (session_key.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Error while uploading data to FileTrack: "
                "authentication error (API key = " << api_key << ").");
    }

    return session_key;
}

CJsonNode SFileTrackAPI::GetFileInfo(CNetStorageObjectID* object_id)
{
    string url(object_id->GetFileTrackURL() + "/ftmeta/files/");
    url += object_id->GetUniqueKey();
    url += '/';

    SFileTrackRequest request(this, object_id, url,
            kEmptyStr, s_HTTPParseHeader_SaveStatus);

    return request.ReadJsonResponse();
}

string SFileTrackAPI::GetFileAttribute(CNetStorageObjectID* object_id,
        const string& attr_name)
{
    string url(object_id->GetFileTrackURL() + "/ftmeta/files/");
    url += object_id->GetUniqueKey();
    url += "/attribs/";
    url += attr_name;

    SFileTrackRequest request(this, object_id, url,
            kEmptyStr, s_HTTPParseHeader_SaveStatus);

    string http_response;

    try {
        CNcbiOstrstream sstr;
        NcbiStreamCopy(sstr, request.m_HTTPStream);
        sstr << NcbiEnds;
        http_response = sstr.str();
    }
    catch (CException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while reading attribute \"" << attr_name <<
                "\" of \"" << object_id->GetID() <<
                "\" (storage key \"" << object_id->GetUniqueKey() <<
                "\"); HTTP status " << request.m_HTTPStatus);
    }

    if (request.m_HTTPStatus > 0 &&
            request.m_HTTPStatus != CRequestStatus::e200_Ok) {
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Error while reading attribute \"" << attr_name <<
                "\" of \"" << object_id->GetID() <<
                "\" (storage key \"" << object_id->GetUniqueKey() <<
                "\"); HTTP status " << request.m_HTTPStatus <<
                ": " << s_RemoveHTMLTags(http_response.c_str()));
    }

    return http_response;
}

void SFileTrackAPI::SetFileAttribute(CNetStorageObjectID* object_id,
        const string& attr_name, const string& attr_value)
{
    string session_key(LoginAndGetSessionKey(object_id));

    string boundary(GenerateUniqueBoundary());

    string user_header(MakeMutipartFormDataHeader(boundary));

    user_header.append("Cookie: " FILETRACK_SIDCOOKIE "=");
    user_header.append(session_key);
    user_header.append("\r\n", 2);

    string url(object_id->GetFileTrackURL() + "/ftmeta/files/");
    url += object_id->GetUniqueKey();
    url += "/attribs/";

    SFileTrackPostRequest request(this, object_id, url, boundary,
            user_header, s_HTTPParseHeader_SaveStatus);

    request.SendFormInput("cmd", "set");
    request.SendFormInput("name", attr_name);
    request.SendFormInput("value", attr_value);
    request.SendEndOfFormData();

    try {
        EIO_Status status = CONN_Wait(request.m_HTTPStream.GetCONN(),
                eIO_Read, &kZeroTimeout);
        if (status != eIO_Success) {
            if (status != eIO_Timeout ||
                    (status = request.m_HTTPStream.Status(eIO_Write)) != eIO_Success) {
                THROW_IO_EXCEPTION(eWrite,
                        "Error while sending HTTP request to " <<
                        url << ": ", status);
            }
        }
    }
    catch (CException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while uploading \"" << object_id->GetID() <<
                "\" (storage key \"" << object_id->GetUniqueKey() <<
                "\"); HTTP status " << request.m_HTTPStatus);
    }

    string http_response;

    try {
        CNcbiOstrstream sstr;
        NcbiStreamCopy(sstr, request.m_HTTPStream);
        sstr << NcbiEnds;
        http_response = sstr.str();
    }
    catch (CException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while accessing \"" << object_id->GetID() <<
                "\" (storage key \"" << object_id->GetUniqueKey() <<
                "\"); HTTP status " << request.m_HTTPStatus);
    }

    request.CheckIOStatus();
}

Uint8 SFileTrackAPI::GetRandom()
{
    CFastMutexGuard guard(m_RandomMutex);

    return m_Random.GetRand() * m_Random.GetRand();
}

string SFileTrackAPI::GenerateUniqueBoundary()
{
    string boundary("FileTrack-" + NStr::NumericToString(time(NULL)));
    boundary += '-';
    boundary += NStr::NumericToString(GetRandom());

    return boundary;
}

string SFileTrackAPI::MakeMutipartFormDataHeader(const string& boundary)
{
    string header("Content-Type: multipart/form-data; boundary=" + boundary);

    header.append("\r\n", 2);

    return header;
}

END_NCBI_SCOPE
