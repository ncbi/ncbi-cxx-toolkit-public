#include "testserial.hpp"
#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/objostrb.hpp>

int main(int argc, char** argv)
{
    CTestSerial().AppMain(argc, argv);
}

int CTestSerial::Run(void)
{
    SetDiagStream(&NcbiCerr);
    try {
        CSerialObject t;
        CObjectOStreamBinary(cout) << t;
    }
    catch (exception e) {
        NcbiCerr << typeid(e).name() << ": " << e.what() << endl;
    }
    catch (...) {
        NcbiCerr << "Unknown error" << endl;
    }
    return 0;
}
