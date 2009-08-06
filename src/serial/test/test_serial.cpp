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
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/serialasn.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <serial/iterator.hpp>
#include <serial/objhook.hpp>
#include "cppwebenv.hpp"
#include <serial/serialimpl.hpp>
#include <serial/streamiter.hpp>

#ifdef HAVE_NCBI_C
# include <asn.h>
# include "twebenv.h"
#else
# include <serial/test/Web_Env.hpp>
#endif

#include <corelib/ncbifile.hpp>
#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <common/test_assert.h>  /* This header must go last */

/////////////////////////////////////////////////////////////////////////////
// Test ASN serialization

BOOST_AUTO_TEST_CASE(s_TestAsnSerialization)
{
    string text_in("webenv.ent"), text_out("webenv.ento");
    string  bin_in("webenv.bin"),  bin_out("webenv.bino");
#ifdef HAVE_NCBI_C
    {
        WebEnv* env = 0;
        {
            // read ASN text
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(text_in, eSerial_AsnText));
            in->Read(&env, GetSequenceTypeRef(&env).Get());
        }
        {
            // write ASN binary
            auto_ptr<CObjectOStream> out(
                CObjectOStream::Open(bin_out,eSerial_AsnBinary));
            out->Write(&env, GetSequenceTypeRef(&env).Get());
        }
        BOOST_CHECK( CFile( bin_in).Compare( bin_out) );
        {
            // C-style Object must be clean before loading: using new WebEnv instance
            WebEnv* env2 = 0;
            // read ASN binary
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(bin_in,eSerial_AsnBinary));
            in->Read(&env2, GetSequenceTypeRef(&env2).Get());
            // write ASN text
            auto_ptr<CObjectOStream> out(
                CObjectOStream::Open(text_out,eSerial_AsnText));
            out->SetAutoSeparator(false);
            out->Write(&env2, GetSequenceTypeRef(&env2).Get());
        }
        BOOST_CHECK( CFile(text_in).Compare(text_out) );
    }
#else
    {
        CRef<CWeb_Env> env(new CWeb_Env);
        {
            // read ASN text
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(text_in,eSerial_AsnText));
            *in >> *env;
        }
        {
            // write ASN binary
            auto_ptr<CObjectOStream> out(
                CObjectOStream::Open(bin_out,eSerial_AsnBinary));
            *out << *env;
        }
        BOOST_CHECK( CFile( bin_in).Compare( bin_out) );
        {
            // write ASN binary
            CNcbiOfstream ofs(bin_out.c_str(),
                IOS_BASE::out | IOS_BASE::trunc | IOS_BASE::binary);
            ofs << MSerial_AsnBinary << *env;
        }
        BOOST_CHECK( CFile( bin_in).Compare( bin_out) );
        {
            // write ASN binary
            auto_ptr<CObjectOStream> out(
                CObjectOStream::Open(bin_out,eSerial_AsnBinary));
            out->Write( env, env->GetThisTypeInfo());
        }
        BOOST_CHECK( CFile( bin_in).Compare( bin_out) );
    }
    {
        CRef<CWeb_Env> env(new CWeb_Env);
        {
            // read ASN binary
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(bin_in,eSerial_AsnBinary));
            *in >> *env;
        }
        {
            // write ASN text
            auto_ptr<CObjectOStream> out(
                CObjectOStream::Open(text_out,eSerial_AsnText));
            out->SetAutoSeparator(false);
            *out << *env;
        }
        BOOST_CHECK( CFile(text_in).Compare(text_out) );
        {
            // write ASN binary
            CNcbiOfstream ofs(bin_out.c_str(),
                IOS_BASE::out | IOS_BASE::trunc | IOS_BASE::binary);
            ofs << MSerial_AsnBinary << *env;
        }
        BOOST_CHECK( CFile( bin_in).Compare( bin_out) );
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////
// TestPrintAsn

void PrintAsn(CNcbiOstream& out, const CConstObjectInfo& object);

