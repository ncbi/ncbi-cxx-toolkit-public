#ifndef FILECODE_HPP
#define FILECODE_HPP

#include <corelib/ncbistd.hpp>
#include "autoptr.hpp"
#include <map>
#include <set>

USING_NCBI_SCOPE;

class CDataType;
class CClassCode;

class CFileCode
{
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;
    typedef map<string, AutoPtr<CClassCode> > TClasses;

    CFileCode(const string& baseName, const string& headerPrefix);
    ~CFileCode(void);

    bool AddType(const CDataType* type);

    string Include(const string& s) const;
    const string& GetFileBaseName(void) const
        {
            return m_BaseName;
        }
    const string& GetHeaderPrefix(void) const
        {
            return m_HeaderPrefix;
        }
    string GetHPPName(void) const;
    string GetCPPName(void) const;
    string GetUserHPPName(void) const;
    string GetUserCPPName(void) const;
    string GetBaseDefine(void) const;
    string GetHPPDefine(void) const;
    string GetUserHPPDefine(void) const;

    void AddHPPInclude(const string& s);
    void AddCPPInclude(const string& s);
    void AddForwardDeclaration(const string& cls,
                               const string& ns = NcbiEmptyString);
    void AddHPPIncludes(const TIncludes& includes);
    void AddCPPIncludes(const TIncludes& includes);
    void AddForwardDeclarations(const TForwards& forwards);

    void GenerateHPP(const string& path) const;
    void GenerateCPP(const string& path) const;
    bool GenerateUserHPP(const string& path) const;
    bool GenerateUserCPP(const string& path) const;

private:
    // file names
    string m_BaseName;
    string m_HeaderPrefix;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;

    // classes code
    TClasses m_Classes;

};

class CNamespace
{
public:
    CNamespace(CNcbiOstream& out);
    ~CNamespace(void);
    
    void Set(const string& ns);

    void End(void);

private:
    void Begin(void);

    CNcbiOstream& m_Out;
    string m_Namespace;
};

#endif
