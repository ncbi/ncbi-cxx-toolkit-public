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
 CFlatFileConfig::TCustom custom) :
    m_Ctx(new CFlatFileContext(CFlatFileConfig(format, mode, style, flags, view, custom)))
{
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


    bool onlyNearFeats = false;
    bool nearFeatsSuppress = false;

    bool isNc = false;
    bool isNgNtNwNz = false;
    bool isGED = false;
    bool isTPA = false;

    bool hasLocalFeat = false;
    bool forceOnlyNear = false;

    for (CBioseq_CI bi(entry); bi; ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bi->GetCompleteBioseq())) {
            const CSeq_id& sid = **it;
            switch (sid.Which()) {
                case CSeq_id::e_Genbank:
                case CSeq_id::e_Embl:
                case CSeq_id::e_Ddbj:
                    isGED = true;
                    break;
                case CSeq_id::e_Tpg:
                case CSeq_id::e_Tpe:
                case CSeq_id::e_Tpd:
                    isTPA = true;
                    break;
                case CSeq_id::e_Other:
                    {
                         const CTextseq_id* tsid = sid.GetTextseq_Id ();
                        if (tsid != NULL && tsid->IsSetAccession()) {
                            const string& acc = tsid->GetAccession().substr(0, 3);
                            if (acc == "NC_") {
                                isNc = true;
                            } else if (acc == "NG_" || acc == "NT_" || acc == "NW_" || acc == "NZ_") {
                                isNgNtNwNz = true;
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        FOR_EACH_SEQANNOT_ON_BIOSEQ (annot_it, *(bi->GetCompleteBioseq())) {
            FOR_EACH_SEQFEAT_ON_SEQANNOT (feat_it, **annot_it) {
                const CSeq_feat& sft = **feat_it;
                const CSeqFeatData& data = sft.GetData();
                CSeqFeatData::ESubtype subtype = data.GetSubtype();
                if (isNc) {
                    switch (subtype) {
                        case CSeqFeatData::eSubtype_centromere:
                        case CSeqFeatData::eSubtype_telomere:
                        case CSeqFeatData::eSubtype_rep_origin:
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

        FOR_EACH_SEQDESC_ON_BIOSEQ (desc_it, *(bi->GetCompleteBioseq())) {
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
    }

    if (forceOnlyNear) {
        onlyNearFeats = true;
    } else if (isNc) {
        nearFeatsSuppress = true;
    } else if (isNgNtNwNz) {
        onlyNearFeats = true;
    } else if (isTPA) {
        onlyNearFeats = true;
    } else if (isGED) {
        nearFeatsSuppress = true;
    }

    if (onlyNearFeats) {
        m_Ctx->SetAnnotSelector().SetResolveDepth(0);
    } else if (nearFeatsSuppress) {
        if (hasLocalFeat) {
            m_Ctx->SetAnnotSelector().SetResolveDepth(0);
        } else {
            m_Ctx->SetAnnotSelector().SetResolveDepth(1);
        }
    } else {
        m_Ctx->SetAnnotSelector().SetResolveDepth(1);
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
}


void CFlatFileGenerator::Generate
(CSeq_submit& submit,
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
    CRef<CSeq_entry> e(submit.SetData().SetEntrys().front());
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

    CFlatFileConfig& cfg = m_Ctx->GetConfig();
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


void CFlatFileGenerator::Generate
(CSeq_submit& submit,
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

END_SCOPE(objects)
END_NCBI_SCOPE
