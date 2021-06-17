/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   .......
 *
 */

#include <ncbi_pch.hpp>
#include "test_serial.hpp"


/////////////////////////////////////////////////////////////////////////////
// TestHooks

void InitializeTestObject(
#ifdef HAVE_NCBI_C
    WebEnv*& env,
#else
    CRef<CWeb_Env>& env,
#endif
    CTestSerialObject& write, CTestSerialObject2& write1)
{
    string text_in("webenv.ent");
#ifdef HAVE_NCBI_C
    {
        unique_ptr<CObjectIStream> in(
            CObjectIStream::Open(text_in, eSerial_AsnText));
        in->Read(&env, GetSequenceTypeRef(&env).Get());
    }
#else
    {
        unique_ptr<CObjectIStream> in(CObjectIStream::Open(text_in,eSerial_AsnText));
        *in >> *env;
    }
#endif

    _TRACE("CTestSerialObject(object1): " << intptr_t(&write));
    _TRACE("CTestSerialObject2(object2): " << intptr_t(&write1));
    _TRACE("CTestSerialObject(object2): " <<
           intptr_t(static_cast<CTestSerialObject*>(&write1)));

    write.m_Name = "name";
    write.m_HaveName = false;
    //        write.m_NamePtr = &write1.m_Name;
    write.m_Size = -1;
    write.m_Attributes.push_back("m_Attributes");
    write.m_Attributes.push_back("m_Size");
    write.m_Attributes.push_back("m_");
    write.m_Next = &write1;
    const char* s = "data";
    write.m_Data.insert(write.m_Data.begin(), s, s + 4);
    write.m_Offsets.push_back(25);
    write.m_Offsets.push_back(-1024);
    write.m_Names[0] = "zero";
    write.m_Names[1] = "one";
    write.m_Names[2] = "two";
    write.m_Names[3] = "three";
    write.m_Names[10] = "ten";
    write.m_WebEnv = env;

    write1.m_Name = "write1";
    write1.m_HaveName = true;
    write1.m_NamePtr = new string("test");
    write1.m_Size = 0x7fff;
    write1.m_Attributes.push_back("write1");
    //        write1.m_Next = &write1;
    write1.m_WebEnv = 0;
    write1.m_Name2 = "name2";
}

void CWriteSerialObjectHook::WriteObject(CObjectOStream& out,
                                         const CConstObjectInfo& object)
{
    string function("CWriteSerialObjectHook::WriteObject: ");
    LOG_POST(function << "stack path = " << out.GetStackPath());

    const CTestSerialObject* obj = CType<CTestSerialObject>::Get(object);

    if (obj != m_HookedObject) {
        // This is not the object we would like to process.
        // Use default write method.
        LOG_POST(function << " -- using default writer");
        DefaultWrite(out, object);
        return;
    }

    // Special object processing
    // Write object open tag
    out.BeginClass(object.GetClassTypeInfo());
    // Iterate object members
    for (CConstObjectInfo::CMemberIterator member =
        object.BeginMembers(); member; ++member) {
        if (member.GetAlias() == "m_Name") {
            // This member will be written using CharBlock instead of the
            // default method.
            LOG_POST(function
                << " -- using CharBlock to write "
                << member.GetAlias());
            // Write the special member
            out.BeginClassMember(member.GetMemberInfo()->GetId());
            // Start char block, specify output stream and block size
            CObjectOStream::CharBlock cb(out, obj->m_Name.size());
                cb.Write(obj->m_Name.c_str(), obj->m_Name.size());   
            // End char block and member
            cb.End();
            out.EndClassMember();
        }
        else {
            // Do not need special processing for this member -- use
            // default write method. Do not write unset members (although
            // it is possible).
            if ( member.IsSet() ) {
                LOG_POST(function
                    << " -- using default member writer for "
                    << member.GetAlias());
                out.WriteClassMember(member);
            }
        }
    }
    // Close the object
    out.EndClass();
}

void CReadSerialObjectHook::ReadObject(CObjectIStream& in,
                                       const CObjectInfo& object)
{
    string function("CReadSerialObjectHook::ReadObject: ");
    LOG_POST(function << "stack path = " << in.GetStackPath());

    const CTestSerialObject* obj = CType<CTestSerialObject>::Get(object);
    
    if (obj != m_HookedObject) {
        // Use default read method.
        LOG_POST(function << " -- using default reader");
        DefaultRead(in, object);
        return;
    }

    LOG_POST(function << " -- using overloaded reader");
    // Read the object manually, member by member.
    for ( CIStreamClassMemberIterator i(in, object); i; ++i ) {
        const CObjectTypeInfo::CMemberIterator& nextMember = *i;
        LOG_POST(function
            << " -- using default member reader for "
            << nextMember.GetAlias());
        // Special pre-read processing may be put here
        in.ReadClassMember(CObjectInfoMI(object, nextMember.GetMemberIndex()));
        // Special post-read processing may be put here
    }
}

void CWriteSerialObject_NameHook::WriteClassMember
    (CObjectOStream& out, const CConstObjectInfoMI& member)
{
    string function("CWriteSerialObject_NameHook::WriteClassMember: ");
    LOG_POST(function << "stack path = " << out.GetStackPath());

    // No special processing -- just report writing the member
    // string type has no TypeInfo, so we use GetUnchecked method
    const string& name =
        *CType<string>::GetUnchecked(member.GetMember());
    LOG_POST(function << " -- writing m_Name: " << name);

    DefaultWrite(out, member);
}

