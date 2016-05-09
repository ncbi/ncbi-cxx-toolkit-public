#include <ncbi_pch.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/readers/hgvs/na_irep_to_seqfeat.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_common.hpp>
#include <objtools/readers/hgvs/variation_ref_utils.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

USING_SCOPE(NHgvsTestUtils);

CRef<CSeq_literal> CHgvsNaIrepReader::x_CreateNtSeqLiteral(const string& raw_seq) const
{
    auto lit = x_CreateNtSeqLiteral(raw_seq.size());
    lit->SetSeq_data().SetIupacna(CIUPACna(raw_seq));
    return lit;
}


CRef<CSeq_literal> CHgvsNaIrepReader::x_CreateNtSeqLiteral(TSeqPos length) const 
{
    auto lit = Ref(new CSeq_literal);
    lit->SetLength(length);
    return lit;
}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateNaIdentityVarref(const CNaIdentity& identity,
                                                                 const CVariation_ref::EMethod_E method) const
{
    string nucleotide = identity.IsSetNucleotide() ? 
                        identity.GetNucleotide() : "";

    return x_CreateNaIdentityVarref(identity.GetLoc(), nucleotide, method);

}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateNaIdentityVarref(const CNtLocation& nt_loc,
                                                                 const string& nucleotide,
                                                                 const CVariation_ref::EMethod_E method) const
{

    auto nt_literal = Ref(new CSeq_literal());

    auto length = nucleotide.empty() ? 1 : nucleotide.size();

    if (length > 1) {
        // Throw an exception
    }

    nt_literal->SetLength(length);
    if (!nucleotide.empty()) {
        nt_literal->SetSeq_data().SetIupacna(CIUPACna(nucleotide));
    }
    auto offset = CHgvsNaDeltaHelper::GetIntronOffset(nt_loc.GetSite());
    auto var_ref = g_CreateIdentity(nt_literal, offset);

    if (method != CVariation_ref::eMethod_E_unknown) {
       var_ref->SetMethod().push_back(method); 
    }

    return var_ref;
}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateNaSubVarref(const CNtLocation& nt_loc, 
                                                            const string& initial_nt,
                                                            const string& final_nt,
                                                            const CVariation_ref::EMethod_E method) const 
{

    if (!initial_nt.empty() &&
        (initial_nt.size() != final_nt.size())) {
      // Throw an exception
    }


    auto var_ref = Ref(new CVariation_ref());
    auto& var_set = var_ref->SetData().SetSet();
    var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);

    CRef<CDelta_item> offset;
    if (nt_loc.IsSite()) {
        offset = CHgvsNaDeltaHelper::GetIntronOffset(nt_loc.GetSite());
    }

    { 
        CSeq_data result(final_nt, CSeq_data::e_Iupacna);
        //auto subvar_ref = Ref(new CVariation_ref());

        CRef<CVariation_ref> subvar_ref;
        if (final_nt.size() > 1) {
            subvar_ref = g_CreateMNP(result,
                                     final_nt.size(),
                                     offset);
     //       subvar_ref->SetMNP(result, 
     //                          final_nt.size(),
     //                          offset);
        } else {
            subvar_ref = g_CreateSNV(result, offset);
            //subvar_ref->SetSNV(result, offset);
        }
        if (method != CVariation_ref::eMethod_E_unknown) {
            subvar_ref->SetMethod().push_back(method);
        }
        var_set.SetVariations().push_back(subvar_ref);
    }

    {
        CRef<CSeq_literal> initial;
        if (initial_nt.empty() &&
            nt_loc.IsSite()) {
            initial = x_CreateNtSeqLiteral(1); // Single, unknown residue
        } else {
            initial = x_CreateNtSeqLiteral(initial_nt);
        }
        auto subvar_ref = g_CreateIdentity(initial, offset);
        var_set.SetVariations().push_back(subvar_ref);
    }
    return var_ref;
}



CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateNaSubVarref(const CNaSub& sub,
                                                            const CVariation_ref::EMethod_E method) const
{
    const auto& nt_loc = sub.GetLoc();
    const auto initial_nt = sub.GetInitial();
    const auto final_nt = sub.GetFinal();

    return x_CreateNaSubVarref(nt_loc, 
                               initial_nt,
                               final_nt,
                               method);
}




CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateDeletionVarref(const CDeletion& del,
                                                               const CVariation_ref::EMethod_E method) const
{
    const auto& nt_loc = del.GetLoc().GetNtloc();

    auto start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(nt_loc);

    auto stop_offset = CHgvsNaDeltaHelper::GetStopIntronOffset(nt_loc);


    if (del.IsSetRaw_seq()) {
        auto var_ref = Ref(new CVariation_ref());
        auto& var_set = var_ref->SetData().SetSet();
        var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);
        {
           // auto subvar_ref = Ref(new CVariation_ref());
           // subvar_ref->SetDeletion(start_offset, stop_offset);
            auto subvar_ref = g_CreateDeletion(start_offset, stop_offset);
            if (method != CVariation_ref::eMethod_E_unknown) {
                subvar_ref->SetMethod().push_back(method);
            }
            var_set.SetVariations().push_back(subvar_ref);
        }
        {
            auto raw_seq = x_CreateNtSeqLiteral(del.GetRaw_seq());
            auto subvar_ref = g_CreateIdentity(raw_seq, start_offset, stop_offset);
            var_set.SetVariations().push_back(subvar_ref);
        }
        return var_ref;
    }
    
    return g_CreateDeletion(start_offset, stop_offset);
}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateInsertionVarref(const CInsertion& ins,
                                                                const CVariation_ref::EMethod_E method) const
{
    const auto& nt_int = ins.GetInt().GetNtint();

    auto start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(nt_int);

    auto stop_offset = CHgvsNaDeltaHelper::GetStopIntronOffset(nt_int);

    auto raw_seq = x_CreateNtSeqLiteral(ins.GetSeqinfo().GetRaw_seq());
    auto var_ref = g_CreateInsertion(*raw_seq, start_offset, stop_offset);
    if (method != CVariation_ref::eMethod_E_unknown) {
        var_ref->SetMethod().push_back(method);
    }

    return var_ref;
}


bool CHgvsNaIrepReader::x_LooksLikePolymorphism(const CDelins& delins) const 
{
    if (delins.IsSetDeleted_raw_seq() && 
        delins.GetInserted_seq_info().IsRaw_seq() &&
        delins.GetDeleted_raw_seq().size() ==
        delins.GetInserted_seq_info().GetRaw_seq().size()) {
        return true;
    }


    if (delins.GetLoc().GetNtloc().IsSite() &&
        delins.GetInserted_seq_info().IsRaw_seq() &&
        delins.GetInserted_seq_info().GetRaw_seq().size() == 1) {
        return true;
    }

    return false;
}
                                        

// Need to refactor since this repeats most of x_CreateInsertionVarref
CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateDelinsVarref(const CDelins& delins,
                                                             const CVariation_ref::EMethod_E method) const
{

    const auto& nt_loc = delins.GetLoc().GetNtloc();

    auto start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(nt_loc);
    auto stop_offset = CHgvsNaDeltaHelper::GetStopIntronOffset(nt_loc);

    auto inserted_seq = x_CreateNtSeqLiteral(delins.GetInserted_seq_info().GetRaw_seq());

    if (delins.IsSetDeleted_raw_seq()) {
        auto var_ref = Ref(new CVariation_ref());
        auto& var_set = var_ref->SetData().SetSet();
        var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);
        {
            auto subvar_ref = g_CreateDelins(*inserted_seq, start_offset, stop_offset);
            if (method != CVariation_ref::eMethod_E_unknown) {
                subvar_ref->SetMethod().push_back(method);
            }
            var_set.SetVariations().push_back(subvar_ref);
        }
        {
            auto deleted_seq = x_CreateNtSeqLiteral(delins.GetDeleted_raw_seq());
            auto subvar_ref = g_CreateIdentity(deleted_seq, start_offset, stop_offset);
            var_set.SetVariations().push_back(subvar_ref);
        }
        return var_ref;
    }

    return g_CreateDelins(*inserted_seq, start_offset, stop_offset);
}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateDuplicationVarref(const CDuplication& dup, 
                                                                  const CVariation_ref::EMethod_E method) const
{

    const auto& nt_loc = dup.GetLoc().GetNtloc();

    auto start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(nt_loc);

    auto stop_offset = CHgvsNaDeltaHelper::GetStopIntronOffset(nt_loc);

    if (dup.IsSetRaw_seq()) {
        auto var_ref = Ref(new CVariation_ref());
        auto& var_set = var_ref->SetData().SetSet();
        var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);
        {
            auto subvar_ref = g_CreateDuplication(start_offset, stop_offset);
            if (method != CVariation_ref::eMethod_E_unknown) {
                subvar_ref->SetMethod().push_back(method);
            }
            var_set.SetVariations().push_back(subvar_ref);
        }
        {
            auto duplicated_seq = x_CreateNtSeqLiteral(dup.GetRaw_seq());
            auto subvar_ref = g_CreateIdentity(duplicated_seq, start_offset, stop_offset);
            var_set.SetVariations().push_back(subvar_ref);
        }
        return var_ref;
    } 
    
    return g_CreateDuplication(start_offset, stop_offset);
}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateInversionVarref(const CInversion& inv,
                                                                const CVariation_ref::EMethod_E method) const
{

    const auto& nt_int = inv.GetNtint();

    auto start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(nt_int);

    auto stop_offset = CHgvsNaDeltaHelper::GetStopIntronOffset(nt_int);

    if (inv.IsSetRaw_seq() || inv.IsSetSize()) {
        auto var_ref = Ref(new CVariation_ref());
        auto& var_set = var_ref->SetData().SetSet();
        var_set.SetType(CVariation_ref::TData::TSet::eData_set_type_package);
        {
            auto subvar_ref = g_CreateInversion(start_offset, stop_offset);

            if (method != CVariation_ref::eMethod_E_unknown) {
                subvar_ref->SetMethod().push_back(method);
            }
            var_set.SetVariations().push_back(subvar_ref);
        }
        if (inv.IsSetRaw_seq()) {
            auto inverted_seq = x_CreateNtSeqLiteral(inv.GetRaw_seq());
            auto subvar_ref = g_CreateIdentity(inverted_seq, start_offset, stop_offset);
            var_set.SetVariations().push_back(subvar_ref);
        } else if (inv.IsSetSize()) {
            auto inversion_size = x_CreateNtSeqLiteral(inv.GetSize());
            const bool enforce_assert = true;
            auto subvar_ref = g_CreateIdentity(inversion_size, start_offset, stop_offset, enforce_assert);
            var_set.SetVariations().push_back(subvar_ref);
        }
        return var_ref;
    } 
   
    return g_CreateInversion(start_offset, stop_offset);
}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateConversionVarref(const CConversion& conv,
                                                                 const CVariation_ref::EMethod_E method) const
{
    {
        const auto& replacement_int = conv.GetDst();
        const auto start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(replacement_int);
        const auto stop_offset  = CHgvsNaDeltaHelper::GetStopIntronOffset(replacement_int);

        if (start_offset || stop_offset) {
            string message = "Nucleotide conversions with replacement intervals that begin or end in an intron are not currently supported.";
            message += " Please report to variation-services@ncbi.nlm.nih.gov";
            NCBI_THROW(CVariationIrepException, eUnsupported, message);
        }
    }

    const auto& nt_int = conv.GetSrc();

    auto start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(nt_int);
    auto stop_offset = CHgvsNaDeltaHelper::GetStopIntronOffset(nt_int);



    auto id_string = conv.GetDst().GetStart().GetSite().GetSeqid(); 
    auto seq_type = conv.GetDst().GetStart().GetSite().GetSeqtype();

    auto seq_id = m_IdResolver->GetAccessionVersion(id_string).GetSeqId();

    auto seq_loc = CNtSeqlocHelper::CreateSeqloc(*seq_id,
                                                  conv.GetDst(),
                                                  seq_type,
                                                  m_Scope,
                                                  m_MessageListener);
    
    auto var_ref = g_CreateConversion(*seq_loc, start_offset, stop_offset);
    if (method != CVariation_ref::eMethod_E_unknown) {
        var_ref->SetMethod().push_back(method);
    }
    return var_ref;
}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateSSRVarref(const CRepeat& ssr,
                                                          const CVariation_ref::EMethod_E method) const
{
    CRef<CSeq_literal> seq_literal;
    if (ssr.IsSetRaw_seq()) {
        seq_literal = Ref(new CSeq_literal());
        const auto raw_seq = ssr.GetRaw_seq();
        seq_literal->SetLength(raw_seq.size());
        seq_literal->SetSeq_data().SetIupacna(CIUPACna(raw_seq));
    }

    const auto& nt_loc = ssr.GetLoc().GetNtloc();

    auto start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(nt_loc);

    auto stop_offset = CHgvsNaDeltaHelper::GetStopIntronOffset(nt_loc);

    auto delta = CDeltaHelper::CreateSSR(ssr.GetCount(), seq_literal, m_MessageListener);
    auto var_ref = g_CreateMicrosatellite(delta, start_offset, stop_offset);
    if (method != CVariation_ref::eMethod_E_unknown) {
        var_ref->SetMethod().push_back(method);
    }
    return var_ref;
}


