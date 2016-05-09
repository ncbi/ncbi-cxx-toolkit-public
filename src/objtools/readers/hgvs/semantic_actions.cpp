#include <ncbi_pch.hpp>
#include <objtools/readers/hgvs/semantic_actions.hpp>

BEGIN_NCBI_SCOPE

template <typename T>
CRef<T> MakeResult(CRef<T> result) 
{
    return Ref(new T());
}

template<class T>
CRef<T> CreateResultIfNull(CRef<T> result)
{
    return result.IsNull() ? Ref(new T()) : result;    
}

void TagAsChimera(CRef<CSequenceVariant>& seq_var)
{
    seq_var->SetComplex().SetChimera();
}

void TagAsMosaic(CRef<CSequenceVariant>& seq_var)
{
    seq_var->SetComplex().SetMosaic();
}

void AssignRefSeqIdentifier(const string& identifier, CRef<CVariantExpression>& result)
{
    result = CreateResultIfNull(result);
    result->SetReference_id(identifier.substr(0,identifier.size()-1));
}


void AssignSequenceVariant(CRef<CSequenceVariant>& variant, CRef<CVariantExpression>& result)
{
    result = CreateResultIfNull(result);
    //result->SetSeqvars().push_back(variant);
    result->SetSequence_variant(variant.GetNCObject());
}


void AppendToLocalVariantSeq(CRef<CSimpleVariant>& var_desc, CRef<CSimpleVariantSeq>& result)
{
    result = CreateResultIfNull(result);
    result->SetVariants().push_back(var_desc);
}


void MakeLocalVariantSeqFuzzy(CRef<CSimpleVariantSeq>& seq)
{
    seq->SetFuzzy(true);
}


void AssignMissense(CRef<CAaSite>& initial, 
                    const CProteinSub::TFinal& final, 
                    CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    auto& sub = result->SetType().SetProt_sub();

    sub.SetType(CProteinSub::eType_missense);
    sub.SetInitial(*initial);
    sub.SetFinal(final);
}


void AssignNonsense(CRef<CAaSite>& initial, CRef<CSimpleVariant>& result)
{
	result = CreateResultIfNull(result);

    result->SetType().SetProt_sub().SetType(CProteinSub::eType_nonsense);
	result->SetType().SetProt_sub().SetInitial(*initial);
}


void AssignUnknownSub(CRef<CAaSite>& initial, CRef<CSimpleVariant>& result)
{
	result = CreateResultIfNull(result);

    result->SetType().SetProt_sub().SetType(CProteinSub::eType_unknown);
	result->SetType().SetProt_sub().SetInitial(*initial);
}


void AssignAaDup(CRef<CAaLocation>& aa_loc, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetDup().SetLoc().SetAaloc(*aa_loc);    
}


void AssignAaDel(CRef<CAaLocation>& aa_loc, CRef<CSimpleVariant>& result)
{   
    result = CreateResultIfNull(result);
    result->SetType().SetDel().SetLoc().SetAaloc(*aa_loc);
}


void AssignNtermExtension(CRef<CAaSite>& initial_start_site, CRef<CCount>& new_start_site, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetProt_ext().SetNterm_ext().SetNewStart(*new_start_site);

}

void AssignNtermExtension(CRef<CAaSite>& initial_start_site, const string& new_aa, CRef<CCount>& new_start_site, CRef<CSimpleVariant>& result)
{
    AssignNtermExtension(initial_start_site, new_start_site, result);
    result->SetType().SetProt_ext().SetNterm_ext().SetNewAa(new_aa);    
} 


void AssignCtermExtension(const string& reference_stop, const string& aa, CRef<CCount>& length, CRef<CSimpleVariant>& result) 
{

    const auto index = NStr::StringToNumeric<TSeqPos>(reference_stop);    
    result = CreateResultIfNull(result);

    auto& cterm_ext = result->SetType().SetProt_ext().SetCterm_ext();
    cterm_ext.SetRefStop(index);
    cterm_ext.SetNewAa(aa);
    cterm_ext.SetLength(*length);
}


