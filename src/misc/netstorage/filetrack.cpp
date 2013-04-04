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

#include <misc/netstorage/netstorage.hpp>
#include <misc/netstorage/error_codes.hpp>
#include <misc/netstorage/FT_Upload.hpp>

#include <connect/ncbi_gnutls.h>

#include <util/random_gen.hpp>
#include <util/util_exception.hpp>

#include <serial/objistr.hpp>

#include <corelib/rwstream.hpp>

#include <time.h>

#define NCBI_USE_ERRCODE_X  NetStorage_FileTrack

#define FILETRACK_BASEURL "https://dsubmit.ncbi.nlm.nih.gov"

#define FILETRACK_APIKEY \
    "bqyMqDEHsQ3foORxBO87FMNhXjv9LxuzF9Rbs4HLuiaf2pHOku7D9jDRxxyiCtp2"

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
    reinterpret_cast<SFileTrackRequest*>(user_data)->m_HTTPStatus = server_error;

    return eHTTP_HeaderComplete;
}

static EHTTP_HeaderParse s_HTTPParseHeader_GetContentLength(
        const char* http_header, void* user_data, int server_error)
{
    SFileTrackRequest* http_request =
            reinterpret_cast<SFileTrackRequest*>(user_data);

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
        CNetFileID* file_id,
        const string& url,
        const string& boundary,
        const string& user_header,
        FHTTP_ParseHeader parse_header) :
    m_FileID(file_id),
    m_URL(url),
    m_Boundary(boundary),
    m_HTTPStatus(0),
    m_ContentLength((size_t) -1),
    m_HTTPStream(url, NULL, user_header, parse_header, this, NULL,
            NULL, fHTTP_AutoReconnect, &storage_impl->m_WriteTimeout)
{
    m_HTTPStream.SetTimeout(eIO_Close, &storage_impl->m_ReadTimeout);
    m_HTTPStream.SetTimeout(eIO_Read, &storage_impl->m_ReadTimeout);
}

void SFileTrackRequest::SendContentDisposition(const char* input_name)
{
    m_HTTPStream << "--" << m_Boundary << "\r\n"
        "Content-Disposition: form-data; name=\"" << input_name << "\"\r\n"
        "\r\n";
}

void SFileTrackRequest::SendFormInput(const char* input_name, const string& value)
{
    SendContentDisposition(input_name);

    m_HTTPStream << value << "\r\n";
}

void SFileTrackRequest::SendEndOfFormData()
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

    while (isspace(*text))
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

    return result;
}

CRef<SFileTrackRequest> SFileTrackAPI::StartUpload(CNetFileID* file_id)
{
    string session_key(LoginAndGetSessionKey());

    string boundary(GenerateUniqueBoundary());

    string user_header(MakeMutipartFormDataHeader(boundary));

    user_header.append("Cookie: " FILETRACK_SIDCOOKIE "=");
    user_header.append(session_key);
    user_header.append("\r\n", 2);

    CRef<SFileTrackRequest> new_request(new SFileTrackRequest(this, file_id,
            FILETRACK_BASEURL "/ft/upload/", boundary, user_header,
            s_HTTPParseHeader_SaveStatus));

    new_request->SendContentDisposition("file\"; filename=\"contents");

    return new_request;
}

void SFileTrackRequest::Write(const void* buf,
        size_t count, size_t* bytes_written)
{
    if (m_HTTPStream.write(reinterpret_cast<const char*>(buf), count).bad()) {
        THROW_IO_EXCEPTION(eWrite, "Error while sending data to " << m_URL,
                m_HTTPStream.Status());
    }

    if (bytes_written != NULL)
        *bytes_written = count;
}

void SFileTrackRequest::FinishUpload()
{
    static const STimeout kZeroTimeout = {0};

    m_HTTPStream << "\r\n";

    SendFormInput("expires", kEmptyStr);
    SendFormInput("owner_id", kEmptyStr);
    SendFormInput("file_id", m_FileID->GetUniqueKey());
    SendEndOfFormData();

    objects::CFT_Upload obj;

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
        auto_ptr<CObjectIStream> is(
                CObjectIStream::Open(eSerial_Json, m_HTTPStream));
        is->ReadObject(&obj, obj.GetThisTypeInfo());
    }
    catch (CSerialException&) {
        string error;
        char err_buf[1024];

        while (m_HTTPStream.good()) {
            m_HTTPStream.getline(err_buf, sizeof(err_buf));
            if (NStr::Find(err_buf, "error", 0, NPOS,
                    NStr::eFirst, NStr::eCase) != NPOS) {
                error = s_RemoveHTMLTags(err_buf);
                break;
            }
        }

        CWStream null(new CNullWriter, 0, 0, CRWStreambuf::fOwnWriter);
        NcbiStreamCopy(null, m_HTTPStream);

        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "Error while uploading \"" << m_FileID->GetID() << "\": " <<
                error << " (HTTP status " << m_HTTPStatus << ')');
    }
    catch (CException& e) {
        NCBI_RETHROW_FMT(e, CNetStorageException, eIOError,
                "Error while uploading \"" << m_FileID->GetID() <<
                "\" (HTTP status " << m_HTTPStatus << ')');
    }

    CheckIOStatus();
}

CRef<SFileTrackRequest> SFileTrackAPI::StartDownload(CNetFileID* file_id)
{
    string url(FILETRACK_BASEURL "/ft/byid/" + file_id->GetUniqueKey());
    url += "/contents";

    CRef<SFileTrackRequest> new_request(new SFileTrackRequest(this, file_id,
            url, kEmptyStr, kEmptyStr, s_HTTPParseHeader_GetContentLength));

    new_request->m_FirstRead = true;

    return new_request;
}

ERW_Result SFileTrackRequest::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (m_HTTPStream.read(reinterpret_cast<char*>(buf), count).bad()) {
        THROW_IO_EXCEPTION(eWrite, "Error while reading data from " << m_URL,
                m_HTTPStream.Status());
    }

    if (m_FirstRead) {
        if (m_HTTPStatus >= 400) {
            NCBI_THROW_FMT(CNetStorageException, eNotExist,
                    "Cannot open \"" << m_FileID->GetID() <<
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

SFileTrackAPI::SFileTrackAPI() : m_Random((Uint4) time(NULL))
{
    // TODO: m_Random.Randomize();

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

string SFileTrackAPI::LoginAndGetSessionKey()
{
    string session_key;

    CConn_HttpStream http_stream(
            FILETRACK_BASEURL "/accounts/api_login?key=" FILETRACK_APIKEY,
            NULL, kEmptyStr, s_HTTPParseHeader_GetSID, &session_key, NULL, NULL,
            fHTTP_AutoReconnect, &m_WriteTimeout);

    string dummy;
    http_stream >> dummy;

    return session_key;
}

void SFileTrackAPI::SetFileTrackAttribute(CNetFileID* file_id,
        const string& attr_name, const string& attr_value)
{
    string url(FILETRACK_BASEURL "/ftmeta/files/" + file_id->GetUniqueKey());
    url += "/attribs/";

    SFileTrackRequest request(this, file_id, url, kEmptyStr,
            kEmptyStr, s_HTTPParseHeader_SaveStatus);

    request.SendFormInput("cmd", "set");
    request.SendFormInput("name", attr_name);
    request.SendFormInput("value", attr_value);
    request.SendEndOfFormData();
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
