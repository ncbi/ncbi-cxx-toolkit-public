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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 * conversion to/from ASN1
 *
 */

#include <ncbi_pch.hpp>
#include <algo/gnomon/asn1.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include "gnomon_seq.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)
USING_SCOPE(ncbi::objects);

struct SModelData {
    SModelData(const CGeneModel& model, const CEResidueVec& contig_seq);

    const CGeneModel& model;
    CFrameShiftedSeqMap mrna_map;
    CEResidueVec mrna_seq;
    CRef<CSeq_id> mrna_sid;
    CRef<CSeq_id> prot_sid;
};

SModelData::SModelData(const CGeneModel& m, const CEResidueVec& contig_seq) : model(m), mrna_map(m)
{
    mrna_map.EditedSequence(contig_seq, mrna_seq);

    string model_tag = "hmm." + NStr::IntToString(model.ID());
    prot_sid = new CSeq_id(CSeq_id::e_Local, model_tag + ".p");  
    mrna_sid = new CSeq_id(CSeq_id::e_Local, model_tag + ".m");  
}


class CAnnotationASN1::CImplementationData {
public:
    CImplementationData(const string& contig_name, const CResidueVec& seq);

    void AddModel(const CGeneModel& model);
    CRef<CSeq_entry> main_seq_entry;

private:


    CRef<CSeq_feat> create_gene_feature(const SModelData& md) const;
    void update_gene_feature(CSeq_feat& gene, CSeq_feat& mrna_feat) const;
    CRef<CSeq_entry>  create_prot_seq_entry(const SModelData& md) const;
    CRef<CSeq_entry> create_mrna_seq_entry(SModelData& md);
    CRef<CSeq_feat> create_mrna_feature(SModelData& md);
    enum EWhere { eOnGenome, eOnMrna };
    CRef<CSeq_feat> create_cdregion_feature(SModelData& md,EWhere onWhat);
    CRef<CSeq_feat> create_5prime_stop_feature(SModelData& md) const;
    CRef<CSeq_align> model2spliced_seq_align(SModelData& md);
    CRef<CSpliced_exon> spliced_exon (const CAlignExon& e, EStrand strand) const;
    CRef<CSeq_loc> create_packed_int_seqloc(const CGeneModel& model, TSignedSeqRange limits = TSignedSeqRange::GetWhole());

    CRef<CSeq_id> contig_sid;
    const CResidueVec& contig_seq;
    CDoubleStrandSeq  contig_ds_seq;

    CBioseq_set::TSeq_set* nucprots;
    CSeq_annot::C_Data::TFtable* feature_table;
    typedef map<int,CRef<CSeq_feat> > TGeneMap;
    TGeneMap genes;
};

CAnnotationASN1::CImplementationData::CImplementationData(const string& contig_name, const CResidueVec& seq) :
    main_seq_entry(new CSeq_entry),
    contig_sid(CreateSeqid(contig_name)), contig_seq(seq)
{
    Convert(contig_seq, contig_ds_seq);

    CBioseq_set& bioseq_set = main_seq_entry->SetSet();

    nucprots = &bioseq_set.SetSeq_set();

    CRef<CSeq_annot> seq_annot(new CSeq_annot);
    bioseq_set.SetAnnot().push_back(seq_annot);
    feature_table = &seq_annot->SetData().SetFtable();       
}

CAnnotationASN1::CAnnotationASN1(const string& contig_name, const CResidueVec& seq) :
    m_data(new CImplementationData(contig_name, seq))
{
}

CAnnotationASN1::~CAnnotationASN1()
{
}

CRef<CSeq_entry> CAnnotationASN1::GetASN1() const
{
    return m_data->main_seq_entry;
}


void CAnnotationASN1::AddModel(const CGeneModel& model)
{
    m_data->AddModel(model);
}

