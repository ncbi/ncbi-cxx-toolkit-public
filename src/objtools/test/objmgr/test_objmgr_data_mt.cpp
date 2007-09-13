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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/test_mt.hpp>

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
#include <objmgr/annot_ci.hpp>
#include <objmgr/prefetch_manager.hpp>
#include <objmgr/prefetch_actions.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>

#include <algorithm>
#include <map>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;

DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);

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
const int g_acc_from = 1;

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

    typedef vector<CConstRef<CSeq_feat> >   TFeats;
    typedef pair<CSeq_id_Handle, bool>      TMapKey;
    typedef map<TMapKey, string>            TFeatMap;
    typedef map<TMapKey, int>               TIntMap;
    typedef map<TMapKey, CBlobIdKey>        TBlobIdMap;
    typedef vector<CSeq_id_Handle>          TIds;

    TBlobIdMap  m_BlobIdMap;
    TIntMap     m_DescMap;
    TFeatMap    m_Feat0Map;
    TFeatMap    m_Feat1Map;

    void SetValue(TBlobIdMap& vm, const TMapKey& key, const CBlobIdKey& value);
    void SetValue(TFeatMap& vm, const TMapKey& key, const TFeats& value);
    void SetValue(TIntMap& vm, const TMapKey& key, int value);

    TIds m_Ids;

    bool m_load_only;
    bool m_no_seq_map;
    bool m_no_snp;
    bool m_no_named;
    bool m_adaptive;
    int  m_pass_count;
    bool m_no_reset;
    bool m_keep_handles;
    bool m_verbose;
    bool m_release_all_memory;

    CRef<CPrefetchManager> m_prefetch_manager;

    bool failed;
};


/////////////////////////////////////////////////////////////////////////////


void CTestOM::SetValue(TBlobIdMap& vm, const TMapKey& key,
                       const CBlobIdKey& value)
{
    if ( m_release_all_memory ) return;
    const CBlobIdKey* old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        TBlobIdMap::iterator it = vm.lower_bound(key);
        if ( it == vm.end() || it->first != key ) {
            it = vm.insert(it, TBlobIdMap::value_type(key, value));
        }
        old_value = &it->second;
    }}
    if ( *old_value != value ) {
        string name;
        if ( &vm == &m_BlobIdMap ) name = "blob-id";
        ERR_POST("Inconsistent "<<name<<" on "<<
                 key.first.AsString()<<" "<<key.second<<
                 " was "<<old_value->ToString()<<" now "<<value.ToString());
    }
    _ASSERT(*old_value == value);
}


void CTestOM::SetValue(TIntMap& vm, const TMapKey& key, int value)
{
    if ( m_release_all_memory ) return;
    int old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        old_value = vm.insert(TIntMap::value_type(key, value)).first->second;
    }}
    if ( old_value != value ) {
        string name;
        if ( &vm == &m_DescMap ) name = "desc";
        ERR_POST("Inconsistent "<<name<<" on "<<
                 key.first.AsString()<<" "<<key.second<<
                 " was "<<old_value<<" now "<<value);
    }
    _ASSERT(old_value == value);
}


void CTestOM::SetValue(TFeatMap& vm, const TMapKey& key, const TFeats& value)
{
    if ( m_release_all_memory ) return;
    CNcbiOstrstream str;
    {{
        CObjectOStreamAsnBinary out(str);
        ITERATE ( TFeats, it, value )
            out << **it;
    }}
    string new_str = CNcbiOstrstreamToString(str);
    const string* old_str;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        TFeatMap::iterator it = vm.lower_bound(key);
        if ( it == vm.end() || it->first != key ) {
            it = vm.insert(it, TFeatMap::value_type(key, new_str));
        }
        old_str = &it->second;
    }}
    if ( *old_str != new_str ) {
        TFeats old_value;
        {{
            CNcbiIstrstream str(old_str->data(), old_str->size());
            CObjectIStreamAsnBinary in(str);
            while ( in.HaveMoreData() ) {
                CRef<CSeq_feat> feat(new CSeq_feat);
                in >> *feat;
                old_value.push_back(feat);
            }
        }}
        CNcbiOstrstream s;
        string name;
        if ( &vm == &m_Feat0Map ) name = "feat0";
        if ( &vm == &m_Feat1Map ) name = "feat1";
        s << "Inconsistent "<<name<<" on "<<key.first.AsString()<<
            " was "<<old_value.size()<<" now "<<value.size() << NcbiEndl;
        ITERATE ( TFeats, it, old_value ) {
            s << " old: " << MSerial_AsnText << **it;
        }
        ITERATE ( TFeats, it, value ) {
            s << " new: " << MSerial_AsnText << **it;
        }
        ERR_POST(string(CNcbiOstrstreamToString(s)));
    }
    _ASSERT(*old_str == new_str);
}


