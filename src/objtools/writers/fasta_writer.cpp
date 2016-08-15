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
 * Authors:  Sergiy Gotvyanskyy, Justin Foley
 *
 * File Description:  Write object as a hierarchy of FASTA objects
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/writers/fasta_writer.hpp>

#include <corelib/ncbifile.hpp>

#include <objmgr/util/sequence.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/context.hpp>
#include <objtools/format/items/flat_seqloc.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/writer_exception.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sequtil/sequtil.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);


CFastaOstreamEx::CFastaOstreamEx(CNcbiOstream& out) : 
    CFastaOstream(out), 
    m_FeatCount(0), 
    m_InternalScope(new CScope(*CObjectManager::GetInstance()))
{
}


void CFastaOstreamEx::ResetFeatureCount(void) 
{
    m_FeatCount = 0;
}


void CFastaOstreamEx::WriteFeature(const CSeq_feat& feat, 
                                   CScope& scope,
                                   const bool translate_cds)
{
    // Could change this to return false if data not set
    if (!feat.IsSetData()) { 
        return; 
    }

    const bool IsCdregion = feat.GetData().IsCdregion();

    // So far, only Gene, CDS, RNA features are handled
    if (!IsCdregion &&
        !feat.GetData().IsGene() &&
        !feat.GetData().IsRna()) {
        return;
    }
    WriteFeatureTitle(feat, scope, translate_cds);

    if (translate_cds &&
        IsCdregion) {
        x_WriteTranslatedCds(feat, scope);
        return;
    }

    CBioseq_Handle bsh = scope.GetBioseqHandle(feat.GetLocation());
    if (!bsh) {
        string err_msg = "Empty bioseq handle";
        NCBI_THROW(CObjWriterException, eBadInput, err_msg); 
    }

    if (!IsCdregion ||
        feat.GetData().GetCdregion().GetFrame()<=1) {
        WriteSequence(bsh, &(feat.GetLocation()), CSeq_loc::fMerge_AbuttingOnly);
        return;
    }

    // Cdregion with frame != 1
    const auto& loc = feat.GetLocation();
    const auto frame = feat.GetData().GetCdregion().GetFrame();
    const auto strand = loc.GetStrand();
    auto trimmed_loc = x_TrimLocation(frame, strand, scope, loc);
    WriteSequence(bsh, trimmed_loc.GetPointer(), CSeq_loc::fMerge_AbuttingOnly);
}


void CFastaOstreamEx::WriteFeatureTitle(const CSeq_feat& feat,
                                        CScope& scope,
                                        const bool translate_cds)
{
    if (!feat.IsSetData()) {
        return;
    }

    string id_string;
    if (feat.GetData().IsCdregion()) {
        id_string = x_GetCDSIdString(feat, scope, translate_cds);
    } else if (feat.GetData().IsGene()) {
        id_string = x_GetGeneIdString(feat, scope);
    } else if (feat.GetData().IsRna()) {
        id_string = x_GetRNAIdString(feat, scope);
    }

    if (id_string.empty()) { // skip 
        return;
    }
   
    m_Out << ">lcl|" << id_string;
    x_WriteFeatureAttributes(feat, scope);
}


CRef<CSeq_loc> CFastaOstreamEx::x_TrimLocation(const TSeqPos frame, 
                                               const ENa_strand strand,
                                               CScope& scope,
                                               const CSeq_loc& loc)
{
    if (frame !=2 && frame != 3) {
        string err_msg = "Unexpected frame value : " + to_string(frame);
        NCBI_THROW(CObjWriterException, eInternal, err_msg);
    }

    auto seq_id = Ref(new CSeq_id());
    seq_id->Assign(*loc.GetId());
    // if frame == 2, trim just a single site
    auto start_trim = loc.GetStart(eExtreme_Biological);
    auto stop_trim = start_trim;
    if (frame == 3) {
        if (strand == eNa_strand_minus) {
            if (!start_trim) {
                string err_msg = "Expected a positive start index\n";
                NCBI_THROW(CObjWriterException, eInternal, err_msg);
            }
            --start_trim;
        } else {
            stop_trim++;
        }
    }

    auto trim_interval = Ref(new CSeq_loc(*seq_id, start_trim, stop_trim, strand));

    return sequence::Seq_loc_Subtract(loc, 
                                      *trim_interval, 
                                      CSeq_loc::fMerge_AbuttingOnly,
                                      &scope);
}


