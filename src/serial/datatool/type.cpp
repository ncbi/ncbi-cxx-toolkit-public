#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/enumerated.hpp>
#include <serial/choice.hpp>
#include <serial/autoptrinfo.hpp>
#include <algorithm>
#include "type.hpp"
#include "value.hpp"
#include "module.hpp"
#include "moduleset.hpp"
#include "code.hpp"
#include "exceptions.hpp"

TTypeInfo CAnyTypeSource::GetTypeInfo(void)
{
    TTypeInfo typeInfo = m_Type->GetTypeInfo();
    if ( typeInfo->GetSize() > sizeof(AnyType) )
        typeInfo = new CAutoPointerTypeInfo(typeInfo);
    return typeInfo;
}

class CContainerTypeInfo : public CClassInfoTmpl
{
public:
    CContainerTypeInfo(const string& name, size_t size)
        : CClassInfoTmpl(name, typeid(void), MemberOffset(size)),
          m_Size(size)
        {
        }

    TObjectPtr Create(void) const
        {
            return new AnyType[m_Size];
        }

    static size_t MemberOffset(size_t index)
        {
            return sizeof(AnyType) * index;
        }

private:
    size_t m_Size;
};

inline
string replace(string s, char from, char to)
{
    replace(s.begin(), s.end(), from, to);
    return s;
}

inline
string Identifier(const string& typeName)
{
    string s;
    s.reserve(typeName.size());
    string::const_iterator i = typeName.begin();
    if ( i != typeName.end() ) {
        s += toupper(*i);
        while ( ++i != typeName.end() ) {
            char c = *i;
            if ( c == '-' || c == '.' )
                c = '_';
            s += c;
        }
    }
    return s;
}

static
string GetTemplateHeader(const string& tmpl)
{
    if ( tmpl == "multiset" )
        return "<set>";
    if ( tmpl == "multimap" )
        return "<map>";
    if ( tmpl == "auto_ptr" )
        return "<memory>";
    if ( tmpl == "AutoPtr" )
        return "<corelib/autoptr.hpp>";
    return '<' + tmpl + '>';
}

static
bool IsSimpleTemplate(const string& tmpl)
{
    if ( tmpl == "AutoPtr" )
        return true;
    return false;
}

static
string GetTemplateNamespace(const string& tmpl)
{
    if ( tmpl == "AutoPtr" )
        return "NCBI_NS_NCBI";
    return "NCBI_NS_STD";
}

static
string GetTemplateMacro(const string& tmpl)
{
    if ( tmpl == "auto_ptr" )
        return "STL_CHOICE_auto_ptr";
    return "STL_" + tmpl;
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

inline
void CTypeStrings::AddMember(CClassCode& code,
                             const string& member) const
{
    x_AddMember(code, "NCBI_NS_NCBI::NcbiEmptyString", member);
}

inline
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

    if ( type == eComplexType ) {
        code.CPP() <<
            "    ADD_N_M(" << name << ", " << member << ", " << macro << ')';
    }
    else {
        code.CPP() <<
            "    ADD_N_STD_M(" << name << ", " << member << ')';
    }
}

ASNType::ASNType(const CDataTypeContext& c)
    : main(false), exported(false), context(c), inSet(false)
{
/*
    _TRACE(c.GetFilePos().ToString() << ": ASNType() in [" <<
           c.GetConfigPos().GetSection() << ']' <<
           c.GetConfigPos().GetKeyPrefix());
*/
}

/*
ASNType::ASNType(const CDataTypeContext& c, const string& n)
    : name(n), main(false), exported(false), context(c), inSet(false)
{

    _TRACE(c.GetFilePos().ToString() << ": ASNType(" << name << ") in [" <<
           c.GetConfigPos().GetSection() << ']' <<
           c.GetConfigPos().GetKeyPrefix());

}
*/

ASNType::~ASNType()
{
}

void ASNType::Warning(const string& mess) const
{
    cerr << context.GetFilePos() << ": " << mess << endl;
}

bool ASNType::Check(void)
{
    return true;
}

ostream& ASNType::NewLine(ostream& out, int indent)
{
    out << endl;
    for ( int i = 0; i < indent; ++i )
        out << "  ";
    return out;
}

const string& ASNType::GetVar(const CNcbiRegistry& registry,
                              const string& value) const
{
    return context.GetConfigPos().GetVar(registry, GetModule().name, value);
}

