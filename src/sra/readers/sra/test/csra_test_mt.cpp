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
 *   Sample test application for cSRA reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/test_mt.hpp>
#include <util/random_gen.hpp>
#include <sra/readers/sra/csraread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <align/align-access.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>

#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CCSRATestApp::


class CCSRATestApp : public CThreadedApp
{
private:
    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
    virtual bool TestApp_Args(CArgDescriptions& args);

    bool m_Verbose;
    int m_Seed;
    int m_IterCount, m_IterSize;
    int m_ErrorCount;
    vector<string> m_Accession;
    vector<Uint8> m_MaxSpotId;
    
    CVDBMgr m_Mgr;
};


/////////////////////////////////////////////////////////////////////////////
//  Init test
bool CCSRATestApp::TestApp_Args(CArgDescriptions& args)
{
    // Specify USAGE context
    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "csra_test_mt");

    args.AddDefaultKey("accs", "Accessions",
                       "comma separated SRA accession list",
                       CArgDescriptions::eString,
                       "SRR000010,SRR389414,SRR494733,SRR505887,SRR035417");
    args.AddDefaultKey("iter_count", "IterationCount",
                       "Number of read iterations",
                       CArgDescriptions::eInteger,
                       "10");
    args.AddDefaultKey("iter_size", "IterationSize",
                       "Number of sequential sequences in one iteration",
                       CArgDescriptions::eInteger,
                       "10");
    args.AddFlag("verbose", "Print info about progress");

    return true;
}


bool CCSRATestApp::TestApp_Init(void)
{
    SetDiagPostLevel(eDiag_Info);
    const CArgs& args = GetArgs();
    m_Verbose = args["verbose"];
    m_ErrorCount = 0;
    m_Seed = args["seed"]? args["seed"].AsInteger(): int(time(0));
    if ( m_Verbose ) {
        LOG_POST(Info<<"Seed: "<<m_Seed);
    }
    NStr::Tokenize(args["accs"].AsString(), ",", m_Accession);
    if ( m_Accession.empty() ) {
        ERR_POST(Fatal<<"empty accession list");
    }
    m_IterCount = args["iter_count"].AsInteger();
    m_IterSize = args["iter_size"].AsInteger();
    m_MaxSpotId.assign(m_Accession.size(), 0);
    return true;
}


bool CCSRATestApp::TestApp_Exit(void)
{
    if ( m_ErrorCount ) {
        ERR_POST("Errors found: "<<m_ErrorCount);
    }
    else {
        LOG_POST("Done.");
    }
    return !m_ErrorCount;
}

/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////

void s_Check(const CBioseq& seq)
{
    _ASSERT(!seq.GetId().empty());
    const CSeq_inst& inst = seq.GetInst();
    const string& seqdata = inst.GetSeq_data().GetIupacna().Get();
    _ASSERT(seqdata.size() == inst.GetLength());
    ITERATE ( string, i, seqdata ) {
        _ASSERT(*i >= 'A' && *i <= 'Z');
    }
}

string s_AsFASTA(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    const string& seqdata = inst.GetSeq_data().GetIupacna().Get();
    return seq.GetId().front()->AsFastaString()+' '+seqdata;
}

bool CCSRATestApp::Thread_Run(int idx)
{
    CRandom random(m_Seed+idx);
    _ASSERT(!m_Accession.empty());
    _ASSERT(m_IterCount);
    _ASSERT(m_IterSize);
    for ( int ti = 0; ti < m_IterCount; ++ti ) {
        size_t index = random.GetRandIndex(m_Accession.size());
        const string& acc = m_Accession[index];
        if ( m_Verbose ) {
            LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc);
        }
        CCSraDb csra(m_Mgr, acc);
        if ( !m_MaxSpotId[index] ) {
            DEFINE_STATIC_FAST_MUTEX(s_mutex);
            CFastMutexGuard guard(s_mutex);
            m_MaxSpotId[index] = CCSraShortReadIterator(csra).GetMaxSpotId();
            if ( m_Verbose ) {
                LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                         <<": max id = " << m_MaxSpotId[index]);
            }
            _ASSERT(m_MaxSpotId[index] > 0);
        }
        int count = int(min(m_MaxSpotId[index], Uint8(m_IterSize)));
        Uint8 start_id = random.GetRandIndex(m_MaxSpotId[index]-count)+1;
        Uint8 stop_id = start_id+count;
        if ( m_Verbose ) {
            LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                     <<": scan " << start_id<<" - "<<(stop_id-1));
        }
        size_t seq_count = 0;
        for ( CCSraShortReadIterator i(csra, start_id);
              i && i.GetSpotId() < stop_id; ++i ) {
            CRef<CBioseq> seq = i.GetShortBioseq();
            s_Check(*seq);
            ++seq_count;
            if ( true ) {
                if ( m_Verbose ) {
                    LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                             <<": "<<s_AsFASTA(*seq));
                }
            }
            else {
                if ( m_Verbose ) {
                    LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                             <<": "<<i.GetShortSeq_id()->AsFastaString());
                }
            }
        }
        _ASSERT(seq_count);
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CCSRATestApp().AppMain(argc, argv);
}
