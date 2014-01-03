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
#include <algo/sequence/gene_model.hpp>
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


/// Uncomment this to provide extended evidence output
//#define _EVIDENCE_WANTED

// defined in gnomon_model.cpp
typedef map<string,string> TAttributes;
void CollectAttributes(const CAlignModel& a, TAttributes& attributes);
void ParseAttributes(TAttributes& attributes, CAlignModel& a);

// defined in gnomon_objmgr.cpp
Int8 GetModelId(const CSeq_align& seq_align);


const string kGnomonConstructed = "Is [re]constructed alignment";


struct SModelData {
    SModelData(const CAlignModel& model, const CEResidueVec& contig_seq);

    CAlignModel model;
    CEResidueVec mrna_seq;
    CRef<CSeq_id> mrna_sid;
    CRef<CSeq_id> prot_sid;

    bool is_ncrna;
};

SModelData::SModelData(const CAlignModel& m, const CEResidueVec& contig_seq) : model(m)
{
    CAlignMap amap = model.GetAlignMap();
    CCDSInfo cds = model.GetCdsInfo();
    if(cds.IsMappedToGenome()) {
        cds = cds.MapFromOrigToEdited(amap);  // all coords on mRNA
        model.SetCdsInfo(cds);
    }

    amap.EditedSequence(contig_seq, mrna_seq, true);

    prot_sid.Reset(new CSeq_id);
    prot_sid->Assign(*CIdHandler::GnomonProtein(model.ID())); 
    mrna_sid.Reset(new CSeq_id);
    mrna_sid->Assign(*CIdHandler::GnomonMRNA(model.ID()));

    is_ncrna = m.ReadingFrame().Empty();
}


class CAnnotationASN1::CImplementationData {
public:
    CImplementationData(const string& contig_name, const CResidueVec& seq, IEvidence& evdnc, int genetic_code);

    void AddModel(const CAlignModel& model);
    CRef<CSeq_entry> main_seq_entry;

private:
    void CreateModelProducts(SModelData& model);
    CRef<CSeq_feat> create_internal_feature(const SModelData& md);
    CRef<CSeq_feat> create_cdregion_feature(SModelData& md);
    CRef<CSeq_align> model2spliced_seq_align(SModelData& md);
    //    CRef<CSpliced_exon> spliced_exon (const CModelExon& e, EStrand strand) const;
    CRef<CSeq_loc> create_packed_int_seqloc(const CGeneModel& model, TSignedSeqRange limits_on_mrna = TSignedSeqRange::GetWhole());

    CRef< CUser_object > create_ModelEvidence_user_object(const CGeneModel& model);
    void AddInternalFeature(const SModelData& md);
    void DumpEvidence(const SModelData& md);
    void DumpUnusedChains();

    string contig_name;
    CRef<CSeq_id> contig_sid;
    const CResidueVec& contig_seq;
    CDoubleStrandSeq  contig_ds_seq;

    int gencode;

    CBioseq_set::TSeq_set* nucprots;
    CSeq_annot* gnomon_models_annot;
    CSeq_annot::C_Data::TFtable* feature_table;
    CSeq_annot::C_Data::TAlign*  model_alignments;
    CSeq_annot::C_Data::TFtable* internal_feature_table;
    set<int> models_in_internal_feature_table;

    typedef map<int,CRef<CSeq_feat> > TGeneMap;
    TGeneMap genes;
    IEvidence& evidence;

    auto_ptr<CFeatureGenerator> feature_generator;
    CRef<CScope> scope;

    friend class CAnnotationASN1;
};

void NameAnnot(CSeq_annot& annot, const string& name)
{
    annot.SetNameDesc(name);
    annot.SetTitleDesc(name);
}