void CReadSerialObject_NameHook::ReadClassMember
    (CObjectIStream& in, const CObjectInfoMI& member)
{
    string function("CReadSerialObject_NameHook::ReadClassMember: ");
    LOG_POST(function << "stack path = " << in.GetStackPath());

    // No special processing -- just report reading the member
    DefaultRead(in, member);

    // string type has no TypeInfo, so we use GetUnchecked method
    const string& name =
        *CType<string>::GetUnchecked(member.GetMember());
    LOG_POST(function << " -- reading m_Name: " << name);
}

void CTestSerialObjectHook::Process(const CTestSerialObject& obj)
{
    LOG_POST("CTestSerialObjectHook::Process: obj.m_Name = " << obj.m_Name);
}

void CStringObjectHook::Process(const string& obj)
{
    LOG_POST("CStringObjectHook::Process: obj = " << obj);
}

/////////////////////////////////////////////////////////////////////////////
// TestPrintAsn

static int asnIndentLevel=0;

void PrintAsnValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnPrimitiveValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnClassValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnChoiceValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnContainerValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnPointerValue(CNcbiOstream& out, const CConstObjectInfo& object);
void ResetAsnIndent(void);
void IncAsnIndent(void);
void DecAsnIndent(void);
void PrintAsnIndent(CNcbiOstream& out);

class AsnBlock
{
public:
    AsnBlock(CNcbiOstream& out)
        : m_Out(out), m_Empty(true)
        {
            out << "{\n";
            IncAsnIndent();
        }
    ~AsnBlock(void)
        {
            if ( !m_Empty )
                m_Out << '\n';
            DecAsnIndent();
            PrintAsnIndent(m_Out);
            m_Out << '}';
        }

    void NextElement(void)
        {
            if ( m_Empty )
                m_Empty = false;
            else
                m_Out << ",\n";
            PrintAsnIndent(m_Out);
        }

private:
    CNcbiOstream& m_Out;
    bool m_Empty;
};

void PrintAsn(CNcbiOstream& out, const CConstObjectInfo& object)
{
    BOOST_CHECK(object);
    BOOST_CHECK(!object.GetTypeInfo()->GetName().empty());
    ResetAsnIndent();
    out << object.GetTypeInfo()->GetName() << " ::= ";
    PrintAsnValue(out, object);
}

void PrintAsnValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    BOOST_CHECK(object);
    switch ( object.GetTypeFamily() ) {
    case eTypeFamilyPrimitive:
        PrintAsnPrimitiveValue(out, object);
        break;
    case eTypeFamilyClass:
        PrintAsnClassValue(out, object);
        break;
    case eTypeFamilyChoice:
        PrintAsnChoiceValue(out, object);
        break;
    case eTypeFamilyContainer:
        PrintAsnContainerValue(out, object);
        break;
    case eTypeFamilyPointer:
        PrintAsnPointerValue(out, object);
        break;
    }
}

void PrintAsnPrimitiveValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    switch ( object.GetPrimitiveValueType() ) {
    case ePrimitiveValueBool:
        out << (object.GetPrimitiveValueBool()? "TRUE": "FALSE");
        break;
    case ePrimitiveValueChar:
        out << '\'' << object.GetPrimitiveValueChar() << '\'';
        break;
    case ePrimitiveValueInteger:
        if ( object.IsPrimitiveValueSigned() )
            out << object.GetPrimitiveValueLong();
        else
            out << object.GetPrimitiveValueULong();
        break;
    case ePrimitiveValueReal:
        out << object.GetPrimitiveValueDouble();
        break;
    case ePrimitiveValueString:
        out << '"' << object.GetPrimitiveValueString() << '"';
        break;
    case ePrimitiveValueEnum:
        {
            string s;
            object.GetPrimitiveValueString(s);
            if ( !s.empty() )
                out << s;
            else if ( object.IsPrimitiveValueSigned() )
                out << object.GetPrimitiveValueLong();
            else
                out << object.GetPrimitiveValueULong();
        }
        break;
    case ePrimitiveValueOctetString:
        out << '\'';
        {
            static const char Hex[] = "0123456789ABCDEF";
            vector<char> s;
            object.GetPrimitiveValueOctetString(s);
            ITERATE ( vector<char>, i, s ) {
                char c = *i;
                out << Hex[(c >> 4) & 15] << Hex[c & 15];
            }
        }
        out << "'H";
        break;
    default:
        out << "...";
        break;
    }
}

void PrintAsnClassValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    AsnBlock block(out);

    for ( CConstObjectInfo::CMemberIterator i(object); i; ++i ) {
        if ( i.IsSet() ) {
            // print member separator
            block.NextElement();

            // print member id if any
            const string& alias = i.GetAlias();
            if ( !alias.empty() )
                out << alias << ' ';

            // print member value
            PrintAsnValue(out, *i);
        }
    }
}

void PrintAsnChoiceValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    CConstObjectInfo::CChoiceVariant variant(object);
    if ( !variant )
        THROW1_TRACE(runtime_error, "cannot print empty choice");
    out << variant.GetAlias() << ' ';
    PrintAsnValue(out, *variant);
}

void PrintAsnContainerValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    AsnBlock block(out);
    
    for ( CConstObjectInfo::CElementIterator i(object); i; ++i ) {
        block.NextElement();
        
        PrintAsnValue(out, *i);
    }
}

void PrintAsnPointerValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    PrintAsnValue(out, object.GetPointedObject());
}

void ResetAsnIndent(void)
{
    asnIndentLevel = 0;
}

void IncAsnIndent(void)
{
    asnIndentLevel++;
}

void DecAsnIndent(void)
{
    BOOST_CHECK(asnIndentLevel > 0);
    asnIndentLevel--;
}

void PrintAsnIndent(CNcbiOstream& out)
{
    for ( int i = 0; i < asnIndentLevel; ++i )
        out << "  ";
}
