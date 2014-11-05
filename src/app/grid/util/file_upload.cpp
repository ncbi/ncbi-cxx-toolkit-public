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

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>

USING_NCBI_SCOPE;

enum EErrorMessageVerbosity {
    eEMV_Verbose,
    eEMV_Terse,
    eEMV_LogRef
};

class CFileUploadApplication : public CCgiApplication
{
public:
    void Init();
    int  ProcessRequest(CCgiContext& ctx);

private:
    CNetStorage m_NetStorage;
    string m_SourceFieldName;
    EErrorMessageVerbosity m_ErrorMessageVerbosity;
};

#define CONFIG_SECTION "file_upload"
#define INIT_STRING_PARAM "destination"
#define ERROR_MESSAGE_VERBOSITY_PARAM "error_message"

void CFileUploadApplication::Init()
{
    CCgiApplication::Init();

    SetRequestFlags(CCgiRequest::fParseInputOnDemand);

    const IRegistry& reg = GetConfig();

    string init_string(reg.Get(CONFIG_SECTION, INIT_STRING_PARAM));

    if (init_string.empty()) {
        NCBI_THROW(CConfigException, eParameterMissing,
                "Missing configuration parameter ["
                CONFIG_SECTION "]." INIT_STRING_PARAM);
    }

    m_SourceFieldName = reg.Get(CONFIG_SECTION, "source_field_name");
    if (m_SourceFieldName.empty())
        m_SourceFieldName = "file";

    string error_message_verbosity(reg.Get(CONFIG_SECTION,
            ERROR_MESSAGE_VERBOSITY_PARAM));

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
                "Invalid '" ERROR_MESSAGE_VERBOSITY_PARAM "' value '" <<
                        error_message_verbosity << '\'');
    }

    m_NetStorage = CNetStorage(init_string);
}

static string s_MakeLogRef()
{
    CCompoundID logref = CCompoundIDPool().NewID(eCIC_GenericID);
    logref.AppendCurrentTime();
    logref.AppendRandom();
    return logref.ToString();
}

int CFileUploadApplication::ProcessRequest(CCgiContext& ctx)
{
    CCgiRequest& request = ctx.GetRequest();
    CCgiResponse& response = ctx.GetResponse();

    response.SetContentType("application/json");

    CJsonNode response_json = CJsonNode::NewObjectNode();

    string filename("input data");

    try {
        for (;;) {
            TCgiEntriesI it = request.GetNextEntry();

            if (it == request.GetEntries().end()) {
                response_json.SetBoolean("success", false);
                response_json.SetString("msg", "Nothing to upload");
                break;
            }

            if (it->first == m_SourceFieldName) {
                response_json.SetBoolean("success", true);

                CCgiEntry& entry(it->second);

                const string& filename_from_entry = entry.GetFilename();
                if (!filename_from_entry.empty())
                    response_json.SetString("name",
                            filename = filename_from_entry);

                auto_ptr<CNcbiIstream> input_stream(entry.GetValueStream());

                CNetStorageObject netstorage_object = m_NetStorage.Create();

                CWStream output_stream(&netstorage_object.GetWriter(), 0, 0,
                        CRWStreambuf::fLeakExceptions);

                NcbiStreamCopy(output_stream, *input_stream);

                netstorage_object.Close();

                response_json.SetString("key", netstorage_object.GetLoc());

                break;
            }
        }
    }
    catch (CException& e) {
        response_json.SetBoolean("success", false);

        string err_msg("Error while uploading ");
        err_msg.append(filename);
        err_msg.append(": ", 2);

        if (m_ErrorMessageVerbosity != eEMV_LogRef) {
            ERR_POST(err_msg << e);

            response_json.SetString("msg", err_msg +
                    (m_ErrorMessageVerbosity == eEMV_Verbose ?
                            e.ReportAll() : e.GetMsg()));
        } else {
            string logref(s_MakeLogRef());

            ERR_POST('[' << logref << "] " << err_msg << e);
            GetDiagContext().Extra().Print("logref", logref);

            const char* err_code = e.GetErrCodeString();
            if (*err_code == 'e')
                ++err_code;
            string error_details(err_code);
            error_details.append(" [", 2);
            error_details.append(logref);
            error_details.append(1, ']');

            response_json.SetString("msg", err_msg + error_details);
        }
    }
    catch (exception& e) {
        response_json.SetBoolean("success", false);

        string err_msg("Error while uploading ");
        err_msg.append(filename);
        err_msg.append(": ", 2);

        if (m_ErrorMessageVerbosity != eEMV_LogRef) {
            ERR_POST(err_msg << e.what());

            response_json.SetString("msg", err_msg + e.what());
        } else {
            string logref(s_MakeLogRef());

            ERR_POST('[' << logref << "] " << err_msg << e.what());
            GetDiagContext().Extra().Print("logref", logref);

            response_json.SetString("msg", err_msg + logref);
        }
    }

    response.WriteHeader();
    response.out() << response_json.Repr();

    return 0;
}

int main(int argc, char** argv)
{
    return CFileUploadApplication().AppMain(argc, argv, 0, eDS_Default);
}
