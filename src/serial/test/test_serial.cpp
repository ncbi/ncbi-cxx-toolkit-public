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
#include <serial/object.hpp>
#include <serial/iterator.hpp>
#include <serial/objhook.hpp>
#include "cppwebenv.hpp"

#include <serial/stdtypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/continfo.hpp>

#if HAVE_NCBI_C
# include <asn.h>
# include "twebenv.h"
#else
# include <serial/test/Web_Env.hpp>
#endif

#include <test/test_assert.h>  /* This header must go last */

int main(int argc, char** argv)
{
    CTestSerial().AppMain(argc, argv);
	return 0;
}

void PrintAsn(CNcbiOstream& out, const CConstObjectInfo& object);


class CWriteSerialObjectHook : public CWriteObjectHook
{
public:
    CWriteSerialObjectHook(CTestSerialObject* hooked)
        : m_HookedObject(hooked) {}
    void WriteObject(CObjectOStream& out,
                     const CConstObjectInfo& object);
private:
    CTestSerialObject* m_HookedObject;
};


void CWriteSerialObjectHook::WriteObject(CObjectOStream& out,
                                         const CConstObjectInfo& object)
{
    const CTestSerialObject& obj = *reinterpret_cast<const CTestSerialObject*>
        (object.GetObjectPtr());
    
    if (&obj != m_HookedObject) {
        // This is not the object we would like to process.
        // Use default write method.
        LOG_POST("CWriteSerialObjectHook::WriteObject()"
            " -- using default writer");
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
            LOG_POST("CWriteSerialObjectHook::WriteObject()"
                " -- using CharBlock to write " << member.GetAlias());
            // Write the special member
            out.BeginClassMember(member.GetMemberInfo()->GetId());
            // Start char block, specify output stream and block size
            CObjectOStream::CharBlock cb(out, obj.m_Name.size());
                cb.Write(obj.m_Name.c_str(), obj.m_Name.size());   
            // End char block and member
            cb.End();
            out.EndClassMember();
        }
        else {
            // Do not need special processing for this member -- use
            // default write method. Do not write unset members (although
            // it is possible).
            if ( member.IsSet() ) {
                LOG_POST("CWriteSerialObjectHook::WriteObject()"
                    " -- using default member writer for " <<
                    member.GetAlias());
                out.WriteClassMember(member);
            }
        }
    }
    // Close the object
    out.EndClass();
}


class CReadSerialObjectHook : public CReadObjectHook
{
public:
    CReadSerialObjectHook(CTestSerialObject* hooked)
        : m_HookedObject(hooked) {}
    void ReadObject(CObjectIStream& in,
                    const CObjectInfo& object);
private:
    CTestSerialObject* m_HookedObject;
};


void CReadSerialObjectHook::ReadObject(CObjectIStream& in,
                                       const CObjectInfo& object)
{
    const CTestSerialObject& obj = *reinterpret_cast<const CTestSerialObject*>
        (object.GetObjectPtr());
    
    if (&obj != m_HookedObject) {
        // Use default read method.
        LOG_POST("CReadSerialObjectHook::ReadObject()"
            " -- using default reader");
        DefaultRead(in, object);
        return;
    }

    LOG_POST("CReadSerialObjectHook::ReadObject()"
        " -- using overloaded reader");
    // Read the object manually, member by member.
    for ( CIStreamClassMemberIterator i(in, object); i; ++i ) {
        const CObjectTypeInfo::CMemberIterator& nextMember = *i;
        LOG_POST("CReadSerialObjectHook::ReadObject()"
            " -- using default member reader for " <<
            nextMember.GetAlias());
        // Special pre-read processing may be put here
        in.ReadClassMember(CObjectInfoMI(object, nextMember.GetMemberIndex()));
        // Special post-read processing may be put here
    }
}


class CWriteSerialObject_NameHook : public CWriteClassMemberHook
{
public:
    virtual void WriteClassMember(CObjectOStream& out,
                                  const CConstObjectInfoMI& member);
};

void CWriteSerialObject_NameHook::WriteClassMember
    (CObjectOStream& out, const CConstObjectInfoMI& member)
{
    // No special processing -- just report writing the member
    const string& name = *static_cast<const string*>
        (member.GetMember().GetObjectPtr());
    LOG_POST("CWriteSerialObject_NameHook::WriteClassMember()"
        " -- writing m_Name: " << name);
    DefaultWrite(out, member);
}


