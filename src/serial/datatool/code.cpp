#include <corelib/ncbireg.hpp>
#include "code.hpp"
#include "type.hpp"
#include <fstream>

CCode::CCode(const string& baseName, const string& headerPrefix,
             const string& ns)
{
    string s = headerPrefix + baseName;
    m_HeaderName = s + "_Base.hpp";
    m_UserHeaderName = s + ".hpp";
    m_CPPName = baseName + "_Base.cpp";
    for ( string::const_iterator i = baseName.begin(); i != baseName.end(); ++i ) {
        char c = *i;
        if ( c >= 'a' && c <= 'z' )
            c = c + ('A' - 'a');
        else if ( c < 'A' || c > 'Z' )
            c = '_';
        m_HeaderDefineName += c;
    }
    m_HeaderDefineName += "_BASE_HPP";
    m_Namespace = ns;
}

CCode::~CCode(void)
{
}

void CCode::GenerateHeader(const string& path) const
{
    ofstream header((path + m_HeaderName).c_str());

    header <<
        "// This is generated file, don't modify" << endl <<
        "#ifndef " << m_HeaderDefineName << endl <<
        "#define " << m_HeaderDefineName << endl <<
        endl << "// standard includes" << endl <<
        "#include <corelib/ncbistd.hpp>" << endl;

    if ( !m_HeaderDefineName.empty() ) {
        header <<
            endl << "// generated includes" << endl;
        for ( set<string>::const_iterator stri = m_HeaderIncludes.begin();
              stri != m_HeaderIncludes.end();
              ++stri) {
            header <<
                "#include <" << *stri << '>' << endl;
        }
    }

    if ( !m_Namespace.empty() )
        header << endl << "namespace " << m_Namespace << ';' << endl;
    
    if ( !m_ForwardDeclarations.empty() ) {
        header <<
            endl << "// forward declarations" << endl;
        for ( set<string>::const_iterator stri = m_ForwardDeclarations.begin();
              stri != m_ForwardDeclarations.end();
              ++stri) {
            header <<
                "class " << *stri << ';'<< endl;
        }
    }

    if ( !m_ClassDeclarations.empty() ) {
        header <<
            endl << "// generated classes" << endl;
        for ( set<string>::const_iterator stri = m_ForwardDeclarations.begin();
              stri != m_ForwardDeclarations.end();
              ++stri) {
            header <<
                "class " << *stri << ';'<< endl;
        }
    }
    
    if ( !m_Namespace.empty() )
        header << endl << "};" << endl;

    header << endl <<
        "#endif // " << m_HeaderDefineName << endl;
}

void CCode::GenerateCPP(const string& path) const
{
    ofstream code((path + m_CPPName).c_str());

    code <<
        "// This is generated file, don't modify" << endl <<
        endl << "// standard includes" << endl <<
        "#include <serial/serial.hpp>" << endl <<
        endl << "// generated includes" << endl <<
        "#include <" << m_HeaderName << ">" << endl;
}

const string& CCode::GetVar(const string& name) const
{
    return m_Registry->Get(m_Section, name);
}

void CCode::AddType(const ASNType* type, const CNcbiRegistry& def)
{
    m_Section = type->name;
    m_Registry = &def;
    m_HeaderCode.erase();
    m_CPPCode.erase();
    type->GenerateCode(*this);
    m_ClassDeclarations.push_back(m_HeaderCode);
    m_ClassDefinitions.push_back(m_CPPCode);
    m_HeaderCode.erase();
    m_CPPCode.erase();
    m_Registry = 0;
    m_Section.erase();
}
