#include <type.hpp>
#include <value.hpp>
#include <module.hpp>
#include <moduleset.hpp>
#include <corelib/ncbireg.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/asntypes.hpp>
#include <serial/autoptrinfo.hpp>

inline
string replace(string s, char from, char to)
{
    replace(s.begin(), s.end(), from, to);
    return s;
}

inline
string Identifier(const string& typeName)
{
    return replace(typeName, '-', '_');
}

inline
string ClassName(const CNcbiRegistry& reg, const string& typeName)
{
    const string& className = reg.Get(typeName, "_class");
    if ( className.empty() )
        return Identifier(typeName);
    else
        return className;
}

CTypeStrings CTypeStrings::ToSimple(void) const
{
    if ( IsSimple() )
        return *this;
    else
        return CTypeStrings(cType + '*',
                            "POINTER, (" + GetRef() + ')');
}

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

void CTypeStrings::AddMember(CNcbiOstream& header, CNcbiOstream& code,
                             const string& member) const
{
    header <<
        "    " << cType << ' ' << member << ';' << endl;

    if ( type == eComplexType ) {
        code <<
            "    ADD_N_M(NcbiEmptyString, " << member << ", " << macro << ')';
    }
    else {
        code <<
            "    ADD_N_STD_M(NcbiEmptyString, " << member << ')';
    }
}

void CTypeStrings::AddMember(CNcbiOstream& header, CNcbiOstream& code,
                             const string& name, const string& member) const
{
    header <<
        "    " << cType << ' ' << member << ';' << endl;

    if ( type == eComplexType ) {
        code <<
            "    ADD_N_M(\"" << name << "\", " << member << ", " <<
            macro << ')';
    }
    else {
        code <<
            "    ADD_N_STD_M(\"" << name << "\", " << member << ')';
    }
}

class CTypeSource : public CTypeInfoSource
{
public:
    CTypeSource(ASNType* type)
        : m_Type(type)
        {
        }

    TTypeInfo GetTypeInfo(void)
        {
            return m_Type->GetTypeInfo();
        }

private:
    ASNType* m_Type;
};

union dataval {
    bool booleanValue;
    int integerValue;
    double realValue;
    void* pointerValue;
};

ASNType::ASNType(ASNModule& module, const string& n)
    : m_Module(module), line(0), name(n)
{
}

ASNType::~ASNType()
{
}

void ASNType::Warning(const string& mess) const
{
    if ( line > 0 )
        cerr << line;
    cerr << ": " << mess << endl;
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

TTypeInfo ASNType::GetTypeInfo(void)
{
    _TRACE(name << "::GetTypeInfo()");
    if ( !m_CreatedTypeInfo ) {
        if ( !(m_CreatedTypeInfo = CreateTypeInfo()) )
            THROW1_TRACE(runtime_error, "type undefined");
    }
    _TRACE(name << "::GetTypeInfo(): " << m_CreatedTypeInfo->GetName());
    return m_CreatedTypeInfo.get();
}

void ASNType::CollectUserTypes(set<string>& /*types*/) const
{
}

void ASNType::GenerateCode(ostream& header, ostream& code,
                           const string& section,
                           const CNcbiRegistry& def, const string& ) const
{
    // by default, generate implicit class with one data member
    string memberName = def.Get(section, "_member");
    if ( memberName.empty() ) {
        memberName = "m_" + Identifier(name);
    }

    code <<
        "    info->SetImplicit();" << endl;
    GetCType(def, section, "").AddMember(header, code, memberName);
    code << ';' << endl;
}

CTypeInfo* ASNType::CreateTypeInfo(void)
{
    return 0;
}

CTypeStrings ASNType::GetCType(const CNcbiRegistry& def,
                               const string& section,
                               const string& key) const
{
    string type = def.Get(section, key + "._type");
    if ( type.empty() )
        type = GetDefaultCType();
    return CTypeStrings(type);
}

string ASNType::GetDefaultCType(void) const
{
    strstream msg;
    msg << typeid(*this).name() << ": Cannot generate C++ type: ";
    Print(msg, 0);
    THROW1_TRACE(runtime_error, msg.str());
}

bool ASNType::SimpleType(void) const
{
    return false;
}

#define CheckValueType(value, type, name) do{ \
if ( dynamic_cast<const type*>(&(value)) == 0 ) { \
    (value).Warning(name " value expected"); return false; \
} } while(0)