CAnnotationASN1::CImplementationData::CImplementationData(const string& a_contig_name, const CResidueVec& seq, IEvidence& evdnc, int genetic_code) :
    main_seq_entry(new CSeq_entry),
    contig_name(a_contig_name),
    contig_sid(CIdHandler::ToSeq_id(a_contig_name)), contig_seq(seq),
    gencode(genetic_code),
    evidence(evdnc)
{
    Convert(contig_seq, contig_ds_seq);

    CBioseq_set& bioseq_set = main_seq_entry->SetSet();

    nucprots = &bioseq_set.SetSeq_set();

    gnomon_models_annot = new CSeq_annot;
    NameAnnot(*gnomon_models_annot, "Gnomon models");
    CRef<CAnnotdesc> desc(new CAnnotdesc());
    CRef<CSeq_loc> genomic_seqloc(new CSeq_loc());
    genomic_seqloc->SetWhole(*contig_sid);
    desc->SetRegion(*genomic_seqloc);
    gnomon_models_annot->SetDesc().Set().push_back(desc);

    CRef<CSeq_annot> gnomon_models_annot_ref(gnomon_models_annot);
    bioseq_set.SetAnnot().push_back(gnomon_models_annot_ref);
    feature_table = &gnomon_models_annot->SetData().SetFtable();

    CRef<CSeq_annot> seq_annot(new CSeq_annot);
    NameAnnot(*seq_annot, "Gnomon internal attributes");
    bioseq_set.SetAnnot().push_back(seq_annot);
    internal_feature_table = &seq_annot->SetData().SetFtable();

    model_alignments = NULL;
#ifdef _EVIDENCE_WANTED
    seq_annot.Reset(new CSeq_annot);
    bioseq_set.SetAnnot().push_back(seq_annot);
    model_alignments = &seq_annot->SetData().SetAlign();
    NameAnnot(*seq_annot, "Model Alignments");
#endif

    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    scope.Reset(new CScope(*obj_mgr));
    scope->AddDefaults();
    feature_generator.reset(new CFeatureGenerator(*scope));
    feature_generator->SetFlags(CFeatureGenerator::fCreateGene | CFeatureGenerator::fCreateMrna | CFeatureGenerator::fCreateCdregion | CFeatureGenerator::fForceTranslateCds | CFeatureGenerator::fForceTranscribeMrna | CFeatureGenerator::fDeNovoProducts);
    feature_generator->SetMinIntron(numeric_limits<TSeqPos>::max());
    feature_generator->SetAllowedUnaligned(0);
}

CAnnotationASN1::CAnnotationASN1(const string& contig_name, const CResidueVec& seq, IEvidence& evdnc,
                    int genetic_code) :
    m_data(new CImplementationData(contig_name, seq, evdnc, genetic_code))
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

void CAnnotationASN1::CImplementationData::CreateModelProducts(SModelData& md)
{
    CRef< CSeq_align > align = model2spliced_seq_align(md);
    CRef<CSeq_feat> cds_feat;

    CSeq_feat* cds_feat_ptr = NULL;
    if (!md.is_ncrna) {
         cds_feat = create_cdregion_feature(md);
         cds_feat_ptr = cds_feat.GetPointer();
    }

    CRef<CSeq_entry> model_products(new CSeq_entry);
    nucprots->push_back(model_products);
    CRef<CSeq_annot> annot(new CSeq_annot);
    feature_generator->ConvertAlignToAnnot(*align, *annot, model_products->SetSet(), 0, cds_feat_ptr);
}

void CAnnotationASN1::CImplementationData::AddInternalFeature(const SModelData& md)
{
    Int8 id = md.model.ID();
    if (models_in_internal_feature_table.find(id) == models_in_internal_feature_table.end()) {
        CRef<CSeq_feat> internal_feat = create_internal_feature(md);
        internal_feature_table->push_back(internal_feat);
        models_in_internal_feature_table.insert(id);
    }
}

