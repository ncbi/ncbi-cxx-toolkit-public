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
 * Authors:  Sergiy Gotvyanskyy
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
#include <objects/seqfeat/RNA_gen.hpp>
//#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sequtil/sequtil.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)


CFastaOstreamEx::CFastaOstreamEx(CNcbiOstream& out) : CFastaOstream(out), m_FeatCount(0)
{
}


void CFastaOstreamEx::ResetFeatureCount(void) 
{
    m_FeatCount = 0;
}


void CFastaOstreamEx::WriteFeature(CScope& scope,
                                   const CSeq_feat& feat)
{
    if (!feat.IsSetData() ||
        !feat.IsSetLocation()) {
        return;
    }

    auto bsh = scope.GetBioseqHandle(feat.GetLocation());

    if (!bsh) {
        return;
    }

    WriteFeature(bsh, feat);
}


void CFastaOstreamEx::WriteFeatureTitle(CScope& scope, 
                                        const CSeq_feat& feat)
{

    if (!feat.IsSetData() ||
        !feat.IsSetLocation()) {
        return;
    }

    auto bsh = scope.GetBioseqHandle(feat.GetLocation());

    if (!bsh) {
        return;
    }

    WriteFeatureTitle(bsh, feat);
}


void CFastaOstreamEx::WriteFeature(const CBioseq_set_Handle& handle,
                                   const CSeq_feat& feat)
{
    if (!feat.IsSetData()) {
        return;
    }

    auto bsh = handle.GetScope().GetBioseqHandle(feat.GetLocation());

    if (!bsh) {
        return;
    }


    WriteFeature(bsh, feat);


}


void CFastaOstreamEx::WriteFeature(const CBioseq_Handle& handle, 
                                   const CSeq_feat& feat)
{
    if (!feat.IsSetData()) {
        return;
    }

    if (!feat.GetData().IsCdregion() &&
        !feat.GetData().IsGene() &&
        !feat.GetData().IsRna()) {
        return;
    }

    WriteFeatureTitle(handle, feat);
    if (!feat.GetData().IsCdregion() ||
        !feat.GetData().GetCdregion().IsSetFrame() ||
        feat.GetData().GetCdregion().GetFrame()==1) {
        WriteSequence(handle, &(feat.GetLocation()), CSeq_loc::fMerge_AbuttingOnly);
        return;
    }

    // Cdregion with frameshift
    const auto& loc = feat.GetLocation();
    const auto loc_start = loc.GetStart(eExtreme_Positional);
    const auto strand = loc.GetStrand();
    auto seq_id = Ref(new CSeq_id());
    seq_id->Assign(*loc.GetId());

    auto frame = feat.GetData().GetCdregion().GetFrame();
    auto untranslated_loc = Ref(new CSeq_loc(*seq_id, loc_start, loc_start+frame-2, strand));

    auto translated_loc = sequence::Seq_loc_Subtract(loc, *untranslated_loc, 
                                                CSeq_loc::fMerge_AbuttingOnly, &(handle.GetScope()));

    WriteSequence(handle, translated_loc.GetPointer(), CSeq_loc::fMerge_AbuttingOnly);
}


void CFastaOstreamEx::WriteFeatureTitle(const CBioseq_Handle& handle, 
                                        const CSeq_feat& feat)
{
    if (!feat.IsSetData()) {
        return;
    }

    string id_string;
    if (feat.GetData().IsCdregion()) {
        id_string = x_GetCDSIdString(handle, feat);
    } else if (feat.GetData().IsGene()) {
        id_string = x_GetGeneIdString(handle, feat);
    } else if (feat.GetData().IsRna()) {
        id_string = x_GetRNAIdString(handle, feat);
    }

    if (id_string.empty()) { // skip - maybe throw an exception instead
        return;
    }
   
    m_Out << ">lcl|" << id_string;

    x_WriteFeatureModifiers(handle, feat);
}


string CFastaOstreamEx::x_GetCDSIdString(const CBioseq_Handle& handle,
                                              const CSeq_feat& cds)
{
    // Need to put a whole bunch of checks in here
    const auto& src_loc = cds.GetLocation();

    auto id_string  = sequence::GetAccessionForId(*(src_loc.GetId()), handle.GetScope());
    id_string += "_cds";


    if (cds.IsSetProduct()) {

        const auto& product = cds.GetProduct();
        _ASSERT(product.IsWhole());
        try {
            auto prod_accver = sequence::GetAccessionForId(product.GetWhole(), handle.GetScope());
            id_string += "_" + prod_accver;
        } catch (...) {
            // Move on if there's a problem getting the product accession
        }
    }

    id_string += "_";
    id_string += to_string(++m_FeatCount);

    return id_string;
}


