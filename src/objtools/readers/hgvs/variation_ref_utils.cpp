#include <ncbi_pch.hpp>

#include <objects/seq/Seq_literal.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/readers/hgvs/variation_ref_utils.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(NHgvsTestUtils)

CRef<CVariation_ref> g_CreateSNV(const CSeq_data& nucleotide,
                                 const CRef<CDelta_item> offset)
{
    auto var_ref = Ref(new CVariation_ref());
    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_snv);
    inst.SetDelta().clear();

    if (offset.NotNull()) {
        inst.SetDelta().push_back(offset);
    }

    auto delta_item = Ref(new CDelta_item());
    auto& seq_literal = delta_item->SetSeq().SetLiteral();
    seq_literal.SetSeq_data().Assign(nucleotide);
    seq_literal.SetLength(1);
    inst.SetDelta().push_back(delta_item);

    return var_ref;
}


CRef<CVariation_ref> g_CreateMNP(const CSeq_data& nucleotide,
                                 const TSeqPos length,
                                 const CRef<CDelta_item> offset)
{
    auto var_ref = Ref(new CVariation_ref());
    
    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_mnp);
    inst.SetDelta().clear();

    if (offset.NotNull()) {
        inst.SetDelta().push_back(offset);
    }
    
    auto delta_item = Ref(new CDelta_item());
    auto& seq_literal = delta_item->SetSeq().SetLiteral();
    seq_literal.SetSeq_data().Assign(nucleotide);
    seq_literal.SetLength(length);
    inst.SetDelta().push_back(delta_item);

    return var_ref;
}


CRef<CVariation_ref> g_CreateMissense(const CSeq_data& amino_acid)
{
    auto var_ref = Ref(new CVariation_ref());

    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_prot_missense);
    inst.SetDelta().clear();
    auto delta_item = Ref(new CDelta_item());
    delta_item->SetSeq().SetLiteral().SetSeq_data().Assign(amino_acid);
    delta_item->SetSeq().SetLiteral().SetLength() = 1;
    inst.SetDelta().push_back(delta_item);

    return var_ref;
}


CRef<CVariation_ref> g_CreateDeletion(CRef<CDelta_item> start_offset,
                                      CRef<CDelta_item> stop_offset)
{
    auto var_ref = Ref(new CVariation_ref());

    CVariation_inst& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_del);
    inst.SetDelta().clear();
    
    if (start_offset.NotNull()) {
        inst.SetDelta().push_back(start_offset);
    }
    
    auto delta_item = Ref(new CDelta_item());
    delta_item->SetSeq().SetThis();
    delta_item->SetAction(CDelta_item::eAction_del_at);

    inst.SetDelta().push_back(delta_item);

    if (stop_offset.NotNull()) {
        inst.SetDelta().push_back(stop_offset);
    }

    return var_ref;
}


CRef<CVariation_ref> g_CreateDuplication(CRef<CDelta_item> start_offset,
                                         CRef<CDelta_item> stop_offset)
{
    auto var_ref = Ref(new CVariation_ref());

    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_ins);
    inst.SetDelta().clear();
    if (start_offset.NotNull()) {
        inst.SetDelta().push_back(start_offset);
    }
    
    auto delta_item = Ref(new CDelta_item());
    delta_item->SetSeq().SetThis();
    delta_item->SetMultiplier() = 2;
    inst.SetDelta().push_back(delta_item);
    if (stop_offset.NotNull()) {
        inst.SetDelta().push_back(stop_offset);
    }

    return var_ref;
}


CRef<CVariation_ref> g_CreateIdentity(CRef<CSeq_literal> seq_literal,
                                      const CRef<CDelta_item> start_offset,
                                      const CRef<CDelta_item> stop_offset,
                                      const bool enforce_assert)
{
    auto var_ref = Ref(new CVariation_ref());

    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_identity);
    if (seq_literal->IsSetSeq_data() || 
        enforce_assert) {
        inst.SetObservation(CVariation_inst::eObservation_asserted);
    }
    inst.SetDelta().clear();

    if (start_offset.NotNull()) {
        inst.SetDelta().push_back(start_offset);
    }

    auto delta_item = Ref(new CDelta_item());
    delta_item->SetSeq().SetLiteral(*seq_literal);
    inst.SetDelta().push_back(delta_item);

    if (stop_offset.NotNull()) {
        inst.SetDelta().push_back(stop_offset);
    }

    return var_ref;
}