void CAnnotationASN1::CImplementationData::AddModel(const CAlignModel& model)
{
    SModelData md(model, contig_ds_seq[ePlus]);

    CRef< CSeq_align > align = model2spliced_seq_align(md);
    CRef<CSeq_feat> cds_feat;
    CSeq_feat* cds_feat_ptr = NULL;
    if (!md.is_ncrna) {
         cds_feat = create_cdregion_feature(md);
         cds_feat_ptr = cds_feat.GetPointer();
    }

    try {
        CRef<CSeq_entry> model_products(new CSeq_entry);
        nucprots->push_back(model_products);

        CRef<CSeq_feat> mrna_feat = feature_generator->ConvertAlignToAnnot(*align, *gnomon_models_annot, model_products->SetSet(), model.GeneID(), cds_feat_ptr);

        DumpEvidence(md);

        CRef< CUser_object > user_obj = create_ModelEvidence_user_object(model);
        mrna_feat->SetExts().push_back(user_obj);

        if (model_alignments != NULL) {
            model_alignments->push_back(align);
        }

        AddInternalFeature(md);
    }
    catch (CException& e) {
        cerr << MSerial_AsnText << *align;
        throw;
    }
}

CRef<CSeq_feat> CAnnotationASN1::CImplementationData::create_cdregion_feature(SModelData& md)
{
    const CGeneModel& model = md.model;
    const CCDSInfo& cds = model.GetCdsInfo();

    CRef<CSeq_feat> cdregion_feature(new CSeq_feat);  

    CRef<CGenetic_code::C_E> cdrcode(new CGenetic_code::C_E);
    cdrcode->SetId(gencode);
    cdregion_feature->SetData().SetCdregion().SetCode().Set().push_back(cdrcode);

   if(model.PStop()) {
        CCdregion::TCode_break& code_breaks = cdregion_feature->SetData().SetCdregion().SetCode_break();
        ITERATE(CCDSInfo::TPStops,s,cds.PStops()) {
            CRef< CCode_break > code_break(new CCode_break);
            CRef<CSeq_loc> pstop;

            TSeqPos from = s->GetFrom();
            TSeqPos to = s->GetTo();
            _ASSERT(from+2==to);
            
            pstop.Reset(new CSeq_loc(*md.mrna_sid, from, to, eNa_strand_plus));

            code_break->SetLoc(*pstop);
            CRef<CCode_break::C_Aa> aa(new CCode_break::C_Aa);
            aa->SetNcbieaa('X');
            code_break->SetAa(*aa);
            code_breaks.push_back(code_break);
        }
    }

    cdregion_feature->SetProduct().SetWhole(*md.prot_sid);

    TSignedSeqRange tlim = model.TranscriptLimits();
    int altstart = (cds.MaxCdsLimits()&tlim).GetFrom();
    int start = cds.Cds().GetFrom();
    int stop = (cds.MaxCdsLimits()&tlim).GetTo();

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

    cdregion_location.Reset(new CSeq_loc(*md.mrna_sid, start, stop, eNa_strand_plus));

    if (0 < altstart && altstart != start)
        cdregion_location->SetInt().SetFuzz_from().SetAlt().push_back(altstart);

    if (!model.HasStart())
        cdregion_location->SetPartialStart(true,eExtreme_Biological);
    if (!model.HasStop())
        cdregion_location->SetPartialStop(true,eExtreme_Biological);
    cdregion_feature->SetLocation(*cdregion_location);

    if (!model.HasStart() || !model.HasStop() || !model.Continuous())
        cdregion_feature->SetPartial(true);

    return cdregion_feature;
}

