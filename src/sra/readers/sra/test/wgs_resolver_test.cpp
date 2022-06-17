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
 *   Test for long term running of WGS resolver
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/sra/vdbread.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <util/thread_nonstop.hpp>
#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CWGSResolverTestApp::


class CWGSResolverTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

protected:
    class CIndexUpdateThread : public CThreadNonStop
    {
    public:
        CIndexUpdateThread(unsigned update_delay,
                           bool verbose,
                           CRef<CWGSResolver> resolver)
            : CThreadNonStop(update_delay),
              m_UpdateCount(0),
              m_ReopenCount(0),
              m_ReopenTime(0),
              m_ReuseCount(0),
              m_ReuseTime(0),
              m_Verbose(verbose),
              m_FirstRun(true),
              m_Resolver(resolver)
            {
            }

        unsigned m_UpdateCount;
        unsigned m_ReopenCount;
        double m_ReopenTime;
        unsigned m_ReuseCount;
        double m_ReuseTime;
        
    protected:
        virtual void DoJob(void) {
            if ( m_FirstRun ) {
                // CThreadNonStop runs first iteration immediately, ignore it
                m_FirstRun = false;
                return;
            }
            try {
                ++m_UpdateCount;
                CStopWatch sw(CStopWatch::eStart);
                if ( m_Resolver->Update() ) {
                    ++m_ReopenCount;
                    m_ReopenTime += sw.Elapsed();
                    if ( m_Verbose || 1 ) {
                        LOG_POST("Updated WGS index");
                    }
                }
                else {
                    ++m_ReuseCount;
                    m_ReuseTime += sw.Elapsed();
                    if ( m_Verbose ) {
                        LOG_POST("Same WGS index");
                    }
                }
            }
            catch ( CException& exc ) {
                ERR_POST("Exception while updating WGS index: "<<exc);
            }
            catch ( exception& exc ) {
                ERR_POST("Exception while updating WGS index: "<<exc.what());
            }
        }

    private:
        bool m_Verbose;
        bool m_FirstRun;
        CRef<CWGSResolver> m_Resolver;
    };

    typedef vector<TGi> TGis;
    typedef vector<string> TAccs;
    
    class CResolveThread : public CThreadNonStop
    {
    public:
        CResolveThread(unsigned resolve_delay,
                       unsigned run_count,
                       unsigned id_index,
                       const TGis& gis,
                       const TAccs& accs,
                       bool verbose,
                       CRef<CWGSResolver> resolver)
            : CThreadNonStop(resolve_delay),
              m_RunCount(run_count),
              m_IdIndex(id_index),
              m_GiResolveCount(0),
              m_GiResolveTime(0),
              m_AccResolveCount(0),
              m_AccResolveTime(0),
              m_Gis(gis),
              m_Accs(accs),
              m_Verbose(verbose),
              m_Resolver(resolver)
            {
            }

        int m_RunCount;
        unsigned m_IdIndex;
        unsigned m_GiResolveCount;
        double m_GiResolveTime;
        unsigned m_AccResolveCount;
        double m_AccResolveTime;
        const TGis& m_Gis;
        const TAccs& m_Accs;
        bool m_Verbose;
        
    protected:
        virtual void DoJob(void) {
            try {
                {{
                    TGi gi = m_Gis[m_IdIndex%size(m_Gis)];
                    CStopWatch sw(CStopWatch::eStart);
                    auto prefixes = m_Resolver->GetPrefixes(gi);
                    ++m_GiResolveCount;
                    m_GiResolveTime += sw.Elapsed();
                    if ( m_Verbose ) {
                        LOG_POST("Resolved gi "<<gi<<" to "<<prefixes.size()<<" projects");
                    }
                    _ASSERT(prefixes.size() > 0);
                }}
                {{
                    string acc = m_Accs[m_IdIndex%size(m_Accs)];
                    CStopWatch sw(CStopWatch::eStart);
                    auto prefixes = m_Resolver->GetPrefixes(acc);
                    ++m_AccResolveCount;
                    m_AccResolveTime += sw.Elapsed();
                    if ( m_Verbose ) {
                        LOG_POST("Resolved acc "<<acc<<" to "<<prefixes.size()<<" projects");
                    }
                    _ASSERT(prefixes.size() > 0);
                }}
                ++m_IdIndex;
                if ( --m_RunCount <= 0 ) {
                    RequestStop();
                }
            }
            catch ( CException& exc ) {
                ERR_POST("Exception while resolving: "<<exc);
            }
            catch ( exception& exc ) {
                ERR_POST("Exception while resolving: "<<exc.what());
            }
        }

    private:
        CRef<CWGSResolver> m_Resolver;
    };
    
    void InitRequests();

    void InitResolver();
    void CloseResolver();

    void StartWorkers();
    void InterruptWorkers();
    void StopWorkers();
    
    void ResolveGi(TGi gi);
    void ResolveAcc(const string& acc);
    
private:
    bool m_Verbose;
    CVDBMgr m_Mgr;
    CRef<CWGSResolver> m_Resolver;
    CRef<CIndexUpdateThread> m_UpdateThread;
    TGis m_Gis;
    TAccs m_Accs;
    vector<CRef<CResolveThread>> m_ResolveThreads;
    unsigned m_RunTime;
};


/////////////////////////////////////////////////////////////////////////////
//  Init test