void CFastaOstreamEx::x_WriteTranslatedCds(const CSeq_feat& cds, CScope& scope)
{
    CBioseq_Handle bsh;
    try {
        auto bioseq = CSeqTranslator::TranslateToProtein(cds, scope); // Need to set seq-id on this.
        bsh = m_InternalScope->AddBioseq(bioseq.GetObject());
    } catch(CException& e) {
        string err_msg = "CDS translation error: ";
        err_msg += e.GetMsg();
        NCBI_THROW(CObjWriterException, eInternal, err_msg);
    }

    if (!bsh) {
        NCBI_THROW(CObjWriterException, eInternal, "Empty bioseq handle");
    }

    WriteSequence(bsh, nullptr, CSeq_loc::fMerge_AbuttingOnly);
}


void CFastaOstreamEx::x_WriteFeatureAttributes(const CSeq_feat& feat,
                                               CScope& scope) const
{

    string defline = "";
    if (!feat.IsSetData()) {
        return;
    }

    x_AddGeneAttributes(feat, scope, defline);

    x_AddDbxrefAttribute(feat, scope, defline);

    x_AddProteinNameAttribute(feat, scope, defline);

    x_AddRNAProductAttribute(feat, defline);

    x_AddncRNAClassAttribute(feat, defline);

    x_AddPseudoAttribute(feat, scope, defline);

    x_AddPseudoGeneAttribute(feat, scope, defline);

    x_AddReadingFrameAttribute(feat, defline);

    x_AddPartialAttribute(feat, scope, defline);

    x_AddTranslationExceptionAttribute(feat, scope, defline);

    x_AddExceptionAttribute(feat, defline);

    x_AddProteinIdAttribute(feat, scope, defline);

    x_AddTranscriptIdAttribute(feat, scope, defline);

    x_AddLocationAttribute(feat, scope, defline);

    m_Out << defline << "\n"; // endl flushes output? 
}


string CFastaOstreamEx::x_GetCDSIdString(const CSeq_feat& cds,
                                         CScope& scope,
                                         const bool translate_cds) 
{
    const auto& src_loc = cds.GetLocation();

    auto id_string  = sequence::GetAccessionForId(*(src_loc.GetId()), scope);

    if (translate_cds) {
        id_string += "_prot_";
    } else {
        id_string += "_cds_";
    }

    if (cds.IsSetProduct()) {
        const auto& product = cds.GetProduct();
        _ASSERT(product.IsWhole());
        try {
            auto prod_accver = sequence::GetAccessionForId(product.GetWhole(), scope);
            id_string +=  prod_accver + "_";
        } catch (...) {
            // Move on if there's a problem getting the product accession
        }
    }

    id_string += to_string(++m_FeatCount);
    return id_string;
}


string CFastaOstreamEx::x_GetRNAIdString(const CSeq_feat& feat, 
                                         CScope& scope) 
{
    if (!feat.IsSetData() ||
        !feat.GetData().IsRna()) {
        return "";
    } 

    const auto& src_loc = feat.GetLocation();
    auto id_string = sequence::GetAccessionForId(*(src_loc.GetId()), scope);
    const auto& rna = feat.GetData().GetRna();
    const auto rna_type = rna.IsSetType() ? rna.GetType() : CRNA_ref::eType_unknown;

    string rna_tag;
    switch (rna_type) {
    case CRNA_ref::eType_mRNA: 
    {
        rna_tag = "_mrna_";
        break;
    }

    case CRNA_ref::eType_snoRNA:
    case CRNA_ref::eType_scRNA:
    case CRNA_ref::eType_snRNA:
    case CRNA_ref::eType_ncRNA: 
    {
        rna_tag = "_ncrna_";
        break;
    }

    case CRNA_ref::eType_rRNA: 
    {
        rna_tag = "_rrna_";
        break;
    }

    case CRNA_ref::eType_tRNA: 
    {
        rna_tag = "_trna_";
        break;
    }

    case CRNA_ref::eType_premsg:
    {
        rna_tag = "_precursorrna_";
        break;
    }

    case CRNA_ref::eType_tmRNA:
    {
        rna_tag = "_tmrna_";
        break;
    }

    default: 
    {
        rna_tag = "_miscrna_";
        break;
    }
    }
    
    id_string += rna_tag;

    if (feat.IsSetProduct()) {
        const auto& product = feat.GetProduct();
        _ASSERT(product.IsWhole());
        try {
            auto prod_accver = sequence::GetAccessionForId(product.GetWhole(), scope);
            id_string +=  prod_accver + "_";
        } catch (...) {
            // Move on if there's a problem getting the product accession
        }
    }

    return id_string + to_string(++m_FeatCount);
}