void CAnnotationASN1::CImplementationData::AddModel(const CGeneModel& model)
{
    SModelData md(model, contig_ds_seq[ePlus]);

    CRef<CSeq_entry> model_products(new CSeq_entry);
    nucprots->push_back(model_products);
    model_products->SetSet().SetClass(CBioseq_set::eClass_nuc_prot);


    model_products->SetSet().SetSeq_set().push_back(create_mrna_seq_entry(md));
    model_products->SetSet().SetSeq_set().push_back(create_prot_seq_entry(md));

    CRef<CSeq_feat> mrna_feat = create_mrna_feature(md);
    feature_table->push_back(mrna_feat);

    CRef<CSeq_feat> cds_feat = create_cdregion_feature(md,eOnGenome);
    feature_table->push_back(cds_feat);

    CRef< CSeqFeatXref > cdsxref( new CSeqFeatXref() );
    cdsxref->SetId(*cds_feat->SetIds().front());
//     cdsxref->SetData().SetCdregion();
    
    CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
    mrnaxref->SetId(*mrna_feat->SetIds().front());
//     mrnaxref->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);

    cds_feat->SetXref().push_back(mrnaxref);
    mrna_feat->SetXref().push_back(cdsxref);


    if (model.GeneID()) {
        CImplementationData::TGeneMap::iterator gene = genes.find(model.GeneID());
        if (gene == genes.end()) {
            CRef<CSeq_feat> gene_feature = create_gene_feature(md);
            gene = genes.insert(make_pair(model.GeneID(), gene_feature)).first;
            feature_table->push_back(gene_feature);
        }
        update_gene_feature(*gene->second, *mrna_feat);
    }

}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_cdregion_feature(SModelData& md, EWhere where)
{
    const CGeneModel& model = md.model;
    EStrand strand = model.Strand();

    CRef<CSeq_feat> cdregion_feature(new CSeq_feat);  

    if (where==eOnGenome) {
        CRef<CObject_id> obj_id( new CObject_id() );
        obj_id->SetStr("cds." + NStr::IntToString(model.ID()));
        CRef<CFeat_id> feat_id( new CFeat_id() );
        feat_id->SetLocal(*obj_id);
        cdregion_feature->SetIds().push_back(feat_id);
    }

    CRef<CGenetic_code::C_E> cdrcode(new CGenetic_code::C_E);
    cdrcode->SetId(1);
    cdregion_feature->SetData().SetCdregion().SetCode().Set().push_back(cdrcode);

   if(model.PStop() && (where==eOnMrna || model.FrameShifts().empty())) {
        CCdregion::TCode_break& code_breaks = cdregion_feature->SetData().SetCdregion().SetCode_break();
        ITERATE(CCDSInfo::TPStops,s,model.GetCdsInfo().PStops()) {
            CRef< CCode_break > code_break(new CCode_break);
            CRef<CSeq_loc> pstop;

            TSeqPos from = s->GetFrom();
            TSeqPos to = s->GetTo();
            switch (where) {
            case eOnMrna:
                from = md.mrna_map.MapOrigToEdited(from);
                to = md.mrna_map.MapOrigToEdited(to);
                
                if (strand==eMinus)
                    swap(from,to);
                _ASSERT(from+2==to);
                pstop.Reset(new CSeq_loc(*md.mrna_sid, from, to, eNa_strand_plus));
                break;
            case eOnGenome:
                _ASSERT(model.FShiftedLen(from,to)==3);
                pstop = create_packed_int_seqloc(model,*s);
                break;
            }

            code_break->SetLoc(*pstop);
            CRef<CCode_break::C_Aa> aa(new CCode_break::C_Aa);
            aa->SetNcbieaa('X');
            code_break->SetAa(*aa);
            code_breaks.push_back(code_break);
        }
    }

    cdregion_feature->SetProduct().SetWhole(*md.prot_sid);

    int start, stop, altstart;
    if (strand==ePlus) {
        altstart = md.mrna_map.MapOrigToEdited(model.MaxCdsLimits().GetFrom());
        start = md.mrna_map.MapOrigToEdited(model.RealCdsLimits().GetFrom());
        stop = md.mrna_map.MapOrigToEdited(model.MaxCdsLimits().GetTo());
    } else {
        altstart = md.mrna_map.MapOrigToEdited(model.MaxCdsLimits().GetTo());
        start = md.mrna_map.MapOrigToEdited(model.RealCdsLimits().GetTo());
        stop = md.mrna_map.MapOrigToEdited(model.MaxCdsLimits().GetFrom());
    }

    int frame = 0;
    if(!model.HasStart()) {
        _ASSERT(altstart == 0);
        frame = start;
        start = 0;
    }
    CCdregion::EFrame ncbi_frame = CCdregion::eFrame_one;
    if(frame == 1) ncbi_frame = CCdregion::eFrame_two;
    else if(frame == 2) ncbi_frame = CCdregion::eFrame_three; 
    cdregion_feature->SetData().SetCdregion().SetFrame(ncbi_frame);

    CRef<CSeq_loc> cdregion_location;
    switch (where) {
    case eOnMrna:
        cdregion_location.Reset(new CSeq_loc(*md.mrna_sid, start, stop, eNa_strand_plus));

        if (0 < altstart && altstart != start)
            cdregion_location->SetInt().SetFuzz_from().SetAlt().push_back(altstart);

        break;
    case eOnGenome:
        cdregion_location = create_packed_int_seqloc(model,model.RealCdsLimits());

        if (strand==ePlus) {
            altstart = model.MaxCdsLimits().GetFrom();
            start = model.RealCdsLimits().GetFrom();
        } else {
            altstart = model.MaxCdsLimits().GetTo();
            start = model.RealCdsLimits().GetTo();
        }

        if(!model.FrameShifts().empty()) {
            cdregion_feature->SetExcept(true);
            cdregion_feature->SetExcept_text("unclassified translation discrepancy");
        }

        if (model.Status() & CGeneModel::ePseudo) {
            cdregion_feature->SetPseudo(true);
        }
 
        break;
    }

    if (!model.HasStart() || model.OpenCds())
        cdregion_location->SetPartialStart(true,eExtreme_Biological);
    if (!model.HasStop())
        cdregion_location->SetPartialStop(true,eExtreme_Biological);
    cdregion_feature->SetLocation(*cdregion_location);
    if (!model.HasStart() || model.OpenCds() || !model.HasStop())
        cdregion_feature->SetPartial(true);

    return cdregion_feature;
}


CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_5prime_stop_feature(SModelData& md) const
{
    const CGeneModel& model = md.model;


    int frame = -1;
    TIVec starts[3], stops[3];

    FindStartsStops(model, contig_ds_seq[model.Strand()], md.mrna_seq, md.mrna_map, starts, stops, frame);
    _ASSERT( starts[frame].size()>0 && stops[frame].size()>0 );

    TSignedSeqPos stop_5prime = stops[frame][0];
    if (FindUpstreamStop(stops[frame],starts[frame][0],stop_5prime) && stop_5prime>=0) {
        CRef<CSeq_feat> stop_5prime_feature(new CSeq_feat);
        stop_5prime_feature->SetData().SetImp().SetKey("misc_feature");
        stop_5prime_feature->SetData().SetImp().SetDescr("5' stop");
        CRef<CSeq_loc> stop_5prime_location(new CSeq_loc(*md.mrna_sid, stop_5prime, stop_5prime+2, eNa_strand_plus));
        stop_5prime_feature->SetLocation(*stop_5prime_location);
        return stop_5prime_feature;
    } else
        return CRef<CSeq_feat>();
}

CRef< CUser_object > create_ModelEvidence_user_object()
{
    CRef< CUser_object > user_obj(new CUser_object);
    CRef< CObject_id > type(new CObject_id);
    type->SetStr("ModelEvidence");
    user_obj->SetType(*type);

    return user_obj;
}

