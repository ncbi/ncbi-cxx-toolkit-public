#ifndef __COL_VALIDATOR_HPP_INCLUDED__
#define __COL_VALIDATOR_HPP_INCLUDED__

#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

class CTempString;

class CColumnValidator
{
protected:
    CColumnValidator();
public:
    virtual ~CColumnValidator();
    virtual bool DoValidate(const CTempString& value, string& error) const = 0;
    static bool IsDiscouraged(const string& column);
};

class CColumnValidatorRegistry
{
protected:
    CColumnValidatorRegistry();
    ~CColumnValidatorRegistry();
public:
    typedef map<string, CColumnValidator*> TColRegistry;
    static CColumnValidatorRegistry& GetInstance();
    void Register(const CTempString& name, CColumnValidator* val);
    void Register(const CTempString& name, const CTempString& alias);
    void UnRegister(CColumnValidator* val);

    bool IsSupported(const string& datatype) const; 
    void PrintSupported(CNcbiOstream& out_stream) const;

    bool DoValidate(const string& datatype, const CTempString& value, string& error) const;
private:
    TColRegistry m_registry;
};


END_NCBI_SCOPE

#endif