string CFastaOstreamEx::x_GetProtIdString(const CSeq_feat& prot,
                                          CScope& scope) 
{
    const auto& src_loc = prot.GetLocation();

    auto id_string = sequence::GetAccessionForId(*(src_loc.GetId()), scope);
    id_string += "_prot";

    if (prot.IsSetProduct()) {
        const auto& product = prot.GetProduct();
        _ASSERT(product.IsWhole());
        try {
            auto prod_accver = sequence::GetAccessionForId(product.GetWhole(), scope);
            id_string += "_" + prod_accver;
        } catch (...) {
            // Move on...
        }
    }
    id_string += "_" + to_string(++m_FeatCount);

    return id_string;
}


string CFastaOstreamEx::x_GetGeneIdString(const CSeq_feat& gene,
                                          CScope& scope) 
{
    const auto& src_loc = gene.GetLocation();
    
    auto id_string = sequence::GetAccessionForId(*(src_loc.GetId()), scope);
    id_string += "_gene_" + to_string(++m_FeatCount);

    return id_string;
}

CConstRef<CSeq_feat> s_GetBestGeneForFeat(const CSeq_feat& feat,
                                           CScope& scope)
{
    CConstRef<CSeq_feat> no_gene;
    if (!feat.IsSetData()) {
        return no_gene;
    }


    if (feat.GetData().IsCdregion()) {
        return sequence::GetBestGeneForCds(feat, scope);
    }

    if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
        return sequence::GetBestGeneForMrna(feat, scope);
    }

    // Non-messenger RNA
    if (feat.GetData().IsRna()) {
        return GetBestOverlappingFeat(feat.GetLocation(),
                                      CSeqFeatData::eSubtype_gene,
                                      eOverlap_Simple,
                                      scope);
    }

    return no_gene;
}


void CFastaOstreamEx::x_AddDeflineAttribute(const string& label,
                                            const string& value,
                                            string& defline) const
{
    if (label.empty() || value.empty()) {
        return;
    }
    defline += " [" + label + "=" + value + "]";
}


void CFastaOstreamEx::x_AddDeflineAttribute(const string& label,
                                            const bool value,
                                            string& defline) const
{
    if (label.empty() || !value) {
        return;
    }
    defline += " [" + label + "=true]";
}
                                     

void CFastaOstreamEx::x_AddGeneAttributes(const CSeq_feat& feat,
                                          CScope& scope,
                                          string& defline) const
{
    if (!feat.IsSetData()) {
        return;
    }

    auto gene = Ref(new CGene_ref());

    if (feat.GetData().IsGene()) {
        gene->Assign(feat.GetData().GetGene());
    } else {
        auto gene_feat = s_GetBestGeneForFeat(feat, scope);
        if (gene_feat.Empty() ||
            !gene_feat->IsSetData() ||
            !gene_feat->GetData().IsGene()) {
            return;
        }
        gene->Assign(gene_feat->GetData().GetGene());
    } 

    if (gene->IsSetLocus()) {
        auto gene_locus = gene->GetLocus();
        x_AddDeflineAttribute("gene", gene_locus, defline);
    }

    if (gene->IsSetLocus_tag()) {
        auto gene_locus_tag = gene->GetLocus_tag();
        x_AddDeflineAttribute("locus_tag", gene_locus_tag, defline);
    }
}