void AssignAaIntervalLocation(CRef<CAaInterval>& aa_interval, CRef<CAaLocation>& result) 
{
    result = CreateResultIfNull(result);
    result->SetInt(*aa_interval);
}


void AssignAaSiteLocation(CRef<CAaSite>& aa_site, CRef<CAaLocation>& result)
{
    result = CreateResultIfNull(result);
    result->SetSite(*aa_site);
}


void AssignAaInterval(CRef<CAaSite>& start, CRef<CAaSite>& stop, CRef<CAaInterval>& result)
{
    result = CreateResultIfNull(result);
    result->SetStart(*start);
    result->SetStop(*stop);
}


void AssignAaSite(const string& aa, const string& pos, CRef<CAaSite>& result) 
{
    result = CreateResultIfNull(result);
    auto index = NStr::StringToNumeric<CAaSite::TIndex>(pos);
    // Try ... catch here?

    result->SetAa(aa);
    result->SetIndex(index);
}


void AssignCount(const string& count, CRef<CCount>& result) 
{
    result = CreateResultIfNull(result);

    if ( count == "?" ) {
        result->SetUnknown();
    }
    else {
        const auto val = NStr::StringToNumeric<CCount::TVal>(count);
        result->SetVal(val);
    }
}


void AssignFuzzyCount(const string& count, CRef<CCount>& result)
{
    result = CreateResultIfNull(result);
    const auto len = count.size();
    const auto c = NStr::StringToNumeric<CCount::TVal>(count.substr(1,len-2));
    result->SetFuzzy_val(c);
}


void AssignCountRange(const string& start, const string& stop, CRef<CCount>& result)
{
    result = CreateResultIfNull(result);
    if ( start == "?" ) {
        result->SetRange().SetStart().SetUnknown(); 
    } 
    else {
        const auto start_val = NStr::StringToNumeric<CCount::TVal>(start);
        result->SetRange().SetStart().SetVal(start_val);
    }

    if ( stop == "?" ) {
        result->SetRange().SetStop().SetUnknown(); 
    } 
    else {
        const auto stop_val = NStr::StringToNumeric<CCount::TVal>(stop);
        result->SetRange().SetStop().SetVal(stop_val);
    }
}


void AssignMinCount(const string& min_count, CRef<CCount>& result)
{
    result = CreateResultIfNull(result);
    const auto min_val = NStr::StringToNumeric<CCount::TVal>(min_count);
    result->SetRange().SetStart().SetVal(min_val);
    result->SetRange().SetStop().SetUnknown();
}


void AssignMaxCount(const string& max_count, CRef<CCount>& result)
{
    result = CreateResultIfNull(result);
    result->SetRange().SetStart().SetUnknown();
    const auto max_val = NStr::StringToNumeric<CCount::TVal>(max_count);
    result->SetRange().SetStop().SetVal(max_val);
}


void AssignAaSSR(CRef<CAaLocation>& aa_loc, CRef<CCount>& count, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);

    auto& ssr = result->SetType().SetRepeat();

    ssr.SetLoc().SetAaloc(*aa_loc);
	ssr.SetCount(*count);
}


void AssignAaInsertion(CRef<CAaInterval>& aa_interval, 
                       const CInsertion::TSeqinfo::TRaw_seq& raw_seq, 
                       CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetIns().SetInt().SetAaint(*aa_interval);
    result->SetType().SetIns().SetSeqinfo().SetRaw_seq(raw_seq);
}


void AssignAaInsertionSize(CRef<CAaInterval>& aa_interval, CRef<CCount>& seq_size, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);

    auto& insertion = result->SetType().SetIns();
    
   	insertion.SetInt().SetAaint(*aa_interval);
    insertion.SetSeqinfo().SetCount(*seq_size);
}


void AssignFrameshift(CRef<CAaSite>& aa_site, CRef<CSimpleVariant>& result)
{
	result = CreateResultIfNull(result);
	result->SetType().SetFrameshift().SetAasite(*aa_site);
}