string CFastaOstreamEx::x_GetRNAIdString(const CBioseq_Handle& handle,
                                               const CSeq_feat& feat)
{
    if (!feat.IsSetData() ||
        !feat.GetData().IsRna()) {
        return "";
    } 


    const auto& src_loc = feat.GetLocation();

    auto id_string = sequence::GetAccessionForId(*(src_loc.GetId()), handle.GetScope());

    if (!feat.GetData().GetRna().IsSetType()) {
        return id_string +  "_miscrna_" + to_string(++m_FeatCount);
    }

    const auto rna_type = feat.GetData().GetRna().GetType();

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

    return id_string + rna_tag + to_string(++m_FeatCount);
}


string CFastaOstreamEx::x_GetProtIdString(const CBioseq_Handle& handle,
                                               const CSeq_feat& prot) 
{
    const auto& src_loc = prot.GetLocation();

    auto id_string = sequence::GetAccessionForId(*(src_loc.GetId()), handle.GetScope());
    id_string += "_prot";

    if (prot.IsSetProduct()) {
        const auto& product = prot.GetProduct();
        _ASSERT(product.IsWhole());
        try {
            auto prod_accver = sequence::GetAccessionForId(product.GetWhole(), handle.GetScope());
            id_string += "_" + prod_accver;
        } catch (...) {
            // Move on...
        }
    }
    id_string += "_" + to_string(++m_FeatCount);

    return id_string;
}


string CFastaOstreamEx::x_GetGeneIdString(const CBioseq_Handle& handle, 
                                               const CSeq_feat& gene)
{
    const auto& src_loc = gene.GetLocation();
    
    auto id_string = sequence::GetAccessionForId(*(src_loc.GetId()), handle.GetScope());
    id_string += "_gene_" + to_string(++m_FeatCount);

    return id_string;
}


void CFastaOstreamEx::x_AddGeneAttributes(const CBioseq_Handle& handle, 
                                               const CSeq_feat& feat,
                                               string& defline)
{
    if (!feat.IsSetData()) {
        return;
    }

    auto gene = Ref(new CGene_ref());

    if (feat.GetData().IsCdregion()) {
        auto gene_feat = sequence::GetBestGeneForCds(feat, handle.GetScope());
        if (gene_feat.Empty() ||
            !gene_feat->IsSetData() ||
            !gene_feat->GetData().IsGene()) {
            return;
        }
        gene->Assign(gene_feat->GetData().GetGene());
    } 
    else if (feat.GetData().IsGene()) {
        gene->Assign(feat.GetData().GetGene());
    }

    if (gene->IsSetLocus()) {
        auto gene_locus = gene->GetLocus();
        if (!gene_locus.empty()) {
            defline += " [gene=" + gene_locus + "]";
        }
    }

    if (gene->IsSetLocus_tag()) {
        auto gene_locus_tag = gene->GetLocus_tag();
        if (!gene_locus_tag.empty()) {
            defline += " [locus_tag=" + gene_locus_tag + "]";
        }
    }
}


void CFastaOstreamEx::x_AddDbxrefAttribute(const CBioseq_Handle& handle,
                                                const CSeq_feat& feat,
                                                string& defline)
{
    if (feat.IsSetDbxref()) {
        string dbxref_string = "";
        for (auto&& pDbtag : feat.GetDbxref()) {

            const CDbtag& dbtag = *pDbtag;
            if (dbtag.IsSetDb() && dbtag.IsSetTag()) {
                if (!dbxref_string.empty()) {
                    dbxref_string += ",";
                }
                dbxref_string += dbtag.GetDb() + ":";
                if (dbtag.GetTag().IsId()) {
                    dbxref_string += to_string(dbtag.GetTag().GetId());
                } else {
                    dbxref_string += dbtag.GetTag().GetStr();
                }
            }
        }
        if (!dbxref_string.empty()) {
            defline += " [db_xref=" + dbxref_string  + "]";
        }
    }
}


void CFastaOstreamEx::x_AddProteinNameAttribute(const CBioseq_Handle& handle, 
                                              const CSeq_feat& feat,
                                              string& defline)
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
                auto product_handle = handle.GetScope().GetBioseqHandle(*pId);
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

    if (!protein_name.empty()) {
       defline += " [protein=" + protein_name + "]";
    }
}


void CFastaOstreamEx::x_AddReadingFrameAttribute(const CSeq_feat& feat,
                                               string& defline)
{
    if (!feat.IsSetData()) {
        return;
    }

    if (feat.GetData().IsCdregion() &&
        feat.GetData().GetCdregion().IsSetFrame()) {
        auto frame = feat.GetData().GetCdregion().GetFrame();
        if (frame > 1) {
           defline += " [frame=" + to_string(frame) + "]";
        }
    }
}