void CWGSResolverTestApp::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("update-interval", "UpdateInterval",
                            "Time in seconds between WGS index updates",
                            CArgDescriptions::eInteger,
                            "100");
    arg_desc->AddFlag("verbose",
                      "Print progress logs");
    arg_desc->AddDefaultKey("threads", "Threads",
                            "Number of resolve threads",
                            CArgDescriptions::eInteger,
                            "20");
    arg_desc->AddDefaultKey("resolve-interval", "ResolveInterval",
                            "Time in seconds between WGS resolve requests",
                            CArgDescriptions::eInteger,
                            "30");
    arg_desc->AddDefaultKey("run-time", "RunTime",
                            "Total time in seconds to run the test",
                            CArgDescriptions::eInteger,
                            "500");

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "wgs_resolver_test");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void CWGSResolverTestApp::InitResolver()
{
    unsigned update_interval = GetArgs()["update-interval"].AsInteger();
    m_Resolver = CWGSResolver::CreateResolver(m_Mgr);
    _ASSERT(m_Resolver);
    m_UpdateThread = new CIndexUpdateThread(update_interval, m_Verbose, m_Resolver);
    m_UpdateThread->Run();
}


void CWGSResolverTestApp::CloseResolver()
{
    m_UpdateThread->RequestStop();
    m_UpdateThread->Join();
    LOG_POST("WGS index updated "<<
             m_UpdateThread->m_ReopenCount<<"/"<<
             m_UpdateThread->m_UpdateCount<<" times");
    if ( m_UpdateThread->m_ReopenCount ) {
        LOG_POST("Average reopen time: "<<
                 m_UpdateThread->m_ReopenTime/m_UpdateThread->m_ReopenCount*1e3<<" ms");
    }
    if ( m_UpdateThread->m_ReuseCount ) {
        LOG_POST("Average reuse time: "<<
                 m_UpdateThread->m_ReuseTime/m_UpdateThread->m_ReuseCount*1e3<<" ms");
    }
    m_UpdateThread = null;
    m_Resolver = null;
}


/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////


void CWGSResolverTestApp::InitRequests()
{
    static const Int8 gis[] = {
        19589319,
        322102187,
        922102180,
        922433854,
        1322102180,
        1322114654,
        1922102180
    };
    static const char* const accs[] = {
        "CAA0012091",
        "ACE87879",
        "ACE87881",
        "ACE87886",
        "CAA0013033",
        "CAA0013034",
        "CPW11821",
        "EIX0116187",
        "EIX0117187",
        "EIX0119999",
        "CXV59962",
        "VZO40000",
        "VZO40385",
        "VZO40685",
        "VZV95036",
        "VZV95056",
        "VZV95081"
    };

    for ( auto gi : gis ) {
        m_Gis.push_back(GI_FROM(Int8, gi));
    }
    for ( auto acc : accs ) {
        m_Accs.push_back(acc);
    }
}


void CWGSResolverTestApp::StartWorkers()
{
    unsigned resolve_interval = GetArgs()["resolve-interval"].AsInteger();
    unsigned run_time = GetArgs()["run-time"].AsInteger();
    if ( 1 ) {
        int thread_count = GetArgs()["threads"].AsInteger();
        LOG_POST("Running "<<thread_count<<" resolve threads for "<<run_time<<" seconds");
        for ( int t = 0; t < thread_count; ++t ) {
            m_ResolveThreads.push_back(Ref(new CResolveThread(resolve_interval,
                                                              run_time/resolve_interval, t,
                                                              m_Gis, m_Accs,
                                                              m_Verbose, m_Resolver)));
        }
        for ( auto& t : m_ResolveThreads ) {
            t->Run();
        }
    }
    else {
        for ( unsigned t = 0; t*resolve_interval < run_time; ++t ) {
            SleepSec(resolve_interval);
            ResolveGi(m_Gis[t%size(m_Gis)]);
            ResolveAcc(m_Accs[t%size(m_Accs)]);
        }
    }
}


void CWGSResolverTestApp::InterruptWorkers()
{
    for ( auto& t : m_ResolveThreads ) {
        t->RequestStop();
    }
}


void CWGSResolverTestApp::StopWorkers()
{
    unsigned gi_resolve_count = 0;
    double gi_resolve_time = 0;
    unsigned acc_resolve_count = 0;
    double acc_resolve_time = 0;
    for ( auto& t : m_ResolveThreads ) {
        t->Join();
        gi_resolve_count += t->m_GiResolveCount;
        gi_resolve_time += t->m_GiResolveTime;
        acc_resolve_count += t->m_AccResolveCount;
        acc_resolve_time += t->m_AccResolveTime;
    }
    LOG_POST("Resolved "<<gi_resolve_count<<" gis "
             "average time: "<<gi_resolve_time/gi_resolve_count*1e3<<" ms");
    LOG_POST("Resolved "<<acc_resolve_count<<" accs "
             "average time: "<<acc_resolve_time/acc_resolve_count*1e3<<" ms");
}


void CWGSResolverTestApp::ResolveGi(TGi gi)
{
    auto prefixes = m_Resolver->GetPrefixes(gi);
    LOG_POST("Resolved gi "<<gi<<" to "<<prefixes.size()<<" projects");
    _ASSERT(prefixes.size() > 0);
}


void CWGSResolverTestApp::ResolveAcc(const string& acc)
{
    auto prefixes = m_Resolver->GetPrefixes(acc);
    LOG_POST("Resolved acc "<<acc<<" to "<<prefixes.size()<<" projects");
    _ASSERT(prefixes.size() > 0);
}


int CWGSResolverTestApp::Run(void)
{
    m_Verbose = GetArgs()["verbose"];
    
    InitRequests();
    
    InitResolver();
    StartWorkers();

    // start controlling thread

    StopWorkers();
    CloseResolver();
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CWGSResolverTestApp::Exit(void)
{
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CWGSResolverTestApp().AppMain(argc, argv);
}
