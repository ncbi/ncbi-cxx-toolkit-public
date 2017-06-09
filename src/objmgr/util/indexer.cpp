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

// Constructors take top-level object, create Seq-entry wrapper if necessary
CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, TFlags flags)
    : m_flags(flags)
{
    topsep.Parentize();
    m_topSEP.Reset(&topsep);

    x_Init();
}

CSeqEntryIndex::CSeqEntryIndex (CBioseq_set& seqset, TFlags flags)
    : m_flags(flags)
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

CSeqEntryIndex::CSeqEntryIndex (CBioseq& bioseq, TFlags flags)
    : m_flags(flags)
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

CSeqEntryIndex::CSeqEntryIndex (CSeq_submit& submit, TFlags flags)
    : m_flags(flags)
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

CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, CSubmit_block &sblock, TFlags flags)
    : m_flags(flags)
{
    topsep.Parentize();
    m_topSEP.Reset(&topsep);
    m_sbtBlk.Reset(&sblock);

    x_Init();
}

CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, CSeq_descr &descr, TFlags flags)
    : m_flags(flags)
{
    topsep.Parentize();
    m_topSEP.Reset(&topsep);
    m_topDescr.Reset(&descr);

    x_Init();
}

// Destructor
CSeqEntryIndex::~CSeqEntryIndex (void)

{
}
// Recursively explores from top-level Seq-entry to make flattened vector of CBioseqIndex objects
void CSeqEntryIndex::x_InitSeqs (const CSeq_entry& sep, CRef<CSeqsetIndex> prnt)

{
    if (sep.IsSeq()) {
        // Is Bioseq
        const CBioseq& bsp = sep.GetSeq();
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(bsp);
        if (bsh) {
            // create CBioseqIndex object for current Bioseq
            CRef<CBioseqIndex> bsx(new CBioseqIndex(bsh, bsp, bsh, prnt, *this, false));

            // record CBioseqIndex in vector for IterateBioseqs or ProcessBioseq
            m_bsxList.push_back(bsx);

            // map from accession string to CBioseqIndex object
            string& accn = bsx->m_Accession;
            m_accnIndexMap[accn] = bsx;
        }
    } else if (sep.IsSet()) {
        // Is Bioseq-set
        const CBioseq_set& bssp = sep.GetSet();
        CBioseq_set_Handle ssh = m_scope->GetBioseq_setHandle(bssp);
        if (ssh) {
            // create CSeqsetIndex object for current Bioseq-set
            CRef<CSeqsetIndex> ssx(new CSeqsetIndex(ssh, bssp, prnt, *this));

            // record CSeqsetIndex in vector
            m_ssxList.push_back(ssx);

            if (bssp.CanGetSeq_set()) {
                // recursively explore current Bioseq-set
                for (const CRef<CSeq_entry>& tmp : bssp.GetSeq_set()) {
                    x_InitSeqs(*tmp, ssx);
                }
            }
        }
    }
}

// Common initialization function
void CSeqEntryIndex::x_Init (void)

{
    try {
        m_objmgr = CObjectManager::GetInstance();
        if ( !m_objmgr ) {
            /* raise hell */;
        }

        m_scope.Reset( new CScope( *m_objmgr ) );
        if ( !m_scope ) {
            /* raise hell */;
        }

        m_Counter.Set(0);

        m_scope->AddDefaults();

        m_topSEH = m_scope->AddTopLevelSeqEntry( *m_topSEP );

        // Populate vector of CBioseqIndex objects representing local Bioseqs in blob
        CRef<CSeqsetIndex> noparent;
        x_InitSeqs( *m_topSEP, noparent );
    }
    catch (CException& e) {
        LOG_POST(Error << "Error in CSeqEntryIndex::x_Init: " << e.what());
    }
}

CRef<CSeq_id> CSeqEntryIndex::MakeUniqueId(void)
{
    CRef<CSeq_id> id(new CSeq_id());
    bool good = false;
    while (!good) {
        id->SetLocal().SetStr("tmp_delta_subset_" + NStr::NumericToString(m_Counter.Add(1)));
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(*id);
        if (! bsh) {
            good = true;
        }
    }
    return id;
}

CRef<CBioseqIndex> CSeqEntryIndex::x_DeltaIndex(const CSeq_loc& loc)