class CReadSerialObject_NameHook : public CReadClassMemberHook
{
public:
    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfoMI& member);
};

void CReadSerialObject_NameHook::ReadClassMember
    (CObjectIStream& in, const CObjectInfoMI& member)
{
    // No special processing -- just report reading the member
    DefaultRead(in, member);
    const string& name = *reinterpret_cast<const string*>
        (member.GetMember().GetObjectPtr());
    LOG_POST("CReadSerialObject_NameHook::ReadClassMember()"
        " -- reading m_Name: " << name);
}


void TestHooks(CTestSerialObject& obj)
{
    // Test object hooks
    char* buf = 0;
    {{
        CNcbiOstrstream ostrs;
        auto_ptr<CObjectOStream> os(CObjectOStream::Open
            (eSerial_AsnText, ostrs));
        CObjectHookGuard<CTestSerialObject> w_hook
            (*(new CWriteSerialObjectHook(&obj)), &(*os));
        *os << obj;
		  os->FlushBuffer();
		  ostrs << '\0';
		  buf = ostrs.str();
    }}
    {{
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(CObjectIStream::Open
            (eSerial_AsnText, istrs));
        CTestSerialObject obj_copy;
        CObjectHookGuard<CTestSerialObject> r_hook
            (*(new CReadSerialObjectHook(&obj_copy)), &(*is));
        *is >> obj_copy;
#if HAVE_NCBI_C
        // Can not use SerialEquals<> with C-objects
#else
        _ASSERT(SerialEquals<CTestSerialObject>(obj, obj_copy));
#endif
    }}
    delete[] buf;

    // Test member hooks
    char* buf2 = 0;
    {{
        CNcbiOstrstream ostrs;
        auto_ptr<CObjectOStream> os(CObjectOStream::Open
            (eSerial_AsnText, ostrs));
        CObjectHookGuard<CTestSerialObject> w_hook
            ("m_Name", *(new CWriteSerialObject_NameHook), &(*os));
        *os << obj;
		  os->FlushBuffer();
		  ostrs << '\0';
        buf2 = ostrs.str();
    }}
    {{
        CNcbiIstrstream istrs(buf2);
        auto_ptr<CObjectIStream> is(CObjectIStream::Open
            (eSerial_AsnText, istrs));
        CTestSerialObject obj_copy;
        CObjectHookGuard<CTestSerialObject> r_hook
            ("m_Name", *(new CReadSerialObject_NameHook), &(*is));
        *is >> obj_copy;
#if HAVE_NCBI_C
        // Can not use SerialEquals<> with C-objects
#else
        _ASSERT(SerialEquals<CTestSerialObject>(obj, obj_copy));
#endif
    }}
    delete[] buf2;
}

int CTestSerial::Run(void)
{
    CNcbiOfstream diag("test_serial.log");
    SetDiagStream(&diag);
    try {
#if HAVE_NCBI_C
        WebEnv* env = 0;
        {
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("webenv.ent",
                                                                 eSerial_AsnText));
                in->Read(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("webenv.bino",
                                                                  eSerial_AsnBinary));
                out->Write(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                // C-style Object must be clean before loading: using new WebEnv instance
                WebEnv* env2 = 0;
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("webenv.bin",
                                                                 eSerial_AsnBinary));
                in->Read(&env2, GetSequenceTypeRef(&env2).Get());
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("webenv.ento",
                                                                  eSerial_AsnText));
                out->Write(&env2, GetSequenceTypeRef(&env2).Get());
            }
            {
                CNcbiOfstream out("webenv.ento2");
                PrintAsn(out,
                         CConstObjectInfo(&env,
                                          GetSequenceTypeRef(&env).Get()));
            }
        }
#else
        CRef<CWeb_Env> env(new CWeb_Env);
        {
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("webenv.ent",
                                                                 eSerial_AsnText));
                *in >> *env;
            }
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("webenv.bino",
                                                                  eSerial_AsnBinary));
                *out << *env;
            }
            {
                CRef<CWeb_Env> env2(new CWeb_Env);
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("webenv.bin",
                                                                 eSerial_AsnBinary));
                *in >> *env2;

                auto_ptr<CObjectOStream> out(CObjectOStream::Open("webenv.ento",
                                                                  eSerial_AsnText));
                out->SetAutoSeparator(false);
                *out << *env2;
            }
        }
        {
            CNcbiOfstream out("test_serial.asno2");
            PrintAsn(out, ConstObjectInfo(*env));
        }
