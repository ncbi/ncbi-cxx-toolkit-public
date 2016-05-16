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

BEGIN_NCBI_SCOPE
//BEGIN_objects_SCOPE
USING_SCOPE(objects);

class CAaSeqlocHelper 
{
public: 
    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CSimpleVariant& var_desc,
                                       CVariationIrepMessageListener& listener);

    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id, 
                                       const CAaLocation& aa_loc,
                                       CVariationIrepMessageListener& listener);

    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id, 
                                       const CAaSite& aa_site,
                                       CVariationIrepMessageListener& listener);

    static CRef<CSeq_loc> CreateSeqloc(const CSeq_id& seq_id,
                                       const CAaInterval& aa_int,
                                       CVariationIrepMessageListener& listener);
};


class CHgvsProtIrepReader 
{
public:
        using TSet = CVariation_ref::C_Data::C_Set;
        using TDelta = CVariation_inst::TDelta::value_type;
       
        CHgvsProtIrepReader(CScope& scope, CVariationIrepMessageListener& message_listener) 
            : m_Scope(scope), 
              m_IdResolver(Ref(new CIdResolver(scope))), 
              m_MessageListener(message_listener) {}

 private:
        CSeq_data::TNcbieaa x_ConvertToNcbieaa(string aa_seq) const;

        CRef<CVariation_ref> x_CreateInsertionSubvarref(CSeq_literal::TLength length) const;

        CRef<CVariation_ref> x_CreateInsertedRawseqSubvarref(const string& raw_seq) const;

        CRef<CVariation_ref> x_CreateInsertedCountSubvarref(const CCount& count) const;

        CRef<CVariation_ref> x_CreateInsertionSubvarref(const CInsertion::TSeqinfo& insert) const;

        CRef<CVariation_ref> x_CreateDelinsSubvarref(const CDelins::TInserted_seq_info& insert) const;

        CRef<CVariation_ref> x_CreateIdentitySubvarref(const CAaLocation& aa_loc) const;

        CRef<CVariation_ref> x_CreateIdentitySubvarref(const string& seq_str, 
                                                       CSeq_literal::TLength length) const;

        CRef<CVariation_ref> x_CreateVarref(const string& var_name,
                                            const string& identifier, 
                                            const CSimpleVariant& var_desc) const;

        CRef<CVariation_ref> x_CreateProteinDelVarref(const string& identifier,
                                                      const CDeletion& del,
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateProteinDupVarref(const string& identifier,
                                                      const CDuplication& dup,
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateProteinSubVarref(const string& identifier, 
                                                      const CProteinSub& sub,
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateFrameshiftVarref(const string& identifier,
                                                      const CFrameshift& fs,
                                                      const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateInsertionVarref(const string& identifier, 
                                                     const CInsertion& ins,
                                                     const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateDelinsVarref(const string& identifier,
                                                  const CDelins& delins,
                                                  const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CRef<CVariation_ref> x_CreateSSRVarref(const string& identifier, 
                                               const CRepeat& ssr,
                                               const CVariation_ref::EMethod_E method=CVariation_ref::eMethod_E_unknown) const;

        CScope& m_Scope;
        CRef<CIdResolver> m_IdResolver;
        CVariationIrepMessageListener& m_MessageListener;

        CRef<CSeq_feat> x_CreateSeqfeat(const string& var_name,
                                        const string& identifier,
                                        const CSimpleVariant& var_desc) const;

public:
        CRef<CSeq_feat> CreateSeqfeat(CRef<CVariantExpression>& variant_expr) const;

        list<CRef<CSeq_feat>> CreateSeqfeats(CRef<CVariantExpression>& variant_expr) const;
}; // CHgvsProtIrepReader

//END_objects_SCOPE
END_NCBI_SCOPE

#endif // _PROTEIN_IREP_TO_SEQFEAT_HPP_