BOOST_AUTO_TEST_CASE(s_TestPrintAsn)
{
    string text_in("webenv.ent"), text_out("webenv.ento");
#ifdef HAVE_NCBI_C
    {
        WebEnv* env = 0;
        {
            // read ASN text
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(text_in, eSerial_AsnText));
            in->Read(&env, GetSequenceTypeRef(&env).Get());
        }
        {
            CNcbiOfstream out(text_out.c_str());
            PrintAsn(out, CConstObjectInfo(&env,GetSequenceTypeRef(&env).Get()));
        }
        BOOST_CHECK( CFile(text_in).Compare(text_out) );
    }
#else
    {
        CRef<CWeb_Env> env(new CWeb_Env);
        {
            // read ASN text
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(text_in,eSerial_AsnText));
            *in >> *env;
        }
        {
            // write ASN text
            CNcbiOfstream out(text_out.c_str());
            PrintAsn(out, ConstObjectInfo(*env));
        }
        BOOST_CHECK( CFile(text_in).Compare(text_out) );
    }
#endif
}

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

/////////////////////////////////////////////////////////////////////////////
// TestHooks

void InitializeTestObject(
#ifdef HAVE_NCBI_C
    WebEnv* env,
#else
    CRef<CWeb_Env>& env,
#endif
    CTestSerialObject& obj, CTestSerialObject2& write1);

class CWriteSerialObjectHook : public CWriteObjectHook
{
public:
    CWriteSerialObjectHook(CTestSerialObject* hooked)
        : m_HookedObject(hooked) {}
    virtual void
    WriteObject(CObjectOStream& out, const CConstObjectInfo& object);
private:
    CTestSerialObject* m_HookedObject;
};

class CReadSerialObjectHook : public CReadObjectHook
{
public:
    CReadSerialObjectHook(CTestSerialObject* hooked)
        : m_HookedObject(hooked) {}
    virtual void
    ReadObject(CObjectIStream& in, const CObjectInfo& object);
private:
    CTestSerialObject* m_HookedObject;
};

class CWriteSerialObject_NameHook : public CWriteClassMemberHook
{
public:
    virtual void
    WriteClassMember(CObjectOStream& out, const CConstObjectInfoMI& member);
};

class CReadSerialObject_NameHook : public CReadClassMemberHook
{
public:
    virtual void
    ReadClassMember(CObjectIStream& in, const CObjectInfoMI& member);
};

class CTestSerialObjectHook : public CSerial_FilterObjectsHook<CTestSerialObject>
{
public:
    virtual void Process(const CTestSerialObject& obj)
    {
        LOG_POST("CTestSerialObjectHook::Process: obj.m_Name = " << obj.m_Name);
    }
};

class CStringObjectHook : public CSerial_FilterObjectsHook<string>
{
public:
    virtual void Process(const string& obj)
    {
        LOG_POST("CStringObjectHook::Process: obj = " << obj);
    }
};

