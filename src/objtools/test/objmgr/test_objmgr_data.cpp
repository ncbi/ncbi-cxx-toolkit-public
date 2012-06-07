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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_util.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/seq__.hpp>

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
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
//#include <internal/asn_cache/lib/asn_cache_loader.hpp>

#include <algorithm>

#include <common/test_assert.h>  /* This header must go last */


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
const int g_acc_from = 1;

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
    typedef map<TMapKey, string>            TFeatMap;
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
    CRef<CPrefetchManager> m_prefetch_manager;
    bool m_verbose;
    bool m_count_all;
    int  m_pass_count;
    int  m_idx;
    bool m_no_reset;
    bool m_keep_handles;
    bool m_selective_reset;
    bool m_edit_handles;
    bool m_release_all_memory;
    bool m_get_acc;
    bool m_get_gi;
    bool m_get_ids;
    bool m_get_label;
    bool m_get_taxid;
    bool m_get_length;
    bool m_get_type;
};


/////////////////////////////////////////////////////////////////////////////

void CTestOM::SetValue(TIntMap& vm, const CSeq_id_Handle& id, int value)
{
    if ( m_release_all_memory ) return;
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
        s << "Inconsistent "<<name<<" on "<<key.AsString()<<
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


int CTestOM::Run(void)
{
    if ( !Thread_Run(m_idx) )
        return 1;
    else
        return 0;
}

void CTestOM::Init(void)
{
    //CONNECT_Init(&GetConfig());
    //CORE_SetLOCK(MT_LOCK_cxx2c());
    //CORE_SetLOG(LOG_cxx2c());

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


static string GetAccession(const vector<CSeq_id_Handle>& ids)
{
    CSeq_id_Handle gi;
    CSeq_id_Handle text;
    ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
        if ( it->Which() == CSeq_id::e_Gi ) {
            _ASSERT(!gi);
            gi = *it;
        }
        else {
            CConstRef<CSeq_id> seq_id = it->GetSeqId();
            const CTextseq_id* text_id = seq_id->GetTextseq_Id();
            if ( text_id ) {
                _ASSERT(!text);
                text = *it;
            }
        }
    }
    if ( text ) {
        CSeq_id id;
        id.Assign(*text.GetSeqId());
        CTextseq_id* text_id = const_cast<CTextseq_id*>(id.GetTextseq_Id());
        text_id->ResetName();
        text_id->ResetRelease();
        return id.AsFastaString();
    }
    else if ( gi ) {
        return gi.AsString();
    }
    return string();
}


bool CTestOM::Thread_Run(int idx)
{
    // initialize scope
    CScope scope(*m_ObjMgr);
    scope.AddDefaults();

    vector<CSeq_id_Handle> ids = m_Ids;
    
    // make them go in opposite directions
    bool rev = idx % 2;
    if ( rev ) {
        reverse(ids.begin(), ids.end());
    }

    int pause = m_pause;

    set<CBioseq_Handle> handles;
    
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
    if ( idx%2 == 0 ) {
        sel.SetResolveMethod(sel.eResolve_All);
    }
    else if ( idx%4 == 1 ) {
        sel.SetOverlapType(sel.eOverlap_Intervals);
    }
    
    int seq_count = 0, all_ids_count = 0;
    int all_feat_count = 0, all_desc_count = 0;
    bool ok = true;
    typedef list<CConstRef<CSeq_entry> > TEntries;
    TEntries all_entries;
    for ( int pass = 0; pass < m_pass_count; ++pass ) {
        CRef<CPrefetchSequence> prefetch;
        if ( m_prefetch_manager ) {
            // Initialize prefetch token;
            prefetch = new CPrefetchSequence
                (*m_prefetch_manager,
                 new CPrefetchFeat_CIActionSource(CScopeSource::New(scope),
                                                  m_Ids, sel));
            if ( 0 ) {
                SleepMilliSec(100);
                prefetch = null;
                m_prefetch_manager = null;
            }
        }

        const int kMaxErrorCount = 30;
        int error_count = 0, null_handle_count = 0;
        TFeats feats;
        for ( size_t i = 0; i < ids.size(); ++i ) {
            CSeq_id_Handle sih = m_Ids[i];

            bool done = false;
            if ( m_get_acc ) {
                scope.GetAccVer(sih);
                done = true;
            }

            if ( m_get_gi ) {
                scope.GetGi(sih);
                done = true;
            }

            if ( m_get_ids ) {
                scope.GetIds(sih);
                done = true;
            }

            if ( m_get_label ) {
                scope.GetLabel(sih);
                done = true;
            }

            if ( m_get_taxid ) {
                scope.GetTaxId(sih);
                done = true;
            }

            if ( m_get_length ) {
                scope.GetSequenceLength(sih);
                done = true;
            }

            if ( m_get_type ) {
                scope.GetSequenceType(sih);
                done = true;
            }
            
            if ( done ) {
                continue;
            }

            if ( i != 0 && pause ) {
                SleepSec(pause);
            }
            if ( m_verbose ) {
                NcbiCout << CTime(CTime::eCurrent).AsString() << " " <<
                    i << ": " << sih.AsString() << NcbiFlush;
            }
            try {
                // load sequence
                bool preload_ids = !prefetch && (i & 1) != 0;
                vector<CSeq_id_Handle> ids1, ids2, ids3;
                if ( preload_ids ) {
                    ids1 = scope.GetIds(sih);
                    sort(ids1.begin(), ids1.end());
                }

                CRef<CPrefetchRequest> token;
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
                    ++null_handle_count;
                    LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                             ": INVALID HANDLE: "<<handle.GetState());
                    continue;
                }
                ++seq_count;

                ids2 = handle.GetId();
                all_ids_count += ids2.size();
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
                    else if ( 1 ) {
                        if ( GetAccession(ids1) != GetAccession(ids2) ) {
                            NcbiCerr << sih.AsString() <<
                                ": Ids discrepancy:\n";
                            NcbiCerr << " GetIds(): " << GetAccession(ids1);
                            NcbiCerr << "\n   handle: " << GetAccession(ids2);
                            NcbiCerr << NcbiEndl;
                        }
                    }
                    else {
                        if ( ids1 != ids2 ) {
                            NcbiCerr << sih.AsString() <<
                                ": Ids discrepancy:\n";
                            NcbiCerr << " GetIds():";
                            ITERATE ( vector<CSeq_id_Handle>, it, ids1 ) {
                                NcbiCerr << " " << it->AsString();
                            }
                            NcbiCerr << "\n   handle:";
                            ITERATE ( vector<CSeq_id_Handle>, it, ids2 ) {
                                NcbiCerr << " " << it->AsString();
                            }
                            NcbiCerr << NcbiEndl;
                        }
                    }
                }
                
                if ( m_load_only ) {
                    if ( m_verbose ) {
                        NcbiCout << NcbiEndl;
                    }
                }
                else {
                    // check CSeqMap_CI
                    if ( !m_no_seq_map ) {
                        SSeqMapSelector sel(CSeqMap::fFindRef, kMax_UInt);
                        for ( CSeqMap_CI it(handle, sel); it; ++it ) {
                            _ASSERT(it.GetType() == CSeqMap::eSeqRef);
                        }
                    }

                    // check seqvector
                    if ( 0 ) {
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
                    }

                    // enumerate descriptions
                    // Seqdesc iterator
                    int desc_count = 0;
                    for (CSeqdesc_CI desc_it(handle); desc_it;  ++desc_it) {
                        desc_count++;
                    }
                    all_desc_count += desc_count;
                    // verify result
                    SetValue(m_DescMap, sih, desc_count);

                    // enumerate features

                    feats.clear();
                    set<CSeq_annot_Handle> annots;
                    if ( idx%2 == 0 ) {
                        CFeat_CI it;
                        if ( token ) {
                            it = CStdPrefetch::GetFeat_CI(token);
                        }
                        else {
                            it = CFeat_CI(handle, sel);
                        }

                        all_feat_count += it.GetSize();
                        for ( ; it; ++it ) {
                            if ( 0 && m_verbose ) {
                                NcbiCout << MSerial_AsnText
                                         << it->GetOriginalFeature();
                                NcbiCout << MSerial_AsnText
                                         << *it->GetAnnot().GetCompleteObject();
                            }
                            feats.push_back(ConstRef(&it->GetOriginalFeature()));
                            annots.insert(it.GetAnnot());
                        }
                        //NcbiCout << " Seq-annots: " << annots.size() << NcbiEndl;
                        CAnnot_CI annot_it(handle, sel);
                        if ( m_verbose ) {
                            NcbiCout
                                << " Seq-annots: " << annot_it.size() << "/" << annots.size()
                                << " features: " << feats.size() << NcbiEndl;
                            if ( 0 ) {
                                set<CSeq_annot_Handle> new_annots;
                                for ( ; annot_it; ++annot_it ) {
                                    new_annots.insert(*annot_it);
                                }
                                NcbiCout << "Old:\n";
                                ITERATE( set<CSeq_annot_Handle>, it, annots ) {
                                    NcbiCout << MSerial_AsnText
                                             << *it->GetCompleteObject()
                                             << NcbiEndl;
                                }
                                NcbiCout << "New:\n";
                                ITERATE( set<CSeq_annot_Handle>, it, new_annots ) {
                                    NcbiCout << MSerial_AsnText
                                             << *it->GetCompleteObject()
                                             << NcbiEndl;
                                }
                                NcbiCout << "Done" << NcbiEndl;
                            }
                            annot_it.Rewind();
                        }

                        // verify result
                        SetValue(m_Feat0Map, sih, feats);

                        _ASSERT(annot_it.size() == annots.size());
                        set<CSeq_annot_Handle> annots2;
                        if ( m_edit_handles ) {
                            handle.GetEditHandle();
                        }
                        for ( ; annot_it; ++annot_it ) {
                            if ( m_edit_handles ) {
                                annot_it->GetEditHandle();
                            }
                            annots2.insert(*annot_it);
                        }
                        _ASSERT(annots.size() == annots2.size());
                        _ASSERT(annots == annots2);
                    }
                    else if ( idx%4 == 1 ) {
                        CSeq_loc loc;
                        loc.SetWhole(const_cast<CSeq_id&>(*sih.GetSeqId()));
                        CFeat_CI it;
                        if ( token ) {
                            it = CStdPrefetch::GetFeat_CI(token);
                        }
                        else {
                            it = CFeat_CI(scope, loc, sel);
                        }

                        all_feat_count += it.GetSize();
                        for ( ; it;  ++it ) {
                            feats.push_back(ConstRef(&it->GetOriginalFeature()));
                            annots.insert(it.GetAnnot());
                        }
                        CAnnot_CI annot_it(scope, loc, sel);
                        if ( m_verbose ) {
                            NcbiCout
                                << " Seq-annots: " << annot_it.size()
                                << " features: " << feats.size() << NcbiEndl;
                        }

                        // verify result
                        SetValue(m_Feat1Map, sih, feats);

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
                        all_feat_count += feat_it.GetSize();
                        for ( ; feat_it; ++feat_it ) {
                            feats.push_back(ConstRef(&feat_it->GetOriginalFeature()));
                            annots.insert(feat_it.GetAnnot());
                        }
                        CAnnot_CI annot_it(feat_it);
                        if ( m_verbose ) {
                            NcbiCout
                                << " Seq-annots: " << annot_it.size()
                                << " features: " << feats.size() << NcbiEndl;
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
                if ( m_selective_reset ) {
                    scope.ResetHistory();
                    CAnnot_CI annot_it(handle);
                }
                if ( m_no_reset ) {
                    if ( m_keep_handles ) {
                        handles.insert(handle);
                    }
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
                if ( 1 ) {
                    scope.ResetDataAndHistory();
                }
                else {
                    CBioseq_Handle handle = scope.GetBioseqHandle(sih);
                    CConstRef<CSeq_entry> entry(SerialClone(*handle.GetTopLevelEntry().GetCompleteObject()));
                    _ASSERT(entry->ReferencedOnlyOnce());
                    scope.ResetDataAndHistory();
                    _ASSERT(!handle);
                    _ASSERT(entry->ReferencedOnlyOnce());
                    scope.AddTopLevelSeqEntry(*entry);
                    handle = scope.GetBioseqHandle(sih);
                    _ASSERT(handle);
                    _ASSERT(entry==handle.GetTopLevelEntry().GetCompleteObject());
                    CFeat_CI it(handle);
                    scope.ResetDataAndHistory();
                    _ASSERT(!handle);
                    if ( !entry->ReferencedOnlyOnce() ) {
                        all_entries.push_back(entry);
                    }
                }
            }
        }
        if ( error_count || null_handle_count ) {
            ERR_POST("Null: " << null_handle_count <<
                     " Error: " << error_count);
        }
        if ( m_count_all ) {
            NcbiCout << "Found " << seq_count << " sequences." << NcbiEndl;
            NcbiCout << "Total " << all_ids_count << " ids." << NcbiEndl;
            NcbiCout << "Total " << all_desc_count << " descr." << NcbiEndl;
            NcbiCout << "Total " << all_feat_count << " feats." << NcbiEndl;
        }
    }

    if ( m_prefetch_manager ) {
        m_prefetch_manager->Shutdown();
    }

    ITERATE ( TEntries, it, all_entries ) {
        _ASSERT((*it)->ReferencedOnlyOnce());
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
    args.AddFlag("no_external", "Exclude all external annotations");
    args.AddFlag("adaptive", "Use adaptive depth for feature iteration");
    args.AddFlag("verbose", "Print each Seq-id before processing");
    args.AddFlag("get_acc", "Get accession.version only");
    args.AddFlag("get_gi", "Get gi only");
    args.AddFlag("get_ids", "Get all seq-ids only");
    args.AddFlag("get_label", "Get sequence label only");
    args.AddFlag("get_taxid", "Get sequence TaxId only");
    args.AddFlag("get_length", "Get sequence length only");
    args.AddFlag("get_type", "Get sequence type only");
    args.AddDefaultKey("thread_index", "ThreadIndex",
                       "Thread index, affects test mode",
                       CArgDescriptions::eInteger, "0");
    args.AddDefaultKey
        ("pause", "Pause",
         "Pause between requests in seconds",
         CArgDescriptions::eInteger, "0");
    args.AddFlag("prefetch", "Use prefetching");
    args.AddDefaultKey("pass_count", "PassCount",
                       "Run test several times",
                       CArgDescriptions::eInteger, "1");
    args.AddFlag("count_all",
                 "Display count of all collected data");
    args.AddFlag("selective_reset",
                 "Check ResetHistory() with locked handles");
    args.AddFlag("no_reset",
                 "Do not reset scope history after each id");
    args.AddFlag("edit_handles",
                 "Check GetEditHandle()");
    args.AddFlag("keep_handles",
                 "Remember bioseq handles if not resetting scope history");
    args.AddFlag("release_all_memory",
                 "Do not keep any objects to check memory leaks");
    return true;
}

bool CTestOM::TestApp_Init(void)
{
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
        NcbiCout << "Testing ObjectManager ("
            "accessions and gi from " <<
            g_gi_from << " to " << g_gi_to << ")..." << NcbiEndl;
    }
    m_load_only = args["load_only"];
    m_no_seq_map = args["no_seq_map"];
    m_no_snp = args["no_snp"];
    m_no_named = args["no_named"];
    m_no_external = args["no_external"];
    m_adaptive = args["adaptive"];
    m_pause    = args["pause"].AsInteger();
#ifdef NCBI_THREADS
    if ( args["prefetch"] ) {
        LOG_POST("Using prefetch");
        // Initialize prefetch token;
        m_prefetch_manager = new CPrefetchManager();
    }
#endif
    m_verbose = args["verbose"];
    m_pass_count = args["pass_count"].AsInteger();
    m_count_all = args["count_all"];
    m_selective_reset = args["selective_reset"];
    m_no_reset = args["no_reset"];
    m_edit_handles = args["edit_handles"];
    m_keep_handles = args["keep_handles"];
    m_idx = args["thread_index"].AsInteger();
    m_release_all_memory = args["release_all_memory"];
    m_get_acc = args["get_acc"];
    m_get_gi = args["get_gi"];
    m_get_ids = args["get_ids"];
    m_get_label = args["get_label"];
    m_get_taxid = args["get_taxid"];
    m_get_length = args["get_length"];
    m_get_type = args["get_type"];

    m_ObjMgr = CObjectManager::GetInstance();
#ifdef HAVE_PUBSEQ_OS
    DBAPI_RegisterDriver_FTDS();
    GenBankReaders_Register_Pubseq();
#endif
#if 0
    string asn_cache_db =
        "/panfs/pan1/gpipe/prod/data01/Homo_sapiens/QA_AllPaths_YH1.14134/sequence_cache";
    CAsnCache_DataLoader::RegisterInObjectManager(*m_ObjMgr, asn_cache_db,
                                                  CObjectManager::eDefault,
                                                  88);
#endif
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
