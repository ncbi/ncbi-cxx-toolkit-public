#ifndef CODE_HPP
#define CODE_HPP

#include <corelib/ncbistd.hpp>
#include <set>
#include <map>
#include <list>
#include "autoptr.hpp"

USING_NCBI_SCOPE;

class CDataType;
class CChoiceDataType;
class CFileCode;

class CClassCode
{
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;
    typedef list< pair<string, string> > TMethods;

    CClassCode(CFileCode& file, const string& typeName, const CDataType* type);
    ~CClassCode(void);
    
    const string& GetTypeName(void) const
        {
            return m_TypeName;
        }
    const CDataType* GetType(void) const
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

    const CDataType* GetParentType(void) const;
    string GetParentClass(void) const;

    enum EClassType {
        eNormal,
        eAbstract,
        eEnum,
        eAlias
    };
    void SetClassType(EClassType type);
    EClassType GetClassType(void) const
        {
            return m_ClassType;
        }

    void AddHPPInclude(const string& s);
    void AddCPPInclude(const string& s);
    void AddForwardDeclaration(const string& s, const string& ns);
    void AddHPPIncludes(const TIncludes& includes);
    void AddCPPIncludes(const TIncludes& includes);
    void AddForwardDeclarations(const TForwards& forwards);

    CNcbiOstream& ClassPublic(void)
        {
            return m_ClassPublic;
        }
    CNcbiOstream& ClassPrivate(void)
        {
            return m_ClassPrivate;
        }
    CNcbiOstream& Methods(void)
        {
            return m_Methods;
        }
    CNcbiOstream& TypeInfoBody(void)
        {
            return m_TypeInfoBody;
        }

    CNcbiOstream& GenerateHPP(CNcbiOstream& header) const;
    CNcbiOstream& GenerateCPP(CNcbiOstream& code) const;
    CNcbiOstream& GenerateUserHPP(CNcbiOstream& header) const;
    CNcbiOstream& GenerateUserCPP(CNcbiOstream& code) const;

private:
    CFileCode& m_Code;
    string m_TypeName;
    const CDataType* m_Type;
    string m_Namespace;
    string m_ClassName;
    string m_ParentClass;

    CNcbiOstrstream m_ClassPublic;
    CNcbiOstrstream m_ClassPrivate;
    CNcbiOstrstream m_Methods;
    CNcbiOstrstream m_TypeInfoBody;

    EClassType m_ClassType;
};

#endif