string ASNType::IdName(void) const
{
    const CConfigPosition& config = context.GetConfigPos();
    if ( config.GetKeyPrefix().empty() )
        return config.GetSection();
    else
        return config.GetSection() + '.' + config.GetKeyPrefix();
}

string ASNType::ClassName(const CNcbiRegistry& registry) const
{
    const string& s = GetVar(registry, "_class");
    if ( !s.empty() )
        return s;
    /*
    if ( !name.empty() )
        return Identifier(name);
    */
    return Identifier(IdName());
}

string ASNType::FileName(const CNcbiRegistry& registry) const
{
    const string& s = GetVar(registry, "_file");
    if ( !s.empty() )
        return s;
    return Identifier(IdName());
}

string ASNType::Namespace(const CNcbiRegistry& registry) const
{
    return GetVar(registry, "_namespace");
}

string ASNType::ParentClass(const CNcbiRegistry& registry) const
{
    return GetVar(registry, "_parent_class");
}

const ASNType* ASNType::ParentType(const CNcbiRegistry& registry) const
{
    const string& parentName = GetVar(registry, "_parent");
    if ( parentName.empty() )
        return 0;

    return GetModule().Resolve(parentName);
}

ASNType* ASNType::Resolve(void)
{
    return this;
}

const ASNType* ASNType::Resolve(void) const
{
    return this;
}

TTypeInfo ASNType::GetTypeInfo(void)
{
    if ( !m_CreatedTypeInfo ) {
        m_CreatedTypeInfo = CreateTypeInfo();
        if ( !m_CreatedTypeInfo )
            THROW1_TRACE(runtime_error, "type undefined");
    }
    return m_CreatedTypeInfo.get();
}

void ASNType::GenerateCode(CClassCode& code) const
{
    // by default, generate implicit class with one data member
    string memberName = GetVar(code, "_member");
    if ( memberName.empty() ) {
        memberName = "m_" + Identifier(IdName());
    }
    code.CPP() <<
        "    info->SetImplicit();" << endl;
    CTypeStrings tType;
    GetCType(tType, code);
    tType.AddMember(code, memberName);
    code.CPP() << ';' << endl;
}

void ASNType::GetCType(CTypeStrings& , CClassCode& ) const
{
    THROW1_TRACE(runtime_error,
                 context.GetFilePos().ToString() + ": C++ type undefined");
}

CTypeInfo* ASNType::CreateTypeInfo(void)
{
    return 0;
}

#define CheckValueType(value, type, name) do{ \
if ( dynamic_cast<const type*>(&(value)) == 0 ) { \
    (value).Warning(name " value expected"); return false; \
} } while(0)



ASNFixedType::ASNFixedType(const CDataTypeContext& context, const string& kw)
    : ASNType(context), keyword(kw)
{
}

ostream& ASNFixedType::Print(ostream& out, int ) const
{
    return out << keyword;
}

void ASNFixedType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    string type = GetVar(code, "_type");
    if ( type.empty() )
        type = GetDefaultCType();
    tType.SetStd(type);
}

string ASNFixedType::GetDefaultCType(void) const
{
    CNcbiOstrstream msg;
    msg << typeid(*this).name() << ": Cannot generate C++ type: ";
    Print(msg, 0);
    THROW1_TRACE(runtime_error, string(msg.str(), msg.pcount()));
}

ASNNullType::ASNNullType(const CDataTypeContext& context)
    : ASNFixedType(context, "NULL")
{
}

bool ASNNullType::CheckValue(const ASNValue& value)
{
    CheckValueType(value, ASNNullValue, "NULL");
    return true;
}

TObjectPtr ASNNullType::CreateDefault(const ASNValue& )
{
    THROW1_TRACE(runtime_error, "NULL type not implemented");
}

TTypeInfo ASNNullType::GetTypeInfo(void)
{
    THROW1_TRACE(runtime_error, "NULL type not implemented");
}

void ASNNullType::GenerateCode(CClassCode& ) const
{
}

ASNBooleanType::ASNBooleanType(const CDataTypeContext& context)
    : ASNFixedType(context, "BOOLEAN")
{
}

bool ASNBooleanType::CheckValue(const ASNValue& value)
{
    CheckValueType(value, ASNBoolValue, "BOOLEAN");
    return true;
}

TObjectPtr ASNBooleanType::CreateDefault(const ASNValue& value)
{
    return new bool(dynamic_cast<const ASNBoolValue&>(value).value);
}