CRef<CVariation_ref> CHgvsNaIrepReader::x_CreateVarref(const string& var_name,
                                                       const CSimpleVariant& var_desc) const
{
    CRef<CVariation_ref> var_ref;
    auto& var_type = var_desc.GetType();


    CVariation_ref::EMethod_E method = (var_desc.IsSetFuzzy() && var_desc.GetFuzzy()) ?
        CVariation_ref::eMethod_E_computational :
        CVariation_ref::eMethod_E_unknown;

    switch (var_type.Which()) {
       default:
           NCBI_THROW(CVariationIrepException, eUnknownVariation, "Unknown variation type");
       case CSimpleVariant::TType::e_Na_identity:
           var_ref = x_CreateNaIdentityVarref(var_type.GetNa_identity(), method);
           break;
       case CSimpleVariant::TType::e_Na_sub:
           {
               const auto& na_sub = var_type.GetNa_sub();
               const auto initial_nt = na_sub.GetInitial();
               const auto final_nt = na_sub.GetFinal();
               if (final_nt == initial_nt) {
                   var_ref = x_CreateNaIdentityVarref(na_sub.GetLoc(),
                                                      initial_nt,
                                                      method);
                   break;
               }
           }
           var_ref = x_CreateNaSubVarref(var_type.GetNa_sub(), method);
           break;
       case CSimpleVariant::TType::e_Dup:
           var_ref = x_CreateDuplicationVarref(var_type.GetDup(), method);
           break;
       case CSimpleVariant::TType::e_Del:
           var_ref = x_CreateDeletionVarref(var_type.GetDel(), method);
           break;
       case CSimpleVariant::TType::e_Ins:
           var_ref = x_CreateInsertionVarref(var_type.GetIns(), method);
           break;
       case CSimpleVariant::TType::e_Delins:
           if (x_LooksLikePolymorphism(var_type.GetDelins())) {
               const auto& delins = var_type.GetDelins();

               const auto& deleted = delins.IsSetDeleted_raw_seq() ?
                                     delins.GetDeleted_raw_seq() : 
                                     "";

               var_ref = x_CreateNaSubVarref(delins.GetLoc().GetNtloc(),
                                             deleted,
                                             delins.GetInserted_seq_info().GetRaw_seq(),
                                             method);
               break;
           }
           var_ref = x_CreateDelinsVarref(var_type.GetDelins(), method);
           break;
       case CSimpleVariant::TType::e_Inv:
           var_ref = x_CreateInversionVarref(var_type.GetInv(), method);
           break;
       case CSimpleVariant::TType::e_Conv:
           var_ref = x_CreateConversionVarref(var_type.GetConv(), method);
           break;
       case CSimpleVariant::TType::e_Repeat:
           var_ref = x_CreateSSRVarref(var_type.GetRepeat(), method);
           break;
    }

    var_ref->SetName(var_name);

    return var_ref;
}



