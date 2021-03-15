/*
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
* Author:  Jonathan Kans
*
*/

#include <ncbi_pch.hpp>

#include <util/unicode.hpp>
#include <util/static_set.hpp>
#include <util/static_map.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/error_codes.hpp>

#include <objmgr/util/indexer.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature_edit.hpp>

#define NCBI_USE_ERRCODE_X  ObjMgr_Indexer

BEGIN_NCBI_SCOPE
NCBI_DEFINE_ERR_SUBCODE_X(11);
BEGIN_SCOPE(objects)


// CSeqEntryIndex

// Constructors take top-level sequence object, create a CRef<CSeqMasterIndex>, and call its initializer
CSeqEntryIndex::CSeqEntryIndex (CSeq_entry_Handle& topseh, EPolicy policy, TFlags flags)

{
    m_Idx.Reset(new CSeqMasterIndex);
    m_Idx->x_Initialize(topseh, policy, flags);
}

CSeqEntryIndex::CSeqEntryIndex (CBioseq_Handle& bsh, EPolicy policy, TFlags flags)

{
    m_Idx.Reset(new CSeqMasterIndex);
    m_Idx->x_Initialize(bsh, policy, flags);
}

CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, EPolicy policy, TFlags flags)

{
    m_Idx.Reset(new CSeqMasterIndex);
    m_Idx->x_Initialize(topsep, policy, flags);
}

CSeqEntryIndex::CSeqEntryIndex (CBioseq_set& seqset, EPolicy policy, TFlags flags)

{
    m_Idx.Reset(new CSeqMasterIndex);
    m_Idx->x_Initialize(seqset, policy, flags);
}

CSeqEntryIndex::CSeqEntryIndex (CBioseq& bioseq, EPolicy policy, TFlags flags)

{
    m_Idx.Reset(new CSeqMasterIndex);
    m_Idx->x_Initialize(bioseq, policy, flags);
}

CSeqEntryIndex::CSeqEntryIndex (CSeq_submit& submit, EPolicy policy, TFlags flags)

{
    m_Idx.Reset(new CSeqMasterIndex);
    m_Idx->x_Initialize(submit, policy, flags);
}

CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, CSubmit_block &sblock, EPolicy policy, TFlags flags)

{
    m_Idx.Reset(new CSeqMasterIndex);
    m_Idx->x_Initialize(topsep, sblock, policy, flags);
}

CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, CSeq_descr &descr, EPolicy policy, TFlags flags)

{
    m_Idx.Reset(new CSeqMasterIndex);
    m_Idx->x_Initialize(topsep, descr, policy, flags);
}

// Get first Bioseq index
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (void)

{
    return m_Idx->GetBioseqIndex();
}

// Get Nth Bioseq index
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (int n)

{
    return m_Idx->GetBioseqIndex(n);
}

// Get Bioseq index by accession
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (const string& accn)

{
    return m_Idx->GetBioseqIndex(accn);
}

// Get Bioseq index by handle (via best Seq-id string)
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (CBioseq_Handle bsh)

{
    return m_Idx->GetBioseqIndex(bsh);
}

// // Get Bioseq index by feature
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (const CMappedFeat& mf)

{
    return m_Idx->GetBioseqIndex(mf);
}

// Get Bioseq index by sublocation
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (const CSeq_loc& loc)

{
    return m_Idx->GetBioseqIndex(loc);
}

const vector<CRef<CBioseqIndex>>& CSeqEntryIndex::GetBioseqIndices(void)

{
    return m_Idx->GetBioseqIndices();
}

const vector<CRef<CSeqsetIndex>>& CSeqEntryIndex::GetSeqsetIndices(void)

{
    return m_Idx->GetSeqsetIndices();
}

bool CSeqEntryIndex::DistributedReferences(void)

{
    return m_Idx->DistributedReferences();
}

void CSeqEntryIndex::SetSnpFunc(FAddSnpFunc* snp)

{
    m_Idx->SetSnpFunc (snp);
}

FAddSnpFunc* CSeqEntryIndex::GetSnpFunc(void)

{
    return m_Idx->GetSnpFunc();
}

void CSeqEntryIndex::SetFeatDepth(int featDepth)

{
    m_Idx->SetFeatDepth (featDepth);
}

int CSeqEntryIndex::GetFeatDepth(void)

{
    return m_Idx->GetFeatDepth();
}

void CSeqEntryIndex::SetGapDepth(int featDepth)

{
    m_Idx->SetGapDepth (featDepth);
}

int CSeqEntryIndex::GetGapDepth(void)

{
    return m_Idx->GetGapDepth();
}

bool CSeqEntryIndex::IsFetchFailure(void)

{
    return m_Idx->IsFetchFailure();
}

bool CSeqEntryIndex::IsIndexFailure(void)

{
    return m_Idx->IsIndexFailure();
}


// CSeqMasterIndex

// Initializers take top-level sequence object, create Seq-entry wrapper if necessary
void CSeqMasterIndex::x_Initialize (CSeq_entry_Handle& topseh, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags)
{
    m_Policy = policy;
    m_Flags = flags;

    m_Tseh = topseh.GetTopLevelEntry();
    CConstRef<CSeq_entry> tcsep = m_Tseh.GetCompleteSeq_entry();
    CSeq_entry& topsep = const_cast<CSeq_entry&>(*tcsep);
    topsep.Parentize();
    m_Tsep.Reset(&topsep);

    m_FeatTree = new feature::CFeatTree;

    m_HasOperon = false;
    m_IsSmallGenomeSet = false;
    m_DistributedReferences = false;
    m_SnpFunc = 0;
    m_FeatDepth = 0;
    m_GapDepth = 0;
    m_IndexFailure = false;

    try {
        // Code copied from x_Init, then modified to reuse existing scope from CSeq_entry_Handle
        m_Objmgr = CObjectManager::GetInstance();
        if ( !m_Objmgr ) {
            // raise hell
            m_IndexFailure = true;
        }

        m_Scope.Reset( &m_Tseh.GetScope() );
        if ( !m_Scope ) {
            // raise hell
            m_IndexFailure = true;
        }

        m_Counter.Set(0);

        // Populate vector of CBioseqIndex objects representing local Bioseqs in blob
        CRef<CSeqsetIndex> noparent;
        x_InitSeqs( *m_Tsep, noparent );
    }
    catch (CException& e) {
        m_IndexFailure = true;
        ERR_POST_X(1, Error << "Error in CSeqMasterIndex::x_Init: " << e.what());
    }
}

void CSeqMasterIndex::x_Initialize (CBioseq_Handle& bsh, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags)
{
    m_Policy = policy;
    m_Flags = flags;

    m_Tseh = bsh.GetTopLevelEntry();
    CConstRef<CSeq_entry> tcsep = m_Tseh.GetCompleteSeq_entry();
    CSeq_entry& topsep = const_cast<CSeq_entry&>(*tcsep);
    topsep.Parentize();
    m_Tsep.Reset(&topsep);

    m_FeatTree = new feature::CFeatTree;

    m_HasOperon = false;
    m_IsSmallGenomeSet = false;
    m_DistributedReferences = false;
    m_SnpFunc = 0;
    m_FeatDepth = 0;
    m_GapDepth = 0;
    m_IndexFailure = false;

    try {
        // Code copied from x_Init, then modified to reuse existing scope from CSeq_entry_Handle
        m_Objmgr = CObjectManager::GetInstance();
        if ( !m_Objmgr ) {
            // raise hell
            m_IndexFailure = true;
        }

        m_Scope.Reset( &m_Tseh.GetScope() );
        if ( !m_Scope ) {
            // raise hell
            m_IndexFailure = true;
        }

        m_Counter.Set(0);

        // Populate vector of CBioseqIndex objects representing local Bioseqs in blob
        CRef<CSeqsetIndex> noparent;
        x_InitSeqs( *m_Tsep, noparent );
    }
    catch (CException& e) {
        m_IndexFailure = true;
        ERR_POST_X(1, Error << "Error in CSeqMasterIndex::x_Init: " << e.what());
    }
}

void CSeqMasterIndex::x_Initialize (CSeq_entry& topsep, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags)
{
    m_Policy = policy;
    m_Flags = flags;

    topsep.Parentize();
    m_Tsep.Reset(&topsep);

    x_Init();
}

void CSeqMasterIndex::x_Initialize (CBioseq_set& seqset, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags)
{
    m_Policy = policy;
    m_Flags = flags;

    CSeq_entry* parent = seqset.GetParentEntry();
    if (parent) {
        parent->Parentize();
        m_Tsep.Reset(parent);
    } else {
        CRef<CSeq_entry> sep(new CSeq_entry);
        sep->SetSet(seqset);
        sep->Parentize();
        m_Tsep.Reset(sep);
    }

    x_Init();
}

void CSeqMasterIndex::x_Initialize (CBioseq& bioseq, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags)
{
    m_Policy = policy;
    m_Flags = flags;

    CSeq_entry* parent = bioseq.GetParentEntry();
    if (parent) {
        parent->Parentize();
        m_Tsep.Reset(parent);
    } else {
        CRef<CSeq_entry> sep(new CSeq_entry);
        sep->SetSeq(bioseq);
        sep->Parentize();
        m_Tsep.Reset(sep);
    }

    x_Init();
}

void CSeqMasterIndex::x_Initialize (CSeq_submit& submit, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags)
{
    m_Policy = policy;
    m_Flags = flags;

    _ASSERT(submit.CanGetData());
    _ASSERT(submit.CanGetSub());
    _ASSERT(submit.GetData().IsEntrys());
    _ASSERT(!submit.GetData().GetEntrys().empty());

    CRef<CSeq_entry> sep = submit.GetData().GetEntrys().front();
    sep->Parentize();
    m_Tsep.Reset(sep);
    m_SbtBlk.Reset(&submit.GetSub());

    x_Init();
}

void CSeqMasterIndex::x_Initialize (CSeq_entry& topsep, CSubmit_block &sblock, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags)
{
    m_Policy = policy;
    m_Flags = flags;

    topsep.Parentize();
    m_Tsep.Reset(&topsep);
    m_SbtBlk.Reset(&sblock);

    x_Init();
}

void CSeqMasterIndex::x_Initialize (CSeq_entry& topsep, CSeq_descr &descr, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags)
{
    m_Policy = policy;
    m_Flags = flags;

    topsep.Parentize();
    m_Tsep.Reset(&topsep);
    m_TopDescr.Reset(&descr);

    x_Init();
}

void CSeqMasterIndex::SetSnpFunc (FAddSnpFunc* snp)

{
    m_SnpFunc = snp;
}

FAddSnpFunc* CSeqMasterIndex::GetSnpFunc (void)

{
    return m_SnpFunc;
}

void CSeqMasterIndex::SetFeatDepth (int featDepth)

{
    m_FeatDepth = featDepth;
}

int CSeqMasterIndex::GetFeatDepth (void)

{
    return m_FeatDepth;
}

void CSeqMasterIndex::SetGapDepth (int gapDepth)

{
    m_GapDepth = gapDepth;
}

int CSeqMasterIndex::GetGapDepth (void)

{
    return m_GapDepth;
}


// At end of program, poll all Bioseqs to check for far fetch failure flag
bool CSeqMasterIndex::IsFetchFailure (void)

{
    for (auto& bsx : m_BsxList) {
        if (bsx->IsFetchFailure()) {
            return true;
        }
    }
    return false;
}

// FindBestIdChoice modified from feature_item.cpp
static int s_IdxSeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::Score(id_non_const);
}

static CSeq_id_Handle s_IdxFindBestIdChoice(const CBioseq_Handle::TId& ids)
{
    CBestChoiceTracker< CSeq_id_Handle, int (*)(const CSeq_id_Handle&) > 
        tracker(s_IdxSeqIdHandle);

    ITERATE( CBioseq_Handle::TId, it, ids ) {
        switch( (*it).Which() ) {
            case CSeq_id::e_Local:
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Gi:
            case CSeq_id::e_Other:
            case CSeq_id::e_General:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
            case CSeq_id::e_Gpipe:
                tracker(*it);
                break;
            default:
                break;
        }
    }
    return tracker.GetBestChoice();
}

