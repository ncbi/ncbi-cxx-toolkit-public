#include <ncbi_pch.hpp>

#include "col_validator.hpp"
#include <algorithm>

#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/taxon1/taxon1.hpp>

BEGIN_NCBI_SCOPE

#define DEFINE_COL_VALIDATOR_WITH_ALT_NAMES(name, text1, text2, text3) \
class CAutoColValidator##name: public CColumnValidator \
{ \
protected: \
    static CAutoColValidator##name _singleton; \
    CAutoColValidator##name() \
    { \
    if (text1) CColumnValidatorRegistry::GetInstance().Register(text1, this); \
    if (text2) CColumnValidatorRegistry::GetInstance().Register(text2, this); \
    if (text3) CColumnValidatorRegistry::GetInstance().Register(text3, this); \
    } \
    bool DoValidate(const CTempString& value, string& error); \
}; \
CAutoColValidator##name CAutoColValidator##name::_singleton; \
bool CAutoColValidator##name::DoValidate(const CTempString& value, string& error)

#define DEFINE_COL_VALIDATOR(name) DEFINE_COL_VALIDATOR_WITH_ALT_NAMES(name, #name, 0, 0)
#define DEFINE_COL_VALIDATOR2(name, text) DEFINE_COL_VALIDATOR_WITH_ALT_NAMES(name, #text, 0, 0)
#define DEFINE_COL_VALIDATOR3(name, text1, text2) DEFINE_COL_VALIDATOR_WITH_ALT_NAMES(name, #text1, #text2, 0)
#define DEFINE_COL_VALIDATOR4(name, text1, text2, text3) DEFINE_COL_VALIDATOR_WITH_ALT_NAMES(name, text1, text2, text3)

USING_SCOPE(objects);
namespace
{
    auto_ptr<objects::CTaxon1> m_taxClient;
}

CColumnValidator::CColumnValidator()
{
}

CColumnValidator::~CColumnValidator()
{
    //CColumnValidatorRegistry::GetInstance().UnRegister(this);
}

CColumnValidatorRegistry::CColumnValidatorRegistry()
{
}

CColumnValidatorRegistry::~CColumnValidatorRegistry()
{
}


CColumnValidatorRegistry& CColumnValidatorRegistry::GetInstance()
{
    static CColumnValidatorRegistry m_singleton;
    return m_singleton;
}

void CColumnValidatorRegistry::Register(const CTempString& name, CColumnValidator* val)
{
    m_registry[name] = val;
}

bool CColumnValidatorRegistry::DoValidate(const string& datatype, const CTempString& value, string& error)
{
    map<string, CColumnValidator*>::iterator it = m_registry.find(datatype);
    if (it == m_registry.end())
    {
        error = "Datatype \"";
        error += datatype;
        error += "\" is unknown";
        return true;
    }
    else
    {
        return it->second->DoValidate(value, error);
    }
}

void CColumnValidatorRegistry::UnRegister(CColumnValidator* val)
{
#if 0
    struct _pred
    {
        bool operator()(const pair<string, CColumnValidator*>& l) const
        {
            return (l.second == _this);
        }
        CColumnValidator* _this;
    } pred;

    pred._this = val;
    map<string, CColumnValidator*>::iterator it = find_if(m_registry.begin(), m_registry.end(), pred);
    if (it != m_registry.end())
        m_registry.erase(it);
#endif
}

DEFINE_COL_VALIDATOR(taxid)
{
    if (m_taxClient.get() == 0)
    {
        m_taxClient.reset(new CTaxon1);
        m_taxClient->Init();
    }

    try
    {
        int taxid = NStr::StringToInt(value);
        m_taxClient->GetById(taxid);
    }
    catch(const CException& ex)
    {
        error = ex.GetMsg();
    }

    return false;
}


DEFINE_COL_VALIDATOR4(orgname, "taxname", "org", "organism")
{
    if (m_taxClient.get() == 0)
    {
        m_taxClient.reset(new CTaxon1);
        m_taxClient->Init();
    }

    try
    {
        int new_taxid = m_taxClient->GetTaxIdByName(value);
    }
    catch(const CException& ex)
    {
        error = ex.GetMsg();
    }

    return false;
}

DEFINE_COL_VALIDATOR(genome)
{
    CBioSource::EGenome genome = CBioSource::GetGenomeByOrganelle (value);
    return false;
}

DEFINE_COL_VALIDATOR(origin)
{
    CBioSource::EOrigin origin = CBioSource::GetOriginByString(value);
    if (origin == CBioSource::eOrigin_unknown)
        error = "unknown origin type";
    return false;
}

DEFINE_COL_VALIDATOR(subsource)
{
    try
    {
    CSubSource::TSubtype subtype = CSubSource::GetSubtypeValue(value);
    }
    catch(CException& ex)
    {
        error = ex.GetMsg();
    }
    return false;
}

DEFINE_COL_VALIDATOR(orgmod)
{
    return false;
}

DEFINE_COL_VALIDATOR(subtype)
{
    try
    {
    COrgMod::TSubtype subtype = COrgMod::GetSubtypeValue(value);
    }
    catch(CException& ex)
    {
        error = ex.GetMsg();
    }
    return false;
}
DEFINE_COL_VALIDATOR(accession)
{
    CSeq_id::EAccessionInfo info = CSeq_id::IdentifyAccession(value);
    if (info == CSeq_id::eAcc_unknown)
    {
        error = "Unknown accession type";
    }
    return false;
}
DEFINE_COL_VALIDATOR(haplotype)
{
    return false;
}
DEFINE_COL_VALIDATOR(lineage)
{
    return false;
}
DEFINE_COL_VALIDATOR(serotype)
{
    return false;
}
DEFINE_COL_VALIDATOR(strain)
{
    return false;
}

END_NCBI_SCOPE