CRef<CSeq_loc> CAnnotationASN1::CImplementationData::create_packed_int_seqloc(const CGeneModel& model, TSignedSeqRange limits)
{
    CRef<CSeq_loc> seq_loc(new CSeq_loc);
    CPacked_seqint& packed_int = seq_loc->SetPacked_int();
    ENa_strand strand = model.Strand()==ePlus?eNa_strand_plus:eNa_strand_minus; 

    ITERATE(CGeneModel::TExons, e, model.Exons()) {
        TSignedSeqRange interval_range = e->Limits() & limits;
        if (interval_range.Empty())
            continue;
        CRef<CSeq_interval> interval(new CSeq_interval(*contig_sid, interval_range.GetFrom(),interval_range.GetTo(), strand));
        interval->SetPartialStart(!e->m_fsplice && interval_range.GetFrom()==e->GetFrom(),eExtreme_Positional);
        interval->SetPartialStop(!e->m_ssplice && interval_range.GetTo()==e->GetTo(),eExtreme_Positional);
        packed_int.AddInterval(*interval);
    }
    return seq_loc->Merge(CSeq_loc::fSort, NULL);
}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_mrna_feature(SModelData& md)
{
    const CGeneModel& model = md.model;

    CRef<CSeq_feat> mrna_feature(new CSeq_feat);

    CRef<CObject_id> obj_id( new CObject_id() );
    obj_id->SetStr("rna." + NStr::IntToString(model.ID()));
    CRef<CFeat_id> feat_id( new CFeat_id() );
    feat_id->SetLocal(*obj_id);
    mrna_feature->SetIds().push_back(feat_id);

    mrna_feature->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna_feature->SetProduct().SetWhole(*md.mrna_sid);

    mrna_feature->SetLocation(*create_packed_int_seqloc(model));
                
    if(!model.HasStart() || !model.HasStop())
        mrna_feature->SetPartial(true);

    if(model.HasStart())
        mrna_feature->SetLocation().SetPartialStart(false,eExtreme_Biological);
    if(model.HasStop())
        mrna_feature->SetLocation().SetPartialStop(false,eExtreme_Biological);

    if(!model.FrameShifts().empty()) {
        mrna_feature->SetExcept(true);
        mrna_feature->SetExcept_text("unclassified transcription discrepancy");
    }
//     if(model.PStop()) {
//         mrna_feature->SetExcept(true);
//         if (mrna_feature->IsSetExcept_text() && mrna_feature->GetExcept_text().size()>0)
//             mrna_feature->SetExcept_text() += ", ";
//         else
//             mrna_feature->SetExcept_text(kEmptyStr);
//         mrna_feature->SetExcept_text() += "internal stop(s)";
//     }

    if (model.Status() & CGeneModel::ePseudo) {
        mrna_feature->SetPseudo(true);
    }

    CRef< CUser_object > user_obj = create_ModelEvidence_user_object();
    mrna_feature->SetExts().push_back(user_obj);
    user_obj->AddField("Method",CGeneModel::TypeToString(model.Type()));
    if (!model.Support().empty()) {
        CRef<CUser_field> support_field(new CUser_field());
        user_obj->SetData().push_back(support_field);
        support_field->SetLabel().SetStr("Support");
        vector<string> proteins;
        vector<string> mrnas;
        vector<string> ests;
        ITERATE(CSupportInfoSet,s,model.Support()) {
            string accession = s->Seqid()->GetSeqIdString(true);
            if (s->CoreAlignment())
                support_field->AddField("Core",accession);
            if (s->Type()&CGeneModel::eProt)
                proteins.push_back(accession);
            if (s->Type()&CGeneModel::emRNA)
                mrnas.push_back(accession);
            if (s->Type()&CGeneModel::eEST)
                ests.push_back(accession);
        }
        if (!proteins.empty())
            support_field->AddField("Proteins",proteins);
        if (!mrnas.empty())
            support_field->AddField("mRNAs",mrnas);
        if (!ests.empty())
            support_field->AddField("ESTs",ests);
    }

    if(!model.ProteinHit().empty())
        user_obj->AddField("BestTargetProteinHit",model.ProteinHit());
    if(model.Status()&CGeneModel::eFullSupCDS)
        user_obj->AddField("CDS support",string("full"));
    return mrna_feature;
}



CRef<CSeq_entry>  CAnnotationASN1::CImplementationData::create_prot_seq_entry(const SModelData& md) const
{
    const CGeneModel& model = md.model;

    CRef<CSeq_entry> sprot(new CSeq_entry);
    sprot->SetSeq().SetId().push_back(md.prot_sid);

    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    sprot->SetSeq().SetDescr().Set().push_back(desc);


    string strprot(model.GetProtein(contig_seq));
    if(model.HasStop()) strprot.resize(strprot.size() - 1);

    sprot->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    sprot->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    sprot->SetSeq().SetInst().SetLength(strprot.size());
    CRef<CSeq_data> dprot(new CSeq_data(strprot, CSeq_data::e_Ncbieaa));
    sprot->SetSeq().SetInst().SetSeq_data(*dprot);
    return sprot;
}