ASNFixedType::ASNFixedType(ASNModule& module, const string& kw)
    : ASNType(module, kw), keyword(kw)
{
}

ostream& ASNFixedType::Print(ostream& out, int ) const
{
    return out << keyword;
}

bool ASNFixedType::SimpleType(void) const
{
    return true;
}

ASNNullType::ASNNullType(ASNModule& module)
    : ASNFixedType(module, "NULL")
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

void ASNNullType::GenerateCode(ostream& , ostream& , const string& ,
                           const CNcbiRegistry& , const string& ) const
{
}

ASNBooleanType::ASNBooleanType(ASNModule& module)
    : ASNFixedType(module, "BOOLEAN")
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

ASNRealType::ASNRealType(ASNModule& module)
    : ASNFixedType(module, "REAL")
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

ASNVisibleStringType::ASNVisibleStringType(ASNModule& module)
    : ASNFixedType(module, "VisibleString")
{
}

ASNVisibleStringType::ASNVisibleStringType(ASNModule& module, const string& kw)
    : ASNFixedType(module, kw)
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
    return "string";
}

ASNStringStoreType::ASNStringStoreType(ASNModule& module)
    : ASNVisibleStringType(module, "StringStore")
{
}

ASNBitStringType::ASNBitStringType(ASNModule& module)
    : ASNFixedType(module, "BIT STRING")
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

ASNOctetStringType::ASNOctetStringType(ASNModule& module)
    : ASNFixedType(module, "OCTET STRING")
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

string ASNOctetStringType::GetDefaultCType(void) const
{
    return "vector<char>";
}

ASNEnumeratedType::ASNEnumeratedType(ASNModule& module, const string& kw)
    : ASNType(module, kw), keyword(kw)
{
}

void ASNEnumeratedType::AddValue(const string& valueName, int value)
{
    values.push_back(TValues::value_type(valueName, value));
}

ostream& ASNEnumeratedType::Print(ostream& out, int indent) const
{
    out << keyword << " {";
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
    if ( id == 0 ) {
        CheckValueType(value, ASNIntegerValue, "ENUMERATED");
    }
    for ( TValues::const_iterator i = values.begin();
          i != values.end(); ++i ) {
        if ( i->id == id->id )
            return true;
    }
    value.Warning("illegal ENUMERATED value: " + id->id);
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
    AutoPtr<CEnumeratedTypeInfo> info(new CEnumeratedTypeInfo(name));
    for ( TValues::const_iterator i = values.begin();
          i != values.end(); ++i ) {
        info->AddValue(i->id, i->value);
    }
    return info.release();
}

string ASNEnumeratedType::GetDefaultCType(void) const
{
    return "int";
}

bool ASNEnumeratedType::SimpleType(void) const
{
    return true;
}

ASNIntegerType::ASNIntegerType(ASNModule& module)
    : ASNFixedType(module, "INTEGER")
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
    return CStdTypeInfo<int>::GetTypeInfo();
}

string ASNIntegerType::GetDefaultCType(void) const
{
    return "int";
}

ASNUserType::ASNUserType(ASNModule& module, const string& n)
    : ASNType(module, n), userTypeName(n)
{
}

ostream& ASNUserType::Print(ostream& out, int ) const
{
    return out << userTypeName;
}

bool ASNUserType::Check(void)
{
    const ASNModule::TypeInfo* typeInfo =  GetModule().FindType(userTypeName);
    if ( typeInfo == 0 ) {
        Warning("type " + userTypeName + " undefined");
        return false;
    }
    return true;
}

bool ASNUserType::CheckValue(const ASNValue& value)
{
    const ASNModule::TypeInfo* typeInfo =  GetModule().FindType(userTypeName);
    if ( !typeInfo || !typeInfo->type ) {
        Warning("type " + userTypeName + " undefined");
        return false;
    }
    return typeInfo->type->CheckValue(value);
}

void ASNUserType::CollectUserTypes(set<string>& types) const
{
    types.insert(userTypeName);
}

TTypeInfo ASNUserType::GetTypeInfo(void)
{
    return Resolve()->GetTypeInfo();
}