TTypeInfo ASNBooleanType::GetTypeInfo(void)
{
    return CStdTypeInfo<bool>::GetTypeInfo();
}

string ASNBooleanType::GetDefaultCType(void) const
{
    return "bool";
}

ASNRealType::ASNRealType(const CDataTypeContext& context)
    : ASNFixedType(context, "REAL")
{
}

bool ASNRealType::CheckValue(const ASNValue& value)
{
    const ASNBlockValue* block = dynamic_cast<const ASNBlockValue*>(&value);
    if ( !block ) {
        value.Warning("REAL value expected");
        return false;
    }
    if ( block->values.size() != 3 ) {
        value.Warning("wrong number of elements in REAL value");
        return false;
    }
    for ( ASNBlockValue::TValues::const_iterator i = block->values.begin();
          i != block->values.end(); ++i ) {
        CheckValueType(**i, ASNIntegerValue, "INTEGER");
    }
    return true;
}

TObjectPtr ASNRealType::CreateDefault(const ASNValue& )
{
    THROW1_TRACE(runtime_error, "REAL default not implemented");
}

TTypeInfo ASNRealType::GetTypeInfo(void)
{
    return CStdTypeInfo<double>::GetTypeInfo();
}

string ASNRealType::GetDefaultCType(void) const
{
    return "double";
}

ASNVisibleStringType::ASNVisibleStringType(const CDataTypeContext& context)
    : ASNFixedType(context, "VisibleString")
{
}

ASNVisibleStringType::ASNVisibleStringType(const CDataTypeContext& context,
                                           const string& kw)
    : ASNFixedType(context, kw)
{
}

bool ASNVisibleStringType::CheckValue(const ASNValue& value)
{
    CheckValueType(value, ASNStringValue, "string");
    return true;
}

TObjectPtr ASNVisibleStringType::CreateDefault(const ASNValue& value)
{
    return new string(dynamic_cast<const ASNStringValue&>(value).value);
}

TTypeInfo ASNVisibleStringType::GetTypeInfo(void)
{
    return CAutoPointerTypeInfo::GetTypeInfo(
        CStdTypeInfo<string>::GetTypeInfo());
}

string ASNVisibleStringType::GetDefaultCType(void) const
{
    return "NCBI_NS_STD::string";
}

ASNStringStoreType::ASNStringStoreType(const CDataTypeContext& context)
    : ASNVisibleStringType(context, "StringStore")
{
}

ASNBitStringType::ASNBitStringType(const CDataTypeContext& context)
    : ASNFixedType(context, "BIT STRING")
{
}

bool ASNBitStringType::CheckValue(const ASNValue& value)
{
    CheckValueType(value, ASNBitStringValue, "BIT STRING");
    return true;
}

TObjectPtr ASNBitStringType::CreateDefault(const ASNValue& )
{
    THROW1_TRACE(runtime_error, "BIT STRING default not implemented");
}

ASNOctetStringType::ASNOctetStringType(const CDataTypeContext& context)
    : ASNFixedType(context, "OCTET STRING")
{
}

bool ASNOctetStringType::CheckValue(const ASNValue& value)
{
    CheckValueType(value, ASNBitStringValue, "OCTET STRING");
    return true;
}

TObjectPtr ASNOctetStringType::CreateDefault(const ASNValue& )
{
    THROW1_TRACE(runtime_error, "OCTET STRING default not implemented");
}

void ASNOctetStringType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    string charType = GetVar(code, "_char");
    if ( charType.empty() )
        charType = "char";
    tType.SetComplex("NCBI_NS_STD::vector<" + charType + '>',
                     "STL_CHAR_vector, (" + charType + ')');
    tType.AddHPPInclude("<vector>");
}

ASNEnumeratedType::ASNEnumeratedType(const CDataTypeContext& context,
                                     const string& kw)
    : ASNType(context), keyword(kw), isInteger(kw == "INTEGER")
{
}

void ASNEnumeratedType::AddValue(const string& valueName, int value)
{
    values.push_back(TValues::value_type(valueName, value));
}

ostream& ASNEnumeratedType::Print(ostream& out, int indent) const
{
    out << Keyword() << " {";
    indent++;
    for ( TValues::const_iterator i = values.begin();
          i != values.end(); ++i ) {
        if ( i != values.begin() )
            out << ',';
        NewLine(out, indent);
        out << i->id << " (" << i->value << ")";
    }
    NewLine(out, indent - 1);
    return out << "}";
}

