#ifndef CODE_HPP
#define CODE_HPP

#include <corelib/ncbistd.hpp>
#include <set>
#include <map>
#include <list>
#include "autoptr.hpp"

BEGIN_NCBI_SCOPE

class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class ASNType;
class CClassCode;

class CFileCode
{
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;
    typedef map<string, AutoPtr<CClassCode> > TClasses;

    CFileCode(const CNcbiRegistry& registry,
              const string& baseName, const string& headerPrefix);
    ~CFileCode(void);

    bool AddType(const ASNType* type);

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

    const CNcbiRegistry& GetRegistry(void) const
        {
            return m_Registry;
        }
    operator const CNcbiRegistry&(void) const
        {
            return m_Registry;
        }
    
private:
    // current type state
    const CNcbiRegistry& m_Registry;

    // file names
    string m_BaseName;
    string m_HeaderPrefix;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;

    // classes code
    TClasses m_Classes;

};

class CClassCode
{
public:
    typedef CFileCode::TIncludes TIncludes;
    typedef CFileCode::TForwards TForwards;
    typedef list< pair<string, string> > TEnums;

    CClassCode(CFileCode& file, const string& typeName, const ASNType* type);
    ~CClassCode(void);
    
    const CNcbiRegistry& GetRegistry(void) const
        {
            return m_Code.GetRegistry();
        }
    operator const CNcbiRegistry&(void) const
        {
            return m_Code.GetRegistry();
        }

    const string& GetTypeName(void) const
        {
            return m_TypeName;
        }
    const ASNType* GetType(void) const
        {
            return m_Type;
        }

    const string& GetNamespace(void) const
        {
            return m_Namespace;
        }
    const string& GetClassName(void) const
        {
            return m_ClassName;
        }

    const ASNType* GetParentType(void) const;
    string GetParentClass(void) const;

    void SetAbstract(bool abstract = true)
        {
            m_Abstract = abstract;
        }
    bool IsAbstract(void) const
        {
            return m_Abstract;
        }

    void AddHPPInclude(const string& s)
        {
            m_Code.AddHPPInclude(s);
        }
    void AddCPPInclude(const string& s)
        {
            m_Code.AddCPPInclude(s);
        }
    void AddForwardDeclaration(const string& s, const string& ns)
        {
            m_Code.AddForwardDeclaration(s, ns);
        }
    void AddHPPIncludes(const TIncludes& includes)
        {
            m_Code.AddHPPIncludes(includes);
        }
    void AddCPPIncludes(const TIncludes& includes)
        {
            m_Code.AddCPPIncludes(includes);
        }
    void AddForwardDeclarations(const TForwards& forwards)
        {
            m_Code.AddForwardDeclarations(forwards);
        }
    void AddEnum(const string& enumHPP, const string& enumCPP)
        {
            m_Enums.push_back(make_pair(enumHPP, enumCPP));
        }

    CNcbiOstream& HPP(void)
        {
            return m_HPP;
        }

    CNcbiOstream& CPP(void)
        {
            return m_CPP;
        }

    CNcbiOstream& GenerateHPP(CNcbiOstream& header) const;
    CNcbiOstream& GenerateCPP(CNcbiOstream& code) const;

private:
    CFileCode& m_Code;
    string m_TypeName;
    const ASNType* m_Type;
    string m_Namespace;
    string m_ClassName;
    string m_ParentClass;
    bool m_Abstract;

    mutable CNcbiOstrstream m_HPP;
    mutable CNcbiOstrstream m_CPP;
    TEnums m_Enums;
};

class CTypeStrings {
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;

    enum ETypeType {
        eStdType,
        eClassType,
        eComplexType,
        ePointerType,
        eEnumType
    };
    void SetStd(const string& c);
    void SetClass(const string& c);
    void SetEnum(const string& c, const string& e);
    void SetComplex(const string& c, const string& m);
    void SetComplex(const string& c, const string& m,
                    const CTypeStrings& arg);
    void SetComplex(const string& c, const string& m,
                    const CTypeStrings& arg1, const CTypeStrings& arg2);

    ETypeType type;
    string cType;
    string macro;

    void ToSimple(void);
        
    string GetRef(void) const;

    void AddHPPInclude(const string& s)
        {
            m_HPPIncludes.insert(s);
        }
    void AddCPPInclude(const string& s)
        {
            m_CPPIncludes.insert(s);
        }
    void AddForwardDeclaration(const string& s, const string& ns)
        {
            m_ForwardDeclarations[s] = ns;
        }
    void AddIncludes(const CTypeStrings& arg);

    void AddMember(CClassCode& code,
                   const string& member) const;
    void AddMember(CClassCode& code,
                   const string& name, const string& member) const;
private:
    void x_AddMember(CClassCode& code,
                     const string& name, const string& member) const;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;
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
