#include <ncbi_pch.hpp>
#include <objtools/readers/hgvs/hgvs_validator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

const char* CHgvsVariantException::GetErrCodeString(void) const
{
    switch( GetErrCode() ) {
        case eParseError:              return "eParserError";
        case eInvalidNALocation:       return "eInvalidNALocation";
        case eInvalidProteinLocation:  return "eInvalidProteinLocation";
        case eInvalidNucleotideCode:   return "eInvalidNucleotideCode";
        case eInvalidAminoAcidCode:    return "eInvalidAminoAcidCode";
        case eInvalidCount:            return "eInvalidCount";
        case eInvalidVariantType:      return "eInvalidVariantType";
        case eMissingVariantData:      return "eMissingVariantData";
        case eUnsupported:             return "eUnsupported";

        default:                       return CException::GetErrCodeString();
    }
}


void CHgvsVariantValidator::Validate(const CVariantExpression& hgvs_variant) 
{
    // Need to fix this

    const auto& first_subvariant = hgvs_variant.GetSequence_variant().GetSubvariants().front();

    if (!first_subvariant->IsSimple()) {
        return;
    }

    const auto& simple_variant = hgvs_variant.GetSequence_variant().GetSubvariants().front()->GetSimple();

    auto seq_type = hgvs_variant.GetSequence_variant().GetSeqtype();

    x_ValidateSimpleVariant(simple_variant, seq_type);
}


void CHgvsVariantValidator::x_ValidateSimpleVariant(const CSimpleVariant& simple_variant,
                                                    const CSequenceVariant::TSeqtype& seq_type)
{
    if (seq_type == eVariantSeqType_p) {
        x_ValidateSimpleProteinVariant(simple_variant);
    } else {
        x_ValidateSimpleNAVariant(simple_variant, seq_type);  
    }
}


