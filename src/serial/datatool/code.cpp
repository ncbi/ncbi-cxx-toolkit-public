//#include <corelib/ncbireg.hpp>
#include "code.hpp"
#include "type.hpp"

CFileCode::CFileCode(const CNcbiRegistry& registry,
                     const string& baseName, const string& headerPrefix)
    : m_Registry(registry)
{
    m_HeaderPrefix = headerPrefix;
    string s = headerPrefix + baseName;
    m_HeaderName = s + "_Base.hpp";
    m_UserHeaderName = s + ".hpp";
    m_CPPName = baseName + "_Base.cpp";
    for ( string::const_iterator i = baseName.begin();
          i != baseName.end();
          ++i ) {
        char c = *i;
        if ( c >= 'a' && c <= 'z' )
            c = c + ('A' - 'a');
        else if ( c < 'A' || c > 'Z' )
            c = '_';
        m_HeaderDefineName += c;
    }
    m_HeaderDefineName += "_BASE_HPP";
}

CFileCode::~CFileCode(void)
{
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
        return '<' + m_HeaderPrefix + s + ".hpp>";
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
    ofstream header((path + m_HeaderName).c_str());
    if ( !header ) {
        NcbiCerr << "Cannot create file: " << path << m_HeaderName << endl;
        return;
    }

    header <<
        "// This is generated file, don't modify" << endl <<
        "#ifndef " << m_HeaderDefineName << endl <<
        "#define " << m_HeaderDefineName << endl <<
        endl <<
        "// standard includes" << endl <<
        "#include <corelib/ncbistd.hpp>" << endl;

    // resolve parent classes
    for ( TClasses::const_iterator ci = m_Classes.begin();
          ci != m_Classes.end();
          ++ci) {
        CClassCode* cls = ci->second.get();
        cls->GetParentClass();
    }

    if ( !m_HPPIncludes.empty() ) {
        header << endl <<
            "// generated includes" << endl;
        for ( TIncludes::const_iterator stri = m_HPPIncludes.begin();
              stri != m_HPPIncludes.end();
              ++stri) {
            header <<
                "#include " << *stri << endl;
        }
    }

    CNamespace ns(header);
    
    if ( !m_ForwardDeclarations.empty() ) {
        header << endl <<
            "// forward declarations" << endl;
        for ( TForwards::const_iterator fi = m_ForwardDeclarations.begin();
              fi != m_ForwardDeclarations.end();
              ++fi) {
            ns.Set(fi->second);
            header <<
                "class " << fi->first << ';'<< endl;
        }
    }
    
    if ( !m_Classes.empty() ) {
        header << endl <<
            "// generated classes" << endl;
        for ( TClasses::const_iterator ci = m_Classes.begin();
              ci != m_Classes.end();
              ++ci) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateHPP(header);
        }
    }
    ns.End();
    
    header << endl <<
        "#endif // " << m_HeaderDefineName << endl;
}

void CFileCode::GenerateCPP(const string& path) const
{
    ofstream code((path + m_CPPName).c_str());
    if ( !code ) {
        NcbiCerr << "Cannot create file: " << path << m_CPPName << endl;
        return;
    }

    code <<
        "// This is generated file, don't modify" << endl <<
        endl <<
        "// standard includes" << endl <<
        "#include <serial/serial.hpp>" << endl <<
        endl <<
        "// generated includes" << endl <<
        "#include <" << m_UserHeaderName << ">" << endl;

    if ( !m_CPPIncludes.empty() ) {
        code << endl <<
            "// generated includes" << endl;
        for ( TIncludes::const_iterator stri = m_CPPIncludes.begin();
              stri != m_CPPIncludes.end();
              ++stri) {
            code <<
                "#include " << *stri << endl;
        }
    }

    CNamespace ns(code);
    
    if ( !m_Classes.empty() ) {
        code << endl <<
            "// generated classes" << endl;
        for ( TClasses::const_iterator ci = m_Classes.begin();
              ci != m_Classes.end();
              ++ci) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateCPP(code);
        }
    }
}