BOOST_AUTO_TEST_CASE(s_TestObjectHooks)
{
    CTestSerialObject obj;
    CTestSerialObject2 write1;
#ifdef HAVE_NCBI_C
    WebEnv* env = 0;
#else
    CRef<CWeb_Env> env(new CWeb_Env);
#endif    
    InitializeTestObject( env, obj, write1);

    char* buf = 0;
    {
        // write hook
        CNcbiOstrstream ostrs;
        {
            auto_ptr<CObjectOStream> os(
                CObjectOStream::Open(eSerial_AsnText, ostrs));
            // set local write hook
            CObjectHookGuard<CTestSerialObject> w_hook(
                *(new CWriteSerialObjectHook(&obj)), &(*os));
            *os << obj;
        }
		ostrs << '\0';
		buf = ostrs.str();
    }
    {
        // read hook
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        CTestSerialObject obj_copy;
        CObjectHookGuard<CTestSerialObject> r_hook(
            *(new CReadSerialObjectHook(&obj_copy)), &(*is));
        *is >> obj_copy;
        // Can not use SerialEquals<> with C-objects
#ifndef HAVE_NCBI_C
        BOOST_CHECK(SerialEquals<CTestSerialObject>(obj, obj_copy));
#endif
    }
    delete[] buf;

    {
        // write hook
        CNcbiOstrstream ostrs;
        {
            auto_ptr<CObjectOStream> os(
                CObjectOStream::Open(eSerial_AsnText, ostrs));
            // set local write hook
            CObjectTypeInfo type = CType<CTestSerialObject>();
            type.SetLocalWriteHook(*os, new CWriteSerialObjectHook(&obj));
            *os << obj;
        }
		ostrs << '\0';
		buf = ostrs.str();
    }
    {
        // read hook
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        CTestSerialObject obj_copy;
        CObjectTypeInfo type = CType<CTestSerialObject>();
        type.SetLocalReadHook(*is, new CReadSerialObjectHook(&obj_copy));
        *is >> obj_copy;
#ifndef HAVE_NCBI_C
        BOOST_CHECK(SerialEquals<CTestSerialObject>(obj, obj_copy));
#endif
    }
    delete[] buf;

    {
        // write, stack path hook
        CNcbiOstrstream ostrs;
        {
            auto_ptr<CObjectOStream> os(
                CObjectOStream::Open(eSerial_AsnText, ostrs));
            // set local write hook
            CObjectTypeInfo(CTestSerialObject::GetTypeInfo()).SetPathWriteHook(
                os.get(), "CTestSerialObject.*", new CWriteSerialObjectHook(&obj));
            *os << obj;
        }
		ostrs << '\0';
		buf = ostrs.str();
    }
    {
        // read, stack path hook
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        CTestSerialObject obj_copy;
        CObjectTypeInfo(CTestSerialObject::GetTypeInfo()).SetPathReadHook(
            is.get(), "CTestSerialObject.*", new CReadSerialObjectHook(&obj_copy));

        *is >> obj_copy;
#ifndef HAVE_NCBI_C
        BOOST_CHECK(SerialEquals<CTestSerialObject>(obj, obj_copy));
#endif
    }

#ifndef HAVE_NCBI_C
    {
        // finding serial objects of a specific type
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        Serial_FilterObjects<CTestSerialObject>(*is, new CTestSerialObjectHook);
    }
    {
        // finding non-serial objects, here - strings
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        Serial_FilterStdObjects<CTestSerialObject>(*is, new CStringObjectHook);
    }
#ifdef _MT
    {
        // finding serial objects of a specific type
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        CIStreamObjectIterator<CTestSerialObject,CWeb_Env> i(*is);
        for ( ; i.IsValid(); ++i) {
            const CWeb_Env& obj = *i;
            LOG_POST("CWeb_Env @ " << NStr::PtrToString(&obj));
        }
    }
    {
        // finding non-serial objects, here - strings
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        CIStreamStdIterator<CTestSerialObject,string> i(*is);
        for ( ; i.IsValid(); ++i) {
            const string& obj = *i;
            LOG_POST("CIStreamStdIterator: obj = " << obj);
        }
    }
#endif //_MT
#endif //HAVE_NCBI_C
    delete[] buf;
}

