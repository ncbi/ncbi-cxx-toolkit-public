#include <ncbi_pch.hpp>
#include <objtools/readers/hgvs/special_irep_to_seqfeat.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CRef<CSeq_loc> s_CreateSeqloc(const CSeq_id& seq_id) 
{
    auto seq_loc = Ref(new CSeq_loc());
    seq_loc->SetWhole().Assign(seq_id);
    return seq_loc;
}


CRef<CSeq_feat> g_CreateSpecialSeqfeat(const ESpecialVariant variant, const CSeq_id& seq_id, const string hgvs_expression)
{
    auto seq_feat = Ref(new CSeq_feat());
    CRef<CVariation_ref> var_ref = Ref(new CVariation_ref());


    switch (variant)  {
        default:
            NCBI_THROW(CVariationIrepException, 
                       eUnknownVariation, 
                       "Unknown variation type");
        case eSpecialVariant_splice_expected:
        case eSpecialVariant_splice_possible:
        {
            string message = "Predicted RNA splice expressions are not currently supported.";
            message += " Please report to variation-services@ncbi.nlm.nih.gov";
            NCBI_THROW(CVariationIrepException, 
                       eUnsupported, 
                       message);
        }
        case eSpecialVariant_unknown:
        {
            var_ref->SetName(hgvs_expression);
            var_ref->SetUnknown();
            seq_feat->SetData().SetVariation(*var_ref);
            break;
        }
        case eSpecialVariant_nochange:
        {
            var_ref->SetName(hgvs_expression);
            auto& var_inst = var_ref->SetData().SetInstance();
            var_inst.SetType(CVariation_inst::eType_identity);
            auto delta_item = Ref(new CDelta_item());
            delta_item->SetSeq().SetThis();
            var_inst.SetDelta().push_back(delta_item);
            break;
        }
        case eSpecialVariant_nochange_expected:
        {
            var_ref->SetMethod().push_back(CVariation_ref::eMethod_E_computational);
            var_ref->SetName(hgvs_expression);
            auto& var_inst = var_ref->SetData().SetInstance();
            var_inst.SetType(CVariation_inst::eType_identity);
            auto delta_item = Ref(new CDelta_item());
            delta_item->SetSeq().SetThis();
            var_inst.SetDelta().push_back(delta_item);
            break;
        }
        case eSpecialVariant_noseq:
        {
            var_ref->SetName(hgvs_expression);
            auto& var_inst = var_ref->SetData().SetInstance();
            var_inst.SetType(CVariation_inst::eType_del);
            auto delta_item = Ref(new CDelta_item());
            delta_item->SetDeletion();
            var_inst.SetDelta().push_back(delta_item);
            break;
        }
        case eSpecialVariant_noseq_expected:
        {
            var_ref->SetMethod().push_back(CVariation_ref::eMethod_E_computational);
            var_ref->SetName(hgvs_expression);
            auto& var_inst = var_ref->SetData().SetInstance();
            var_inst.SetType(CVariation_inst::eType_del);
            auto delta_item = Ref(new CDelta_item());
            delta_item->SetDeletion();
            var_inst.SetDelta().push_back(delta_item);
            break;
        }
    }

    seq_feat->SetData().SetVariation(*var_ref);
    CRef<CSeq_loc> loc = s_CreateSeqloc(seq_id);
    seq_feat->SetLocation(loc.GetNCObject());
    return seq_feat;
}

END_SCOPE(objects)
END_NCBI_SCOPE
