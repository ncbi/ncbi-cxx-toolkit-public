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
* Author:  Aaron Ucko
*
* File Description:
*   testing multipart CGI responses
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiargs.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

class CTestCgiEntryReaderApplication : public CCgiApplication
{
public:
    void Init(void);
    int  ProcessRequest(CCgiContext& ctx);
};

void CTestCgiEntryReaderApplication::Init()
{
    SetRequestFlags(CCgiRequest::fParseInputOnDemand);
}

static string s_GetReaderContents(IReader* reader)
{
    string result;
    char   buf[32];
    size_t n;

    while (reader->Read(buf, sizeof(buf), &n) == eRW_Success) {
        result.append(buf, n);
        LOG_POST("Reader got " << n << " bytes: "
                 << NStr::PrintableString(string(buf, n)));
    }
    LOG_POST("Reader got " << result.size() << " bytes total.");

    delete reader;
    return result;
}

static string s_GetStreamContents(CNcbiIstream* is)
{
    string     result;
    char       buf[32];
    streamsize n;

    while (*is) {
        is->read(buf, sizeof(buf));
        n = is->gcount();
        result.append(buf, n);
        LOG_POST("Stream got " << n << " bytes: "
                 << NStr::PrintableString(string(buf, n)));
    }
    LOG_POST("Stream got " << result.size() << " bytes total.");

    delete is;
    return result;
}

static void s_TestEntry(const string& name, CCgiEntry& e)
{
    string preread_value, streamed_value;

    LOG_POST("Position: " << e.GetPosition());
    LOG_POST("Name: " << name);

    if (e.GetFilename().empty()) {
        preread_value = e.GetValue();
    } else {
        LOG_POST("Filename: " << e.GetFilename());
    }

    if (e.GetPosition() % 2) {
        streamed_value = s_GetReaderContents(e.GetValueReader());
    } else {
        streamed_value = s_GetStreamContents(e.GetValueStream());
    }

    _ASSERT(e.GetValue() == preread_value);
    if (e.GetFilename().empty()) {
        _ASSERT(streamed_value == preread_value);
    }
    _ASSERT(s_GetReaderContents(e.GetValueReader()) == preread_value);
    _ASSERT(s_GetStreamContents(e.GetValueStream()) == preread_value);
}

int CTestCgiEntryReaderApplication::ProcessRequest(CCgiContext& ctx)
{
    CCgiRequest& req = ctx.GetRequest();

    for (;;) {
        TCgiEntriesI it = req.GetNextEntry();
        if (it == req.GetEntries().end()) {
            break;
        }

        s_TestEntry(it->first, it->second);
        if (NStr::StartsWith(it->first, "see_")) {
            string name(it->first, 4);
            CCgiEntry* e = req.GetPossiblyUnparsedEntry(name);
            if (e) {
                s_TestEntry(name, *e);
            } else {
                LOG_POST("That's all, folks!");
            }
        }
    }

    return 0;
}

int main(int argc, char** argv)
{
    return CTestCgiEntryReaderApplication().AppMain(argc, argv);
}