void AssignFrameshiftFromStopcodon(CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetFrameshift().SetStopcodon();
}


void AssignAaDelins(CRef<CAaLocation>& aa_loc, 
                    const CInsertion::TSeqinfo::TRaw_seq& raw_seq, 
                    CRef<CSimpleVariant>& result) 
{
    result = CreateResultIfNull(result);

    auto& delins = result->SetType().SetDelins();

    delins.SetLoc().SetAaloc(*aa_loc);
    delins.SetInserted_seq_info().SetRaw_seq(raw_seq);
}


void AssignAaDelinsSize(CRef<CAaLocation>& aa_loc, CRef<CCount> seq_size, CRef<CSimpleVariant>& result) 
{
    result = CreateResultIfNull(result);

    auto& delins = result->SetType().SetDelins();
 
    delins.SetLoc().SetAaloc(*aa_loc);
    delins.SetInserted_seq_info().SetCount(*seq_size);
}


template <typename T>
void AssignFuzzy(CRef<T>& input, CRef<T>& result)
{
    result = input;
    result->SetFuzzy();
}


void AssignFuzzyLocalVariation(CRef<CSimpleVariant>& input, CRef<CSimpleVariant>& result)
{
    AssignFuzzy(input, result);
}


void AssignFuzzyLocalVariantSeq(CRef<CSimpleVariantSeq>& input, CRef<CSimpleVariantSeq>& result)
{
    AssignFuzzy(input, result);
}


void AssignSingleLocalVariation(CRef<CSimpleVariant>& simple_var, CRef<CSequenceVariant>& result)
{
    result = CreateResultIfNull(result);
    CRef<CVariant> variant = Ref(new CVariant());
    variant->SetSimple(*simple_var);
    result->SetSubvariants().push_back(variant);
}


void AssignUnknownChromosomeVariant(CRef<CSimpleVariantSeq>& variant_seq, CRef<CSequenceVariant>& result)
{
    result = CreateResultIfNull(result);
//    result->SetVariants().SetUnknownChromosomeVariant(*variant_seq);
}


void AssignSecondChromosomeVariant(CRef<CSimpleVariantSeq>& variant_seq, CRef<CSequenceVariant>& result)
{
    result = CreateResultIfNull(result);
//    result->SetVariants().SetKnownChromosomeVariant().SetChrom2_variant().SetType().SetSimple_variant_seq(*variant_seq);
}


void AssignSecondChromosomeSpecialVariant(const ESpecialVariant& special_variant, CRef<CSequenceVariant>& result)
{
    result = CreateResultIfNull(result);
//    result->SetVariants().SetKnownChromosomeVariant().SetChrom2_variant().SetType().SetSpecial_variant(special_variant);
}


void AssignChromosomeVariant(CRef<CSimpleVariantSeq>& variant_seq, CRef<CSequenceVariant>& result)
{
    result = CreateResultIfNull(result);
//    result->SetVariants().SetKnownChromosomeVariant().SetChrom1_variant().SetSimple_variant_seq(*variant_seq);
}


void AssignSpecialVariant(const ESpecialVariant& special_variant, CRef<CSequenceVariant>& result)
{
    result = CreateResultIfNull(result);
//    result->SetVariants().SetKnownChromosomeVariant().SetChrom1_variant().SetSpecial_variant(special_variant);
}


void AssignSequenceType(CRef<CSequenceVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetSeqtype(eVariantSeqType_p);
}


// Nucleotide-specific functions
void AssignSimpleNtSite(const string& site_index, CRef<CNtSite>& result)
{
    result = CreateResultIfNull(result);
    if (site_index == "?") {
        result->SetBase().SetUnknown();
        return;
    }
    const auto base_val = NStr::StringToNumeric<CNtSite::TBase::TVal>(site_index);
    result->SetBase().SetVal(base_val);
}


