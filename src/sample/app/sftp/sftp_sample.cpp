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
 * Author: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_sftp.hpp>
#include <connect/ncbi_conn_stream.hpp>

USING_NCBI_SCOPE;

class CSftpSampleApp : public CNcbiApplication
{
public:
    virtual void Init();
    virtual int Run();
};

void CSftpSampleApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "SFTP API sample");
    arg_desc->AddDefaultKey("user", "USER", "User to use", CArgDescriptions::eString, kEmptyStr);
    arg_desc->AddDefaultKey("password", "PASSWORD", "Password to use", CArgDescriptions::eString, kEmptyStr);
    arg_desc->AddDefaultKey("path", "PATH", "Path to start with", CArgDescriptions::eString, kEmptyStr);
    arg_desc->AddDefaultKey("input-file", "FILENAME", "File containing commands and data", CArgDescriptions::eInputFile, "-");
    arg_desc->AddFlag("ftp", "Use FTP instead of SFTP");
    arg_desc->AddFlag("echo", "Echo input data");
    arg_desc->AddFlag("stream-flags", "Output stream state flags after each comand");
    arg_desc->AddPositional("HOST", "Host to connect to", CArgDescriptions::eString);
    SetupArgDescriptions(arg_desc.release());
}

bool s_GetCommandLine(istream& is, string& line, bool echo)
{
    cout << "================================================================================\n";

    if (!getline(is, line)) {
        return false;
    }

    if (echo) {
        cout << line << endl;
    }

    return true;
}

int CSftpSampleApp::Run()
{
    const auto& args = GetArgs();
    const auto& host = args["HOST"].AsString();
    const auto& user = args["user"].AsString();
    const auto& password = args["password"].AsString();
    const auto echo = args["echo"].AsBoolean();
    const auto flags = args["stream-flags"].AsBoolean();
    auto& input = args["input-file"].AsInputFile();
    unique_ptr<iostream> sftp_stream;

    if (args["ftp"].HasValue()) {
        sftp_stream = make_unique<CConn_FtpStream>(host, user.empty() ? "ftp"s : user, password.empty() ? "none"s : password, args["path"].AsString());
    } else {
        CSFTP_Session sftp_session(host, user, password);
        sftp_stream = make_unique<CSFTP_Stream>(sftp_session, args["path"].AsString());
    }

    cout << boolalpha;

    for (string line; s_GetCommandLine(input, line, echo); ) {
        *sftp_stream << line << endl;

        if (auto cmd = string_view(line).substr(0, line.find(' ')); (cmd == "STOR"sv) || (cmd == "APPE"sv)) {
            if (!getline(input, line)) {
                break;
            }

            if (echo) {
                cout << line << endl;
            }

            *sftp_stream << line;
        }

        if (echo) {
            cout << "--------------------------------------------------------------------------------\n";
        }

        NcbiStreamCopy(cout, *sftp_stream);
        cout << endl;
        if (flags) {
            const auto s = sftp_stream->rdstate();
            auto l = [s](auto f) { return bool(s & f); };
            cout << "bad=" << l(ios_base::badbit) << ", fail=" << l(ios_base::failbit) << ", eof=" << l(ios_base::eofbit) << endl;
        }
        sftp_stream->clear();
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CSftpSampleApp().AppMain(argc, argv);
}
