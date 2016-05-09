#include <ncbi_pch.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/readers/hgvs/protein_irep_to_seqfeat.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_common.hpp>
#include <objtools/readers/hgvs/variation_ref_utils.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE
USING_SCOPE(NHgvsTestUtils);

CNCBIeaa CHgvsProtIrepReader::x_ConvertToNcbieaa(string aa_seq) const
{
    NStr::ReplaceInPlace(aa_seq, "Gly", "G");
    NStr::ReplaceInPlace(aa_seq, "Pro", "P");
    NStr::ReplaceInPlace(aa_seq, "Ala", "A");
    NStr::ReplaceInPlace(aa_seq, "Val", "V");
    NStr::ReplaceInPlace(aa_seq, "Leu", "L");
    NStr::ReplaceInPlace(aa_seq, "Ile", "I");
    NStr::ReplaceInPlace(aa_seq, "Met", "M");
    NStr::ReplaceInPlace(aa_seq, "Cys", "C");
    NStr::ReplaceInPlace(aa_seq, "Phe", "F");
    NStr::ReplaceInPlace(aa_seq, "Tyr", "Y");
    NStr::ReplaceInPlace(aa_seq, "Trp", "W");
    NStr::ReplaceInPlace(aa_seq, "His", "H");
    NStr::ReplaceInPlace(aa_seq, "Lys", "K");
    NStr::ReplaceInPlace(aa_seq, "Arg", "R");
    NStr::ReplaceInPlace(aa_seq, "Gln", "Q");
    NStr::ReplaceInPlace(aa_seq, "Asn", "N");
    NStr::ReplaceInPlace(aa_seq, "Glu", "E");
    NStr::ReplaceInPlace(aa_seq, "Asp", "D");
    NStr::ReplaceInPlace(aa_seq, "Ser", "S");
    NStr::ReplaceInPlace(aa_seq, "Thr", "T");
    NStr::ReplaceInPlace(aa_seq, "Ter", "*");

    CNCBIeaa result(aa_seq);
    return result;
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateInsertedRawseqSubvarref(
        const string& raw_seq) const 
{
    auto ncbieaa_seq = x_ConvertToNcbieaa(raw_seq);
    auto seq_literal = Ref(new CSeq_literal());
    seq_literal->SetSeq_data().SetNcbieaa(ncbieaa_seq);
    auto seq_length = ncbieaa_seq.Get().size();
    seq_literal->SetLength(seq_length);
    return g_CreateInsertion(*seq_literal);
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateInsertionSubvarref(
        const CSeq_literal::TLength seq_length) const 
{
    auto seq_literal = Ref(new CSeq_literal());
    seq_literal->SetLength(seq_length);
    return g_CreateInsertion(*seq_literal);
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateInsertedCountSubvarref(
        const CCount& count) const 
{
    const auto seq_length = count.GetVal();
    return x_CreateInsertionSubvarref(seq_length);
}


// I could template this function, maybe
CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateInsertionSubvarref(
        const CInsertion::TSeqinfo& insert) const 
{
    CRef<CVariation_ref> var_ref;
    if ( insert.IsRaw_seq() ) {
        var_ref = x_CreateInsertedRawseqSubvarref(insert.GetRaw_seq());
    } 
    else if ( insert.IsCount() ) {
        var_ref = x_CreateInsertedCountSubvarref(insert.GetCount());
    } 
    else {
        NCBI_THROW(CVariationIrepException, eInvalidInsertion, "Unrecognized insertion type");
    }
    return var_ref;
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateDelinsSubvarref(
        const CDelins::TInserted_seq_info& insert) const 
{
    CRef<CDelta_item> delta;
    auto seq_literal = Ref(new CSeq_literal());
    if ( insert.IsRaw_seq() ) {
        const auto raw_seq = x_ConvertToNcbieaa(insert.GetRaw_seq());
        seq_literal->SetSeq_data().SetNcbieaa(raw_seq);
        seq_literal->SetLength(raw_seq.Get().size());
    } 
    else if ( insert.IsCount() ) {
        const auto seq_length = insert.GetCount().GetVal();
        seq_literal->SetLength(seq_length);
    } 
    else {
        NCBI_THROW(CVariationIrepException, eInvalidInsertion, "Unrecognized insertion type");
    }

    return g_CreateDelins(*seq_literal);
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateIdentitySubvarref(
        const CAaLocation& aa_loc) const
{
    if ( !aa_loc.IsSite() && !aa_loc.IsInt() ) {
        NCBI_THROW(CVariationIrepException, eInvalidLocation, "Unrecognized amino-acid location type");
    }
    const auto aa_string = x_ConvertToNcbieaa(aa_loc.GetString());
    return  x_CreateIdentitySubvarref(aa_string, aa_loc.size());
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateIdentitySubvarref(const string& seq_string,
        const CSeq_literal::TLength length) const 
{
    auto ncbieaa_seq = x_ConvertToNcbieaa(seq_string);
    auto seq_literal = Ref(new CSeq_literal());
    seq_literal->SetSeq_data().SetNcbieaa(ncbieaa_seq);
    seq_literal->SetLength(length);

    return g_CreateIdentity(seq_literal);
}


CRef<CSeq_feat> CHgvsProtIrepReader::x_CreateSeqfeat(const string& var_name,
        const string& identifier, 
        const CSimpleVariant& var_desc) const 
{
    auto seq_feat = Ref(new CSeq_feat());
    auto var_ref = x_CreateVarref(var_name, identifier, var_desc);
    seq_feat->SetData().SetVariation(*var_ref);


    const auto seq_id = m_IdResolver->GetAccessionVersion(identifier).GetSeqId();

    auto seq_loc = CAaSeqlocHelper::CreateSeqloc(*seq_id, 
            var_desc, 
            m_MessageListener);
    seq_feat->SetLocation(*seq_loc);
    return seq_feat;
}



CRef<CSeq_feat> CHgvsProtIrepReader::CreateSeqfeat(CRef<CVariantExpression>& variant_expr) const 
{
    const auto& sequence_variant = variant_expr->GetSequence_variant();
    if (sequence_variant.IsSetComplex()) {
        string message;
        if (sequence_variant.GetComplex().IsChimera()) {
            message = "Chimeras ";
        } else if (sequence_variant.GetComplex().IsMosaic()) {
            message = "Mosaics ";
        }    
        message += "are not currently supported.";
        message += " Please report to variation-services@ncbi.nlm.nih.gov";
        NCBI_THROW(CVariationIrepException, eUnsupported, message);
    }    

    const auto& simple_variant = sequence_variant.GetSubvariants().front()->GetSimple();
    const auto seq_type = sequence_variant.GetSeqtype();

    if (seq_type != eVariantSeqType_p) {
        NCBI_THROW(CVariationIrepException, eInvalidSeqType, "Protein sequence expected");
    }

    return x_CreateSeqfeat(variant_expr->GetInput_expr(),
                           variant_expr->GetReference_id(),
                           simple_variant);
}


list<CRef<CSeq_feat>> CHgvsProtIrepReader::CreateSeqfeats(CRef<CVariantExpression>& variant_expr) const 
{
    list<CRef<CSeq_feat>> feat_list;

    const auto& seq_var = variant_expr->GetSequence_variant();
  //  for (const auto& seq_var : variant_expr->GetSeqvars()) {

        const auto seq_type = seq_var.GetSeqtype();
        if (seq_type != eVariantSeqType_p) {
            NCBI_THROW(CVariationIrepException, eInvalidSeqType, "Protein sequence expected");
        }

        for (const auto& variant : seq_var.GetSubvariants()) {
            const auto& simple_variant = variant->GetSimple();

            auto seq_feat = x_CreateSeqfeat(variant_expr->GetInput_expr(),
                                            variant_expr->GetReference_id(),
                                            simple_variant);
            if (seq_feat.NotNull()) {
                feat_list.push_back(seq_feat);
            }
        }
  //  }

    return feat_list;
}


// Need to add support for special variants


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateVarref(const string& var_name,
        const string& identifier,
        const CSimpleVariant& var_desc) const 
{
    CRef<CVariation_ref> var_ref;
    const auto& var_type = var_desc.GetType();

    const auto method = (var_desc.IsSetFuzzy() && var_desc.GetFuzzy()) ?
        CVariation_ref::eMethod_E_computational :
        CVariation_ref::eMethod_E_unknown;

    switch (var_type.Which())  {
        default: 
            NCBI_THROW(CVariationIrepException, eUnknownVariation, "Unknown variation type");
        case CSimpleVariant::TType::e_Prot_sub:
            var_ref = x_CreateProteinSubVarref(identifier, var_type.GetProt_sub(), method);
            break;
        case CSimpleVariant::TType::e_Del:
            var_ref = x_CreateProteinDelVarref(identifier, var_type.GetDel(), method);
            break;
        case CSimpleVariant::TType::e_Dup:
            var_ref = x_CreateProteinDupVarref(identifier, var_type.GetDup(), method);
            break; 
        case CSimpleVariant::TType::e_Ins:
            var_ref = x_CreateInsertionVarref(identifier, var_type.GetIns(), method);
            break;
        case CSimpleVariant::TType::e_Delins:
            var_ref = x_CreateDelinsVarref(identifier, var_type.GetDelins(), method);
            break;
        case CSimpleVariant::TType::e_Repeat:
            var_ref = x_CreateSSRVarref(identifier, var_type.GetRepeat(), method);
            break;
        case CSimpleVariant::TType::e_Frameshift:
            var_ref = x_CreateFrameshiftVarref(identifier, var_type.GetFrameshift(), method);
            break;
        case CSimpleVariant::TType::e_Prot_ext:
        {
            string message = "Protein-extension HGVS-to-Seqfeat conversion is not currently supported. Please report to variation-services@ncbi.nlm.nih.gov";
            NCBI_THROW(CVariationIrepException, eUnsupported, message);
        }
    }


    var_ref->SetName(var_name);

    return var_ref;
}



CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateProteinDelVarref(const string& identifier,
        const CDeletion& del,
        const CVariation_ref::EMethod_E method) const
{
    auto var_ref = Ref(new CVariation_ref());
    auto& var_set = var_ref->SetData().SetSet();
    var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package); // CVariation_ref needs a SetPackage method, I think.

    auto subvar_ref = g_CreateDeletion();
    if (method != CVariation_ref::eMethod_E_unknown) {
        subvar_ref->SetMethod().push_back(method);
    }
    var_set.SetVariations().push_back(subvar_ref);

    // Add Identity Variation-ref
    subvar_ref = x_CreateIdentitySubvarref(del.GetLoc().GetAaloc());
    var_set.SetVariations().push_back(subvar_ref);

    return var_ref;
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateInsertionVarref(const string& identifier,
        const CInsertion& ins,
        const CVariation_ref::EMethod_E method) const 
{
    auto var_ref = Ref(new CVariation_ref());
    auto& var_set = var_ref->SetData().SetSet();
    var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);

    // Add first subvariation
    auto subvar_ref = x_CreateInsertionSubvarref(ins.GetSeqinfo());
    if (method != CVariation_ref::eMethod_E_unknown) {
        subvar_ref->SetMethod().push_back(method);
    }
    var_set.SetVariations().push_back(subvar_ref);

    // Identity Variation-ref
    const auto& aa_int = ins.GetInt().GetAaint();
    const auto interval_string = x_ConvertToNcbieaa(aa_int.GetString());
    subvar_ref = x_CreateIdentitySubvarref(interval_string, aa_int.size());
    var_set.SetVariations().push_back(subvar_ref);

    return var_ref;
}



CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateDelinsVarref(const string& identifier,
        const CDelins& delins,
        const CVariation_ref::EMethod_E method) const 
{
    auto var_ref = Ref(new CVariation_ref());
    auto& var_set = var_ref->SetData().SetSet();
    var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);

    // Add first Variation-ref
    auto subvar_ref = x_CreateDelinsSubvarref(delins.GetInserted_seq_info());
    if (method != CVariation_ref::eMethod_E_unknown) {
        subvar_ref->SetMethod().push_back(method);
    }
    var_set.SetVariations().push_back(subvar_ref);

    // Identity Variation-ref
    subvar_ref = x_CreateIdentitySubvarref(delins.GetLoc().GetAaloc());
    var_set.SetVariations().push_back(subvar_ref);

    return var_ref;
}



CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateSSRVarref(const string& identifier,
        const CRepeat& ssr,
        const CVariation_ref::EMethod_E method) const 
{
    auto var_ref = Ref(new CVariation_ref());
    auto& var_set = var_ref->SetData().SetSet();
    var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);

    CRef<CSeq_literal> dummy_literal;
    auto delta =  CDeltaHelper::CreateSSR(ssr.GetCount(), dummy_literal,  m_MessageListener);
    auto subvar_ref = g_CreateMicrosatellite(delta);
    if (method != CVariation_ref::eMethod_E_unknown) {
        subvar_ref->SetMethod().push_back(method);
    }
    var_set.SetVariations().push_back(subvar_ref);

    // Identity Variation-ref
    subvar_ref = x_CreateIdentitySubvarref(ssr.GetLoc().GetAaloc());
    var_set.SetVariations().push_back(subvar_ref);

    return var_ref;
}



CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateProteinDupVarref(const string& identifier, 
        const CDuplication& dup,
        const CVariation_ref::EMethod_E method) const 
{
    auto var_ref = Ref(new CVariation_ref());
    auto& var_set = var_ref->SetData().SetSet();
    var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);

    // Add first subvariation
    auto subvar_ref = g_CreateDuplication();
    if (method != CVariation_ref::eMethod_E_unknown) {
        subvar_ref->SetMethod().push_back(method);
    }
    var_set.SetVariations().push_back(subvar_ref); 

    // Add identity Variation-ref
    subvar_ref = x_CreateIdentitySubvarref(dup.GetLoc().GetAaloc());
    var_set.SetVariations().push_back(subvar_ref);

    return var_ref;
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateProteinSubVarref(const string& dentifier, 
        const CProteinSub& aa_sub, 
        const CVariation_ref::EMethod_E method) const
{
    //const auto& aa_sub = sub.GetAasub();
    string final_aa;
    if ( aa_sub.GetType() == CProteinSub::eType_missense ) {
        final_aa = x_ConvertToNcbieaa(aa_sub.GetFinal());
    } 
    else if ( aa_sub.GetType() == CProteinSub::eType_nonsense ) {
        final_aa = "*";
    } 
    else if ( aa_sub.GetType() == CProteinSub::eType_unknown ) {
        final_aa = "?";
    } 
    else {
        NCBI_THROW(CVariationIrepException, eInvalidVariation, "Unrecognized protein substitution type");
    }

    auto var_ref = Ref(new CVariation_ref());
    auto& var_set = var_ref->SetData().SetSet();
    var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);

    // Add first subvariation
//    auto subvar_ref = Ref(new CVariation_ref());
    CRef<CVariation_ref> subvar_ref;
    if ( aa_sub.GetType() == CProteinSub::eType_unknown) {
        subvar_ref = Ref(new CVariation_ref());
        subvar_ref->SetData().SetUnknown();
    } else {
        //auto seq_data = Ref(new CSeq_data());
        const auto final_ncbieaa = x_ConvertToNcbieaa(final_aa);
        CSeq_data seq_data;
        seq_data.SetNcbieaa().Set(final_ncbieaa);
        subvar_ref = g_CreateMissense(seq_data);
    }
    if (method != CVariation_ref::eMethod_E_unknown) {
        subvar_ref->SetMethod().push_back(method);
    }

    var_set.SetVariations().push_back(subvar_ref);

    // Add Identity Variation-ref
    const auto initial_aa = x_ConvertToNcbieaa(aa_sub.GetInitial().GetAa());
    const TSeqPos length = 1;
    subvar_ref = x_CreateIdentitySubvarref(initial_aa, length);
    var_set.SetVariations().push_back(subvar_ref);

    return var_ref;
}


CRef<CVariation_ref> CHgvsProtIrepReader::x_CreateFrameshiftVarref(const string& identifier,
        const CFrameshift& fs,
        const CVariation_ref::EMethod_E method) const 
{
    auto var_ref = Ref(new CVariation_ref());
    auto& var_set = var_ref->SetData().SetSet();
    var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);

    // Add first subvariation
    auto subvar_ref = g_CreateFrameshift();
    if (method != CVariation_ref::eMethod_E_unknown) {
        subvar_ref->SetMethod().push_back(method);
    }
    var_set.SetVariations().push_back(subvar_ref);

    // Add Identity Variation-ref
    const auto first_aa = x_ConvertToNcbieaa(fs.GetAasite().GetAa());
    const TSeqPos length = 1;
    subvar_ref = x_CreateIdentitySubvarref(first_aa, length);
    var_set.SetVariations().push_back(subvar_ref);

    return var_ref;
}


CRef<CSeq_loc> CAaSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id, 
        const CSimpleVariant& var_desc, 
        CVariationIrepMessageListener& listener) 
{
    CRef<CSeq_loc> seq_loc;
    const auto& var_type = var_desc.GetType();

    switch(var_type.Which())  {
        default: 
            NCBI_THROW(CVariationIrepException, eUnknownVariation, "Unsupported variation type");
        case CSimpleVariant::TType::e_Prot_sub: 
            seq_loc = CreateSeqloc(seq_id, 
                    var_type.GetProt_sub().GetInitial(), 
                    listener); 
            break;
        case CSimpleVariant::TType::e_Del: 
            seq_loc = CreateSeqloc(seq_id, 
                    var_type.GetDel().GetLoc().GetAaloc(), 
                    listener);
            break;
        case CSimpleVariant::TType::e_Dup: 
            seq_loc = CreateSeqloc(seq_id, 
                    var_type.GetDup().GetLoc().GetAaloc(), 
                    listener);
            break;
        case CSimpleVariant::TType::e_Ins:  
            seq_loc = CreateSeqloc(seq_id, 
                    var_type.GetIns().GetInt().GetAaint(), 
                    listener);
            break;
        case CSimpleVariant::TType::e_Delins: 
            seq_loc = CreateSeqloc(seq_id, 
                    var_type.GetDelins().GetLoc().GetAaloc(), 
                    listener);
            break;
        case CSimpleVariant::TType::e_Repeat: 
            seq_loc = CreateSeqloc(seq_id, 
                    var_type.GetRepeat().GetLoc().GetAaloc(), 
                    listener);
            break;
        case CSimpleVariant::TType::e_Frameshift: 
            seq_loc = CreateSeqloc(seq_id, 
                    var_type.GetFrameshift().GetAasite(), 
                    listener);
            break;
    }

    return seq_loc;
}