bool ASNEnumeratedType::CheckValue(const ASNValue& value)
{
    const ASNIdValue* id = dynamic_cast<const ASNIdValue*>(&value);
    if ( id ) {
        for ( TValues::const_iterator i = values.begin();
              i != values.end(); ++i ) {
            if ( i->id == id->id )
                return true;
        }
        value.Warning("illegal ENUMERATED value: " + id->id);
        return false;
    }

    if ( !IsInteger() ) {
        value.Warning("ENUMERATED value expected");
        return false;
    }

    const ASNIntegerValue* intValue =
        dynamic_cast<const ASNIntegerValue*>(&value);
    if ( !intValue ) {
        value.Warning("ENUMERATED or INTEGER value expected");
        return false;
    }

    for ( TValues::const_iterator i = values.begin();
          i != values.end(); ++i ) {
        if ( i->value == intValue->value )
            return true;
    }
    
    value.Warning("illegal INTEGER value: " + intValue->value);
    return false;
}

TObjectPtr ASNEnumeratedType::CreateDefault(const ASNValue& value)
{
    const ASNIdValue* id = dynamic_cast<const ASNIdValue*>(&value);
    if ( id == 0 ) {
        return new int(dynamic_cast<const ASNIntegerValue&>(value).value);
    }
    for ( TValues::const_iterator i = values.begin();
          i != values.end(); ++i ) {
        if ( i->id == id->id )
            return new int(i->value);
    }
    value.Warning("illegal ENUMERATED value: " + id->id);
    return 0;
}

CTypeInfo* ASNEnumeratedType::CreateTypeInfo(void)
{
    AutoPtr<CEnumeratedTypeInfo> info(new CEnumeratedTypeInfo(IdName(),
                                                              IsInteger()));
    for ( TValues::const_iterator i = values.begin();
          i != values.end(); ++i ) {
        info->AddValue(i->id, i->value);
    }
    return info.release();
}

void ASNEnumeratedType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    CNcbiOstrstream b;
    string type = GetVar(code, "_type");
    string enumName;
    if ( type.empty() ) {
        // generate enum name from ASN type or field name
        const string& keyPrefix = context.GetConfigPos().GetKeyPrefix();
        if ( !keyPrefix.empty() ) {
            // get field name from key (last part after dot)
            SIZE_TYPE dot = keyPrefix.rfind('.');
            if ( dot == NPOS ) {
                // no dot -> key name is field name
                enumName = 'E' + Identifier(keyPrefix);
            }
            else {
                // get field name
                enumName = 'E' + Identifier(keyPrefix.substr(dot + 1));
            }
        }
        else {
            enumName = 'E' + Identifier(context.GetConfigPos().GetSection());
        }
        // make C++ type name
        if ( IsInteger() )
            type = "int";
        else
            type = enumName;
    }
    else {
        enumName = type;
    }
    b << "    enum " << enumName << " {";
    for ( TValues::const_iterator i = values.begin();
          i != values.end(); ++i ) {
        if ( i != values.begin() )
            b << ',';
        b << endl <<
            "        " << 'e' + Identifier(i->id) << " = " << i->value;
    }
    b << endl <<
        "    };" << endl;
    code.AddEnum(string(b.str(), b.pcount()));
    tType.SetStd(type);
}

ASNIntegerType::ASNIntegerType(const CDataTypeContext& context)
    : ASNFixedType(context, "INTEGER")
{
}

bool ASNIntegerType::CheckValue(const ASNValue& value)
{
    CheckValueType(value, ASNIntegerValue, "INTEGER");
    return true;
}

TObjectPtr ASNIntegerType::CreateDefault(const ASNValue& value)
{
    return new int(dynamic_cast<const ASNIntegerValue&>(value).value);
}

TTypeInfo ASNIntegerType::GetTypeInfo(void)
{
    return CStdTypeInfo<TInteger>::GetTypeInfo();
}

string ASNIntegerType::GetDefaultCType(void) const
{
    return "int";
}

ASNUserType::ASNUserType(const CDataTypeContext& context, const string& n)
    : ASNType(context), userTypeName(n)
{
}

ostream& ASNUserType::Print(ostream& out, int ) const
{
    return out << userTypeName;
}