CRef<CSeq_feat> CHgvsNaIrepReader::x_CreateSeqfeat(const string& var_name,
                                                   const string& id_string,
                                                   const CSequenceVariant::TSeqtype& seq_type,
                                                   const CSimpleVariant& var_desc) const 
{
    auto seq_feat = Ref(new CSeq_feat());
    auto var_ref = x_CreateVarref(var_name, var_desc);
    seq_feat->SetData().SetVariation(*var_ref);

    auto seq_id = m_IdResolver->GetAccessionVersion(id_string).GetSeqId();


    auto seq_loc = CNtSeqlocHelper::CreateSeqloc(*seq_id, 
                                                  var_desc,
                                                  seq_type,
                                                  m_Scope,
                                                  m_MessageListener);
    seq_feat->SetLocation(*seq_loc);

    return seq_feat;
}


CRef<CSeq_feat> CHgvsNaIrepReader::CreateSeqfeat(CRef<CVariantExpression>& variant_expr) const 
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


    const auto& simple_variant = variant_expr->GetSequence_variant().GetSubvariants().front()->GetSimple();
    const auto seq_type = variant_expr->GetSequence_variant().GetSeqtype();

    if (seq_type == eVariantSeqType_p || 
        seq_type == eVariantSeqType_u) {
        // Throw an exception - invalid sequence type
    }

    return x_CreateSeqfeat(variant_expr->GetInput_expr(),
                           variant_expr->GetReference_id(),
                           seq_type,
                           simple_variant);
}

list<CRef<CSeq_feat>> CHgvsNaIrepReader::CreateSeqfeats(CRef<CVariantExpression>& variant_expr) const 
{
    list<CRef<CSeq_feat>> feat_list;

    const auto& seq_var = variant_expr->GetSequence_variant();

        const auto seq_type = seq_var.GetSeqtype();

        for (const auto& variant : seq_var.GetSubvariants()) {
            const auto& simple_variant = variant->GetSimple();

            auto seq_feat = x_CreateSeqfeat(variant_expr->GetInput_expr(),
                                            variant_expr->GetReference_id(),
                                            seq_type,
                                            simple_variant);

            if (seq_feat.NotNull()) {
                feat_list.push_back(seq_feat);
            }
        }
    return feat_list;
}



CRef<CSeq_loc> CNtSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id,
        const CSimpleVariant& var_desc,
        const CSequenceVariant::TSeqtype& seq_type,
        CScope& scope,
        CVariationIrepMessageListener& listener)
{
    CRef<CSeq_loc> seq_loc;
    auto& var_type = var_desc.GetType();

    
    switch (var_type.Which()) {
        default:
            NCBI_THROW(CVariationIrepException, eUnknownVariation, "Unsupported variation type");
        case CSimpleVariant::TType::e_Na_identity:
            seq_loc = CreateSeqloc(seq_id,
                                   var_type.GetNa_identity().GetLoc(),
                                   seq_type,
                                   scope,
                                   listener);
            break;
        case CSimpleVariant::TType::e_Na_sub:
            seq_loc = CreateSeqloc(seq_id,
                                   var_type.GetNa_sub().GetLoc(),
                                   seq_type,
                                   scope,
                                   listener);
            break;
        case CSimpleVariant::TType::e_Dup:     
            seq_loc = CreateSeqloc(seq_id, 
                                   var_type.GetDup().GetLoc().GetNtloc(),
                                   seq_type, 
                                   scope,
                                   listener);
            break; 
        case CSimpleVariant::TType::e_Del:     
            seq_loc = CreateSeqloc(seq_id, 
                                   var_type.GetDel().GetLoc().GetNtloc(),
                                   seq_type, 
                                   scope,
                                   listener);
            break; 
        case CSimpleVariant::TType::e_Ins:
            seq_loc = CreateSeqloc(seq_id,
                                   var_type.GetIns().GetInt().GetNtint(),
                                   seq_type,
                                   scope,
                                   listener);
            break;
        case CSimpleVariant::TType::e_Delins:     
            seq_loc = CreateSeqloc(seq_id, 
                                   var_type.GetDelins().GetLoc().GetNtloc(),
                                   seq_type, 
                                   scope,
                                   listener);
            break; 

        case CSimpleVariant::TType::e_Inv:     
            seq_loc = CreateSeqloc(seq_id, 
                                   var_type.GetInv().GetNtint(),
                                   seq_type, 
                                   scope,
                                   listener);
            break; 

        case CSimpleVariant::TType::e_Conv:
            seq_loc = CreateSeqloc(seq_id, 
                                   var_type.GetConv().GetSrc(),
                                   seq_type, 
                                   scope,
                                   listener);
            break;
       case CSimpleVariant::TType::e_Repeat:
            seq_loc = CreateSeqloc(seq_id,
                                   var_type.GetRepeat().GetLoc().GetNtloc(),
                                   seq_type,
                                   scope,
                                   listener);
            break;
    }
    return seq_loc;
}



