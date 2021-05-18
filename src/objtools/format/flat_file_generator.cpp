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
* Author:  Mati Shomrat
*
* File Description:
*   User interface for generating flat file reports from ASN.1
*   
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/annot_ci.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/item_formatter.hpp>
#include <objtools/format/ostream_text_ostream.hpp>
#include <objtools/format/format_item_ostream.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/flat_expt.hpp>

#include <objects/misc/sequence_macros.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


//////////////////////////////////////////////////////////////////////////////
//
// PUBLIC

// constructor
CFlatFileGenerator::CFlatFileGenerator(const CFlatFileConfig& cfg) :
    m_Ctx(new CFlatFileContext(cfg))
{
    m_Failed = false;
     if ( !m_Ctx ) {
         NCBI_THROW(CFlatException, eInternal, "Unable to initialize context");
     }
}


CFlatFileGenerator::CFlatFileGenerator
(CFlatFileConfig::TFormat format,
 CFlatFileConfig::TMode   mode,
 CFlatFileConfig::TStyle  style,
 CFlatFileConfig::TFlags  flags,
 CFlatFileConfig::TView   view,
 CFlatFileConfig::TCustom custom,
 CFlatFileConfig::TPolicy policy) :
    m_Ctx(new CFlatFileContext(CFlatFileConfig(format, mode, style, flags, view, policy, custom)))
{
    m_Failed = false;
    if ( !m_Ctx ) {
       NCBI_THROW(CFlatException, eInternal, "Unable to initialize context");
    }
}


// destructor

CFlatFileGenerator::~CFlatFileGenerator(void)
{
}


// Set annotation selector for feature gathering

SAnnotSelector& CFlatFileGenerator::SetAnnotSelector(void)
{
    return m_Ctx->SetAnnotSelector();
}

void CFlatFileGenerator::SetFeatTree(feature::CFeatTree* tree)
{
    m_Ctx->SetFeatTree(tree);
}

void CFlatFileGenerator::SetSeqEntryIndex(CRef<CSeqEntryIndex> idx)
{
    m_Ctx->SetSeqEntryIndex(idx);
}

void CFlatFileGenerator::ResetSeqEntryIndex(void)
{
    m_Ctx->ResetSeqEntryIndex();
}



    /*
    template<typename _Pred>
    void VisitAllBioseqs(objects::CSeq_entry& entry, _Pred m)
    {
        if (entry.IsSeq())
        {
            m(entry.SetSeq());
        }
        else
            if (entry.IsSet())
            {
                NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it_se, entry.SetSet().SetSeq_set())
                {
                    VisitAllBioseqs(**it_se, m);
                }
            }
    }
    // also const visitor
    template<typename _Pred>
    void VisitAllBioseqs(const objects::CSeq_entry& entry, _Pred m)
    {
        if (entry.IsSeq())
        {
            m(entry.GetSeq());
        }
        else
            if (entry.IsSet())
            {
                ITERATE(CSeq_entry::TSet::TSeq_set, it_se, entry.GetSet().GetSeq_set())
                {
                    VisitAllBioseqs(**it_se, m);
                }
            }
    }

    template<typename _Pred>
    void VisitAllSeqSets(objects::CSeq_entry& entry, _Pred m)
    {
        if (entry.IsSet())
        {
            m(entry.SetSet());
            NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it_se, entry.SetSet().SetSeq_set())
            {
                VisitAllSeqSets(**it_se, m);
            }
        }
    }
    */
    // also const visitor
    template<typename _Pred>
    void VisitAllSeqSets(const objects::CSeq_entry& entry, _Pred m)
    {
        if (entry.IsSet())
        {
            m(entry.GetSet());
            ITERATE(CSeq_entry::TSet::TSeq_set, it_se, entry.GetSet().GetSeq_set())
            {
                VisitAllSeqSets(**it_se, m);
            }
        }
    }