void CFastaOstreamEx::x_AddPseudoAttribute(const CSeq_feat& feat,
                                           CScope& scope, 
                                           string& defline) const
{
    if (!feat.IsSetData()) {
        return;
    }

    bool is_pseudo = false;

    if (feat.IsSetPseudo() &&
        feat.GetPseudo()) {
        is_pseudo = true;
    }

    if (!is_pseudo ) {
        auto gene_feat = s_GetBestGeneForFeat(feat, scope);
        if (!gene_feat.Empty() &&
            gene_feat->IsSetPseudo() &&
            gene_feat->GetPseudo()) {
            is_pseudo = true;
        } else if (!gene_feat.Empty() &&
                   gene_feat->GetData().IsGene() &&
                   gene_feat->GetData().GetGene().GetPseudo()) {
            is_pseudo = true; 
        }
    } 

    x_AddDeflineAttribute("pseudo", is_pseudo, defline);
}


void CFastaOstreamEx::x_AddPseudoGeneAttribute(const CSeq_feat& feat,
                                              CScope& scope,
                                              string& defline) const
{
    if (!feat.IsSetData()) {
        return;
    }
    auto pseudogene = feat.GetNamedQual("pseudogene");

    if (pseudogene.empty()) {
        if (!feat.GetData().IsLegalQualifier(CSeqFeatData::eQual_pseudogene)) {
            return;
        }
        auto gene_feat = s_GetBestGeneForFeat(feat, scope);
        if (gene_feat.Empty()) {
            return;
        }
        pseudogene = gene_feat->GetNamedQual("pseudogene");
    }

    x_AddDeflineAttribute("pseudogene", pseudogene, defline);
}


void CFastaOstreamEx::x_AddDbxrefAttribute(const CSeq_feat& feat,
                                           CScope& scope,
                                           string& defline) const
{
    string db_xref = "";

    if (feat.IsSetDbxref()) {
        for (auto&& pDbtag : feat.GetDbxref()) {
            const CDbtag& dbtag = *pDbtag;
            if (dbtag.IsSetDb() && dbtag.IsSetTag()) {
                if (!db_xref.empty()) {
                    db_xref += ",";
                }
                db_xref += dbtag.GetDb() + ":";
                if (dbtag.GetTag().IsId()) {
                    db_xref += to_string(dbtag.GetTag().GetId());
                } else {
                    db_xref += dbtag.GetTag().GetStr();
                }
            }
        }
        x_AddDeflineAttribute("db_xref", db_xref, defline);
    }

    // Attempt to get db_xref from parent gene
    if (db_xref.empty() &&
        feat.IsSetData() &&
        !feat.GetData().IsGene()) {
        auto gene_feat = s_GetBestGeneForFeat(feat, scope);   
        if (gene_feat.Empty()) {
            return;
        }
        x_AddDbxrefAttribute(*gene_feat, scope, defline);
    }
}


void CFastaOstreamEx::x_AddProteinNameAttribute(const CSeq_feat& feat,
                                                CScope& scope,
                                                string& defline) const
{
    string protein_name;
    if (feat.GetData().IsProt() &&
        feat.GetData().GetProt().IsSetName() &&
        !feat.GetData().GetProt().GetName().empty()) {
        protein_name = feat.GetData().GetProt().GetName().front();
    }
    else
    if (feat.GetData().IsCdregion()) {
        auto pProtXref = feat.GetProtXref();
        if (pProtXref &&
            pProtXref->IsSetName() &&
            !pProtXref->GetName().empty()) {
            protein_name = pProtXref->GetName().front();
        } 
        else 
        if (feat.IsSetProduct()) { // Copied from gff3_writer
            const auto pId = feat.GetProduct().GetId();
            if (pId) {
                auto product_handle = scope.GetBioseqHandle(*pId);
                if (product_handle) {
                    SAnnotSelector sel(CSeqFeatData::eSubtype_prot);
                    sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
                    CFeat_CI it(product_handle, sel);
                    if (it && 
                        it->IsSetData() &&
                        it->GetData().GetProt().IsSetName() &&
                        !it->GetData().GetProt().GetName().empty()) {

                        protein_name = it->GetData().GetProt().GetName().front();

                    }
                }
            }
        }
    }
    x_AddDeflineAttribute("protein", protein_name, defline);
}