CRef<CVariation_ref> g_CreateDelins(CSeq_literal& insertion,
                                    const CRef<CDelta_item> start_offset,
                                    const CRef<CDelta_item> stop_offset)
{
    auto var_ref = Ref(new CVariation_ref());

    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_delins);
    inst.SetDelta().clear();
    
    if (start_offset.NotNull()) {
        inst.SetDelta().push_back(start_offset);
    }
    
    auto delta_item = Ref(new CDelta_item());
    delta_item->SetSeq().SetLiteral(insertion);
    inst.SetDelta().push_back(delta_item);
    
    if (stop_offset.NotNull()) {
        inst.SetDelta().push_back(stop_offset);
    } 

    return var_ref;
}


CRef<CVariation_ref> g_CreateInsertion(CSeq_literal& insertion,
                                       const CRef<CDelta_item> start_offset,
                                       const CRef<CDelta_item> stop_offset)
{
    auto var_ref = Ref(new CVariation_ref());

    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_ins);
    inst.SetDelta().clear();

    if (start_offset.NotNull()) {
        inst.SetDelta().push_back(start_offset);
    }
    
    auto delta_item = Ref(new CDelta_item());
    delta_item->SetAction(CDelta_item::eAction_ins_before);
    delta_item->SetSeq().SetLiteral(insertion);
    inst.SetDelta().push_back(delta_item);
    
    if (stop_offset.NotNull()) {
        inst.SetDelta().push_back(stop_offset);
    }

    return var_ref;
}


CRef<CVariation_ref> g_CreateInversion(const CRef<CDelta_item> start_offset,
                                       const CRef<CDelta_item> stop_offset)
{
    auto var_ref = Ref(new CVariation_ref());

    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_inv);
    inst.SetDelta().clear();
    if (start_offset.NotNull()) {
        inst.SetDelta().push_back(start_offset);
    }
    
    if (stop_offset.NotNull()) {
        inst.SetDelta().push_back(stop_offset);
    }

    return var_ref;
}


CRef<CVariation_ref> g_CreateMicrosatellite(const CRef<CDelta_item> repeat_info,
                                            const CRef<CDelta_item> start_offset,
                                            const CRef<CDelta_item> stop_offset)
{
    auto var_ref = Ref(new CVariation_ref());

    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_microsatellite);
    inst.SetDelta().clear();

    if (start_offset.NotNull()) {
        inst.SetDelta().push_back(start_offset);
    }
    
    inst.SetDelta().push_back(repeat_info);
    if (stop_offset.NotNull()) {
        inst.SetDelta().push_back(stop_offset);
    }

    return var_ref;
}


CRef<CVariation_ref> g_CreateConversion(const CSeq_loc& interval,
                                        const CRef<CDelta_item> start_offset,
                                        const CRef<CDelta_item> stop_offset)
{
    auto var_ref = Ref(new CVariation_ref());

    auto& inst = var_ref->SetData().SetInstance();
    inst.SetType(CVariation_inst::eType_transposon);
    inst.SetDelta().clear();
    if (start_offset.NotNull()) {
        inst.SetDelta().push_back(start_offset);
    }
   
    auto delta_item = Ref(new CDelta_item());
    delta_item->SetSeq().SetLoc().Assign(interval);
    inst.SetDelta().push_back(delta_item);
    if (stop_offset.NotNull()) {
        inst.SetDelta().push_back(stop_offset);
    }

    return var_ref;
}


CRef<CVariation_ref> g_CreateFrameshift(void)
{
    auto var_ref = Ref(new CVariation_ref());
    
    auto delta_item = Ref(new CDelta_item());
    delta_item->SetSeq().SetLiteral().SetSeq_data().SetNcbieaa().Set(CNCBIeaa("X"));
    delta_item->SetSeq().SetLiteral().SetLength(1);
    var_ref->SetData().SetInstance().SetDelta().push_back(delta_item);
    var_ref->SetData().SetInstance().SetType(CVariation_inst::eType_prot_other);
    auto cons = Ref(new CVariation_ref::TConsequence::value_type::TObjectType());
    cons->SetFrameshift();
    var_ref->SetConsequence().push_back(cons);

    return var_ref;
}



END_SCOPE(NHgvsTestUtils)
END_NCBI_SCOPE