void CHgvsVariantValidator::x_ValidateSimpleNAVariant(const CSimpleVariant& simple_variant,
                                                      const CSequenceVariant::TSeqtype& seq_type)
{

    if (!simple_variant.IsSetType()) {
        NCBI_THROW(CHgvsVariantException, eInvalidVariantType, "Unknown NA variant type");  
    }

    auto& var_type = simple_variant.GetType();
    bool is_rna = false;

    switch (var_type.Which()) {
        default: 
            NCBI_THROW(CHgvsVariantException, eInvalidVariantType, "Unrecognized NA variant type");

        case CSimpleVariant::TType::e_Na_identity:
            x_ValidateNtLocation(var_type.GetNa_identity().GetLoc(), seq_type);
            if (var_type.GetNa_identity().IsSetNucleotide()) {
                x_ValidateNtCode(var_type.GetNa_identity().GetNucleotide(), is_rna);
            }
            break;
        case CSimpleVariant::TType::e_Na_sub:
            x_ValidateNtLocation(var_type.GetNa_sub().GetLoc(), seq_type);
            if (!var_type.GetNa_sub().IsSetInitial()) {
                NCBI_THROW(CHgvsVariantException, eMissingVariantData, "Initial nucleotide in NA substitution not specified");
            }
            x_ValidateNtCode(var_type.GetNa_sub().GetInitial(), is_rna, true);
            if (!var_type.GetNa_sub().IsSetFinal()) {
                NCBI_THROW(CHgvsVariantException, eMissingVariantData, "Final nucleotide in NA substitution not specified");
            }
            x_ValidateNtCode(var_type.GetNa_sub().GetFinal(), is_rna, true);
            break; 
        case CSimpleVariant::TType::e_Dup:
            x_ValidateNtLocation(var_type.GetDup().GetLoc().GetNtloc(), seq_type);
            if (var_type.GetDup().IsSetRaw_seq()) {
                x_ValidateNtCode(var_type.GetDup().GetRaw_seq(), is_rna, true);
            }
            break;
        case CSimpleVariant::TType::e_Del:
            x_ValidateNtLocation(var_type.GetDel().GetLoc().GetNtloc(), seq_type);
            if (var_type.GetDel().IsSetRaw_seq()) {
                x_ValidateNtCode(var_type.GetDel().GetRaw_seq(), is_rna, true);
            }
            break;
        case CSimpleVariant::TType::e_Ins:
            x_ValidateNtLocation(var_type.GetIns().GetInt().GetNtint(), seq_type);
            if (!var_type.GetIns().IsSetSeqinfo()) {
                NCBI_THROW(CHgvsVariantException, eMissingVariantData, "Inserted sequence info is not specified");
            }
            if (var_type.GetIns().GetSeqinfo().IsRaw_seq()) {
                x_ValidateNtCode(var_type.GetIns().GetSeqinfo().GetRaw_seq(), is_rna, true);
            } else if(var_type.GetIns().GetSeqinfo().IsCount()) {
                x_ValidateCount(var_type.GetIns().GetSeqinfo().GetCount());
            }
            break;
        case CSimpleVariant::TType::e_Delins:
            x_ValidateNtLocation(var_type.GetDelins().GetLoc().GetNtloc(), seq_type);
            if (var_type.GetDelins().IsSetDeleted_raw_seq()) {
                x_ValidateNtCode(var_type.GetDelins().GetDeleted_raw_seq(), is_rna, true);
            }
            if (!var_type.GetDelins().IsSetInserted_seq_info()) {
                NCBI_THROW(CHgvsVariantException, eMissingVariantData, "Inserted sequence info is not specified");
            }
            {
                const auto& inserted_seq_info = var_type.GetDelins().GetInserted_seq_info();
                if (inserted_seq_info.IsRaw_seq()) {
                    x_ValidateNtCode(inserted_seq_info.GetRaw_seq(), is_rna, true);
                } else if (inserted_seq_info.IsCount()) {
                    x_ValidateCount(inserted_seq_info.GetCount());
                } // Need to check other info types
            }
            break;
        case CSimpleVariant::TType::e_Inv:
            x_ValidateNtLocation(var_type.GetInv().GetNtint(), seq_type);
            if (var_type.GetInv().IsSetRaw_seq()) {
                x_ValidateNtCode(var_type.GetInv().GetRaw_seq(), is_rna, true);
            }
            break;
        case CSimpleVariant::TType::e_Conv:
            x_ValidateNtLocation(var_type.GetConv().GetLoc(), seq_type); 
            x_ValidateNtLocation(var_type.GetConv().GetOrigin(), seq_type);
            break;
        case CSimpleVariant::TType::e_Repeat:
            x_ValidateNtLocation(var_type.GetRepeat().GetLoc().GetNtloc(), seq_type);
            if (var_type.GetRepeat().IsSetRaw_seq()) { // Need to add a warning here if location is an interval
                x_ValidateNtCode(var_type.GetRepeat().GetRaw_seq(), is_rna, true);
            }
            x_ValidateCount(var_type.GetRepeat().GetCount());
            break;
    } 
}


