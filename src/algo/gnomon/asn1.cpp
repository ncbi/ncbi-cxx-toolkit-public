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
#include <algo/gnomon/id_handler.hpp>

#include <objects/seq/seqport_util.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)
USING_SCOPE(ncbi::objects);

// defined in gnomon_model.cpp
typedef map<string,string> TAttributes;
void CollectAttributes(const CAlignModel& a, TAttributes& attributes);
void ParseAttributes(TAttributes& attributes, CAlignModel& a);

// defined in gnomon_objmgr.cpp
int GetModelId(const CSeq_align& seq_align);


const string kGnomonConstructed = "Is [re]constructed alignment";


struct SModelData {
    SModelData(const CAlignModel& model, const CEResidueVec& contig_seq);

    const CAlignModel& model;
    CEResidueVec mrna_seq;
    CRef<CSeq_id> mrna_sid;
    CRef<CSeq_id> prot_sid;

    bool is_ncrna;
};

SModelData::SModelData(const CAlignModel& m, const CEResidueVec& contig_seq) : model(m)
{
    m.GetAlignMap().EditedSequence(contig_seq, mrna_seq, true);

    prot_sid.Reset(new CSeq_id);
    prot_sid->Assign(*CIdHandler::GnomonProtein(model.ID())); 
    mrna_sid.Reset(new CSeq_id);
    mrna_sid->Assign(*CIdHandler::GnomonMRNA(model.ID()));

    is_ncrna = m.ReadingFrame().Empty();
}


class CAnnotationASN1::CImplementationData {
public:
    CImplementationData(const string& contig_name, const CResidueVec& seq, IEvidence& evdnc);

    void AddModel(const CAlignModel& model);
    CRef<CSeq_entry> main_seq_entry;

private:
    CRef<CSeq_entry> CreateModelProducts(SModelData& model);
    CRef<CSeq_feat> create_gene_feature(const SModelData& md) const;
    void update_gene_feature(CSeq_feat& gene, CSeq_feat& mrna_feat) const;
    CRef<CSeq_entry>  create_prot_seq_entry(const SModelData& md, const CSeq_entry& mrna_seq_entry, const CSeq_feat& cdregion_feature) const;
    CRef<CSeq_entry> create_mrna_seq_entry(SModelData& md, CRef<CSeq_feat> cdregion_feature);
    CRef<CSeq_feat> create_mrna_feature(SModelData& md);
    CRef<CSeq_feat> create_internal_feature(const SModelData& md);
    enum EWhere { eOnGenome, eOnMrna };
    CRef<CSeq_feat> create_cdregion_feature(SModelData& md,EWhere onWhat);
    CRef<CSeq_feat> create_5prime_stop_feature(SModelData& md) const;
    CRef<CSeq_align> model2spliced_seq_align(SModelData& md);
    CRef<CSpliced_exon> spliced_exon (const CModelExon& e, EStrand strand) const;
    CRef<CSeq_loc> create_packed_int_seqloc(const CGeneModel& model, TSignedSeqRange limits = TSignedSeqRange::GetWhole());

    void AddInternalFeature(const SModelData& md);
    void DumpEvidence(const SModelData& md);
    void DumpUnusedChains();

    string contig_name;
    CRef<CSeq_id> contig_sid;
    const CResidueVec& contig_seq;
    CDoubleStrandSeq  contig_ds_seq;

    CBioseq_set::TSeq_set* nucprots;
    CSeq_annot::C_Data::TFtable* feature_table;
    CSeq_annot::C_Data::TAlign*  model_alignments;
    CSeq_annot::C_Data::TFtable* internal_feature_table;
    set<int> models_in_internal_feature_table;

    typedef map<int,CRef<CSeq_feat> > TGeneMap;
    TGeneMap genes;
    IEvidence& evidence;

    friend class CAnnotationASN1;
};

CAnnotationASN1::CImplementationData::CImplementationData(const string& a_contig_name, const CResidueVec& seq, IEvidence& evdnc) :
    main_seq_entry(new CSeq_entry),
    contig_name(a_contig_name),
    contig_sid(CIdHandler::ToSeq_id(a_contig_name)), contig_seq(seq),
    evidence(evdnc)
{
    Convert(contig_seq, contig_ds_seq);

    CBioseq_set& bioseq_set = main_seq_entry->SetSet();

    nucprots = &bioseq_set.SetSeq_set();

    CRef<CSeq_annot> seq_annot(new CSeq_annot);
    seq_annot->AddName("Gnomon models");
    seq_annot->SetTitle("Gnomon models");
    CRef<CAnnotdesc> desc(new CAnnotdesc());
    CRef<CSeq_loc> genomic_seqloc(new CSeq_loc());
    genomic_seqloc->SetWhole(*contig_sid);
    desc->SetRegion(*genomic_seqloc);
    seq_annot->SetDesc().Set().push_back(desc);

    bioseq_set.SetAnnot().push_back(seq_annot);
    feature_table = &seq_annot->SetData().SetFtable();

    seq_annot.Reset(new CSeq_annot);
    seq_annot->AddName("Gnomon internal attributes");
    seq_annot->SetTitle("Gnomon internal attributes");
    bioseq_set.SetAnnot().push_back(seq_annot);
    internal_feature_table = &seq_annot->SetData().SetFtable();

    model_alignments = NULL;
#ifdef _EVIDENCE_WANTED
    seq_annot.Reset(new CSeq_annot);
    bioseq_set.SetAnnot().push_back(seq_annot);
    model_alignments = &seq_annot->SetData().SetAlign();
    seq_annot->AddName("Model Alignments");
    seq_annot->SetTitle("Model Alignments");
#endif
}

CAnnotationASN1::CAnnotationASN1(const string& contig_name, const CResidueVec& seq, IEvidence& evdnc) :
    m_data(new CImplementationData(contig_name, seq, evdnc))
{
}

CAnnotationASN1::~CAnnotationASN1()
{
}