bool ASNUserType::Check(void)
{
    return GetModule().FindType(userTypeName);
}

bool ASNUserType::CheckValue(const ASNValue& value)
{
    try {
        return Resolve()->CheckValue(value);
    }
    catch (CTypeNotFound& exc) {
        Warning(exc.what());
        return false;
    }
}

TTypeInfo ASNUserType::GetTypeInfo(void)
{
    return Resolve()->GetTypeInfo();
}

TObjectPtr ASNUserType::CreateDefault(const ASNValue& value)
{
    return Resolve()->CreateDefault(value);
}

void ASNUserType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    const ASNType* userType = Resolve();
    if ( dynamic_cast<const ASNChoiceType*>(userType) != 0 ) {
        userType->GetCType(tType, code);
        return;
    }
    string className = userType->ClassName(code);
    tType.SetClass(className);
    tType.AddHPPInclude(userType->FileName(code));
    tType.AddForwardDeclaration(className, userType->Namespace(code));

    string memberType = GetVar(code, "_type");
    if ( !memberType.empty() ) {
        if ( memberType == "*" ) {
            tType.ToSimple();
        }
        else {
            tType.AddHPPInclude(GetTemplateHeader(memberType));
            tType.SetComplex(GetTemplateNamespace(memberType) + "::" + memberType,
                             GetTemplateMacro(memberType), tType);
            if ( IsSimpleTemplate(memberType) )
                tType.type = tType.ePointerType;
        }
    }
}

const ASNType* ASNUserType::Resolve(void) const
{
    return GetModule().Resolve(userTypeName);
}

ASNType* ASNUserType::Resolve(void)
{
    return GetModule().Resolve(userTypeName);
}

ASNOfType::ASNOfType(const CDataTypeContext& context, const string& kw)
    : ASNType(context), keyword(kw)
{
}

ostream& ASNOfType::Print(ostream& out, int indent) const
{
    out << keyword << " OF ";
    return type->Print(out, indent);
}

bool ASNOfType::Check(void)
{
    type->Resolve()->inSet = true;
    return type->Check();
}

bool ASNOfType::CheckValue(const ASNValue& value)
{
    const ASNBlockValue* block = dynamic_cast<const ASNBlockValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    bool ok = true;
    for ( ASNBlockValue::TValues::const_iterator i = block->values.begin();
          i != block->values.end(); ++i ) {
        if ( !type->CheckValue(**i) )
            ok = false;
    }
    return ok;
}

TObjectPtr ASNOfType::CreateDefault(const ASNValue& )
{
    THROW1_TRACE(runtime_error, "SET/SEQUENCE OF default not implemented");
}

ASNSetOfType::ASNSetOfType(const CDataTypeContext& context)
    : ASNOfType(context, "SET")
{
}

CTypeInfo* ASNSetOfType::CreateTypeInfo(void)
{
    CStlClassInfoList<AnyType>* l =
        new CStlClassInfoList<AnyType>(new CAnyTypeSource(type.get()));
    l->SetRandomOrder();
    return new CAutoPointerTypeInfo(l);
}

void ASNSetOfType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    string templ = GetVar(code, "_type");
    const ASNSequenceType* seq =
        dynamic_cast<const ASNSequenceType*>(type.get());
    if ( seq && seq->members.size() == 2 ) {
        CTypeStrings tKey;
        seq->members.front()->type->GetCType(tKey, code);
        if ( tKey.type == tKey.eStdType ) {
            CTypeStrings tValue;
            seq->members.back()->type->GetCType(tValue, code);
            tValue.ToSimple();
            if ( templ.empty() )
                templ = "multimap";

            tType.AddHPPInclude(GetTemplateHeader(templ));
            tType.SetComplex(GetTemplateNamespace(templ) + "::" + templ,
                             GetTemplateMacro(templ), tKey, tValue);
            return;
        }
    }
    CTypeStrings tData;
    type->GetCType(tData, code);
    tData.ToSimple();
    if ( templ.empty() )
        templ = "multiset";
    tType.AddHPPInclude(GetTemplateHeader(templ));
    tType.SetComplex(GetTemplateNamespace(templ) + "::" + templ,
                     GetTemplateMacro(templ), tData);
}

ASNSequenceOfType::ASNSequenceOfType(const CDataTypeContext& context)
    : ASNOfType(context, "SEQUENCE")
{
}