void CFastaOstreamEx::x_AddReadingFrameAttribute(const CSeq_feat& feat,
                                               string& defline) const
{
    if (!feat.IsSetData()) {
        return;
    }

    if (feat.GetData().IsCdregion() &&
        feat.GetData().GetCdregion().IsSetFrame()) {
        const auto frame = feat.GetData().GetCdregion().GetFrame();
        if (frame > 1) {
            x_AddDeflineAttribute("frame", to_string(frame), defline);
        }
    }
}


void CFastaOstreamEx::x_AddPartialAttribute(const CSeq_feat& feat,
                                            CScope& scope,
                                            string& defline) const
{
    auto partial = sequence::SeqLocPartialCheck(feat.GetLocation(), &scope);
    string partial_string = "";
    if (partial & sequence::eSeqlocPartial_Nostart) {
        partial_string += "5\'";
    }

    if (partial & sequence::eSeqlocPartial_Nostop) {
        if (!partial_string.empty()) {
            partial_string += ",";
        }
        partial_string += "3\'";
    }

    x_AddDeflineAttribute("partial", partial_string, defline);
}


bool CFastaOstreamEx::x_GetCodeBreak(const CSeq_feat& feat, const CCode_break& cb, CScope& scope, string& cbString) const
{
 string cb_str = ("(pos:");
 if ( cb.IsSetLoc() ) {
     const CCode_break::TLoc& loc = cb.GetLoc();

     TSeqPos seq_pos = sequence::LocationOffset(feat.GetLocation(), loc,
                                                sequence::eOffset_FromStart,
                                                &scope);
      
     int frame = 0; 
     if (feat.GetData().IsCdregion()) {
         const CCdregion& cdr = feat.GetData().GetCdregion();
         if (cdr.IsSetFrame()) {
             switch (cdr.GetFrame()) {
             case CCdregion::eFrame_two: 
                 frame = 1;
                 break;
             case CCdregion::eFrame_three:
                 frame = 2;
                 break;
             default:
                 break;
             }
         }
     }     

     seq_pos -= frame;

     switch( loc.Which() ) {
         default: {
             int width = 1 + loc.GetStop(eExtreme_Positional) - loc.GetStart(eExtreme_Positional);
             cb_str += NStr::IntToString(seq_pos + 1);
             cb_str += "..";
             cb_str += NStr::IntToString(seq_pos + width);
             break;
         }
         case CSeq_loc::e_Int: {
             const CSeq_interval& intv = loc.GetInt();
             int width =  1 + intv.GetTo() - intv.GetFrom();
             string intv_str = "";
             intv_str += NStr::IntToString(seq_pos+1);
             intv_str += "..";
             intv_str += NStr::IntToString(seq_pos+width);
             cb_str += intv_str;
             break;
              
         }
     }
     cb_str += ",aa:";
     string aaName;
     if (!CWriteUtil::GetAaName(cb, aaName)) {
         return false;
     }
     cb_str += aaName + ")";
     cbString = cb_str;
     return true;
 }
 return false;
}




void CFastaOstreamEx::x_AddTranslationExceptionAttribute(const CSeq_feat& feat,
                                                         CScope& scope,
                                                         string& defline) const
{
    if (!feat.IsSetData() ||
        !feat.GetData().IsCdregion() || 
        !feat.GetData().GetCdregion().IsSetCode_break()){
        return;
    }

    const auto code_breaks = feat.GetData().GetCdregion().GetCode_break();

    string transl_exception = "";
    for (auto && code_break : code_breaks) {
        string cb_string = "";
        if (x_GetCodeBreak(feat, *code_break, scope, cb_string)) {
     //   if (CWriteUtil::GetCodeBreak(*code_break, cb_string)) {
            if (!transl_exception.empty()) {
                transl_exception += ",";
            }
            transl_exception += cb_string;
        }
    }

    x_AddDeflineAttribute("transl_except", transl_exception, defline);
}


void CFastaOstreamEx::x_AddExceptionAttribute(const CSeq_feat& feat, 
                                              string& defline) const
{
    if (feat.IsSetExcept_text()) {
        auto except_string = feat.GetExcept_text();
        x_AddDeflineAttribute("exception", except_string, defline);
    }
}