#endif

        CTestSerialObject write;
        CTestSerialObject2 write1;

        _TRACE("CTestSerialObject(object1): " << long(&write));
        _TRACE("CTestSerialObject2(object2): " << long(&write1));
        _TRACE("CTestSerialObject(object2): " <<
               long(static_cast<CTestSerialObject*>(&write1)));

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

        TestHooks(write);

        const CTestSerialObject& cwrite = write;

        CTypeIterator<CTestSerialObject> j1; j1 = Begin(write);
        CTypeIterator<CTestSerialObject> j2(Begin(write));
        CTypeIterator<CTestSerialObject> j3 = Begin(write);

        //j1.Erase();

        CTypeConstIterator<CTestSerialObject> k1; k1 = Begin(write);
        CTypeConstIterator<CTestSerialObject> k2(Begin(write));
        CTypeConstIterator<CTestSerialObject> k3 = ConstBegin(write);

        //k1.Erase();

        CTypeConstIterator<CTestSerialObject> l1; l1 = ConstBegin(write);
        CTypeConstIterator<CTestSerialObject> l2(ConstBegin(write));
        CTypeConstIterator<CTestSerialObject> l3 = ConstBegin(write);

        CTypeConstIterator<CTestSerialObject> m1; m1 = ConstBegin(cwrite);
        CTypeConstIterator<CTestSerialObject> m2(ConstBegin(cwrite));
        CTypeConstIterator<CTestSerialObject> m3 = ConstBegin(cwrite);

        CTypesIterator n1; n1 = Begin(write);
        CTypesConstIterator n2; n2 = ConstBegin(write);
        CTypesConstIterator n3; n3 = ConstBegin(cwrite);
        //CTypesConstIterator n4; n4 = Begin(cwrite);

        {
            for ( CTypeIterator<CTestSerialObject> oi = Begin(write); oi; ++oi ) {
                const CTestSerialObject& obj = *oi;
                NcbiCerr<<"CTestSerialObject @ "<<NStr::PtrToString(&obj)<<
                    NcbiEndl;
                //            oi.Erase();
                CTypeIterator<CTestSerialObject> oi1(Begin(write));
                CTypeIterator<CTestSerialObject> oi2;
                oi2 = Begin(write);
            }
        }
        {
            for ( CTypeConstIterator<CTestSerialObject> oi = ConstBegin(write); oi;
                  ++oi ) {
                const CTestSerialObject& obj = *oi;
                NcbiCerr<<"CTestSerialObject @ "<<NStr::PtrToString(&obj)<<
                    NcbiEndl;
                //            oi.Erase();
                CTypeConstIterator<CTestSerialObject> oi1(Begin(write));
                CTypeConstIterator<CTestSerialObject> oi2;
                oi2 = Begin(write);
            }
        }
        {
            for ( CTypeConstIterator<CTestSerialObject> oi = ConstBegin(cwrite);
                  oi; ++oi ) {
                const CTestSerialObject& obj = *oi;
                NcbiCerr<<"CTestSerialObject @ "<<NStr::PtrToString(&obj)<<
                    NcbiEndl;
                //oi.Erase();
                CTypeConstIterator<CTestSerialObject> oi1(ConstBegin(cwrite));
                CTypeConstIterator<CTestSerialObject> oi2;
                oi2 = ConstBegin(cwrite);
            }
        }

        {
            for ( CStdTypeConstIterator<string> si = ConstBegin(cwrite); si;
                  ++si ) {
                NcbiCerr <<"String: \""<<*si<<"\""<<NcbiEndl;
            }
        }

        {
            for ( CObjectConstIterator oi = ConstBegin(cwrite); oi; ++oi ) {
                const CObject& obj = *oi;
                NcbiCerr <<"CObject: @ "<<NStr::PtrToString(&obj)<<NcbiEndl;
            }
        }

        {
            CTypesIterator i;
            CType<CTestSerialObject>::AddTo(i);
            CType<CTestSerialObject2>::AddTo(i);
            for ( i = Begin(write); i; ++i ) {
                CTestSerialObject* obj = CType<CTestSerialObject>::Get(i);
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
                const CTestSerialObject* obj = CType<CTestSerialObject>::Get(i);
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
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("test_serial.asno",
                                                                  eSerial_AsnText));
                *out << write;
            }
            {
                CNcbiOfstream out("test_serial.asno2");
                PrintAsn(out, ConstObjectInfo(write));
            }
            CTestSerialObject read;
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test_serial.asn",
                                                                 eSerial_AsnText));
                *in >> read;
            }