bool CFileCode::AddType(const ASNType* type)
{
/*
    _TRACE(m_CPPName << '(' << long(this) << ')' <<
           " AddType: " << type->name << '(' << long(type) << ')');
*/
    AutoPtr<CClassCode>& cls = m_Classes[type->name];
    if ( cls )
        return false;

    cls = new CClassCode(*this, type);

    GetClassCode(type);
    return true;
}

CClassCode* CFileCode::GetClassCode(const ASNType* type)
{
/*
    _TRACE(m_CPPName << '(' << long(this) << ')' <<
           " GetClassCode: " << type->name << '(' << long(type) << ')');
*/
    TClasses::const_iterator ci = m_Classes.find(type->name);
    if ( ci == m_Classes.end() )
        THROW1_TRACE(runtime_error, "cannot get class info for " + type->name);

    return ci->second.get();
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
        m_Out << endl <<
            "namespace " << m_Namespace << " {" << endl <<
            endl;
    }
}

void CNamespace::End(void)
{
    if ( !m_Namespace.empty() ) {
        m_Out << endl <<
            "};" << endl;
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

CClassCode::CClassCode(CFileCode& code, const ASNType* type)
    : m_Code(code), m_Type(type),
      m_Namespace(type->Namespace(code)),
      m_ClassName(type->ClassName(code)),
      m_Abstract(false)
{
    string include = type->GetVar(code, "_include");
    if ( !include.empty() )
        code.AddHPPInclude(include);
    type->GenerateCode(*this);
    m_HPP << endl << '\0';
    m_CPP << endl << '\0';
}

CClassCode::~CClassCode(void)
{
}

const ASNType* CClassCode::GetParentType(void) const
{
    const ASNType* parent = GetType()->ParentType(*this);
    if ( !parent ) {
        switch ( GetType()->choices.size() ) {
        case 0:
            // no parent type
            return 0;
        case 1:
            parent = *(GetType()->choices.begin());
            break;
        default:
            ERR_POST("more then one parent of type: " + GetType()->name);
            return 0;
        }
    }
    if ( parent ) {
        m_Code.AddHPPInclude(parent->FileName(*this));
    }
    return parent;
}

string CClassCode::GetParentClass(void) const
{
    const ASNType* parent = GetParentType();
    if ( parent )
        return parent->ClassName(*this);
    else
        return GetType()->ParentClass(*this);
}

CNcbiOstream& CClassCode::GenerateHPP(CNcbiOstream& header) const
{
    header <<
        "class " << GetClassName() << "_Base";
    if ( !GetParentClass().empty() )
        header << " : public " << GetParentClass();
    header << endl <<
        '{' << endl <<
        "public:" << endl <<
        "    " << GetClassName() << "_Base();" << endl <<
        "    " << (IsAbstract()? "virtual ": "") << '~' <<
        GetClassName() << "_Base();" << endl <<
        endl <<
        "    static const NCBI_NS_NCBI::CTypeInfo* GetTypeInfo(void);" <<
        endl;
    for ( TEnums::const_iterator ei = m_Enums.begin();
          ei != m_Enums.end(); ++ei ) {
        header << endl <<
            *ei << endl;
    }
    header << endl <<
        "private:" << endl <<
        "    friend class " << GetClassName() << ';' << endl <<
        endl <<
        m_HPP.str() <<
        "};" << endl;
    return header;
}

CNcbiOstream& CClassCode::GenerateCPP(CNcbiOstream& code) const
{
    code <<
        GetClassName() << "_Base::" <<
        GetClassName() << "_Base()" << endl <<
        '{' << endl <<
        '}' << endl <<
        endl <<
        GetClassName() << "_Base::~" <<
        GetClassName() << "_Base()" << endl <<
        '{' << endl <<
        '}' << endl <<
        endl;
    if ( IsAbstract() )
        code << "BEGIN_ABSTRACT_CLASS_INFO3(\"";
    else
        code << "BEGIN_CLASS_INFO3(\"";
    code << GetType()->name << "\", " <<
        GetClassName() << ", " << GetClassName() << "_Base)" << endl <<
        '{' << endl;
    if ( GetParentType() ) {
        code << "    SET_PARENT_CLASS(" <<
            GetParentClass() << ")->SetOptional();" << endl;
    }
    code << endl <<
        m_CPP.str() <<
        '}' << endl <<
        "END_CLASS_INFO" << endl;
    return code;
}
