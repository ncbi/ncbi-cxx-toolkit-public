#include "filecode.hpp"
#include "code.hpp"
#include "type.hpp"

CFileCode::CFileCode(const string& baseName, const string& headerPrefix)
    : m_BaseName(baseName), m_HeaderPrefix(headerPrefix)
{
}

CFileCode::~CFileCode(void)
{
}

string CFileCode::GetHPPName(void) const
{
    return GetHeaderPrefix() + GetFileBaseName() + "_Base.hpp";
}

string CFileCode::GetUserHPPName(void) const
{
    return GetHeaderPrefix() + GetFileBaseName() + ".hpp";
}

string CFileCode::GetCPPName(void) const
{
    return GetFileBaseName() + "_Base.cpp";
}

string CFileCode::GetUserCPPName(void) const
{
    return GetFileBaseName() + ".cpp";
}

string CFileCode::GetBaseDefine(void) const
{
    string s;
    for ( string::const_iterator i = GetFileBaseName().begin();
          i != GetFileBaseName().end();
          ++i ) {
        char c = *i;
        if ( c >= 'a' && c <= 'z' )
            c = c + ('A' - 'a');
        else if ( (c < 'A' || c > 'Z') &&
                  (c < '0' || c > '9') )
            c = '_';
        s += c;
    }
    return s;
}

string CFileCode::GetHPPDefine(void) const
{
    return GetBaseDefine() + "_BASE_HPP";
}

string CFileCode::GetUserHPPDefine(void) const
{
    return GetBaseDefine() + "_HPP";
}

string CFileCode::Include(const string& s) const
{
    if ( s.empty() )
        THROW1_TRACE(runtime_error, "Empty file name");
    switch ( s[0] ) {
    case '<':
    case '"':
        return s;
    default:
        return '<' + GetHeaderPrefix() + s + ".hpp>";
    }
}

void CFileCode::AddHPPInclude(const string& s)
{
    m_HPPIncludes.insert(Include(s));
}

void CFileCode::AddCPPInclude(const string& s)
{
    m_CPPIncludes.insert(Include(s));
}

void CFileCode::AddForwardDeclaration(const string& cls, const string& ns)
{
    m_ForwardDeclarations[cls] = ns;
}

void CFileCode::AddHPPIncludes(const TIncludes& includes)
{
    for ( TIncludes::const_iterator i = includes.begin();
          i != includes.end();
          ++i ) {
        AddHPPInclude(*i);
    }
}

void CFileCode::AddCPPIncludes(const TIncludes& includes)
{
    for ( TIncludes::const_iterator i = includes.begin();
          i != includes.end();
          ++i ) {
        AddCPPInclude(*i);
    }
}

void CFileCode::AddForwardDeclarations(const TForwards& forwards)
{
	for ( TForwards::const_iterator i = forwards.begin();
          i != forwards.end(); ++i )
		m_ForwardDeclarations.insert(*i);
}

void CFileCode::GenerateHPP(const string& path) const
{
    string fileName = path + GetHPPName();
    CNcbiOfstream header(fileName.c_str());
    if ( !header ) {
        ERR_POST("Cannot create file: " << fileName);
        return;
    }

    string hppDefine = GetHPPDefine();
    header <<
        "// This is generated file, don't modify" << NcbiEndl <<
        "#ifndef " << hppDefine << NcbiEndl <<
        "#define " << hppDefine << NcbiEndl <<
        NcbiEndl <<
        "// standard includes" << NcbiEndl <<
        "#include <corelib/ncbistd.hpp>" << NcbiEndl;

    // collect parent classes
    for ( TClasses::const_iterator ci = m_Classes.begin();
          ci != m_Classes.end();
          ++ci) {
        ci->second->GetParentType();
    }

    if ( !m_HPPIncludes.empty() ) {
        header << NcbiEndl <<
            "// generated includes" << NcbiEndl;
        for ( TIncludes::const_iterator stri = m_HPPIncludes.begin();
              stri != m_HPPIncludes.end();
              ++stri) {
            header <<
                "#include " << *stri << NcbiEndl;
        }
    }

    CNamespace ns(header);
    
    if ( !m_ForwardDeclarations.empty() ) {
        header << NcbiEndl <<
            "// forward declarations" << NcbiEndl;
        for ( TForwards::const_iterator fi = m_ForwardDeclarations.begin();
              fi != m_ForwardDeclarations.end();
              ++fi) {
            ns.Set(fi->second);
            header <<
                "class " << fi->first << ';'<< NcbiEndl;
        }
    }
    
    if ( !m_Classes.empty() ) {
        header << NcbiEndl <<
            "// generated classes" << NcbiEndl;
        for ( TClasses::const_iterator ci = m_Classes.begin();
              ci != m_Classes.end();
              ++ci) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateHPP(header);
        }
    }
    ns.End();
    
    header << NcbiEndl <<
        "#endif // " << hppDefine << NcbiEndl;
    header.close();
    if ( !header )
        ERR_POST("Error writing file " << fileName);
}

