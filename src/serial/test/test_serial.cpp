#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objistrb.hpp>
#include <serial/objostrb.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objostrasn.hpp>
#include "webenv.hpp"

#include <asn.h>
#include <webenv.h>

#include <fstream>

int main(int argc, char** argv)
{
    CTestSerial().AppMain(argc, argv);
	return 0;
}

int CTestSerial::Run(void)
{
    ofstream diag("test.log");
    SetDiagStream(&diag);
    try {
        /*
        WebEnv* env = 0;
        {
            {
                ifstream in("webenv.ent");
                CObjectIStreamAsn(in).
                    Read(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                ofstream out("webenv.bino", ios::out | ios::binary);
                CObjectOStreamAsnBinary(out).
                    Write(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                ifstream in("webenv.bin", ios::in | ios::binary);
                CObjectIStreamAsnBinary(in).
                    Read(&env, GetSequenceTypeRef(&env).Get());
            }
            {
                ofstream out("webenv.ento");
                CObjectOStreamAsn(out).
                    Write(&env, GetSequenceTypeRef(&env).Get());
            }
        }
        */

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
        write.m_WebEnv = 0; //env;

        write1.m_Name = "write1";
        write1.m_NamePtr = new string("test");
        write1.m_Size = 0x7fff;
        write1.m_Attributes.push_back("write1");
        write1.m_Next = &write1;
        write1.m_WebEnv = 0;
        write1.m_Name2 = "name2";

        {
            {
                ofstream out("test.asno");
                CObjectOStreamAsn oout(out);
                oout << write;
            }
            CSerialObject read;
            {
                ifstream in("test.asno");
                CObjectIStreamAsn iin(in);
                iin >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                ifstream in("test.asno");
                CObjectIStreamAsn(in).SkipValue();
            }
        }

        {
            {
                ofstream out("test.asnb", ios::binary);
                CObjectOStreamAsnBinary oout(out);
                oout << write;
            }
            CSerialObject read;
            {
                ifstream in("test.asnb", ios::binary);
                CObjectIStreamAsnBinary iin(in);
                iin >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                ifstream in("test.asnb", ios::binary);
                CObjectIStreamAsnBinary(in).SkipValue();
            }
        }

        {
            {
                ofstream out("test.bin", ios::binary);
                CObjectOStreamBinary oout(out);
                oout << write;
            }
            CSerialObject read;
            {
                ifstream in("test.bin", ios::binary);
                CObjectIStreamBinary iin(in);
                iin >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
            {
                ifstream in("test.bin", ios::binary);
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
