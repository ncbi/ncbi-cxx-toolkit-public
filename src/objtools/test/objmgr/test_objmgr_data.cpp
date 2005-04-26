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
* Revision 1.16  2005/04/26 15:44:31  vasilche
* Added option -no_external. More detailed error report.
*
* Revision 1.15  2005/02/28 17:11:54  vasilche
* Use correct AppMain() arguments.
*
* Revision 1.14  2004/12/22 15:56:43  vasilche
* Add options -pass_count and -no_reset.
*
* Revision 1.13  2004/12/14 17:41:03  grichenk
* Reduced number of CSeqMap::FindResolved() methods, simplified
* BeginResolved and EndResolved. Marked old methods as deprecated.
*
* Revision 1.12  2004/11/01 19:33:09  grichenk
* Removed deprecated methods
*
* Revision 1.11  2004/09/13 18:33:34  vasilche
* Removed obsolete code.
*
* Revision 1.10  2004/09/08 16:29:13  vasilche
* Added time in verbose output.
*
* Revision 1.9  2004/08/31 14:43:21  vasilche
* Added options -no_seq_map, -no_named, -verbose -adaptive.
*
* Revision 1.8  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.7  2004/07/13 14:03:08  vasilche
* Added option "idlist" to test_objmgr_data & test_objmgr_data_mt.
*
* Revision 1.6  2004/06/30 20:58:07  vasilche
* Added option to test list of Seq-ids from file.
*
* Revision 1.5  2004/05/21 21:42:55  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/04/19 14:53:31  grichenk
* Added prefetching test (-prefetch argument)
*
* Revision 1.3  2004/04/16 13:31:47  grichenk
* Added data pre-fetching functions.
*
* Revision 1.2  2004/04/05 17:00:55  grichenk
* Fixed iterator flags
*
* Revision 1.1  2003/12/16 17:51:17  kuznets
* Code reorganization
*
* Revision 1.12  2003/11/26 17:56:03  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.11  2003/11/03 21:21:20  vasilche
* Limit amount of exceptions to catch while testing.
*
* Revision 1.10  2003/10/22 17:58:10  vasilche
* Do not catch CLoaderException::eNoConnection.
*
* Revision 1.9  2003/09/30 16:22:05  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.8  2003/06/02 16:06:39  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.7  2003/05/20 15:44:39  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.6  2003/05/06 16:52:55  vasilche
* Added 'pause' argument.
*
* Revision 1.5  2003/04/24 16:12:39  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.4  2003/04/15 14:23:11  vasilche
* Added missing includes.
*
* Revision 1.3  2003/03/18 21:48:33  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.2  2003/01/29 17:34:18  vasilche
* Added test code for negative strand CSeqVector.
*
* Revision 1.1  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.4  2002/12/06 15:36:03  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.3  2002/07/22 22:49:05  kimelman
* test fixes for confidential data retrieval
*
* Revision 1.2  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.1  2002/04/30 19:04:05  gouriano
* multi-threaded data retrieval test
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_util.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/prefetch.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <test/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;

static CFastMutex    s_GlobalLock;

// GIs to process
#if 0
    const int g_gi_from = 156894;
    const int g_gi_to   = 156896;
#elif 0
    const int g_gi_from = 156201;
    const int g_gi_to   = 156203;
#else
    const int g_gi_from = 156000;
    const int g_gi_to   = 157000;
#endif

/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestOM : public CNcbiApplication
{
protected:
    virtual int Run(void);
    virtual void Init(void);
    virtual void Exit(void);

    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Args( CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

    typedef vector<CConstRef<CSeq_feat> >   TFeats;
    typedef CSeq_id_Handle                  TMapKey;
    typedef map<TMapKey, TFeats>            TFeatMap;
    typedef map<CSeq_id_Handle, int>        TIntMap;
    typedef vector<CSeq_id_Handle> TIds;

    CRef<CObjectManager> m_ObjMgr;

    TIntMap     m_DescMap;
    TFeatMap    m_Feat0Map;
    TFeatMap    m_Feat1Map;

    void SetValue(TIntMap& vm, const CSeq_id_Handle& id, int value);
    void SetValue(TFeatMap& vm, const TMapKey& key, const TFeats& value);

    TIds m_Ids;

    bool m_load_only;
    bool m_no_seq_map;
    bool m_no_snp;
    bool m_no_named;
    bool m_no_external;
    bool m_adaptive;
    int  m_pause;
    bool m_prefetch;
    bool m_verbose;
    int  m_pass_count;
    bool m_no_reset;
    bool m_keep_handles;
};


/////////////////////////////////////////////////////////////////////////////

void CTestOM::SetValue(TIntMap& vm, const CSeq_id_Handle& id, int value)
{
    int old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        old_value = vm.insert(TIntMap::value_type(id, value)).first->second;
    }}
    if ( old_value != value ) {
        string name;
        if ( &vm == &m_DescMap ) name = "desc";
        ERR_POST("Inconsistent "<<name<<" on "<<id.AsString()<<
                 " was "<<old_value<<" now "<<value);
    }
    _ASSERT(old_value == value);
}