void CFastaOstreamEx::x_AddProteinIdAttribute(const CSeq_feat& feat,
                                              CScope& scope,
                                              string& defline) const
{
    if (feat.GetData().IsCdregion() &&
        feat.IsSetProduct() &&
        feat.GetProduct().GetId()) {
        string protein_id = sequence::GetAccessionForId(*(feat.GetProduct().GetId()), scope);

        x_AddDeflineAttribute("protein_id", protein_id, defline);
    }
}


void CFastaOstreamEx::x_AddTranscriptIdAttribute(const CSeq_feat& feat,
                                                 CScope& scope,
                                                 string& defline) const
{
    if (!feat.GetData().IsRna()) {
        return;
    }

    string transcript_id = feat.GetNamedQual("transcript_id");

    if (transcript_id.empty() &&
        feat.IsSetProduct() &&
        feat.GetProduct().GetId()) {
        transcript_id = sequence::GetAccessionForId(*(feat.GetProduct().GetId()), scope);
    }
    x_AddDeflineAttribute("transcript_id", transcript_id, defline);
}


void CFastaOstreamEx::x_AddLocationAttribute(const CSeq_feat& feat, 
                                             CScope& scope,
                                             string& defline) const
{
    CFlatFileConfig cfg;
    CFlatFileContext ffctxt(cfg);

    auto bsh = scope.GetBioseqHandle(feat.GetLocation());
    if (!bsh) {
        return;
    }

    CBioseqContext ctxt(bsh, ffctxt);
    auto loc_string = CFlatSeqLoc(feat.GetLocation(), ctxt).GetString();

    x_AddDeflineAttribute("location", loc_string, defline);
}


void CFastaOstreamEx::x_AddncRNAClassAttribute(const CSeq_feat& feat,
                                               string& defline) const
{
    if (!feat.IsSetData() ||
        !feat.GetData().IsRna() ||
        !feat.GetData().GetRna().IsSetExt() ||
        !feat.GetData().GetRna().GetExt().IsGen() ||
        !feat.GetData().GetRna().GetExt().GetGen().IsSetClass()) {
        return;
    }

    const auto ncRNA_class = feat.GetData().GetRna().GetExt().GetGen().GetClass();

    x_AddDeflineAttribute("ncRNA_class", ncRNA_class, defline);
}


static const string s_TrnaList[] = {
    "tRNA-Gap",
    "tRNA-Ala",
    "tRNA-Asx",
    "tRNA-Cys",
    "tRNA-Asp",
    "tRNA-Glu",
    "tRNA-Phe",
    "tRNA-Gly",
    "tRNA-His",
    "tRNA-Ile",
    "tRNA-Xle",
    "tRNA-Lys",
    "tRNA-Leu",
    "tRNA-Met",
    "tRNA-Asn",
    "tRNA-Pyl",
    "tRNA-Pro",
    "tRNA-Gln",
    "tRNA-Arg",
    "tRNA-Ser",
    "tRNA-Thr",
    "tRNA-Sec",
    "tRNA-Val",
    "tRNA-Trp",
    "tRNA-OTHER",
    "tRNA-Tyr",
    "tRNA-Glx",
    "tRNA-TERM"
};


void CFastaOstreamEx::x_AddRNAProductAttribute(const CSeq_feat& feat,
                                               string& defline) const
{
    if (!feat.IsSetData() ||
        !feat.GetData().IsRna()) {
        return;
    }

    const auto& rna = feat.GetData().GetRna();
    const auto rna_type = rna.IsSetType() ?
        rna.GetType() : CRNA_ref::eType_unknown;

    string product_string;
    if (rna_type == CRNA_ref::eType_tRNA) {
        if (rna.IsSetExt() && rna.GetExt().IsTRNA()) {
            const auto& trna = rna.GetExt().GetTRNA();
            CWriteUtil::GetTrnaProductName(trna, product_string);
        }
    } // rna_type == CRNA_ref::eType_tRNA

    if (product_string.empty() && 
        rna.IsSetExt() && 
        rna.GetExt().IsName()) {
        product_string = rna.GetExt().GetName();
    }

    if (product_string.empty() &&
        rna.IsSetExt() &&
        rna.GetExt().IsGen() &&
        rna.GetExt().GetGen().IsSetProduct()) {
        product_string = rna.GetExt().GetGen().GetProduct();
    }

    if (product_string.empty()) {
        product_string = feat.GetNamedQual("product");
    }

    x_AddDeflineAttribute("product", product_string, defline);
}


