#ifndef _ID_RESOLVER_HPP_
#define _ID_RESOLVER_HPP_

#include <objmgr/scope.hpp>
#include <corelib/ncbiexpt.hpp>
#include <util/xregexp/regexp.hpp>
#include <objects/entrez2/entrez2_client.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CIdResolver : public CObject
{
public:
    CIdResolver(CScope& scope);
    CSeq_id_Handle GetAccessionVersion(const string& identifier) const;

private:
    bool x_TryProcessLRG(const string& identifier, CSeq_id_Handle& idh) const;
    bool x_TryProcessGenomicLRG(const string& identifier, CSeq_id_Handle& idh) const;
//    CSeq_id_Handle x_ProcessCCDS(const string& identifier);
    bool x_LooksLikeLRG(const string& identifier) const;
    bool x_LooksLikeCCDS(const string& identifier) const;
    CScope& m_Scope; 
    unique_ptr<CRegexp> m_LRGregex; // CRef<CRegexp> is not supported
    unique_ptr<CRegexp> m_CCDSregex;
    unique_ptr<CEntrez2Client> m_E2Client;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ACCESSION_RESOLVER_HPP_
