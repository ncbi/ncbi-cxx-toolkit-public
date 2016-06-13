#ifndef _IREP_TO_SEQFEAT_HPP_
#define _IREP_TO_SEQFEAT_HPP_

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/varrep/varrep__.hpp>
#include <objmgr/scope.hpp>
#include <objtools/readers/hgvs/id_resolver.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>
#include <objects/seqfeat/Variation_ref.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CHgvsIrepReader 
{
public: 
    CHgvsIrepReader(CScope& scope) :
                    m_Scope(scope),
                    m_IdResolver(Ref(new CIdResolver(scope))) {}

    virtual ~CHgvsIrepReader() {}

    virtual CRef<CSeq_feat> CreateSeqfeat(const CVariantExpression& variant_expr) const = 0;
protected:
    void x_SetMethod(CRef<CVariation_ref> var_ref, CVariation_ref::EMethod_E method) const 
    {
        if (method == CVariation_ref::eMethod_E_unknown) {
            return;
        }
        var_ref->SetMethod().push_back(method);
    }

    CScope& m_Scope;
    CRef<CIdResolver> m_IdResolver;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
