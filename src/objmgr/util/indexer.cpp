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

#include <objmgr/util/indexer.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// CSeqEntryIndex

// Constructor
CSeqEntryIndex::CSeqEntryIndex (TFlags flags)

{
    m_flags = flags;
}

// Destructor
CSeqEntryIndex::~CSeqEntryIndex (void)

{
}

// Initializers take top-level object, create Seq-entry wrapper if necessary
void CSeqEntryIndex::Initialize (CSeq_entry& topsep)
{
    topsep.Parentize();
    m_topSEP.Reset(&topsep);

    x_Init();
}

void CSeqEntryIndex::Initialize (CBioseq_set& seqset)
{
    CSeq_entry* parent = seqset.GetParentEntry();
    if (parent) {
        parent->Parentize();
        m_topSEP.Reset(parent);
    } else {
        CRef<CSeq_entry> sep(new CSeq_entry);
        sep->SetSet(seqset);
        sep->Parentize();
        m_topSEP.Reset(sep);
    }

    x_Init();
}

void CSeqEntryIndex::Initialize (CBioseq& bioseq)
{
    CSeq_entry* parent = bioseq.GetParentEntry();
    if (parent) {
        parent->Parentize();
        m_topSEP.Reset(parent);
    } else {
        CRef<CSeq_entry> sep(new CSeq_entry);
        sep->SetSeq(bioseq);
        sep->Parentize();
        m_topSEP.Reset(sep);
    }

    x_Init();
}

void CSeqEntryIndex::Initialize (CSeq_submit& submit)
{
    _ASSERT(submit.CanGetData());
    _ASSERT(submit.CanGetSub());
    _ASSERT(submit.GetData().IsEntrys());
    _ASSERT(!submit.GetData().GetEntrys().empty());

    CRef<CSeq_entry> sep = submit.GetData().GetEntrys().front();
    sep->Parentize();
    m_topSEP.Reset(sep);
    m_sbtBlk.Reset(&submit.GetSub());

    x_Init();
}

void CSeqEntryIndex::Initialize (CSeq_entry& topsep, CSubmit_block &sblock)
{
    topsep.Parentize();
    m_topSEP.Reset(&topsep);
    m_sbtBlk.Reset(&sblock);

    x_Init();
}

void CSeqEntryIndex::Initialize (CSeq_entry& topsep, CSeq_descr &descr)
{
    topsep.Parentize();
    m_topSEP.Reset(&topsep);
    m_topDescr.Reset(&descr);

    x_Init();
}

// Recursively explores from top-level Seq-entry to make flattened vector of CBioseqIndex objects
void CSeqEntryIndex::BspInSep (const CSeq_entry& sep)

{
    if (sep.IsSeq()) {
        // Is Bioseq
        const CBioseq& bsp = sep.GetSeq();
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(bsp);
        if (bsh) {
            // create CBioseqIndex object for current Bioseq
            CRef<CBioseqIndex> bsx(new CBioseqIndex(bsh, bsp, *this));
            // record as CBioseqIndex in vector
            m_bsxList.push_back(bsx);
            // index CBioseqIndex from accession string
            string& accn = bsx->m_Accession;
            m_accnIndexMap[accn] = bsx;
        }
    } else if (sep.IsSet()) {
        // Is Bioseq-set
        const CBioseq_set& bssp = sep.GetSet();
        if (bssp.CanGetSeq_set()) {
            // recursively explore current Bioseq-set
            for (const CRef<CSeq_entry>& tmp : bssp.GetSeq_set()) {
                BspInSep(*tmp);
            }
        }
    }
}

// Common initialization function
void CSeqEntryIndex::x_Init (void)

{
    m_objmgr = CObjectManager::GetInstance();
    if ( !m_objmgr ) {
        /* raise hell */;
    }

    m_scope.Reset( new CScope( *m_objmgr ) );
    if ( !m_scope ) {
        /* raise hell */;
    }

    m_scope->AddDefaults();

    m_topSEH = m_scope->AddTopLevelSeqEntry( *m_topSEP );

    // Populate vector of CBioseqIndex objects representing local Bioseqs in blob
    BspInSep( *m_topSEP );
}





// CBioseqIndex