CRef<CSeq_loc> CNtSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id,
        const CNtLocation& nt_loc,
        const CSequenceVariant::TSeqtype& seq_type,
        CScope& scope,
        CVariationIrepMessageListener& listener)
{
    if (nt_loc.IsSite()) {
        return CreateSeqloc(seq_id,
                            nt_loc.GetSite(),
                            seq_type,
                            scope,
                            listener);
    }

    
    if (nt_loc.IsRange()) {
        return CreateSeqloc(seq_id,
                            nt_loc.GetRange(),
                            seq_type,
                            scope,
                            listener);
    }


    if (!nt_loc.IsInt()) {
        NCBI_THROW(CVariationIrepException, eInvalidLocation, "Invalid nucleotide sequence location");
    }

    return CreateSeqloc(seq_id,
                        nt_loc.GetInt(),
                        seq_type,
                        scope,
                        listener);
}


CRef<CSeq_loc> CNtSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id,
        const CNtSite& nt_site,
        const CSequenceVariant::TSeqtype& seq_type,
        CScope& scope,
        CVariationIrepMessageListener& listener)
{
    TSeqPos site_index;
    const bool know_site = x_ComputeSiteIndex(seq_id,
                                              nt_site,
                                              seq_type,
                                              scope,
                                              listener,
                                              site_index);
    auto seq_loc = Ref(new CSeq_loc());
    if (!know_site) {
        seq_loc->SetEmpty().Assign(seq_id);
        return seq_loc;
    }

    const ENa_strand strand = nt_site.GetStrand_minus() ? eNa_strand_minus : eNa_strand_plus;

    seq_loc->SetPnt().SetPoint(site_index);
    seq_loc->SetPnt().SetId().Assign(seq_id);
    seq_loc->SetPnt().SetStrand(strand);

    if (nt_site.IsSetFuzzy() &&
        nt_site.GetFuzzy()) {
        seq_loc->SetPnt().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    }

    return seq_loc;
}



CRef<CSeq_loc> CNtSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id,
        const CNtSiteRange& nt_range,
        const CSequenceVariant::TSeqtype& seq_type,
        CScope& scope,
        CVariationIrepMessageListener& listener)
{
    TSeqPos site_index;
    bool know_site = x_ComputeSiteIndex(seq_id, 
                                        nt_range,
                                        seq_type,
                                        scope,
                                        listener,
                                        site_index);

    auto seq_loc = Ref(new CSeq_loc());
    if (!know_site) {
        seq_loc->SetEmpty().Assign(seq_id);
        return seq_loc;
    }

    ENa_strand strand = eNa_strand_plus;
    if (nt_range.GetStart().GetStrand_minus()) {
        strand = eNa_strand_minus;
    }
    seq_loc->SetPnt().SetPoint(site_index);
    seq_loc->SetPnt().SetId().Assign(seq_id);
    seq_loc->SetPnt().SetStrand(strand);

    auto fuzz = x_CreateIntFuzz(seq_id,
                                nt_range,
                                seq_type,
                                scope,
                                listener);

    if (fuzz.NotNull()) {
        seq_loc->SetPnt().SetFuzz(*fuzz);  
    }    

    return seq_loc;
}