CTypeInfo* ASNSequenceOfType::CreateTypeInfo(void)
{
    return new CAutoPointerTypeInfo(new CStlClassInfoList<AnyType>(new CAnyTypeSource(type.get())));
}

void ASNSequenceOfType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    string templ = GetVar(code, "_type");
    CTypeStrings tData;
    type->GetCType(tData, code);
    tData.ToSimple();
    if ( templ.empty() )
        templ = "list";
    tType.AddHPPInclude(GetTemplateHeader(templ));
    tType.SetComplex(GetTemplateNamespace(templ) + "::" + templ,
                     GetTemplateMacro(templ), tData);
}

ASNMemberContainerType::ASNMemberContainerType(const CDataTypeContext& context,
                                               const string& kw)
    : ASNType(context), keyword(kw)
{
}

ostream& ASNMemberContainerType::Print(ostream& out, int indent) const
{
    out << keyword << " {";
    indent++;
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        if ( i != members.begin() )
            out << ',';
        NewLine(out, indent);
        (*i)->Print(out, indent);
    }
    NewLine(out, indent - 1);
    return out << "}";
}

bool ASNMemberContainerType::Check(void)
{
    bool ok = true;
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        if ( !(*i)->Check() )
            ok = false;
    }
    return ok;
}

TObjectPtr ASNMemberContainerType::CreateDefault(const ASNValue& )
{
    THROW1_TRACE(runtime_error, keyword + " default not implemented");
}

ASNContainerType::ASNContainerType(const CDataTypeContext& context,
                                   const string& kw)
    : CParent(context, kw)
{
}

CTypeInfo* ASNContainerType::CreateTypeInfo(void)
{
    return CreateClassInfo();
}

CClassInfoTmpl* ASNContainerType::CreateClassInfo(void)
{
    auto_ptr<CClassInfoTmpl> typeInfo(new CContainerTypeInfo(IdName(),
                                                             members.size()));
    int index = 0;
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i, ++index ) {
        ASNMember* member = i->get();
        CMemberInfo* memberInfo =
            typeInfo->AddMember(member->name,
                new CRealMemberInfo(CContainerTypeInfo::MemberOffset(index),
                                    new CAnyTypeSource(member->type.get())));
        if ( member->Optional() )
            memberInfo->SetOptional();
        else if ( member->defaultValue )
            memberInfo->SetDefault(member->type->
                                   CreateDefault(*member->defaultValue));
    }
    return typeInfo.release();
}

void ASNContainerType::GenerateCode(CClassCode& code) const
{
    for ( TMembers::const_iterator i = members.begin();
          i != members.end();
          ++i ) {
        const ASNMember* m = i->get();
        string fieldName = Identifier(m->name);
        if ( fieldName.empty() ) {
            continue;
        }
        CTypeStrings tType;
        m->type->GetCType(tType, code);
        tType.AddMember(code, m->name, "m_" + fieldName);
        if ( m->defaultValue )
            code.CPP() << "->SetDefault(...)";
        else if ( m->optional )
            code.CPP() << "->SetOptional()";
        code.CPP() << ';' << endl;
    }
}

ASNSetType::ASNSetType(const CDataTypeContext& context)
    : CParent(context, "SET")
{
}

bool ASNSetType::CheckValue(const ASNValue& value)
{
    const ASNBlockValue* block = dynamic_cast<const ASNBlockValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }

    map<string, const ASNMember*> mms;
    for ( TMembers::const_iterator m = members.begin();
          m != members.end(); ++m ) {
        mms[m->get()->name] = m->get();
    }

    for ( ASNBlockValue::TValues::const_iterator v = block->values.begin();
          v != block->values.end(); ++v ) {
        const ASNNamedValue* currvalue =
            dynamic_cast<const ASNNamedValue*>(v->get());
        if ( !currvalue ) {
            v->get()->Warning("named value expected");
            return false;
        }
        map<string, const ASNMember*>::iterator member =
            mms.find(currvalue->name);
        if ( member == mms.end() ) {
            currvalue->Warning("unexpected member");
            return false;
        }
        if ( !member->second->type->CheckValue(*currvalue->value) ) {
            return false;
        }
        mms.erase(member);
    }
    
    for ( map<string, const ASNMember*>::const_iterator member = mms.begin();
          member != mms.end(); ++member ) {
        if ( !member->second->Optional() ) {
            value.Warning(member->first + " member expected");
            return false;
        }
    }
    return true;
}