CFastaOstreamComp::CFastaOstreamComp(const string& dir, const string& filename_without_ext)
: m_filename_without_ext(filename_without_ext),
  m_Flags(-1)
{        
    m_dir = CDir::AddTrailingPathSeparator(dir);
}

CFastaOstreamComp::~CFastaOstreamComp()
{
    NON_CONST_ITERATE(vector<TStreams>, it, m_streams)
    {
        delete it->m_fasta_stream; it->m_fasta_stream = 0;
        delete it->m_ostream; it->m_ostream = 0;
    }
}

void CFastaOstreamComp::x_GetNewFilename(string& filename, E_FileSection sel)
{
    filename = m_dir;
    filename += m_filename_without_ext;
    const char* suffix = 0;
    switch (sel)
    {
    case eFS_nucleotide:
        suffix = "";
        break;
    case eFS_CDS:
        suffix = "_cds_from_genomic";
        break;
    case eFS_RNA:
        suffix = "_rna_from_genomic";
        break;
    default:
        _ASSERT(0);
    }
    filename.append(suffix);
    const char* ext = 0;
    switch (sel)
    {
    case eFS_nucleotide:
        ext = ".fsa";
        break;
    case eFS_CDS:
    case eFS_RNA:
        ext = ".fna";
        break;          
    default:
        _ASSERT(0);
    }
    filename.append(ext);
}

CNcbiOstream* CFastaOstreamComp::x_GetOutputStream(const string& filename, E_FileSection sel)
{
    return new CNcbiOfstream(filename.c_str());
}

CFastaOstream* CFastaOstreamComp::x_GetFastaOstream(CNcbiOstream& ostr, E_FileSection sel)
{
    CFastaOstream* fstr = new CFastaOstream(ostr);
    if (m_Flags != -1)
       fstr->SetAllFlags(m_Flags);
    return fstr;
}

CFastaOstreamComp::TStreams& CFastaOstreamComp::x_GetStream(E_FileSection sel)
{
    if (m_streams.size() <= sel)
    {
        m_streams.resize(sel + 1);
    }
    TStreams& res = m_streams[sel];
    if (res.m_filename.empty())
    { 
        x_GetNewFilename(res.m_filename, sel);
    }
    if (res.m_ostream == 0)
    {
        res.m_ostream = x_GetOutputStream(res.m_filename, sel);
    }
    if (res.m_fasta_stream == 0)
    {
        res.m_fasta_stream = x_GetFastaOstream(*res.m_ostream, sel);
    }
    return res;
}

void CFastaOstreamComp::Write(const CSeq_entry_Handle& handle, const CSeq_loc* location)
{
    for (CBioseq_CI it(handle); it; ++it) {
        if (location) {
            CSeq_loc loc2;
            loc2.SetWhole().Assign(*it->GetSeqId());
            int d = sequence::TestForOverlap
                (*location, loc2, sequence::eOverlap_Interval,
                kInvalidSeqPos, &handle.GetScope());
            if (d < 0) {
                continue;
            }
        }
        x_Write(*it, location);
    }
}

void CFastaOstreamComp::x_Write(const CBioseq_Handle& handle, const CSeq_loc* location)
{
    E_FileSection sel = eFS_nucleotide;
    if (handle.CanGetInst_Mol())
    {
        CSeq_inst::EMol mol = handle.GetInst_Mol();
        switch (mol)
        {
        case ncbi::objects::CSeq_inst_Base::eMol_dna:
            sel = eFS_RNA;
            break;
        case ncbi::objects::CSeq_inst_Base::eMol_rna:
            sel = eFS_RNA;
            break;
        case ncbi::objects::CSeq_inst_Base::eMol_aa:
            sel = eFS_CDS;
            break;
        case ncbi::objects::CSeq_inst_Base::eMol_na:
            break;
        default:
            break;
        }
    }
    TStreams& res = x_GetStream(sel);
    res.m_fasta_stream->Write(handle, location);
}

END_NCBI_SCOPE