CRef<CSeq_loc> CNtSeqlocHelper::CreateSeqloc(const CSeq_id& seq_id,
        const CNtInterval& nt_int,
        const CSequenceVariant::TSeqtype& seq_type,
        CScope& scope, // cannot be NULL
        CVariationIrepMessageListener& listener) 
{

    if ( !nt_int.IsSetStart() || !nt_int.IsSetStop() ) {
        NCBI_THROW(CVariationIrepException, eInvalidInterval, "Undefined interval limits");
    }


    auto seq_loc = Ref(new CSeq_loc());
    seq_loc->SetInt().SetId().Assign(seq_id);
    
    ENa_strand strand = eNa_strand_plus;
    
    TSeqPos start_index, stop_index;


    bool know_start = x_ComputeSiteIndex(seq_id,
                                         nt_int.GetStart(),
                                         seq_type,
                                         scope,
                                         listener,
                                         start_index);

    if ((nt_int.GetStart().IsSite() && 
         nt_int.GetStart().GetSite().GetStrand_minus()) ||
        (nt_int.GetStart().IsRange() &&
         nt_int.GetStart().GetRange().GetStart().GetStrand_minus())) {
        strand = eNa_strand_minus;
    }

    bool know_stop = x_ComputeSiteIndex(seq_id, 
                                        nt_int.GetStop(),
                                        seq_type,
                                        scope,
                                        listener,
                                        stop_index);

    if ( (know_start && know_stop) && (start_index > stop_index) ) {
        string err_string = "Reversed interval limits";
        // Post a warning message and reverse interval limits
        CVariationIrepMessage msg(err_string, eDiag_Warning);
        msg.SetNtInterval(nt_int);
        listener.Post(msg);
        const auto temp = start_index;
        stop_index = start_index;
        start_index = temp;
    }

    if (know_start) {
        seq_loc->SetInt().SetFrom(start_index);
        if (nt_int.GetStart().IsSite()) {
            if(nt_int.GetStart().GetSite().IsSetFuzzy() &&
               nt_int.GetStart().GetSite().GetFuzzy()) {
                seq_loc->SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_unk);
            }
        }
        else if (nt_int.GetStart().IsRange() &&
                 !nt_int.GetStart().GetRange().GetStart().IsSetOffset() &&
                 !nt_int.GetStart().GetRange().GetStop().IsSetOffset()) {

            auto fuzz_from = CNtSeqlocHelper::x_CreateIntFuzz(seq_id,
                                                              nt_int.GetStart().GetRange(),
                                                              seq_type,
                                                              scope,
                                                              listener);
            seq_loc->SetInt().SetFuzz_from(*fuzz_from);  
        } 
        else if (nt_int.GetStart().IsRange() &&
                 nt_int.GetStart().GetRange().GetStart().GetBase().IsUnknown()) {
            seq_loc->SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_tl);
        }
    }
    else { // don't know start site
        seq_loc->SetInt().SetFrom(kInvalidSeqPos);
        seq_loc->SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_other);
    }


    if (know_stop) {
        seq_loc->SetInt().SetTo(stop_index);
        if (nt_int.GetStop().IsSite()) {
            if(nt_int.GetStop().GetSite().IsSetFuzzy() &&
               nt_int.GetStop().GetSite().GetFuzzy()) {
                seq_loc->SetInt().SetFuzz_to().SetLim(CInt_fuzz::eLim_unk);
            }
        }
        else if(nt_int.GetStop().IsRange() &&
                !nt_int.GetStop().GetRange().GetStart().IsSetOffset() &&
                !nt_int.GetStop().GetRange().GetStop().IsSetOffset()) {

            auto fuzz_to = CNtSeqlocHelper::x_CreateIntFuzz(seq_id,
                                                            nt_int.GetStop().GetRange(),
                                                            seq_type,
                                                            scope,
                                                            listener);
            seq_loc->SetInt().SetFuzz_to(*fuzz_to); 
        }
        else if (nt_int.GetStop().IsRange() &&
                 nt_int.GetStop().GetRange().GetStop().GetBase().IsUnknown()) {
            seq_loc->SetInt().SetFuzz_to().SetLim(CInt_fuzz::eLim_tr);
        } 
    } 
    else { 
        seq_loc->SetInt().SetTo(kInvalidSeqPos);
        seq_loc->SetInt().SetFuzz_to().SetLim(CInt_fuzz::eLim_other);
    }


    seq_loc->SetInt().SetStrand(strand);
    
    return seq_loc; 
}



CRef<CInt_fuzz> CNtSeqlocHelper::x_CreateIntFuzz(const CSeq_id& seq_id,
                                                 const CNtSiteRange& nt_range,
                                                 const CSequenceVariant::TSeqtype& seq_type,
                                                 CScope& scope,
                                                 CVariationIrepMessageListener& listener)
{
    auto int_fuzz = Ref(new CInt_fuzz());
 
    TSeqPos start_index = 0;
    bool know_start = x_ComputeSiteIndex(seq_id,
                                         nt_range.GetStart(),
                                         seq_type,
                                         scope,
                                         listener,
                                         start_index);   

    TSeqPos stop_index = 0;
    bool know_stop = x_ComputeSiteIndex(seq_id,
                                        nt_range.GetStop(),
                                        seq_type,
                                        scope,
                                        listener,
                                        stop_index);

    if (know_start && know_stop) {
        int_fuzz->SetRange().SetMin(start_index);
        int_fuzz->SetRange().SetMax(stop_index);
    }
    else if (!know_start && know_stop) {
        int_fuzz->SetLim(CInt_fuzz::eLim_lt);

    } 
    else if (know_start && !know_stop) {
        int_fuzz->SetLim(CInt_fuzz::eLim_gt);
    }
    else {
        int_fuzz->SetLim(CInt_fuzz::eLim_other);
    }


    return int_fuzz;
}


bool CNtSeqlocHelper::x_ComputeSiteIndex(const CSeq_id& seq_id,
                                         const CNtIntLimit& nt_limit,
                                         const CSequenceVariant::TSeqtype& seq_type,
                                         CScope& scope,
                                         CVariationIrepMessageListener& listener,
                                         TSeqPos& site_index)
{
   if (nt_limit.IsSite()) {
       return x_ComputeSiteIndex(seq_id, 
                                 nt_limit.GetSite(),
                                 seq_type,
                                 scope,
                                 listener,
                                 site_index);
   }

   return x_ComputeSiteIndex(seq_id,
                             nt_limit.GetRange(),
                             seq_type,
                             scope,
                             listener,
                             site_index);
}