void AssignFuzzyNtSite(CRef<CNtSite>& center_site, CRef<CNtSite>& result)
{
    result = center_site;
    result->ResetFuzzy();
}


void AssignFuzzySimpleNtSite(const string& site_index, CRef<CNtSite>& result)
{
    auto center_site = site_index; 

    NStr::ReplaceInPlace(center_site, "(", "");
    NStr::ReplaceInPlace(center_site, ")", "");

    AssignSimpleNtSite(center_site, result);
    result->SetFuzzy();
}


void AssignIntronSite(const string& base, const string& offset, CRef<CNtSite>& result)
{
    result = CreateResultIfNull(result);

    if (base == "?") {
        result->SetBase().SetUnknown();
    }
    else { 
        const auto base_val = NStr::StringToNumeric<CNtSite::TBase::TVal>(base);
        result->SetBase().SetVal(base_val);
    }

    if (offset == "+?") {
        result->SetOffset().SetPlus_unknown();
        return;
    }  

    if (offset == "-?") {
        result->SetOffset().SetMinus_unknown();
        return;
    }


    if (base.front() == '(' && offset.back() == ')') {
        result->SetFuzzy();
    }
    else if (offset.size() >= 2 && 
             offset[1] == '(' 
             && offset.back() == ')') {
        result->SetFuzzy_offset();
    }

    auto offset_value = offset;
    NStr::ReplaceInPlace(offset_value, "(", "");
    NStr::ReplaceInPlace(offset_value, ")", "");
    result->SetOffset().SetVal(NStr::StringToNumeric<CNtSite::TOffset::TVal>(offset_value));
}


void Assign5primeUTRSite(CRef<CNtSite>& nt_site, CRef<CNtSite>& result)
{
    result = Ref(nt_site.GetPointer());
    result->SetUtr().SetFive_prime();
}


void Assign3primeUTRSite(CRef<CNtSite>& nt_site, CRef<CNtSite>& result)
{
    result = Ref(nt_site.GetPointer());
    result->SetUtr().SetThree_prime();
}


void AssignNtSiteRange(CRef<CNtSite>& start, CRef<CNtSite>& stop, CRef<CNtLocation>& result)
{
    result = CreateResultIfNull(result);
    result->SetRange().SetStart(*start);
    result->SetRange().SetStop(*stop);
}

void AssignNtSite(CRef<CNtSite>& nt_site, CRef<CNtLocation>& result)
{
    result = CreateResultIfNull(result);
    result->SetSite(*nt_site);
}

void AssignNtInterval(CRef<CNtLocation>& start, CRef<CNtLocation>& stop, CRef<CNtLocation>& result)
{
    result = CreateResultIfNull(result);
    // TODO - Need to check that the input arguments 
    // are nucleotide sites
    if (start->IsSite()) {
        result->SetInt().SetStart().SetSite(start->SetSite()); 
    } 
    else if (start->IsRange()) {
        result->SetInt().SetStart().SetRange(start->SetRange());
    }
    if (stop->IsSite()) {
        result->SetInt().SetStop().SetSite(stop->SetSite()); 
    }
    else if (stop->IsRange()) {
        result->SetInt().SetStop().SetRange(stop->SetRange());
    }
}

void AssignNtSiteLocation(CRef<CNtSite>& nt_site, CRef<CNtLocation>& result)
{
    result = CreateResultIfNull(result);
    result->SetSite(*nt_site);
}

void AssignNtIntervalLocation(CRef<CNtInterval>& nt_int, CRef<CNtLocation>& result)
{
    result = CreateResultIfNull(result);
    result->SetInt(*nt_int);
}