void CFastaOstreamEx::x_AddPartialAttribute(const CBioseq_Handle& handle, 
                                          const CSeq_feat& feat,
                                          string& defline)
{
    auto partial = sequence::SeqLocPartialCheck(feat.GetLocation(), &(handle.GetScope()));
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

    if (!partial_string.empty()) {
        defline += " [partial=" + partial_string + "]";
    }
}


void CFastaOstreamEx::x_AddTranslationExceptionAttribute(const CBioseq_Handle& handle,
                                                         const CSeq_feat& feat,
                                                         string& defline)
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
        if (CWriteUtil::GetCodeBreak(*code_break, cb_string)) {
            if (!transl_exception.empty()) {
                transl_exception += ",";
            }
            transl_exception += cb_string;
        }
    }

    if (!transl_exception.empty()) {
        defline += " [transl_except=" + transl_exception + "]";
    }

    return;
}


void CFastaOstreamEx::x_AddExceptionAttribute(const CSeq_feat& feat, 
                                                   string& defline)
{
    if (feat.IsSetExcept_text()) {
        auto except_string = feat.GetExcept_text();
        if (!except_string.empty()) {
            defline += " [exception=" + except_string + "]";
        }
    }
}


void CFastaOstreamEx::x_AddProteinIdAttribute(const CBioseq_Handle& handle,
                                                   const CSeq_feat& feat,
                                                   string& defline)
{
    if (feat.GetData().IsCdregion() &&
        feat.IsSetProduct() &&
        feat.GetProduct().GetId()) {
        string protein_id = sequence::GetAccessionForId(*(feat.GetProduct().GetId()), handle.GetScope());
        if (!protein_id.empty()) {
            defline += " [protein_id=" + protein_id + "]";
        }
    }
}


void CFastaOstreamEx::x_AddLocationAttribute(const CBioseq_Handle& handle, 
                                             const CSeq_feat& feat, 
                                             string& defline)
{
    CFlatFileConfig cfg;
    CFlatFileContext ffctxt(cfg);
    CBioseqContext ctxt(handle, ffctxt);
    

    auto loc_string = CFlatSeqLoc(feat.GetLocation(), ctxt).GetString();

    if (loc_string.empty()) {
        return;
    }

    defline += " [location=" + loc_string + "]";
    return;
}


void CFastaOstreamEx::x_AddncRNAClassAttribute(const CSeq_feat& feat,
                                               string& defline)
{
    if (!feat.IsSetData() ||
        !feat.GetData().IsRna() ||
        !feat.GetData().GetRna().IsSetExt() ||
        !feat.GetData().GetRna().GetExt().IsGen() ||
        !feat.GetData().GetRna().GetExt().GetGen().IsSetClass()) {
        return;
    }

    const auto ncRNA_class = feat.GetData().GetRna().GetExt().GetGen().GetClass();

    if (ncRNA_class.empty()) {
        return;
    }

    defline += " [ncRNA_class=" + ncRNA_class + "]";
    return;
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


static const string& s_AaName(int aa)
{
    int idx = 255;
    if (aa != '*') {
        idx = aa - 64;
    } else {
        idx = 27;
    }
    if ( idx > 0 && idx < ArraySize(s_TrnaList) ) {
        return s_TrnaList [idx];
    }
    return kEmptyStr;
}




void CFastaOstreamEx::x_AddRNAProductAttribute(const CBioseq_Handle& handle,
                                               const CSeq_feat& feat,
                                               string& defline)
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

    if (product_string.empty() &&
        feat.IsSetQual()) {

        const auto& quals = feat.GetQual();

        for (const auto& qual : feat.GetQual()) 
        {
            if (qual->IsSetQual() && 
                qual->IsSetVal() &&
                qual->GetQual() == "product") {
                product_string = qual->GetVal();
                if (!product_string.empty()) {
                    break;
                }
            }
        }
    }

    defline += " [product=" + product_string + "]"; 
}



void CFastaOstreamEx::x_WriteFeatureModifiers(const CBioseq_Handle& handle, 
                                              const CSeq_feat& feat)
{

    string defline = "";
    if (!feat.IsSetData()) {
        return;
    }

    x_AddGeneAttributes(handle, feat, defline);

    x_AddDbxrefAttribute(handle, feat, defline);

    x_AddProteinNameAttribute(handle, feat, defline);

    x_AddRNAProductAttribute(handle, feat, defline);

    x_AddncRNAClassAttribute(feat,defline);

    x_AddReadingFrameAttribute(feat, defline);

    x_AddPartialAttribute(handle, feat, defline);

    x_AddTranslationExceptionAttribute(handle, feat, defline);

    x_AddExceptionAttribute(feat, defline);

    x_AddProteinIdAttribute(handle, feat, defline);

    x_AddLocationAttribute(handle, feat, defline);

    m_Out << defline << "\n";
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

END_SCOPE(objects)
END_NCBI_SCOPE
