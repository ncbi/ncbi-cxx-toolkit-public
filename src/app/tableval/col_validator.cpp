#include <ncbi_pch.hpp>

#include "col_validator.hpp"
#include <algorithm>

#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/taxon1/taxon1.hpp>

#include <corelib/ncbiexpt.hpp>

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
    bool DoValidate(const CTempString& value, string& error) const; \
}; \
CAutoColValidator##name CAutoColValidator##name::_singleton; \
bool CAutoColValidator##name::DoValidate(const CTempString& value, string& error) const

#define DEFINE_COL_VALIDATOR(name) DEFINE_COL_VALIDATOR_WITH_ALT_NAMES(name, #name, 0, 0)
#define DEFINE_COL_VALIDATOR2(name, text) DEFINE_COL_VALIDATOR_WITH_ALT_NAMES(name, text, 0, 0)
#define DEFINE_COL_VALIDATOR3(name, text1, text2) DEFINE_COL_VALIDATOR_WITH_ALT_NAMES(name, text1, text2, 0)
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

void CColumnValidatorRegistry::Register(const CTempString& name, const CTempString& alias)
{
    if (m_registry.find(alias) == m_registry.end())
    {
       NCBI_THROW(CException, eUnknown, string("Datatype ") + string(alias) + string(" isn't supported"));
    }
    else
       m_registry[name] = m_registry[alias];
}

void CColumnValidatorRegistry::Register(const CTempString& name, CColumnValidator* val)
{
    m_registry[name] = val;
}

bool CColumnValidatorRegistry::IsSupported(const string& datatype) const
{
    TColRegistry::const_iterator it = m_registry.find(datatype);
    return (it != m_registry.end());
}

void CColumnValidatorRegistry::PrintSupported(CNcbiOstream& out_stream) const
{
    set<string> names;
    ITERATE(TColRegistry, it, m_registry)
    {
        names.insert(it->first);
    }

    static const char* message = "Supported data types are: ";
    size_t n = strlen(message);
    out_stream << message; 
    ITERATE(set<string>, it, names)
    {
        if (it != names.begin()) {
          out_stream << ", ";
          n+=2;
        }
        if (n + it->length() > 77) {
          out_stream << endl; 
          n = 0;
        }
        out_stream << *it;
        n += it->length();
    }
    out_stream << endl;
}

bool CColumnValidatorRegistry::DoValidate(const string& datatype, 
    const CTempString& value, string& error) const
{
    TColRegistry::const_iterator it = m_registry.find(datatype);
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
        /*int new_taxid =*/ m_taxClient->GetTaxIdByName(value);
    }
    catch(const CException& ex)
    {
        error = ex.GetMsg();
    }

    return false;
}

DEFINE_COL_VALIDATOR(genome)
{
    /*CBioSource::EGenome genome =*/ CBioSource::GetGenomeByOrganelle (value);
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
    /*CSubSource::TSubtype subtype = */CSubSource::GetSubtypeValue(value);
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
    /*COrgMod::TSubtype subtype = */COrgMod::GetSubtypeValue(value);
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

DEFINE_COL_VALIDATOR(seqid)
{
    if (!value.empty())
    try
    {
        CSeq_id id(value, CSeq_id::fParse_AnyLocal);
    }
    catch(const CException& ex)
    {
        error = ex.GetMsg();
    }
    
    return false;
}

DEFINE_COL_VALIDATOR(date)
{
    //if (!value.empty())
    static const char* supported_formats[]= {
       "d-b-Y", "d-B-Y", "b-Y", "B-Y", "Y", 
       "M-D-Y", "M/D/Y",
       0
    };
    bool bad_format(true), in_future(false);
    if (CSubSource::IsISOFormatDate(value))
    {
       CRef<CDate> date(CSubSource::GetDateFromISODate(value));
       bad_format = false;
       CDate today(CTime(CTime::eCurrent));
       in_future = (date->Compare(today) == CDate::eCompare_after);
    }
    else
    {
      for (const char** fmt = supported_formats; *fmt && bad_format; fmt++)
      {
         try
         {
             CTime time(value, *fmt);
             bad_format = false;
             in_future = (time > CTime(CTime::eCurrent));
         }
         catch (const CException& ex)
         {
             error = ex.GetMsg();
         }
      }
    }

    if (bad_format)
      error = "Unsupported date format";
    else
    if (in_future)
      error = "Date in future";
    else
      error.clear();
   
    return false;
}

DEFINE_COL_VALIDATOR(country)
{
    if (!value.empty())
    try
    {
        bool is_miscapitalized(false);
        if (!CCountries::IsValid(value, is_miscapitalized))
            error = "Country is not valid";
        if (is_miscapitalized)
            error = "Country is miscapitalized";
    }
    catch (const CException& ex)
    {
        error = ex.GetMsg();
    }

    return false;
}

DEFINE_COL_VALIDATOR(boolean)
{
    if (value != "TRUE" &&
        value != "true")
       error = "Can have TRUE only";

    return false;
}

DEFINE_COL_VALIDATOR3(isolation_source, "isolation source", "host")
{
    return false;
}

bool CColumnValidator::IsDiscouraged(const string& column)
{
    try
    {
       CSubSource::TSubtype st = CSubSource::GetSubtypeValue(column);
       if (CSubSource::IsDiscouraged(st))
           return true;
    }
    catch (const CSerialException&)
    {
        // that's ok
    }

    try
    {
       COrgMod::TSubtype om = COrgMod::GetSubtypeValue(column);
       if (COrgMod::IsDiscouraged(om))
           return true;
    }
    catch (const CSerialException&)
    {
        // that's ok
    }

    return false;
}


END_NCBI_SCOPE



