#include <type.hpp>
#include <value.hpp>
#include <module.hpp>
#include <moduleset.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/asntypes.hpp>
#include <serial/autoptrinfo.hpp>

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
    long integerValue;
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

CTypeInfo* ASNType::CreateTypeInfo(void)
{
    return 0;
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

ASNNullType::ASNNullType(ASNModule& module)
    : ASNFixedType(module, "NULL")
{
}

bool ASNNullType::CheckValue(const ASNValue& value)
{
    CheckValueType(value, ASNNullValue, "NULL");
    return true;
}

TObjectPtr ASNNullType::CreateDefault(const ASNValue& value)
{
    THROW1_TRACE(runtime_error, "NULL type not implemented");
}

TTypeInfo ASNNullType::GetTypeInfo(void)
{
    THROW1_TRACE(runtime_error, "NULL type not implemented");
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

TObjectPtr ASNRealType::CreateDefault(const ASNValue& value)
{
    THROW1_TRACE(runtime_error, "REAL default not implemented");
}

TTypeInfo ASNRealType::GetTypeInfo(void)
{
    return CStdTypeInfo<double>::GetTypeInfo();
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

TObjectPtr ASNBitStringType::CreateDefault(const ASNValue& value)
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

TObjectPtr ASNOctetStringType::CreateDefault(const ASNValue& value)
{
    THROW1_TRACE(runtime_error, "OCTET STRING default not implemented");
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
    return new long(dynamic_cast<const ASNIntegerValue&>(value).value);
}

TTypeInfo ASNIntegerType::GetTypeInfo(void)
{
    return CStdTypeInfo<long>::GetTypeInfo();
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

TTypeInfo ASNUserType::GetTypeInfo(void)
{
    return Resolve()->GetTypeInfo();
}

TObjectPtr ASNUserType::CreateDefault(const ASNValue& value)
{
    return Resolve()->CreateDefault(value);
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

TObjectPtr ASNOfType::CreateDefault(const ASNValue& value)
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

ASNSequenceOfType::ASNSequenceOfType(const AutoPtr<ASNType>& type)
    : ASNOfType("SEQUENCE", type)
{
}

CTypeInfo* ASNSequenceOfType::CreateTypeInfo(void)
{
    return new CSequenceOfTypeInfo(name, type->GetTypeInfo());
}

ASNContainerType::ASNContainerType(ASNModule& module, const string& kw)
    : ASNType(module, kw), keyword(kw)
{
}

ostream& ASNContainerType::Print(ostream& out, int indent) const
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

bool ASNContainerType::Check(void)
{
    bool ok = true;
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        (*i)->Check();
    }
    return ok;
}

CTypeInfo* ASNContainerType::CreateTypeInfo(void)
{
    auto_ptr<CClassInfoTmpl> typeInfo(new CStructInfoTmpl(name, typeid(void),
        members.size()*sizeof(dataval), keyword == "SET"));
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
    return new CAutoPointerTypeInfo(typeInfo.release());
}

ASNSetType::ASNSetType(ASNModule& module)
    : ASNContainerType(module, "SET")
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

TObjectPtr ASNSetType::CreateDefault(const ASNValue& value)
{
    THROW1_TRACE(runtime_error, "SET default not implemented");
}

ASNSequenceType::ASNSequenceType(ASNModule& module)
    : ASNContainerType(module, "SEQUENCE")
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

TObjectPtr ASNSequenceType::CreateDefault(const ASNValue& value)
{
    THROW1_TRACE(runtime_error, "SEQUENCE default not implemented");
}

ASNChoiceType::ASNChoiceType(ASNModule& module)
    : ASNContainerType(module, "CHOICE")
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

TObjectPtr ASNChoiceType::CreateDefault(const ASNValue& value)
{
    THROW1_TRACE(runtime_error, "CHOICE default not implemented");
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