CRef<CSeq_loc> CAaSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id,
        const CAaLocation& aa_loc, 
        CVariationIrepMessageListener& listener)
{
    if ( aa_loc.IsSite() ) {
        return CreateSeqloc(seq_id, aa_loc.GetSite(), listener);
    }

    if ( !aa_loc.IsInt() ) {
        NCBI_THROW(CVariationIrepException, eInvalidLocation, "Invalid protein sequence location");
    }
    return CreateSeqloc(seq_id, aa_loc.GetInt(), listener);
}


CRef<CSeq_loc> CAaSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id,
        const CAaSite& aa_site, 
        CVariationIrepMessageListener& listener) 
{
    const auto site_index = aa_site.GetIndex()-1;

    if ( site_index < 0 ) {
        NCBI_THROW(CVariationIrepException, eInvalidLocation, "Invalid sequence index");
    }

    auto seq_loc = Ref(new CSeq_loc());

    seq_loc->SetPnt().SetPoint(site_index);
    seq_loc->SetPnt().SetId().Assign(seq_id);
    seq_loc->SetPnt().SetStrand(eNa_strand_plus);
    return seq_loc;
}


CRef<CSeq_loc> CAaSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id,
        const CAaInterval& aa_int,  
        CVariationIrepMessageListener& listener)  
{
    if ( !aa_int.IsSetStart() || !aa_int.IsSetStop() ) {
        NCBI_THROW(CVariationIrepException, eInvalidInterval, "Undefined interval limits");
    }

    auto start_index = aa_int.GetStart().GetIndex()-1;
    auto stop_index = aa_int.GetStop().GetIndex()-1;

    if ( start_index > stop_index ) {
        string err_string = "Reversed interval limits";
        // Post a warning message and reverse interval limits
        CVariationIrepMessage msg(err_string, eDiag_Warning);
        msg.SetAaInterval(aa_int);
        listener.Post(msg);
        const auto temp = start_index;
        stop_index = start_index;
        start_index = temp;
    }

    auto seq_loc = Ref(new CSeq_loc());

    seq_loc->SetInt().SetId().Assign(seq_id);
    seq_loc->SetInt().SetFrom(start_index);
    seq_loc->SetInt().SetTo(stop_index);
    seq_loc->SetInt().SetStrand(eNa_strand_plus);

    return seq_loc;
}


END_objects_SCOPE
END_NCBI_SCOPE