void CTestOM::SetValue(TFeatMap& vm, const TMapKey& key, const TFeats& value)
{
    const TFeats* old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        TFeatMap::iterator it = vm.lower_bound(key);
        if ( it == vm.end() || it->first != key ) {
            it = vm.insert(it, TFeatMap::value_type(key, value));
        }
        old_value = &it->second;
    }}
    if ( old_value->size() != value.size() ) {
        CNcbiOstrstream s;
        string name;
        if ( &vm == &m_Feat0Map ) name = "feat0";
        if ( &vm == &m_Feat1Map ) name = "feat1";
        s << "Inconsistent "<<name<<" on "<<key.AsString()<<
            " was "<<old_value->size()<<" now "<<value.size() << NcbiEndl;
        ITERATE ( TFeats, it, *old_value ) {
            s << " old: " << MSerial_AsnText << **it;
        }
        ITERATE ( TFeats, it, value ) {
            s << " new: " << MSerial_AsnText << **it;
        }
        ERR_POST(string(CNcbiOstrstreamToString(s)));
    }
    _ASSERT(old_value->size() == value.size());
}


int CTestOM::Run(void)
{
    if ( !Thread_Run(0) )
        return 1;
    else
        return 0;
}

void CTestOM::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Let test application add its own arguments
    TestApp_Args(*arg_desc);

    string prog_description =
        "test";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());

    if ( !TestApp_Init() )
        THROW1_TRACE(runtime_error, "Cannot init test application");
}

void CTestOM::Exit(void)
{
    if ( !TestApp_Exit() )
        THROW1_TRACE(runtime_error, "Cannot exit test application");
}

bool CTestOM::Thread_Run(int idx)
{
    // initialize scope
    CScope scope(*m_ObjMgr);
    scope.AddDefaults();

    int from, to;
    // make them go in opposite directions
    if (idx % 2 == 0) {
        from = 0;
        to = m_Ids.size()-1;
    } else {
        from = m_Ids.size()-1;
        to = 0;
    }
    int delta = (to > from) ? 1 : -1;
    int pause = m_pause;

    set<CBioseq_Handle> handles;

    bool ok = true;
    for ( int pass = 0; pass < m_pass_count; ++pass ) {
        CPrefetchToken token;
        if (m_prefetch) {
            LOG_POST("Using prefetch");
            // Initialize prefetch token;
            token = CPrefetchToken(scope, m_Ids);
        }

        const int kMaxErrorCount = 30;
        int error_count = 0;
        TFeats feats;
        for ( int i = from, end = to+delta; i != end; i += delta ) {
            CSeq_id_Handle sih = m_Ids[i];
            if ( i != from && pause ) {
                SleepSec(pause);
            }
            if ( m_verbose ) {
                NcbiCout << CTime(CTime::eCurrent).AsString() << " " <<
                    abs(i-from) << ": " << sih.AsString() << NcbiFlush;
            }
            try {
                // load sequence
                CBioseq_Handle handle;
                if (m_prefetch) {
                    if (!token) {
                        LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                                 ": INVALID PREFETCH TOKEN");
                        continue;
                    }
                    handle = token.NextBioseqHandle(scope);
                }
                else {
                    handle = scope.GetBioseqHandle(sih);
                }
                if (!handle) {
                    LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                             ": INVALID HANDLE");
                    continue;
                }

                if ( !m_load_only ) {
                    int count = 0;

                    // check CSeqMap_CI
                    if ( !m_no_seq_map ) {
                        SSeqMapSelector sel(CSeqMap::fFindRef, kMax_UInt);
                        for ( CSeqMap_CI it(handle, sel); it; ++it ) {
                            _ASSERT(it.GetType() == CSeqMap::eSeqRef);
                        }
                    }

                    // check seqvector
                    if ( 0 ) {{
                        string buff;
                        CSeqVector sv =
                            handle.GetSeqVector(handle.eCoding_Iupac, 
                                                handle.eStrand_Plus);

                        int start = max(0, int(sv.size()-600000));
                        int stop  = sv.size();

                        sv.GetSeqData(start, stop, buff);
                        //cout << "POS: " << buff << endl;

                        sv = handle.GetSeqVector(handle.eCoding_Iupac, 
                                                 handle.eStrand_Minus);
                        sv.GetSeqData(sv.size()-stop, sv.size()-start, buff);
                        //cout << "NEG: " << buff << endl;
                             }}

                    // enumerate descriptions
                    // Seqdesc iterator
                    for (CSeqdesc_CI desc_it(handle); desc_it;  ++desc_it) {
                        count++;
                    }
                    // verify result
                    SetValue(m_DescMap, sih, count);

                    // enumerate features
                    SAnnotSelector sel(CSeqFeatData::e_not_set);
                    if ( m_no_named ) {
                        sel.ResetAnnotsNames().AddUnnamedAnnots();
                    }
                    else if ( m_no_snp ) {
                        sel.ExcludeNamedAnnots("SNP");
                    }
                    if ( m_no_external ) {
                        sel.SetExcludeExternal();
                    }
                    if ( m_adaptive ) {
                        sel.SetAdaptiveDepth();
                    }

                    feats.clear();
                    if ( idx%2 == 0 ) {
                        sel.SetResolveMethod(sel.eResolve_All);
                        for ( CFeat_CI it(handle, sel); it; ++it ) {
                            feats.push_back(ConstRef(&it->GetOriginalFeature()));
                        }
                        if ( m_verbose ) {
                            NcbiCout << " features: " << count << NcbiEndl;
                        }
                        // verify result
                        SetValue(m_Feat0Map, sih, feats);
                    }
                    else {
                        sel.SetOverlapType(sel.eOverlap_Intervals);
                        CSeq_loc loc;
                        loc.SetWhole(const_cast<CSeq_id&>(*sih.GetSeqId()));
                        for ( CFeat_CI it(scope, loc, sel); it;  ++it ) {
                            feats.push_back(ConstRef(&it->GetOriginalFeature()));
                        }
                        // verify result
                        SetValue(m_Feat1Map, sih, feats);
                    }
                }
                if ( m_no_reset && m_keep_handles ) {
                    handles.insert(handle);
                }
            }
            catch (CLoaderException& e) {
                LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                         ": EXCEPTION = " << e.what());
                ok = false;
                if ( e.GetErrCode() == CLoaderException::eNoConnection ) {
                    break;
                }
                if ( ++error_count > kMaxErrorCount ) {
                    break;
                }
            }
            catch (exception& e) {
                LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                         ": EXCEPTION = " << e.what());
                ok = false;
                if ( ++error_count > kMaxErrorCount ) {
                    break;
                }
            }
            if ( !m_no_reset ) {
                scope.ResetHistory();
            }
        }
    }

    if ( ok ) {
        NcbiCout << " Passed" << NcbiEndl << NcbiEndl;
    }
    else {
        NcbiCout << " Failed" << NcbiEndl << NcbiEndl;
    }

    return ok;
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
    args.AddOptionalKey
        ("idlist", "IdList",
         "File with list of Seq-ids to test",
         CArgDescriptions::eInputFile);
    args.AddFlag("load_only", "Do not work with sequences - only load them");
    args.AddFlag("no_seq_map", "Do not scan CSeqMap on the sequence");
    args.AddFlag("no_snp", "Exclude SNP features from processing");
    args.AddFlag("no_named", "Exclude features from named Seq-annots");
    args.AddFlag("no_external", "Exclude all external annotations");
    args.AddFlag("adaptive", "Use adaptive depth for feature iteration");
    args.AddFlag("verbose", "Print each Seq-id before processing");
    args.AddDefaultKey
        ("pause", "Pause",
         "Pause between requests in seconds",
         CArgDescriptions::eInteger, "0");
    args.AddFlag("prefetch", "Use prefetching");
    args.AddDefaultKey("pass_count", "PassCount",
                       "Run test several times",
                       CArgDescriptions::eInteger, "1");
    args.AddFlag("no_reset", "Do not reset scope history after each id");
    args.AddFlag("keep_handles",
                 "Remember bioseq handles if not resetting scope history");
    return true;
}

