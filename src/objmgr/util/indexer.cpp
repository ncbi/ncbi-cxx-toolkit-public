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

#include <objmgr/util/indexer.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// CSeqEntryIndex

// Constructors take top-level object, create Seq-entry wrapper if necessary
CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, TFlags flags)
    : m_Flags(flags)
{
    topsep.Parentize();
    m_Tsep.Reset(&topsep);

    x_Init();
}

CSeqEntryIndex::CSeqEntryIndex (CBioseq_set& seqset, TFlags flags)
    : m_Flags(flags)
{
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

CSeqEntryIndex::CSeqEntryIndex (CBioseq& bioseq, TFlags flags)
    : m_Flags(flags)
{
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

CSeqEntryIndex::CSeqEntryIndex (CSeq_submit& submit, TFlags flags)
    : m_Flags(flags)
{
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

CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, CSubmit_block &sblock, TFlags flags)
    : m_Flags(flags)
{
    topsep.Parentize();
    m_Tsep.Reset(&topsep);
    m_SbtBlk.Reset(&sblock);

    x_Init();
}

CSeqEntryIndex::CSeqEntryIndex (CSeq_entry& topsep, CSeq_descr &descr, TFlags flags)
    : m_Flags(flags)
{
    topsep.Parentize();
    m_Tsep.Reset(&topsep);
    m_TopDescr.Reset(&descr);

    x_Init();
}

// Recursively explores from top-level Seq-entry to make flattened vector of CBioseqIndex objects
void CSeqEntryIndex::x_InitSeqs (const CSeq_entry& sep, CRef<CSeqsetIndex> prnt)

{
    if (sep.IsSeq()) {
        // Is Bioseq
        const CBioseq& bsp = sep.GetSeq();
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(bsp);
        if (bsh) {
            // create CBioseqIndex object for current Bioseq
            CRef<CBioseqIndex> bsx(new CBioseqIndex(bsh, bsp, bsh, prnt, m_Tseh, m_Scope, m_LocalFeatures, false));

            // record CBioseqIndex in vector for IterateBioseqs or GetBioseqIndex
            m_BsxList.push_back(bsx);

            // map from accession string to CBioseqIndex object
            const string& accn = bsx->GetAccession();
            m_AccnIndexMap[accn] = bsx;
        }
    } else if (sep.IsSet()) {
        // Is Bioseq-set
        const CBioseq_set& bssp = sep.GetSet();
        CBioseq_set_Handle ssh = m_Scope->GetBioseq_setHandle(bssp);
        if (ssh) {
            // create CSeqsetIndex object for current Bioseq-set
            CRef<CSeqsetIndex> ssx(new CSeqsetIndex(ssh, bssp, prnt));

            // record CSeqsetIndex in vector
            m_SsxList.push_back(ssx);

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
        m_Objmgr = CObjectManager::GetInstance();
        if ( !m_Objmgr ) {
            /* raise hell */;
        }

        m_Scope.Reset( new CScope( *m_Objmgr ) );
        if ( !m_Scope ) {
            /* raise hell */;
        }

        m_Counter.Set(0);

        m_Scope->AddDefaults();

        m_Tseh = m_Scope->AddTopLevelSeqEntry( *m_Tsep );

        m_FetchFailure = false;

        m_LocalFeatures = false;
        if ((m_Flags & CSeqEntryIndex::fSkipRemoteFeatures) != 0) {
            m_LocalFeatures = true;
        }

        // Populate vector of CBioseqIndex objects representing local Bioseqs in blob
        CRef<CSeqsetIndex> noparent;
        x_InitSeqs( *m_Tsep, noparent );
    }
    catch (CException& e) {
        LOG_POST(Error << "Error in CSeqEntryIndex::x_Init: " << e.what());
    }
}

CRef<CSeq_id> CSeqEntryIndex::x_MakeUniqueId(void)
{
    CRef<CSeq_id> id(new CSeq_id());
    bool good = false;
    while (!good) {
        id->SetLocal().SetStr("tmp_delta_subset_" + NStr::NumericToString(m_Counter.Add(1)));
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id);
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
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(loc);
        CRef<CBioseq> delta(new CBioseq());
        delta->SetId().push_back(x_MakeUniqueId());
        delta->SetInst().Assign(bsh.GetInst());
        delta->SetInst().ResetSeq_data();
        delta->SetInst().ResetExt();
        delta->SetInst().SetRepr(CSeq_inst::eRepr_delta);
        CRef<CDelta_seq> element(new CDelta_seq());
        element->SetLoc().Assign(loc);
        delta->SetInst().SetExt().SetDelta().Set().push_back(element);
        delta->SetInst().SetLength(sequence::GetLength(loc, m_Scope));

        // add to scope
        CBioseq_Handle deltaBsh = m_Scope->AddBioseq(*delta);

        if (deltaBsh) {
            // create CBioseqIndex object for delta Bioseq
            CRef<CSeqsetIndex> noparent;
            CRef<CBioseqIndex> bsx(new CBioseqIndex(deltaBsh, *delta, bsh, noparent, m_Tseh, m_Scope, m_LocalFeatures, true));

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
    TAccnIndexMap::iterator it = m_AccnIndexMap.find(accn);
    if (it != m_AccnIndexMap.end()) {
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
    for (auto& bsx : m_BsxList) {
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Nth Bioseq index
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (int n)

{
    for (auto& bsx : m_BsxList) {
        n--;
        if (n > 0) continue;
        return bsx;
    }
    return CRef<CBioseqIndex> ();
}

// Get Bioseq index by accession
CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (const string& accn)

{
    TAccnIndexMap::iterator it = m_AccnIndexMap.find(accn);
    if (it != m_AccnIndexMap.end()) {
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
    string accession = accn;
    if (accession.empty()) {
        CRef<CBioseqIndex> bsx = GetBioseqIndex();
        if (bsx) {
            accession = bsx->GetAccession();
        }
    }

    if (! accession.empty()) {
        CConstRef<CSeq_loc> loc = x_SubRangeLoc(accession, from, to, rev_comp);

        if (loc) {
            return GetBioseqIndex(*loc);
        }
    }
    return CRef<CBioseqIndex> ();
}

CRef<CBioseqIndex> CSeqEntryIndex::GetBioseqIndex (int from, int to, bool rev_comp)

{
    return GetBioseqIndex("", from, to, rev_comp);
}

const vector<CRef<CBioseqIndex>>& CSeqEntryIndex::GetBioseqIndices(void)

{
    return m_BsxList;
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
                            bool localFeatures,
                            bool surrogate)
    : m_Bsh(bsh),
      m_Bsp(bsp),
      m_OrigBsh(obsh),
      m_Prnt(prnt),
      m_Tseh(tseh),
      m_Scope(scope),
      m_LocalFeatures(localFeatures),
      m_Surrogate(surrogate)
{
    m_DescsInitialized = false;
    m_FeatsInitialized = false;

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

    m_IsNA = m_Bsh.IsNa();
    m_IsAA = m_Bsh.IsAa();
    m_Topology = CSeq_inst::eTopology_not_set;
    m_Length = 0;

    m_IsDelta = false;
    m_IsVirtual = false;
    m_IsMap = false;

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
    }

    m_Title.clear();
    m_MolInfo.Reset();
    m_BioSource.Reset();
    m_Taxname.clear();
    m_Defline.clear();
    m_Dlflags = 0;

    m_Biomol = CMolInfo::eBiomol_unknown;
    m_Tech = CMolInfo::eTech_unknown;
    m_Completeness = CMolInfo::eCompleteness_unknown;

    m_ForceOnlyNearFeats = false;
    m_OnlyNearFeats = false;
}

// Descriptor collection (delayed until needed)
void CBioseqIndex::x_InitDescs (void)

{
    try {
        if (m_DescsInitialized) {
           return;
        }

        m_DescsInitialized = true;

        // explore descriptors, pass original target BioseqHandle if using Bioseq sublocation
        for (CSeqdesc_CI desc_it(m_OrigBsh); desc_it; ++desc_it) {
            const CSeqdesc& sd = *desc_it;
            CRef<CDescriptorIndex> sdx(new CDescriptorIndex(sd, *this));
            m_SdxList.push_back(sdx);

            switch (sd.Which()) {
                case CSeqdesc::e_Source:
                {
                    if (! m_BioSource) {
                        const CBioSource& biosrc = sd.GetSource();
                        m_BioSource.Reset (&biosrc);
                        if (biosrc.IsSetOrgname()) {
                            const COrg_ref& org = biosrc.GetOrg();
                            if (org.CanGetTaxname()) {
                                m_Taxname = org.GetTaxname();
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
                    }
                    break;
                }
                case CSeqdesc::e_Title:
                {
                    if (m_Title.empty()) {
                        m_Title = sd.GetTitle();
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
        if (m_ForceOnlyNearFeats) {
            m_OnlyNearFeats = true;
        }

        // initialization option can also prevent far feature exploration
        if (m_LocalFeatures) {
            m_OnlyNearFeats = true;
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
        if (m_FeatsInitialized) {
           return;
        }

        if (! m_DescsInitialized) {
            // initialize descriptors first to get m_OnlyNearFeats flag
            x_InitDescs();
        }

        m_FeatsInitialized = true;

        SAnnotSelector sel;

        // handle far fetching policy, from user object or initializer argument
        if (m_OnlyNearFeats) {

            if (m_Surrogate) {
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
        for (CFeat_CI feat_it(m_Bsh, sel); feat_it; ++feat_it) {
            const CMappedFeat mf = *feat_it;
            CSeq_feat_Handle hdl = mf.GetSeq_feat_Handle();

            const CSeq_feat& mpd = mf.GetMappedFeature();
            CConstRef<CSeq_loc> fl(&mpd.GetLocation());

            CRef<CFeatureIndex> sfx(new CFeatureIndex(hdl, mf, fl, *this));
            m_SfxList.push_back(sfx);

            m_FeatTree.AddFeature(mf);

            // CFeatIndex from CMappedFeat for use with GetBestGene
            m_FeatIndexMap[mf] = sfx;
        }
    }
    catch (CException& e) {
        LOG_POST(Error << "Error in CBioseqIndex::x_InitFeats: " << e.what());
    }
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

CConstRef<CBioSource> CBioseqIndex::GetBioSource (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_BioSource;
}

const string& CBioseqIndex::GetTaxname (void)

{
    if (! m_DescsInitialized) {
        x_InitDescs();
    }

    return m_Taxname;
}

// Run defline generator
const string& CBioseqIndex::GetDefline (sequence::CDeflineGenerator::TUserFlags flags)

{
    if (! m_FeatsInitialized) {
        x_InitFeats();
    }

    if (flags == m_Dlflags && ! m_Defline.empty()) {
        // Return previous result if flags are the same
        return m_Defline;
    }

    sequence::CDeflineGenerator gen (m_Tseh);

    // Pass original target BioseqHandle if using Bioseq sublocation
    m_Defline = gen.GenerateDefline (m_OrigBsh, m_FeatTree, flags);

    m_Dlflags = flags;

    return m_Defline;
}

CRef<CFeatureIndex> CBioseqIndex::GetFeatIndex (CMappedFeat mf)

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
                m_SeqVec->SetCoding(CBioseq_Handle::eCoding_Iupac);
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


// CDescriptorIndex

// Constructor
CDescriptorIndex::CDescriptorIndex (const CSeqdesc& sd,
                                    CBioseqIndex& bsx)
    : m_Sd(sd),
      m_Bsx(&bsx)
{
    m_Subtype = m_Sd.Which();
}


// CFeatureIndex

// Constructor
CFeatureIndex::CFeatureIndex (CSeq_feat_Handle sfh,
                              const CMappedFeat mf,
                              CConstRef<CSeq_loc> fl,
                              CBioseqIndex& bsx)
    : m_Sfh(sfh),
      m_Mf(mf),
      m_Fl(fl),
      m_Bsx(&bsx)
{
    m_Subtype = m_Mf.GetData().GetSubtype();
}

// Find CFeatureIndex object for best gene using internal CFeatTree
CRef<CFeatureIndex> CFeatureIndex::GetBestGene (void)

{
    try {
        CMappedFeat best;
        CWeakRef<CBioseqIndex> bsx = GetBioseqIndex();
        auto bsxl = bsx.Lock();
        if (bsxl) {
            best = feature::GetBestGeneForFeat(m_Mf, &bsxl->GetFeatTree(), 0,
                                               feature::CFeatTree::eBestGene_AllowOverlapped);
            if (best) {
                return bsxl->GetFeatIndex(best);
            }
        }
    } catch (CException& e) {
        LOG_POST(Error << "Error in CFeatureIndex::GetBestGene: " << e.what());
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
                m_SeqVec = new CSeqVector(*m_Fl, *bsxl->GetScope());
                if (m_SeqVec) {
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
    "am",
    "among",
    "an",
    "and",
    "another",
    "any",
    "anybody",
    "anyhow",
    "anyone",
    "anything",
    "anyway",
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
    "doing",
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
    "he",
    "her",
    "here",
    "him",
    "his",
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
    "me",
    "mg",
    "might",
    "ml",
    "mm",
    "most",
    "mostly",
    "must",
    "my",
    "myself",
    "nearly",
    "neither",
    "no",
    "none",
    "nor",
    "not",
    "now",
    "obtained",
    "of",
    "off",
    "often",
    "on",
    "or",
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
    "too",
    "two",
    "upon",
    "use",
    "used",
    "using",
    "various",
    "very",
    "was",
    "we",
    "went",
    "were",
    "what",
    "whatever",
    "whats",
    "when",
    "where",
    "which",
    "while",
    "who",
    "whom",
    "whose",
    "why",
    "will",
    "with",
    "within",
    "without",
    "would",
    "yet",
    "you",
    "your",
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
        if (*src == '<') {
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

    if (NStr::Find(str, "<") != NPOS) {
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