TObjectPtr ASNUserType::CreateDefault(const ASNValue& value)
{
    return Resolve()->CreateDefault(value);
}

CTypeStrings ASNUserType::GetCType(const CNcbiRegistry& def,
                                   const string& section,
                                   const string& key) const
{
    string className = def.Get(section, key + "._class");
    if ( className.empty() ) {
        className = def.Get(userTypeName, "_class");
        if ( className.empty() )
            className = Identifier(userTypeName);
    }
    return CTypeStrings(CTypeStrings::eClassType, className);
}

ASNType* ASNUserType::Resolve(void)
{
    const ASNModule::TypeInfo* typeInfo =  GetModule().FindType(userTypeName);
    if ( !typeInfo )
        THROW1_TRACE(runtime_error, "type " + userTypeName + " undefined");
    
    if ( typeInfo->type )
        return typeInfo->type;

    typeInfo = GetModule().moduleSet->FindType(typeInfo);
    if ( !typeInfo->type )
        THROW1_TRACE(runtime_error, "type " + userTypeName + " undefined");

    return typeInfo->type;
}

ASNOfType::ASNOfType(const string& kw, const AutoPtr<ASNType>& t)
    : ASNType(t->GetModule(), kw + " OF " + t->name), keyword(kw), type(t)
{
}

ostream& ASNOfType::Print(ostream& out, int indent) const
{
    out << keyword << " OF ";
    return type->Print(out, indent);
}

bool ASNOfType::Check(void)
{
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

void ASNOfType::CollectUserTypes(set<string>& types) const
{
    type->CollectUserTypes(types);
}

TObjectPtr ASNOfType::CreateDefault(const ASNValue& )
{
    THROW1_TRACE(runtime_error, "SET/SEQUENCE OF default not implemented");
}

ASNSetOfType::ASNSetOfType(const AutoPtr<ASNType>& type)
    : ASNOfType("SET", type)
{
}

CTypeInfo* ASNSetOfType::CreateTypeInfo(void)
{
    return new CSetOfTypeInfo(name, type->GetTypeInfo());
}

CTypeStrings ASNSetOfType::GetCType(const CNcbiRegistry& def,
                                    const string& section,
                                    const string& key) const
{
    const ASNSequenceType* seq =
        dynamic_cast<const ASNSequenceType*>(type.get());
    if ( seq && seq->members.size() == 2 ) {
        CTypeStrings tKey =
            seq->members.front()->type->GetCType(def, section,
                                                 key + ".key");
        if ( tKey.IsSimple() ) {
            CTypeStrings tValue = 
                seq->members.back()->type->GetCType(def, section,
                                                    key + ".value").ToSimple();
            return CTypeStrings(
                "map< " + tKey.cType + ", " + tValue.cType + " >",
                "STL_MAP, (" + tKey.GetRef() + ", " + tValue.GetRef() + ")");
        }
    }
    CTypeStrings tData = type->GetCType(def, section, key).ToSimple();
    return CTypeStrings("set< " + tData.cType + " >",
                        "STL_SET, (" + tData.GetRef() + ')');
}

ASNSequenceOfType::ASNSequenceOfType(const AutoPtr<ASNType>& type)
    : ASNOfType("SEQUENCE", type)
{
}

CTypeInfo* ASNSequenceOfType::CreateTypeInfo(void)
{
    return new CSequenceOfTypeInfo(name, type->GetTypeInfo());
}

CTypeStrings ASNSequenceOfType::GetCType(const CNcbiRegistry& def,
                                         const string& section,
                                         const string& key) const
{
    CTypeStrings tData = type->GetCType(def, section, key + ".data").ToSimple();
    return CTypeStrings("list< " + tData.cType + " >",
                        "STL_LIST, (" + tData.GetRef() + ')');
}

ASNMemberContainerType::ASNMemberContainerType(ASNModule& module,
                                               const string& kw)
    : ASNType(module, kw), keyword(kw)
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
        (*i)->Check();
    }
    return ok;
}

void ASNMemberContainerType::CollectUserTypes(set<string>& types) const
{
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        (*i)->type->CollectUserTypes(types);
    }
}

TObjectPtr ASNMemberContainerType::CreateDefault(const ASNValue& )
{
    THROW1_TRACE(runtime_error, keyword + " default not implemented");
}

