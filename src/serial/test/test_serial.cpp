#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
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
#endif

int main(int argc, char** argv)
{
    CTestSerial().AppMain(argc, argv);
	return 0;
}

void PrintAsn(CNcbiOstream& out, const CConstObjectInfo& object);

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
                in->Read(&env, GetSequenceTypeRef(&env));
            }
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("webenv.bino",
                                                                  eSerial_AsnBinary));
                out->Write(&env, GetSequenceTypeRef(&env));
            }
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("webenv.bin",
                                                                 eSerial_AsnBinary));
                in->Read(&env, GetSequenceTypeRef(&env));
            }
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("webenv.ento",
                                                                  eSerial_AsnText));
                out->Write(&env, GetSequenceTypeRef(&env));
            }
            {
                CNcbiOfstream out("webenv.ento2");
                PrintAsn(out,
                         CConstObjectInfo(&env,
                                          GetSequenceTypeRef(&env).Get()));
            }
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
#if HAVE_NCBI_C
        write.m_WebEnv = env;
#endif

        write1.m_Name = "write1";
        write1.m_HaveName = true;
        write1.m_NamePtr = new string("test");
        write1.m_Size = 0x7fff;
        write1.m_Attributes.push_back("write1");
        //        write1.m_Next = &write1;
#if HAVE_NCBI_C
        write1.m_WebEnv = 0;
#endif
        write1.m_Name2 = "name2";

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
#if 0
            {
                CNcbiOfstream out("test.asno2");
                PrintAsn(out, ConstObjectInfo(write));
            }
#endif
            CSerialObject read;
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.asn",
                                                                 eSerial_AsnText));
                *in >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.asn",
                                                                 eSerial_AsnText));
                in->SkipValue();
            }
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
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.asb",
                                                                 eSerial_AsnBinary));
                in->SkipValue();
            }
        }

/*
        {
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("test.bino",
                                    eSerial_AsnBinary));
                *out << write;
            }
            CSerialObject read;
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.bin",
                                    eSerial_AsnBinary));
                *in >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open("test.bin",
                                    eSerial_AsnBinary));
                in->SkipValue();
            }
        }
*/
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
    case CTypeInfo::eTypePrimitive:
        PrintAsnPrimitiveValue(out, object);
        break;
    case CTypeInfo::eTypeClass:
        PrintAsnClassValue(out, object);
        break;
    case CTypeInfo::eTypeChoice:
        PrintAsnChoiceValue(out, object);
        break;
    case CTypeInfo::eTypeContainer:
        PrintAsnContainerValue(out, object);
        break;
    case CTypeInfo::eTypePointer:
        PrintAsnPointerValue(out, object);
        break;
    }
}

static const char Hex[] = "0123456789ABCDEF";

void PrintAsnPrimitiveValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    switch ( object.GetPrimitiveValueType() ) {
    case CPrimitiveTypeInfo::eBool:
        if ( object.GetPrimitiveValueBool() )
            out << "TRUE";
        else
            out << "FALSE";
        break;
    case CPrimitiveTypeInfo::eChar:
        out << '\'' << object.GetPrimitiveValueChar() << '\'';
        break;
    case CPrimitiveTypeInfo::eInteger:
        if ( object.IsPrimitiveValueSigned() )
            out << object.GetPrimitiveValueLong();
        else
            out << object.GetPrimitiveValueULong();
        break;
    case CPrimitiveTypeInfo::eReal:
        out << object.GetPrimitiveValueDouble();
        break;
    case CPrimitiveTypeInfo::eString:
        out << '"' << object.GetPrimitiveValueString() << '"';
        break;
    case CPrimitiveTypeInfo::eEnum:
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
    case CPrimitiveTypeInfo::eOctetString:
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

    for ( CConstObjectInfo::CMemberIterator i(object); i; i.Next() ) {
        if ( i.IsSet() ) {
            // print member separator
            block.NextElement();

            // print member id if any
            const CMemberId& id = i.GetId();
            if ( !id.GetName().empty() )
                out << id.GetName() << ' ';

            // print member value
            PrintAsnValue(out, *i);
        }
    }
}

void PrintAsnChoiceValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    int index = object.WhichChoice();
    _ASSERT(index >= 0);
    out << object.GetMemberId(index).GetName() << ' ';
    PrintAsnValue(out, object.GetCurrentChoiceVariant());
}

void PrintAsnContainerValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    AsnBlock block(out);
    
    for ( CConstObjectInfo::CElementIterator i(object); i; i.Next() ) {
        block.NextElement();
        
        PrintAsnValue(out, *i);
    }
}

void PrintAsnPointerValue(CNcbiOstream& out, const CConstObjectInfo& object)
{
    const CPointerTypeInfo* info = object.GetPointerTypeInfo();
    CConstObjectInfo pointedObject(object);
    info->GetPointedObject(pointedObject);
    PrintAsnValue(out, object.GetPointedObject());
}
