#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objtools/readers/hgvs/post_process.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>
#include <corelib/ncbidiag.hpp>

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

    if (!identity_inst.SetType()) {
        ERR_POST(Warning << "CVariation_inst: Type not set");
        return;
    }

    if (identity_inst.GetType() != CVariation_inst::eType_identity)
    {
        string message = "CVariation_inst: Invalid type ("
                       + NStr::NumericToString(identity_inst.GetType())
                       + ").";
        ERR_POST(Warning << message);
        return;
    }

    if (!identity_inst.IsSetDelta()) {
        ERR_POST(Warning << "CVariation_inst: Delta-item not set");
        return;
    }

    if (utils.HasIntronOffset(identity_inst)) {
        // Post a warning
        return;
    }


    auto& delta_item = *identity_inst.SetDelta().back();

    if (!delta_item.IsSetSeq()) {
        ERR_POST(Warning << "CDelta_item: Sequence not set");
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


CRef<CSeq_feat> g_NormalizeVariationSeqfeat(const CSeq_feat& feat,
                                            CScope* scope)
{
    CNormalizeVariant normalizer(*scope);
    return normalizer.GetNormalizedIdentity(feat);
}


void s_ValidateSeqLiteral(const CSeq_literal& literal, const CSeq_loc& location, CScope* scope, bool IsCDS=false)
{
    if (IsCDS) {
        ERR_POST(Warning << "Cannot validate Seq-literal in CDS variant");
        return;
    }


    if (literal.IsSetLength() &&
        location.IsInt()) {
        const auto literal_length = literal.GetLength();
        const auto interval_length = sequence::GetLength(location, scope);
        if (literal_length != interval_length) {
            string message = "Literal length (" + NStr::IntToString(literal_length) 
                             + ") not consistent with interval length (" + NStr::IntToString(interval_length) + ")";
            NCBI_THROW(CVariationValidateException, 
                       eSeqliteralIntervalError, 
                       message);
        }
    }
}


// Turn this into a static method maybe. 
// Just use the class to organise the data
void CValidateVariant::ValidateIdentityInst(const CVariation_inst& identity_inst, const CSeq_loc& location, bool IsCDS) 
{
    if (!identity_inst.IsSetType() ||
         identity_inst.GetType() != CVariation_inst::eType_identity)
    {
        string message = "CVariation_inst: ";
        if (!identity_inst.IsSetType()) {
            message += "Type not set. ";
        } else {
            message += "Invalid type (" 
                    + NStr::NumericToString(identity_inst.GetType()) 
                    + ").";
        }
        message += "Expected eType_identity"; 
        
        NCBI_THROW(CVariationValidateException,
                   eInvalidType,
                   message);
    }

    if (!identity_inst.IsSetDelta()) {
        NCBI_THROW(CVariationValidateException, 
                   eIncompleteObject,
                   "Delta-item not set");
    }

    auto& delta_item = *identity_inst.GetDelta().back();
    if (!delta_item.IsSetSeq()) {
        NCBI_THROW(CVariationValidateException,
                   eIncompleteObject,
                   "Seq not set");
    }

    if (!delta_item.GetSeq().IsLiteral()) {
        NCBI_THROW(CVariationValidateException,
                   eInvalidType,
                   "Seq-literal expected");
    }

    const auto& literal = delta_item.GetSeq().GetLiteral();

    // Check that the sequence location is consistent with the specified sequence
    s_ValidateSeqLiteral(literal, location, &m_Scope, IsCDS);
    return;
}



void CValidateVariant::ValidateMicrosatelliteInst(const CVariation_inst& ms_inst, const CSeq_loc& location, bool IsCDS)
{
    if (!ms_inst.IsSetType())
    {
        string message = "CVariation_inst: ";
        message += "Type not set. ";
        
        NCBI_THROW(CVariationValidateException,
                   eInvalidType,
                   message);
    }
    
    // Only worry about microsatellites here 
    if (ms_inst.GetType() != CVariation_inst::eType_microsatellite) {
        return;
    }

    if (!ms_inst.IsSetDelta()) {
        NCBI_THROW(CVariationValidateException, 
                   eIncompleteObject,
                   "Delta-item not set");
    }
    
    auto& delta_item = *ms_inst.GetDelta().back();
    if (!delta_item.IsSetSeq()) {
        NCBI_THROW(CVariationValidateException,
                   eIncompleteObject,
                   "Seq not set");
    }

    if (delta_item.GetSeq().IsLiteral()) {
        s_ValidateSeqLiteral(delta_item.GetSeq().GetLiteral(),
                             location,
                             & m_Scope,
                             IsCDS);
    }
}


void g_ValidateVariationSeqfeat(const CSeq_feat& feat, CScope* scope, bool IsCDS)
{
    if (!feat.IsSetData() ||
        !feat.GetData().IsVariation() ||
        !feat.GetData().GetVariation().IsSetData()) {
        NCBI_THROW(CVariationValidateException,
                eIncompleteObject,
                "Variation feature data not specified" );
    }

    if (feat.GetData().GetVariation().GetData().IsInstance()) {
        // Attempt to validate microsatellite
        const auto& var_inst = feat.GetData().GetVariation().GetData().GetInstance();
        CValidateVariant validator(*scope);
        validator.ValidateMicrosatelliteInst(var_inst, feat.GetLocation(), IsCDS);
        return;
    } else 
    if (!feat.GetData().GetVariation().GetData().IsSet()) {
        return;
    }

    const auto& data_set = feat.GetData().GetVariation().GetData().GetSet();

    // We are only consider package variants from here on
    if (!data_set.IsSetType() ||
        data_set.GetType() != CVariation_ref::TData::TSet::eData_set_type_package ||
        !data_set.IsSetVariations()) {
        return;
    }

    // reference to list<CRef<CVariation_ref>> 
    auto&& variation_list = data_set.GetVariations();
    if (variation_list.size() != 2) {
        // Should Throw an exception here
        return;
     }

    auto&& assertion = variation_list.front();
    auto&& reference = variation_list.back();

    if (!assertion->IsSetData() ||
        !reference->IsSetData()) {
        NCBI_THROW(CVariationValidateException,
                   eIncompleteObject,
                   "Variation-ref data field not set");
    }
    // Check the reference Variation-ref
    if (!reference->GetData().IsInstance()) {
         NCBI_THROW(CVariationValidateException,
                    eInvalidType, 
                    "Variation-ref does not reference Variation-inst object");
    }
    const auto& reference_inst = reference->GetData().GetInstance();
    if (!reference_inst.IsSetType()) {
        NCBI_THROW(CVariationValidateException,
                   eIncompleteObject,
                   "Variation-inst type not specified");
    }


    // Check the asserted Variation-ref
    if (assertion->GetData().IsUnknown()) {
        return;
    }
    if (!assertion->GetData().IsInstance()) {
         NCBI_THROW(CVariationValidateException,
                    eInvalidType, 
                    "Variation-ref does not reference Variation-inst object");
    }

    const auto& assertion_inst = assertion->GetData().GetInstance();

    if (!assertion_inst.IsSetType()) {
        NCBI_THROW(CVariationValidateException,
                   eIncompleteObject,
                   "Variation-inst type not specified");
    }

    const auto type = assertion_inst.GetType();
    // Checking for duplications, deletions, inversions, and Indels
    // Note that duplications have eType_ins
    if (type != CVariation_inst::eType_ins && 
        type != CVariation_inst::eType_del &&
        type != CVariation_inst::eType_delins &&
        type != CVariation_inst::eType_inv) {
        return; 
    }

    CValidateVariant validator(*scope);
    validator.ValidateIdentityInst(reference_inst, feat.GetLocation(), IsCDS);

    return;
}

END_SCOPE(objects)
END_NCBI_SCOPE