{
    try {
        // create delta sequence referring to location or range, using temporary local Seq-id
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(loc);
        CRef<CBioseq> delta(new CBioseq());
        delta->SetId().push_back(MakeUniqueId());
        delta->SetInst().Assign(bsh.GetInst());
        delta->SetInst().ResetSeq_data();
        delta->SetInst().ResetExt();
        delta->SetInst().SetRepr(CSeq_inst::eRepr_delta);
        CRef<CDelta_seq> element(new CDelta_seq());
        element->SetLoc().Assign(loc);
        delta->SetInst().SetExt().SetDelta().Set().push_back(element);
        delta->SetInst().SetLength(sequence::GetLength(loc, m_scope));

        // add to scope
        CBioseq_Handle deltaBsh = m_scope->AddBioseq(*delta);

        if (deltaBsh) {
            // create CBioseqIndex object for delta Bioseq
            CRef<CSeqsetIndex> noparent;
            CRef<CBioseqIndex> bsx(new CBioseqIndex(deltaBsh, *delta, bsh, noparent, *this, true));

           return bsx;
        }
    }
    catch (CException& e) {
        LOG_POST(Error << "Error in CSeqEntryIndex::x_DeltaIndex: " << e.what());
    }
    return CRef<CBioseqIndex> ();
}

CConstRef<CSeq_loc> CSeqEntryIndex::x_SubRangeLoc(const string& accn, int from, int to, bool rev_comp)