void CFileCode::GenerateCPP(const string& path) const
{
    string fileName = path + GetCPPName();
    CNcbiOfstream code(fileName.c_str());
    if ( !code ) {
        ERR_POST("Cannot create file: " << fileName);
        return;
    }

    code <<
        "// This is generated file, don't modify" << NcbiEndl <<
        NcbiEndl <<
        "// standard includes" << NcbiEndl <<
        "#include <serial/serial.hpp>" << NcbiEndl <<
        NcbiEndl <<
        "// generated includes" << NcbiEndl <<
        "#include <" << GetUserHPPName() << ">" << NcbiEndl;

    if ( !m_CPPIncludes.empty() ) {
        code << NcbiEndl <<
            "// generated includes" << NcbiEndl;
        for ( TIncludes::const_iterator stri = m_CPPIncludes.begin();
              stri != m_CPPIncludes.end();
              ++stri) {
            code <<
                "#include " << *stri << NcbiEndl;
        }
    }

    CNamespace ns(code);
    
    if ( !m_Classes.empty() ) {
        code << NcbiEndl <<
            "// generated classes" << NcbiEndl;
        for ( TClasses::const_iterator ci = m_Classes.begin();
              ci != m_Classes.end();
              ++ci) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateCPP(code);
        }
    }
    code.close();
    if ( !code )
        ERR_POST("Error writing file " << fileName);
}

bool CFileCode::GenerateUserHPP(const string& path) const
{
    string fileName = path + GetUserHPPName();
    CNcbiOfstream header(fileName.c_str(), IOS_BASE::app);
    if ( !header ) {
        ERR_POST("Cannot create file: " << fileName);
        return false;
    }
	header.seekp(0, IOS_BASE::end);
    if ( streampos(header.tellp()) != streampos(0) ) {
        ERR_POST(Warning <<
                 "Will not overwrite existing user file: " << fileName);
        return false;
    }

    string hppDefine = GetUserHPPDefine();
    header <<
        "// This is generated file, you may modify it freely" << NcbiEndl <<
        "#ifndef " << hppDefine << NcbiEndl <<
        "#define " << hppDefine << NcbiEndl <<
        NcbiEndl <<
        "// standard includes" << NcbiEndl <<
        "#include <corelib/ncbistd.hpp>" << NcbiEndl;

    header << NcbiEndl <<
        "// generated includes" << NcbiEndl <<
        "#include <" << GetHPPName() << '>' << NcbiEndl;
    
    CNamespace ns(header);
    
    if ( !m_Classes.empty() ) {
        header << NcbiEndl <<
            "// generated classes" << NcbiEndl;
        for ( TClasses::const_iterator ci = m_Classes.begin();
              ci != m_Classes.end();
              ++ci) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateUserHPP(header);
        }
    }
    ns.End();
    
    header << NcbiEndl <<
        "#endif // " << hppDefine << NcbiEndl;
    header.close();
    if ( !header )
        ERR_POST("Error writing file " << fileName);
    return true;
}

bool CFileCode::GenerateUserCPP(const string& path) const
{
    string fileName = path + GetUserCPPName();
    CNcbiOfstream code(fileName.c_str(), IOS_BASE::app);
    if ( !code ) {
        ERR_POST("Cannot create file: " << fileName);
        return false;
    }
	code.seekp(0, IOS_BASE::end);
    if ( streampos(code.tellp()) != streampos(0) ) {
        ERR_POST(Warning <<
                 "Will not overwrite existing user file: " << fileName);
        return false;
    }

    code <<
        "// This is generated file, you may modify it freely" << NcbiEndl <<
        NcbiEndl <<
        "// standard includes" << NcbiEndl <<
        "#include <serial/serial.hpp>" << NcbiEndl <<
        NcbiEndl <<
        "// generated includes" << NcbiEndl <<
        "#include <" << GetUserHPPName() << ">" << NcbiEndl;

    CNamespace ns(code);
    
    if ( !m_Classes.empty() ) {
        code << NcbiEndl <<
            "// generated classes" << NcbiEndl;
        for ( TClasses::const_iterator ci = m_Classes.begin();
              ci != m_Classes.end();
              ++ci) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateUserCPP(code);
        }
    }

    code.close();
    if ( !code )
        ERR_POST("Error writing file " << fileName);
    return true;
}

bool CFileCode::AddType(const CDataType* type)
{
    AutoPtr<CClassCode>& cls = m_Classes[type->IdName()];
    if ( cls ) {
        _ASSERT(cls->GetType() == type);
        return false;
    }

    cls = new CClassCode(*this, type->IdName(), type);
    return true;
}

CNamespace::CNamespace(CNcbiOstream& out)
    : m_Out(out)
{
}

CNamespace::~CNamespace(void)
{
    End();
}

void CNamespace::Begin(void)
{
    if ( !m_Namespace.empty() ) {
        m_Out << NcbiEndl <<
            "namespace " << m_Namespace << " {" << NcbiEndl <<
            NcbiEndl;
    }
}

void CNamespace::End(void)
{
    if ( !m_Namespace.empty() ) {
        m_Out << NcbiEndl <<
            "}" << NcbiEndl << NcbiEndl;
        m_Namespace.erase();
    }
}

void CNamespace::Set(const string& ns)
{
    if ( ns != m_Namespace ) {
        End();
        m_Namespace = ns;
        Begin();
    }
}

