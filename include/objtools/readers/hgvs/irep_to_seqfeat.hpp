#ifndef _IREP_TO_SEQFEAT_HPP_
#define _IREP_TO_SEQFEAT_HPP_

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/varrep/varrep__.hpp>
#include <objmgr/scope.hpp>
#include <objtools/readers/hgvs/id_resolver.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CHgvsIrepReader 
{
public: 
    CHgvsIrepReader(CScope& scope, CVariationIrepMessageListener& message_listener) :
                    m_Scope(scope),
                    m_IdResolver(Ref(new CIdResolver(scope))),
                    m_MessageListener(message_listener) {}

    virtual CRef<CSeq_feat> CreateSeqfeat(const CVariantExpression& variant_expr) const = 0;
    virtual list<CRef<CSeq_feat>> CreateSeqfeats(const CVariantExpression& variant_expr) const = 0;

    virtual ~CHgvsIrepReader() {}

protected:
    CScope& m_Scope;
    CRef<CIdResolver> m_IdResolver;
    CVariationIrepMessageListener& m_MessageListener;
};



END_SCOPE(objects)
END_NCBI_SCOPE



#endif