void s_SetSequenceInfo(CRef<CNtLocation>& nt_loc, const string& identifier, const EVariantSeqType& seq_type)
{
    if (nt_loc->IsSite()) {
        bool is_minus_strand = (identifier[0] == 'o');
        const string seq_id = is_minus_strand ? identifier.substr(1) : identifier;
        nt_loc->SetSite().SetSeqid(seq_id);
        nt_loc->SetSite().SetSeqtype(seq_type);
        nt_loc->SetSite().SetStrand_minus(is_minus_strand);
        // opposite orientation
    } 
    else if (nt_loc->IsRange()) {
        auto start_loc = Ref(new CNtLocation());
        start_loc->SetSite(nt_loc->SetRange().SetStart());
        s_SetSequenceInfo(start_loc, identifier, seq_type);
         
        auto stop_loc = Ref(new CNtLocation());
        stop_loc->SetSite(nt_loc->SetRange().SetStop());
        s_SetSequenceInfo(stop_loc, identifier, seq_type);
    } 
    else if (nt_loc->IsInt()) {
        auto start_loc = Ref(new CNtLocation());
        if (nt_loc->GetInt().GetStart().IsSite()) {
            start_loc->SetSite(nt_loc->SetInt().SetStart().SetSite());
        }
        else {
            start_loc->SetRange(nt_loc->SetInt().SetStart().SetRange());
        }
        s_SetSequenceInfo(start_loc, identifier, seq_type);

        auto stop_loc = Ref(new CNtLocation());
        if (nt_loc->GetInt().GetStop().IsSite()) {
            stop_loc->SetSite(nt_loc->SetInt().SetStop().SetSite());
        }
        else {
            stop_loc->SetRange(nt_loc->SetInt().SetStart().SetRange());
        }
        s_SetSequenceInfo(stop_loc, identifier, seq_type);
    }

    return;
}


EVariantSeqType s_GetSeqType(const string& type_string) 
{
    if (type_string == "g.") {
        return eVariantSeqType_g;
    }

    if (type_string == "c.") {
        return eVariantSeqType_c;
    }

    if (type_string == "p.") {
        return eVariantSeqType_p;
    }

    if (type_string == "r.") {
        return eVariantSeqType_r;
    }

    if (type_string == "m.") {
        return eVariantSeqType_m;
    }

    if (type_string == "n.") {
        return eVariantSeqType_n;
    }

    return eVariantSeqType_u; // unknown type
}


void AssignNtRemoteLocation(const string& identifier, const string& type_string, CRef<CNtLocation>& nt_loc, CRef<CNtLocation>& result) 
{
    result = Ref(nt_loc.GetPointer());
    const string seq_id = identifier.substr(0,identifier.size()-1);
    const EVariantSeqType seq_type = s_GetSeqType(type_string);

    s_SetSequenceInfo(result, seq_id, seq_type);
}


void AssignNtRemoteLocation(const string& identifier, CRef<CNtLocation>& nt_loc, CRef<CNtLocation>& result)
{
    AssignNtRemoteLocation(identifier, "g.", nt_loc, result);
}

void AssignNtSSR(CRef<CNtLocation>& nt_loc, CRef<CCount>& count, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetRepeat().SetLoc().SetNtloc(*nt_loc);
    result->SetType().SetRepeat().SetCount(*count);
}

void AssignNtSSR(CRef<CNtLocation>& nt_loc, const CRepeat::TRaw_seq& raw_seq, CRef<CCount>& count, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetRepeat().SetLoc().SetNtloc(*nt_loc);
    result->SetType().SetRepeat().SetCount(*count);
    if (raw_seq.empty()) {
        return;
    }
    result->SetType().SetRepeat().SetRaw_seq(raw_seq);
}

void AssignNtInv(CRef<CNtLocation>& nt_loc, const CInversion::TRaw_seq& raw_seq, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetInv().SetNtint(nt_loc->SetInt());
        
    if (raw_seq.empty()) {
        return;
    }
    result->SetType().SetInv().SetRaw_seq(raw_seq);
}


void AssignNtInv(CRef<CNtLocation>& nt_loc, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetInv().SetNtint(nt_loc->SetInt());
}


void AssignNtInvSize(CRef<CNtLocation>& nt_loc,  string size,  CRef<CSimpleVariant>& result)
{
    AssignNtInv(nt_loc, result);
    result->SetType().SetInv().SetSize(NStr::StringToNumeric<CInversion::TSize>(size));
}