ASNContainerType::ASNContainerType(ASNModule& module, const string& kw)
    : CParent(module, kw)
{
}

CTypeInfo* ASNContainerType::CreateTypeInfo(void)
{
    return new CAutoPointerTypeInfo(CreateClassInfo());
}

CClassInfoTmpl* ASNContainerType::CreateClassInfo(void)
{
    auto_ptr<CClassInfoTmpl> typeInfo(new CStructInfoTmpl(name, typeid(void),
        members.size()*sizeof(dataval)));

    int index = 0;
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i, ++index ) {
        ASNMember* member = i->get();
        ASNType* type = member->type.get();
        CMemberInfo* memberInfo =
            typeInfo->AddMember(member->name,
                                new CRealMemberInfo(index*sizeof(dataval),
                                                    new CTypeSource(type)));
        if ( member->Optional() )
            memberInfo->SetOptional();
        else if ( member->defaultValue )
            memberInfo->SetDefault(type->CreateDefault(*member->defaultValue));
    }
    return typeInfo.release();
}

void ASNContainerType::GenerateCode(ostream& header, ostream& code,
                                    const string& typeName,
                                    const CNcbiRegistry& def,
                                    const string& section) const
{
    for ( TMembers::const_iterator i = members.begin();
          i != members.end();
          ++i ) {
        const ASNMember* m = i->get();
        string fieldName = Identifier(m->name);
        if ( fieldName.empty() ) {
            continue;
        }
        m->type->GetCType(def, typeName,
                          m->name).AddMember(header, code,
                                             m->name, "m_" + fieldName);
        if ( m->defaultValue )
            code << "->SetDefault(...)";
        else if ( m->optional )
            code << "->SetOptional()";
        code << ';' << endl;
    }
}

ASNSetType::ASNSetType(ASNModule& module)
    : CParent(module, "SET")
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

ASNSequenceType::ASNSequenceType(ASNModule& module)
    : CParent(module, "SEQUENCE")
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

ASNChoiceType::ASNChoiceType(ASNModule& module)
    : CParent(module, "CHOICE")
{
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
    auto_ptr<CChoiceTypeInfo> typeInfo(new CChoiceTypeInfo(name));
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        ASNMember* member = i->get();
        typeInfo->AddVariant(member->name, new CTypeSource(member->type.get()));
    }
    return new CAutoPointerTypeInfo(typeInfo.release());
}

void ASNChoiceType::GenerateCode(ostream& header, ostream& code,
                                 const string& typeName,
                                 const CNcbiRegistry& def,
                                 const string& section) const
{
    for ( TMembers::const_iterator i = members.begin();
          i != members.end();
          ++i ) {
        const ASNMember* m = i->get();
        string fieldName = Identifier(m->name);
        if ( fieldName.empty() ) {
            continue;
        }
        code <<
            "    ADD_SUB_CLASS2(\"" << m->name << "\", " <<
            ClassName(def, m->type->name) << ");" << endl;
    }
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

/*
            if ( dynamic_cast<const ASNContainerType*>(type) ) {
                // SET, SEQUENCE or CHOICE
                if ( dynamic_cast<const ASNChoiceType*>(type) ) {
                    // CHOICE
                    ERR_POST(Fatal << "Cannot generate class for CHOICE type");
                }
                else {
                    // SET or SEQUENCE
                    
                    ->GenerateVariable(out, def, def.Get(sectionName, "_type"));
                }
            }
            else if ( dynamic_cast<const ASNOfType*>(type) ) {
                // SET OF ot SEQUENCE OF
                string stlContainer = def.Get(sectionName, "_type");
                if ( stlContainer.empty() ) {
                    if ( dynamic_cast<const ASNSetOfType*>(type) ) {
                        // SET
                        stlContainer = "set";
                    }
                    else {
                        // SEQUENCE
                        stlContainer = "list";
                    }
                }
                string memberName = def.Get(sectionName, "_member");
                if ( memberName.empty() ) {
                    memberName = "m_" + replace(typeName, '-', '_');
                }
                header << "    " << stlContainer << "<";
                if ( type
                header << "> " << memberName << ";" << endl;
            }
            else {
                ERR_POST(Fatal << "Cannot generate class for simple type");
            }
            }

*/