bool CNtSeqlocHelper::x_ComputeSiteIndex(const CSeq_id& seq_id,
                                         const CNtSiteRange& nt_range,
                                         const CSequenceVariant::TSeqtype& seq_type,
                                         CScope& scope,
                                         CVariationIrepMessageListener& listener,
                                         TSeqPos& site_index)
{
    const auto& nt_site = nt_range.GetStart().GetBase().IsVal() ? nt_range.GetStart() : nt_range.GetStop();

    if (!nt_site.GetBase().IsVal()) {
        return false;
    }

    return x_ComputeSiteIndex(seq_id,
                              nt_site,
                              seq_type,
                              scope,
                              listener,
                              site_index);
}


bool CNtSeqlocHelper::x_ComputeSiteIndex(const CSeq_id& seq_id,
                                         const CNtSite& nt_site,
                                         const CSequenceVariant::TSeqtype& seq_type,
                                         CScope& scope,
                                         CVariationIrepMessageListener& listener,
                                         TSeqPos& site_index) 
{
    TSeqPos offset = 0;
    if (seq_type == eVariantSeqType_c) {
        auto bioseq_handle = scope.GetBioseqHandle(seq_id);
        if (!bioseq_handle) {
            NCBI_THROW(CVariationIrepException, eInvalidSeqId, "Cannot resolve bioseq from seq ID");
        }
        SAnnotSelector selector;
        selector.SetResolveTSE();
        selector.IncludeFeatType(CSeqFeatData::e_Cdregion);
        CConstRef<CSeq_feat> coding_seq;
        for(CFeat_CI it(bioseq_handle, selector); it; ++it) {
            const auto& mapped_feat = *it;
            if (mapped_feat.GetData().IsCdregion()) {
                if (coding_seq.NotNull()) {
                    NCBI_THROW(CVariationIrepException, eCDSError, "Multiple CDS features on sequence");
                }
                coding_seq = mapped_feat.GetSeq_feat();
            }
        }
        if (coding_seq.IsNull()) {
            // Not sure about the wording here
            NCBI_THROW(CVariationIrepException, eCDSError, "Could not find CDS feature");
        }
        if (nt_site.IsSetUtr() && nt_site.GetUtr().IsThree_prime()) {
            offset = coding_seq->GetLocation().GetStop(eExtreme_Biological) + 1;
        }
        else {
            offset = coding_seq->GetLocation().GetStart(eExtreme_Biological);
        }
    }

    if (nt_site.IsSetBase() && nt_site.GetBase().IsVal()) { 
        site_index = nt_site.GetBase().GetVal();
        if (nt_site.IsSetUtr() && nt_site.GetUtr().IsFive_prime()) {
            if (site_index >= offset) {
               NCBI_THROW(CVariationIrepException, eInvalidLocation, "Error deducing 5' UTR location");
            }
            site_index = offset - site_index;
        }
        else {
            if (site_index == 0 ) {
                NCBI_THROW(CVariationIrepException, eInvalidLocation, "Invalid site index: 0");
            }
            site_index = site_index + offset-1;
        }
    } 
    else { 
        return false;
    }

    return true;
}


void s_SetDeltaItemOffset(const TSeqPos length, 
                          const CDelta_item::TMultiplier multiplier, 
                          const bool fuzzy,
                          CDelta_item& delta_item)
 {
     delta_item.SetAction(CDelta_item::eAction_offset);

     if (length < kInvalidSeqPos) {
         delta_item.SetSeq().SetLiteral().SetLength(length);
         if (multiplier != 1) {
             delta_item.SetMultiplier(multiplier);
         }

         if (fuzzy) {
             delta_item.SetSeq().SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
         }
         return;
     }

     delta_item.SetSeq().SetLiteral().SetLength(0);
     const CInt_fuzz::ELim fuzz_lim = (multiplier>0) ? CInt_fuzz::eLim_gt
                                                     : CInt_fuzz::eLim_lt;

     delta_item.SetSeq().SetLiteral().SetFuzz().SetLim(fuzz_lim);
 }



CRef<CDelta_item> CHgvsNaDeltaHelper::GetIntronOffset(const CNtSite& nt_site)
{
    CRef<CDelta_item> delta_item;

    if (nt_site.IsSetOffset()) {
        TSeqPos offset_length = kInvalidSeqPos;
        TSignedSeqPos multiplier = 1;
        bool fuzzy = false;
        delta_item = Ref(new CDelta_item());
        if(nt_site.GetOffset().IsVal()) {
            auto offset = nt_site.GetOffset().GetVal();
            if (offset < 0) {
                offset = -offset;
                multiplier = -1;
            }
            offset_length = offset; 
            if (nt_site.IsSetFuzzy_offset()) {
                fuzzy = nt_site.GetFuzzy_offset();
            }
        } 
        else if (nt_site.GetOffset().IsMinus_unknown()) {
            multiplier = -1;
        }
        s_SetDeltaItemOffset(offset_length, multiplier, fuzzy, *delta_item);
       // delta_item->SetOffset(offset_length, multiplier, fuzzy);
    }

    return delta_item;
}


