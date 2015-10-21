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
 *   Sample test application for cSRA data loader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/test_mt.hpp>
#include <util/random_gen.hpp>
#include <sra/data_loaders/csra/csraloader.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/scope.hpp>

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

    CSeq_id_Handle GetHandle(const string& acc,
                             Int8 spot_id,
                             int read_id) const;
    bool ExistsSpotId(CScope& scope,
                      const string& acc,
                      Int8 spot_id);
    Int8 FindMaxSpotId(const string& acc);

    bool m_Verbose;
    bool m_PreLoad;
    bool m_FullSeq;
    bool m_ResetHistory;
    int m_Seed;
    int m_IterCount, m_IterSize;
    int m_ErrorCount;
    vector<string> m_Accession;
    vector<Int8> m_MaxSpotId;
    
    CRef<CObjectManager> m_OM;
    CRef<CScope> m_SharedScope;
};


/////////////////////////////////////////////////////////////////////////////
//  Init test
bool CCSRATestApp::TestApp_Args(CArgDescriptions& args)
{
    // Specify USAGE context
    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "test_csra_loader_mt");

    args.AddDefaultKey("accs", "Accessions",
                       "comma separated SRA accession list",
                       CArgDescriptions::eString,
                       "SRR000010");
                     //"SRR000010,SRR389414,SRR494733,SRR505887,SRR035417");
    args.AddOptionalKey("accs-file", "Accessions",
                        "file with SRA accession list",
                        CArgDescriptions::eInputFile);
    args.AddDefaultKey("iter-count", "IterationCount",
                       "Number of read iterations",
                       CArgDescriptions::eInteger,
                       "10");
    args.AddDefaultKey("iter-size", "IterationSize",
                       "Number of sequential sequences in one iteration",
                       CArgDescriptions::eInteger,
                       "10");
    args.AddFlag("verbose", "Print info about progress");
    args.AddFlag("preload", "Try to preload libraries in main thread");
    args.AddFlag("full-seq", "Load full sequence");
    args.AddFlag("shared-scope", "Use shared scope in all threads");
    args.AddFlag("reset-history", "Reset scope's history after each iteration");

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
    if ( args["accs-file"] ) {
        m_Accession.clear();
        CNcbiIstream& in = args["accs-file"].AsInputFile();
        string acc;
        while ( in >> acc ) {
            m_Accession.push_back(acc);
        }
    }
    if ( m_Accession.empty() ) {
        ERR_POST(Fatal<<"empty accession list");
    }
    m_IterCount = args["iter-count"].AsInteger();
    m_IterSize = args["iter-size"].AsInteger();
    m_MaxSpotId.assign(m_Accession.size(), 0);
    m_OM = CObjectManager::GetInstance();
    CCSRADataLoader::RegisterInObjectManager(*m_OM, CObjectManager::eDefault);
    if ( args["shared-scope"] ) {
        m_SharedScope = new CScope(*m_OM);
        m_SharedScope->AddDefaults();
    }
    m_ResetHistory = args["reset-history"];
    m_FullSeq = args["full-seq"];
    if ( args["preload"] ) {
        Thread_Run(-1);
    }
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
    return seq.GetId().front()->AsFastaString()+" "+
        seq.GetInst().GetSeq_data().GetIupacna().Get();
}

CSeq_id_Handle CCSRATestApp::GetHandle(const string& acc,
                                       Int8 spot_id,
                                       int read_id) const
{
    CNcbiOstrstream str;
    str << "gnl|SRA|" << acc << '.' << spot_id << '.' << read_id;
    return CSeq_id_Handle::GetHandle(CNcbiOstrstreamToString(str));
}

bool CCSRATestApp::ExistsSpotId(CScope& scope,
                                const string& acc,
                                Int8 spot_id)
{
    for ( int read_id = 1; read_id <= 4; ++read_id ) {
        if ( !scope.GetIds(GetHandle(acc, spot_id, read_id)).empty() ) {
            return true;
        }
    }
    return false;
}

Int8 CCSRATestApp::FindMaxSpotId(const string& acc)
{
    CScope scope(*m_OM);
    scope.AddDefaults();
    Int8 a = 0;
    Int8 b = 0xfffffff;
    while ( ExistsSpotId(scope, acc, b) ) {
        a = b;
        b *= 2;
    }
    while ( b-a > 1 ) {
        Int8 m = (a+b)/2;
        if ( ExistsSpotId(scope, acc, m) ) {
            a = m;
        }
        else {
            b = m;
        }
    }
    return a;
}

bool CCSRATestApp::Thread_Run(int idx)
{
    CRandom random(m_Seed+idx);
    for ( int ti = 0; ti < m_IterCount; ++ti ) {
        size_t index = random.GetRandIndexSize_t(m_Accession.size());
        const string& acc = m_Accession[index];
        if ( m_Verbose ) {
            LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc);
        }
        
        if ( !m_MaxSpotId[index] ) {
            m_MaxSpotId[index] = FindMaxSpotId(acc);
            if ( m_Verbose ) {
                LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                         <<": max id = " << m_MaxSpotId[index]);
            }
            _ASSERT(m_MaxSpotId[index] > 0);
        }
        Int8 count = min(m_MaxSpotId[index], Int8(m_IterSize));
        Int8 start_id = random.GetRandUint8(1, m_MaxSpotId[index]-count);
        Int8 stop_id = start_id+count;
        if ( m_Verbose ) {
            LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                     <<": scan " << start_id<<" - "<<(stop_id-1));
        }
        CRef<CScope> scope_ref = m_SharedScope;
        if ( !scope_ref ) {
            scope_ref = new CScope(*m_OM);
            scope_ref->AddDefaults();
        }
        CScope& scope = *scope_ref;
        size_t seq_count = 0;
        for ( Int8 spot_id = start_id; spot_id < stop_id; ++spot_id ) {
            for ( int read_id = 1; read_id <= 4; ++read_id ) {
                CSeq_id_Handle id = GetHandle(acc, spot_id, read_id);
                _ASSERT(!scope.GetAccVer(id));
                _ASSERT(!scope.GetGi(id));
                if ( scope.GetIds(id).empty() ) {
                    _ASSERT(scope.GetSequenceLength(id) == kInvalidSeqPos);
                    _ASSERT(scope.GetSequenceType(id) == CSeq_inst::eMol_not_set);
                    _ASSERT(!scope.GetBioseqHandle(id));
                    _ASSERT(scope.GetTaxId(id) == -1);
                    continue;
                }
                ++seq_count;
                _ASSERT(scope.GetIds(id).front() == id);
                _ASSERT(scope.GetSequenceType(id) == CSeq_inst::eMol_na);
                TSeqPos len = scope.GetSequenceLength(id);
                int taxid = scope.GetTaxId(id);

                if ( !m_FullSeq ) {
                    if ( m_Verbose ) {
                        LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                                 <<": "<<len);
                    }
                    continue;
                }
                CBioseq_Handle bh = scope.GetBioseqHandle(id);
                _ASSERT(bh);
                _ASSERT(bh.GetBioseqLength() == len);
                _ASSERT(scope.GetTaxId(id) == taxid);
                CConstRef<CBioseq> seq = bh.GetCompleteObject();
                s_Check(*seq);
                if ( true ) {
                    if ( m_Verbose ) {
                        LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                                 <<": "<<s_AsFASTA(*seq));
                    }
                }
                else {
                    if ( m_Verbose ) {
                        LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                                 <<": "<<bh.GetSeqId());
                    }
                }
            }
        }
        _ASSERT(seq_count);
        if ( m_ResetHistory ) {
            scope.ResetHistory();
        }
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
