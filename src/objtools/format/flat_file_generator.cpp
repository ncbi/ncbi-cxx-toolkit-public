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
#include <corelib/ncbitime.hpp>
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
#include <objmgr/util/objutil.hpp>

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


const int kInferenceCutoff = 1000;

static void x_InferenceInSeqs (const CSeq_entry& sep, int *numInferences)

{
    if (*numInferences >= kInferenceCutoff) {
        return;
    }
    if (sep.IsSeq()) {
        const CBioseq& bsp = sep.GetSeq();
        if (bsp.IsSetAnnot()) {
            for (auto& annt : bsp.GetAnnot()) {
                if (! annt->IsFtable()) {
                    continue;
                }
                for (auto& feat : annt->GetData().GetFtable()) {
                    if (! feat->IsSetQual()) {
                        continue;
                    }
                    for (auto& qual : feat->GetQual()) {
                        if (qual->IsSetQual() && NStr::EqualNocase(qual->GetQual(), "inference")) {
                            (*numInferences)++;
                            if (*numInferences >= kInferenceCutoff) {
                                return;
                            }
                        }
                    }
                }
            }
        }
    } else if (sep.IsSet()) {
        const CBioseq_set& bssp = sep.GetSet();
        for (auto& seqentry : bssp.GetSeq_set()) {
            x_InferenceInSeqs(*seqentry, numInferences);
            if (*numInferences >= kInferenceCutoff) {
                return;
            }
            if (bssp.IsSetAnnot()) {
                for (auto& annt : bssp.GetAnnot()) {
                    if (! annt->IsFtable()) {
                        continue;
                    }
                    for (auto& feat : annt->GetData().GetFtable()) {
                        if (! feat->IsSetQual()) {
                            continue;
                        }
                        for (auto& qual : feat->GetQual()) {
                            if (qual->IsSetQual() && NStr::EqualNocase(qual->GetQual(), "inference")) {
                                (*numInferences)++;
                                if (*numInferences >= kInferenceCutoff) {
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

bool CFlatFileGenerator::HasInference(const CSeq_entry_Handle& topseh)
{
    CSeq_entry_Handle tseh = topseh.GetTopLevelEntry();
    CConstRef<CSeq_entry> tcsep = tseh.GetCompleteSeq_entry();
    const CSeq_entry& topsep = const_cast<CSeq_entry&>(*tcsep);

    int numInferences = 0;

    x_InferenceInSeqs( topsep, &numInferences );

    if (numInferences >= kInferenceCutoff) {
        return true;
    }
    return false;
}


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


// This version iterates Bioseqs within the Bioseq_set
void CFlatFileGenerator::Generate(
    const CSeq_entry_Handle& entry,
    CFlatItemOStream& item_os,
    const multiout& mo)
{
    // useSeqEntryIndexing now turned ON by default
    bool useSeqEntryIndexing = true;

    /*
    // useSeqEntryIndexing argument also set by relevant flags in CFlatFileConfig
    if ( m_Ctx->GetConfig().UseSeqEntryIndexer() ) {
        useSeqEntryIndexing = true;
    }
    */

    // Setting the -custom 1048576 bit flag turns OFF default indexing
    if ( m_Ctx->GetConfig().DisableDefaultIndex() ) {
        useSeqEntryIndexing = false;
        m_Ctx->SetConfig().SetUseSeqEntryIndexer(false);
    }

    if (useSeqEntryIndexing) {
        m_Ctx->SetConfig().SetUseSeqEntryIndexer(true);
    } else {
        SetFeatTree(new feature::CFeatTree(entry));
    }

    _ASSERT(entry  &&  entry.Which() != CSeq_entry::e_not_set);

    const CFlatFileConfig& cfg = m_Ctx->GetConfig();

    bool showDebugTiming = cfg.ShowDebugTiming();

    CStopWatch sw;

    bool doNuc = false;
    bool doProt = false;
    bool doFastSets = false;

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
    if ( cfg.FasterReleaseSets() ) {
        doFastSets = true;
    }

    if ( cfg.BasicCleanup() && ! HasInference(entry) )
    {
        if (showDebugTiming) {
            sw.Start();
        }

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
        Uint4 options = CCleanup::eClean_ForFlatfile;
        cleanup.BasicCleanup( *tmp_se, options );

        if ( tmp_se->IsSet() ) {
            tseh.SelectSet(bseth);
        }
        else {
            tseh.SelectSeq(bseqh);
        }

        if (showDebugTiming) {
            NcbiCerr << "Cleanup: " << sw.Elapsed() << ", ";
            sw.Reset();
        }
    }

    if (showDebugTiming) {
        sw.Start();
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
                if ( cfg.OnlyNearFeatures() && ! ( cfg.IsPolicyFtp() || cfg.IsPolicyGenomes() ) ) {
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
                if ( cfg.IsPolicyGenomes() ) {
                    policy = CSeqEntryIndex::eGenomes;
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

    if (showDebugTiming) {
        // NcbiCerr << "Prepare: " << sw.Elapsed() << ", ";
        sw.Reset();
    }

    const string accn_filt = cfg.GetSingleAccession();

    // internal Bioseq iterator loop moved up from x_GatherSeqEntry
    for (CBioseq_CI bioseq_it(entry);  bioseq_it;  ++bioseq_it) {

        if (showDebugTiming) {
            sw.Start();
        }

        CBioseq_Handle bsh = *bioseq_it;
        if (! bsh) continue;

        if ( ! accn_filt.empty()) {
            bool okay = false;
            const CBioseq& bsp = *bsh.GetBioseqCore();
            for (auto& sid : bsp.GetId()) {
                switch (sid->Which()) {
                    case NCBI_SEQID(Genbank):
                    case NCBI_SEQID(Embl):
                    case NCBI_SEQID(Ddbj):
                    case NCBI_SEQID(Tpg):
                    case NCBI_SEQID(Tpe):
                    case NCBI_SEQID(Tpd):
                    case NCBI_SEQID(Local):
                    case NCBI_SEQID(General):
                    case NCBI_SEQID(Other):
                    case NCBI_SEQID(Gpipe):
                    {
                        const string accn_string = sid->GetSeqIdString();
                        if ( accn_filt == accn_string ) {
                            okay = true;
                        }
                        const string fasta_str = sid->AsFastaString();
                        if ( accn_filt == fasta_str ) {
                            okay = true;
                        }
                        break;
                    }
                     default:
                       break;
                }
            }
            if ( ! okay) {
                continue;
            }
        }

        CSeq_id_Handle sidh = bsh.GetAccessSeq_id_Handle();
        if (showDebugTiming) {
            NcbiCerr << "Accession: " << sidh << ", ";
        }
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

        if (mo.m_Os) {
            flatfile_os = mo.m_Os;
            if ( bsh.IsNa() && ! doNuc ) continue;
            if ( bsh.IsAa() && ! doProt ) continue;
        } else if ( bsh.IsNa() ) {
            if (mo.m_On) {
                flatfile_os = mo.m_On;
            } else if ((is_genomic || ! closest_molinfo) && mo.m_Og) {
                flatfile_os = mo.m_Og;
            } else if (is_RNA && mo.m_Or) {
                flatfile_os = mo.m_Or;
            }
        } else if ( bsh.IsAa() ) {
            if (mo.m_Op) {
                flatfile_os = mo.m_Op;
            }
        } else {
            if (mo.m_Ou) {
                flatfile_os = mo.m_Ou;
            } else if (mo.m_On) {
                flatfile_os = mo.m_On;
            }
        }

        CRef<CFlatItemOStream> newitem_os;
        if (! flatfile_os) {
            if (! mo) {
                if ( bsh.IsNa() && ! doNuc ) continue;
                if ( bsh.IsAa() && ! doProt ) continue;
                newitem_os.Reset(&item_os);
            } else {
                continue;
            }
        } else {
            newitem_os.Reset(
                new CFormatItemOStream(new COStreamTextOStream(*flatfile_os)));
        }
        if (newitem_os.Empty()) continue;

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

        gatherer->Gather(*m_Ctx, *pItemOS, ent, bsh, useSeqEntryIndexing, doNuc, doProt, doFastSets);

        if (showDebugTiming) {
            NcbiCerr << "Elapsed: " << sw.Elapsed() << NcbiEndl;
            sw.Reset();
        }
    }

    /*
    if ( m_Ctx->GetConfig().UseSeqEntryIndexer() ) {
        m_Ctx->ResetSeqEntryIndex();
    }
    */

    /// reset the context, but preserve our selector
    /// we do this a bit oddly since resetting the context erases the selector;
    /// since the caller is reusing this object (most likely), we automatically
    /// restore the selector to its former glory
    m_Ctx->Reset();
    m_Ctx->SetAnnotSelector() = sel;
}


void CFlatFileGenerator::Generate(
    const CSeq_entry_Handle& entry,
    CNcbiOstream& os,
    const multiout& mo)
{
    CRef<CFlatItemOStream>
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    Generate(entry, *item_os, mo);
}


void CFlatFileGenerator::Generate(
    const CBioseq_Handle& bsh,
    CNcbiOstream& os,
    const multiout& mo)
{
    CRef<CFlatItemOStream>
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    const CSeq_entry_Handle entry = bsh.GetSeq_entry_Handle();
    Generate(entry, *item_os, mo);
}


void CFlatFileGenerator::Generate(
    const CSeq_submit& submit,
    CScope& scope,
    CNcbiOstream& os,
    const multiout& mo)
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

        CRef<CFlatItemOStream>
            item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

        Generate(entry, *item_os, mo);
    }
}


void CFlatFileGenerator::Generate(
    const CBioseq& bioseq,
    CScope& scope,
    CNcbiOstream& os,
    const multiout& mo)
{
    CRef<CFlatItemOStream>
        item_os(new CFormatItemOStream(new COStreamTextOStream(os)));

    const CBioseq_Handle bsh = scope.GetBioseqHandle(bioseq);
    const CSeq_entry_Handle entry = bsh.GetSeq_entry_Handle();
    Generate(entry, *item_os, mo);
}


void CFlatFileGenerator::Generate(
    const CSeq_loc& loc,
    CScope& scope,
    CNcbiOstream& os,
    const multiout& mo)
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

    Generate(entry, os, mo);
}


void CFlatFileGenerator::Generate(
    const CSeq_id& id,
    const TRange& range,
    ENa_strand strand,
    CScope& scope,
    CNcbiOstream& os,
    const multiout& mo)
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
        Generate(*loc, scope, os, mo);
    }
}



//void CFlatFileGenerator::Reset(void)
//{
//    m_Ctx->Reset();
//}


string CFlatFileGenerator::GetSeqFeatText
(const CMappedFeat& feat,
 CScope& scope,
 const CFlatFileConfig& cfg,
 CRef<feature::CFeatTree> ftree)
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

    if (ftree.Empty()) {
        CSeq_entry_Handle tseh = seq.GetTopLevelEntry();
        CFeat_CI iter (tseh);
        ftree.Reset (new feature::CFeatTree (iter));
    }

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


string CFlatFileGenerator::GetFTableAnticodonText(
        const CTrna_ext& trna_ext,
        CBioseqContext& ctx)
{
    if (!trna_ext.IsSetAnticodon()) {
        return kEmptyStr;
    }

    const auto& loc = trna_ext.GetAnticodon();
    string pos = CFlatSeqLoc(loc, ctx).GetString();

    string aa;
    switch(trna_ext.GetAa().Which()) {
    case CTrna_ext::C_Aa::e_Iupacaa:
        aa = GetAAName(trna_ext.GetAa().GetIupacaa(), true);
        break;
    case CTrna_ext::C_Aa::e_Ncbieaa:
        aa = GetAAName(trna_ext.GetAa().GetNcbieaa(), true);
        break;
    case CTrna_ext::C_Aa::e_Ncbi8aa:
        aa = GetAAName(trna_ext.GetAa().GetNcbi8aa(), false);
        break;
    case CTrna_ext::C_Aa::e_Ncbistdaa:
        aa = GetAAName(trna_ext.GetAa().GetNcbistdaa(), false);
        break;
    default:
        break;
    }

    string seq("---");
    try {
        CSeqVector seq_vec(loc, ctx.GetScope(), CBioseq_Handle::eCoding_Iupac);
        seq_vec.GetSeqData(0, 3, seq);
        NStr::ToLower(seq);
    }
    catch(...)
    {}

    return "(pos:" + pos + ",aa:" + aa + ",seq:" + seq + ")";
}


string CFlatFileGenerator::GetFTableAnticodonText(
        const CSeq_feat& feat,
        CScope& scope)
{
    const CSeqFeatData& data = feat.GetData();
    CSeqFeatData::E_Choice type = data.Which();

    if (type != CSeqFeatData::e_Rna) {
        return kEmptyStr;
    }

    const CRNA_ref& rna = data.GetRna();

    if (! rna.IsSetType()) {
        return kEmptyStr;
    }

    CRNA_ref::TType rna_type = rna.GetType();
    if (rna_type != CRNA_ref::eType_tRNA) {
        return kEmptyStr;
    }

    if (! rna.IsSetExt()) {
        return kEmptyStr;
    }

    const CRNA_ref::C_Ext& ext = rna.GetExt();
    if (ext.Which() != CRNA_ref::C_Ext::e_TRNA) {
        return kEmptyStr;
    }

    const CTrna_ext& trna = ext.GetTRNA();

    const CSeq_loc& loc = feat.GetLocation();
    CBioseq_Handle bsh = GetBioseqFromSeqLoc(loc, scope);

    CRef<CFlatFileContext> ffc(new CFlatFileContext(CFlatFileConfig()));
    CBioseqContext ctx(CBioseqContext(bsh, *ffc));

    return CFlatFileGenerator::GetFTableAnticodonText(trna, ctx);
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
