#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objostrb.hpp>
#include <serial/objistrb.hpp>
#include <serial/objostrasn.hpp>

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
        CObjectOStreamAsn(cout) << write;
        CSerialObject read;
        CObjectIStreamBinary(cin) >> read;
        read.Dump(cerr);
    }
    catch (exception& e) {
        ERR_POST(typeid(e).name() << ": " << e.what());
    }
    catch (...) {
        ERR_POST("Unknown error");
    }
    return 0;
}