#if HAVE_NCBI_C
            // Some functions does not work with C-style objects.
            // Reset WebEnv for SerialEquals() to work.
            TWebEnv* saved_env_read = read.m_WebEnv;
            TWebEnv* saved_env_write = write.m_WebEnv;
            read.m_WebEnv = 0;
            write.m_WebEnv = 0;
#endif
            _ASSERT(SerialEquals<CTestSerialObject>(read, write));
#if HAVE_NCBI_C
            // Restore C-style objects
            read.m_WebEnv = saved_env_read;
            write.m_WebEnv = saved_env_write;
#endif
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
#if !defined(HAVE_NCBI_C)
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test_serial.asn",
                                                                 eSerial_AsnText));
                in->Skip(ObjectType(read));
            }
#endif
        }

        {
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("test_serial.asbo",
                                                                  eSerial_AsnBinary));
                *out << write;
            }
            CTestSerialObject read;
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test_serial.asb",
                                                                 eSerial_AsnBinary));
                *in >> read;
            }
#if HAVE_NCBI_C
            // Some functions does not work with C-style objects.
            // Reset WebEnv for SerialEquals() to work.
            TWebEnv* saved_env_read = read.m_WebEnv;
            TWebEnv* saved_env_write = write.m_WebEnv;
            read.m_WebEnv = 0;
            write.m_WebEnv = 0;
#endif
            _ASSERT(SerialEquals<CTestSerialObject>(read, write));
#if HAVE_NCBI_C
            // Restore C-style objects
            read.m_WebEnv = saved_env_read;
            write.m_WebEnv = saved_env_write;
#endif
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
#if !defined(HAVE_NCBI_C)
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test_serial.asb",
                                                                 eSerial_AsnBinary));
                in->Skip(ObjectType(read));
            }
#endif
        }

        NcbiCerr << "OK" << endl;
    }
    catch (NCBI_NS_STD::exception& e) {
        ERR_POST(typeid(e).name() << ": " << e.what());
    }
    catch (...) {
        ERR_POST("Unknown error");
    }
    SetDiagStream(&NcbiCerr);
    return 0;
}

void PrintAsnValue(CNcbiOstream& out, const CConstObjectInfo& object);
static int asnIndentLevel;

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
    _ASSERT(asnIndentLevel > 0);
    asnIndentLevel--;
}

void PrintAsnIndent(CNcbiOstream& out)
{
    for ( int i = 0; i < asnIndentLevel; ++i )
        out << "  ";
}

void PrintAsn(CNcbiOstream& out, const CConstObjectInfo& object)
{
    _ASSERT(object);
    _ASSERT(!object.GetTypeInfo()->GetName().empty());
    ResetAsnIndent();
    out << object.GetTypeInfo()->GetName() << " ::= ";
    PrintAsnValue(out, object);
}

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

void PrintAsnPrimitiveValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnClassValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnChoiceValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnContainerValue(CNcbiOstream& out, const CConstObjectInfo& object);
void PrintAsnPointerValue(CNcbiOstream& out, const CConstObjectInfo& object);

void PrintAsnValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    _ASSERT(object);
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

static const char Hex[] = "0123456789ABCDEF";

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



/*
 * ===========================================================================
 * $Log$
 * Revision 1.65  2005/04/27 16:07:16  gouriano
 * Generated required sources automatically
 *
 * Revision 1.64  2004/05/17 21:03:33  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.63  2004/01/21 15:18:15  grichenk
 * Re-arranged webenv files for serial test
 *
 * Revision 1.62  2003/06/16 16:53:46  gouriano
 * corrections to make test run OK
 *
 * Revision 1.61  2003/06/13 20:44:33  ivanov
 * Changed names of the test files from "test.*" to "test_serial.*"
 *
 * Revision 1.60  2003/06/02 16:48:51  lavr
 * Renamed testserial -> test_serial
 *
 * Revision 1.59  2003/03/11 20:07:11  kuznets
 * iterate -> ITERATE
 *
 * Revision 1.58  2002/12/30 22:41:58  vakatov
 * Fixed for absent terminating '\0' when calling strstream::str().
 * Added standard NCBI header and log.
 *
 * ===========================================================================
 */