void AssignNtConversion(CRef<CNtLocation>& src_int, CRef<CNtLocation>& dst_int, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetConv().SetSrc(src_int->SetInt());
    result->SetType().SetConv().SetDst(dst_int->SetInt());
}

void AssignNtConversion(CRef<CNtLocation>& src_int, const string& seq_id, CRef<CNtLocation>& dst_int, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetConv().SetSrc(src_int->SetInt());
    result->SetType().SetConv().SetDst(dst_int->SetInt());
}


void AssignNtInsertion(CRef<CNtLocation>& nt_loc, const CInsertion::TSeqinfo::TRaw_seq& raw_seq, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetIns().SetInt().SetNtint(nt_loc->SetInt());
   
    if (raw_seq.empty()) {
        return;
    }
    result->SetType().SetIns().SetSeqinfo().SetRaw_seq(raw_seq);
}


void AssignNtDeletion(CRef<CNtLocation>& nt_loc, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetDel().SetLoc().SetNtloc(*nt_loc);
}


void AssignNtDeletion(CRef<CNtLocation>& nt_loc, const CDeletion::TRaw_seq& raw_seq, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetDel().SetLoc().SetNtloc(*nt_loc);
    if (raw_seq.empty()) {
        return;
    }
    result->SetType().SetDel().SetRaw_seq(raw_seq);
}


void AssignNtDelins(CRef<CNtLocation>& nt_loc, const CInsertion::TSeqinfo::TRaw_seq& inserted_seq, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetDelins().SetLoc().SetNtloc(*nt_loc);
    if (inserted_seq.empty()) {
        return;
    }
    result->SetType().SetDelins().SetInserted_seq_info().SetRaw_seq(inserted_seq);
}


void AssignNtDelins(CRef<CNtLocation>& nt_loc, 
                    const CDeletion::TRaw_seq& deleted_seq, 
                    const CInsertion::TSeqinfo::TRaw_seq& inserted_seq,
                    CRef<CSimpleVariant>& result) 
{
    AssignNtDelins(nt_loc, inserted_seq, result);

    if (deleted_seq.empty()) {
        return;
    }
    result->SetType().SetDelins().SetDeleted_raw_seq(deleted_seq);
}


void AssignNtDup(CRef<CNtLocation>& nt_loc, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetDup().SetLoc().SetNtloc(*nt_loc);
}


void AssignNtDup(CRef<CNtLocation>& nt_loc, const CDuplication::TRaw_seq& raw_seq, CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetDup().SetLoc().SetNtloc(*nt_loc);
    if (raw_seq.empty()) {
        return;
    }
    result->SetType().SetDup().SetRaw_seq(raw_seq);
}


void AssignNtSub(CRef<CNtLocation>& nt_loc, 
                 const CNaSub::TInitial& initial_nt,
                 const CNaSub::TFinal& final_nt,
                 CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetNa_sub().SetLoc(*nt_loc);
    result->SetType().SetNa_sub().SetInitial(initial_nt);
    result->SetType().SetNa_sub().SetFinal(final_nt);
}


void AssignNtIdentity(CRef<CNtLocation>& nt_loc,
                      const CNaIdentity::TNucleotide& nucleotide,
                      CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result); 
    result->SetType().SetNa_identity().SetLoc(*nt_loc);
    result->SetType().SetNa_identity().SetNucleotide(nucleotide);
}


void AssignNtIdentity(CRef<CNtLocation>& nt_loc,
                      CRef<CSimpleVariant>& result)
{
    result = CreateResultIfNull(result);
    result->SetType().SetNa_identity().SetLoc(*nt_loc);
}


void AssignSequenceType(const string& type_string, CRef<CSequenceVariant>& result)
{
    result = CreateResultIfNull(result);
    auto seq_type = s_GetSeqType(type_string);
    result->SetSeqtype(seq_type);
}

END_NCBI_SCOPE


