#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/iterator.hpp>
#include "cppwebenv.hpp"

#if HAVE_NCBI_C
# include <asn.h>
# include "twebenv.h"
#endif

int main(int argc, char** argv)
{
    CTestSerial().AppMain(argc, argv);
	return 0;
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
        }
#endif

        CSerialObject write;
        CSerialObject2 write1;

        _TRACE("CSerialObject(object1): " << long(&write));
        _TRACE("CSerialObject2(object2): " << long(&write1));
        _TRACE("CSerialObject(object2): " <<
               long(static_cast<CSerialObject*>(&write1)));

        write.m_Name = "name";
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
        write1.m_NamePtr = new string("test");
        write1.m_Size = 0x7fff;
        write1.m_Attributes.push_back("write1");
        //        write1.m_Next = &write1;
#if HAVE_NCBI_C
        write1.m_WebEnv = 0;
#endif
        write1.m_Name2 = "name2";

        const CSerialObject& cwrite = write;

        {
            for ( CTypeIterator<CSerialObject> oi = Begin(write); oi; ++oi ) {
                const CSerialObject& obj = *oi;
                NcbiCerr<<"CSerialObject @ "<<NStr::PtrToString(&obj)<<NcbiEndl;
                //            oi.Erase();
                CTypeIterator<CSerialObject> oi1(Begin(write));
                CTypeIterator<CSerialObject> oi2;
                oi2 = Begin(write);
            }
        }
        {
            for ( CTypeConstIterator<CSerialObject> oi = Begin(write); oi;
                  ++oi ) {
                const CSerialObject& obj = *oi;
                NcbiCerr<<"CSerialObject @ "<<NStr::PtrToString(&obj)<<NcbiEndl;
                //            oi.Erase();
                CTypeConstIterator<CSerialObject> oi1(Begin(write));
                CTypeConstIterator<CSerialObject> oi2;
                oi2 = Begin(write);
            }
        }
        {
            for ( CTypeConstIterator<CSerialObject> oi = ConstBegin(cwrite); oi;
                  ++oi ) {
                const CSerialObject& obj = *oi;
                NcbiCerr<<"CSerialObject @ "<<NStr::PtrToString(&obj)<<NcbiEndl;
                //oi.Erase();
                CTypeConstIterator<CSerialObject> oi1(ConstBegin(cwrite));
                CTypeConstIterator<CSerialObject> oi2;
                oi2 = ConstBegin(cwrite);
            }
        }

        {
            for ( CStdTypeConstIterator<string> si = ConstBegin(cwrite); si; ++si ) {
                NcbiCerr <<"String: \""<<*si<<"\""<<NcbiEndl;
            }
        }

        {
            for ( CObjectsConstIterator oi = ConstBegin(cwrite); oi; ++oi ) {
                const CObject& obj = *oi;
                NcbiCerr <<"CObject: @ "<<NStr::PtrToString(&obj)<<NcbiEndl;
            }
        }

        {
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open("test.asno",
                                                                  eSerial_AsnText));
                *out << write;
            }
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