CRef<CDelta_item> CHgvsNaDeltaHelper::GetIntronOffset(const CNtSiteRange& nt_range)
{
    CRef<CDelta_item> delta_item;

    if (!nt_range.GetStart().IsSetOffset() &&
        !nt_range.GetStop().IsSetOffset()) {
        return delta_item;
    }

    if (nt_range.GetStart().IsSetBase() &&
        nt_range.GetStart().GetBase().IsVal() &&
        nt_range.GetStop().IsSetBase() &&
        nt_range.GetStop().GetBase().IsVal() &&
        nt_range.GetStart().GetBase().GetVal() != 
        nt_range.GetStop().GetBase().GetVal()) {
        NCBI_THROW(CVariationIrepException, eUnsupported, 
                "Base points in intronic fuzzy location must be equal");
    }

    delta_item = Ref(new CDelta_item());
    delta_item->SetAction(CDelta_item::eAction_offset);

    TSignedSeqPos offset_length = 0;
    if (nt_range.GetStart().IsSetBase() &&
        nt_range.GetStart().GetBase().IsUnknown() &&
        nt_range.GetStop().IsSetOffset() &&
        nt_range.GetStop().GetOffset().IsVal()) {
        offset_length = nt_range.GetStop().GetOffset().GetVal();
        delta_item->SetSeq().SetLiteral().SetLength(offset_length);
        return delta_item;
    }


    if (nt_range.GetStop().IsSetBase() &&
        nt_range.GetStop().GetBase().IsUnknown() &&
        nt_range.GetStart().IsSetOffset() &&
        nt_range.GetStart().GetOffset().IsVal()) {
        offset_length = nt_range.GetStart().GetOffset().GetVal();
        delta_item->SetSeq().SetLiteral().SetLength(offset_length);
        return delta_item;
    }

    TSignedSeqPos start_offset=0;
    TSignedSeqPos stop_offset=0;

    if (nt_range.GetStart().IsSetOffset() &&
        nt_range.GetStart().GetOffset().IsVal()) {
        start_offset = nt_range.GetStart().GetOffset().GetVal();
    } 

    if (nt_range.GetStop().IsSetOffset() &&
        nt_range.GetStop().GetOffset().IsVal()) {
        stop_offset = nt_range.GetStop().GetOffset().GetVal();
    }

    if (start_offset*stop_offset < 0) {
        NCBI_THROW(CVariationIrepException, eUnsupported, 
                "Offsets in intronic fuzzy location cannot differ in sign");
    }

    if (start_offset >= 0) {
        delta_item->SetSeq().SetLiteral().SetLength(start_offset);
        delta_item->SetSeq().SetLiteral().SetFuzz().SetRange().SetMin(start_offset);
    }
    else if (start_offset < 0) {
        delta_item->SetSeq().SetLiteral().SetFuzz().SetRange().SetMax(-start_offset);
    }

    if (stop_offset <= 0) {
        delta_item->SetSeq().SetLiteral().SetLength(-stop_offset);
        delta_item->SetSeq().SetLiteral().SetFuzz().SetRange().SetMin(-stop_offset);
    } else if (stop_offset > 0) {
        delta_item->SetSeq().SetLiteral().SetFuzz().SetRange().SetMax(stop_offset);
    }

    if (start_offset < 0 || stop_offset < 0) {
        delta_item->SetMultiplier(-1);
    }


    return delta_item;
}


CRef<CDelta_item> CHgvsNaDeltaHelper::GetStartIntronOffset(const CNtInterval& nt_int)
{
    if (nt_int.IsSetStart() &&
        nt_int.GetStart().IsSite()) {
        return CHgvsNaDeltaHelper::GetIntronOffset(nt_int.GetStart().GetSite());
    } 
    else if (nt_int.IsSetStart() &&
             nt_int.GetStart().IsRange()) {
        return CHgvsNaDeltaHelper::GetIntronOffset(nt_int.GetStart().GetRange());
    }
    return CRef<CDelta_item>();
}


CRef<CDelta_item> CHgvsNaDeltaHelper::GetStopIntronOffset(const CNtInterval& nt_int)
{

    // Need to extend this to handle ranges
    if (nt_int.IsSetStop() &&
        nt_int.GetStop().IsSite()) {
        return CHgvsNaDeltaHelper::GetIntronOffset(nt_int.GetStop().GetSite());
    }
    else if (nt_int.IsSetStop() &&
             nt_int.GetStop().IsRange()) {
        return CHgvsNaDeltaHelper::GetIntronOffset(nt_int.GetStop().GetRange());
    }
    return CRef<CDelta_item>();
}


CRef<CDelta_item> CHgvsNaDeltaHelper::GetStartIntronOffset(const CNtLocation& nt_loc)
{

    CRef<CDelta_item> start_offset;
    if (nt_loc.IsSite()) {
        start_offset = CHgvsNaDeltaHelper::GetIntronOffset(nt_loc.GetSite());
    } 
    else if (nt_loc.IsInt()) {
        start_offset = CHgvsNaDeltaHelper::GetStartIntronOffset(nt_loc.GetInt());
    }
    return start_offset;
}



CRef<CDelta_item> CHgvsNaDeltaHelper::GetStopIntronOffset(const CNtLocation& nt_loc)
{
    CRef<CDelta_item> stop_offset;
    if (nt_loc.IsInt()) {
        stop_offset = CHgvsNaDeltaHelper::GetStopIntronOffset(nt_loc.GetInt());
    }
    return stop_offset;
}



END_objects_SCOPE
END_NCBI_SCOPE

