#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objostrb.hpp>
#include <serial/objistrb.hpp>
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
    SetDiagStream(&NcbiCerr);
    try {
        CSerialObject write;

        write.m_Name = "name";
        write.m_NamePtr = &write.m_Name;
        write.m_Size = -1;
        write.m_Attributes.push_back("m_Attributes");
        write.m_Attributes.push_back("m_Size");
        write.m_Attributes.push_back("m_");

        {
            {
                CObjectOStreamBinary(ofstream("test.bin")) << write;
            }
            CSerialObject read;
            {
                CObjectIStreamBinary(ifstream("test.bin")) >> read;
            }
            read.Dump(cerr);
        }

        {
            {
                CObjectOStreamAsn(ofstream("test.asn")) << write;
            }
            CSerialObject read;
            {
                CObjectIStreamAsn(ifstream("test.asn")) >> read;
            }
            read.Dump(cerr);
        }
    }
    catch (exception& e) {
        ERR_POST(typeid(e).name() << ": " << e.what());
    }
    catch (...) {
        ERR_POST("Unknown error");
    }
    return 0;
}