BOOST_AUTO_TEST_CASE(s_TestMemberHooks)
{
    CTestSerialObject obj;
    CTestSerialObject2 write1;
#ifdef HAVE_NCBI_C
    WebEnv* env = 0;
#else
    CRef<CWeb_Env> env(new CWeb_Env);
#endif    
    InitializeTestObject( env, obj, write1);

    char* buf = 0;
    {
        // write hook
        CNcbiOstrstream ostrs;
        auto_ptr<CObjectOStream> os(
            CObjectOStream::Open(eSerial_AsnText, ostrs));
        CObjectHookGuard<CTestSerialObject> w_hook(
            "m_Name", *(new CWriteSerialObject_NameHook), &(*os));
        *os << obj;
		os->FlushBuffer();
		ostrs << '\0';
        buf = ostrs.str();
    }
    {
        // read hook
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        CTestSerialObject obj_copy;
        CObjectHookGuard<CTestSerialObject> r_hook(
            "m_Name", *(new CReadSerialObject_NameHook), &(*is));
        *is >> obj_copy;
        // Can not use SerialEquals<> with C-objects
#ifndef HAVE_NCBI_C
        BOOST_CHECK(SerialEquals<CTestSerialObject>(obj, obj_copy));
#endif
    }
    delete[] buf;

    {
        // write hook
        CNcbiOstrstream ostrs;
        auto_ptr<CObjectOStream> os(
            CObjectOStream::Open(eSerial_AsnText, ostrs));
        CObjectTypeInfo type = CType<CTestSerialObject>();
        type.FindMember("m_Name").SetLocalWriteHook(
            *os, new CWriteSerialObject_NameHook);
        *os << obj;
		os->FlushBuffer();
		ostrs << '\0';
        buf = ostrs.str();
    }
    {
        // read hook
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        CTestSerialObject obj_copy;
        CObjectTypeInfo type = CType<CTestSerialObject>();
        type.FindMember("m_Name").SetLocalReadHook(
            *is, new CReadSerialObject_NameHook);
        *is >> obj_copy;
        // Can not use SerialEquals<> with C-objects
#ifndef HAVE_NCBI_C
        BOOST_CHECK(SerialEquals<CTestSerialObject>(obj, obj_copy));
#endif
    }
    {
        // read, stack path hook
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(
            CObjectIStream::Open(eSerial_AsnText, istrs));
        CTestSerialObject obj_copy;
        is->SetPathReadMemberHook( "CTestSerialObject.m_Name",new CReadSerialObject_NameHook);
        *is >> obj_copy;
        // Can not use SerialEquals<> with C-objects
#ifndef HAVE_NCBI_C
        BOOST_CHECK(SerialEquals<CTestSerialObject>(obj, obj_copy));
#endif
    }
    delete[] buf;
}

