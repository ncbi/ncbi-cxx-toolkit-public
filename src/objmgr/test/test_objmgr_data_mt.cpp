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
* Authors:  Andrei Gourianov
*
* File Description:
*   Object Manager test: multiple threads working with annotations
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/04/30 19:04:05  gouriano
* multi-threaded data retrieval test
*
*
* ===========================================================================
*/

#include <corelib/ncbithr.hpp>
#include <corelib/test_mt.hpp>

#include <connect/ncbi_util.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/scope.hpp>
#include <objects/objmgr1/seq_vector.hpp>
#include <objects/objmgr1/seqdesc_ci.hpp>
#include <objects/objmgr1/feat_ci.hpp>
#include <objects/objmgr1/align_ci.hpp>
#include <objects/objmgr1/gbloader.hpp>
#include <objects/objmgr1/reader_id1.hpp>

#include <test/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;

static CFastMutex    s_GlobalLock;

// GIs to process
#if 1
    const int g_gi_from = 156000;
    const int g_gi_to   = 157000;
#else
    const int g_gi_from = 156201;
    const int g_gi_to   = 156203;
#endif

/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestOM : public CThreadedApp
{
protected:
    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Args( CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

    CRef<CScope> m_Scope;
    CRef<CObjectManager> m_ObjMgr;

    map<int, int> m_mapGiToDesc;
    map<int, int> m_mapGiToFeat0;
    map<int, int> m_mapGiToFeat1;

    int m_gi_from;
    int m_gi_to;
};


/////////////////////////////////////////////////////////////////////////////

bool CTestOM::Thread_Run(int idx)
{
// initialize scope
    CScope scope(*m_ObjMgr);
    scope.AddDefaults();

    int from, to, delta;
    // make them go in opposite directions
    if (idx % 2 == 0) {
        from = m_gi_from;
        to = m_gi_to;
    } else {
        from = m_gi_to;
        to = m_gi_from;
    }
    delta = (to > from) ? 1 : -1;

    for (int i = from;
        ((delta > 0) && (i <= to)) || ((delta < 0) && (i >= to)); i += delta) {
        try {
// load sequence
            CSeq_id sid;
            sid.SetGi(i);
            CBioseq_Handle handle = scope.GetBioseqHandle(sid);
            if (!handle) {
                LOG_POST("T" << idx << ": gi = " << i << ": INVALID HANDLE");
                continue;
            }

            int count = 0, count_prev;

// enumerate descriptions
            // Seqdesc iterator
            for (CSeqdesc_CI desc_it(handle); desc_it;  ++desc_it) {
                count++;
            }
// verify result
            {
                CFastMutexGuard guard(s_GlobalLock);
                if (m_mapGiToDesc.find(i) != m_mapGiToDesc.end()) {
                    count_prev = m_mapGiToDesc[i];
                    _ASSERT( m_mapGiToDesc[i] == count);
                } else {
                    m_mapGiToDesc[i] = count;
                }
            }

// enumerate features
            CSeq_loc loc;
            loc.SetWhole(sid);
            count = 0;
            if ( idx%2 == 0 ) {
                for (CFeat_CI feat_it(scope, loc,
                    CSeqFeatData::e_not_set, CAnnotTypes_CI::eResolve_All);
                    feat_it;  ++feat_it) {
                    count++;
                }
// verify result
                {
                    CFastMutexGuard guard(s_GlobalLock);
                    if (m_mapGiToFeat0.find(i) != m_mapGiToFeat0.end()) {
                        count_prev = m_mapGiToFeat0[i];
                        _ASSERT( m_mapGiToFeat0[i] == count);
                    } else {
                        m_mapGiToFeat0[i] = count;
                    }
                }
            }
            else {
                for (CFeat_CI feat_it(handle, 0, 0, CSeqFeatData::e_not_set);
                    feat_it;  ++feat_it) {
                    count++;
                }
// verify result
                {
                    CFastMutexGuard guard(s_GlobalLock);
                    if (m_mapGiToFeat1.find(i) != m_mapGiToFeat1.end()) {
                        count_prev = m_mapGiToFeat1[i];
                        _ASSERT( m_mapGiToFeat1[i] == count);
                    } else {
                        m_mapGiToFeat1[i] = count;
                    }
                }
            }
        } catch (exception& e) {
            LOG_POST("T" << idx << ": gi = " << i 
                << ": EXCEPTION = " << e.what());
        }
        scope.ResetHistory();
    }
    return true;
}

bool CTestOM::TestApp_Args( CArgDescriptions& args)
{
    args.AddDefaultKey
        ("fromgi", "FromGi",
         "Process sequences in the interval FROM this Gi",
         CArgDescriptions::eInteger, NStr::IntToString(g_gi_from));
    args.AddDefaultKey
        ("togi", "ToGi",
         "Process sequences in the interval TO this Gi",
         CArgDescriptions::eInteger, NStr::IntToString(g_gi_to));
    return true;
}

bool CTestOM::TestApp_Init(void)
{
    const CArgs& args = GetArgs();
    m_gi_from = args["fromgi"].AsInteger();
    m_gi_to   = args["togi"].AsInteger();

    NcbiCout << "Testing ObjectManager (" << s_NumThreads
        << " threads, gi from "
        << m_gi_from << " to " << m_gi_to << ")..." << NcbiEndl;

    m_ObjMgr = new CObjectManager;
    m_ObjMgr->RegisterDataLoader(*new CGBDataLoader("ID"),CObjectManager::eDefault);


    // Scope shared by all threads
/*
    m_Scope = new CScope(*m_ObjMgr);
    m_Scope->AddDefaults();
*/
    return true;
}

bool CTestOM::TestApp_Exit(void)
{
    NcbiCout << " Passed" << NcbiEndl << NcbiEndl;

/*
    map<int, int>::iterator it;
    for (it = m_mapGiToDesc.begin(); it != m_mapGiToDesc.end(); ++it) {
        LOG_POST(
            "gi = "         << it->first
            << ": desc = "  << it->second
            << ", feat0 = " << m_mapGiToFeat0[it->first]
            << ", feat1 = " << m_mapGiToFeat1[it->first]
            );
    }
*/
    return true;
}
END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestOM().AppMain(argc, argv, 0, eDS_Default, 0);
}
