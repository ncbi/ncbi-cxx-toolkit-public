#ifndef CODE_HPP
#define CODE_HPP

#include <corelib/ncbistd.hpp>
#include <set>
#include <list>

BEGIN_NCBI_SCOPE

class CNcbiRegistry;

END_NCBI_SCOPE

USING_NCBI_SCOPE;

class ASNType;

class CCode
{
public:
    CCode(const string& baseName, const string& headerPrefix,
          const string& ns);
    ~CCode(void);

    void AddType(const ASNType* type, const CNcbiRegistry& def);

    void AddHeaderInclude(const string& s)
        {
            m_HeaderIncludes.insert(s);
        }
    void AddCPPInclude(const string& s)
        {
            m_CPPIncludes.insert(s);
        }
    void AddInclude(const string& s)
        {
            AddHeaderInclude(s);
            AddCPPInclude(s);
        }
    void AddForwardDeclaration(const string& s)
        {
            m_ForwardDeclarations.insert(s);
        }

    void GenerateHeader(const string& path) const;
    void GenerateCPP(const string& path) const;

    const string& GetVar(const string& name) const;

private:
    string m_HeaderName;
    string m_UserHeaderName;
    string m_CPPName;
    string m_HeaderDefineName;

    string m_Namespace;

    // header info
    set<string> m_HeaderIncludes;
    set<string> m_ForwardDeclarations;
    list<string> m_ClassDeclarations;

    // cpp info
    set<string> m_CPPIncludes;
    list<string> m_ClassDefinitions;

    // current type state
    const CNcbiRegistry* m_Registry;
    string m_Section;
    string m_HeaderCode;
    string m_CPPCode;
};

#endif