bool CTestOM::TestApp_Init(void)
{
    CORE_SetLOCK(MT_LOCK_cxx2c());
    CORE_SetLOG(LOG_cxx2c());

    const CArgs& args = GetArgs();
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
        
        NcbiCout << "Testing ObjectManager (" <<
            m_Ids.size() << " Seq-ids from file)..." << NcbiEndl;
    }
    if ( m_Ids.empty() ) {
        int gi_from  = args["fromgi"].AsInteger();
        int gi_to    = args["togi"].AsInteger();
        int delta = gi_to > gi_from? 1: -1;
        for ( int gi = gi_from; gi != gi_to+delta; gi += delta ) {
            m_Ids.push_back(CSeq_id_Handle::GetGiHandle(gi));
        }
        NcbiCout << "Testing ObjectManager ("
            "gi from " << gi_from << " to " << gi_to << ")..." << NcbiEndl;
    }
    m_load_only = args["load_only"];
    m_no_seq_map = args["no_seq_map"];
    m_no_snp = args["no_snp"];
    m_no_named = args["no_named"];
    m_no_external = args["no_external"];
    m_adaptive = args["adaptive"];
    m_pause    = args["pause"].AsInteger();
    m_prefetch = args["prefetch"];
    m_verbose = args["verbose"];
    m_pass_count = args["pass_count"].AsInteger();
    m_no_reset = args["no_reset"];
    m_keep_handles = args["keep_handles"];

    m_ObjMgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);

    return true;
}

bool CTestOM::TestApp_Exit(void)
{
/*
    TIntMap::iterator it;
    for (it = m_DescMap.begin(); it != m_DescMap.end(); ++it) {
        LOG_POST(
            "id = "         << it->first.AsString()
            << ": desc = "  << it->second
            << ", feat0 = " << m_Feat0Map[it->first]
            << ", feat1 = " << m_Feat1Map[it->first]
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
    return CTestOM().AppMain(argc, argv);
}