CRef<CSeq_loc> CAnnotationASN1::CImplementationData::create_packed_int_seqloc(const CGeneModel& model, TSignedSeqRange limits_on_mrna)
{
    CRef<CSeq_loc> seq_loc(new CSeq_loc);
    CPacked_seqint& packed_int = seq_loc->SetPacked_int();
    ENa_strand strand = model.Strand()==ePlus?eNa_strand_plus:eNa_strand_minus; 

    CAlignMap amap = model.GetAlignMap();

    for (size_t i=0; i < model.Exons().size(); ++i) {
        const CModelExon* e = &model.Exons()[i];

        if(e->Limits().NotEmpty()) {  // not a ggap
            TSignedSeqRange interval_range_on_mrna = amap.MapRangeOrigToEdited(e->Limits()) & limits_on_mrna;
            if (interval_range_on_mrna.Empty())
                continue;

            bool extends_to_left = interval_range_on_mrna.GetFrom() > limits_on_mrna.GetFrom();
            bool extends_to_right = interval_range_on_mrna.GetTo() < limits_on_mrna.GetTo();
            if(model.Strand() == eMinus)
                swap(extends_to_left,extends_to_right);

            TSignedSeqRange interval_range = amap.MapRangeEditedToOrig(interval_range_on_mrna);
            CRef<CSeq_interval> interval(new CSeq_interval(*contig_sid, interval_range.GetFrom(),interval_range.GetTo(), strand));
        
            if (i > 0 && (!e->m_fsplice || (model.Exons()[i-1].Limits().Empty() && extends_to_left))) {
                _ASSERT(interval_range.GetFrom() == e->Limits().GetFrom());
                interval->SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
            }
            if (i < model.Exons().size()-1 && (!e->m_ssplice || (model.Exons()[i+1].Limits().Empty() && extends_to_right))) {
                _ASSERT(interval_range.GetTo() == e->Limits().GetTo());
                interval->SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
            }
        
            packed_int.AddInterval(*interval);
        }
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

    {{
         string id_str = CIdHandler::ToString(*md.mrna_sid);

         NameAnnot(*seq_annot, "Evidence for " + id_str);

         CRef<CAnnot_id> annot_id(new CAnnot_id);
         annot_id->SetGeneral().SetDb("GNOMON");
         annot_id->SetGeneral().SetTag().SetId(md.model.ID());
         seq_annot->SetId().push_back(annot_id);
     }}
    
    
    ITERATE(CSupportInfoSet, s, model.Support()) {
        Int8 id = s->GetId();

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
    NameAnnot(*seq_annot, "Unused chains");

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

int CollectUserField(const CUser_field& field, const string& name, vector<string>& values)
{
    int count = 0;
    if (field.HasField(name)) {
        const CUser_field& fn =  field.GetField(name);
        const CUser_field::C_Data::TStrs& strs = fn.GetData().GetStrs();
        copy(strs.begin(), strs.end(), back_inserter(values));
        if(fn.CanGetNum())
            count = fn.GetNum();
        else
            count = strs.size();
    }

    return count;
}


string ModelMethod(const CGeneModel& model) {
    string method;
    bool ggap = false;
    for(int i = 1; i < (int)model.Exons().size() && !ggap; ++i) {
        ggap = model.Exons()[i-1].m_ssplice_sig == "XX" || model.Exons()[i].m_fsplice_sig == "XX";
    }
    if(model.Type()&CGeneModel::eChain) {
        method = ggap ? "Chainer_GapFilled" : "Chainer";
    } else if(model.Type()&CGeneModel::eGnomon) {
        if(model.Support().empty())
            method = "FullAbInitio";
        else
            method = ggap ? "PartAbInitio_GapFilled" : "PartAbInitio";
    } else {
        method = CGeneModel::TypeToString(model.Type());
    }

    return method;
}


CRef< CUser_object > CAnnotationASN1::CImplementationData::create_ModelEvidence_user_object(const CGeneModel& model)
{
    CRef< CUser_object > user_obj(new CUser_object);
    CRef< CObject_id > type(new CObject_id);
    type->SetStr("ModelEvidence");
    user_obj->SetType(*type);

    user_obj->AddField("Method",ModelMethod(model));

    if (!model.Support().empty()) {
        CRef<CUser_field> support_field(new CUser_field());
        user_obj->SetData().push_back(support_field);
        support_field->SetLabel().SetStr("Support");
        vector<string> chains;
        vector<string> cores;
        vector<string> proteins;
        vector<string> mrnas;
        vector<string> ests;
        vector<string> short_reads;
        vector<string> long_sras;
        vector<string> others;
        vector<string> unknown;

        CSupportInfoSet support;

        ExpandSupport(model.Support(), support, evidence);

        int ests_count = 0;
        int long_sras_count = 0;
        int others_count = 0;

        ITERATE(CSupportInfoSet, s, support) {
            
            Int8 id = s->GetId();
            
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
            else if (type&CGeneModel::eEST) {
                if(NStr::StartsWith(accession, "gi|")) {
                    ests.push_back(accession);
                    ests_count += m->Weight();
                } else if(NStr::StartsWith(accession, "gnl|SRA")) {
                    long_sras.push_back(accession);
                    long_sras_count += m->Weight();
                } else {
                    others.push_back(accession);
                    others_count += m->Weight();
                }
            } else if (type&CGeneModel::eSR)
                short_reads.push_back(accession);
            else
                unknown.push_back(accession);
        }

        if (proteins.empty() && mrnas.empty() && ests.empty() && short_reads.empty() && long_sras.empty() && others.empty()) {
            if ((model.Type()&CGeneModel::eChain)) {
                support.clear();
                support.insert(CSupportInfo(model.ID()));
            } else {
                support = model.Support();
            }
            vector<string> collected_cores;
            ITERATE(CSupportInfoSet, s, support) {
                Int8 id = s->GetId();
                const CGeneModel* m = (id == model.ID()) ? &model : evidence.GetModel(id);
                if (m != NULL && (m->Type()&CGeneModel::eChain)) {
                    CRef<CUser_object> uo = evidence.GetModelEvidenceUserObject(id);
                    if (uo.NotNull() && uo->HasField("Support")) {
                        const CUser_field& support_field = uo->GetField("Support");
                        CollectUserField(support_field, "Core", collected_cores);
                        CollectUserField(support_field, "Proteins", proteins);
                        CollectUserField(support_field, "mRNAs", mrnas);
                        ests_count += CollectUserField(support_field, "ESTs", ests);
                        CollectUserField(support_field, "RNASeq", short_reads);
                        long_sras_count += CollectUserField(support_field, "longSRA", long_sras);
                        others_count += CollectUserField(support_field, "other", others);
                    }
                }
            }
            if (!(proteins.empty() && mrnas.empty() && ests.empty() && short_reads.empty() && long_sras.empty() && others.empty())) {
                cores = collected_cores;
                unknown.clear();
            }
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
            support_field->SetData().SetFields().back()->SetNum(ests_count);
        }
        if (!short_reads.empty()) {
            sort(short_reads.begin(),short_reads.end());
            support_field->AddField("RNASeq",short_reads);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(short_reads.size());
        }
        if (!long_sras.empty()) {
            sort(long_sras.begin(),long_sras.end());
            support_field->AddField("longSRA",long_sras);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(long_sras_count);
        }
        if (!others.empty()) {
            sort(others.begin(),others.end());
            support_field->AddField("other",others);
            // SetNum should be done in AddField actually. Won't be needed when AddField fixed in the toolkit.
            support_field->SetData().SetFields().back()->SetNum(others_count);
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

    return user_obj;
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
        user->AddField("Method", ModelMethod(model));

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

    const CCDSInfo& cds_info = model.GetCdsInfo();
    TSignedSeqRange rcds_on_mrna = cds_info.MaxCdsLimits() & model.TranscriptLimits();
    int frame = 0;
    if(cds_info.HasStart())
        rcds_on_mrna.SetFrom(cds_info.Start().GetFrom());
    else
        frame = (cds_info.Cds().GetFrom()-rcds_on_mrna.GetFrom())%3;

    if (cds_info.ReadingFrame().NotEmpty() && model.GetAlignMap().MapRangeOrigToEdited(model.Limits(),false).IntersectingWith(rcds_on_mrna)) {    // at least some CDS could be projected on genome
        // create cdregion feature as there is no other place to show CDS on evidence alignment

        _ASSERT((model.Status()&CGeneModel::eReversed) == 0);  // needs to be changed if not true

        feature->SetLocation(*create_packed_int_seqloc(model,rcds_on_mrna));
        CRef<CGenetic_code::C_E> cdrcode(new CGenetic_code::C_E);
        cdrcode->SetId(1);
        feature->SetData().SetCdregion().SetCode().Set().push_back(cdrcode);

        CCdregion::EFrame ncbi_frame = CCdregion::eFrame_one;
        if(frame == 1) ncbi_frame = CCdregion::eFrame_two;
        else if(frame == 2) ncbi_frame = CCdregion::eFrame_three; 
        feature->SetData().SetCdregion().SetFrame(ncbi_frame);        
    } else {
        feature->SetLocation(*create_packed_int_seqloc(model,model.TranscriptLimits()));
        feature->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    }

    return feature;
}

//CRef<CSpliced_exon> CAnnotationASN1::CImplementationData::spliced_exon (const CModelExon& e, EStrand strand) const
CRef<CSpliced_exon> spliced_exon (const CModelExon& e, EStrand strand)
{
    CRef<CSpliced_exon> se(new CSpliced_exon());

    if(e.Limits().NotEmpty()) { // normal exon
        se->SetGenomic_start(e.GetFrom());
        se->SetGenomic_end(e.GetTo());
    } else {    // gap filler
        CRef<CSeq_id> fillerid = CIdHandler::ToSeq_id(e.m_source.m_acc);
        se->SetGenomic_id(*fillerid);
        se->SetGenomic_strand(e.m_source.m_strand==ePlus?eNa_strand_plus:eNa_strand_minus);
        se->SetGenomic_start(e.m_source.m_range.GetFrom());
        se->SetGenomic_end(e.m_source.m_range.GetTo());
    }

    if(e.m_ident > 0) {
        CRef<CScore> scr(new CScore);
        scr->SetValue().SetReal(e.m_ident);
        CRef<CObject_id> id(new CObject_id());
        id->SetStr("idty");
        scr->SetId(*id);
        se->SetScores().Set().push_back(scr);
    }

    if (e.m_fsplice) {
        //        _ASSERT(e.m_fsplice_sig.length() == 2);
        if (strand==ePlus) {
            se->SetAcceptor_before_exon().SetBases(e.m_fsplice_sig);
        } else {
            se->SetDonor_after_exon().SetBases(e.m_fsplice_sig);
        }
    }
    if (e.m_ssplice) {
        //        _ASSERT(e.m_ssplice_sig.length() == 2);
        if (strand==ePlus) {
            se->SetDonor_after_exon().SetBases(e.m_ssplice_sig);
        } else {
            se->SetAcceptor_before_exon().SetBases(e.m_ssplice_sig);
        }
    }
    return se;
}


CRef<CProduct_pos> NucPosToProtPos(int nultripos) {
    CRef<CProduct_pos> pos(new CProduct_pos);
    pos->SetProtpos().SetFrame(nultripos%3 + 1);
    pos->SetProtpos().SetAmin(nultripos/3);
    return pos;
}

CRef<CSeq_align> AlignModelToSeqalign(const CAlignModel& model, CSeq_id& mrnaid, CSeq_id& contigid, bool is_align, bool is_protalign, bool stop_found) {

    CRef< CSeq_align > seq_align( new CSeq_align );

    CRef<CObject_id> id(new CObject_id());
    id->SetId(model.ID());
    seq_align->SetId().push_back(id);

    seq_align->SetType(CSeq_align::eType_partial);
    CSpliced_seg& spliced_seg = seq_align->SetSegs().SetSpliced();

    if(is_protalign) {
        spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_protein);
        int product_length = model.TargetLen()/3;
        if(stop_found)
            --product_length;
        spliced_seg.SetProduct_length(product_length);
    } else {
        spliced_seg.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
        spliced_seg.SetProduct_length(model.TargetLen());
    }
    if (model.Status() & CAlignModel::ePolyA) {
        spliced_seg.SetPoly_a((model.Status() & CAlignModel::eReversed)? model.PolyALen() - 1 : model.TargetLen() - model.PolyALen());
    }

    spliced_seg.SetProduct_id(mrnaid);
    spliced_seg.SetGenomic_id(contigid);
    spliced_seg.SetProduct_strand((model.Status() & CGeneModel::eReversed)==0 ? eNa_strand_plus : eNa_strand_minus);
    spliced_seg.SetGenomic_strand(model.Strand()==ePlus?eNa_strand_plus:eNa_strand_minus);

    CSpliced_seg::TExons& exons = spliced_seg.SetExons();

    TInDels indels = model.GetInDels(false);

    TInDels::const_iterator indel_i = indels.begin();
    for (size_t i=0; i < model.Exons().size(); ++i) {
        const CModelExon *e = &model.Exons()[i]; 

        CRef<CSpliced_exon> se = spliced_exon(*e,model.Strand());

        TSignedSeqRange transcript_exon = model.TranscriptExon(i);
        if(is_protalign) {
            se->SetProduct_start(*NucPosToProtPos(transcript_exon.GetFrom()));
            se->SetProduct_end(*NucPosToProtPos(transcript_exon.GetTo()));
        } else {
            se->SetProduct_start().SetNucpos(transcript_exon.GetFrom());
            se->SetProduct_end().SetNucpos(transcript_exon.GetTo());
        }

        if(e->Limits().NotEmpty()) {    // normal exon
            int last_chunk = e->GetFrom();
            while (indel_i != indels.end() && indel_i->Loc() <= e->GetTo()+1) {
                const CInDelInfo& indel = *indel_i;
                _ASSERT( e->GetFrom() <= indel.Loc() );
            
                if (indel.Loc()-last_chunk > 0) {
                    CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
                    if(is_align)
                        chunk->SetDiag(indel.Loc()-last_chunk);
                    else
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
                if(is_align)
                    chunk->SetDiag(e->GetTo()-last_chunk+1);
                else
                    chunk->SetMatch(e->GetTo()-last_chunk+1);
                se->SetParts().push_back(chunk);
            }
        }  else  {   // gap filler
            CRef< CSpliced_exon_chunk > chunk(new CSpliced_exon_chunk);
            if(is_align)
                chunk->SetDiag(e->m_source.m_range.GetLength());
            else
                chunk->SetMatch(e->m_source.m_range.GetLength());
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

    return seq_align;
}

CRef<objects::CSeq_align> CAlignModel::MakeSeqAlign(const string& contig) const {

    bool is_align = !(Type()&(eChain|eGnomon));

    CAlignModel model = *this;
    if(is_align && (Type()&eProt) && HasStop()) {
        _ASSERT(GetCdsInfo().IsMappedToGenome());
        TSignedSeqRange lim = GetCdsInfo().Start()+GetCdsInfo().ReadingFrame();
        model.Clip(lim,eRemoveExons);   // protein alignments doesn't include stop
    }

    CRef<CSeq_id> contig_sid(CIdHandler::ToSeq_id(contig));
    CRef<CSeq_id> mrna_sid(new CSeq_id);
    mrna_sid->Assign(*GetTargetId());
    CRef<CSeq_align> seq_align = AlignModelToSeqalign(model, *mrna_sid, *contig_sid, is_align, is_align && (Type()&eProt), is_align && (Type()&eProt) && HasStop());

    if(is_align) {
        CSpliced_seg& spliced_seg = seq_align->SetSegs().SetSpliced();

        if(Ident() > 0) {
            int matches = Ident()*seq_align->GetAlignLength()/100.+0.5;
            seq_align->SetNamedScore("matches", matches);
        }

        if(Status()&eBestPlacement) 
            seq_align->SetNamedScore("rank", 1);

        if(Status()&eUnknownOrientation)
            seq_align->SetNamedScore("ambiguous_orientation", 1);

        if(Weight() > 1)
            seq_align->SetNamedScore("count", int(Weight()+0.5));

        if(Type()&eProt) {
            if(HasStart()) {
                CRef<CSpliced_seg_modifier> modi(new CSpliced_seg_modifier);
                modi->SetStart_codon_found(true);
                spliced_seg.SetModifiers().push_back(modi);
            }
            if(HasStop()) {
                CRef<CSpliced_seg_modifier> modi(new CSpliced_seg_modifier);
                modi->SetStop_codon_found(true);
                spliced_seg.SetModifiers().push_back(modi);
            }
        }
    }

    return seq_align;
}

CRef<CSeq_align> CAnnotationASN1::CImplementationData::model2spliced_seq_align(SModelData& md)
{
    const CAlignModel& model = md.model;

    CRef<CSeq_align> seq_align = AlignModelToSeqalign(model, *md.mrna_sid, *contig_sid, false, false, false);

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

CRef<CUser_object> GetModelEvidenceUserObject(const CSeq_feat_Handle& feat)
{
    ITERATE(CSeq_feat::TExts, uo, feat.GetSeq_feat()->GetExts()) {
        if ((*uo)->GetType().GetStr() == "ModelEvidence" ) {
            return *uo;
        }
    }
    return CRef<CUser_object>();
}

void RestoreModelMethod(const CSeq_feat_Handle& feat, CAlignModel& model)
{
    const CUser_object& user = *feat.GetSeq_feat()->GetExts().front();
    _ASSERT( user.GetType().GetStr() == "Model Internal Attributes" || user.GetType().GetStr() == "ModelEvidence" );
    if (user.HasField("Method")) {
        string method = user.GetField("Method").GetData().GetStr();
        if (method.find("AbInitio") != string::npos || method.find("Gnomon") != string::npos) 
            model.SetType(CGeneModel::eGnomon);
        else if (method.find("Chainer") != string::npos) 
            model.SetType(CGeneModel::eChain);
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

    if(model.GetCdsInfo().ReadingFrame().NotEmpty()) {
        CCDSInfo cds_info_t = model.GetCdsInfo();
        CCDSInfo cds_info_g = cds_info_t.MapFromEditedToOrig(model.GetAlignMap());
        if(cds_info_g.ReadingFrame().NotEmpty())   // successful projection
            model.SetCdsInfo(cds_info_g);
    }
}

void RestoreModelReadingFrame(const CSeq_feat_Handle& feat, CAlignModel& model)
{
    if (feat && feat.GetFeatType() == CSeqFeatData::e_Cdregion) {
        TSeqRange cds_range = feat.GetLocation().GetTotalRange();
        TSignedSeqRange rf = TSignedSeqRange(cds_range.GetFrom(), cds_range.GetTo());
        if (!feat.GetLocation().GetId()->Match(*CIdHandler::GnomonMRNA(model.ID()))) {
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

        CCDSInfo cds_info(false);  // mrna coordinates; will be projected to genome at the end if possible
        cds_info.SetReadingFrame(rf, false);
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
    CBioseq_Handle mrna_handle = scope.GetBioseqHandle(*feat.GetProduct().GetId());
    CConstRef<CBioseq> mrna = mrna_handle.GetCompleteBioseq();
    _ASSERT(mrna->IsNa());

    const CSeq_align& align = *mrna->GetInst().GetHist().GetAssembly().front();

    Int8 id = GetModelId(align);

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

        CSeq_feat_Handle cds_handle = tse_handle.GetFeatureWithId(CSeqFeatData::e_Cdregion, "cds."+NStr::IntToString(id));
        if(!cds_handle)
            cds_handle = feat_handle;

        auto_ptr<CAlignModel> model( RestoreModel(feat_handle, cds_handle, seq_align) );
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
                                      list<CRef<CSeq_align> >& evidence_alignments,
                                      map<Int8, CRef<CUser_object> >& model_evidence_uo)
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
        model_evidence_uo[model->ID()] = GetModelEvidenceUserObject(feat_ci->GetSeq_feat_Handle());
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
