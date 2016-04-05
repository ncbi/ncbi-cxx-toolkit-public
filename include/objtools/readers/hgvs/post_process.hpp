#ifndef _POST_PROCESS_HPP_
#define _POST_PROCESS_HPP_

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/valerr/ValidErrItem.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CPostProcessUtils 
{
public:
    CRef<CSeq_literal> GetLiteralAtLoc(const CSeq_loc& loc, CScope& scope) const;

    bool HasIntronOffset(const CVariation_ref& var_ref) const;

    bool HasIntronOffset(const CVariation_inst& var_inst) const;
};


class CNormalizeVariant 
{
public:
    CNormalizeVariant(CScope& scope) : m_Scope(scope) {}
    CRef<CSeq_feat> GetNormalizedIdentity(const CSeq_feat& seaq_feat) const;
    CRef<CSeq_feat> GetNormalizedSNP(const CSeq_feat& seq_feat) const;
    void NormalizeIdentityInstance(CVariation_inst& identity_inst, const CSeq_loc& location) const;

private:
    CScope& m_Scope;

    CPostProcessUtils utils;
};



class CValidateVariant
{
public:
    CValidateVariant(CScope& scope) : m_Scope(scope) {}
    void ValidateAaIdentityInst(CVariation_inst& identity_inst, const CSeq_loc& location) const;
    void ValidateNaIdentityInst(CVariation_inst& identity_inst, const CSeq_loc& location) const;

private:
    CScope& m_Scope;


    CPostProcessUtils utils;
};



END_SCOPE(objects)
END_NCBI_SCOPE


#endif // _POST_PROCESS_HPP_
