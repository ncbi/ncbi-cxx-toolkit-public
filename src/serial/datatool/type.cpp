#include <type.hpp>
#include <value.hpp>
#include <module.hpp>

ASNType::ASNType()
    : line(0)
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

bool ASNType::Check(const ASNModule& )
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

ASNFixedType::ASNFixedType(const string& kw)
    : keyword(kw)
{
}

ostream& ASNFixedType::Print(ostream& out, int ) const
{
    return out << keyword;
}

ASNNullType::ASNNullType()
    : ASNFixedType("NULL")
{
}

#define CheckValueType(value, type, name) do{ \
if ( dynamic_cast<const type*>(&(value)) == 0 ) { \
    (value).Warning(name " value expected"); return false; \
} } while(0)

bool ASNNullType::CheckValue(const ASNModule& , const ASNValue& value)
{
    CheckValueType(value, ASNNullValue, "NULL");
    return true;
}

ASNBooleanType::ASNBooleanType()
    : ASNFixedType("BOOLEAN")
{
}

bool ASNBooleanType::CheckValue(const ASNModule& , const ASNValue& value)
{
    CheckValueType(value, ASNBoolValue, "BOOLEAN");
    return true;
}

ASNRealType::ASNRealType()
    : ASNFixedType("REAL")
{
}

bool ASNRealType::CheckValue(const ASNModule& , 
                             const ASNValue& value)
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

ASNVisibleStringType::ASNVisibleStringType()
    : ASNFixedType("VisibleString")
{
}

bool ASNVisibleStringType::CheckValue(const ASNModule& ,
                                      const ASNValue& value)
{
    CheckValueType(value, ASNStringValue, "string");
    return true;
}

ASNStringStoreType::ASNStringStoreType()
    : ASNFixedType("StringStore")
{
}

bool ASNStringStoreType::CheckValue(const ASNModule& ,
                                    const ASNValue& value)
{
    CheckValueType(value, ASNStringValue, "string");
    return true;
}

ASNBitStringType::ASNBitStringType()
    : ASNFixedType("BIT STRING")
{
}

bool ASNBitStringType::CheckValue(const ASNModule& ,
                                  const ASNValue& value)
{
    CheckValueType(value, ASNBitStringValue, "BIT STRING");
    return true;
}

ASNOctetStringType::ASNOctetStringType()
    : ASNFixedType("OCTET STRING")
{
}

bool ASNOctetStringType::CheckValue(const ASNModule& ,
                                    const ASNValue& value)
{
    CheckValueType(value, ASNBitStringValue, "OCTET STRING");
    return true;
}

ASNEnumeratedType::ASNEnumeratedType(const string& kw)
    : keyword(kw)
{
}

void ASNEnumeratedType::AddValue(const string& name)
{
    values.push_back(TValues::value_type(name));
}

void ASNEnumeratedType::AddValue(const string& name, long value)
{
    values.push_back(TValues::value_type(name, value));
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
        out << i->id;
        if ( i->hasValue )
            out << " (" << i->value << ")";
    }
    NewLine(out, indent - 1);
    return out << "}";
}

bool ASNEnumeratedType::CheckValue(const ASNModule& ,
                                   const ASNValue& value)
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

ASNIntegerType::ASNIntegerType()
    : ASNFixedType("INTEGER")
{
}

bool ASNIntegerType::CheckValue(const ASNModule& ,
                                const ASNValue& value)
{
    CheckValueType(value, ASNIntegerValue, "INTEGER");
    return true;
}

ASNUserType::ASNUserType(const string& n)
    : name(n)
{
}

ostream& ASNUserType::Print(ostream& out, int ) const
{
    return out << name;
}

bool ASNUserType::Check(const ASNModule& module)
{
    const ASNModule::TypeInfo* typeInfo =  module.FindType(name);
    if ( typeInfo == 0 ) {
        Warning("type " + name + " undefined");
        return false;
    }
    return true;
}

bool ASNUserType::CheckValue(const ASNModule& module,
                             const ASNValue& value)
{
    const ASNModule::TypeInfo* typeInfo =  module.FindType(name);
    if ( !typeInfo || !typeInfo->type ) {
        Warning("type " + name + " undefined");
        return false;
    }
    return typeInfo->type->CheckValue(module, value);
}

ASNOfType::ASNOfType(const string& kw, const AutoPtr<ASNType>& t)
    : keyword(kw), type(t)
{
}

ostream& ASNOfType::Print(ostream& out, int indent) const
{
    out << keyword << " OF ";
    return type->Print(out, indent);
}

bool ASNOfType::Check(const ASNModule& module)
{
    return type->Check(module);
}

bool ASNOfType::CheckValue(const ASNModule& module,
                           const ASNValue& value)
{
    const ASNBlockValue* block = dynamic_cast<const ASNBlockValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    bool ok = true;
    for ( ASNBlockValue::TValues::const_iterator i = block->values.begin();
          i != block->values.end(); ++i ) {
        if ( !type->CheckValue(module, **i) )
            ok = false;
    }
    return ok;
}

ASNSetOfType::ASNSetOfType(const AutoPtr<ASNType>& type)
    : ASNOfType("SET", type)
{
}

ASNSequenceOfType::ASNSequenceOfType(const AutoPtr<ASNType>& type)
    : ASNOfType("SEQUENCE", type)
{
}

ASNContainerType::ASNContainerType(const string& kw)
    : keyword(kw)
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

bool ASNContainerType::Check(const ASNModule& module)
{
    bool ok = true;
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        (*i)->Check(module);
    }
    return ok;
}

ASNSetType::ASNSetType()
    : ASNContainerType("SET")
{
}

bool ASNSetType::CheckValue(const ASNModule& module,
                            const ASNValue& value)
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
        if ( !member->second->type->CheckValue(module, *currvalue->value) ) {
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

ASNSequenceType::ASNSequenceType()
    : ASNContainerType("SEQUENCE")
{
}

bool ASNSequenceType::CheckValue(const ASNModule& module,
                                 const ASNValue& value)
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
        if ( !(*member)->type->CheckValue(module, *currvalue->value) ) {
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

ASNChoiceType::ASNChoiceType()
    : ASNContainerType("CHOICE")
{
}

bool ASNChoiceType::CheckValue(const ASNModule& module,
                               const ASNValue& value)
{
    const ASNNamedValue* choice = dynamic_cast<const ASNNamedValue*>(&value);
    if ( !choice ) {
        value.Warning("CHOICE value expected");
        return false;
    }
    for ( TMembers::const_iterator i = members.begin();
          i != members.end(); ++i ) {
        if ( (*i)->name == choice->name )
            return (*i)->type->CheckValue(module, *choice->value);
    }
    return false;
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

bool ASNMember::Check(const ASNModule& module)
{
    if ( !type->Check(module) )
        return false;
    if ( !defaultValue )
        return true;
    return type->CheckValue(module, *defaultValue);
}
