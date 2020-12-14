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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Sample test application for SNP primary track resolver
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/random_gen.hpp>
#include <objects/dbsnp/primary_track/snpptis.hpp>
#include <thread>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CSNPTestApp::


class CSnpPtisTestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test

void CSnpPtisTestApp::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "snp_test");

    arg_desc->AddOptionalKey("id", "Id",
                             "Seq-id to resolve - gi or accession.version",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("id_file", "IdFile",
                             "File with Seq-id list to resolve",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("threads", "Threads",
                            "number of threads for PTIS requests",
                            CArgDescriptions::eInteger, "1");
    
    arg_desc->AddFlag("print_results", "Print resolution results");
    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////


int CSnpPtisTestApp::Run(void)
{
    if ( !CSnpPtisClient::IsEnabled() ) {
        ERR_POST("Test is disabled due to lack of GRPC support");
        return 0;
    }
    const CArgs& args = GetArgs();

    vector<string> ids;
    if ( args["id"] ) {
        ids.push_back(args["id"].AsString());
    }
    else if ( args["id_file"] ) {
        auto& in = args["id_file"].AsInputFile();
        string id;
        while ( in >> id ) {
            ids.push_back(id);
        }
    }
    else {
        ids.push_back("NC_000001.11");
    }

    CStopWatch sw(CStopWatch::eStart);
    
    sw.Restart();
    auto client = CSnpPtisClient::CreateClient();
    LOG_POST("Opened connection in "<<sw.Elapsed()<<" sec");

    if ( 1 && !ids.empty() ) {
        sw.Restart();
        try {
            client->GetPrimarySnpTrackForId(ids.front());
            LOG_POST("Resolved first SNP track in "<<sw.Elapsed()<<" sec");
        }
        catch (exception& exc) {
            ERR_POST("Exception while resolving SNP track for "<<ids.front()<<": "<<exc);
            throw;
        }
    }

    vector<string> tracks;
    
    sw.Restart();
    size_t NQ = args["threads"].AsInteger();
    vector<thread> tt(NQ);
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i] =
            thread([&]
                   (size_t thread_id, vector<string> ids, vector<string>* out_tracks)
                   {
                       if ( thread_id % 2 ) {
                           CRandom random((int)thread_id);
                           for ( size_t i = 0; i < ids.size(); ++i ) {
                               swap(ids[i], ids[random.GetRandIndexSize_t(i+1)]);
                           }
                       }
                       vector<string> tracks;
                       tracks.reserve(ids.size());
                       for ( const auto& id : ids ) {
                           string track;
                           try {
                               if ( isdigit(id[0]) ) {
                                   track = client->GetPrimarySnpTrackForGi(NStr::StringToNumeric<TGi>(id));
                               }
                               else {
                                   track = client->GetPrimarySnpTrackForAccVer(id);
                               }
                           }
                           catch (exception& exc) {
                               ERR_POST("Exception while resolving SNP track for "<<id<<": "<<exc);
                               throw;
                           }
                           tracks.push_back(track);
                       }
                       if ( out_tracks ) {
                           *out_tracks = tracks;
                       }
                   }, i, ids, i==0? &tracks: 0);
    }
    for ( size_t i = 0; i < NQ; ++i ) {
        tt[i].join();
    }
    double t = sw.Elapsed();

    if ( args["print_results"] ) {
        for ( size_t i = 0; i < ids.size(); ++i ) {
            LOG_POST(ids[i]<<": "<<tracks[i]);
        }
    }
    
    LOG_POST("Resolved SNP tracks in "<<t<<" sec");
    if ( ids.size() > 1 ) {
        LOG_POST("Time per one of "<<ids.size()<<" Seq-ids: "<<t/ids.size()<<" sec");
    }
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CSnpPtisTestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSnpPtisTestApp().AppMain(argc, argv);
}
