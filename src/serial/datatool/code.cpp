//#include <corelib/ncbireg.hpp>
#include "code.hpp"
#include "type.hpp"

CFileCode::CFileCode(const CNcbiRegistry& registry,
                     const string& baseName, const string& headerPrefix)
    : m_Registry(registry), m_BaseName(baseName), m_HeaderPrefix(headerPrefix)
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
        else if ( c < 'A' || c > 'Z' )
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
        "// This is generated file, don't modify" << endl <<
        "#ifndef " << hppDefine << endl <<
        "#define " << hppDefine << endl <<
        endl <<
        "// standard includes" << endl <<
        "#include <corelib/ncbistd.hpp>" << endl;

    // collect parent classes
    for ( TClasses::const_iterator ci = m_Classes.begin();
          ci != m_Classes.end();
          ++ci) {
        ci->second->GetParentType();
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
        "#endif // " << hppDefine << endl;
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
        "// This is generated file, don't modify" << endl <<
        endl <<
        "// standard includes" << endl <<
        "#include <serial/serial.hpp>" << endl <<
        endl <<
        "// generated includes" << endl <<
        "#include <" << GetUserHPPName() << ">" << endl;

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
        "// This is generated file, you may modify it freely" << endl <<
        "#ifndef " << hppDefine << endl <<
        "#define " << hppDefine << endl <<
        endl <<
        "// standard includes" << endl <<
        "#include <corelib/ncbistd.hpp>" << endl;

    header << endl <<
        "// generated includes" << endl <<
        "#include <" << GetHPPName() << '>' << endl;
    
    CNamespace ns(header);
    
    if ( !m_Classes.empty() ) {
        header << endl <<
            "// generated classes" << endl;
        for ( TClasses::const_iterator ci = m_Classes.begin();
              ci != m_Classes.end();
              ++ci) {
            CClassCode* cls = ci->second.get();
            ns.Set(cls->GetNamespace());
            cls->GenerateUserHPP(header);
        }
    }
    ns.End();
    
    header << endl <<
        "#endif // " << hppDefine << endl;
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
        "// This is generated file, you may modify it freely" << endl <<
        endl <<
        "// standard includes" << endl <<
        "#include <serial/serial.hpp>" << endl <<
        endl <<
        "// generated includes" << endl <<
        "#include <" << GetUserHPPName() << ">" << endl;

    CNamespace ns(code);
    
    if ( !m_Classes.empty() ) {
        code << endl <<
            "// generated classes" << endl;
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

bool CFileCode::AddType(const ASNType* type)
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
        m_Out << endl <<
            "namespace " << m_Namespace << " {" << endl <<
            endl;
    }
}