CRef<CSeq_entry> CAnnotationASN1::GetASN1() const
{
#ifdef _EVIDENCE_WANTED
    m_data->DumpUnusedChains();
#endif
    return m_data->main_seq_entry;
}


void CAnnotationASN1::AddModel(const CAlignModel& model)
{
    m_data->AddModel(model);
}

CRef<CSeq_entry> CAnnotationASN1::CImplementationData::CreateModelProducts(SModelData& md)
{
    CRef<CSeq_entry> model_products(new CSeq_entry);
    nucprots->push_back(model_products);
    model_products->SetSet().SetClass(CBioseq_set::eClass_nuc_prot);

    
    CRef<CSeq_feat> cdregion_feature;
    if (!md.is_ncrna)
        cdregion_feature = create_cdregion_feature(md,eOnMrna);
    CRef<CSeq_entry> mrna_seq_entry = create_mrna_seq_entry(md, cdregion_feature);
    model_products->SetSet().SetSeq_set().push_back(mrna_seq_entry);

    if (!md.is_ncrna)
        model_products->SetSet().SetSeq_set().push_back(create_prot_seq_entry(md, *mrna_seq_entry, *cdregion_feature));

    return mrna_seq_entry;
}


void CAnnotationASN1::CImplementationData::AddInternalFeature(const SModelData& md)
{
    int id = md.model.ID();
    if (models_in_internal_feature_table.find(id) == models_in_internal_feature_table.end()) {
        CRef<CSeq_feat> internal_feat = create_internal_feature(md);
        internal_feature_table->push_back(internal_feat);
        models_in_internal_feature_table.insert(id);
    }
}

void CAnnotationASN1::CImplementationData::AddModel(const CAlignModel& model)
{
    SModelData md(model, contig_ds_seq[ePlus]);

    CRef<CSeq_entry> mrna_seq_entry = CreateModelProducts(md);

    if (model_alignments != NULL)
        model_alignments->push_back(
                                    mrna_seq_entry->GetSeq().GetInst().GetHist().GetAssembly().front()
                                    );


    CRef<CSeq_feat> mrna_feat = create_mrna_feature(md);
    feature_table->push_back(mrna_feat);

    AddInternalFeature(md);

    if (!md.is_ncrna) {
        CRef<CSeq_feat> cds_feat;
        cds_feat = create_cdregion_feature(md,eOnGenome);
        feature_table->push_back(cds_feat);

        CRef< CSeqFeatXref > cdsxref( new CSeqFeatXref() );
        cdsxref->SetId(*cds_feat->SetIds().front());
        //     cdsxref->SetData().SetCdregion();
        
        CRef< CSeqFeatXref > mrnaxref( new CSeqFeatXref() );
        mrnaxref->SetId(*mrna_feat->SetIds().front());
        //     mrnaxref->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
        
        cds_feat->SetXref().push_back(mrnaxref);
        mrna_feat->SetXref().push_back(cdsxref);

    }
    if (model.GeneID()) {
        TGeneMap::iterator gene = genes.find(model.GeneID());
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
                from = model.GetAlignMap().MapOrigToEdited(from);
                to = model.GetAlignMap().MapOrigToEdited(to);
                
                if (strand==eMinus)
                    swap(from,to);
                _ASSERT(from+2==to);
                pstop.Reset(new CSeq_loc(*md.mrna_sid, from, to, eNa_strand_plus));
                break;
            case eOnGenome:
                _ASSERT(model.GetAlignMap().FShiftedLen(from,to)==3);
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
        altstart = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetFrom());
        start = model.GetAlignMap().MapOrigToEdited(model.GetCdsInfo().Cds().GetFrom());
        stop = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetTo());
    } else {
        altstart = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetTo());
        start = model.GetAlignMap().MapOrigToEdited(model.GetCdsInfo().Cds().GetTo());
        stop = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetFrom());
    }

    int frame = 0;
    if(!model.HasStart()) {
        _ASSERT(altstart == 0);
        frame = start%3;
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

    if (!model.HasStart())
        cdregion_location->SetPartialStart(true,eExtreme_Biological);
    if (!model.HasStop())
        cdregion_location->SetPartialStop(true,eExtreme_Biological);
    cdregion_feature->SetLocation(*cdregion_location);

    if (!model.HasStart() || !model.HasStop() || !model.Continuous())
        cdregion_feature->SetPartial(true);

    return cdregion_feature;
}


CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_5prime_stop_feature(SModelData& md) const
{
    const CGeneModel& model = md.model;


    int frame = -1;
    TIVec starts[3], stops[3];

    FindStartsStops(model, contig_ds_seq[model.Strand()], md.mrna_seq, model.GetAlignMap(), starts, stops, frame);
    _ASSERT( starts[frame].size()>0 && stops[frame].size()>0 );

    TSignedSeqPos stop_5prime = stops[frame][0];
    if (FindUpstreamStop(stops[frame],starts[frame][0],stop_5prime) && stop_5prime>=0) {
        CRef<CSeq_feat> stop_5prime_feature(new CSeq_feat);
        stop_5prime_feature->SetData().SetImp().SetKey("misc_feature");
        stop_5prime_feature->SetComment("upstream in-frame stop codon");
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

    for (size_t i=0; i < model.Exons().size(); ++i) {
        const CModelExon* e = &model.Exons()[i];
        TSignedSeqRange interval_range = e->Limits() & limits;
        if (interval_range.Empty())
            continue;
        CRef<CSeq_interval> interval(new CSeq_interval(*contig_sid, interval_range.GetFrom(),interval_range.GetTo(), strand));
        
        if (!e->m_fsplice && 0 < i) {
            interval->SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
        }
        if (!e->m_ssplice && i < model.Exons().size()-1) {
            interval->SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
        }
        
        packed_int.AddInterval(*interval);
    }
    return seq_loc->Merge(CSeq_loc::fSort, NULL);
}

void ExpandSupport(const CSupportInfoSet& src, CSupportInfoSet& dst, IEvidence& evidence)
{
    ITERATE(CSupportInfoSet, s, src) {
        dst.insert(*s);

        const CAlignModel* m = evidence.GetModel(s->GetId());
        if (m && (m->Type()&(CAlignModel::eChain|CAlignModel::eGnomon))!=0) {
            _ASSERT( !s->IsCore() );
            ExpandSupport(m->Support(), dst, evidence);
        }
    }
}

void CAnnotationASN1::CImplementationData::DumpEvidence(const SModelData& md)
{
    const CGeneModel& model = md.model;
    evidence.GetModel(model.ID()); // complete chains might not get marked as used otherwise

    if (model.Support().empty())
        return;
    CRef<CSeq_annot> seq_annot(new CSeq_annot);
    main_seq_entry->SetSet().SetAnnot().push_back(seq_annot);
    CSeq_annot::C_Data::TAlign* aligns = &seq_annot->SetData().SetAlign();
    seq_annot->AddName("Evidence for "+CIdHandler::ToString(*md.mrna_sid));
    seq_annot->SetTitle("Evidence for "+CIdHandler::ToString(*md.mrna_sid));
    
    
    ITERATE(CSupportInfoSet, s, model.Support()) {
        int id = s->GetId();

        const CAlignModel* m = evidence.GetModel(id);
        auto_ptr<SModelData> smd;
        if (m != NULL) {
            smd.reset( new SModelData(*m, contig_ds_seq[ePlus]) );
            AddInternalFeature(*smd);
        }

        CRef<CSeq_align> a(const_cast<CSeq_align*>(evidence.GetSeq_align(id).GetPointerOrNull()));
        if (a.NotEmpty()) {
            aligns->push_back(a);
            continue;
        }

        if (m == NULL)
            continue;

        if (m->Type()&CGeneModel::eChain) {
            CreateModelProducts(*smd);
            DumpEvidence(*smd);
        } else {
            smd->mrna_sid->Assign(*m->GetTargetId()); 
        }
        aligns->push_back(model2spliced_seq_align(*smd));
    }
}

void CAnnotationASN1::CImplementationData::DumpUnusedChains()
{
    CRef<CSeq_annot> seq_annot(new CSeq_annot);
    main_seq_entry->SetSet().SetAnnot().push_back(seq_annot);
    CSeq_annot::C_Data::TAlign* aligns = &seq_annot->SetData().SetAlign();
    seq_annot->AddName("Unused chains");
    seq_annot->SetTitle("Unused chains");

    auto_ptr<IEvidence::iterator> it( evidence.GetUnusedModels(contig_name) );
    const CAlignModel* m;
    while ((m = it->GetNext()) != NULL) {
        if ((m->Type()&CAlignModel::eChain) == 0)
            continue;
        
        SModelData smd(*m, contig_ds_seq[ePlus]);

        aligns->push_back(model2spliced_seq_align(smd));

        AddInternalFeature(smd);
        CreateModelProducts(smd);
        DumpEvidence(smd);
    }
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

    mrna_feature->SetData().SetRna().SetType(md.is_ncrna ? CRNA_ref::eType_ncRNA : CRNA_ref::eType_mRNA);
    mrna_feature->SetProduct().SetWhole(*md.mrna_sid);

    mrna_feature->SetLocation(*create_packed_int_seqloc(model));
                
    if(!model.HasStart())
        mrna_feature->SetLocation().SetPartialStart(true,eExtreme_Biological);
    if(!model.HasStop())
        mrna_feature->SetLocation().SetPartialStop(true,eExtreme_Biological);

    if(!model.HasStart() || !model.HasStop() || !model.Continuous())
        mrna_feature->SetPartial(true);

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

#ifdef _EVIDENCE_WANTED
    DumpEvidence(md);
#endif

    CRef< CUser_object > user_obj = create_ModelEvidence_user_object();
    mrna_feature->SetExts().push_back(user_obj);
    user_obj->AddField("Method",CGeneModel::TypeToString(model.Type()));
    if (!model.Support().empty()) {
        CRef<CUser_field> support_field(new CUser_field());
        user_obj->SetData().push_back(support_field);
        support_field->SetLabel().SetStr("Support");
        vector<string> chains;
        vector<string> cores;
        vector<string> proteins;
        vector<string> mrnas;
        vector<string> ests;
        vector<string> unknown;

        CSupportInfoSet support;

        ExpandSupport(model.Support(), support, evidence);

        ITERATE(CSupportInfoSet, s, support) {
            
            int id = s->GetId();
            
            const CAlignModel* m = evidence.GetModel(id);

            int type = m ?  m->Type() : 0;

            string accession;
            if (m == NULL || (m->Type()&CGeneModel::eChain)) {
                accession = CIdHandler::ToString(*CIdHandler::GnomonMRNA(id));
            } else {
                accession = CIdHandler::ToString(*m->GetTargetId());
            }

            if (s->IsCore())
                cores.push_back(accession);

            if (type&CGeneModel::eChain)
                chains.push_back(accession);
            else if (type&CGeneModel::eProt)
                proteins.push_back(accession);
            else if (type&CGeneModel::emRNA)
                mrnas.push_back(accession);
            else if (type&CGeneModel::eEST)
                ests.push_back(accession);
            else
                unknown.push_back(accession);
        }
        if (!chains.empty()) {
            support_field->AddField("Chains",chains);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(chains.size());
        }
        if (!cores.empty()) {
            support_field->AddField("Core",cores);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(cores.size());
        }
        if (!proteins.empty()) {
            sort(proteins.begin(),proteins.end());
            support_field->AddField("Proteins",proteins);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(proteins.size());
        }
        if (!mrnas.empty()) {
            sort(mrnas.begin(),mrnas.end());
            support_field->AddField("mRNAs",mrnas);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(mrnas.size());
        }
        if (!ests.empty()) {
            sort(ests.begin(),ests.end());
            support_field->AddField("ESTs",ests);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(ests.size());
        }
        if (!unknown.empty()) {
            support_field->AddField("unknown",unknown);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(unknown.size());
        }
    }

    if(!model.ProteinHit().empty())
        user_obj->AddField("BestTargetProteinHit",model.ProteinHit());
    if(model.Status()&CGeneModel::eFullSupCDS)
        user_obj->AddField("CDS support",string("full"));
    return mrna_feature;
}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_internal_feature(const SModelData& md)
{
    const CAlignModel& model = md.model;

    CRef<CSeq_feat> feature(new CSeq_feat);

    CRef<CObject_id> obj_id( new CObject_id() );
    obj_id->SetId(model.ID());
    CRef<CFeat_id> feat_id( new CFeat_id() );
    feat_id->SetLocal(*obj_id);
    feature->SetIds().push_back(feat_id);

    CRef<CUser_object> user( new CUser_object);
    user->SetClass("Gnomon");
    user->SetType().SetStr("Model Internal Attributes");
    feature->SetExts().push_back(user);

    if (model.Type() & (CGeneModel::eGnomon | CGeneModel::eChain))
        user->AddField("Method", CGeneModel::TypeToString(model.Type()));

    TAttributes attributes;
    CollectAttributes(model, attributes);
    if (model.GetTargetId().NotEmpty())
        attributes["Target"] = CIdHandler::ToString(*model.GetTargetId());
    ITERATE (TAttributes, a, attributes) {
        if (!a->second.empty())
            user->AddField(a->first, a->second);
    }
   
    if (model.Score() != BadScore())
        user->AddField("cds_score", model.Score());

    if (model.RealCdsLimits().NotEmpty()) {
        // create cdregion feature as there is no other place to show CDS on evidence alignment

        feature->SetLocation(*create_packed_int_seqloc(model,model.RealCdsLimits()));
        CRef<CGenetic_code::C_E> cdrcode(new CGenetic_code::C_E);
        cdrcode->SetId(1);
        feature->SetData().SetCdregion().SetCode().Set().push_back(cdrcode);

        int frame = 0;
        if(!model.HasStart()) {
            int start, altstart;
            if (model.Strand()==ePlus) {
                altstart = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetFrom());
                start = model.GetAlignMap().MapOrigToEdited(model.GetCdsInfo().Cds().GetFrom());
            } else {
                altstart = model.GetAlignMap().MapOrigToEdited(model.MaxCdsLimits().GetTo());
                start = model.GetAlignMap().MapOrigToEdited(model.GetCdsInfo().Cds().GetTo());
            }
            frame = (start-altstart)%3;
        }
        CCdregion::EFrame ncbi_frame = CCdregion::eFrame_one;
        if(frame == 1) ncbi_frame = CCdregion::eFrame_two;
        else if(frame == 2) ncbi_frame = CCdregion::eFrame_three; 
        feature->SetData().SetCdregion().SetFrame(ncbi_frame);
    } else {
        feature->SetLocation(*create_packed_int_seqloc(model));
        feature->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    }

    return feature;
}