static string s_IdxGetBestIdString(CBioseq_Handle bsh)

{
    if (bsh) {
        const CBioseq_Handle::TId& ids = bsh.GetId();
        if (! ids.empty()) {
            CSeq_id_Handle best = s_IdxFindBestIdChoice(ids);
            if (best) {
                return best.AsString();
            }
        }
    }

    return "";
}

// Recursively explores from top-level Seq-entry to make flattened vector of CBioseqIndex objects
void CSeqMasterIndex::x_InitSeqs (const CSeq_entry& sep, CRef<CSeqsetIndex> prnt, int level)

{
    if (sep.IsSeq()) {
        // Is Bioseq
        const CBioseq& bsp = sep.GetSeq();
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(bsp);
        if (bsh) {
            // create CBioseqIndex object for current Bioseq
            CRef<CBioseqIndex> bsx(new CBioseqIndex(bsh, bsp, bsh, prnt, m_Tseh, m_Scope, *this, m_Policy, m_Flags));

            // record CBioseqIndex in vector for IterateBioseqs or GetBioseqIndex
            m_BsxList.push_back(bsx);

            // map from accession string to CBioseqIndex object
            const string& accn = bsx->GetAccession();
            m_AccnIndexMap[accn] = bsx;

            const CBioseq_Handle::TId& ids = bsh.GetId();
            if (! ids.empty()) {
                ITERATE( CBioseq_Handle::TId, it, ids ) {
                    switch( (*it).Which() ) {
                        case CSeq_id::e_Local:
                        case CSeq_id::e_Genbank:
                        case CSeq_id::e_Embl:
                        case CSeq_id::e_Ddbj:
                        case CSeq_id::e_Gi:
                        case CSeq_id::e_Other:
                        case CSeq_id::e_General:
                        case CSeq_id::e_Tpg:
                        case CSeq_id::e_Tpe:
                        case CSeq_id::e_Tpd:
                        case CSeq_id::e_Gpipe:
                        {
                            // map from handle to Seq-id string to CBioseqIndex object
                            string str = (*it).AsString();
                            m_BestIdIndexMap[str] = bsx;
                            break;
                        }
                        default:
                            break;
                    }
                }
            }

            if (bsp.IsSetDescr()) {
                for (auto& desc : bsp.GetDescr().Get()) {
                    if (desc->Which() == CSeqdesc::e_Pub) {
                        m_DistributedReferences = true;
                    }
                }
            }

            if (bsp.IsSetAnnot()) {
                for (auto& annt : bsp.GetAnnot()) {
                    if (annt->IsFtable()) {
                        for (auto& feat : annt->GetData().GetFtable()) {
                            if (feat->IsSetData() && feat->GetData().Which() == CSeqFeatData::e_Pub) {
                                m_DistributedReferences = true;
                            } else if (feat->IsSetCit()) {
                                m_DistributedReferences = true;
                            }
                        }
                    }
                }
            }
        }
    } else if (sep.IsSet()) {
        // Is Bioseq-set
        const CBioseq_set& bssp = sep.GetSet();
        CBioseq_set_Handle ssh = m_Scope->GetBioseq_setHandle(bssp);
        if (ssh) {
            // create CSeqsetIndex object for current Bioseq-set
            CRef<CSeqsetIndex> ssx(new CSeqsetIndex(ssh, bssp, prnt));

            if (ssx->GetClass() ==  CBioseq_set::eClass_small_genome_set) {
                m_IsSmallGenomeSet = true;
            }

            if (level > 0 && bssp.IsSetDescr()) {
                for (auto& desc : bssp.GetDescr().Get()) {
                    if (desc->Which() == CSeqdesc::e_Pub) {
                        m_DistributedReferences = true;
                    }
                }
            }

            // record CSeqsetIndex in vector
            m_SsxList.push_back(ssx);

            if (bssp.CanGetSeq_set()) {
                // recursively explore current Bioseq-set
                for (const CRef<CSeq_entry>& tmp : bssp.GetSeq_set()) {
                    x_InitSeqs(*tmp, ssx, level + 1);
                }
            }

            if (bssp.IsSetAnnot()) {
                for (auto& annt : bssp.GetAnnot()) {
                    if (annt->IsFtable()) {
                        for (auto& feat : annt->GetData().GetFtable()) {
                            if (feat->IsSetData() && feat->GetData().Which() == CSeqFeatData::e_Pub) {
                                m_DistributedReferences = true;
                            } else if (feat->IsSetCit()) {
                                m_DistributedReferences = true;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Common initialization function creates local default CScope
void CSeqMasterIndex::x_Init (void)

{
    m_FeatTree = new feature::CFeatTree;

    m_HasOperon = false;
    m_IsSmallGenomeSet = false;
    m_DistributedReferences = false;
    m_SnpFunc = 0;
    m_FeatDepth = 0;
    m_GapDepth = 0;
    m_IndexFailure = false;

    try {
        m_Objmgr = CObjectManager::GetInstance();
        if ( !m_Objmgr ) {
            // raise hell
            m_IndexFailure = true;
        }

        m_Scope.Reset( new CScope( *m_Objmgr ) );
        if ( !m_Scope ) {
            // raise hell
            m_IndexFailure = true;
        }

        m_Counter.Set(0);

        m_Scope->AddDefaults();

        m_Tseh = m_Scope->AddTopLevelSeqEntry( *m_Tsep );

        // Populate vector of CBioseqIndex objects representing local Bioseqs in blob
        CRef<CSeqsetIndex> noparent;
        x_InitSeqs( *m_Tsep, noparent );
    }
    catch (CException& e) {
        m_IndexFailure = true;
        ERR_POST_X(1, Error << "Error in CSeqMasterIndex::x_Init: " << e.what());
    }
}

// Get first Bioseq index
CRef<CBioseqIndex> CSeqMasterIndex::GetBioseqIndex (void)

{
    for (auto& bsx : m_BsxList) {
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Nth Bioseq index
CRef<CBioseqIndex> CSeqMasterIndex::GetBioseqIndex (int n)

{
    for (auto& bsx : m_BsxList) {
        n--;
        if (n > 0) continue;
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Bioseq index by accession
CRef<CBioseqIndex> CSeqMasterIndex::GetBioseqIndex (const string& accn)

{
    TAccnIndexMap::iterator it = m_AccnIndexMap.find(accn);
    if (it != m_AccnIndexMap.end()) {
        CRef<CBioseqIndex> bsx = it->second;
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Bioseq index by handle (via best Seq-id string)
CRef<CBioseqIndex> CSeqMasterIndex::GetBioseqIndex (CBioseq_Handle bsh)

{
    string bestid = s_IdxGetBestIdString(bsh);
    TBestIdIndexMap::iterator it = m_BestIdIndexMap.find(bestid);
    if (it != m_BestIdIndexMap.end()) {
        CRef<CBioseqIndex> bsx = it->second;
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Bioseq index by string
CRef<CBioseqIndex> CSeqMasterIndex::GetBioseqIndex (string& str)

{
    TBestIdIndexMap::iterator it = m_BestIdIndexMap.find(str);
    if (it != m_BestIdIndexMap.end()) {
        CRef<CBioseqIndex> bsx = it->second;
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Bioseq index by feature
CRef<CBioseqIndex> CSeqMasterIndex::GetBioseqIndex (const CMappedFeat& mf)

{
    CSeq_id_Handle idh = mf.GetLocationId();
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(idh);
    return GetBioseqIndex(bsh);
}

// Get Bioseq index by sublocation
CRef<CBioseqIndex> CSeqMasterIndex::GetBioseqIndex (const CSeq_loc& loc)

{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(loc);
    return GetBioseqIndex(bsh);
}

// Allow access to internal vectors for application to use in iterators
const vector<CRef<CBioseqIndex>>& CSeqMasterIndex::GetBioseqIndices(void)

{
    return m_BsxList;
}

const vector<CRef<CSeqsetIndex>>& CSeqMasterIndex::GetSeqsetIndices(void)

{
    return m_SsxList;
}


// CSeqsetIndex

// Constructor
CSeqsetIndex::CSeqsetIndex (CBioseq_set_Handle ssh,
                            const CBioseq_set& bssp,
                            CRef<CSeqsetIndex> prnt)
    : m_Ssh(ssh),
      m_Bssp(bssp),
      m_Prnt(prnt)
{
    m_Class = CBioseq_set::eClass_not_set;

    if (ssh.IsSetClass()) {
        m_Class = ssh.GetClass();
    }
}


// CBioseqIndex

// Constructor
CBioseqIndex::CBioseqIndex (CBioseq_Handle bsh,
                            const CBioseq& bsp,
                            CBioseq_Handle obsh,
                            CRef<CSeqsetIndex> prnt,
                            CSeq_entry_Handle tseh,
                            CRef<CScope> scope,
                            CSeqMasterIndex& idx,
                            CSeqEntryIndex::EPolicy policy,
                            CSeqEntryIndex::TFlags flags)
    : m_Bsh(bsh),
      m_Bsp(bsp),
      m_OrigBsh(obsh),
      m_Prnt(prnt),
      m_Tseh(tseh),
      m_Scope(scope),
      m_Idx(&idx),
      m_Policy(policy),
      m_Flags(flags)
{
    m_FetchFailure = false;

    m_GapsInitialized = false;
    m_DescsInitialized = false;
    m_FeatsInitialized = false;
    m_SourcesInitialized = false;
    m_FeatForProdInitialized = false;
    m_BestProtFeatInitialized = false;

    m_ForceOnlyNearFeats = false;

    // reset member variables to cleared state
    m_IsNA = false;
    m_IsAA = false;
    m_Topology = NCBI_SEQTOPOLOGY(not_set);

    m_IsDelta = false;
    m_IsDeltaLitOnly = false;
    m_IsVirtual = false;
    m_IsMap = false;

    m_Title.clear();

    m_MolInfo.Reset();
    m_Biomol = CMolInfo::eBiomol_unknown;
    m_Tech = CMolInfo::eTech_unknown;
    m_Completeness = CMolInfo::eCompleteness_unknown;

    m_Accession.clear();

    m_IsRefSeq = false;
    m_IsNC = false;
    m_IsNM = false;
    m_IsNR = false;
    m_IsNZ = false;
    m_IsPatent = false;
    m_IsPDB = false;
    m_IsWP = false;
    m_ThirdParty = false;
    m_WGSMaster = false;
    m_TSAMaster = false;
    m_TLSMaster = false;

    m_GeneralStr.clear();
    m_GeneralId = 0;
    m_PatentCountry.clear();
    m_PatentNumber.clear();

    m_PatentSequence = 0;

    m_PDBChain = 0;
    m_PDBChainID.clear();

    m_HTGTech = false;
    m_HTGSUnfinished = false;
    m_IsTLS = false;
    m_IsTSA = false;
    m_IsWGS = false;
    m_IsEST_STS_GSS = false;

    m_UseBiosrc = false;

    m_HTGSCancelled = false;
    m_HTGSDraft = false;
    m_HTGSPooled = false;
    m_TPAExp = false;
    m_TPAInf = false;
    m_TPAReasm = false;
    m_Unordered = false;

    m_PDBCompound.clear();

    m_DescBioSource.Reset();
    m_DescTaxname.clear();

    m_BioSource.Reset();
    m_Taxname.clear();
    m_Common.clear();
    m_Lineage.clear();
    m_Taxid = ZERO_TAX_ID;
    m_UsingAnamorph = false;
    m_Genus.clear();
    m_Species.clear();
    m_Multispecies = false;
    m_Genome = NCBI_GENOME(unknown);
    m_IsPlasmid = false;
    m_IsChromosome = false;

    m_Organelle.clear();

    m_FirstSuperKingdom.clear();
    m_SecondSuperKingdom.clear();
    m_IsCrossKingdom = false;

    m_Chromosome.clear();
    m_LinkageGroup.clear();
    m_Clone.clear();
    m_has_clone = false;
    m_Map.clear();
    m_Plasmid.clear();
    m_Segment.clear();

    m_Breed.clear();
    m_Cultivar.clear();
    m_Isolate.clear();
    m_Strain.clear();
    m_Substrain.clear();
    m_MetaGenomeSource.clear();

    m_IsUnverified = false;
    m_IsUnverifiedFeature = false;
    m_IsUnverifiedOrganism = false;
    m_IsUnverifiedMisassembled = false;
    m_IsUnverifiedContaminant = false;

    m_TargetedLocus.clear();

    m_Comment.clear();
    m_IsPseudogene = false;

    m_HasGene = false;
    m_HasMultiIntervalGenes = false;
    m_HasSource = false;

    m_rEnzyme.clear();

    // now start setting member variables from Bioseq
    m_IsNA = m_Bsh.IsNa();
    m_IsAA = m_Bsh.IsAa();
    m_Topology = CSeq_inst::eTopology_not_set;
    m_Length = 0;

    if (m_Bsh.IsSetInst()) {
        if (m_Bsh.IsSetInst_Topology()) {
            m_Topology = m_Bsh.GetInst_Topology();
        }

        if (m_Bsh.IsSetInst_Length()) {
            m_Length = m_Bsh.GetInst_Length();
        } else {
            m_Length = m_Bsh.GetBioseqLength();
        }

        if (m_Bsh.IsSetInst_Repr()) {
            CBioseq_Handle::TInst_Repr repr = m_Bsh.GetInst_Repr();
            m_IsDelta = (repr == CSeq_inst::eRepr_delta);
            m_IsVirtual = (repr == CSeq_inst::eRepr_virtual);
            m_IsMap = (repr == CSeq_inst::eRepr_map);
        }
        if (m_IsDelta && m_Bsh.IsSetInst_Ext()) {
            const CBioseq_Handle::TInst_Ext& ext = m_Bsh.GetInst_Ext();
            bool hasLoc = false;
            if ( ext.IsDelta() ) {
                ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
                    if ( (*it)->IsLoc() ) {
                        const CSeq_loc& loc = (*it)->GetLoc();
                        if (loc.IsNull()) continue;
                        hasLoc = true;
                    }
                }
            }
            if (! hasLoc) {
                m_IsDeltaLitOnly = true;
            }
        }
    }

    // process Seq-ids
    for (CSeq_id_Handle sid : obsh.GetId()) {
        // first switch to set RefSeq and ThirdParty flags
        switch (sid.Which()) {
            case NCBI_SEQID(Other):
                m_IsRefSeq = true;
                break;
            case NCBI_SEQID(Tpg):
            case NCBI_SEQID(Tpe):
            case NCBI_SEQID(Tpd):
                m_ThirdParty = true;
                break;
            default:
                break;
        }
        // second switch now avoids complicated flag setting logic
        switch (sid.Which()) {
            case NCBI_SEQID(Tpg):
            case NCBI_SEQID(Tpe):
            case NCBI_SEQID(Tpd):
            case NCBI_SEQID(Other):
            case NCBI_SEQID(Genbank):
            case NCBI_SEQID(Embl):
            case NCBI_SEQID(Ddbj):
            {
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CTextseq_id& tsid = *id->GetTextseq_Id ();
                if (tsid.IsSetAccession()) {
                    m_Accession = tsid.GetAccession ();
                    TACCN_CHOICE type = CSeq_id::IdentifyAccession (m_Accession);
                    TACCN_CHOICE div = (TACCN_CHOICE) (type & NCBI_ACCN(division_mask));
                    if ( div == NCBI_ACCN(wgs) )
                    {
                        if( (type & CSeq_id::fAcc_master) != 0 ) {
                            m_WGSMaster = true;
                        }
                    } else if ( div == NCBI_ACCN(tsa) )
                    {
                        if( (type & CSeq_id::fAcc_master) != 0 && m_IsVirtual ) {
                            m_TSAMaster = true;
                        }
                    } else if (type == NCBI_ACCN(refseq_chromosome)) {
                        m_IsNC = true;
                    } else if (type == NCBI_ACCN(refseq_mrna)) {
                        m_IsNM = true;
                    } else if (type == NCBI_ACCN(refseq_mrna_predicted)) {
                        m_IsNM = true;
                    } else if (type == NCBI_ACCN(refseq_ncrna)) {
                        m_IsNR = true;
                    } else if (type == NCBI_ACCN(refseq_contig)) {
                         m_IsNZ = true;
                    } else if (type == NCBI_ACCN(refseq_unique_prot)) {
                        m_IsWP = true;
                    }
                }
                break;
            }
            case NCBI_SEQID(General):
            {
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CDbtag& gen_id = id->GetGeneral ();
                if (! gen_id.IsSkippable ()) {
                    if (gen_id.IsSetTag ()) {
                        const CObject_id& oid = gen_id.GetTag();
                        if (oid.IsStr()) {
                            m_GeneralStr = oid.GetStr();
                        } else if (oid.IsId()) {
                            m_GeneralId = oid.GetId();
                        }
                    }
                }
                break;
            }
            case NCBI_SEQID(Pdb):
            {
                m_IsPDB = true;
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CPDB_seq_id& pdb_id = id->GetPdb ();
                if (pdb_id.IsSetChain_id()) {
                    m_PDBChainID = pdb_id.GetChain_id();
                } else if (pdb_id.IsSetChain()) {
                    m_PDBChain = pdb_id.GetChain();
                }
                break;
            }
            case NCBI_SEQID(Patent):
            {
                m_IsPatent = true;
                CConstRef<CSeq_id> id = sid.GetSeqId();
                const CPatent_seq_id& pat_id = id->GetPatent();
                if (pat_id.IsSetSeqid()) {
                    m_PatentSequence = pat_id.GetSeqid();
                }
                if (pat_id.IsSetCit()) {
                    const CId_pat& cit = pat_id.GetCit();
                    m_PatentCountry = cit.GetCountry();
                    m_PatentNumber = cit.GetSomeNumber();
                }
                break;
            }
            case NCBI_SEQID(Gpipe):
                break;
            default:
                break;
        }
    }

    // process restriction map
    if (m_IsMap) {
        if (bsh.IsSetInst_Ext() && bsh.GetInst_Ext().IsMap()) {
            const CMap_ext& mp = bsh.GetInst_Ext().GetMap();
            if (mp.IsSet()) {
                const CMap_ext::Tdata& ft = mp.Get();
                ITERATE (CMap_ext::Tdata, itr, ft) {
                    const CSeq_feat& feat = **itr;
                    const CSeqFeatData& data = feat.GetData();
                    if (! data.IsRsite()) continue;
                    const CRsite_ref& rsite = data.GetRsite();
                    if (rsite.IsStr()) {
                        m_rEnzyme = rsite.GetStr();
                    }
                }
            }
        }
    }
}

// Destructor
CBioseqIndex::~CBioseqIndex (void)

{
}

// Gap collection (delayed until needed)
void CBioseqIndex::x_InitGaps (void)

{
    try {
        if (m_GapsInitialized) {
            return;
        }

        m_GapsInitialized = true;

        if (! m_IsDelta) {
            return;
        }

        SSeqMapSelector sel;

        size_t resolveCount = 0;

        CWeakRef<CSeqMasterIndex> idx = GetSeqMasterIndex();
        auto idxl = idx.Lock();
        if (idxl) {
            resolveCount = idxl->GetGapDepth();
        }

        sel.SetFlags(CSeqMap::fFindGap)
           .SetResolveCount(resolveCount);

        // explore gaps, pass original target BioseqHandle if using Bioseq sublocation
        for (CSeqMap_CI gap_it(m_OrigBsh, sel); gap_it; ++gap_it) {

            TSeqPos start = gap_it.GetPosition();
            TSeqPos end = gap_it.GetEndPosition();
            TSeqPos length = gap_it.GetLength();

            // attempt to find CSeq_gap info
            const CSeq_gap * pGap = NULL;
            if( gap_it.IsSetData() && gap_it.GetData().IsGap() ) {
                pGap = &gap_it.GetData().GetGap();
            } else {
                CConstRef<CSeq_literal> pSeqLiteral = gap_it.GetRefGapLiteral();
                if( pSeqLiteral && pSeqLiteral->IsSetSeq_data() ) {
                     const CSeq_data & seq_data = pSeqLiteral->GetSeq_data();
                     if( seq_data.IsGap() ) {
                         pGap = &seq_data.GetGap();
                     }
                }
            }

            CFastaOstream::SGapModText gap_mod_text;
            if( pGap ) {
                CFastaOstream::GetGapModText(*pGap, gap_mod_text);
            }
            string type = gap_mod_text.gap_type;
            vector<string>& evidence = gap_mod_text.gap_linkage_evidences;

            bool isUnknownLength = gap_it.IsUnknownLength();

            // feature name depends on what quals we use
            bool isAssemblyGap = ( ! type.empty() || ! evidence.empty() );

            CRef<CGapIndex> sgx(new CGapIndex(start, end, length, type, evidence, isUnknownLength, isAssemblyGap, *this));
            m_GapList.push_back(sgx);
        }
    }
    catch (CException& e) {
        ERR_POST_X(3, Error << "Error in CBioseqIndex::x_InitGaps: " << e.what());
    }
}

static const char* x_OrganelleName (
    TBIOSOURCE_GENOME genome,
    bool has_plasmid,
    bool virus_or_phage,
    bool wgs_suffix
)

{
    const char* result = kEmptyCStr;

    switch (genome) {
        case NCBI_GENOME(chloroplast):
            result = "chloroplast";
            break;
        case NCBI_GENOME(chromoplast):
            result = "chromoplast";
            break;
        case NCBI_GENOME(kinetoplast):
            result = "kinetoplast";
            break;
        case NCBI_GENOME(mitochondrion):
        {
            if (has_plasmid || wgs_suffix) {
                result = "mitochondrial";
            } else {
                result = "mitochondrion";
            }
            break;
        }
        case NCBI_GENOME(plastid):
            result = "plastid";
            break;
        case NCBI_GENOME(macronuclear):
        {
            result = "macronuclear";
            break;
        }
        case NCBI_GENOME(extrachrom):
        {
            if (! wgs_suffix) {
                result = "extrachromosomal";
            }
            break;
        }
        case NCBI_GENOME(plasmid):
        {
            if (! wgs_suffix) {
                result = "plasmid";
            }
            break;
        }
        // transposon and insertion-seq are obsolete
        case NCBI_GENOME(cyanelle):
            result = "cyanelle";
            break;
        case NCBI_GENOME(proviral):
        {
            if (! virus_or_phage) {
                if (has_plasmid || wgs_suffix) {
                    result = "proviral";
                } else {
                    result = "provirus";
                }
            }
            break;
        }
        case NCBI_GENOME(virion):
        {
            if (! virus_or_phage) {
                result = "virus";
            }
            break;
        }
        case NCBI_GENOME(nucleomorph):
        {
            if (! wgs_suffix) {
                result = "nucleomorph";
            }
           break;
        }
        case NCBI_GENOME(apicoplast):
            result = "apicoplast";
            break;
        case NCBI_GENOME(leucoplast):
            result = "leucoplast";
            break;
        case NCBI_GENOME(proplastid):
            result = "proplastid";
            break;
        case NCBI_GENOME(endogenous_virus):
            result = "endogenous virus";
            break;
        case NCBI_GENOME(hydrogenosome):
            result = "hydrogenosome";
            break;
        case NCBI_GENOME(chromosome):
            result = "chromosome";
            break;
        case NCBI_GENOME(chromatophore):
            result = "chromatophore";
            break;
    }

    return result;
}

static bool s_BlankOrNotSpecialTaxname (string taxname)

{
    if (taxname.empty()) {
        return true;
    }

    if (NStr::EqualNocase (taxname, "synthetic construct")) {
        return false;
    }
    if (NStr::EqualNocase (taxname, "artificial sequence")) {
        return false;
    }
    if (NStr::EqualNocase (taxname, "vector")) {
        return false;
    }
    if (NStr::EqualNocase (taxname, "Vector")) {
        return false;
    }

    return true;
}

void CBioseqIndex::x_InitSource (void)

{
    try {
        if (m_SourcesInitialized) {
           return;
        }

        m_SourcesInitialized = true;

        if (! m_DescsInitialized) {
            x_InitDescs();
        }

        if (m_IsAA && m_DescBioSource.NotEmpty() && m_DescBioSource->IsSetTaxname()) {
            m_DescTaxname = m_DescBioSource->GetTaxname();
            if (m_DescTaxname.empty() || s_BlankOrNotSpecialTaxname(m_DescTaxname)) {
                CRef<CFeatureIndex> sfxp = GetFeatureForProduct();
                if (sfxp) {
                    CRef<CFeatureIndex> bsrx = sfxp->GetOverlappingSource();
                    if (bsrx) {
                        CMappedFeat src_feat = bsrx->GetMappedFeat();
                        if (src_feat) {
                            const CBioSource& bsrc = src_feat.GetData().GetBiosrc();
                            m_BioSource.Reset (&bsrc);
                        }
                    }
                }
            }
        }

        if (m_DescBioSource && ! m_BioSource) {
            m_BioSource = m_DescBioSource;
        }

        if (m_BioSource.NotEmpty()) {
            const string *common = 0;

            // get organism name
            if (m_BioSource->IsSetTaxname()) {
                m_Taxname = m_BioSource->GetTaxname();
            }
            if (m_BioSource->IsSetCommon()) {
                common = &m_BioSource->GetCommon();
            }
            if (m_BioSource->IsSetOrgname()) {
                const COrgName& onp = m_BioSource->GetOrgname();
                if (onp.CanGetLineage()) {
                    m_Lineage = onp.GetLineage();
                }
            }
            if (m_BioSource->CanGetOrg()) {
                const COrg_ref& org = m_BioSource->GetOrg();
                m_Taxid = org.GetTaxId();
            }
            if (m_BioSource->IsSetGenome()) {
                m_Genome = m_BioSource->GetGenome();
                m_IsPlasmid = (m_Genome == NCBI_GENOME(plasmid));
                m_IsChromosome = (m_Genome == NCBI_GENOME(chromosome));
            }

            // process SubSource
            FOR_EACH_SUBSOURCE_ON_BIOSOURCE (sbs_itr, *m_BioSource) {
                const CSubSource& sbs = **sbs_itr;
                if (! sbs.IsSetName()) continue;
                const string& str = sbs.GetName();
                SWITCH_ON_SUBSOURCE_CHOICE (sbs) {
                    case NCBI_SUBSOURCE(chromosome):
                        m_Chromosome = str;
                        break;
                    case NCBI_SUBSOURCE(clone):
                        m_Clone = str;
                        m_has_clone = true;
                        break;
                    case NCBI_SUBSOURCE(map):
                        m_Map = str;
                        break;
                    case NCBI_SUBSOURCE(plasmid_name):
                        m_Plasmid = str;
                        break;
                    case NCBI_SUBSOURCE(segment):
                        m_Segment = str;
                        break;
                    case NCBI_SUBSOURCE(linkage_group):
                        m_LinkageGroup = str;
                        break;
                    default:
                        break;
                }
            }

            if (m_BioSource->IsSetOrgname()) {
                const COrgName& onp = m_BioSource->GetOrgname();
                if (onp.IsSetName()) {
                    const COrgName::TName& nam = onp.GetName();
                    if (nam.IsBinomial()) {
                        const CBinomialOrgName& bon = nam.GetBinomial();
                        if (bon.IsSetGenus()) {
                            m_Genus = bon.GetGenus();
                        }
                        if (bon.IsSetSpecies()) {
                            m_Species = bon.GetSpecies();
                        }
                    } else if (nam.IsPartial()) {
                        const CPartialOrgName& pon = nam.GetPartial();
                        if (pon.IsSet()) {
                            const CPartialOrgName::Tdata& tx = pon.Get();
                            ITERATE (CPartialOrgName::Tdata, itr, tx) {
                                const CTaxElement& te = **itr;
                                if (te.IsSetFixed_level()) {
                                    int fl = te.GetFixed_level();
                                    if (fl > 0) {
                                        m_Multispecies = true;
                                    } else if (te.IsSetLevel()) {
                                        const string& lvl = te.GetLevel();
                                        if (! NStr::EqualNocase (lvl, "species")) {
                                            m_Multispecies = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // process OrgMod
            const string *com = 0, *acr = 0, *syn = 0, *ana = 0,
                *gbacr = 0, *gbana = 0, *gbsyn = 0, *met = 0;
            int numcom = 0, numacr = 0, numsyn = 0, numana = 0,
                numgbacr = 0, numgbana = 0, numgbsyn = 0, nummet = 0;

            FOR_EACH_ORGMOD_ON_BIOSOURCE (omd_itr, *m_BioSource) {
                const COrgMod& omd = **omd_itr;
                if (! omd.IsSetSubname()) continue;
                const string& str = omd.GetSubname();
                SWITCH_ON_ORGMOD_CHOICE (omd) {
                    case NCBI_ORGMOD(strain):
                        if (m_Strain.empty()) {
                            m_Strain = str;
                        }
                        break;
                    case NCBI_ORGMOD(substrain):
                        if (m_Substrain.empty()) {
                            m_Substrain = str;
                        }
                        break;
                    case NCBI_ORGMOD(cultivar):
                        if (m_Cultivar.empty()) {
                            m_Cultivar = str;
                        }
                        break;
                    case NCBI_ORGMOD(isolate):
                        if (m_Isolate.empty()) {
                            m_Isolate = str;
                        }
                        break;
                    case NCBI_ORGMOD(breed):
                        if (m_Breed.empty()) {
                            m_Breed = str;
                        }
                    case NCBI_ORGMOD(common):
                        com = &str;
                        numcom++;
                        break;
                    case NCBI_ORGMOD(acronym):
                        acr = &str;
                        numacr++;
                        break;
                    case NCBI_ORGMOD(synonym):
                        syn = &str;
                        numsyn++;
                        break;
                    case NCBI_ORGMOD(anamorph):
                        ana = &str;
                        numana++;
                        break;
                    case NCBI_ORGMOD(gb_acronym):
                        gbacr = &str;
                        numgbacr++;
                        break;
                    case NCBI_ORGMOD(gb_synonym):
                        gbsyn = &str;
                        numgbsyn++;
                        break;
                    case NCBI_ORGMOD(gb_anamorph):
                        gbana = &str;
                        numgbana++;
                        break;
                    case NCBI_ORGMOD(metagenome_source):
                        if (m_MetaGenomeSource.empty()) {
                            m_MetaGenomeSource = str;
                        }
                        met = &str;
                        nummet++;
                        break;
                    default:
                        break;
                }
            }

            if (numacr > 1) {
               acr = NULL;
            }
            if (numana > 1) {
               ana = NULL;
            }
            if (numcom > 1) {
               com = NULL;
            }
            if (numsyn > 1) {
               syn = NULL;
            }
            if (numgbacr > 1) {
               gbacr = NULL;
            }
            if (numgbana > 1) {
               gbana = NULL;
            }
            if (numgbsyn > 1) {
               gbsyn = NULL;
            }
            if( nummet > 1 ) {
                met = NULL;
            }

            if( met != 0 ) {
                m_Common = *met;
            } else if ( syn != 0 ) {
                m_Common = *syn;
            } else if ( acr != 0 ) {
                m_Common = *acr;
            } else if ( ana != 0 ) {
                m_Common = *ana;
                m_UsingAnamorph = true;
            } else if ( com != 0 ) {
                m_Common = *com;
            } else if ( gbsyn != 0 ) {
                m_Common = *gbsyn;
            } else if ( gbacr != 0 ) {
                m_Common = *gbacr;
            } else if ( gbana != 0 ) {
                m_Common = *gbana;
                m_UsingAnamorph = true;
            } else if ( common != 0 ) {
                m_Common = *common;
            }
        }

        bool virus_or_phage = false;
        bool has_plasmid = false;
        bool wgs_suffix = false;

        if (NStr::FindNoCase(m_Taxname, "virus") != NPOS  ||
            NStr::FindNoCase(m_Taxname, "phage") != NPOS) {
            virus_or_phage = true;
        }

        if (! m_Plasmid.empty()) {
            has_plasmid = true;
            /*
            if (NStr::FindNoCase(m_Plasmid, "plasmid") == NPOS  &&
                NStr::FindNoCase(m_Plasmid, "element") == NPOS) {
                pls_pfx = " plasmid ";
            }
            */
        }

        if (m_IsWGS) {
            wgs_suffix = true;
        }

        m_Organelle = x_OrganelleName (m_Genome, has_plasmid, virus_or_phage, wgs_suffix);
    }
    catch (CException& e) {
        ERR_POST_X(4, Error << "Error in CBioseqIndex::x_InitSource: " << e.what());
    }
}

// Descriptor collection (delayed until needed)
void CBioseqIndex::x_InitDescs (void)

{
    try {
        if (m_DescsInitialized) {
           return;
        }

        m_DescsInitialized = true;

        const list <string> *keywords = NULL;

        int num_super_kingdom = 0;
        bool super_kingdoms_different = false;

        // explore descriptors, pass original target BioseqHandle if using Bioseq sublocation
        for (CSeqdesc_CI desc_it(m_OrigBsh); desc_it; ++desc_it) {
            const CSeqdesc& sd = *desc_it;
            CRef<CDescriptorIndex> sdx(new CDescriptorIndex(sd, *this));
            m_SdxList.push_back(sdx);

            switch (sd.Which()) {
                case CSeqdesc::e_Source:
                {
                    if (! m_DescBioSource) {
                        const CBioSource& biosrc = sd.GetSource();
                        m_DescBioSource.Reset (&biosrc);
                        if (m_IsNA && ! m_BioSource) {
                            m_BioSource = m_DescBioSource;
                        }
                    }
                    if (m_IsWP) {
                        const CBioSource &bsrc = sd.GetSource();
                        if (! bsrc.IsSetOrgname()) break;
                        const COrgName& onp = bsrc.GetOrgname();
                        if (! onp.IsSetName()) break;
                        const COrgName::TName& nam = onp.GetName();
                        if (! nam.IsPartial()) break;
                        const CPartialOrgName& pon = nam.GetPartial();
                        if (! pon.IsSet()) break;
                        const CPartialOrgName::Tdata& tx = pon.Get();
                        ITERATE (CPartialOrgName::Tdata, itr, tx) {
                            const CTaxElement& te = **itr;
                            if (! te.IsSetFixed_level()) continue;
                            if (te.GetFixed_level() != 0) continue;
                            if (! te.IsSetLevel()) continue;
                            const string& lvl = te.GetLevel();
                            if (! NStr::EqualNocase (lvl, "superkingdom")) continue;
                            num_super_kingdom++;
                            if (m_FirstSuperKingdom.empty() && te.IsSetName()) {
                                m_FirstSuperKingdom = te.GetName();
                            } else if (te.IsSetName() && ! NStr::EqualNocase (m_FirstSuperKingdom, te.GetName())) {
                                if (m_SecondSuperKingdom.empty()) {
                                    super_kingdoms_different = true;
                                    m_SecondSuperKingdom = te.GetName();
                                }
                            }
                            if (num_super_kingdom > 1 && super_kingdoms_different) {
                                m_IsCrossKingdom = true;
                            }
                        }
                    }
                    break;
                }
                case CSeqdesc::e_Molinfo:
                {
                    if (! m_MolInfo) {
                        const CMolInfo& molinf = sd.GetMolinfo();
                        m_MolInfo.Reset (&molinf);
                        m_Biomol = molinf.GetBiomol();
                        m_Tech = molinf.GetTech();
                        m_Completeness = molinf.GetCompleteness();

                        switch (m_Tech) {
                            case NCBI_TECH(htgs_0):
                            case NCBI_TECH(htgs_1):
                            case NCBI_TECH(htgs_2):
                                m_HTGSUnfinished = true;
                                // manufacture all titles for unfinished HTG sequences
                                // m_Reconstruct = true;
                                m_Title.clear();
                                // fall through
                            case NCBI_TECH(htgs_3):
                                m_HTGTech = true;
                                m_UseBiosrc = true;
                                break;
                            case NCBI_TECH(est):
                            case NCBI_TECH(sts):
                            case NCBI_TECH(survey):
                                m_IsEST_STS_GSS = true;
                                m_UseBiosrc = true;
                                break;
                            case NCBI_TECH(wgs):
                                m_IsWGS = true;
                                m_UseBiosrc = true;
                                break;
                            case NCBI_TECH(tsa):
                                m_IsTSA = true;
                                m_UseBiosrc = true;
                                 if (m_IsVirtual) {
                                    m_TSAMaster = true;
                                }
                               break;
                            case NCBI_TECH(targeted):
                                m_IsTLS = true;
                                m_UseBiosrc = true;
                                if (m_IsVirtual) {
                                    m_TLSMaster = true;
                                }
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                }
                case CSeqdesc::e_Title:
                {
                    if (m_Title.empty()) {
                        // // for non-PDB proteins, title must be packaged on Bioseq
                        if (m_IsNA  ||  m_IsPDB  ||  desc_it.GetSeq_entry_Handle().IsSeq()) {
                            m_Title = sd.GetTitle();
                        }
                    }
                    break;
                }
                case CSeqdesc::e_User:
                {
                    const CUser_object& usr = sd.GetUser();
                    if (usr.IsSetType()) {
                        const CObject_id& oi = usr.GetType();
                        if (oi.IsStr()) {
                            const string& type = oi.GetStr();
                            if (NStr::EqualNocase(type, "FeatureFetchPolicy")) {
                                FOR_EACH_USERFIELD_ON_USEROBJECT (uitr, usr) {
                                    const CUser_field& fld = **uitr;
                                    if (fld.IsSetLabel() && fld.GetLabel().IsStr()) {
                                        const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                                        if (! NStr::EqualNocase(label_str, "Policy")) continue;
                                        if (fld.IsSetData() && fld.GetData().IsStr()) {
                                            const string& str = fld.GetData().GetStr();
                                            if (NStr::EqualNocase(str, "OnlyNearFeatures")) {
                                                m_ForceOnlyNearFeats = true;
                                            }
                                        }
                                    }
                                }
                            } else if (NStr::EqualNocase(type, "Unverified")) {
                                m_IsUnverified = true;
                                if (usr.IsUnverifiedOrganism()) {
                                    m_IsUnverifiedOrganism = true;
                                }
                                if (usr.IsUnverifiedMisassembled()) {
                                    m_IsUnverifiedMisassembled = true;
                                }
                                if (usr.IsUnverifiedContaminant()) {
                                    m_IsUnverifiedContaminant = true;
                                }
                                if (usr.IsUnverifiedFeature()) {
                                    m_IsUnverifiedFeature = true;
                                }
                            } else if (NStr::EqualNocase(type, "AutodefOptions")) {
                                FOR_EACH_USERFIELD_ON_USEROBJECT (uitr, usr) {
                                    const CUser_field& fld = **uitr;
                                    if (! FIELD_IS_SET_AND_IS(fld, Label, Str)) continue;
                                    const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                                    if (! NStr::EqualNocase(label_str, "Targeted Locus Name")) continue;
                                    if (fld.IsSetData() && fld.GetData().IsStr()) {
                                        m_TargetedLocus = fld.GetData().GetStr();
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                case CSeqdesc::e_Comment:
                {
                    m_Comment = sd.GetComment();
                    if (NStr::Find (m_Comment, "[CAUTION] Could be the product of a pseudogene") != string::npos) {
                        m_IsPseudogene = true;
                    }
                    break;
                }
                case CSeqdesc::e_Genbank:
                {
                    const CGB_block& gbk = desc_it->GetGenbank();
                    if (gbk.IsSetKeywords()) {
                        keywords = &gbk.GetKeywords();
                    }
                    break;
                }
                case CSeqdesc::e_Embl:
                {
                    const CEMBL_block& ebk = desc_it->GetEmbl();
                    if (ebk.IsSetKeywords()) {
                        keywords = &ebk.GetKeywords();
                    }
                    break;
                }
                case CSeqdesc::e_Pdb:
                {
                    if (m_PDBCompound.empty()) {
                        _ASSERT(m_IsPDB);
                        const CPDB_block& pbk = desc_it->GetPdb();
                        FOR_EACH_COMPOUND_ON_PDBBLOCK (cp_itr, pbk) {
                            if (m_PDBCompound.empty()) {
                                m_PDBCompound = *cp_itr;
                                break;
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }

        if (keywords != NULL) {
            FOR_EACH_STRING_IN_LIST (kw_itr, *keywords) {
                const string& clause = *kw_itr;
                list<string> kywds;
                NStr::Split( clause, ";", kywds, NStr::fSplit_Tokenize );
                FOR_EACH_STRING_IN_LIST ( k_itr, kywds ) {
                    const string& str = *k_itr;
                    if (NStr::EqualNocase (str, "UNORDERED")) {
                        m_Unordered = true;
                    }
                    if ((! m_HTGTech) && (! m_ThirdParty)) continue;
                    if (NStr::EqualNocase (str, "HTGS_DRAFT")) {
                        m_HTGSDraft = true;
                    } else if (NStr::EqualNocase (str, "HTGS_CANCELLED")) {
                        m_HTGSCancelled = true;
                    } else if (NStr::EqualNocase (str, "HTGS_POOLED_MULTICLONE")) {
                        m_HTGSPooled = true;
                    } else if (NStr::EqualNocase (str, "TPA:experimental")) {
                        m_TPAExp = true;
                    } else if (NStr::EqualNocase (str, "TPA:inferential")) {
                        m_TPAInf = true;
                    } else if (NStr::EqualNocase (str, "TPA:reassembly")) {
                        m_TPAReasm = true;
                    } else if (NStr::EqualNocase (str, "TPA:assembly")) {
                        m_TPAReasm = true;
                    }
                }
            }
        }
    }
    catch (CException& e) {
        ERR_POST_X(5, Error << "Error in CBioseqIndex::x_InitDescs: " << e.what());
    }
}

void CBioseqIndex::x_DefaultSelector(SAnnotSelector& sel, CSeqEntryIndex::EPolicy policy, CSeqEntryIndex::TFlags flags, bool onlyNear, CScope& scope)

{
    bool snpOK = false;
    bool cddOK = false;

    if (policy == CSeqEntryIndex::eExhaustive) {

        // experimental policy forces collection of features from all sequence levels
        sel.SetResolveAll();
        sel.SetResolveDepth(kMax_Int);
        // ignores RefSeq/INSD barrier, overrides far fetch policy user object
        // for now, always excludes external annots, ignores custom enable bits

    } else if (policy == CSeqEntryIndex::eInternal || onlyNear) {

        // do not fetch features from underlying sequence component records
        sel.SetResolveDepth(0);
        sel.SetExcludeExternal(true);
        // always excludes external annots, ignores custom enable bits

    } else if (policy == CSeqEntryIndex::eAdaptive) {

        sel.SetResolveAll();
        // normal situation uses adaptive depth for feature collection,
        // includes barrier between RefSeq and INSD accession types
        sel.SetAdaptiveDepth(true);

        // conditionally allows external annots, based on custom enable bits
        if ((flags & CSeqEntryIndex::fShowSNPFeats) != 0) {
            snpOK = true;
        }
        if ((flags & CSeqEntryIndex::fShowCDDFeats) != 0) {
            cddOK = true;
        }

    } else if (policy == CSeqEntryIndex::eExternal) {

        // same as eAdaptive
        sel.SetResolveAll();
        sel.SetAdaptiveDepth(true);

        // but always allows external annots without need for custom enable bits
        snpOK = true;
        cddOK = true;

    } else if (policy == CSeqEntryIndex::eFtp) {

        // for public ftp releases
        if (m_IsRefSeq) {
            sel.SetResolveAll();
            sel.SetAdaptiveDepth(true);
        } else if (m_IsDeltaLitOnly) {
            sel.SetResolveDepth(0);
            sel.SetExcludeExternal(true);
        } else {
            sel.SetResolveDepth(0);
            sel.SetExcludeExternal(true);
        }

    } else if (policy == CSeqEntryIndex::eWeb) {

        // for public web pages
        if (m_IsRefSeq) {
            sel.SetResolveAll();
            sel.SetAdaptiveDepth(true);
        } else if (m_IsDeltaLitOnly) {
            sel.SetResolveAll();
            sel.SetAdaptiveDepth(true);
        } else {
            sel.SetResolveAll();
            sel.SetAdaptiveDepth(true);
        }

        // ID-6366 additional tests for -policy web to prevent gridlock caused by loading huge numbers of SNPs
        if (GetLength() <= 1000000) {
            // conditionally allows external annots, based on custom enable bits
            if ((flags & CSeqEntryIndex::fShowSNPFeats) != 0) {
                snpOK = true;
            }
            if ((flags & CSeqEntryIndex::fShowCDDFeats) != 0) {
                cddOK = true;
            }
        }
    }

    // fHideSNPFeats and fHideCDDFeats flags override any earlier settings
    if ((flags & CSeqEntryIndex::fHideSNPFeats) != 0) {
        snpOK = false;
    }
    if ((flags & CSeqEntryIndex::fHideCDDFeats) != 0) {
        cddOK = false;
    }

    // configure remote annot settings in selector
    if ( snpOK ) {

        CWeakRef<CSeqMasterIndex> idx = GetSeqMasterIndex();
        auto idxl = idx.Lock();
        if (idxl) {
            FAddSnpFunc* func = idxl->GetSnpFunc();
            if (func) {
                // under PubSeq Gateway, need to get exact accession for SNP retrieval
                CBioseq_Handle bsh = GetBioseqHandle();
                string na_acc;
                (*func) (bsh, na_acc);
                if (na_acc.length() > 0) {
                    sel.IncludeNamedAnnotAccession(na_acc);
                }
            } else {
                // otherwise just give SNP name
                sel.IncludeNamedAnnotAccession("SNP");
            }
        }

    } else {
        sel.ExcludeNamedAnnotAccession("SNP");
    }

    if ( cddOK ) {
        sel.IncludeNamedAnnotAccession("CDD");
    } else {
        sel.ExcludeNamedAnnotAccession("CDD");
    }

    CWeakRef<CSeqMasterIndex> idx = GetSeqMasterIndex();
    auto idxl = idx.Lock();
    if (idxl) {
        int featDepth = idxl->GetFeatDepth();
        if (featDepth > 0) {
            sel.SetResolveDepth(featDepth);
        }
    }

    // bit flags exclude specific features
    // source features are collected elsewhere
    sel.ExcludeFeatType(CSeqFeatData::e_Biosrc);
    // pub features are used in the REFERENCES section
    sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_pub);
    // some feature types are always excluded (deprecated?)
    // sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_non_std_residue)
    sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_rsite)
       .ExcludeFeatSubtype(CSeqFeatData::eSubtype_seq);
    // exclude other types based on user flags
    if ((flags & CSeqEntryIndex::fHideImpFeats) != 0) {
        sel.ExcludeFeatType(CSeqFeatData::e_Imp);
    }
    if ((flags & CSeqEntryIndex::fHideSTSFeats) != 0) {
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_STS);
    }
    if ((flags & CSeqEntryIndex::fHideExonFeats) != 0) {
        sel.ExcludeNamedAnnots("Exon");
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_exon);
    }
    if ((flags & CSeqEntryIndex::fHideIntronFeats) != 0) {
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_intron);
    }
    if ((flags & CSeqEntryIndex::fHideMiscFeats) != 0) {
        sel.ExcludeFeatType(CSeqFeatData::e_Site);
        sel.ExcludeFeatType(CSeqFeatData::e_Bond);
        sel.ExcludeFeatType(CSeqFeatData::e_Region);
        sel.ExcludeFeatType(CSeqFeatData::e_Comment);
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_misc_feature);
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_preprotein);
    }
    if ((flags & CSeqEntryIndex::fHideGapFeats) != 0) {
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gap);
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_assembly_gap);
    }

    // additional common settings
    sel.SetFeatComparator(new feature::CFeatComparatorByLabel);

    // limit exploration of far deltas with no features to avoid timeout
    sel.SetMaxSearchSegments(500);
    sel.SetMaxSearchSegmentsAction(SAnnotSelector::eMaxSearchSegmentsSilent);
    sel.SetMaxSearchTime(25);

    // request exception to capture fetch failure
    sel.SetFailUnresolved();
}

// Feature collection common implementation method (delayed until needed)
void CBioseqIndex::x_InitFeats (CSeq_loc* slpp)

{
    try {
        // Do not bail on m_FeatsInitialized flag

        if (! m_DescsInitialized) {
            // initialize descriptors first to get m_ForceOnlyNearFeats flag
            x_InitDescs();
        }

        m_FeatsInitialized = true;

        SAnnotSelector sel;

        x_DefaultSelector(sel, m_Policy, m_Flags, m_ForceOnlyNearFeats, *m_Scope);

        bool onlyGeneRNACDS = false;
        if ((m_Flags & CSeqEntryIndex::fGeneRNACDSOnly) != 0) {
            onlyGeneRNACDS = true;
        }

        // variables for setting m_BestProteinFeature
        TSeqPos longest = 0;
        CProt_ref::EProcessed bestprocessed = CProt_ref::eProcessed_not_set;
        CProt_ref::EProcessed processed;

        CWeakRef<CSeqMasterIndex> idx = GetSeqMasterIndex();
        auto idxl = idx.Lock();
        if (idxl) {
            /*
            if (! idxl->IsSmallGenomeSet()) {
                // limit feature collection to immediate Bioseq-set parent
                CRef<CSeqsetIndex> prnt = GetParent();
                if (prnt) {
                    CBioseq_set_Handle bssh = prnt->GetSeqsetHandle();
                    if (bssh) {
                        CSeq_entry_Handle pseh = bssh.GetParentEntry();
                        if (pseh) {
                            sel.SetLimitSeqEntry(pseh);
                        }
                    }
                }
            }
            */

            CRef<feature::CFeatTree> ft = idxl->GetFeatTree();

            // start collection over on each segment
            m_SfxList.clear();

            // iterate features on Bioseq or sublocation
            CFeat_CI feat_it;
            CRef<CSeq_loc_Mapper> slice_mapper;
            if (slpp == 0) {
                feat_it = CFeat_CI(m_Bsh, sel);
            } else {
                SAnnotSelector sel_cpy = sel;
                sel_cpy.SetIgnoreStrand();
                /*
                if (selp->IsSetStrand() && selp->GetStrand() == eNa_strand_minus) {
                    sel_cpy.SetSortOrder(SAnnotSelector::eSortOrder_Reverse);
                }
                */
                CConstRef<CSeq_id> bsid = m_Bsh.GetSeqId();
                if (bsid) {
                    SetDiagFilter(eDiagFilter_All, "!(1305.28,31)");
                    CSeq_id seq_id;
                    seq_id.Assign( *bsid );
                    CSeq_loc old_loc;
                    old_loc.SetInt().SetId( seq_id );
                    old_loc.SetInt().SetFrom( 0 );
                    old_loc.SetInt().SetTo( m_Length - 1 );
                    slice_mapper = new CSeq_loc_Mapper( *slpp, old_loc, m_Scope );
                    slice_mapper->SetFuzzOption( CSeq_loc_Mapper::fFuzzOption_RemoveLimTlOrTr );
                    slice_mapper->TruncateNonmappingRanges();
                    SetDiagFilter(eDiagFilter_All, "");
                }
                feat_it = CFeat_CI(*m_Scope, *slpp, sel_cpy);
            }

            // iterate features on Bioseq
            for (; feat_it; ++feat_it) {
                const CMappedFeat mf = *feat_it;

                const CSeqFeatData& data = mf.GetData();
                CSeqFeatData::E_Choice typ = data.Which();
                if (onlyGeneRNACDS) {
                    if (typ != CSeqFeatData::e_Gene &&
                        typ != CSeqFeatData::e_Rna &&
                        typ != CSeqFeatData::e_Cdregion) {
                        continue;
                    }
                }

                CSeq_feat_Handle hdl = mf.GetSeq_feat_Handle();

                CConstRef<CSeq_loc> feat_loc(&mf.GetLocation());
                if (slpp) {
                    feat_loc.Reset( slice_mapper->Map( mf.GetLocation() ) );
                }

                CRef<CFeatureIndex> sfx(new CFeatureIndex(hdl, mf, feat_loc, *this));
                m_SfxList.push_back(sfx);

                ft->AddFeature(mf);

                // CFeatureIndex from CMappedFeat for use with GetBestGene
                m_FeatIndexMap[mf] = sfx;

                // set specific flags for various feature types
                CSeqFeatData::E_Choice type = sfx->GetType();
                CSeqFeatData::ESubtype subtype = sfx->GetSubtype();

                if (type == CSeqFeatData::e_Biosrc) {
                    m_HasSource = true;
                    if (! m_BioSource) {
                        if (! mf.IsSetData ()) continue;
                        const CSeqFeatData& sfdata = mf.GetData();
                        const CBioSource& biosrc = sfdata.GetBiosrc();
                        m_BioSource.Reset (&biosrc);
                    }
                    continue;
                }

                if (type == CSeqFeatData::e_Gene) {
                    m_HasGene = true;
                    if (m_HasMultiIntervalGenes) {
                        continue;
                    }
                    const CSeq_loc& loc = mf.GetLocation ();
                    switch (loc.Which()) {
                        case CSeq_loc::e_Packed_int:
                        case CSeq_loc::e_Packed_pnt:
                        case CSeq_loc::e_Mix:
                        case CSeq_loc::e_Equiv:
                            m_HasMultiIntervalGenes = true;
                            break;
                        default:
                            break;
                    }
                    continue;
                }

                if (subtype == CSeqFeatData::eSubtype_operon) {
                    idxl->SetHasOperon(true);
                    continue;
                }

                if (type == CSeqFeatData::e_Prot && IsAA()) {
                    if (! mf.IsSetData ()) continue;
                    const CSeqFeatData& sfdata = mf.GetData();
                    const CProt_ref& prp = sfdata.GetProt();
                    processed = CProt_ref::eProcessed_not_set;
                    if (prp.IsSetProcessed()) {
                        processed = prp.GetProcessed();
                    }
                    const CSeq_loc& loc = mf.GetLocation ();
                    TSeqPos prot_length = sequence::GetLength(loc, m_Scope);
                    if (prot_length > longest) {
                        m_BestProtFeatInitialized = true;
                        m_BestProteinFeature = sfx;
                        longest = prot_length;
                        bestprocessed = processed;
                    } else if (prot_length == longest) {
                        // unprocessed 0 > preprotein 1 > mat peptide 2
                        if (processed < bestprocessed) {
                            m_BestProtFeatInitialized = true;
                            m_BestProteinFeature = sfx;
                            longest = prot_length;
                            bestprocessed = processed;
                        }
                    }
                    continue;
                }

                if (type == CSeqFeatData::e_Cdregion && IsNA()) {
                } else if (type == CSeqFeatData::e_Rna && IsNA()) {
                } else if (type == CSeqFeatData::e_Prot && IsAA()) {
                } else {
                    continue;
                }

                // index feature for (local) product Bioseq (CDS -> protein, mRNA -> cDNA, or Prot -> peptide)
                CSeq_id_Handle idh = mf.GetProductId();
                if (idh) {
                    string str = idh.AsString();
                    CRef<CBioseqIndex> bsxp = idxl->GetBioseqIndex(str);
                    if (bsxp) {
                        bsxp->m_FeatForProdInitialized = true;
                        bsxp->m_FeatureForProduct = sfx;
                    }
                }
            }
        }
    }
    catch (CException& e) {
        m_FetchFailure = true;
        ERR_POST_X(6, Error << "Error in CBioseqIndex::x_InitFeats: " << e.what());
    }
}

// Feature collection methods (delayed until needed)
void CBioseqIndex::x_InitFeats (void)

{
    x_InitFeats(0);
}

void CBioseqIndex::x_InitFeats (CSeq_loc& slp)

{
    x_InitFeats(&slp);
}

// GetFeatureForProduct allows hypothetical protein defline generator to obtain gene locus tag
CRef<CFeatureIndex> CBioseqIndex::GetFeatureForProduct (void)

{
    if (! m_FeatForProdInitialized) {
        if (m_Bsh) {
            CFeat_CI fi(m_Bsh,
                        SAnnotSelector(CSeqFeatData::e_Cdregion)
                        .SetByProduct().SetLimitTSE(m_Bsh.GetTSE_Handle()));
            if (! fi) {
                fi = CFeat_CI(m_Bsh,
                              SAnnotSelector(CSeqFeatData::e_Rna)
                              .SetByProduct().SetLimitTSE(m_Bsh.GetTSE_Handle()));
            }
            if (! fi) {
                fi = CFeat_CI(m_Bsh,
                              SAnnotSelector(CSeqFeatData::e_Prot)
                              .SetByProduct().SetLimitTSE(m_Bsh.GetTSE_Handle()));
            }
            if (fi) {
                CMappedFeat mf = *fi;
                CSeq_id_Handle idh = mf.GetLocationId();
                CBioseq_Handle nbsh = m_Scope->GetBioseqHandle(idh);
                if (nbsh) {
                    CWeakRef<CSeqMasterIndex> idx = GetSeqMasterIndex();
                    auto idxl = idx.Lock();
                    if (idxl) {
                        CRef<CBioseqIndex> bsxn = idxl->GetBioseqIndex(nbsh);
                        if (bsxn) {
                            if (! bsxn->m_FeatsInitialized) {
                                bsxn->x_InitFeats();
                            }
                        }
                    }
                }
            }
        }
    }

    return m_FeatureForProduct;
}

// Get Bioseq index containing feature with product pointing to this Bioseq
CWeakRef<CBioseqIndex> CBioseqIndex::GetBioseqForProduct (void)

{
    CRef<CFeatureIndex> sfxp = GetFeatureForProduct();
    if (sfxp) {
        return sfxp->GetBioseqIndex();
    }

    return CWeakRef<CBioseqIndex> ();
}

// GetBestProteinFeature indexes longest protein feature on protein Bioseq
CRef<CFeatureIndex> CBioseqIndex::GetBestProteinFeature (void)

{
    if (! m_BestProtFeatInitialized) {
        if (! m_FeatsInitialized) {
            x_InitFeats();
        }
    }

    return m_BestProteinFeature;
}

// Common descriptor field getters
const string& CBioseqIndex::GetTitle (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_Title;
}

CConstRef<CMolInfo> CBioseqIndex::GetMolInfo (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_MolInfo;
}

CMolInfo::TBiomol CBioseqIndex::GetBiomol (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_Biomol;
}

CMolInfo::TTech CBioseqIndex::GetTech (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_Tech;
}

CMolInfo::TCompleteness CBioseqIndex::GetCompleteness (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_Completeness;
}

bool CBioseqIndex::IsHTGTech (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_HTGTech;
}

bool CBioseqIndex::IsHTGSUnfinished (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_HTGSUnfinished;
}

bool CBioseqIndex::IsTLS (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsTLS;
}

bool CBioseqIndex::IsTSA (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsTSA;
}

bool CBioseqIndex::IsWGS (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsWGS;
}

bool CBioseqIndex::IsEST_STS_GSS (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsEST_STS_GSS;
}

bool CBioseqIndex::IsUseBiosrc (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_UseBiosrc;
}

CConstRef<CBioSource> CBioseqIndex::GetBioSource (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_BioSource;
}

const string& CBioseqIndex::GetTaxname (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Taxname;
}

const string& CBioseqIndex::GetDescTaxname (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_DescTaxname;
}

const string& CBioseqIndex::GetCommon (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Common;
}

const string& CBioseqIndex::GetLineage (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Lineage;
}

TTaxId CBioseqIndex::GetTaxid (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Taxid;
}

bool CBioseqIndex::IsUsingAnamorph (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_UsingAnamorph;
}

CTempString CBioseqIndex::GetGenus (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Genus;
}

CTempString CBioseqIndex::GetSpecies (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Species;
}

bool CBioseqIndex::IsMultispecies (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Multispecies;
}

CBioSource::TGenome CBioseqIndex::GetGenome (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Genome;
}

bool CBioseqIndex::IsPlasmid (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_IsPlasmid;
}

bool CBioseqIndex::IsChromosome (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_IsChromosome;
}

const string& CBioseqIndex::GetOrganelle (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Organelle;
}

string CBioseqIndex::GetFirstSuperKingdom (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_FirstSuperKingdom;
}

string CBioseqIndex::GetSecondSuperKingdom (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_SecondSuperKingdom;
}

bool CBioseqIndex::IsCrossKingdom (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_IsCrossKingdom;
}

CTempString CBioseqIndex::GetChromosome (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Chromosome;
}

CTempString CBioseqIndex::GetLinkageGroup (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_LinkageGroup;
}

CTempString CBioseqIndex::GetClone (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Clone;
}

bool CBioseqIndex::HasClone (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_has_clone;
}

CTempString CBioseqIndex::GetMap (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Map;
}

CTempString CBioseqIndex::GetPlasmid (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Plasmid;
}

CTempString CBioseqIndex::GetSegment (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Segment;
}

CTempString CBioseqIndex::GetBreed (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Breed;
}

CTempString CBioseqIndex::GetCultivar (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Cultivar;
}

CTempString CBioseqIndex::GetIsolate (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Isolate;
}

CTempString CBioseqIndex::GetStrain (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Strain;
}

CTempString CBioseqIndex::GetSubstrain (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_Substrain;
}

CTempString CBioseqIndex::GetMetaGenomeSource (void)

{
    if (! m_SourcesInitialized) {
        x_InitSource();
    }

    return m_MetaGenomeSource;
}

bool CBioseqIndex::IsHTGSCancelled (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_HTGSCancelled;
}

bool CBioseqIndex::IsHTGSDraft (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_HTGSDraft;
}

bool CBioseqIndex::IsHTGSPooled (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_HTGSPooled;
}

bool CBioseqIndex::IsTPAExp (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_TPAExp;
}

bool CBioseqIndex::IsTPAInf (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_TPAInf;
}

bool CBioseqIndex::IsTPAReasm (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_TPAReasm;
}

bool CBioseqIndex::IsUnordered (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_Unordered;
}

CTempString CBioseqIndex::GetPDBCompound (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_PDBCompound;
}

bool CBioseqIndex::IsForceOnlyNearFeats (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_ForceOnlyNearFeats;
}

bool CBioseqIndex::IsUnverified (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsUnverified;
}

bool CBioseqIndex::IsUnverifiedFeature (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsUnverifiedFeature;
}

bool CBioseqIndex::IsUnverifiedOrganism (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsUnverifiedOrganism;
}

bool CBioseqIndex::IsUnverifiedMisassembled (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsUnverifiedMisassembled;
}

bool CBioseqIndex::IsUnverifiedContaminant (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsUnverifiedContaminant;
}

CTempString CBioseqIndex::GetTargetedLocus (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_TargetedLocus;
}

const string& CBioseqIndex::GetComment (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_Comment;
}

bool CBioseqIndex::IsPseudogene (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_IsPseudogene;
}

bool CBioseqIndex::HasOperon (void)

{
    if (! m_FeatsInitialized) {
        x_InitFeats();
    }

    CWeakRef<CSeqMasterIndex> idx = GetSeqMasterIndex();
    auto idxl = idx.Lock();
    if (idxl) {
        return idxl->HasOperon();
    }

    return false;
}

bool CBioseqIndex::HasGene (void)

{
    if (! m_FeatsInitialized) {
        x_InitFeats();
    }

    return m_HasGene;
}

bool CBioseqIndex::HasMultiIntervalGenes (void)

{
    if (! m_FeatsInitialized) {
        x_InitFeats();
    }

    return m_HasMultiIntervalGenes;
}

bool CBioseqIndex::HasSource (void)

{
    if (! m_FeatsInitialized) {
        x_InitFeats();
    }

    return m_HasSource;
}

string CBioseqIndex::GetrEnzyme (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_rEnzyme;
}

CRef<CFeatureIndex> CBioseqIndex::GetFeatIndex (const CMappedFeat& mf)

{
    CRef<CFeatureIndex> sfx;

    TFeatIndexMap::iterator it = m_FeatIndexMap.find(mf);
    if (it != m_FeatIndexMap.end()) {
        sfx = it->second;
    }

    return sfx;
}

void CBioseqIndex::GetSequence (int from, int to, string& buffer)

{
    try {
        if (! m_SeqVec) {
            m_SeqVec = new CSeqVector(m_Bsh);
            if (m_SeqVec) {
                if (IsAA()) {
                    m_SeqVec->SetCoding(CSeq_data::e_Ncbieaa);
                } else {
                    m_SeqVec->SetCoding(CBioseq_Handle::eCoding_Iupac);
                }
            }
        }

        if (m_SeqVec) {
            CSeqVector& vec = *m_SeqVec;
            if (from < 0) {
                from = 0;
            }
            if (to < 0 || to >= (int) vec.size()) {
                to = vec.size();
            }
            if (vec.CanGetRange(from, to)) {
                vec.GetSeqData(from, to, buffer);
            } else {
                m_FetchFailure = true;
            }
        }
    }
    catch (CException& e) {
        ERR_POST_X(7, Error << "Error in CBioseqIndex::GetSequence: " << e.what());
    }
}

string CBioseqIndex::GetSequence (int from, int to)

{
    string buffer;

    GetSequence(from, to, buffer);

    return buffer;
}

void CBioseqIndex::GetSequence (string& buffer)

{
    GetSequence(0, -1, buffer);
}

string CBioseqIndex::GetSequence (void)

{
    string buffer;

    GetSequence(0, -1, buffer);

    return buffer;
}

const vector<CRef<CGapIndex>>& CBioseqIndex::GetGapIndices(void)

{
    if (! m_GapsInitialized) {
        x_InitGaps();
    }

    return m_GapList;
}

const vector<CRef<CDescriptorIndex>>& CBioseqIndex::GetDescriptorIndices(void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_SdxList;
}

const vector<CRef<CFeatureIndex>>& CBioseqIndex::GetFeatureIndices(void)

{
    if (! m_FeatsInitialized) {
        x_InitFeats();
    }

    return m_SfxList;
}


// CGapIndex

// Constructor
CGapIndex::CGapIndex (TSeqPos start,
                      TSeqPos end,
                      TSeqPos length,
                      const string& type,
                      const vector<string>& evidence,
                      bool isUnknownLength,
                      bool isAssemblyGap,
                      CBioseqIndex& bsx)
    : m_Bsx(&bsx),
      m_Start(start),
      m_End(end),
      m_Length(length),
      m_GapType(type),
      m_GapEvidence(evidence),
      m_IsUnknownLength(isUnknownLength),
      m_IsAssemblyGap(isAssemblyGap)
{
}


// CDescriptorIndex

// Constructor
CDescriptorIndex::CDescriptorIndex (const CSeqdesc& sd,
                                    CBioseqIndex& bsx)
    : m_Sd(sd),
      m_Bsx(&bsx)
{
    m_Type = m_Sd.Which();
}


// CFeatureIndex

// Constructor
CFeatureIndex::CFeatureIndex (CSeq_feat_Handle sfh,
                              const CMappedFeat mf,
                              CConstRef<CSeq_loc> feat_loc,
                              CBioseqIndex& bsx)
    : m_Sfh(sfh),
      m_Mf(mf),
      m_Bsx(&bsx)
{
    const CSeqFeatData& data = m_Mf.GetData();
    m_Type = data.Which();
    m_Subtype = data.GetSubtype();
    m_Fl = feat_loc;
    m_Start = m_Fl->GetStart(eExtreme_Positional);
    m_End = m_Fl->GetStop(eExtreme_Positional);
}

// Find CFeatureIndex object for best gene using internal CFeatTree
CRef<CFeatureIndex> CFeatureIndex::GetBestGene (void)

{
    try {
        CMappedFeat best;
        CWeakRef<CBioseqIndex> bsx = GetBioseqIndex();
        auto bsxl = bsx.Lock();
        if (bsxl) {
            CWeakRef<CSeqMasterIndex> idx = bsxl->GetSeqMasterIndex();
            auto idxl = idx.Lock();
            if (idxl) {
                 best = feature::GetBestGeneForFeat(m_Mf, idxl->GetFeatTree(), 0,
                                                   /* feature::CFeatTree::eBestGene_AllowOverlapped */
                                                   feature::CFeatTree::eBestGene_TreeOnly);
            }
            if (best) {
                return bsxl->GetFeatIndex(best);
            }
        }
    } catch (CException& e) {
        ERR_POST_X(8, Error << "Error in CFeatureIndex::GetBestGene: " << e.what());
    }
    return CRef<CFeatureIndex> ();
}


// Find CFeatureIndex object for best parent using internal CFeatTree
CRef<CFeatureIndex> CFeatureIndex::GetBestParent (void)

{
    try {
        CMappedFeat best;
        CWeakRef<CBioseqIndex> bsx = GetBioseqIndex();
        auto bsxl = bsx.Lock();
        if (bsxl) {
            CWeakRef<CSeqMasterIndex> idx = bsxl->GetSeqMasterIndex();
            auto idxl = idx.Lock();
            if (idxl) {
                 static const CSeqFeatData::ESubtype sm_SpecialVDJTypes[] = {
                     CSeqFeatData::eSubtype_C_region,
                     CSeqFeatData::eSubtype_V_segment,
                     CSeqFeatData::eSubtype_D_segment,
                     CSeqFeatData::eSubtype_J_segment,
                     CSeqFeatData::eSubtype_bad
                 };
                 for ( const CSeqFeatData::ESubtype* type_ptr = sm_SpecialVDJTypes;
                     *type_ptr != CSeqFeatData::eSubtype_bad; ++type_ptr ) {
                     best = feature::GetBestParentForFeat(m_Mf, *type_ptr, idxl->GetFeatTree(), 0);
                     if (best) {
                         return bsxl->GetFeatIndex(best);
                     }
                 }
             }
        }
    } catch (CException& e) {
        ERR_POST_X(8, Error << "Error in CFeatureIndex::GetBestParent: " << e.what());
    }
    return CRef<CFeatureIndex> ();
}

void CFeatureIndex::SetFetchFailure (bool fails)

{
    CWeakRef<CBioseqIndex> bsx = GetBioseqIndex();
    auto bsxl = bsx.Lock();
    if (bsxl) {
        bsxl->SetFetchFailure(fails);
    }
}

// Find CFeatureIndex object for overlapping source feature using internal CFeatTree
CRef<CFeatureIndex> CFeatureIndex::GetOverlappingSource (void)

{
    try {
        CMappedFeat best;
        CWeakRef<CBioseqIndex> bsx = GetBioseqIndex();
        auto bsxl = bsx.Lock();
        if (bsxl) {
            if (bsxl->HasSource()) {
                CWeakRef<CSeqMasterIndex> idx = bsxl->GetSeqMasterIndex();
                auto idxl = idx.Lock();
                if (idxl) {
                    CRef<feature::CFeatTree> ft = idxl->GetFeatTree();
                    try {
                        best = ft->GetParent(m_Mf, CSeqFeatData::eSubtype_biosrc);
                    } catch (CException& e) {
                        ERR_POST_X(9, Error << "Error in CFeatureIndex::GetOverlappingSource: " << e.what());
                    }
                }
                if (best) {
                    return bsxl->GetFeatIndex(best);
                }
            }
        }
    } catch (CException& e) {
        ERR_POST_X(10, Error << "Error in CFeatureIndex::GetOverlappingSource: " << e.what());
    }
    return CRef<CFeatureIndex> ();
}

void CFeatureIndex::GetSequence (int from, int to, string& buffer)

{
    try {
        if (! m_SeqVec) {
            CWeakRef<CBioseqIndex> bsx = GetBioseqIndex();
            auto bsxl = bsx.Lock();
            if (bsxl) {
                CConstRef<CSeq_loc> lc = GetMappedLocation();
                if (lc) {
                    m_SeqVec = new CSeqVector(*lc, *bsxl->GetScope());
                    if (m_SeqVec) {
                        if (bsxl->IsAA()) {
                            m_SeqVec->SetCoding(CSeq_data::e_Ncbieaa);
                        } else {
                            m_SeqVec->SetCoding(CBioseq_Handle::eCoding_Iupac);
                        }
                    }
                }
            }
        }

        if (m_SeqVec) {
            CSeqVector& vec = *m_SeqVec;
            if (from < 0) {
                from = 0;
            }
            if (to < 0 || to >= (int) vec.size()) {
                to = vec.size();
            }
            if (vec.CanGetRange(from, to)) {
                vec.GetSeqData(from, to, buffer);
            } else {
                SetFetchFailure(true);
            }
        }
    }
    catch (CException& e) {
        SetFetchFailure(true);
        ERR_POST_X(11, Error << "Error in CFeatureIndex::GetSequence: " << e.what());
    }
}

string CFeatureIndex::GetSequence (int from, int to)

{
    string buffer;

    GetSequence(from, to, buffer);

    return buffer;
}

void CFeatureIndex::GetSequence (string& buffer)

{
    GetSequence(0, -1, buffer);
}

string CFeatureIndex::GetSequence (void)

{
    string buffer;

    GetSequence(0, -1, buffer);

    return buffer;
}


// CWordPairIndexer

// superscript and subscript code points not handled by UTF8ToAsciiString
typedef SStaticPair        <utf8::TUnicode, char> TExtraTranslationPair;
typedef CStaticPairArrayMap<utf8::TUnicode, char> TExtraTranslations;
static const TExtraTranslationPair kExtraTranslations[] = {
    { 0x00B2, '2' },
    { 0x00B3, '3' },
    { 0x00B9, '1' },
    { 0x2070, '0' },
    { 0x2071, '1' },
    { 0x2074, '4' },
    { 0x2075, '5' },
    { 0x2076, '6' },
    { 0x2077, '7' },
    { 0x2078, '8' },
    { 0x2079, '9' },
    { 0x207A, '+' },
    { 0x207B, '-' },
    { 0x207C, '=' },
    { 0x207D, '(' },
    { 0x207E, ')' },
    { 0x207F, 'n' },
    { 0x2080, '0' },
    { 0x2081, '1' },
    { 0x2082, '2' },
    { 0x2083, '3' },
    { 0x2084, '4' },
    { 0x2085, '5' },
    { 0x2086, '6' },
    { 0x2087, '7' },
    { 0x2088, '8' },
    { 0x2089, '9' },
    { 0x208A, '+' },
    { 0x208B, '-' },
    { 0x208C, '=' },
    { 0x208D, '(' },
    { 0x208E, ')' }
};
DEFINE_STATIC_ARRAY_MAP(TExtraTranslations, sc_ExtraTranslations,
                        kExtraTranslations);

string CWordPairIndexer::ConvertUTF8ToAscii( const string& str )

{
    const char* src = str.c_str();
    string dst;
    while (*src) {
        if (static_cast<unsigned char>(*src) < 128) { // no translation needed
            dst += *src++;
        } else {
            utf8::TUnicode character;
            size_t n = utf8::UTF8ToUnicode(src, &character);
            src += n;
            TExtraTranslations::const_iterator it
                = sc_ExtraTranslations.find(character);
            if (it != sc_ExtraTranslations.end()) {
                dst += it->second;
            } else {
                const utf8::SUnicodeTranslation* translation =
                    utf8::UnicodeToAscii(character);
                if (translation != NULL  &&  translation->Type != utf8::eSkip) {
                    _ASSERT(translation->Type == utf8::eString);
                    if (translation->Subst != NULL) {
                        dst += translation->Subst;
                    }
                }
            }
        }
    }
    return dst;
}

static const char* const idxStopWords[] = {
    "+",
    "-",
    "a",
    "about",
    "again",
    "all",
    "almost",
    "also",
    "although",
    "always",
    "among",
    "an",
    "and",
    "another",
    "any",
    "are",
    "as",
    "at",
    "be",
    "because",
    "been",
    "before",
    "being",
    "between",
    "both",
    "but",
    "by",
    "can",
    "could",
    "did",
    "do",
    "does",
    "done",
    "due",
    "during",
    "each",
    "either",
    "enough",
    "especially",
    "etc",
    "for",
    "found",
    "from",
    "further",
    "had",
    "has",
    "have",
    "having",
    "here",
    "how",
    "however",
    "i",
    "if",
    "in",
    "into",
    "is",
    "it",
    "its",
    "itself",
    "just",
    "kg",
    "km",
    "made",
    "mainly",
    "make",
    "may",
    "mg",
    "might",
    "ml",
    "mm",
    "most",
    "mostly",
    "must",
    "nearly",
    "neither",
    "no",
    "nor",
    "obtained",
    "of",
    "often",
    "on",
    "our",
    "overall",
    "perhaps",
    "pmid",
    "quite",
    "rather",
    "really",
    "regarding",
    "seem",
    "seen",
    "several",
    "should",
    "show",
    "showed",
    "shown",
    "shows",
    "significantly",
    "since",
    "so",
    "some",
    "such",
    "than",
    "that",
    "the",
    "their",
    "theirs",
    "them",
    "then",
    "there",
    "therefore",
    "these",
    "they",
    "this",
    "those",
    "through",
    "thus",
    "to",
    "upon",
    "use",
    "used",
    "using",
    "various",
    "very",
    "was",
    "we",
    "were",
    "what",
    "when",
    "which",
    "while",
    "with",
    "within",
    "without",
    "would",
};
typedef CStaticArraySet<const char*, PCase_CStr> TStopWords;
DEFINE_STATIC_ARRAY_MAP(TStopWords, sc_StopWords, idxStopWords);

bool CWordPairIndexer::IsStopWord(const string& str)

{
    TStopWords::const_iterator iter = sc_StopWords.find(str.c_str());
    return (iter != sc_StopWords.end());
}

string CWordPairIndexer::TrimPunctuation (const string& str)

{
    string dst = str;

    int max = dst.length();

    for (; max > 0; max--) {
        char ch = dst[0];
        if (ch != '.' && ch != ',' && ch != ':' && ch != ';') {
            break;
        }
        // trim leading period, comma, colon, and semicolon
        dst.erase(0, 1);
    }

    for (; max > 0; max--) {
        char ch = dst[max-1];
        if (ch != '.' && ch != ',' && ch != ':' && ch != ';') {
            break;
        }
        // // trim trailing period, comma, colon, and semicolon
        dst.erase(max-1, 1);
    }

    if (max > 1) {
        if (dst[0] == '(' && dst[max-1] == ')') {
            // trim flanking parentheses
            dst.erase(max-1, 1);
            dst.erase(0, 1);
            max -= 2;
        }
    }

    if (max > 0) {
        if (dst[0] == '(' && NStr::Find (dst, ")") == NPOS) {
            // trim isolated left parentheses
            dst.erase(0, 1);
            max--;
        }
    }

    if (max > 1) {
        if (dst[max-1] == ')' && NStr::Find (dst, "(") == NPOS) {
            // trim isolated right parentheses
            dst.erase(max-1, 1);
            // max--;
        }
    }

    return dst;
}

static const char* const mixedTags[] = {
    "<b>",
    "<i>",
    "<u>",
    "<sup>",
    "<sub>",
    "</b>",
    "</i>",
    "</u>",
    "</sup>",
    "</sub>",
    "<b/>",
    "<i/>",
    "<u/>",
    "<sup/>",
    "<sub/>",
    "&lt;i&gt;",
    "&lt;/i&gt;",
    "&lt;i/&gt;",
    "&lt;b&gt;",
    "&lt;/b&gt;",
    "&lt;b/&gt;",
    "&lt;u&gt;",
    "&lt;/u&gt;",
    "&lt;u/&gt;",
    "&lt;sub&gt;",
    "&lt;/sub&gt;",
    "&lt;sub/&gt;",
    "&lt;sup&gt;",
    "&lt;/sup&gt;",
    "&lt;sup/&gt;",
    "&amp;lt;i&amp;gt;",
    "&amp;lt;/i&amp;gt;",
    "&amp;lt;i/&amp;gt;",
    "&amp;lt;b&amp;gt;",
    "&amp;lt;/b&amp;gt;",
    "&amp;lt;b/&amp;gt;",
    "&amp;lt;u&amp;gt;",
    "&amp;lt;/u&amp;gt;",
    "&amp;lt;u/&amp;gt;",
    "&amp;lt;sub&amp;gt;",
    "&amp;lt;/sub&amp;gt;",
    "&amp;lt;sub/&amp;gt;",
    "&amp;lt;sup&amp;gt;",
    "&amp;lt;/sup&amp;gt;",
    "&amp;lt;sup/&amp;gt;",
};

static int SkipMixedContent ( const char* ptr )

{
    for (int i = 0; i < sizeof (mixedTags); i++) {
        const char* tag = mixedTags[i];
        const char* tmp = ptr;
        int len = 0;
        while (*tag && *tmp && *tag == *tmp) {
            tag++;
            tmp++;
            len++;
        }
        if (! *tag) {
            return len;
        }
    }
    return 0;
}

string CWordPairIndexer::TrimMixedContent ( const string& str )

{
    const char* src = str.c_str();
    string dst;
    while (*src) {
        if (*src == '<' || *src == '&') {
            int skip = SkipMixedContent (src);
            if (skip > 0) {
                src += skip;
            } else {
                dst += *src++;
            }
        } else {
            dst += *src++;
        }
    }
    return dst;
}

string CWordPairIndexer::x_AddToWordPairIndex (string item, string prev)

{
    if (IsStopWord(item)) {
        return "";
    }
    // append item
    m_Norm.push_back(item);
    if (! prev.empty()) {
        // append prev+" "+item
        string pair = prev + " " + item;
        m_Pair.push_back(pair);
    }
    return item;
}

void CWordPairIndexer::PopulateWordPairIndex (string str)

{
    m_Norm.clear();
    m_Pair.clear();

    str = ConvertUTF8ToAscii(str);
    NStr::ToLower(str);

    if (NStr::Find(str, "<") != NPOS || NStr::Find(str, "&") != NPOS) {
        str = TrimMixedContent(str);
    }

    // split terms at spaces
    list<string> terms;
    NStr::Split( str, " ", terms, NStr::fSplit_Tokenize );
    string prev = "";
    ITERATE( list<string>, it, terms ) {
        string curr = NStr::TruncateSpaces( *it );
        // allow parentheses in chemical formula
        curr = TrimPunctuation(curr);
        prev = x_AddToWordPairIndex (curr, prev);
    }

    // convert non-alphanumeric punctuation to space
    for (int i = 0; i < str.length(); i++) {
        char ch = str[i];
        if (ch >= 'A' && ch <= 'Z') {
        } else if (ch >= 'a' && ch <= 'z') {
        } else if (ch >= '0' && ch <= '9') {
        } else {
            str[i] = ' ';
        }
    }
    // now splitting at all punctuation
    list<string> words;
    NStr::Split( str, " ", words, NStr::fSplit_Tokenize );
    prev = "";
    ITERATE( list<string>, it, words ) {
        string curr = NStr::TruncateSpaces( *it );
        prev = x_AddToWordPairIndex (curr, prev);
    }

    std::sort(m_Norm.begin(), m_Norm.end());
    auto nit = std::unique(m_Norm.begin(), m_Norm.end());
    m_Norm.erase(nit, m_Norm.end());

    std::sort(m_Pair.begin(), m_Pair.end());
    auto pit = std::unique(m_Pair.begin(), m_Pair.end());
    m_Pair.erase(pit, m_Pair.end());
}


END_SCOPE(objects)
END_NCBI_SCOPE