void CNamespace::End(void)
{
    if ( !m_Namespace.empty() ) {
        m_Out << endl <<
            "}" << endl << endl;
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

CClassCode::CClassCode(CFileCode& code,
                       const string& typeName, const ASNType* type)
    : m_Code(code), m_TypeName(typeName), m_Type(type),
      m_Abstract(false), m_NonClass(false)
{
    m_Namespace = type->Namespace(*this);
    m_ClassName = type->ClassName(*this);
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
    const ASNType* t = GetType();
    const ASNType* parent = t->ParentType(*this);
    if ( !parent ) {
        switch ( t->choices.size() ) {
        case 0:
            // no parent type
            return 0;
        case 1:
            parent = *(t->choices.begin());
            break;
        default:
            for ( set<const ASNChoiceType*>::const_iterator i = t->choices.begin();
                  i != t->choices.end(); ++i ) {
                ERR_POST("more then one parent of type: " + t->IdName() +
                         ": " + (*i)->IdName());
            }
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
    if ( IsNonClass() ) {
        // enum
        return header << m_HPP.str();
    }
    header <<
        "class " << GetClassName() << "_Base";
    if ( !GetParentClass().empty() )
        header << " : public " << GetParentClass();
    header << endl <<
        '{' << endl <<
        "public:" << endl <<
        "    " << GetClassName() << "_Base(void);" << endl <<
        "    " << (IsAbstract()? "virtual ": "") << '~' <<
        GetClassName() << "_Base(void);" << endl <<
        endl <<
        "    static const NCBI_NS_NCBI::CTypeInfo* GetTypeInfo(void);" <<
        endl;
    for ( TMethods::const_iterator mi = m_Methods.begin();
          mi != m_Methods.end(); ++mi ) {
        header << endl <<
            mi->first << endl;
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
    if ( IsNonClass() ) {
        // enum
        return code << m_CPP.str();
    }
    code <<
        GetClassName() << "_Base::" <<
        GetClassName() << "_Base(void)" << endl <<
        '{' << endl <<
        '}' << endl <<
        endl <<
        GetClassName() << "_Base::~" <<
        GetClassName() << "_Base(void)" << endl <<
        '{' << endl <<
        '}' << endl <<
        endl;
    for ( TMethods::const_iterator mi = m_Methods.begin();
          mi != m_Methods.end(); ++mi ) {
        code << endl <<
            mi->second << endl;
    }
    if ( IsAbstract() )
        code << "BEGIN_ABSTRACT_CLASS_INFO3(\"";
    else
        code << "BEGIN_CLASS_INFO3(\"";
    code << GetType()->IdName() << "\", " <<
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

CNcbiOstream& CClassCode::GenerateUserHPP(CNcbiOstream& header) const
{
    if ( IsNonClass() ) {
        // enum
        return header;
    }
    header <<
        "class " << GetClassName() << " : public " <<
        GetClassName() << "_Base" << endl <<
        '{' << endl <<
        "public:" << endl <<
        "    " << GetClassName() << "();" << endl <<
        "    " << '~' << GetClassName() << "();" << endl <<
        endl <<
        "};" << endl;
    return header;
}

CNcbiOstream& CClassCode::GenerateUserCPP(CNcbiOstream& code) const
{
    if ( IsNonClass() ) {
        // enum
        return code;
    }
    code <<
        GetClassName() << "::" <<
        GetClassName() << "()" << endl <<
        '{' << endl <<
        '}' << endl <<
        endl <<
        GetClassName() << "::~" <<
        GetClassName() << "()" << endl <<
        '{' << endl <<
        '}' << endl <<
        endl;
    return code;
}

inline
string CTypeStrings::GetRef(void) const
{
    switch ( type ) {
    case eStdType:
        return "STD, (" + cType + ')';
    case eClassType:
        return "CLASS, (" + cType + ')';
    default:
        return macro;
    }
}

void CTypeStrings::SetStd(const string& c)
{
    type = eStdType;
    cType = c;
}

void CTypeStrings::SetClass(const string& c)
{
    type = eClassType;
    cType = c;
}

void CTypeStrings::SetEnum(const string& c, const string& e)
{
    type = eEnumType;
    cType = c;
    macro = "ENUM, (" + c + ", " + e + ")";
}

void CTypeStrings::SetComplex(const string& c, const string& m)
{
    type = eComplexType;
    cType = c;
    macro = m;
}

void CTypeStrings::SetComplex(const string& c, const string& m,
                              const CTypeStrings& arg)
{
    string cc = c + "< " + arg.cType + " >";
    string mm = m + ", (" + arg.GetRef() + ')';
    AddIncludes(arg);
    type = eComplexType;
    cType = cc;
    macro = mm;
}

void CTypeStrings::SetComplex(const string& c, const string& m,
                              const CTypeStrings& arg1,
                              const CTypeStrings& arg2)
{
    string cc = c + "< " + arg1.cType + ", " + arg2.cType + " >";
    string mm = m + ", (" + arg1.GetRef() + ", " + arg2.GetRef() + ')';
    AddIncludes(arg1);
    AddIncludes(arg2);
    type = eComplexType;
    cType = cc;
    macro = mm;
}

template<class C>
inline
void insert(C& dst, const C& src)
{
	for ( typename C::const_iterator i = src.begin(); i != src.end(); ++i )
		dst.insert(*i);
}

void CTypeStrings::AddIncludes(const CTypeStrings& arg)
{
    insert(m_HPPIncludes, arg.m_HPPIncludes);
    insert(m_CPPIncludes, arg.m_CPPIncludes);
    insert(m_ForwardDeclarations, arg.m_ForwardDeclarations);
}

void CTypeStrings::ToSimple(void)
{
    switch (type ) {
    case eStdType:
    case ePointerType:
    case eEnumType:
        return;
    case eClassType:
    case eComplexType:
        macro = "POINTER, (" + GetRef() + ')';
        cType += '*';
        m_CPPIncludes = m_HPPIncludes;
        m_HPPIncludes.clear();
        type = ePointerType;
        break;
    }
}

void CTypeStrings::AddMember(CClassCode& code,
                             const string& member) const
{
    x_AddMember(code, "NCBI_NS_NCBI::NcbiEmptyString", member);
}

void CTypeStrings::AddMember(CClassCode& code,
                             const string& name, const string& member) const
{
    x_AddMember(code, '"' + name + '"', member);
}

void CTypeStrings::x_AddMember(CClassCode& code,
                               const string& name, const string& member) const
{
    code.AddForwardDeclarations(m_ForwardDeclarations);
    code.AddHPPIncludes(m_HPPIncludes);
    code.AddCPPIncludes(m_CPPIncludes);
    code.HPP() <<
        "    " << cType << ' ' << member << ';' << endl;

    if ( type == eStdType || type == eClassType ) {
        code.CPP() <<
            "    ADD_N_STD_M(" << name << ", " << member << ')';
    }
    else {
        code.CPP() <<
            "    ADD_N_M(" << name << ", " << member << ", " << macro << ')';
    }
}