void CHgvsVariantValidator::x_ValidateSimpleProteinVariant(const CSimpleVariant& simple_variant)
{
    if (!simple_variant.IsSetType()) {
        NCBI_THROW(CHgvsVariantException, eInvalidVariantType, "Unknown protein variant type");
    }

    auto& var_type = simple_variant.GetType();

    switch (var_type.Which()) {
        default: 
            NCBI_THROW(CHgvsVariantException, eInvalidVariantType, "Unrecognized protein variant type");
        case CSimpleVariant::TType::e_Prot_sub:
            x_ValidateProteinLocation(var_type.GetProt_sub().GetInitial());
            if (var_type.GetProt_sub().GetType() == CProteinSub::eType_missense) {
                x_ValidateAaCode(var_type.GetProt_sub().GetFinal());
            }
            break;
        case CSimpleVariant::TType::e_Del:
            x_ValidateProteinLocation(var_type.GetDel().GetLoc().GetAaloc());
            if (var_type.GetDel().IsSetRaw_seq()) {
                x_ValidateAaCode(var_type.GetDel().GetRaw_seq(), true);
            }
            break;
        case CSimpleVariant::TType::e_Dup:
            x_ValidateProteinLocation(var_type.GetDup().GetLoc().GetAaloc());
            if (var_type.GetDup().IsSetRaw_seq()) {
                x_ValidateAaCode(var_type.GetDel().GetRaw_seq(), true);
            }
            break;
        case CSimpleVariant::TType::e_Ins:
            x_ValidateProteinLocation(var_type.GetIns().GetInt().GetAaint());
            if (!var_type.GetIns().IsSetSeqinfo()) {
                NCBI_THROW(CHgvsVariantException, eMissingVariantData, "Inserted sequence is unspecified");
            } 
            if (var_type.GetIns().GetSeqinfo().IsRaw_seq()) {
                x_ValidateAaCode(var_type.GetIns().GetSeqinfo().GetRaw_seq(), true);
            }
            break;
        case CSimpleVariant::TType::e_Delins:
            x_ValidateProteinLocation(var_type.GetDelins().GetLoc().GetAaloc());
            if (var_type.GetDelins().IsSetDeleted_raw_seq()) {
                x_ValidateAaCode(var_type.GetDelins().GetDeleted_raw_seq(), true);
            }
            if (!var_type.GetDelins().IsSetInserted_seq_info()) {
                NCBI_THROW(CHgvsVariantException, eMissingVariantData, "Inserted sequence is unspecified");
            }
            if (var_type.GetDelins().GetInserted_seq_info().IsRaw_seq()) {
                x_ValidateAaCode(var_type.GetDelins().GetInserted_seq_info().GetRaw_seq(), true, true);
            }
            // Need to look at other types of Inserted_seq_info
            break;
        case CSimpleVariant::TType::e_Repeat:
            x_ValidateProteinLocation(var_type.GetRepeat().GetLoc().GetAaloc());
            x_ValidateCount(var_type.GetRepeat().GetCount());
            break;
        case CSimpleVariant::TType::e_Frameshift:
            if ( var_type.GetFrameshift().IsStopcodon() ) {
                NCBI_THROW(CHgvsVariantException, 
                           eInvalidVariantType, 
                           "Frameshift beginning at stop codon is not yet supported"); 
                // No reason why this shouldn't be supported 
            } else {
                x_ValidateProteinLocation(var_type.GetFrameshift().GetAasite());
            }
            break;
        case CSimpleVariant::TType::e_Prot_ext:
            break;
    }
}



void CHgvsVariantValidator::x_CheckNoOffsets(const CNtLocation& nt_loc) 
{
    string error_msg = "Intronic sites not supported";

    if (nt_loc.IsSite() &&
        nt_loc.GetSite().IsSetOffset()) {
        return;
    }

    if (nt_loc.IsRange() &&
        !((nt_loc.GetRange().IsSetStart() &&
           nt_loc.GetRange().GetStart().IsSetOffset())) &&
        !((nt_loc.GetRange().IsSetStop() &&
           nt_loc.GetRange().GetStop().IsSetOffset()))) {
        return;
    }
     

    if (nt_loc.IsInt()) { // Need to finish this!
        std::cout << "Need to finish\n";
        return;
    }
            
    NCBI_THROW(CHgvsVariantException, eUnsupported, error_msg);
}



void CHgvsVariantValidator::x_ValidateNtLocation(const CNtLocation& nt_loc,
                                                 const CSequenceVariant::TSeqtype& seq_type) 
{
    if (nt_loc.IsSite()) {
        x_ValidateNtLocation(nt_loc.GetSite(), seq_type);
        return;
    }

    if (nt_loc.IsRange()) {
        x_ValidateNtLocation(nt_loc.GetRange(), seq_type);
    }

    // Must be interval
    x_ValidateNtLocation(nt_loc.GetInt(), seq_type);
}



