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
* Author:
*           Andrei Gourianov, Michael Kimelman
*
* File Description:
*           Basic test of GenBank data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;


const int g_gi_from = 156000;
const int g_gi_to   = 157000;
const int g_acc_from = 1;


/////////////////////////////////////////////////////////////////////////////
//
//  CTestApplication::
//


class CTestApplication : public CNcbiApplication
{
public:
    void TestApp_Args(CArgDescriptions& args);
    bool TestApp_Init(const CArgs& args);

    virtual void Init(void);
    virtual int Run(void);

    typedef vector<CSeq_id_Handle> TIds;

    vector<CSeq_id_Handle> m_Ids;
    bool m_Verbose;
};


void CTestApplication::Init(void)
{
    //CONNECT_Init(&GetConfig());
    //CORE_SetLOCK(MT_LOCK_cxx2c());
    //CORE_SetLOG(LOG_cxx2c());

    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Let test application add its own arguments
    TestApp_Args(*arg_desc);
    SetupArgDescriptions(arg_desc.release());

    if ( !TestApp_Init(GetArgs()) )
        THROW1_TRACE(runtime_error, "Cannot init test application");
}


void CTestApplication::TestApp_Args(CArgDescriptions& args)
{
    args.AddOptionalKey
        ("fromgi", "FromGi",
         "Process sequences in the interval FROM this Gi",
         CArgDescriptions::eInteger);
    args.AddOptionalKey
        ("togi", "ToGi",
         "Process sequences in the interval TO this Gi",
         CArgDescriptions::eInteger);
    args.AddOptionalKey
        ("idlist", "IdList",
         "File with list of Seq-ids to test",
         CArgDescriptions::eInputFile);
    args.AddFlag("verbose", "Verbose results");

    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "test_bulkinfo", false);
}


bool CTestApplication::TestApp_Init(const CArgs& args)
{
    if ( args["idlist"] ) {
        CNcbiIstream& file = args["idlist"].AsInputFile();
        string line;
        while ( getline(file, line) ) {
            size_t comment = line.find('#');
            if ( comment != NPOS ) {
                line.erase(comment);
            }
            line = NStr::TruncateSpaces(line);
            if ( line.empty() ) {
                continue;
            }
            
            CSeq_id sid(line);
            CSeq_id_Handle sih = CSeq_id_Handle::GetHandle(sid);
            if ( !sih ) {
                continue;
            }
            m_Ids.push_back(sih);
        }
        
        NcbiCout << "Testing bulk info load (" <<
            m_Ids.size() << " Seq-ids from file)..." << NcbiEndl;
    }
    if ( m_Ids.empty() && (args["fromgi"] || args["togi"]) ) {
        int gi_from  = args["fromgi"]? args["fromgi"].AsInteger(): g_gi_from;
        int gi_to    = args["togi"]? args["togi"].AsInteger(): g_gi_to;
        int delta = gi_to > gi_from? 1: -1;
        for ( int gi = gi_from; gi != gi_to+delta; gi += delta ) {
            m_Ids.push_back(CSeq_id_Handle::GetGiHandle(gi));
        }
        NcbiCout << "Testing bulk info load ("
            "gi from " << gi_from << " to " << gi_to << ")..." << NcbiEndl;
    }
    if ( m_Ids.empty() ) {
        int count = g_gi_to-g_gi_from+1;
        for ( int i = 0; i < count; ++i ) {
            if ( i % 3 != 0 ) {
                m_Ids.push_back(CSeq_id_Handle::GetGiHandle(i+g_gi_from));
            }
            else {
                CNcbiOstrstream str;
                if ( i & 1 )
                    str << "A";
                else
                    str << "a";
                if ( i & 2 )
                    str << "A";
                else
                    str << "a";
                str << setfill('0') << setw(6) << (i/3+g_acc_from);
                string acc = CNcbiOstrstreamToString(str);
                CSeq_id seq_id(acc);
                m_Ids.push_back(CSeq_id_Handle::GetHandle(seq_id));
            }
        }
        NcbiCout << "Testing bulk info load ("
            "accessions and gi from " <<
            g_gi_from << " to " << g_gi_to << ")..." << NcbiEndl;
    }
    m_Verbose = args["verbose"];
    return true;
}


int CTestApplication::Run(void)
{
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*pOm);

    CScope scope(*pOm);
    scope.AddDefaults();
    size_t count = m_Ids.size();
    TIds ids = scope.GetAccVers(m_Ids);
    if ( m_Verbose ) {
        for ( size_t i = 0; i < count; ++i ) {
            if ( ids[i] ) {
                LOG_POST(m_Ids[i]<<" -> "<<ids[i]);
            }
            else {
                LOG_POST(m_Ids[i]<<" -> null");
            }
        }
    }
    for ( size_t i = 0; i < count; ++i ) {
        _ASSERT(!scope.GetBioseqHandle(m_Ids[i], CScope::eGetBioseq_Loaded));
    }
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//
//  MAIN
//


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv, 0, eDS_Default, 
                                      "test_objmgr_gbloader.ini");
}
