#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objistrb.hpp>
#include <serial/objostrb.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objostrasn.hpp>
#include "cppwebenv.hpp"

#if HAVE_NCBI_C
# include <asn.h>
# include <twebenv.h>
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
                CNcbiIfstream in("webenv.ent");
                if ( !in )
                    THROW1_TRACE(runtime_error, "File not found webenv.ent");
                CObjectIStreamAsn(in).
                    Read(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                CNcbiOfstream out("webenv.bino", ios::out | ios::binary);
                if ( !out )
                    THROW1_TRACE(runtime_error, "Cannot create webenv.bino");
                CObjectOStreamAsnBinary(out).
                    Write(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                CNcbiIfstream in("webenv.bin", ios::in | ios::binary);
                if ( !in )
                    THROW1_TRACE(runtime_error, "File not found webenv.bin");
                CObjectIStreamAsnBinary(in).
                    Read(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                CNcbiOfstream out("webenv.ento");
                if ( !out )
                    THROW1_TRACE(runtime_error, "Cannot create webenv.ento");
                CObjectOStreamAsn(out).
                    Write(&env, GetSequenceTypeRef(&env).Get());
            }
        }
#endif

        CSerialObject write;
        CSerialObject2 write1;

        _TRACE("CSerialObject(object1): " << long(&write));
        _TRACE("CSerialObject2(object2): " << long(&write1));
        _TRACE("CSerialObject(object2): " << long(static_cast<CSerialObject*>(&write1)));

        write.m_Name = "name";
        write.m_NamePtr = &write1.m_Name;
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
        write1.m_Next = &write1;
#if HAVE_NCBI_C
        write1.m_WebEnv = 0;
#endif
        write1.m_Name2 = "name2";

        {
            {
                CNcbiOfstream out("test.asno");
                if ( !out )
                    THROW1_TRACE(runtime_error, "Cannot create test.asno");
                CObjectOStreamAsn oout(out);
                oout << write;
            }
            CSerialObject read;
            {
                CNcbiIfstream in("test.asn");
                if ( !in )
                    THROW1_TRACE(runtime_error, "File not found test.asn");
                CObjectIStreamAsn iin(in);
                iin >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                CNcbiIfstream in("test.asn");
                if ( !in )
                    THROW1_TRACE(runtime_error, "File not found test.asn");
                CObjectIStreamAsn(in).SkipValue();
            }
        }

        {
            {
                CNcbiOfstream out("test.asbo", ios::binary);
                if ( !out )
                    THROW1_TRACE(runtime_error, "Cannot create test.asbo");
                CObjectOStreamAsnBinary oout(out);
                oout << write;
            }
            CSerialObject read;
            {
                CNcbiIfstream in("test.asb", ios::binary);
                if ( !in )
                    THROW1_TRACE(runtime_error, "File not found test.asb");
                CObjectIStreamAsnBinary iin(in);
                iin >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                CNcbiIfstream in("test.asb", ios::binary);
                if ( !in )
                    THROW1_TRACE(runtime_error, "File not found test.asb");
                CObjectIStreamAsnBinary(in).SkipValue();
            }
        }

        {
            {
                CNcbiOfstream out("test.bino", ios::binary);
                if ( !out )
                    THROW1_TRACE(runtime_error, "Cannot create test.bino");
                CObjectOStreamBinary oout(out);
                oout << write;
            }
            CSerialObject read;
            {
                CNcbiIfstream in("test.bin", ios::binary);
                if ( !in )
                    THROW1_TRACE(runtime_error, "File not found test.bin");
                CObjectIStreamBinary iin(in);
                iin >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                CNcbiIfstream in("test.bin", ios::binary);
                if ( !in )
                    THROW1_TRACE(runtime_error, "File not found test.bin");
                CObjectIStreamBinary(in).SkipValue();
            }
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
