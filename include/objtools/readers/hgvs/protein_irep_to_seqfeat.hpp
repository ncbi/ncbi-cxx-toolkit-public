#ifndef _PROTEIN_IREP_TO_SEQFEAT_HPP_ 
#define _PROTEIN_IREP_TO_SEQFEAT_HPP_

#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objmgr/scope.hpp>
#include <objects/varrep/varrep__.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>
#include <objtools/readers/hgvs/id_resolver.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// Helper class for constructing a CSeq_loc instance for a protein variant
class CProtSeqlocHelper 
{
public: 
    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CSimpleVariant& simple_var);

    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id, 
                                       const CAaLocation& aa_loc);

    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id, 
                                       const CAaSite& aa_site);

    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CAaInterval& aa_int);
};


class CHgvsProtIrepReader : public CHgvsIrepReader
{
public:
        using TSet = CVariation_ref::C_Data::C_Set;
        using TDelta = CVariation_inst::TDelta::value_type;
 
        CHgvsProtIrepReader(CScope& scope) 
            : CHgvsIrepReader(scope) {}

        /// Construct the CSeq_feat object for a protein variant in the intermediate expression 
        CRef<CSeq_feat> CreateSeqfeat(const CVariantExpression& variant_expr) const;

 private:
        /// Construct the CSeq_feat object for a "simple" protein variant
        CRef<CSeq_feat> x_CreateSimpleVariantFeat(
            const string& var_name,
            const string& identifier,
            const CSimpleVariant& simple_var) const;
          
        /// Construct the CVariation_ref object for a "simple" variant
        CRef<CVariation_ref> x_CreateVarref(
            const string& var_name,
            const CSimpleVariant& simple_var) const;
        
        CRef<CVariation_ref> x_CreateSilentVarref(
            const CAaLocation& loc,
            const CVariation_ref::EMethod_E method) const;

        CRef<CVariation_ref> x_CreateProteinDelVarref(
            const CDeletion& del,
            CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateProteinDupVarref(
            const CDuplication& dup,
            CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateProteinSubstVarref(
            const CProteinSub& sub,
            CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateFrameshiftVarref(
            const CFrameshift& fs,
            CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateInsertionVarref(
            const CInsertion& ins,
            CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDelinsVarref(
            const CDelins& delins,
            CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateSSRVarref(
            const CRepeat& ssr,
            CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;
        
        CRef<CVariation_ref> x_CreateVarrefFromLoc(
            CVariation_inst::EType type,
            const CAaLocation& aa_loc, 
            CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        /// Construct the first "sub" Variation-ref in the composite Variation-ref appearing in an Insertion Seq-feat
        CRef<CVariation_ref> x_CreateInsertionSubvarref(const CInsertion::TSeqinfo& insert) const;

        /// Construct the "sub" Variation-ref for an insertion where only the size of the insertion is known
        CRef<CVariation_ref> x_CreateInsertionSubvarref(CSeq_literal::TLength length) const;

        /// Construct the "sub" Variation-ref for a the inserted raw sequence
        CRef<CVariation_ref> x_CreateInsertedRawseqSubvarref(const string& raw_seq) const;

        CRef<CVariation_ref> x_CreateDelinsSubvarref(const CDelins::TInserted_seq_info& insert) const;

        CRef<CVariation_ref> x_CreateIdentitySubvarref(const CAaLocation& aa_loc) const;

        CRef<CVariation_ref> x_CreateIdentitySubvarref(const string& seq_str, 
                                                       CSeq_literal::TLength length) const;

        CSeq_data::TNcbieaa x_ConvertToNcbieaa(string aa_seq) const;

}; // CHgvsProtIrepReader

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _PROTEIN_IREP_TO_SEQFEAT_HPP_
