#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/serialasn.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/object.hpp>
#include <serial/iterator.hpp>
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
# include "Web_Env.hpp"
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
    CWriteSerialObjectHook(CSerialObject* hooked)
        : m_HookedObject(hooked) {}
    void WriteObject(CObjectOStream& out,
                     const CConstObjectInfo& object);
private:
    CSerialObject* m_HookedObject;
};


void CWriteSerialObjectHook::WriteObject(CObjectOStream& out,
                                         const CConstObjectInfo& object)
{
    const CSerialObject& obj = *reinterpret_cast<const CSerialObject*>
        (object.GetObjectPtr());
    
    if (&obj != m_HookedObject) {
        // This is not the object we would like to process.
        // Use default write method.
        LOG_POST("CWriteSerialObjectHook::WriteObject()"
            " -- using default writer");
        object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
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
    CReadSerialObjectHook(CSerialObject* hooked)
        : m_HookedObject(hooked) {}
    void ReadObject(CObjectIStream& in,
                    const CObjectInfo& object);
private:
    CSerialObject* m_HookedObject;
};


void CReadSerialObjectHook::ReadObject(CObjectIStream& in,
                                       const CObjectInfo& object)
{
    const CSerialObject& obj = *reinterpret_cast<const CSerialObject*>
        (object.GetObjectPtr());
    
    if (&obj != m_HookedObject) {
        // Use default read method.
        LOG_POST("CReadSerialObjectHook::ReadObject()"
            " -- using default reader");
        object.GetTypeInfo()->DefaultReadData(in, object.GetObjectPtr());
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
    out.WriteClassMember(member);
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
    in.ReadClassMember(member);
    const string& name = *reinterpret_cast<const string*>
        (member.GetMember().GetObjectPtr());
    LOG_POST("CReadSerialObject_NameHook::ReadClassMember()"
        " -- reading m_Name: " << name);
}


void TestHooks(CSerialObject& obj)
{
    // Test object hooks
    char* buf = 0;
    {{
        CNcbiOstrstream ostrs;
        auto_ptr<CObjectOStream> os(CObjectOStream::Open
            (eSerial_AsnText, ostrs));
        CObjectTypeInfo info = Type<CSerialObject>();
        info.SetLocalWriteHook(*os, new CWriteSerialObjectHook(&obj));
        *os << obj;
        buf = ostrs.rdbuf()->str();
    }}
    {{
        CNcbiIstrstream istrs(buf);
        auto_ptr<CObjectIStream> is(CObjectIStream::Open
            (eSerial_AsnText, istrs));
        CSerialObject obj_copy;
        CObjectTypeInfo info = Type<CSerialObject>();
        info.SetLocalReadHook(*is, new CReadSerialObjectHook(&obj_copy));
        *is >> obj_copy;
#if HAVE_NCBI_C
        // Can not use SerialEquals<> with C-objects
#else
        _ASSERT(SerialEquals<CSerialObject>(obj, obj_copy));
#endif
    }}
    delete[] buf;

    // Test member hooks
    char* buf2 = 0;
    {{
        CNcbiOstrstream ostrs;
        auto_ptr<CObjectOStream> os(CObjectOStream::Open
            (eSerial_AsnText, ostrs));
        CObjectTypeInfo info = Type<CSerialObject>();
        info.FindMember("m_Name").SetLocalWriteHook(*os,
            new CWriteSerialObject_NameHook);
        *os << obj;
        buf2 = ostrs.rdbuf()->str();
    }}
    {{
        CNcbiIstrstream istrs(buf2);
        auto_ptr<CObjectIStream> is(CObjectIStream::Open
            (eSerial_AsnText, istrs));
        CSerialObject obj_copy;
        CObjectTypeInfo info = Type<CSerialObject>();
        info.FindMember("m_Name").SetLocalReadHook(*is,
            new CReadSerialObject_NameHook);
        *is >> obj_copy;
#if HAVE_NCBI_C
        // Can not use SerialEquals<> with C-objects
#else
        _ASSERT(SerialEquals<CSerialObject>(obj, obj_copy));
#endif
    }}
    delete[] buf2;
}

int CTestSerial::Run(void)
{
    CNcbiOfstream diag("test.log");
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
                *out << *env2;
            }
        }
        {
            CNcbiOfstream out("test.asno2");
            PrintAsn(out, ConstObjectInfo(*env));
        }
#endif

        CSerialObject write;
        CSerialObject2 write1;

        _TRACE("CSerialObject(object1): " << long(&write));
        _TRACE("CSerialObject2(object2): " << long(&write1));
        _TRACE("CSerialObject(object2): " <<
               long(static_cast<CSerialObject*>(&write1)));

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

        const CSerialObject& cwrite = write;

        CTypeIterator<CSerialObject> j1; j1 = Begin(write);
        CTypeIterator<CSerialObject> j2(Begin(write));
        CTypeIterator<CSerialObject> j3 = Begin(write);

        //j1.Erase();

        CTypeConstIterator<CSerialObject> k1; k1 = Begin(write);
        CTypeConstIterator<CSerialObject> k2(Begin(write));
        CTypeConstIterator<CSerialObject> k3 = ConstBegin(write);

        //k1.Erase();

        CTypeConstIterator<CSerialObject> l1; l1 = ConstBegin(write);
        CTypeConstIterator<CSerialObject> l2(ConstBegin(write));
        CTypeConstIterator<CSerialObject> l3 = ConstBegin(write);

        CTypeConstIterator<CSerialObject> m1; m1 = ConstBegin(cwrite);
        CTypeConstIterator<CSerialObject> m2(ConstBegin(cwrite));
        CTypeConstIterator<CSerialObject> m3 = ConstBegin(cwrite);

        CTypesIterator n1; n1 = Begin(write);
        CTypesConstIterator n2; n2 = ConstBegin(write);
        CTypesConstIterator n3; n3 = ConstBegin(cwrite);
        //CTypesConstIterator n4; n4 = Begin(cwrite);

        {
            for ( CTypeIterator<CSerialObject> oi = Begin(write); oi; ++oi ) {
                const CSerialObject& obj = *oi;
                NcbiCerr<<"CSerialObject @ "<<NStr::PtrToString(&obj)<<
                    NcbiEndl;
                //            oi.Erase();
                CTypeIterator<CSerialObject> oi1(Begin(write));
                CTypeIterator<CSerialObject> oi2;
                oi2 = Begin(write);
            }
        }
        {
            for ( CTypeConstIterator<CSerialObject> oi = ConstBegin(write); oi;
                  ++oi ) {
                const CSerialObject& obj = *oi;
                NcbiCerr<<"CSerialObject @ "<<NStr::PtrToString(&obj)<<
                    NcbiEndl;
                //            oi.Erase();
                CTypeConstIterator<CSerialObject> oi1(Begin(write));
                CTypeConstIterator<CSerialObject> oi2;
                oi2 = Begin(write);
            }
        }
        {
            for ( CTypeConstIterator<CSerialObject> oi = ConstBegin(cwrite);
                  oi; ++oi ) {
                const CSerialObject& obj = *oi;
                NcbiCerr<<"CSerialObject @ "<<NStr::PtrToString(&obj)<<
                    NcbiEndl;
                //oi.Erase();
                CTypeConstIterator<CSerialObject> oi1(ConstBegin(cwrite));
                CTypeConstIterator<CSerialObject> oi2;
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
            Type<CSerialObject>::AddTo(i);
            Type<CSerialObject2>::AddTo(i);
            for ( i = Begin(write); i; ++i ) {
                CSerialObject* obj = Type<CSerialObject>::Get(i);
                if ( obj ) {
                    NcbiCerr <<"CObject: @ "<<NStr::PtrToString(obj)<<NcbiEndl;
                }
                else {
                    NcbiCerr <<"CObject2: @ "<<
                        NStr::PtrToString(Type<CSerialObject2>::Get(i))<<
                        NcbiEndl;
                }
            }
        }

        {
            CTypesConstIterator i;
            Type<CSerialObject>::AddTo(i);
            Type<CSerialObject2>::AddTo(i);
            for ( i = ConstBegin(write); i; ++i ) {
                const CSerialObject* obj = Type<CSerialObject>::Get(i);
                if ( obj ) {
                    NcbiCerr <<"CObject: @ "<<NStr::PtrToString(obj)<<NcbiEndl;
                }
                else {
                    NcbiCerr <<"CObject2: @ "<<
                        NStr::PtrToString(Type<CSerialObject2>::Get(i))<<
                        NcbiEndl;
                }
            }
        }

        {
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("test.asno",
                                                                  eSerial_AsnText));
                *out << write;
            }
            {
                CNcbiOfstream out("test.asno2");
                PrintAsn(out, ConstObjectInfo(write));
            }
            CSerialObject read;
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.asn",
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
            _ASSERT(SerialEquals<CSerialObject>(read, write));
#if HAVE_NCBI_C
            // Restore C-style objects
            read.m_WebEnv = saved_env_read;
            write.m_WebEnv = saved_env_write;
#endif
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
#if !defined(HAVE_NCBI_C)
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.asn",
                                                                 eSerial_AsnText));
                in->Skip(ObjectType(read));
            }
#endif
        }

        {
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("test.asbo",
                                                                  eSerial_AsnBinary));
                *out << write;
            }
            CSerialObject read;
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.asb",
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
            _ASSERT(SerialEquals<CSerialObject>(read, write));
#if HAVE_NCBI_C
            // Restore C-style objects
            read.m_WebEnv = saved_env_read;
            write.m_WebEnv = saved_env_write;
#endif
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
#if !defined(HAVE_NCBI_C)
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.asb",
                                                                 eSerial_AsnBinary));
                in->Skip(ObjectType(read));
            }
#endif
        }

        NcbiCerr << "OK" << endl;
    }
    catch (exception& e) {
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
            iterate ( vector<char>, i, s ) {
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
