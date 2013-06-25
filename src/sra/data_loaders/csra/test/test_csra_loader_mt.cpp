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
                             Uint8 spot_id,
                             int read_id) const;
    bool ExistsSpotId(CScope& scope,
                      const string& acc,
                      Uint8 spot_id);
    Uint8 FindMaxSpotId(const string& acc);

    bool m_Verbose;
    int m_Seed;
    int m_IterCount, m_IterSize;
    int m_ErrorCount;
    vector<string> m_Accession;
    vector<Uint8> m_MaxSpotId;
    
    CRef<CObjectManager> m_OM;
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
    args.AddOptionalKey("accs_file", "Accessions",
                        "file with SRA accession list",
                        CArgDescriptions::eInputFile);
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
    if ( args["accs_file"] ) {
        m_Accession.clear();
        CNcbiIstream& in = args["accs_file"].AsInputFile();
        string acc;
        while ( in >> acc ) {
            m_Accession.push_back(acc);
        }
    }
    if ( m_Accession.empty() ) {
        ERR_POST(Fatal<<"empty accession list");
    }
    m_IterCount = args["iter_count"].AsInteger();
    m_IterSize = args["iter_size"].AsInteger();
    m_MaxSpotId.assign(m_Accession.size(), 0);
    m_OM = CObjectManager::GetInstance();
    CCSRADataLoader::RegisterInObjectManager(*m_OM, CObjectManager::eDefault);
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

string s_AsFASTA(const CBioseq& seq)
{
    return seq.GetId().front()->AsFastaString()+" "+
        seq.GetInst().GetSeq_data().GetIupacna().Get();
}

CSeq_id_Handle CCSRATestApp::GetHandle(const string& acc,
                                       Uint8 spot_id,
                                       int read_id) const
{
    CNcbiOstrstream str;
    str << "gnl|SRA|" << acc << '.' << spot_id << '.' << read_id;
    return CSeq_id_Handle::GetHandle(CNcbiOstrstreamToString(str));
}

bool CCSRATestApp::ExistsSpotId(CScope& scope,
                                const string& acc,
                                Uint8 spot_id)
{
    for ( int read_id = 1; read_id <= 4; ++read_id ) {
        if ( !scope.GetIds(GetHandle(acc, spot_id, read_id)).empty() ) {
            return true;
        }
    }
    return false;
}

Uint8 CCSRATestApp::FindMaxSpotId(const string& acc)
{
    CScope scope(*m_OM);
    scope.AddDefaults();
    Uint8 a = 0;
    Uint8 b = 0xffff;
    while ( ExistsSpotId(scope, acc, b) ) {
        a = b;
        b *= 2;
    }
    while ( b-a > 1 ) {
        Uint8 m = (a+b)/2;
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
        size_t index = random.GetRand(0u, m_Accession.size()-1);
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
        }
        int count = int(min(m_MaxSpotId[index], Uint8(m_IterSize)));
        Uint8 start_id = random.GetRand(1u, m_MaxSpotId[index]-count+1);
        Uint8 end_id = start_id+count-1;
        if ( m_Verbose ) {
            LOG_POST(Info<<"T"<<idx<<"."<<ti<<": acc["<<index<<"] "<<acc
                     <<": scan " << start_id<<" - "<<end_id);
        }
        CScope scope(*m_OM);
        for ( Uint8 spot_id = start_id; spot_id <= end_id; ++spot_id ) {
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
                _ASSERT(scope.GetIds(id).front() == id);
                _ASSERT(scope.GetSequenceType(id) == CSeq_inst::eMol_na);
                TSeqPos len = scope.GetSequenceLength(id);
                int taxid = scope.GetTaxId(id);
                //_ASSERT(!scope.GetBioseqHandle(id, CScope::eGetBioseq_Resolved));
                if ( true ) continue;
                CBioseq_Handle bh = scope.GetBioseqHandle(id);
                _ASSERT(bh);
                _ASSERT(bh.GetBioseqLength() == len);
                _ASSERT(scope.GetTaxId(id) == taxid);
                if ( true ) {
                    CConstRef<CBioseq> seq = bh.GetCompleteObject();
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