{
    TAccnIndexMap::iterator it = m_accnIndexMap.find(accn);
    if (it != m_accnIndexMap.end()) {
        CRef<CBioseqIndex> bsx = it->second;
        for (const CRef<CSeq_id>& id : bsx->GetBioseq().GetId()) {
            switch (id->Which()) {
                case CSeq_id::e_Other:
                case CSeq_id::e_Genbank:
                case CSeq_id::e_Embl:
                case CSeq_id::e_Ddbj:
                case CSeq_id::e_Tpg:
                case CSeq_id::e_Tpe:
                case CSeq_id::e_Tpd:
                    {
                        CSeq_loc::TStrand strand = eNa_strand_unknown;
                        if (rev_comp) {
                            strand = eNa_strand_minus;
                        }
                        CSeq_id& nc_id = const_cast<CSeq_id&>(*id);
                        // create location from range
                        CConstRef<CSeq_loc> loc(new CSeq_loc(nc_id, from, to, strand));
                        if (loc) {
                           return loc;
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
    return CConstRef<CSeq_loc> ();
}

// Get first Bioseq index
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (void)

{
    for (auto& bsx : m_bsxList) {
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Nth Bioseq index
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (int n)

{
    for (auto& bsx : m_bsxList) {
        n--;
        if (n > 0) continue;
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Bioseq index by accession
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (const string& accn)

{
    TAccnIndexMap::iterator it = m_accnIndexMap.find(accn);
    if (it != m_accnIndexMap.end()) {
        CRef<CBioseqIndex> bsx = it->second;
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Bioseq index by sublocation
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (const CSeq_loc& loc)

{
    CRef<CBioseqIndex> bsx = x_DeltaIndex(loc);

    if (bsx) {
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Bioseq index by subrange
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (const string& accn, int from, int to, bool rev_comp)

{
    CConstRef<CSeq_loc> loc = x_SubRangeLoc(accn, from, to, rev_comp);

    if (loc) {
        return GetBioseqIndex(*loc);
    }
    return CRef<CBioseqIndex> ();
}


// CSeqsetIndex

// Constructor
CSeqsetIndex::CSeqsetIndex (CBioseq_set_Handle ssh,
                            const CBioseq_set& bssp,
                            CRef<CSeqsetIndex> prnt,
                            CSeqEntryIndex& enx)
    : m_ssh(ssh),
      m_bssp(bssp),
      m_prnt(prnt),
      m_enx(enx),
      m_scope(enx.m_scope)
{
    m_class = CBioseq_set::eClass_not_set;

    if (ssh.IsSetClass()) {
        m_class = ssh.GetClass();
    }
}

// Destructor
CSeqsetIndex::~CSeqsetIndex (void)

{
}


// CBioseqIndex

// Constructor
CBioseqIndex::CBioseqIndex (CBioseq_Handle bsh,
                            const CBioseq& bsp,
                            CBioseq_Handle obsh,
                            CRef<CSeqsetIndex> prnt,
                            CSeqEntryIndex& enx,
                            bool surrogate)
    : m_bsh(bsh),
      m_bsp(bsp),
      m_obsh(obsh),
      m_prnt(prnt),
      m_enx(enx),
      m_scope(enx.m_scope),
      m_surrogate(surrogate)
{
    m_descsInitialized = false;
    m_featsInitialized = false;

    m_Accession.clear();

    for (CSeq_id_Handle sid : obsh.GetId()) {
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
    m_topology = CSeq_inst::eTopology_not_set;
    m_length = 0;

    m_IsDelta = false;
    m_IsVirtual = false;
    m_IsMap = false;

    if (m_bsh.IsSetInst()) {
        if (m_bsh.IsSetInst_Topology()) {
            m_topology = m_bsh.GetInst_Topology();
        }

        if (m_bsh.IsSetInst_Length()) {
            m_length = m_bsh.GetInst_Length();
        } else {
            m_length = m_bsh.GetBioseqLength();
        }

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
    m_taxname.clear();
    m_defline.clear();
    m_dlflags = 0;

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
    try {
        if (m_descsInitialized) {
           return;
        }

        m_descsInitialized = true;

        // explore descriptors, pass original target BioseqHandle if using Bioseq sublocation
        for (CSeqdesc_CI desc_it(m_obsh); desc_it; ++desc_it) {
            const CSeqdesc& sd = *desc_it;
            CRef<CDescriptorIndex> sdx(new CDescriptorIndex(sd, *this));
            m_sdxList.push_back(sdx);

            switch (sd.Which()) {
                case CSeqdesc::e_Source:
                {
                    if (! m_bioSource) {
                        const CBioSource& biosrc = sd.GetSource();
                        m_bioSource.Reset (&biosrc);
                        if (biosrc.IsSetOrgname()) {
                            const COrg_ref& org = biosrc.GetOrg();
                            if (org.CanGetTaxname()) {
                                m_taxname = org.GetTaxname();
                            }
                        }
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
                                    if (fld.IsSetLabel() && fld.GetLabel().IsStr()) {
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
    catch (CException& e) {
        LOG_POST(Error << "Error in CBioseqIndex::x_InitDescs: " << e.what());
    }
}

// Feature collection (delayed until needed)
void CBioseqIndex::x_InitFeats (void)

{
    try {
        if (m_featsInitialized) {
           return;
        }

        if (! m_descsInitialized) {
            // initialize descriptors first to get m_onlyNearFeats flag
            x_InitDescs();
        }

        m_featsInitialized = true;

        SAnnotSelector sel;

        // handle far fetching policy, from user object or initializer argument
        if (m_onlyNearFeats) {
  
            if (m_surrogate) {
                // delta with sublocation needs to map features from original Bioseq
                sel.SetResolveAll();
                sel.SetResolveDepth(1);
                sel.SetExcludeExternal();
            } else {
                // otherwise limit collection to local records in top-level Seq-entry
                sel.SetResolveDepth(0);
                sel.SetExcludeExternal();
            }

        } else {

            // normal situation allowing adaptive depth for feature collection
            sel.SetResolveAll();
            sel.SetResolveDepth(kMax_Int);
            sel.SetAdaptiveDepth(true);
        }

        // iterate features on Bioseq
        for (CFeat_CI feat_it(m_bsh, sel); feat_it; ++feat_it) {
            const CMappedFeat mf = *feat_it;
            CSeq_feat_Handle hdl = mf.GetSeq_feat_Handle();

            const CSeq_feat& mpd = mf.GetMappedFeature();
            CConstRef<CSeq_loc> fl(&mpd.GetLocation());

            CRef<CFeatureIndex> sfx(new CFeatureIndex(hdl, mf, fl, *this));
            m_sfxList.push_back(sfx);

            m_featTree.AddFeature(mf);

            // CFeatIndex from CMappedFeat for use with GetBestGene
            m_featIndexMap[mf] = sfx;
        }
    }
    catch (CException& e) {
        LOG_POST(Error << "Error in CBioseqIndex::x_InitFeats: " << e.what());
    }
}

// Common descriptor field getters
const string& CBioseqIndex::GetTitle (void)

{
    if (! m_descsInitialized) {
        x_InitDescs();
    }

    return m_title;
}

CConstRef<CMolInfo> CBioseqIndex::GetMolInfo (void)

{
    if (! m_descsInitialized) {
        x_InitDescs();
    }

    return m_molInfo;
}

CMolInfo::TBiomol CBioseqIndex::GetBiomol (void)

{
    if (! m_descsInitialized) {
        x_InitDescs();
    }

    return m_biomol;
}

CMolInfo::TTech CBioseqIndex::GetTech (void)

{
    if (! m_descsInitialized) {
        x_InitDescs();
    }

    return m_tech;
}

CMolInfo::TCompleteness CBioseqIndex::GetCompleteness (void)

{
    if (! m_descsInitialized) {
        x_InitDescs();
    }

    return m_completeness;
}

CConstRef<CBioSource> CBioseqIndex::GetBioSource (void)

{
    if (! m_descsInitialized) {
        x_InitDescs();
    }

    return m_bioSource;
}

const string& CBioseqIndex::GetTaxname (void)

{
    if (! m_descsInitialized) {
        x_InitDescs();
    }

    return m_taxname;
}

// Run defline generator
const string& CBioseqIndex::GetDefline (sequence::CDeflineGenerator::TUserFlags flags)

{
    if (! m_featsInitialized) {
        x_InitFeats();
    }

    if (flags == m_dlflags && ! m_defline.empty()) {
        // Return previous result if flags are the same
        return m_defline;
    }

    sequence::CDeflineGenerator gen (m_enx.m_topSEH);

    // Pass original target BioseqHandle if using Bioseq sublocation
    m_defline = gen.GenerateDefline (m_obsh, m_featTree, flags);

    m_dlflags = flags;

    return m_defline;
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

void CBioseqIndex::GetSequence (int from, int to, string& buffer)

{
    try {
        if (! m_sv) {
            m_sv = new CSeqVector(m_bsh);
            if (m_sv) {
                m_sv->SetCoding(CBioseq_Handle::eCoding_Iupac);
            }
        }

        if (m_sv) {
            CSeqVector& vec = *m_sv;
            if (from < 0) {
                from = 0;
            }
            if (to < 0 || to >= vec.size()) {
                to = vec.size();
            }
            if (vec.CanGetRange(from, to)) {
                vec.GetSeqData(from, to, buffer);
            }
        }
    }
    catch (CException& e) {
        LOG_POST(Error << "Error in CBioseqIndex::GetSequence: " << e.what());
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


// CDescriptorIndex

// Constructor
CDescriptorIndex::CDescriptorIndex (const CSeqdesc& sd,
                                    CBioseqIndex& bsx)
    : m_sd(sd),
      m_bsx(bsx)
{
    m_subtype = m_sd.Which();
}

// Destructor
CDescriptorIndex::~CDescriptorIndex (void)

{
}


// CFeatureIndex

// Constructor
CFeatureIndex::CFeatureIndex (CSeq_feat_Handle sfh,
                              const CMappedFeat mf,
                              CConstRef<CSeq_loc> fl,
                              CBioseqIndex& bsx)
    : m_sfh(sfh),
      m_mf(mf),
      m_fl(fl),
      m_bsx(bsx)
{
    m_subtype = m_mf.GetData().GetSubtype();
}

// Destructor
CFeatureIndex::~CFeatureIndex (void)

{
}

void CFeatureIndex::GetSequence (int from, int to, string& buffer)

{
    try {
        if (! m_sv) {
            m_sv = new CSeqVector(*m_fl, *GetBioseqIndex().GetScope());
            if (m_sv) {
                m_sv->SetCoding(CBioseq_Handle::eCoding_Iupac);
            }
        }

        if (m_sv) {
            CSeqVector& vec = *m_sv;
            if (from < 0) {
                from = 0;
            }
            if (to < 0 || to >= vec.size()) {
                to = vec.size();
            }
            if (vec.CanGetRange(from, to)) {
                vec.GetSeqData(from, to, buffer);
            }
        }
    }
    catch (CException& e) {
        LOG_POST(Error << "Error in CFeatureIndex::GetSequence: " << e.what());
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


END_SCOPE(objects)
END_NCBI_SCOPE
