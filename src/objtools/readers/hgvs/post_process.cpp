#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objtools/readers/hgvs/post_process.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CRef<CSeq_literal> CPostProcessUtils::GetLiteralAtLoc(const CSeq_loc& loc, CScope& scope) const
{
    CRef<CSeq_literal> literal = Ref(new CSeq_literal());

    CSeqVector seqvec(loc, scope, CBioseq_Handle::eCoding_Iupac);

    string sequence;
    seqvec.GetSeqData(seqvec.begin(), seqvec.end(), sequence);

    if (sequence.size() > 1) {
        sequence = sequence.front() + ".." + sequence.back();
    }

    if (seqvec.IsProtein()) {
        literal->SetSeq_data().SetNcbieaa().Set(sequence);
    } 
    else {
        literal->SetSeq_data().SetIupacna().Set(sequence);
    }

    literal->SetLength(sequence.size());

    return literal;
}


bool CPostProcessUtils::HasIntronOffset(const CVariation_ref& var_ref) const
{
    return false;
}


bool CPostProcessUtils::HasIntronOffset(const CVariation_inst& var_inst) const
{
    if (!var_inst.IsSetDelta()) {
        return false;
    }

    ITERATE(CVariation_inst::TDelta, it, var_inst.GetDelta()) {
        if ((*it)->IsSetAction() &&
            (*it)->GetAction() == CDelta_item::eAction_offset) {
            return true;
        }
    }
    return false;
}



CRef<CSeq_feat> CNormalizeVariant::GetNormalizedIdentity(const CSeq_feat& identity_feat) const
{
    auto normalized_feat = Ref(new CSeq_feat());
    normalized_feat->Assign(identity_feat);

    if (!normalized_feat->IsSetData() ||
        !normalized_feat->GetData().IsVariation() ||
        !normalized_feat->GetData().GetVariation().IsSetData() ||
        !normalized_feat->GetData().GetVariation().GetData().IsInstance() ||
        !normalized_feat->GetData().GetVariation().GetData().GetInstance().IsSetType() ||
        normalized_feat->GetData().GetVariation().GetData().GetInstance().GetType() != CVariation_inst::eType_identity) {
        return normalized_feat;
    } 

    auto& var_inst = normalized_feat->SetData().SetVariation().SetData().SetInstance();
/*
    if (utils.HasIntronOffset(var_inst)) {
        return normalized_feat;
    }

    if (var_inst.IsSetDelta()) {
        auto& delta_item = *var_inst.SetDelta().back();
        if( delta_item.IsSetSeq() &&
            delta_item.GetSeq().IsLiteral() &&
            !delta_item.GetSeq().GetLiteral().IsSetSeq_data()) {
            auto new_literal = utils.GetLiteralAtLoc(normalized_feat->GetLocation(), m_Scope);
            delta_item.SetSeq().SetLiteral(*new_literal);
        }
    }

    var_inst.SetObservation(CVariation_inst::eObservation_asserted);
*/
    NormalizeIdentityInstance(var_inst, normalized_feat->GetLocation());
    return normalized_feat;
}





CRef<CSeq_feat> CNormalizeVariant::GetNormalizedSNP(const CSeq_feat& seq_feat) const
{
    auto normalized_feat = Ref(new CSeq_feat());
    normalized_feat->Assign(seq_feat);

    if (!normalized_feat->IsSetData() ||
        !normalized_feat->GetData().IsVariation() ||
        !normalized_feat->GetData().GetVariation().IsSetData() ||
        !normalized_feat->GetData().GetVariation().GetData().IsSet() ||
        !normalized_feat->GetData().GetVariation().GetData().GetSet().IsSetVariations() ||
        normalized_feat->GetData().GetVariation().GetData().GetSet().GetVariations().size() != 2) {

        return normalized_feat;
    }

    const auto& snv_inst = normalized_feat->GetData().GetVariation().GetData().GetSet().GetVariations().front()->GetData().GetInstance();

    if (!snv_inst.IsSetType() ||
        snv_inst.GetType() != CVariation_inst::eType_identity) 
    {
        return normalized_feat;
    }



    auto& identity_inst = normalized_feat->SetData().SetVariation().SetData().SetSet().SetVariations().back()->SetData().SetInstance();

    NormalizeIdentityInstance(identity_inst, normalized_feat->GetLocation());

    return normalized_feat;
}


void CNormalizeVariant::NormalizeIdentityInstance(CVariation_inst& identity_inst, const CSeq_loc& location) const
{
    // Should post a warning
    if (identity_inst.GetType() != CVariation_inst::eType_identity)
    {
        // Post a warning
        return;
    }

    if (!identity_inst.IsSetDelta()) {
        // Post a warning
        return;
    }

    if (utils.HasIntronOffset(identity_inst)) {
        // Post a warning
        return;
    }


    auto& delta_item = *identity_inst.SetDelta().back();

    if (!delta_item.IsSetSeq()) {
        // Post a warning
        return;
    }


    if (delta_item.IsSetSeq() &&
        (!delta_item.GetSeq().IsLiteral() ||
        !delta_item.GetSeq().GetLiteral().IsSetSeq_data())) {
        auto new_literal = utils.GetLiteralAtLoc(location, m_Scope);
        delta_item.SetSeq().SetLiteral(*new_literal);
    }
    identity_inst.SetObservation(CVariation_inst::eObservation_asserted); 
}



void CValidateVariant::ValidateNaIdentityInst(CVariation_inst& identity_inst, const CSeq_loc& location) const
{
    if (!identity_inst.IsSetType() ||
         identity_inst.GetType() != CVariation_inst::eType_identity)
    {
        // Throw an exception
    }


    if (!identity_inst.IsSetDelta()) {
        // Throw an exception
    }


    auto& delta_item = *identity_inst.GetDelta().back();

    if (!delta_item.IsSetSeq()) {
        // Throw an exception
    }


    if (!delta_item.GetSeq().IsLiteral()) {
        // Throw an exception
    }

//    ValidateNaSeqLiteral(delta_item.GetSeq().GetLiteral(), location);

    return;
}



void CValidateVariant::ValidateAaIdentityInst(CVariation_inst& identity_inst, const CSeq_loc& location) const
{
}


END_SCOPE(objects)
END_NCBI_SCOPE