CRef<CSeq_entry>  CAnnotationASN1::CImplementationData::create_prot_seq_entry(const SModelData& md, const CSeq_entry& mrna_seq_entry, const CSeq_feat& cdregion_feature) const
{
    const CGeneModel& model = md.model;

    CRef<CSeq_entry> sprot(new CSeq_entry);
//     CRef<CBioseq> prot_bioseq = CSeqTranslator::TranslateToProtein(cdregion_feature, scope);
//     sprot->SetSeq(*prot_bioseq);
    sprot->SetSeq().SetId().push_back(md.prot_sid);

    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    desc->SetMolinfo().SetCompleteness(
	model.Continuous()?
	    (model.HasStart()?
		(model.HasStop()?
		    CMolInfo::eCompleteness_complete:
                    CMolInfo::eCompleteness_has_left):
                (model.HasStop()?
                    CMolInfo::eCompleteness_has_right:
                    CMolInfo::eCompleteness_no_ends)):
	    CMolInfo::eCompleteness_partial
	);
    sprot->SetSeq().SetDescr().Set().push_back(desc);


    string strprot;
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CScope scope(*obj_mgr);
    scope.AddTopLevelSeqEntry(mrna_seq_entry);
    CSeqTranslator::Translate(cdregion_feature, scope, strprot, false);

    CSeq_inst& seq_inst = sprot->SetSeq().SetInst();
    seq_inst.SetMol(CSeq_inst::eMol_aa);
    seq_inst.SetLength(strprot.size());

    if (model.Continuous()) {
        seq_inst.SetRepr(CSeq_inst::eRepr_raw);
        CRef<CSeq_data> dprot(new CSeq_data(strprot, CSeq_data::e_Ncbieaa));
        sprot->SetSeq().SetInst().SetSeq_data(*dprot);
    } else {
        seq_inst.SetRepr(CSeq_inst::eRepr_delta);
        CSeqVector seqv(cdregion_feature.GetLocation(), scope, CBioseq_Handle::eCoding_Ncbi);
        CConstRef<CSeqMap> map;
        map.Reset(&seqv.GetSeqMap());

        const CCdregion& cdr = cdregion_feature.GetData().GetCdregion();
        int frame = 0;
        if (cdr.IsSetFrame ()) {
            switch (cdr.GetFrame ()) {
            case CCdregion::eFrame_two :
                frame = 1;
                break;
            case CCdregion::eFrame_three :
                frame = 2;
                break;
            default :
                break;
            }
        }

        size_t b = 0;
        size_t e = 0;

        for( CSeqMap_CI ci = map->BeginResolved(&scope); ci; ) {
            TSeqPos len = ci.GetLength() - frame;
            frame = 0;
            e = b + (len+2)/3;
            if (ci.IsUnknownLength()) {
                seq_inst.SetExt().SetDelta().AddLiteral(len);
                seq_inst.SetExt().SetDelta().Set().back()->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            } else {
                if (e > strprot.size()) {
                    _ASSERT( len%3 != 0 || md.model.HasStop() );
                    --e;
                }
                if (b < e)
                    seq_inst.SetExt().SetDelta().AddLiteral(strprot.substr(b,e-b),CSeq_inst::eMol_aa);
                
            }
            b = e;

            ci.Next();

            _ASSERT( len%3 == 0 || !ci );
        }
        _ASSERT( b == strprot.size() );
    }
#ifdef _DEBUG
    scope.AddTopLevelSeqEntry(*sprot);
    CBioseq_Handle prot_h = scope.GetBioseqHandle(*md.prot_sid);
    CSeqVector vec(prot_h, CBioseq_Handle::eCoding_Iupac);
    string result;
    vec.GetSeqData(0, vec.size(), result);
    _ASSERT( strprot==result );
#endif
    return sprot;
}