CRef<CSeq_entry> CAnnotationASN1::CImplementationData::create_mrna_seq_entry(SModelData& md)
{
    const CGeneModel& model = md.model;

    CRef<CSeq_entry> smrna(new CSeq_entry);
    smrna->SetSeq().SetId().push_back(md.mrna_sid);

    CRef<CSeqdesc> mdes(new CSeqdesc);
    smrna->SetSeq().SetDescr().Set().push_back(mdes);
    mdes->SetMolinfo().SetBiomol(CMolInfo::eBiomol_mRNA);

    smrna->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    smrna->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);

    CResidueVec mrna_seq_vec;
    md.mrna_map.EditedSequence(contig_seq, mrna_seq_vec);
    string mrna_seq_str((char*)&mrna_seq_vec[0],mrna_seq_vec.size());

    CRef<CSeq_data> dmrna(new CSeq_data(mrna_seq_str, CSeq_data::e_Iupacna));
    smrna->SetSeq().SetInst().SetSeq_data(*dmrna);
    smrna->SetSeq().SetInst().SetLength(mrna_seq_str.size());
    smrna->SetSeq().SetInst().SetHist().SetAssembly().push_back(model2spliced_seq_align(md));

    CRef<CSeq_annot> annot(new CSeq_annot);
    smrna->SetSeq().SetAnnot().push_back(annot);
    annot->SetData().SetFtable().push_back(create_cdregion_feature(md,eOnMrna));

    if (!model.Open5primeEnd()) {
        CRef<CSeq_feat> stop = create_5prime_stop_feature(md);
        if (stop.NotEmpty())
            annot->SetData().SetFtable().push_back(stop);
    }

    return smrna;
}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_gene_feature(const SModelData& md) const
{
    const CGeneModel& model = md.model;

    CRef<CSeq_feat> gene_feature(new CSeq_feat);

    CRef<CObject_id> obj_id( new CObject_id() );
    obj_id->SetStr("gene." + NStr::IntToString(model.GeneID()));
    CRef<CFeat_id> feat_id( new CFeat_id() );
    feat_id->SetLocal(*obj_id);
    gene_feature->SetIds().push_back(feat_id);

    gene_feature->SetData().SetGene().SetPseudo(model.Status() & CGeneModel::ePseudo);
    //    .SetLocus_tag("...");

    CRef<CSeq_loc> gene_location(new CSeq_loc());
    gene_feature->SetLocation(*gene_location);

    return gene_feature;
}

void CAnnotationASN1::CImplementationData::update_gene_feature(CSeq_feat& gene, CSeq_feat& mrna) const
{
    CRef< CSeqFeatXref > genexref( new CSeqFeatXref() );
    genexref->SetId(*gene.SetIds().front());
//     genexref->SetData().SetGene();
    
    CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
    mrnaxref->SetId(*mrna.SetIds().front());
//     mrnaxref->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);

    gene.SetXref().push_back(mrnaxref);
    mrna.SetXref().push_back(genexref);

    gene.SetLocation(*gene.GetLocation().Add(mrna.GetLocation(),CSeq_loc::fMerge_SingleRange,NULL));

    if (mrna.CanGetPartial() && mrna.GetPartial())
        gene.SetPartial(true);
}

CRef<CSpliced_exon> CAnnotationASN1::CImplementationData::spliced_exon (const CAlignExon& e, EStrand strand) const
{
    CRef<CSpliced_exon> se(new CSpliced_exon());
    // se->SetProduct_start(...); se->SetProduct_end(...); don't fill in here
    se->SetGenomic_start(e.GetFrom());
    se->SetGenomic_end(e.GetTo());
    if (e.m_fsplice) {
        string bases((char*)&contig_seq[e.GetFrom()-2], 2);
        if (strand==ePlus) {
            se->SetSplice_5_prime().SetBases(bases);
        } else {
            ReverseComplement(bases.begin(),bases.end());
            se->SetSplice_3_prime().SetBases(bases);
        }
    }
    if (e.m_ssplice) {
        string bases((char*)&contig_seq[e.GetTo()+1], 2);
        if (strand==ePlus) {
            se->SetSplice_3_prime().SetBases(bases);
        } else {
            ReverseComplement(bases.begin(),bases.end());
            se->SetSplice_5_prime().SetBases(bases);
        }
    }
    return se;
}