// Constructor
CBioseqIndex::CBioseqIndex (CBioseq_Handle bsh, const CBioseq& bsp, CSeqEntryIndex& enx)
    : m_bsh(bsh), m_bsp(bsp), m_enx(enx), m_scope(enx.m_scope)
{
    m_descsInitialized = false;
    m_featsInitialized = false;

    m_Accession.clear();

    for (CSeq_id_Handle sid : bsh.GetId()) {
        switch (sid.Which()) {
            case CSeq_id::e_Other:
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                {
                    CConstRef<CSeq_id> id = sid.GetSeqId();
                    const CTextseq_id& tsid = *id->GetTextseq_Id ();
                    if (tsid.IsSetAccession()) {
                        m_Accession = tsid.GetAccession ();
                    }
                }
                break;
            default:
                break;
        }
    }

    m_IsNA = m_bsh.IsNa();
    m_IsAA = m_bsh.IsAa();
    m_topology = m_bsh.GetInst_Topology();

    m_IsDelta = false;
    m_IsVirtual = false;
    m_IsMap = false;

    if (m_bsh.IsSetInst()) {
        if (m_bsh.IsSetInst_Repr()) {
            CBioseq_Handle::TInst_Repr repr = m_bsh.GetInst_Repr();
            m_IsDelta = (repr == CSeq_inst::eRepr_delta);
            m_IsVirtual = (repr == CSeq_inst::eRepr_virtual);
            m_IsMap = (repr == CSeq_inst::eRepr_map);
        }
    }

    m_title.clear();
    m_molInfo.Reset();
    m_bioSource.Reset();

    m_biomol = CMolInfo::eBiomol_unknown;
    m_tech = CMolInfo::eTech_unknown;
    m_completeness = CMolInfo::eCompleteness_unknown;

    m_forceOnlyNearFeats = false;
    m_onlyNearFeats = false;
}

// Destructor
CBioseqIndex::~CBioseqIndex (void)

{
}

// Descriptor collection (delayed until needed)
void CBioseqIndex::x_InitDescs (void)