void CHgvsVariantValidator::x_ValidateNtLocation(const CNtInterval& nt_int,
                                                 const CSequenceVariant::TSeqtype& seq_type)
{
    if (!nt_int.IsSetStart()) {
        string error_msg = "Genomic interval start site unspecified";
        NCBI_THROW(CHgvsVariantException, eInvalidNALocation, error_msg);
    }

    if (nt_int.GetStart().IsSite()) {
        x_ValidateNtLocation(nt_int.GetStart().GetSite(), seq_type);
    } else if (nt_int.GetStart().IsRange()) {
        x_ValidateNtLocation(nt_int.GetStart().GetRange(), seq_type);
    }

    if (!nt_int.IsSetStop()) {
        string error_msg = "Genomic interval stop site unspecified";
        NCBI_THROW(CHgvsVariantException, eInvalidNALocation, error_msg);
    }

    if (nt_int.GetStop().IsSite()) {
        x_ValidateNtLocation(nt_int.GetStop().GetSite(), seq_type);
    } else if (nt_int.GetStop().IsRange()) {
        x_ValidateNtLocation(nt_int.GetStop().GetRange(), seq_type);
    }
}


void CHgvsVariantValidator::x_ValidateNtLocation(const CNtSiteRange& nt_range,
                                                 const CSequenceVariant::TSeqtype& seq_type)
{
    if (!nt_range.IsSetStart()) {
        string error_msg = "Genomic range start site unspecified";
        NCBI_THROW(CHgvsVariantException, eInvalidNALocation, error_msg);
    }
    x_ValidateNtLocation(nt_range.GetStart(), seq_type);

    if (!nt_range.IsSetStop()) {
        string error_msg = "Genomic range stop site unspecified";
        NCBI_THROW(CHgvsVariantException, eInvalidNALocation, error_msg);
    }
    x_ValidateNtLocation(nt_range.GetStop(), seq_type);
}


void CHgvsVariantValidator::x_ValidateNtLocation(const CNtSite & nt_site,
                                                 const CSequenceVariant::TSeqtype& seq_type)
{
    if (seq_type == eVariantSeqType_c ||
        seq_type == eVariantSeqType_r) {
        return;
    }

    string error_msg = "";
    if (nt_site.IsSetOffset()) {
        error_msg = "Genomic site descriptor cannot contain offset";
    } else if (nt_site.IsSetUtr() &&
               nt_site.IsSetBase() &&
               nt_site.GetBase().IsVal()) {
        string site;
        if (nt_site.GetUtr().IsFive_prime()) {
            site = "-" + NStr::IntToString(nt_site.GetBase().GetVal());
        } else { // 3' UTR
            site = "*" + NStr::IntToString(nt_site.GetBase().GetVal());
        }
        error_msg = "Invalid genomic site: " + site;
    }
              
    if (!error_msg.empty()) {
        NCBI_THROW(CHgvsVariantException, eInvalidNALocation, error_msg);
    }
}


void CHgvsVariantValidator::x_ValidateProteinLocation(const CAaLocation& aa_loc)
{
    if (aa_loc.IsSite()) {
        x_ValidateProteinLocation(aa_loc.GetSite());
        return;
    }

    if (aa_loc.IsRange()) {
        if (!aa_loc.GetRange().IsSetStart()) {
            string error_msg = "Amino-acid range start site is unspecified";
            NCBI_THROW(CHgvsVariantException, eInvalidProteinLocation, error_msg);
        }
        x_ValidateProteinLocation(aa_loc.GetRange().GetStart());

        if (!aa_loc.GetRange().IsSetStop()) {
            string error_msg = "Amino-acid range stop site is unspecified";
            NCBI_THROW(CHgvsVariantException, eInvalidProteinLocation, error_msg);
        }
        x_ValidateProteinLocation(aa_loc.GetRange().GetStop());
    }

    x_ValidateProteinLocation(aa_loc.GetInt());
}