// Generate a flat-file report for a Seq-entry
// (the other CFlatFileGenerator::Generate functions ultimately
// call this)
void CFlatFileGenerator::Generate
(const CSeq_entry_Handle& entry,
 CFlatItemOStream& item_os)
{
    _ASSERT(entry  &&  entry.Which() != CSeq_entry::e_not_set);

    if ( m_Ctx->GetConfig().BasicCleanup() )
    {

        entry.GetTopLevelEntry().GetCompleteObject();
        CSeq_entry_EditHandle tseh = entry.GetTopLevelEntry().GetEditHandle();
        CBioseq_set_EditHandle bseth;
        CBioseq_EditHandle bseqh;
        CRef<CSeq_entry> tmp_se(new CSeq_entry);

        if ( tseh.IsSet() ) {
            bseth = tseh.SetSet();
            CConstRef<CBioseq_set> bset = bseth.GetCompleteObject();
            bseth.Remove(bseth.eKeepSeq_entry);
            tmp_se->SetSet(const_cast<CBioseq_set&>(*bset));
        }
        else {
            bseqh = tseh.SetSeq();
            CConstRef<CBioseq> bseq = bseqh.GetCompleteObject();
            bseqh.Remove(bseqh.eKeepSeq_entry);
            tmp_se->SetSeq(const_cast<CBioseq&>(*bseq));
        }

        CCleanup cleanup;
        cleanup.BasicCleanup( *tmp_se );

        if ( tmp_se->IsSet() ) {
            tseh.SelectSet(bseth);
        }
        else {
            tseh.SelectSeq(bseqh);
        }
    }

    m_Ctx->SetSGS(false);
    CConstRef<CSeq_entry> topent = entry.GetTopLevelEntry().GetCompleteSeq_entry();
    if (topent && topent->IsSet()) {
        /*
        const CBioseq_set& topset = topent->GetSet();
        VISIT_ALL_SEQSETS_WITHIN_SEQSET (itr, topset) {
            const CBioseq_set& bss = *itr;
            if (bss.GetClass() == CBioseq_set::eClass_small_genome_set) {
                    m_Ctx->SetSGS(true);
            }
        }
        */
        VisitAllSeqSets(*topent, [this](const CBioseq_set& bss){
            if (bss.GetClass() == CBioseq_set::eClass_small_genome_set) {
                m_Ctx->SetSGS(true);
            }});
    }

    CRef<CFlatItemOStream> pItemOS( & item_os );
    // If there is a ICancel callback, wrap the item_os so
    // that every call checks it.
    const ICanceled * pCanceled = 
        m_Ctx->GetConfig().GetCanceledCallback();
    if( pCanceled ) {
        pItemOS.Reset( 
            new CCancelableFlatItemOStreamWrapper(
            item_os, pCanceled) );
    }

    /// archive a copy of the annot selector before we generate!
    SAnnotSelector sel = m_Ctx->SetAnnotSelector();
    m_Ctx->SetEntry(entry);

    if ( m_Ctx->GetConfig().UseSeqEntryIndexer() ) {
        // CSeq_entry& top = const_cast<CSeq_entry&> (*topent);
        CSeq_entry_Handle topseh = entry.GetTopLevelEntry();
        if (m_Ctx->UsingSeqEntryIndex()) {
            const CRef<CSeqEntryIndex> idx = m_Ctx->GetSeqEntryIndex();
            if (idx) {
                const CRef<CSeqMasterIndex>& midx = idx->GetMasterIndex();
                if (midx) {
                    if (midx->GetTopSEH() != topseh) {
                        m_Ctx->ResetSeqEntryIndex();
                    }
                }
            }
        }
        if (! m_Ctx->UsingSeqEntryIndex()) {
            try {
                CSeqEntryIndex::EPolicy policy = CSeqEntryIndex::eAdaptive;
                CSeqEntryIndex::TFlags flags = CSeqEntryIndex::fDefault;
                if ( m_Ctx->GetConfig().OnlyNearFeatures() ) {
                    policy = CSeqEntryIndex::eInternal;
                }
                if ( m_Ctx->GetConfig().HideSNPFeatures() ) {
                    flags |= CSeqEntryIndex::fHideSNPFeats;
                }
                if ( m_Ctx->GetConfig().HideCDDFeatures() ) {
                    flags |= CSeqEntryIndex::fHideCDDFeats;
                }
                if ( m_Ctx->GetConfig().ShowSNPFeatures() ) {
                    flags |= CSeqEntryIndex::fShowSNPFeats;
                }
                if ( m_Ctx->GetConfig().ShowCDDFeatures() ) {
                    flags |= CSeqEntryIndex::fShowCDDFeats;
                }
                if ( m_Ctx->GetConfig().HideExonFeatures() ) {
                    flags |= CSeqEntryIndex::fHideExonFeats;
                }
                if ( m_Ctx->GetConfig().HideIntronFeatures() ) {
                    flags |= CSeqEntryIndex::fHideIntronFeats;
                }
                if ( m_Ctx->GetConfig().HideMiscFeatures() ) {
                    flags |= CSeqEntryIndex::fHideMiscFeats;
                }
                if ( m_Ctx->GetConfig().GeneRNACDSFeatures() ) {
                    flags |= CSeqEntryIndex::fGeneRNACDSOnly;
                }
                if ( m_Ctx->GetConfig().IsPolicyInternal() ) {
                    policy = CSeqEntryIndex::eInternal;
                }
                if ( m_Ctx->GetConfig().IsPolicyExternal() ) {
                    policy = CSeqEntryIndex::eExternal;
                }
                if ( m_Ctx->GetConfig().IsPolicyExhaustive() ) {
                    policy = CSeqEntryIndex::eExhaustive;
                }
                if ( m_Ctx->GetConfig().IsPolicyFtp() ) {
                    policy = CSeqEntryIndex::eFtp;
                }
                if ( m_Ctx->GetConfig().IsPolicyWeb() ) {
                    policy = CSeqEntryIndex::eWeb;
                }
                CRef<CSeqEntryIndex> idx(new CSeqEntryIndex( topseh, policy, flags ));
                m_Ctx->SetSeqEntryIndex(idx);
                if (idx->IsIndexFailure()) {
                    m_Failed = true;
                    return;
                }
            } catch(CException &) {
                m_Failed = true;
                return;
            }
        }
    }


    bool onlyNearFeats = false;
    // bool nearFeatsSuppress = false;

    bool isNc = false;
    /*
    bool isNgNtNwNz = false;
    bool isGED = false;
    bool isTPA = false;
    */

    bool hasLocalFeat = false;
    bool forceOnlyNear = false;

    for (CBioseq_CI bi(entry); bi; ++bi) {
        const CBioseq_Handle& bh = *bi;
  
        const CBioseq& bsp = *(bi->GetCompleteBioseq());

        FOR_EACH_SEQID_ON_BIOSEQ (it, bsp) {
            const CSeq_id& sid = **it;
            switch (sid.Which()) {
                case CSeq_id::e_Genbank:
                case CSeq_id::e_Embl:
                case CSeq_id::e_Ddbj:
                    // isGED = true;
                    break;
                case CSeq_id::e_Tpg:
                case CSeq_id::e_Tpe:
                case CSeq_id::e_Tpd:
                    // isTPA = true;
                    break;
                case CSeq_id::e_Other:
                    {
                         const CTextseq_id* tsid = sid.GetTextseq_Id ();
                        if (tsid != NULL && tsid->IsSetAccession()) {
                            const string& acc = tsid->GetAccession().substr(0, 3);
                            if (acc == "NC_") {
                                isNc = true;
                            } else if (acc == "NG_" || acc == "NT_" || acc == "NW_" || acc == "NZ_") {
                                // isNgNtNwNz = true;
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        FOR_EACH_SEQDESC_ON_BIOSEQ (desc_it, bsp) {
            const CSeqdesc& desc = **desc_it;
            if (! desc.IsUser()) continue;
            if (! desc.GetUser().IsSetType()) continue;
            const CUser_object& usr = desc.GetUser();
            const CObject_id& oi = usr.GetType();
            if (! oi.IsStr()) continue;
            const string& type = oi.GetStr();
            if (! NStr::EqualNocase(type, "FeatureFetchPolicy")) continue;
            FOR_EACH_USERFIELD_ON_USEROBJECT (uitr, usr) {
                const CUser_field& fld = **uitr;
                if (FIELD_IS_SET_AND_IS(fld, Label, Str)) {
                    const string &label_str = GET_FIELD(fld.GetLabel(), Str);
                    if (! NStr::EqualNocase(label_str, "Policy")) continue;
                    if (fld.IsSetData() && fld.GetData().IsStr()) {
                        const string& str = fld.GetData().GetStr();
                        if (NStr::EqualNocase(str, "OnlyNearFeatures")) {
                            forceOnlyNear = true;
                        }
                    }
                }
            }
        }

        CSeq_annot_CI annot_ci(bh);
        for (; annot_ci; ++annot_ci) {
              const CSeq_annot_Handle& annt = *annot_ci;
              CConstRef<CSeq_annot> pAnnot = annt.GetCompleteSeq_annot();
              const CSeq_annot& antx = *pAnnot;
              FOR_EACH_SEQFEAT_ON_SEQANNOT (feat_it, antx) {
                  const CSeq_feat& sft = **feat_it;
                  const CSeqFeatData& data = sft.GetData();
                  CSeqFeatData::ESubtype subtype = data.GetSubtype();
                  if (isNc) {
                      switch (subtype) {
                          case CSeqFeatData::eSubtype_centromere:
                          case CSeqFeatData::eSubtype_telomere:
                          case CSeqFeatData::eSubtype_rep_origin:
                          case CSeqFeatData::eSubtype_region:
                              break;
                          default:
                              hasLocalFeat = true;
                              break;
                      }
                  } else {
                      hasLocalFeat = true;
                  }
              }
              if (hasLocalFeat) {
                break;
              }
         }
    }

    if (forceOnlyNear) {
        onlyNearFeats = true;
    /*
    } else if (isNc) {
        nearFeatsSuppress = true;
    } else if (isNgNtNwNz) {
        onlyNearFeats = true;
    } else if (isTPA) {
        onlyNearFeats = true;
    } else if (isGED) {
        nearFeatsSuppress = true;
    */
    }

    if (onlyNearFeats) {
        m_Ctx->SetAnnotSelector().SetResolveDepth(0);
    /*
    } else if (nearFeatsSuppress) {
        if (hasLocalFeat) {
            m_Ctx->SetAnnotSelector().SetResolveDepth(0);
        } else {
            m_Ctx->SetAnnotSelector().SetResolveDepth(1);
        }
    */
    } else {
        // m_Ctx->SetAnnotSelector().SetResolveDepth(1);
        m_Ctx->SetAnnotSelector().SetAdaptiveDepth(true);
    }


    CFlatFileConfig::TFormat format = m_Ctx->GetConfig().GetFormat();
    CRef<CFlatItemFormatter> formatter(CFlatItemFormatter::New(format));
    if ( !formatter ) {
        NCBI_THROW(CFlatException, eInternal, "Unable to initialize formatter");
    }
    formatter->SetContext(*m_Ctx);
    pItemOS->SetFormatter(formatter);

    CRef<CFlatGatherer> gatherer(CFlatGatherer::New(format));
    if ( !gatherer ) {
        NCBI_THROW(CFlatException, eInternal, "Unable to initialize gatherer");
    }
    gatherer->Gather(*m_Ctx, *pItemOS);

    /// reset the context, but preserve our selector
    /// we do this a bit oddly since resetting the context erases the selector;
    /// since the caller is reusing this object (most likely), we automatically
    /// restore the selector to its former glory
    m_Ctx->Reset();
    m_Ctx->SetAnnotSelector() = sel;

    /*
    if ( m_Ctx->GetConfig().UseSeqEntryIndexer() ) {
        m_Ctx->ResetSeqEntryIndex();
    }
    */
}


void CFlatFileGenerator::Generate
(const CSeq_submit& submit,
 CScope& scope,
 CFlatItemOStream& item_os)
{
    _ASSERT(submit.CanGetData());
    _ASSERT(submit.CanGetSub());
    _ASSERT(submit.GetData().IsEntrys());
    _ASSERT(!submit.GetData().GetEntrys().empty());

    // NB: though the spec specifies a submission may contain multiple entries
    // this is not the case. A submission should only have a single Top-level
    // Seq-entry
    CConstRef<CSeq_entry> e(submit.GetData().GetEntrys().front());
    if (e.NotEmpty()) {
        // get Seq_entry_Handle from scope
        CSeq_entry_Handle entry;
        try {
            entry = scope.GetSeq_entryHandle(*e);
        } catch (CException&) {}

        if (!entry) {  // add to scope if not already in it
            entry = scope.AddTopLevelSeqEntry(*e);
        }
        // "remember" the submission block
        m_Ctx->SetSubmit(submit.GetSub());

        Generate(entry, item_os);
    }
}


void CFlatFileGenerator::Generate
(const CBioseq& bioseq,
CScope& scope,
 CFlatItemOStream& item_os)
{
    const CBioseq_Handle bsh = scope.GetBioseqHandle(bioseq);
    const CSeq_entry_Handle entry = bsh.GetSeq_entry_Handle();
    Generate(entry, item_os);
}


void CFlatFileGenerator::Generate
(const CSeq_loc& loc,
 CScope& scope,
 CFlatItemOStream& item_os)
{
    CBioseq_Handle bsh = GetBioseqFromSeqLoc(loc, scope);
    if (!bsh) {
        NCBI_THROW(CFlatException, eInvalidParam, "location not in scope");
    }
    CSeq_entry_Handle entry = bsh.GetParentEntry();
    if (!entry) {
        NCBI_THROW(CFlatException, eInvalidParam, "Id not in scope");
    }
    CRef<CSeq_loc> location(new CSeq_loc);
    location->Assign(loc);
    m_Ctx->SetLocation(location);

    CFlatFileConfig& cfg = m_Ctx->SetConfig();
    if (cfg.IsStyleNormal()) {
        cfg.SetStyleMaster();
    }

    Generate(entry, item_os);
}


void CFlatFileGenerator::Generate
(const CBioseq_Handle& bsh,
 CFlatItemOStream& item_os)
{
    const CSeq_entry_Handle entry = bsh.GetSeq_entry_Handle();
    Generate(entry, item_os);
}


void CFlatFileGenerator::Generate
(const CSeq_id& id,
 const TRange& range,
 ENa_strand strand,
 CScope& scope,
 CFlatItemOStream& item_os)
{
    CRef<CSeq_id> id2(new CSeq_id);
    id2->Assign(id);
    CRef<CSeq_loc> loc;
    if ( range.IsWhole() ) {
        loc.Reset(new CSeq_loc);
        loc->SetWhole(*id2);
    } else {
        loc.Reset(new CSeq_loc(*id2, range.GetFrom(), range.GetTo(), strand));
    }
    if ( loc ) {
        Generate(*loc, scope, item_os);
    }
}


// This version iterates Bioseqs within the Bioseq_set
void CFlatFileGenerator::Generate
(const CSeq_entry_Handle& entry,
 CFlatItemOStream& item_os,
 bool useSeqEntryIndexing,
 CNcbiOstream* m_Os,
 CNcbiOstream* m_On,
 CNcbiOstream* m_Og,
 CNcbiOstream* m_Or,
 CNcbiOstream* m_Op,
 CNcbiOstream* m_Ou
 )
{
    // useSeqEntryIndexing argument also set by relevant flags in CFlatFileConfig
    if ( m_Ctx->GetConfig().UseSeqEntryIndexer() ) {
        useSeqEntryIndexing = true;
    }

    if (useSeqEntryIndexing) {
        m_Ctx->SetConfig().SetUseSeqEntryIndexer(true);
    } else {
        SetFeatTree(new feature::CFeatTree(entry));
    }

    _ASSERT(entry  &&  entry.Which() != CSeq_entry::e_not_set);

    const CFlatFileConfig& cfg = m_Ctx->GetConfig();

    bool doNuc = false;
    bool doProt = false;

    // doNuc and doProt arguments also set by relevant flags in CFlatFileConfig
    if ( cfg.IsViewNuc() ) {
        doNuc = true;
    }
    if ( cfg.IsViewProt() ) {
        doProt = true;
    }
    if ( cfg.IsViewAll() ) {
        doNuc = true;
        doProt = true;
    }

    if ( cfg.BasicCleanup() )
    {

        entry.GetTopLevelEntry().GetCompleteObject();
        CSeq_entry_EditHandle tseh = entry.GetTopLevelEntry().GetEditHandle();
        CBioseq_set_EditHandle bseth;
        CBioseq_EditHandle bseqh;
        CRef<CSeq_entry> tmp_se(new CSeq_entry);

        if ( tseh.IsSet() ) {
            bseth = tseh.SetSet();
            CConstRef<CBioseq_set> bset = bseth.GetCompleteObject();
            bseth.Remove(bseth.eKeepSeq_entry);
            tmp_se->SetSet(const_cast<CBioseq_set&>(*bset));
        }
        else {
            bseqh = tseh.SetSeq();
            CConstRef<CBioseq> bseq = bseqh.GetCompleteObject();
            bseqh.Remove(bseqh.eKeepSeq_entry);
            tmp_se->SetSeq(const_cast<CBioseq&>(*bseq));
        }

        CCleanup cleanup;
        cleanup.BasicCleanup( *tmp_se );

        if ( tmp_se->IsSet() ) {
            tseh.SelectSet(bseth);
        }
        else {
            tseh.SelectSeq(bseqh);
        }
    }

    m_Ctx->SetSGS(false);
    CConstRef<CSeq_entry> topent = entry.GetTopLevelEntry().GetCompleteSeq_entry();
    if (topent && topent->IsSet()) {
        /*
        const CBioseq_set& topset = topent->GetSet();
        VISIT_ALL_SEQSETS_WITHIN_SEQSET (itr, topset) {
            const CBioseq_set& bss = *itr;
            if (bss.GetClass() == CBioseq_set::eClass_small_genome_set) {
                    m_Ctx->SetSGS(true);
            }
        }
        */
        VisitAllSeqSets(*topent, [this](const CBioseq_set& bss){
            if (bss.GetClass() == CBioseq_set::eClass_small_genome_set) {
                m_Ctx->SetSGS(true);
            }});
    }

    // If there is a ICancel callback, wrap the item_os so
    // that every call checks it.
    const ICanceled * pCanceled = cfg.GetCanceledCallback();

    /// archive a copy of the annot selector before we generate!
    SAnnotSelector sel = m_Ctx->SetAnnotSelector();
    m_Ctx->SetEntry(entry);

    if ( cfg.UseSeqEntryIndexer() ) {
        // CSeq_entry& top = const_cast<CSeq_entry&> (*topent);
        CSeq_entry_Handle topseh = entry.GetTopLevelEntry();
        if (m_Ctx->UsingSeqEntryIndex()) {
            const CRef<CSeqEntryIndex> idx = m_Ctx->GetSeqEntryIndex();
            if (idx) {
                const CRef<CSeqMasterIndex>& midx = idx->GetMasterIndex();
                if (midx) {
                    if (midx->GetTopSEH() != topseh) {
                        m_Ctx->ResetSeqEntryIndex();
                    }
                }
            }
        }
        if (! m_Ctx->UsingSeqEntryIndex()) {
            try {
                CSeqEntryIndex::EPolicy policy = CSeqEntryIndex::eAdaptive;
                CSeqEntryIndex::TFlags flags = CSeqEntryIndex::fDefault;
                if ( cfg.OnlyNearFeatures() ) {
                    policy = CSeqEntryIndex::eInternal;
                }
                if ( cfg.HideSNPFeatures() ) {
                    flags |= CSeqEntryIndex::fHideSNPFeats;
                } else if ( cfg.ShowSNPFeatures() ) {
                    flags |= CSeqEntryIndex::fShowSNPFeats;
                }
                if ( cfg.HideCDDFeatures() ) {
                    flags |= CSeqEntryIndex::fHideCDDFeats;
                } else if ( cfg.ShowCDDFeatures() ) {
                    flags |= CSeqEntryIndex::fShowCDDFeats;
                }
                if ( cfg.IsPolicyInternal() ) {
                    policy = CSeqEntryIndex::eInternal;
                }
                if ( cfg.IsPolicyExternal() ) {
                    policy = CSeqEntryIndex::eExternal;
                }
                if ( cfg.IsPolicyExhaustive() ) {
                    policy = CSeqEntryIndex::eExhaustive;
                }
                if ( cfg.IsPolicyFtp() ) {
                    policy = CSeqEntryIndex::eFtp;
                }
                if ( cfg.IsPolicyWeb() ) {
                    policy = CSeqEntryIndex::eWeb;
                }
                CRef<CSeqEntryIndex> idx(new CSeqEntryIndex( topseh, policy, flags));
                m_Ctx->SetSeqEntryIndex(idx);
                if (idx->IsIndexFailure()) {
                    m_Failed = true;
                    return;
                }
                int featDepth = cfg.GetFeatDepth();
                idx->SetFeatDepth(featDepth);
                int gapDepth = cfg.GetGapDepth();
                idx->SetGapDepth(gapDepth);
            } catch(CException &) {
                m_Failed = true;
                return;
            }
        }
    }

    CFlatFileConfig::TFormat format = cfg.GetFormat();
    CRef<CFlatItemFormatter> formatter(CFlatItemFormatter::New(format));
    if ( !formatter ) {
        NCBI_THROW(CFlatException, eInternal, "Unable to initialize formatter");
    }
    formatter->SetContext(*m_Ctx);

    // internal Bioseq iterator loop moved up from x_GatherSeqEntry
    for (CBioseq_CI bioseq_it(entry);  bioseq_it;  ++bioseq_it) {
        CBioseq_Handle bsh = *bioseq_it;
        if (! bsh) continue;
        const CSeq_entry_Handle ent = bsh.GetSeq_entry_Handle();
        CConstRef<CBioseq> bsr = bsh.GetCompleteBioseq();

        CNcbiOstream* flatfile_os = nullptr;

        bool is_genomic = false;
        bool is_RNA = false;

        CConstRef<CSeqdesc> closest_molinfo = bsr->GetClosestDescriptor(CSeqdesc::e_Molinfo);
        if (closest_molinfo) {
            const CMolInfo& molinf = closest_molinfo->GetMolinfo();
            CMolInfo::TBiomol biomol = molinf.GetBiomol();
            switch (biomol) {
                case NCBI_BIOMOL(genomic):
                case NCBI_BIOMOL(other_genetic):
                case NCBI_BIOMOL(genomic_mRNA):
                case NCBI_BIOMOL(cRNA):
                    is_genomic = true;
                    break;
                case NCBI_BIOMOL(pre_RNA):
                case NCBI_BIOMOL(mRNA):
                case NCBI_BIOMOL(rRNA):
                case NCBI_BIOMOL(tRNA):
                case NCBI_BIOMOL(snRNA):
                case NCBI_BIOMOL(scRNA):
                case NCBI_BIOMOL(snoRNA):
                case NCBI_BIOMOL(transcribed_RNA):
                case NCBI_BIOMOL(ncRNA):
                case NCBI_BIOMOL(tmRNA):
                    is_RNA = true;
                    break;
                case NCBI_BIOMOL(other):
                    {
                        CBioseq_Handle::TMol mol = bsh.GetSequenceType();
                        switch (mol) {
                             case CSeq_inst::eMol_dna:
                                is_genomic = true;
                                break;
                            case CSeq_inst::eMol_rna:
                                is_RNA = true;
                                break;
                            case CSeq_inst::eMol_na:
                                is_genomic = true;
                                break;
                            default:
                                break;
                       }
                   }
                   break;
                default:
                    break;
            }
        }

        if ( m_Os ) {
            flatfile_os = m_Os;
            if ( bsh.IsNa() && ! doNuc ) continue;
            if ( bsh.IsAa() && ! doProt ) continue;
        } else if ( bsh.IsNa() ) {
            if ( m_On ) {
                flatfile_os = m_On;
            } else if ( (is_genomic || ! closest_molinfo) && m_Og ) {
                flatfile_os = m_Og;
            } else if ( is_RNA && m_Or ) {
                flatfile_os = m_Or;
            } else {
                continue;
            }
        } else if ( bsh.IsAa() ) {
            if ( m_Op ) {
                flatfile_os = m_Op;
            }
        } else {
            if ( m_Ou ) {
                flatfile_os = m_Ou;
            } else if ( m_On ) {
                flatfile_os = m_On;
            } else {
                continue;
            }
        }

        if ( !flatfile_os ) continue;

        CRef<CFlatItemOStream> newitem_os(new CFormatItemOStream(new COStreamTextOStream(*flatfile_os)));

        CRef<CFlatItemOStream> pItemOS( newitem_os );
        if( pCanceled ) {
            pItemOS.Reset( 
                new CCancelableFlatItemOStreamWrapper(
                *newitem_os, pCanceled) );
        }

        pItemOS->SetFormatter(formatter);

        CRef<CFlatGatherer> gatherer(CFlatGatherer::New(format));
        if ( !gatherer ) {
            NCBI_THROW(CFlatException, eInternal, "Unable to initialize gatherer");
        }

        gatherer->Gather(*m_Ctx, *pItemOS, ent, bsh, useSeqEntryIndexing, doNuc, doProt);
    }

    /// reset the context, but preserve our selector
    /// we do this a bit oddly since resetting the context erases the selector;
    /// since the caller is reusing this object (most likely), we automatically
    /// restore the selector to its former glory
    m_Ctx->Reset();
    m_Ctx->SetAnnotSelector() = sel;

    /*
    if ( m_Ctx->GetConfig().UseSeqEntryIndexer() ) {
        m_Ctx->ResetSeqEntryIndex();
    }
    */
}


void CFlatFileGenerator::Generate
(const CBioseq_Handle& bsh,
 CNcbiOstream& os,
 bool useSeqEntryIndexing,
 CNcbiOstream* m_Os,
 CNcbiOstream* m_On,
 CNcbiOstream* m_Og,
 CNcbiOstream* m_Or,
 CNcbiOstream* m_Op,
 CNcbiOstream* m_Ou
 )
{
    CRef<CFlatItemOStream>
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    const CSeq_entry_Handle entry = bsh.GetSeq_entry_Handle();
    Generate(entry, *item_os, useSeqEntryIndexing, m_Os, m_On, m_Og, m_Or, m_Op, m_Ou);

}


void CFlatFileGenerator::Generate
(const CSeq_submit& submit,
 CScope& scope,
 CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(submit, scope, *item_os);
}


void CFlatFileGenerator::Generate
(const CBioseq& bioseq,
 CScope& scope,
 CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    const CBioseq_Handle bsh = scope.GetBioseqHandle(bioseq);
    const CSeq_entry_Handle entry = bsh.GetSeq_entry_Handle();
    Generate(entry, *item_os);
}


void CFlatFileGenerator::Generate
(const CSeq_entry_Handle& entry,
 CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(entry, *item_os);
}


void CFlatFileGenerator::Generate
(const CBioseq_Handle& bsh,
 CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    const CSeq_entry_Handle entry = bsh.GetSeq_entry_Handle();
    Generate(entry, *item_os);
}


void CFlatFileGenerator::Generate
(const CSeq_loc& loc,
 CScope& scope,
 CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(loc, scope, *item_os);
}


void CFlatFileGenerator::Generate
(const CSeq_id& id,
 const TRange& range, 
 ENa_strand strand,
 CScope& scope,
 CNcbiOstream& os)
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(id, range, strand, scope, *item_os);
}


void CFlatFileGenerator::Generate
(const CSeq_entry_Handle& entry,
 CNcbiOstream& os,
 bool useSeqEntryIndexing,
 CNcbiOstream* m_Os,
 CNcbiOstream* m_On,
 CNcbiOstream* m_Og,
 CNcbiOstream* m_Or,
 CNcbiOstream* m_Op,
 CNcbiOstream* m_Ou
 )
{
    CRef<CFlatItemOStream> 
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(entry, *item_os, useSeqEntryIndexing, m_Os, m_On, m_Og, m_Or, m_Op, m_Ou);
}

void CFlatFileGenerator::Generate
(const CSeq_loc& loc,
 CScope& scope,
 CNcbiOstream& os,
 bool useSeqEntryIndexing,
 CNcbiOstream* m_Os,
 CNcbiOstream* m_On,
 CNcbiOstream* m_Og,
 CNcbiOstream* m_Or,
 CNcbiOstream* m_Op,
 CNcbiOstream* m_Ou
 )
{
    CBioseq_Handle bsh = GetBioseqFromSeqLoc(loc, scope);
    if (!bsh) {
        NCBI_THROW(CFlatException, eInvalidParam, "location not in scope");
    }
    CSeq_entry_Handle entry = bsh.GetParentEntry();
    if (!entry) {
        NCBI_THROW(CFlatException, eInvalidParam, "Id not in scope");
    }
    CRef<CSeq_loc> location(new CSeq_loc);
    location->Assign(loc);
    m_Ctx->SetLocation(location);

    CFlatFileConfig& cfg = m_Ctx->SetConfig();
    if (cfg.IsStyleNormal()) {
        cfg.SetStyleMaster();
    }

    Generate(entry, os, useSeqEntryIndexing, m_Os, m_On, m_Og, m_Or, m_Op, m_Ou);
}



//void CFlatFileGenerator::Reset(void)
//{
//    m_Ctx->Reset();
//}


string CFlatFileGenerator::GetSeqFeatText
(const CMappedFeat& feat,
 CScope& scope,
 const CFlatFileConfig& cfg)
{
    CBioseq_Handle seq = sequence::GetBioseqFromSeqLoc(feat.GetLocation(), scope);
    if (!seq) {
        NCBI_THROW(CFlatException, eUnknown, "Bioseq not found for feature");
    }
    CRef<CFlatItemFormatter> formatter(CFlatItemFormatter::New(cfg.GetFormat()));
    CRef<CFlatFileContext> ctx(new CFlatFileContext(cfg));

    ctx->SetEntry(seq.GetParentEntry());
    formatter->SetContext(*ctx);

    CConn_MemoryStream os;
    CFormatItemOStream item_os(new COStreamTextOStream(os));
    item_os.SetFormatter(formatter);

    CBioseqContext bctx(seq, *ctx);

    CSeq_entry_Handle tseh = seq.GetTopLevelEntry();
    CFeat_CI iter (tseh);
    CRef<feature::CFeatTree> ftree;
    ftree.Reset (new feature::CFeatTree (iter));

    CConstRef<IFlatItem> item;
    if (feat.GetData().IsBiosrc()) {
        item.Reset( new CSourceFeatureItem(feat, bctx, ftree, &feat.GetLocation()) );
        item_os << item;
    } else {
        item.Reset( new CFeatureItem(feat, bctx, ftree, &feat.GetLocation()) );
        item_os << item;
    }

    string text;
    os.ToString(&text);
    return text;
}

void CFlatFileGenerator::x_GetLocation
(const CSeq_entry_Handle& entry,
 TSeqPos from,
 TSeqPos to,
 ENa_strand strand,
 CSeq_loc& loc)
{
    _ASSERT(entry);

    CBioseq_Handle h = x_DeduceTarget(entry);
    if ( !h ) {
        NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
    }
    TSeqPos length = h.GetInst_Length();

    if ( from == CRange<TSeqPos>::GetWholeFrom()  &&  to == length ) {
        // whole
        loc.SetWhole().Assign(*h.GetSeqId());
    } else {
        // interval
        loc.SetInt().SetId().Assign(*h.GetSeqId());
        loc.SetInt().SetFrom(from);
        loc.SetInt().SetTo(to);
        if ( strand > 0 ) {
            loc.SetInt().SetStrand(strand);
        }
    }
}

// if the 'from' or 'to' flags are specified try to guess the bioseq.
CBioseq_Handle CFlatFileGenerator::x_DeduceTarget(const CSeq_entry_Handle& entry)
{
    if ( entry.IsSeq() ) {
        return entry.GetSeq();
    }

    _ASSERT(entry.IsSet());
    CBioseq_set_Handle bsst = entry.GetSet();
    if ( !bsst.IsSetClass() ) {
        NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
    }
    _ASSERT(bsst.IsSetClass());
    switch ( bsst.GetClass() ) {
    case CBioseq_set::eClass_nuc_prot:
        // return the nucleotide
        for ( CSeq_entry_CI it(entry); it; ++it ) {
            if ( it->IsSeq() ) {
                CBioseq_Handle h = it->GetSeq();
                if ( h  &&  CSeq_inst::IsNa(h.GetInst_Mol()) ) {
                    return h;
                }
            }
        }
        break;
    case CBioseq_set::eClass_gen_prod_set:
        // return the genomic
        for ( CSeq_entry_CI it(bsst); it; ++it ) {
            if ( it->IsSeq()  &&
                 it->GetSeq().GetInst_Mol() == CSeq_inst::eMol_dna ) {
                 return it->GetSeq();
            }
        }
        break;
    case CBioseq_set::eClass_segset:
        // return the segmented bioseq
        for ( CSeq_entry_CI it(bsst); it; ++it ) {
            if ( it->IsSeq() ) {
                return it->GetSeq();
            }
        }
        break;
    case CBioseq_set::eClass_genbank:
        {
            CBioseq_CI bi(bsst, CSeq_inst::eMol_na);
            if (bi) {
                return *bi;
            }
        }
        break;
    default:
        break;
    }
    NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
}

void 
CFlatFileGenerator::CCancelableFlatItemOStreamWrapper::SetFormatter(
    IFormatter* formatter)
{
    CFlatItemOStream::SetFormatter(formatter);
    m_pUnderlying->SetFormatter(formatter);
}

void
CFlatFileGenerator::CCancelableFlatItemOStreamWrapper::AddItem(
    CConstRef<IFlatItem> item) 
{
    if( m_pCanceledCallback && m_pCanceledCallback->IsCanceled() ) {
        NCBI_THROW(CFlatException, eHaltRequested, 
            "FlatFileGeneration canceled by ICancel callback");
    }
    m_pUnderlying->AddItem(item);
}

void CFlatFileGenerator::SetConfig(const CFlatFileConfig& cfg)
{
    m_Ctx->SetConfig(cfg);
}

END_SCOPE(objects)
END_NCBI_SCOPE