CClassInfoTmpl* ASNSetType::CreateClassInfo(void)
{
    return CParent::CreateClassInfo()->SetRandomOrder();
}

ASNSequenceType::ASNSequenceType(const CDataTypeContext& context)
    : CParent(context, "SEQUENCE")
{
}

bool ASNSequenceType::CheckValue(const ASNValue& value)
{
    const ASNBlockValue* block = dynamic_cast<const ASNBlockValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    TMembers::const_iterator member = members.begin();
    ASNBlockValue::TValues::const_iterator cvalue = block->values.begin();
    while ( cvalue != block->values.end() ) {
        const ASNNamedValue* currvalue =
            dynamic_cast<const ASNNamedValue*>(cvalue->get());
        if ( !currvalue ) {
            cvalue->get()->Warning("named value expected");
            return false;
        }
        for (;;) {
            if ( member == members.end() ) {
                currvalue->Warning("unexpected value");
                return false;
            }
            if ( (*member)->name == currvalue->name )
                break;
            if ( !(*member)->Optional() ) {
                currvalue->value->Warning((*member)->name + " member expected");
                return false;
            }
            ++member;
        }
        if ( !(*member)->type->CheckValue(*currvalue->value) ) {
            return false;
        }
        ++member;
        ++cvalue;
    }
    while ( member != members.end() ) {
        if ( !(*member)->Optional() ) {
            value.Warning((*member)->name + " member expected");
            return false;
        }
    }
    return true;
}

ASNChoiceType::ASNChoiceType(const CDataTypeContext& context)
    : CParent(context, "CHOICE")
{
}

bool ASNChoiceType::Check(void)
{
    bool ok = true;
    for ( TMembers::const_iterator m = members.begin();
          m != members.end(); ++m ) {
        if ( !(*m)->Check() ) {
            ok = false;
        }
        else {
            (*m)->type->Resolve()->choices.insert(this);
        }
    }
    return ok;
}

bool ASNChoiceType::CheckValue(const ASNValue& value)
{
    const ASNNamedValue* choice = dynamic_cast<const ASNNamedValue*>(&value);
    if ( !choice ) {
        value.Warning("CHOICE value expected");
        return false;
    }
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        if ( (*i)->name == choice->name )
            return (*i)->type->CheckValue(*choice->value);
    }
    return false;
}

CTypeInfo* ASNChoiceType::CreateTypeInfo(void)
{
    auto_ptr<CChoiceTypeInfoBase> typeInfo(new CChoiceTypeInfoTmpl<AnyType>(IdName()));
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        ASNMember* member = i->get();
        typeInfo->AddVariant(member->name, new CAnyTypeSource(member->type.get()));
    }
    return new CAutoPointerTypeInfo(typeInfo.release());
}

void ASNChoiceType::GenerateCode(CClassCode& code) const
{
    code.SetAbstract();
    for ( TMembers::const_iterator i = members.begin();
          i != members.end();
          ++i ) {
        const ASNMember* m = i->get();
        string fieldName = Identifier(m->name);
        if ( fieldName.empty() ) {
            continue;
        }
        ASNType* variant = m->type->Resolve();
        code.AddCPPInclude(variant->FileName(code));
        code.CPP() <<
            "    ADD_SUB_CLASS2(\"" << m->name << "\", " <<
            variant->ClassName(code) << ");" << endl;
    }
}

void ASNChoiceType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    string className = ClassName(code);
    tType.SetClass(className);
    tType.AddHPPInclude(FileName(code));
    tType.AddForwardDeclaration(className, Namespace(code));
    tType.AddHPPInclude(GetTemplateHeader("memory"));
    tType.SetComplex("NCBI_NS_STD::auto_ptr", "STL_CHOICE_auto_ptr", tType);
}

ASNMember::ASNMember(void)
    : optional(false)
{
}

ASNMember::~ASNMember(void)
{
}

ostream& ASNMember::Print(ostream& out, int indent) const
{
    out << name << ' ';
    type->Print(out, indent);
    if ( optional )
        out << " OPTIONAL";
    if ( defaultValue )
        defaultValue->Print(out << " DEFAULT ", indent + 1);
    return out;
}

bool ASNMember::Check(void)
{
    if ( !type->Check() )
        return false;
    if ( !defaultValue )
        return true;
    return type->CheckValue(*defaultValue);
}