void CHgvsVariantValidator::x_ValidateProteinLocation(const CAaInterval& aa_int)
{
    if (!aa_int.IsSetStart()) {
        string err_msg = "Amino-acid interval start site is unspecified";
        NCBI_THROW(CHgvsVariantException, eInvalidProteinLocation, err_msg);
    }
    x_ValidateProteinLocation(aa_int.GetStart());

    if (!aa_int.IsSetStop()) {
        string err_msg = "Amino-acid interval stop site is unspecified";
        NCBI_THROW(CHgvsVariantException, eInvalidProteinLocation, err_msg);
    }
    x_ValidateProteinLocation(aa_int.GetStop());
}


void CHgvsVariantValidator::x_ValidateProteinLocation(const CAaSite& aa_site)
{
    if (!aa_site.IsSetIndex()) {
        NCBI_THROW(CHgvsVariantException, 
                   eInvalidProteinLocation,
                   "Unspecified amino-acid site index");
    }


    if (aa_site.GetIndex() == 0) {
        NCBI_THROW(CHgvsVariantException, 
                   eInvalidProteinLocation, 
                   "Amino-acid site index set to 0");
    }


    if (!aa_site.IsSetAa()) {
        NCBI_THROW(CHgvsVariantException, 
                   eInvalidProteinLocation, 
                   "Unspecified amino-acid code");
    }

     x_ValidateAaCode(aa_site.GetAa());
}


CRegexp CHgvsVariantValidator::aa_regexp(
        "(Ala|Cys|Asp|Glu|Phe|Gly|His|Ile|Lys|Leu|Met|Asn|Pro|Gln|Arg|Ser|Thr|Val|Trp|Tyr|Sec|Pyl|G|P|A|V|L|I|M|C|F|Y|W|H|K|R|Q|N|E|D|S|T)+"
        );


// Need to return to this - it doesn't yet distinquish between a single amino acid and an amino-acid sequence
void CHgvsVariantValidator::x_ValidateAaCode(const string& aa_code, const bool allow_multiple, const bool use_extended_codeset)
{
    aa_regexp.GetMatch(aa_code, 0, 0, CRegexp::fMatch_default, true);
    const int* results = aa_regexp.GetResults(0);

    if (results[1] == aa_code.size()) {
        return;
    } else if (use_extended_codeset) { // The sequence may end with a stop codon
       auto remainder = aa_code.substr(results[1]);
       if (remainder == "Ter" ||
           remainder == "*"   ||
           remainder == "X") {
           return;
       } 
    }

    NCBI_THROW(CHgvsVariantException, 
               eInvalidAminoAcidCode,
               aa_code + " is not a valid amino-acid code");
}


void CHgvsVariantValidator::x_ValidateCount(const CCount& count) 
{
    if (count.IsRange()) {
        if (!count.GetRange().IsSetStart()) {
            NCBI_THROW(CHgvsVariantException,
                       eInvalidCount,
                       "Count range start unspecified");
        }
        if (!count.GetRange().IsSetStop()) {
            NCBI_THROW(CHgvsVariantException,
                       eInvalidCount,
                       "Count range stop unspecified");
        }
    }
    return;
}


void CHgvsVariantValidator::x_ValidateNtCode(const string& nt_code, const bool is_rna, const bool allow_multiple) 
{

    if (nt_code.empty()) {
    }

    if (!allow_multiple && nt_code.size() > 1) {
        NCBI_THROW(CHgvsVariantException,
                   eInvalidNucleotideCode,
                   "Expected single nucleotide. Got " + nt_code);
    }

    for (const char& nt : nt_code) {
       if ((!is_rna && 
           (nt != 'A' &&
            nt != 'G' &&
            nt != 'C' &&
            nt != 'T')) &&
           (is_rna &&
           (nt != 'a' &&
            nt != 'g' &&
            nt != 'c' &&
            nt != 'u'))) {
           NCBI_THROW(CHgvsVariantException, 
                      eInvalidNucleotideCode,
                      string("Invalid nucleotide code: ") + nt);
       }
    } 
}

END_SCOPE(objects)
END_NCBI_SCOPE