CRef< CSeq_align > CAnnotationASN1::CImplementationData::model2spliced_seq_align(SModelData& md)
{
    const CGeneModel& model = md.model;

    CRef< CSeq_align > seq_align( new CSeq_align );
    seq_align->SetType(CSeq_align::eType_partial);
    CSpliced_seg& spliced_seg = seq_align->SetSegs().SetSpliced();

    spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    spliced_seg.SetProduct_id(*md.mrna_sid);
    spliced_seg.SetGenomic_id(*contig_sid);
    spliced_seg.SetProduct_strand(eNa_strand_plus);
    spliced_seg.SetGenomic_strand(model.Strand()==ePlus?eNa_strand_plus:eNa_strand_minus);

    CSpliced_seg::TExons& exons = spliced_seg.SetExons();

    TFrameShifts frameshifts = model.FrameShifts();
    NON_CONST_ITERATE(TFrameShifts, fsi, frameshifts) {
        fsi->RestoreIfReplaced();
    }
    TFrameShifts::const_iterator indel_i = frameshifts.begin();
    ITERATE(CGeneModel::TExons, e, model.Exons()) {
        CRef<CSpliced_exon> se = spliced_exon(*e,model.Strand());
        int last_chunk = e->GetFrom();
        while (indel_i != frameshifts.end() && indel_i->Loc() <= e->GetTo()+1) {
            const CFrameShiftInfo& indel = *indel_i;
            _ASSERT( e->GetFrom() <= indel.Loc() );
            
            if (indel.Loc()-last_chunk > 0) {
                CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
                chunk->SetMatch(indel.Loc()-last_chunk);
                se->SetParts().push_back(chunk);
            }

            if (indel.IsInsertion()) {
                CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
                chunk->SetGenomic_ins(indel.Len());
                se->SetParts().push_back(chunk);

                last_chunk += indel.Len();
            } else {
                CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
                chunk->SetProduct_ins(indel.Len());
                se->SetParts().push_back(chunk);
            }
            ++indel_i;
        }
        if (e->GetFrom() < last_chunk && last_chunk < e->GetTo()) {
            CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
            chunk->SetMatch(e->GetTo()-last_chunk);
            se->SetParts().push_back(chunk);
        }

        exons.push_back(se);
    }
    _ASSERT( indel_i == frameshifts.end() );

    if (model.Strand() == eMinus) {
        //    reverse if minus strand (don't forget to reverse chunks)
        exons.reverse();
        NON_CONST_ITERATE(CSpliced_seg::TExons, exon_i, exons) {
            CSpliced_exon& se = **exon_i;
            if (se.IsSetParts())
                se.SetParts().reverse();
        }
    }

    //    assign product positions in exons
    int accumulated_product_len = 0;
    NON_CONST_ITERATE(CSpliced_seg::TExons, exon_i, exons) {
        CSpliced_exon& se = **exon_i;
        se.SetProduct_start().SetNucpos(accumulated_product_len);
        accumulated_product_len += model.FShiftedLen(se.GetGenomic_start(),se.GetGenomic_end());
        se.SetProduct_end().SetNucpos(accumulated_product_len-1);
    }

    return seq_align;
}

/*
void get_exons_from_seq_loc(CGeneModel& model, const CSeq_loc& loc)
{
    for( CSeq_loc_CI i(loc); i; ++i) {
        model.AddExon(i->GetRange());
        model.back().m_fsplice = i->GetFuzzFrom()==NULL;
        model.back().m_ssplice = i->GetFuzzTo()==NULL;
    }
    model.RecalculateLimits();
}

struct collect_indels : public unary_function<CRef<CDelta_seq>, void>
{
    collect_indels(CGeneModel& m) : model(m), exon_i(m.begin()), next_seg_start(m.Limits().GetFrom()) {}
    void operator() (CRef<CDelta_seq> delta_seq)
    {
        if (delta_seq->IsLiteral()) {
            const CSeq_literal& literal = delta_seq->GetLiteral();
            if (literal.GetLength()==0) return;
            CSeq_data seq_data;
            CSeqportUtil::Convert(literal.GetSeq_data(), &seq_data, CSeq_data::e_Iupacna);
            if (model.Strand() == eMinus)
                ReverseComplement(&seq_data);
            model.FrameShifts().push_back(CFrameShiftInfo(next_seg_start,literal.GetLength(), false, seq_data.GetIupacna()));
        } else {
            const CSeq_interval& loc = delta_seq->GetLoc().GetInt();
            while (exon_i->GetTo() < loc.GetFrom()) {
                if (next_seg_start <= exon_i->GetTo())
                    model.FrameShifts().push_back(CFrameShiftInfo(next_seg_start,exon_i->GetTo()-next_seg_start+1, true));
                ++exon_i;
                next_seg_start = exon_i->GetFrom();
            }
            if (next_seg_start < loc.GetFrom())
                model.FrameShifts().push_back(CFrameShiftInfo(next_seg_start,loc.GetFrom()-next_seg_start, true));
            next_seg_start = loc.GetTo();
        }
    }
    void flush()
    {
        for(;;) {
            if (next_seg_start <= exon_i->GetTo())
                model.FrameShifts().push_back(CFrameShiftInfo(next_seg_start,exon_i->GetTo()-next_seg_start+1, true));
            if ( ++exon_i == m.end())
                break;
            next_seg_start = exon_i->GetFrom();
        }
    }
    CGeneModel& model;
    CGeneModel::iterator exon_i;
    TSignedSeqPos next_seg_start;
};
*/
END_SCOPE(gnomon)
END_NCBI_SCOPE
