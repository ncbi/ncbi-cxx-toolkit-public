#ifndef TYPESTR_HPP
#define TYPESTR_HPP

#include <corelib/ncbistd.hpp>
#include <set>
#include <map>

USING_NCBI_SCOPE;

class CClassCode;

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

#endif