{
    if (m_descsInitialized) {
        return;
    }

    m_descsInitialized = true;

    // explore descriptors
    for (CSeqdesc_CI desc_it(m_bsh); desc_it; ++desc_it) {
        const CSeqdesc& sd = *desc_it;
        CRef<CDescriptorIndex> sdx(new CDescriptorIndex(sd, *this));
        m_sdxList.push_back(sdx);

        switch (sd.Which()) {
            case CSeqdesc::e_Source:
            {
                if (! m_bioSource) {
                    m_bioSource.Reset (&sd.GetSource());
                }
                break;
            }
            case CSeqdesc::e_Molinfo:
            {
                if (! m_molInfo) {
                    const CMolInfo& molinf = sd.GetMolinfo();
                    m_molInfo.Reset (&molinf);
                    m_biomol = molinf.GetBiomol();
                    m_tech = molinf.GetTech();
                    m_completeness = molinf.GetCompleteness();
                }
                break;
            }
            case CSeqdesc::e_Title:
            {
                if (m_title.empty()) {
                    m_title = sd.GetTitle();
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
                                if (FIELD_IS_SET_AND_IS(fld, Label, Str)) {
                                    const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                                    if (! NStr::EqualNocase(label_str, "Policy")) continue;
                                    if (fld.IsSetData() && fld.GetData().IsStr()) {
                                        const string& str = fld.GetData().GetStr();
                                        if (NStr::EqualNocase(str, "OnlyNearFeatures")) {
                                            m_forceOnlyNearFeats = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    // set policy flags for object manager feature exploration
    if (m_forceOnlyNearFeats) {
        m_onlyNearFeats = true;
    }

    // initialization option can also prevent far feature exploration
    if ((m_enx.m_flags & CSeqEntryIndex::fSkipRemoteFeatures) != 0) {
        m_onlyNearFeats = true;
    }
}

// Feature collection (delayed until needed)
void CBioseqIndex::x_InitFeats (void)

{
    if (m_featsInitialized) {
        return;
    }

    if (! m_descsInitialized) {
        // initialize descriptors first to get m_onlyNearFeats flag
        x_InitDescs();
    }

    m_featsInitialized = true;

    SAnnotSelector sel;

    sel.SetLimitTSE(m_bsh.GetTSE_Handle());

    if (m_onlyNearFeats) {
        sel.SetResolveDepth(0);
    } else {
        sel.SetAdaptiveDepth(true);
    }

    // explore features
    for (CFeat_CI feat_it(m_bsh, sel); feat_it; ++feat_it) {
        const CMappedFeat mf = *feat_it;

        CSeq_feat_Handle hdl = mf.GetSeq_feat_Handle();
        CRef<CFeatureIndex> sfx(new CFeatureIndex(hdl, mf, *this));
        m_sfxList.push_back(sfx);

        m_featTree.AddFeature(mf);

        // index CFeatIndex from CMappedFeat for use with GetBestGene
        m_featIndexMap[mf] = sfx;
    }
}

DEFINE_STATIC_MUTEX(idx_UniqueIdMutex);
static size_t idx_UniqueIdOffset = 0;
CRef<CSeq_id> idx_MakeUniqueId(CScope& scope)
{
    CMutexGuard guard(idx_UniqueIdMutex);

    CRef<CSeq_id> id(new CSeq_id());
    bool good = false;
    while (!good) {
        id->SetLocal().SetStr("tmp_delta" + NStr::NumericToString(idx_UniqueIdOffset));
        CBioseq_Handle bsh = scope.GetBioseqHandle(*id);
        if (bsh) {
            idx_UniqueIdOffset++;
        } else {
            good = true;
        }
    }
    return id;
}

static CRef<CBioseq> idx_MakeTemporaryDelta(const CSeq_loc& loc, CScope& scope)
{
    CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
    CRef<CBioseq> seq(new CBioseq());
    seq->SetId().push_back(idx_MakeUniqueId(scope));
    seq->SetInst().Assign(bsh.GetInst());
    seq->SetInst().ResetSeq_data();
    seq->SetInst().ResetExt();
    seq->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    CRef<CDelta_seq> element(new CDelta_seq());
    element->SetLoc().Assign(loc);
    seq->SetInst().SetExt().SetDelta().Set().push_back(element);
    seq->SetInst().SetLength(sequence::GetLength(*loc.GetId(), &scope));
    return seq;
}

void CBioseqIndex::InitializeFeatures (const CSeq_loc& loc)

{
    if (m_featsInitialized) {
        return;
    }

    if (! m_descsInitialized) {
        // initialize descriptors first to get m_onlyNearFeats flag
        x_InitDescs();
    }

    m_featsInitialized = true;

    SAnnotSelector sel;

    // use temporary delta for selector
    sel.SetResolveAll();
    sel.SetResolveDepth(kMax_Int);
    sel.SetAdaptiveDepth(true);
    CRef<CBioseq> delta = idx_MakeTemporaryDelta(loc, *m_scope);
    CBioseq_Handle delta_bsh = m_scope->AddBioseq(*delta);

    // explore features
    for (CFeat_CI feat_it(delta_bsh, sel); feat_it; ++feat_it) {
        const CMappedFeat mf = *feat_it;

        CSeq_feat_Handle hdl = mf.GetSeq_feat_Handle();
        CRef<CFeatureIndex> sfx(new CFeatureIndex(hdl, mf, *this));
        m_sfxList.push_back(sfx);

        m_featTree.AddFeature(mf);

        // index CFeatIndex from CMappedFeat for use with GetBestGene
        m_featIndexMap[mf] = sfx;
    }

    // remove temporary delta
    m_scope->RemoveBioseq(delta_bsh);
}

void CBioseqIndex::InitializeFeatures (int from, int to, bool rev_comp)

{
    CRef<CSeq_loc> loc(new CSeq_loc);
    CSeq_interval& ival = loc->SetInt();
    ival.SetFrom(from);
    ival.SetTo(to);
    if (rev_comp) {
        ival.SetStrand(eNa_strand_minus);
    }
    for (const CRef<CSeq_id>& id : m_bsp.GetId()) {
        switch (id->Which()) {
            case NCBI_SEQID(Other):
            case NCBI_SEQID(Genbank):
            case NCBI_SEQID(Embl):
            case NCBI_SEQID(Ddbj):
            case NCBI_SEQID(Tpg):
            case NCBI_SEQID(Tpe):
            case NCBI_SEQID(Tpd):
                {
                    ival.SetId().Assign(*id);
                    InitializeFeatures(*loc);
                    return;
                }
                break;
            default:
                break;
        }
    }
}

CRef<CFeatureIndex> CBioseqIndex::GetFeatIndex (CMappedFeat mf)

{
    CRef<CFeatureIndex> sfx;

    TFeatIndexMap::iterator it = m_featIndexMap.find(mf);
    if (it != m_featIndexMap.end()) {
        sfx = it->second;
    }

    return sfx;
}

// CDescriptorIndex

// Constructor
CDescriptorIndex::CDescriptorIndex (const CSeqdesc& sd, CBioseqIndex& bsx)
    : m_sd(sd), m_bsx(bsx)
{
    x_Init();
}

// Destructor
CDescriptorIndex::~CDescriptorIndex (void)

{
}

// Common initialization function
void CDescriptorIndex::x_Init (void)

{
    m_subtype = m_sd.Which();
}


// CFeatureIndex

// Constructor
CFeatureIndex::CFeatureIndex (CSeq_feat_Handle sfh, const CMappedFeat mf, CBioseqIndex& bsx)
    : m_sfh(sfh), m_mf(mf), m_bsx(bsx)
{
    x_Init();
}

// Destructor
CFeatureIndex::~CFeatureIndex (void)

{
}

// Common initialization function
void CFeatureIndex::x_Init (void)

{
    m_subtype = m_mf.GetData().GetSubtype();
}


END_SCOPE(objects)
END_NCBI_SCOPE
