#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objostrb.hpp>
#include <serial/objistrb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objistrasn.hpp>
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
        CSerialObject write;
        CSerialObject write1;

        write.m_Name = "name";
        write.m_NamePtr = &write1.m_Name;
        write.m_Size = -1;
        write.m_Attributes.push_back("m_Attributes");
        write.m_Attributes.push_back("m_Size");
        write.m_Attributes.push_back("m_");
        write.m_Next = &write1;

        write1.m_Name = "write1";
        write1.m_NamePtr = new string("test");
        write1.m_Size = 0x7fffffff;
        write1.m_Attributes.push_back("write1");
        write1.m_Next = &write1;

        {
            {
                CObjectOStreamBinary(ofstream("test.bin")) << write;
            }
            CSerialObject read;
            {
                CObjectIStreamBinary(ifstream("test.bin")) >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
        }

        {
            {
                CObjectOStreamAsnBinary(ofstream("test.asnb")) << write;
            }
            CSerialObject read;
            {
                CObjectIStreamAsn(ifstream("test.asn")) >> read;
            }
            read.Dump(NcbiCerr);
            read.m_Next->Dump(NcbiCerr);
        }
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