bool CTestOM::Thread_Run(int idx)
{
    // initialize scope
    CGBDataLoader::RegisterInObjectManager(*CObjectManager::GetInstance());
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    vector<CSeq_id_Handle> ids = m_Ids;
    
    // make them go in opposite directions
    bool rev = idx % 2;
    if ( rev ) {
        reverse(ids.begin(), ids.end());
    }

    SAnnotSelector sel(CSeqFeatData::e_not_set);
    if ( m_no_named ) {
        sel.ResetAnnotsNames().AddUnnamedAnnots();
    }
    else if ( m_no_snp ) {
        sel.ExcludeNamedAnnots("SNP");
    }
    if ( m_adaptive ) {
        sel.SetAdaptiveDepth();
    }
    if ( idx%2 == 0 ) {
        sel.SetOverlapType(sel.eOverlap_Intervals);
        sel.SetResolveMethod(sel.eResolve_All);
    }
    
    set<CBioseq_Handle> handles;

    bool ok = true;
    for ( int pass = 0; pass < m_pass_count; ++pass ) {
        CRef<CPrefetchSequence> prefetch;
        if ( m_prefetch_manager ) {
            // Initialize prefetch token;
            prefetch = new CPrefetchSequence
                (*m_prefetch_manager,
                 new CPrefetchFeat_CIActionSource(CScopeSource::New(scope),
                                                  ids, sel));
        }

        const int kMaxErrorCount = 3;
        static int error_count = 0;
        TFeats feats;
        for ( size_t i = 0; i < ids.size(); ++i ) {
            CSeq_id_Handle sih = ids[i];
            CNcbiOstrstream out;
            if ( m_verbose ) {
                out << CTime(CTime::eCurrent).AsString() << " " << i << ": "
                    << sih.AsString();
            }
            TMapKey key(sih, rev);
            try {
                // load sequence
                bool preload_ids = !prefetch && ((idx ^ i) & 2) != 0;
                vector<CSeq_id_Handle> ids1, ids2, ids3;
                if ( preload_ids ) {
                    ids1 = scope.GetIds(sih);
                    sort(ids1.begin(), ids1.end());
                }

                CPrefetchToken token;
                if ( prefetch ) {
                    token = prefetch->GetNextToken();
                }
                CBioseq_Handle handle;
                if ( token ) {
                    handle = CStdPrefetch::GetBioseqHandle(token);
                }
                else {
                    handle = scope.GetBioseqHandle(sih);
                }

                if ( !handle ) {
                    if ( preload_ids ) {
                        int mask =
                            handle.fState_confidential|
                            handle.fState_withdrawn;
                        bool restricted =
                            (handle.GetState() & mask) != 0;
                        if ( !restricted ) {
                            _ASSERT(ids1.empty());
                        }
                    }
                    LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                             ": INVALID HANDLE");
                    SetValue(m_DescMap, key, -1);
                    continue;
                }
                SetValue(m_BlobIdMap, key, handle.GetTSE_Handle().GetBlobId());

                ids2 = handle.GetId();
                sort(ids2.begin(), ids2.end());
                _ASSERT(!ids2.empty());
                ids3 = handle.GetScope().GetIds(sih);
                sort(ids3.begin(), ids3.end());
                _ASSERT(ids2 == ids3);
                if ( preload_ids ) {
                    if ( 1 ) {
                        if ( ids1.empty() ) {
                            ERR_POST("Ids discrepancy for " << sih.AsString());
                        }
                    }
                    else {
                        if ( ids1 != ids2 ) {
                            ERR_POST("Ids discrepancy for " << sih.AsString());
                        }
                        //_ASSERT(ids1 == ids2);
                    }
                }

                if ( !m_load_only ) {
                    // check CSeqMap_CI
                    if ( !m_no_seq_map ) {
                        SSeqMapSelector sel(CSeqMap::fFindRef, kMax_UInt);
                        for ( CSeqMap_CI it(handle, sel); it; ++it ) {
                            _ASSERT(it.GetType() == CSeqMap::eSeqRef);
                        }
                    }

                    // enumerate descriptions
                    // Seqdesc iterator
                    int desc_count = 0;
                    for (CSeqdesc_CI desc_it(handle); desc_it;  ++desc_it) {
                        desc_count++;
                    }
                    // verify result
                    SetValue(m_DescMap, key, desc_count);

                    // enumerate features
                    CSeq_loc loc;
                    loc.SetWhole(const_cast<CSeq_id&>(*sih.GetSeqId()));

                    feats.clear();
                    set<CSeq_annot_Handle> annots;
                    if ( idx%2 == 0 ) {
                        CFeat_CI it;
                        if ( token ) {
                            it = CStdPrefetch::GetFeat_CI(token);
                        }
                        else {
                            it = CFeat_CI(scope, loc, sel);
                        }
                        for ( ; it;  ++it ) {
                            feats.push_back(ConstRef(&it->GetOriginalFeature()));
                            annots.insert(it.GetAnnot());
                        }
                        CAnnot_CI annot_it(handle.GetScope(), loc, sel);
                        if ( m_verbose ) {
                            out << " Seq-annots: " << annot_it.size()
                                << " features: " << feats.size();
                        }

                        // verify result
                        SetValue(m_Feat0Map, key, feats);

                        _ASSERT(annot_it.size() == annots.size());
                        set<CSeq_annot_Handle> annots2;
                        for ( ; annot_it; ++annot_it ) {
                            annots2.insert(*annot_it);
                        }
                        _ASSERT(annots.size() == annots2.size());
                        _ASSERT(annots == annots2);
                    }
                    else if ( idx%4 == 1 ) {
                        CFeat_CI it;
                        if ( token ) {
                            it = CStdPrefetch::GetFeat_CI(token);
                        }
                        else {
                            it = CFeat_CI(handle, sel);
                        }

                        for ( ; it;  ++it ) {
                            feats.push_back(ConstRef(&it->GetOriginalFeature()));
                            annots.insert(it.GetAnnot());
                        }
                        CAnnot_CI annot_it(handle, sel);
                        if ( m_verbose ) {
                            out << " Seq-annots: " << annot_it.size()
                                << " features: " << feats.size();
                        }

                        // verify result
                        SetValue(m_Feat1Map, key, feats);

                        _ASSERT(annot_it.size() == annots.size());
                        set<CSeq_annot_Handle> annots2;
                        for ( ; annot_it; ++annot_it ) {
                            annots2.insert(*annot_it);
                        }
                        _ASSERT(annots.size() == annots2.size());
                        _ASSERT(annots == annots2);
                    }
                    else {
                        CFeat_CI feat_it(handle.GetTopLevelEntry(), sel);
                        for ( ; feat_it; ++feat_it ) {
                            feats.push_back(ConstRef(&feat_it->GetOriginalFeature()));
                            annots.insert(feat_it.GetAnnot());
                        }
                        CAnnot_CI annot_it(feat_it);
                        if ( m_verbose ) {
                            out << " Seq-annots: " << annot_it.size()
                                << " features: " << feats.size();
                        }

                        _ASSERT(annot_it.size() == annots.size());
                        set<CSeq_annot_Handle> annots2;
                        for ( ; annot_it; ++annot_it ) {
                            annots2.insert(*annot_it);
                        }
                        _ASSERT(annots.size() == annots2.size());
                        _ASSERT(annots == annots2);
                    }
                }
                if ( m_verbose ) {
                    out << NcbiEndl;
                    string msg = CNcbiOstrstreamToString(out);
                    NcbiCout << msg << NcbiFlush;
                }
                if ( m_no_reset && m_keep_handles ) {
                    handles.insert(handle);
                }
            }
            catch (CLoaderException& e) {
                if ( m_verbose ) {
                    string msg = CNcbiOstrstreamToString(out);
                    LOG_POST("T" << idx << ": " << msg);
                }
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
                if ( m_verbose ) {
                    string msg = CNcbiOstrstreamToString(out);
                    LOG_POST("T" << idx << ": " << msg);
                }
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

    if ( !ok ) {
        failed = true;
    }
    return ok;
}

bool CTestOM::TestApp_Args( CArgDescriptions& args)
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
    args.AddFlag("load_only", "Do not work with sequences - only load them");
    args.AddFlag("no_seq_map", "Do not scan CSeqMap on the sequence");
    args.AddFlag("no_snp", "Exclude SNP features from processing");
    args.AddFlag("no_named", "Exclude features from named Seq-annots");
    args.AddFlag("adaptive", "Use adaptive depth for feature iteration");
    args.AddDefaultKey("pass_count", "PassCount",
                       "Run test several times",
                       CArgDescriptions::eInteger, "1");
    args.AddFlag("no_reset", "Do not reset scope history after each id");
    args.AddFlag("keep_handles",
                 "Remember bioseq handles if not resetting scope history");
    args.AddFlag("verbose", "Print each Seq-id before processing");
    args.AddFlag("prefetch", "Use prefetching");
    args.AddFlag("release_all_memory",
                 "Do not keep any objects to check memory leaks");
    return true;
}

bool CTestOM::TestApp_Init(void)
{
    failed = false;
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
    if ( m_Ids.empty() && (args["fromgi"] || args["togi"]) ) {
        int gi_from  = args["fromgi"]? args["fromgi"].AsInteger(): g_gi_from;
        int gi_to    = args["togi"]? args["togi"].AsInteger(): g_gi_to;
        int delta = gi_to > gi_from? 1: -1;
        for ( int gi = gi_from; gi != gi_to+delta; gi += delta ) {
            m_Ids.push_back(CSeq_id_Handle::GetGiHandle(gi));
        }
        NcbiCout << "Testing ObjectManager ("
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
                str << "AA" << setfill('0') << setw(6) << (i/3+g_acc_from);
                string acc = CNcbiOstrstreamToString(str);
                CSeq_id seq_id(acc);
                m_Ids.push_back(CSeq_id_Handle::GetHandle(seq_id));
            }
        }
        NcbiCout << "Testing ObjectManager ("
            "accessions and gi from " <<
            g_gi_from << " to " << g_gi_to << ")..." << NcbiEndl;
    }
    m_load_only = args["load_only"];
    m_no_seq_map = args["no_seq_map"];
    m_no_snp = args["no_snp"];
    m_no_named = args["no_named"];
    m_adaptive = args["adaptive"];
    m_pass_count = args["pass_count"].AsInteger();
    m_no_reset = args["no_reset"];
    m_keep_handles = args["keep_handles"];
    m_verbose = args["verbose"];
    m_release_all_memory = args["release_all_memory"];

    if ( args["prefetch"] ) {
        LOG_POST("Using prefetch");
        m_prefetch_manager = new CPrefetchManager();
    }

    return true;
}

bool CTestOM::TestApp_Exit(void)
{
    if ( failed ) {
        NcbiCout << " Failed" << NcbiEndl << NcbiEndl;
    }
    else {
        NcbiCout << " Passed" << NcbiEndl << NcbiEndl;
    }

/*
    map<int, int>::iterator it;
    for (it = m_DescMap.begin(); it != m_DescMap.end(); ++it) {
        LOG_POST(
            "gi = "         << it->first
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
    s_NumThreads = 12;
    return CTestOM().AppMain(argc, argv);
}
