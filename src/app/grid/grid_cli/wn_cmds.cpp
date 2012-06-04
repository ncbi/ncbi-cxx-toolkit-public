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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Worker node-specific commands of grid_cli.
 *
 */

#include <ncbi_pch.hpp>

#include "grid_cli.hpp"

#include <connect/services/grid_rw_impl.hpp>

#include <cgi/ncbicgi.hpp>

USING_NCBI_SCOPE;

int CGridCommandLineInterfaceApp::Cmd_Replay()
{
    SetUp_GridClient();

    if (IsOptionSet(eDumpCGIEnv)) {
        CNetScheduleJob job;
        job.job_id = m_Opts.id;

        if (m_NetScheduleAPI.GetJobDetails(job) ==
                CNetScheduleAPI::eJobNotFound) {
            fprintf(stderr, GRID_APP_NAME ": job %s has expired.\n",
                    job.job_id.c_str());
            return 3;
        }

        auto_ptr<CNcbiIstream> input_stream(new CRStream(
                new CStringOrBlobStorageReader(job.input, m_NetCacheAPI), 0, 0,
                    CRWStreambuf::fOwnReader | CRWStreambuf::fLeakExceptions));

        input_stream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);

        auto_ptr<CCgiRequest> request;

        try {
            request.reset(new CCgiRequest(*input_stream,
                CCgiRequest::fIgnoreQueryString |
                CCgiRequest::fDoNotParseContent));
        }
        catch (CException& e) {
            fprintf(stderr, GRID_APP_NAME
                    ": Error while parsing CGI request stream: %s.\n",
                    e.what());
            return 3;
        }

        const CNcbiEnvironment& env(request->GetEnvironment());

        list<string> var_names;

        env.Enumerate(var_names);

        ITERATE(list<string>, var, var_names) {
            printf("%s=%s\n", var->c_str(), env.Get(*var).c_str());
        }
    }

    return 0;
}