CRef<CSeq_entry> CAnnotationASN1::CImplementationData::create_mrna_seq_entry(SModelData& md, CRef<CSeq_feat> cdregion_feature)
{
    const CGeneModel& model = md.model;

    CRef<CSeq_entry> smrna(new CSeq_entry);
    smrna->SetSeq().SetId().push_back(md.mrna_sid);

    CRef<CSeqdesc> mdes(new CSeqdesc);
    smrna->SetSeq().SetDescr().Set().push_back(mdes);
    mdes->SetMolinfo().SetBiomol(md.is_ncrna ? CMolInfo::eBiomol_ncRNA : CMolInfo::eBiomol_mRNA);

    mdes->SetMolinfo().SetCompleteness(
        model.Continuous()?
            (model.HasStart()?
                (model.HasStop()?
                    CMolInfo::eCompleteness_unknown:
                    CMolInfo::eCompleteness_no_right):
                (model.HasStop()?
                    CMolInfo::eCompleteness_no_left:
                    CMolInfo::eCompleteness_no_ends)):
            CMolInfo::eCompleteness_partial
        );

    CResidueVec mrna_seq_vec;
    model.GetAlignMap().EditedSequence(contig_seq, mrna_seq_vec, true);
    string mrna_seq_str((char*)&mrna_seq_vec[0],mrna_seq_vec.size());

    CSeq_inst& seq_inst = smrna->SetSeq().SetInst();
    seq_inst.SetMol(CSeq_inst::eMol_rna);
    seq_inst.SetLength(mrna_seq_str.size());

    if (model.Continuous()) {
        seq_inst.SetRepr(CSeq_inst::eRepr_raw);
        CRef<CSeq_data> dmrna(new CSeq_data(mrna_seq_str, CSeq_data::e_Iupacna));
        CSeqportUtil::Pack(dmrna.GetPointer());
        seq_inst.SetSeq_data(*dmrna);
    } else {
        seq_inst.SetRepr(CSeq_inst::eRepr_delta);
        TSeqPos b = 0;
        TSeqPos e = 0;

        if (model.Strand()==ePlus) {
            for (size_t i=0; i< model.Exons().size()-1; ++i) {
                const CModelExon& e1= model.Exons()[i];
                const CModelExon& e2= model.Exons()[i+1];
                if (e1.m_ssplice && e2.m_fsplice)
                    continue;
                e = model.GetAlignMap().MapOrigToEdited(e1.GetTo())+1;
                seq_inst.SetExt().SetDelta().AddLiteral(mrna_seq_str.substr(b,e-b),CSeq_inst::eMol_rna);
                b =  model.GetAlignMap().MapOrigToEdited(e2.GetFrom());

                seq_inst.SetExt().SetDelta().AddLiteral(b-e);
                seq_inst.SetExt().SetDelta().Set().back()->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            }
        } else {
            for (size_t i=model.Exons().size()-1; i > 0; --i) {
                const CModelExon& e1= model.Exons()[i-1];
                const CModelExon& e2= model.Exons()[i];
                if (e1.m_ssplice && e2.m_fsplice)
                    continue;
                e = model.GetAlignMap().MapOrigToEdited(e2.GetFrom())+1;
                seq_inst.SetExt().SetDelta().AddLiteral(mrna_seq_str.substr(b,e-b),CSeq_inst::eMol_rna);
                b =  model.GetAlignMap().MapOrigToEdited(e1.GetTo());

                seq_inst.SetExt().SetDelta().AddLiteral(b-e);
                seq_inst.SetExt().SetDelta().Set().back()->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
            }
        }
        seq_inst.SetExt().SetDelta().AddLiteral(mrna_seq_str.substr(b),CSeq_inst::eMol_rna);
    }

    smrna->SetSeq().SetInst().SetHist().SetAssembly().push_back(model2spliced_seq_align(md));

    if (!cdregion_feature.Empty()) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        smrna->SetSeq().SetAnnot().push_back(annot);
    
        annot->SetData().SetFtable().push_back(cdregion_feature);

        if (!model.Open5primeEnd()) {
            CRef<CSeq_feat> stop = create_5prime_stop_feature(md);
            if (stop.NotEmpty()) 
                annot->SetData().SetFtable().push_back(stop);
        }
    }
    return smrna;
}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_gene_feature(const SModelData& md) const
{
    const CGeneModel& model = md.model;

    CRef<CSeq_feat> gene_feature(new CSeq_feat);

    string gene_id_str = "gene." + NStr::IntToString(model.GeneID());

    CRef<CObject_id> obj_id( new CObject_id() );
    obj_id->SetStr(gene_id_str);
    CRef<CFeat_id> feat_id( new CFeat_id() );
    feat_id->SetLocal(*obj_id);
    gene_feature->SetIds().push_back(feat_id);
    gene_feature->SetData().SetGene().SetDesc(gene_id_str);

    if (model.Status() & CGeneModel::ePseudo) {
        gene_feature->SetData().SetGene().SetPseudo(true);

        int frameshifts = model.FrameShifts().size();
        int pstops = model.GetCdsInfo().PStops().size();
        _ASSERT( frameshifts+pstops > 0 );
        string frameshift_comment;
        string pstop_comment;
        if (frameshifts==1)
            frameshift_comment = "remove a frameshift";
        else if (frameshifts>1)
            frameshift_comment = "remove frameshifts";
        if (pstops==1)
            pstop_comment = "prevent a premature stop codon";
        else if (pstops>1)
            pstop_comment = "prevent premature stop codons";
        if (frameshifts>0 && pstops>0)
            pstop_comment = " and "+pstop_comment;
            
        gene_feature->SetComment("The sequence of the transcript was modified to "+frameshift_comment+pstop_comment+" represented in this assembly.");
    }
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

CRef<CSpliced_exon> CAnnotationASN1::CImplementationData::spliced_exon (const CModelExon& e, EStrand strand) const
{
    CRef<CSpliced_exon> se(new CSpliced_exon());
    // se->SetProduct_start(...); se->SetProduct_end(...); don't fill in here
    se->SetGenomic_start(e.GetFrom());
    se->SetGenomic_end(e.GetTo());
    if (e.m_fsplice) {
        string bases((char*)&contig_seq[e.GetFrom()-2], 2);
        if (strand==ePlus) {
            se->SetAcceptor_before_exon().SetBases(bases);
        } else {
            ReverseComplement(bases.begin(),bases.end());
            se->SetDonor_after_exon().SetBases(bases);
        }
    }
    if (e.m_ssplice) {
        string bases((char*)&contig_seq[e.GetTo()+1], 2);
        if (strand==ePlus) {
            se->SetDonor_after_exon().SetBases(bases);
        } else {
            ReverseComplement(bases.begin(),bases.end());
            se->SetAcceptor_before_exon().SetBases(bases);
        }
    }
    return se;
}

CRef< CSeq_align > CAnnotationASN1::CImplementationData::model2spliced_seq_align(SModelData& md)
{
    const CAlignModel& model = md.model;

    CRef< CSeq_align > seq_align( new CSeq_align );

    CRef<CObject_id> id(new CObject_id());
    id->SetId(model.ID());
    seq_align->SetId().push_back(id);

    seq_align->SetType(CSeq_align::eType_partial);
    CSpliced_seg& spliced_seg = seq_align->SetSegs().SetSpliced();

    spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    spliced_seg.SetProduct_length(model.TargetLen());
    if (model.Status() & CAlignModel::ePolyA) {
        spliced_seg.SetPoly_a((model.Status() & CAlignModel::eReversed)? model.PolyALen() - 1 : model.TargetLen() - model.PolyALen());
    }

    spliced_seg.SetProduct_id(*md.mrna_sid);
    spliced_seg.SetGenomic_id(*contig_sid);
    spliced_seg.SetProduct_strand((model.Status() & CGeneModel::eReversed)==0 ? eNa_strand_plus : eNa_strand_minus);
    spliced_seg.SetGenomic_strand(model.Strand()==ePlus?eNa_strand_plus:eNa_strand_minus);

    CSpliced_seg::TExons& exons = spliced_seg.SetExons();

    TInDels indels = model.GetInDels(false);

    TInDels::const_iterator indel_i = indels.begin();
    for (size_t i=0; i < model.Exons().size(); ++i) {
        const CModelExon *e = &model.Exons()[i]; 

        CRef<CSpliced_exon> se = spliced_exon(*e,model.Strand());

        TSignedSeqRange transcript_exon = model.TranscriptExon(i);
        se->SetProduct_start().SetNucpos(transcript_exon.GetFrom());
        se->SetProduct_end().SetNucpos(transcript_exon.GetTo());

        int last_chunk = e->GetFrom();
        while (indel_i != indels.end() && indel_i->Loc() <= e->GetTo()+1) {
            const CInDelInfo& indel = *indel_i;
            _ASSERT( e->GetFrom() <= indel.Loc() );
            
            if (indel.Loc()-last_chunk > 0) {
                CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
                chunk->SetMatch(indel.Loc()-last_chunk);
                se->SetParts().push_back(chunk);
                last_chunk = indel.Loc();
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
        if (e->GetFrom() <= last_chunk && last_chunk <= e->GetTo()) {
            CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
            chunk->SetMatch(e->GetTo()-last_chunk+1);
            se->SetParts().push_back(chunk);
        }

        exons.push_back(se);
    }
    _ASSERT( indel_i == indels.end() );

    if (model.Strand() == eMinus) {
        //    reverse if minus strand (don't forget to reverse chunks)
        exons.reverse();
        NON_CONST_ITERATE(CSpliced_seg::TExons, exon_i, exons) {
            CSpliced_exon& se = **exon_i;
            if (se.IsSetParts())
                se.SetParts().reverse();
        }
    }

    CRef<CUser_object> user( new CUser_object);
    user->SetClass("Gnomon");
    CRef< CObject_id > type(new CObject_id);
    type->SetStr("AlignmentAttributes");
    user->SetType(*type);
    seq_align->SetExt().push_back(user);
    user->AddField(kGnomonConstructed, true);

#ifdef _DEBUG
    try {
        seq_align->Validate(true);
    } catch (...) {
        _ASSERT(false);
    }
//     try {
//         CNcbiOstrstream ostream;
//         ostream << MSerial_AsnBinary << *seq_align;
//     } catch (...) {
//         _ASSERT(false);
//     }
#endif

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
            model.FrameShifts().push_back(CInDelInfo(next_seg_start,literal.GetLength(), false, seq_data.GetIupacna()));
        } else {
            const CSeq_interval& loc = delta_seq->GetLoc().GetInt();
            while (exon_i->GetTo() < loc.GetFrom()) {
                if (next_seg_start <= exon_i->GetTo())
                    model.FrameShifts().push_back(CInDelInfo(next_seg_start,exon_i->GetTo()-next_seg_start+1, true));
                ++exon_i;
                next_seg_start = exon_i->GetFrom();
            }
            if (next_seg_start < loc.GetFrom())
                model.FrameShifts().push_back(CInDelInfo(next_seg_start,loc.GetFrom()-next_seg_start, true));
            next_seg_start = loc.GetTo();
        }
    }
    void flush()
    {
        for(;;) {
            if (next_seg_start <= exon_i->GetTo())
                model.FrameShifts().push_back(CInDelInfo(next_seg_start,exon_i->GetTo()-next_seg_start+1, true));
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

void RestoreModelMethod(const CSeq_feat_Handle& feat, CAlignModel& model)
{
    const CUser_object& user = *feat.GetSeq_feat()->GetExts().front();
    _ASSERT( user.GetType().GetStr() == "Model Internal Attributes" || user.GetType().GetStr() == "ModelEvidence" );
    if (user.HasField("Method")) {
        string method = user.GetField("Method").GetData().GetStr();
        if (method == "Gnomon") model.SetType(CGeneModel::eGnomon);
        else if (method == "Chainer") model.SetType(CGeneModel::eChain);
    }
}

void RestoreModelAttributes(const CSeq_feat_Handle& feat_handle, CAlignModel& model)
{
    TAttributes attributes;

    _ASSERT(feat_handle);
    
    const CUser_object& user = *feat_handle.GetOriginalSeq_feat()->GetExts().front();
    _ASSERT( user.GetClass() == "Gnomon" );
    _ASSERT( user.GetType().GetStr() == "Model Internal Attributes" );

    
    model.SetType(0);
    RestoreModelMethod(feat_handle, model);
    
    ITERATE(CUser_object::TData, f, user.GetData()) {
        const CUser_field& fld = **f;
        if (fld.GetData().IsStr()) 
            attributes[fld.GetLabel().GetStr()] = fld.GetData().GetStr();
        else if (fld.GetLabel().GetStr() == "cds_score") 
            attributes["cds_score"] = NStr::DoubleToString(fld.GetData().GetReal());
    }

    ParseAttributes(attributes, model);

    if (attributes.find("cds_score") != attributes.end()) {
        double score = NStr::StringToDouble(attributes["cds_score"]);
        CCDSInfo cds_info = model.GetCdsInfo();
        cds_info.SetScore(score, cds_info.OpenCds());
        model.SetCdsInfo(cds_info);
    }
}

void RestoreModelReadingFrame(const CSeq_feat_Handle& feat, CAlignModel& model)
{
    if (feat && feat.GetFeatType() == CSeqFeatData::e_Cdregion) {
        TSeqRange cds_range = feat.GetLocation().GetTotalRange();
        TSignedSeqRange rf = TSignedSeqRange(cds_range.GetFrom(), cds_range.GetTo());
        if (feat.GetLocation().GetId() != CIdHandler::GnomonMRNA(model.ID())) {
            rf =  model.GetAlignMap().MapRangeOrigToEdited(rf, false);
        }

        if (feat.GetData().GetCdregion().CanGetFrame()) {

            CCdregion::EFrame ncbi_frame = feat.GetData().GetCdregion().GetFrame();
            int phase = 0;
            switch (ncbi_frame) {
            case CCdregion::eFrame_not_set:
            case CCdregion::eFrame_one:
                phase = 0;
                break;
            case CCdregion::eFrame_two:
                phase = 1;
                break;
            case CCdregion::eFrame_three:
                phase = 2;
                break;
            default:
                _ASSERT( false);
            }

            bool notreversed = (model.Status()&CGeneModel::eReversed) == 0;
            if(notreversed) {
                rf.SetFrom(rf.GetFrom()+phase);
                rf.SetTo(rf.GetTo()-rf.GetLength()%3);
            } else {
                rf.SetTo(rf.GetTo()-phase);
                rf.SetFrom(rf.GetFrom()+rf.GetLength()%3);
            }
        }

        TSignedSeqRange reading_frame =  model.GetAlignMap().MapRangeEditedToOrig(rf,true);

        CCDSInfo cds_info;
        cds_info.SetReadingFrame(reading_frame, false);
        model.SetCdsInfo(cds_info);
    }
}

CAlignModel* RestoreModel(const CSeq_feat_Handle& internal_feat, const CSeq_feat_Handle& cds_feat, const CSeq_align& align)
{
    CAlignModel* model = new CAlignModel(align);

    RestoreModelReadingFrame(cds_feat, *model);

    RestoreModelAttributes(internal_feat, *model);

    return model;
}

CAlignModel* RestoreModelFromPublicMrnaFeature(const CSeq_feat_Handle& feat)
{
    CScope& scope = feat.GetScope();
    CBioseq_Handle mrna_handle = scope.GetBioseqHandle(feat.GetProduct());
    CConstRef<CBioseq> mrna = mrna_handle.GetCompleteBioseq();
    _ASSERT(mrna->IsNa());

    const CSeq_align& align = *mrna->GetInst().GetHist().GetAssembly().front();

    int id = GetModelId(align);

    CFeat_CI cds_feat(mrna_handle);
    while (cds_feat && !cds_feat->GetOriginalFeature().GetData().IsCdregion())
        ++cds_feat;

    const CTSE_Handle& tse_handle = feat.GetAnnot().GetTSE_Handle();
    CSeq_feat_Handle feat_handle = tse_handle.GetFeatureWithId(CSeqFeatData::e_Rna, id);
    if (!feat_handle)
        feat_handle = tse_handle.GetFeatureWithId(CSeqFeatData::e_Cdregion, id);

    return RestoreModel(feat_handle, *cds_feat, align);
}

CAlignModel* RestoreModelFromInternalGnomonFeature(const CSeq_feat_Handle& feat)
{
    int id = feat.GetOriginalSeq_feat()->GetIds().front()->GetLocal().GetId();

    CScope& scope = feat.GetScope();
    CConstRef<CSeq_id> mrna_seq_id = CIdHandler::GnomonMRNA(id);
    CBioseq_Handle mrna_handle = scope.GetBioseqHandle(*mrna_seq_id);
    if (!mrna_handle)
        return NULL;
    CConstRef<CBioseq> mrna = mrna_handle.GetCompleteBioseq();
    _ASSERT(mrna->IsNa());

    const CSeq_align& align = *mrna->GetInst().GetHist().GetAssembly().front();

    return RestoreModel(feat, feat, align);
}

bool IsGnomonConstructed(const CSeq_align& seq_align)
{
    if (seq_align.CanGetExt()) {
        CSeq_align::TExt ext = seq_align.GetExt();
        ITERATE(CSeq_align::TExt, u, ext) {
            if ((*u)->CanGetClass() && (*u)->GetClass() == "Gnomon" &&
                (*u)->HasField(kGnomonConstructed) &&
                (*u)->GetField(kGnomonConstructed).GetData().GetBool())
                return true;
        }
    }
    return false;
}

void ExtractSupportModels(int model_id,
                          TAlignModelList& evidence_models, list<CRef<CSeq_align> >& evidence_alignments,
                          CSeq_entry_Handle seq_entry_handle, map<string, CRef<CSeq_annot> >& seq_annot_map,
                          set<int>& processed_ids)
{
    map<string, CRef<CSeq_annot> >::iterator annot = seq_annot_map.find("Evidence for "+CIdHandler::ToString(*CIdHandler::GnomonMRNA(model_id)));
    if (annot == seq_annot_map.end())
        return;

    CSeq_annot::TData::TAlign aligns = annot->second->SetData().SetAlign();

    NON_CONST_ITERATE (CSeq_annot::TData::TAlign, align_ci, aligns) {
        CSeq_align& seq_align = **align_ci;
        int id = seq_align.GetId().back()->GetId();

        if (!processed_ids.insert(id).second) // already there
            continue;

        const CTSE_Handle& tse_handle = seq_entry_handle.GetTSE_Handle();
        CSeq_feat_Handle feat_handle = tse_handle.GetFeatureWithId(CSeqFeatData::e_Rna, id);
        if (!feat_handle)
            feat_handle = tse_handle.GetFeatureWithId(CSeqFeatData::e_Cdregion, id);

        auto_ptr<CAlignModel> model( RestoreModel(feat_handle, feat_handle, seq_align) );
        evidence_models.push_back(*model);

        ExtractSupportModels(id, evidence_models, evidence_alignments, seq_entry_handle, seq_annot_map, processed_ids);

        if (IsGnomonConstructed(seq_align))
            continue;

        CRef<CSeq_align> align_ref(&seq_align);
        evidence_alignments.push_back(align_ref);
    }
}

string CAnnotationASN1::ExtractModels(objects::CSeq_entry& seq_entry,
                                      TAlignModelList& model_list,
                                      TAlignModelList& evidence_models,
                                      list<CRef<CSeq_align> >& evidence_alignments)
{
    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seq_entry_handle = scope.AddTopLevelSeqEntry(seq_entry);

    map<string, CRef<CSeq_annot> > seq_annot_map;

    string contig;

    ITERATE(CBioseq_set::TAnnot, annot, seq_entry.SetSet().SetAnnot()) {
        CAnnot_descr::Tdata::const_iterator iter = (*annot)->GetDesc().Get().begin();
        string name;
        string region;
        for ( ;  iter != (*annot)->GetDesc().Get().end();  ++iter) {
            if ((*iter)->IsName() ) {
                name = (*iter)->GetName();
            }
            if ((*iter)->IsRegion() ) {
                region = CIdHandler::ToString(*(*iter)->GetRegion().GetId());
            }
        }
        if (!name.empty()) {
            seq_annot_map[name] = *annot;
            if (name=="Gnomon models") {
                contig = region;
            }
        }
    }

    CRef<CSeq_annot> feature_table = seq_annot_map["Gnomon models"];
    _ASSERT( feature_table.NotEmpty() );
    _ASSERT( feature_table->IsFtable() );

    CRef<CSeq_annot> internal_feature_table = seq_annot_map["Gnomon internal attributes"];
    _ASSERT( internal_feature_table.NotEmpty() );
    _ASSERT( internal_feature_table->IsFtable() );

    SAnnotSelector sel;
    sel.SetFeatType(CSeqFeatData::e_Rna);
    CFeat_CI feat_ci(scope.GetSeq_annotHandle(*feature_table), sel);

    if (contig.empty() && feat_ci) {
        contig = CIdHandler::ToString(*feat_ci->GetLocation().GetId());
    }

    set<int> processed_ids;
    for (; feat_ci; ++feat_ci) {
        auto_ptr<CAlignModel> model( RestoreModelFromPublicMrnaFeature(feat_ci->GetSeq_feat_Handle()) );
        model_list.push_back(*model);
        processed_ids.insert(model->ID());
        ExtractSupportModels(model->ID(), evidence_models, evidence_alignments, seq_entry_handle, seq_annot_map, processed_ids);
    }

    CFeat_CI internal_feat_ci(scope.GetSeq_annotHandle(*internal_feature_table));
    for (; internal_feat_ci; ++internal_feat_ci) {
        int id = internal_feat_ci->GetOriginalFeature().GetIds().front()->GetLocal().GetId();
        if (processed_ids.find(id) != processed_ids.end()) // already there
            continue;
        auto_ptr<CAlignModel> model( RestoreModelFromInternalGnomonFeature(internal_feat_ci->GetSeq_feat_Handle()) );
        if (model.get() == NULL)
            continue;
        evidence_models.push_back(*model);
        processed_ids.insert(id);
        ExtractSupportModels(model->ID(), evidence_models, evidence_alignments, seq_entry_handle, seq_annot_map, processed_ids);
    }

    
    return contig;
}

END_SCOPE(gnomon)
END_NCBI_SCOPE
