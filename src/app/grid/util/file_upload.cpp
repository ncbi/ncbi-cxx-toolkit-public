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
 * File Description: CGI to upload a file to NetStorage or NetCache.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netstorage.hpp>
#include <connect/services/grid_app_version_info.hpp>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>

#include <util/md5.hpp>

USING_NCBI_SCOPE;

enum EErrorMessageVerbosity {
    eEMV_Verbose,
    eEMV_Terse,
    eEMV_LogRef
};

class CFileUploadApplication : public CCgiApplication
{
public:
    CFileUploadApplication();
    void Init();
    int  ProcessRequest(CCgiContext& ctx);

private:
    struct SContext
    {
        CJsonNode json;
        string file;

        SContext(const string& f) : json(CJsonNode::NewObjectNode()), file(f) {}
    };

    template <class What>
    void Error(SContext&, const What&, const string& = kEmptyStr) const;
    void Copy(SContext&, CCgiEntry&, TNetStorageFlags);

    CNetStorage m_NetStorage;
    string m_SourceFieldName;
    EErrorMessageVerbosity m_ErrorMessageVerbosity;
};

#define GRID_APP_NAME "file_upload.cgi"

CFileUploadApplication::CFileUploadApplication()
    : m_NetStorage(eVoid)
{
}

void CFileUploadApplication::Init()
{
    const string kSection = "file_upload";
    const string kInitString = "destination";
    const string kVerbosity = "error_message";

    CCgiApplication::Init();

    SetRequestFlags(CCgiRequest::fParseInputOnDemand);

    const IRegistry& reg = GetConfig();

    string init_string(reg.Get(kSection, kInitString));

    if (init_string.empty()) {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
                "Missing configuration parameter [" <<
                kSection << "]." << kInitString);
    }

    m_SourceFieldName = reg.Get(kSection, "source_field_name");
    if (m_SourceFieldName.empty())
        m_SourceFieldName = "file";

    string error_message_verbosity(reg.Get(kSection, kVerbosity));

    if (error_message_verbosity.empty())
#ifndef _DEBUG
        m_ErrorMessageVerbosity = eEMV_LogRef;
#else
        m_ErrorMessageVerbosity = eEMV_Verbose;
#endif
    else if (NStr::CompareNocase(error_message_verbosity, "verbose") == 0)
        m_ErrorMessageVerbosity = eEMV_Verbose;
    else if (NStr::CompareNocase(error_message_verbosity, "terse") == 0)
        m_ErrorMessageVerbosity = eEMV_Terse;
    else if (NStr::CompareNocase(error_message_verbosity, "logref") == 0)
        m_ErrorMessageVerbosity = eEMV_LogRef;
    else {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
                "Invalid '" << kVerbosity << "' value '" <<
                        error_message_verbosity << '\'');
    }

    m_NetStorage = CNetStorage(init_string);
}

template <class What>
void CFileUploadApplication::Error(SContext& ctx, const What& what,
        const string& msg) const
{
    ctx.json.SetBoolean("success", false);

    string err_msg("Error while uploading ");
    err_msg.append(ctx.file);
    err_msg.append(": ", 2);

    if (m_ErrorMessageVerbosity != eEMV_LogRef) {
        ERR_POST(err_msg << what);
        err_msg += msg;
    } else {
        CCompoundID id = CCompoundIDPool().NewID(eCIC_GenericID);
        id.AppendCurrentTime();
        id.AppendRandom();
        const string& logref = id.ToString();

        ERR_POST('[' << logref << "] " << err_msg << what);
        GetDiagContext().Extra().Print("logref", logref);

        if (msg.empty()) {
            err_msg += logref;
        } else {
            err_msg += msg + " [" + logref + ']';
        }
    }

    ctx.json.SetString("msg", err_msg);
}

void CFileUploadApplication::Copy(SContext& ctx, CCgiEntry& entry,
        TNetStorageFlags flags)
{
    const string& filename_from_entry = entry.GetFilename();
    if (!filename_from_entry.empty()) {
        ctx.json.SetString("name", ctx.file = filename_from_entry);
    }

    auto_ptr<IReader> reader(entry.GetValueReader());
    CNetStorageObject netstorage_object = m_NetStorage.Create(flags);
    IEmbeddedStreamWriter& writer(netstorage_object.GetWriter());

    char buf[16384];
    CMD5 sum;
    size_t length = 0;
    for (;;) {
        size_t read = 0;
        ERW_Result read_result = reader->Read(buf, sizeof(buf), &read);

        if (read_result != eRW_Success && read_result != eRW_Eof) {
            Error(ctx, g_RW_ResultToString(read_result));
            return;
        }

        sum.Update(buf, read);
        length += read;

        size_t written = 0;
        for (const char *ptr = buf; read > 0; read -= written) {
            ERW_Result write_result = writer.Write(ptr, read, &written);

            if (write_result != eRW_Success) {
                Error(ctx, g_RW_ResultToString(write_result));
                return;
            }
        }

        if (read_result == eRW_Eof) {
            netstorage_object.Close();
            string key = netstorage_object.GetLoc();
            string md5 = sum.GetHexSum();
            ctx.json.SetBoolean("success", true);
            ctx.json.SetString("key", key);
            ctx.json.SetString("md5", md5);
            GetDiagContext().Extra().
                Print("key", key).
                Print("md5", md5).
                Print("length", length);
            return;
        }
    }
}

TNetStorageFlags s_GetFlags(const string& dst)
{
    if (!NStr::CompareNocase(dst, "netcache"))
        return fNST_NetCache;
    else if (!NStr::CompareNocase(dst, "filetrack"))
        return fNST_FileTrack;
    else if (!NStr::CompareNocase(dst, "fast"))
        return fNST_Fast;
    else if (!NStr::CompareNocase(dst, "persistent"))
        return fNST_Persistent;
    else
        throw std::runtime_error("Invalid dst value: " + dst);
}

int CFileUploadApplication::ProcessRequest(CCgiContext& ctx)
{
    CCgiRequest& request = ctx.GetRequest();
    CCgiResponse& response = ctx.GetResponse();

    response.SetContentType("application/json");
    SContext app_ctx("input data");

    try {
        CCgiEntry file(request.GetEntry(m_SourceFieldName));

        if (file.empty()) {
            app_ctx.json.SetBoolean("success", false);
            app_ctx.json.SetString("msg", "Nothing to upload");
        } else {
            bool dst_found = false;
            const CCgiEntry& dst(request.GetEntry("dst", &dst_found));
            Copy(app_ctx, file, dst_found ? s_GetFlags(dst) : 0);
        }
    }
    catch (CException& e) {
        string msg;

        switch (m_ErrorMessageVerbosity) {
            case eEMV_Verbose:
                msg = e.ReportAll();
                break;
            case eEMV_Terse:
                msg = e.GetMsg();
                break;
            default:
            {
                const char* err_code = e.GetErrCodeString();
                if (*err_code == 'e')
                    ++err_code;
                msg = err_code;
            }
        }

        Error(app_ctx, e, msg);
    }
    catch (exception& e) {
        if (m_ErrorMessageVerbosity == eEMV_LogRef) {
            Error(app_ctx, e.what());
        } else {
            Error(app_ctx, e.what(), e.what());
        }
    }

    response.WriteHeader();
    response.out() << app_ctx.json.Repr();

    return 0;
}

int main(int argc, char** argv)
{
    GRID_APP_CHECK_VERSION_ARGS();

    SetSplitLogFile(true);
    GetDiagContext().SetOldPostFormat(false);

    return CFileUploadApplication().AppMain(argc, argv);
}