void InitializeTestObject(
#ifdef HAVE_NCBI_C
    WebEnv* env,
#else
    CRef<CWeb_Env>& env,
#endif
    CTestSerialObject& write, CTestSerialObject2& write1)
{
    string text_in("webenv.ent");
#ifdef HAVE_NCBI_C
    {
        auto_ptr<CObjectIStream> in(
            CObjectIStream::Open(text_in, eSerial_AsnText));
        in->Read(&env, GetSequenceTypeRef(&env).Get());
    }
#else
    {
        auto_ptr<CObjectIStream> in(CObjectIStream::Open(text_in,eSerial_AsnText));
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

/////////////////////////////////////////////////////////////////////////////
// Test iterators

BOOST_AUTO_TEST_CASE(s_TestIterators)
{
    CTestSerialObject write;
    CTestSerialObject2 write1;
#ifdef HAVE_NCBI_C
    WebEnv* env = 0;
#else
    CRef<CWeb_Env> env(new CWeb_Env);
#endif    
    InitializeTestObject( env, write, write1);
    const CTestSerialObject& cwrite = write;

    CTypeIterator<CTestSerialObject> j1; j1 = Begin(write);
    BOOST_CHECK( j1.IsValid() );
    CTypeIterator<CTestSerialObject> j2(Begin(write));
    BOOST_CHECK( j2.IsValid() );
    CTypeIterator<CTestSerialObject> j3 = Begin(write);
    BOOST_CHECK( j3.IsValid() );

    CTypeConstIterator<CTestSerialObject> k1; k1 = Begin(write);
    BOOST_CHECK( k1.IsValid() );
    CTypeConstIterator<CTestSerialObject> k2(Begin(write));
    BOOST_CHECK( k2.IsValid() );
    CTypeConstIterator<CTestSerialObject> k3 = ConstBegin(write);
    BOOST_CHECK( k3.IsValid() );

    CTypeConstIterator<CTestSerialObject> l1; l1 = ConstBegin(write);
    BOOST_CHECK( l1.IsValid() );
    CTypeConstIterator<CTestSerialObject> l2(ConstBegin(write));
    BOOST_CHECK( l2.IsValid() );
    CTypeConstIterator<CTestSerialObject> l3 = ConstBegin(write);
    BOOST_CHECK( l3.IsValid() );

    CTypeConstIterator<CTestSerialObject> m1; m1 = ConstBegin(cwrite);
    BOOST_CHECK( m1.IsValid() );
    CTypeConstIterator<CTestSerialObject> m2(ConstBegin(cwrite));
    BOOST_CHECK( m2.IsValid() );
    CTypeConstIterator<CTestSerialObject> m3 = ConstBegin(cwrite);
    BOOST_CHECK( m3.IsValid() );

    CTypesIterator n1; n1 = Begin(write);
    BOOST_CHECK( !n1.IsValid() );
    CTypesConstIterator n2; n2 = ConstBegin(write);
    BOOST_CHECK( !n2.IsValid() );
    CTypesConstIterator n3; n3 = ConstBegin(cwrite);
    BOOST_CHECK( !n3.IsValid() );

    {
        // visit all CTestSerialObjects
        for ( CTypeIterator<CTestSerialObject> oi = Begin(write); oi; ++oi ) {
            const CTestSerialObject& obj = *oi;
            NcbiCerr<<"CTestSerialObject @ "<<NStr::PtrToString(&obj)<<
                NcbiEndl;
            CTypeIterator<CTestSerialObject> oi1(Begin(write));
            BOOST_CHECK( oi1.IsValid() );
            CTypeIterator<CTestSerialObject> oi2;
            oi2 = Begin(write);
            BOOST_CHECK( oi2.IsValid() );
        }
    }
    {
        // visit all CTestSerialObjects
        for ( CTypeConstIterator<CTestSerialObject> oi = ConstBegin(write); oi;
              ++oi ) {
            const CTestSerialObject& obj = *oi;
            NcbiCerr<<"CTestSerialObject @ "<<NStr::PtrToString(&obj)<<
                NcbiEndl;
            CTypeConstIterator<CTestSerialObject> oi1(Begin(write));
            BOOST_CHECK( oi1.IsValid() );
            CTypeConstIterator<CTestSerialObject> oi2;
            oi2 = Begin(write);
            BOOST_CHECK( oi2.IsValid() );
        }
    }
    {
        // visit all CTestSerialObjects
        for ( CTypeConstIterator<CTestSerialObject> oi = ConstBegin(cwrite);
              oi; ++oi ) {
            const CTestSerialObject& obj = *oi;
            NcbiCerr<<"CTestSerialObject @ "<<NStr::PtrToString(&obj)<<
                NcbiEndl;
            CTypeConstIterator<CTestSerialObject> oi1(ConstBegin(cwrite));
            BOOST_CHECK( oi1.IsValid() );
            CTypeConstIterator<CTestSerialObject> oi2;
            oi2 = ConstBegin(cwrite);
            BOOST_CHECK( oi2.IsValid() );
        }
    }

    {
        // visit all strings
        for ( CStdTypeConstIterator<string> si = ConstBegin(cwrite); si;
              ++si ) {
            BOOST_CHECK( si.IsValid() );
            NcbiCerr <<"String: \""<<*si<<"\""<<NcbiEndl;
        }
    }

    {
        // visit all CObjects
        for ( CObjectConstIterator oi = ConstBegin(cwrite); oi; ++oi ) {
            const CObject& obj = *oi;
            BOOST_CHECK( oi.IsValid() );
            NcbiCerr <<"CObject: @ "<<NStr::PtrToString(&obj)<<NcbiEndl;
        }
    }

    {
        CTypesIterator i;
        CType<CTestSerialObject>::AddTo(i);
        CType<CTestSerialObject2>::AddTo(i);
        for ( i = Begin(write); i; ++i ) {
            BOOST_CHECK( i.IsValid() );
            CTestSerialObject* obj = CType<CTestSerialObject>::Get(i);
            BOOST_CHECK( obj );
            if ( obj ) {
                NcbiCerr <<"CObject: @ "<<NStr::PtrToString(obj)<<NcbiEndl;
            }
            else {
                NcbiCerr <<"CObject2: @ "<<
                    NStr::PtrToString(CType<CTestSerialObject2>::Get(i))<<
                    NcbiEndl;
            }
        }
    }

    {
        CTypesConstIterator i;
        CType<CTestSerialObject>::AddTo(i);
        CType<CTestSerialObject2>::AddTo(i);
        for ( i = ConstBegin(write); i; ++i ) {
            BOOST_CHECK( i.IsValid() );
            const CTestSerialObject* obj = CType<CTestSerialObject>::Get(i);
            BOOST_CHECK( obj );
            if ( obj ) {
                NcbiCerr <<"CObject: @ "<<NStr::PtrToString(obj)<<NcbiEndl;
            }
            else {
                NcbiCerr <<"CObject2: @ "<<
                    NStr::PtrToString(CType<CTestSerialObject2>::Get(i))<<
                    NcbiEndl;
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(s_TestSerialization2)
{
    CTestSerialObject write;
    CTestSerialObject2 write1;
#ifdef HAVE_NCBI_C
    WebEnv* env = 0;
#else
    CRef<CWeb_Env> env(new CWeb_Env);
#endif    
    InitializeTestObject( env, write, write1);

#ifdef HAVE_NCBI_C
    string text_in("ctest_serial.asn");
    string bin_in("ctest_serial.asb");
#else
    string text_in("cpptest_serial.asn");
    string bin_in("cpptest_serial.asb");
#endif
    string text_out("test_serial.asno"), text_out2("test_serial.asno2");
    string bin_out("test_serial.asbo");
    {
        {
            auto_ptr<CObjectOStream> out(
                CObjectOStream::Open(text_out,eSerial_AsnText));
            *out << write;
        }
        BOOST_CHECK( CFile(text_in).Compare(text_out) );
        {
            CNcbiOfstream out(text_out2.c_str());
            PrintAsn(out, ConstObjectInfo(write));
        }
        CTestSerialObject read;
        {
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(text_in,eSerial_AsnText));
            *in >> read;
        }
#ifdef HAVE_NCBI_C
        // Some functions does not work with C-style objects.
        // Reset WebEnv for SerialEquals() to work.
        TWebEnv* saved_env_read = read.m_WebEnv;
        TWebEnv* saved_env_write = write.m_WebEnv;
        read.m_WebEnv = 0;
        write.m_WebEnv = 0;
#endif
        BOOST_CHECK(SerialEquals<CTestSerialObject>(read, write));
#ifdef HAVE_NCBI_C
        // Restore C-style objects
        read.m_WebEnv = saved_env_read;
        write.m_WebEnv = saved_env_write;
#endif
        read.Dump(NcbiCerr);
        read.m_Next->Dump(NcbiCerr);
#ifndef HAVE_NCBI_C
        {
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(text_in,eSerial_AsnText));
            in->Skip(ObjectType(read));
        }
#endif
    }

    {
        {
            auto_ptr<CObjectOStream> out(
                CObjectOStream::Open(bin_out,eSerial_AsnBinary));
            *out << write;
        }
        BOOST_CHECK( CFile(bin_in).Compare(bin_out) );
        CTestSerialObject read;
        {
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(bin_in,eSerial_AsnBinary));
            *in >> read;
        }
#ifdef HAVE_NCBI_C
        // Some functions does not work with C-style objects.
        // Reset WebEnv for SerialEquals() to work.
        TWebEnv* saved_env_read = read.m_WebEnv;
        TWebEnv* saved_env_write = write.m_WebEnv;
        read.m_WebEnv = 0;
        write.m_WebEnv = 0;
#endif
        BOOST_CHECK(SerialEquals<CTestSerialObject>(read, write));
#ifdef HAVE_NCBI_C
        // Restore C-style objects
        read.m_WebEnv = saved_env_read;
        write.m_WebEnv = saved_env_write;
#endif
        read.Dump(NcbiCerr);
        read.m_Next->Dump(NcbiCerr);
#ifndef HAVE_NCBI_C
        {
            auto_ptr<CObjectIStream> in(
                CObjectIStream::Open(bin_in,eSerial_AsnBinary));
            in->Skip(ObjectType(read));
        }
#endif
    }
}
