#ifndef _HGVS_VALIDATOR_HPP_
#define _HGVS_VALIDATOR_HPP_


#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/xregexp/regexp.hpp>
#include <objects/varrep/varrep__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CHgvsVariantException : public CException
{
public:
    enum EErrCode {
        eParseError,
        eInvalidNALocation,
        eInvalidProteinLocation,
        eInvalidNucleotideCode,
        eInvalidAminoAcidCode,
        eInvalidCount,
        eInvalidVariantType,
        eMissingVariantData,
        eUnsupported
    };

    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CHgvsVariantException,CException);
};



class CHgvsVariantValidator 
{
public:
    static void Validate(const CVariantExpression& hgvs_variant);

private:

    static void x_ValidateSimpleVariant(const CSimpleVariant& simple_variant,
                                        const CSequenceVariant::TSeqtype& seq_type);

    static void x_ValidateSimpleNAVariant(const CSimpleVariant& simple_variant,
                                          const CSequenceVariant::TSeqtype& seq_type);

    static void x_ValidateSimpleProteinVariant(const CSimpleVariant& simple_variant);

    static void x_ValidateNtLocation(const CNtLocation& nt_loc,
                                     const CSequenceVariant::TSeqtype& seq_type);

    static void x_ValidateNtLocation(const CNtInterval& nt_int,
                                     const CSequenceVariant::TSeqtype& seq_type);

    static void x_ValidateNtLocation(const CNtSiteRange& nt_range,
                                  const CSequenceVariant::TSeqtype& seq_type);

    static void x_ValidateNtLocation(const CNtSite& nt_site,
                                     const CSequenceVariant::TSeqtype& seq_type);

    static void x_ValidateProteinLocation(const CAaLocation& aa_loc);

    static void x_ValidateProteinLocation(const CAaInterval& aa_int);

    static void x_ValidateProteinLocation(const CAaSite& aa_site);

    static void x_ValidateAaCode(const string& aa_code, bool allow_multiple = false, bool use_extended_codeset=false);

    static void x_ValidateNtCode(const string& nt_code, bool is_rna = false, bool allow_multiple = false);

    static void x_ValidateCount(const CCount& count);

    static void x_CheckNoOffsets(const CNtLocation& nt_loc);

    static CRegexp aa_regexp;
};

END_SCOPE(objects)
END_NCBI_SCOPE


#endif // _HGVS_VALIDATOR_HPP_
