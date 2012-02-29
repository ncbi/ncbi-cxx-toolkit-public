#include <ncbi_pch.hpp>
#include <objects/general/Date_std.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CReadObjectHook
{
public:
    virtual void ReadObject(CObjectIStream& strm,
                            const CObjectInfo& passed_info)
    {
        DefaultRead(strm, passed_info);
    }
};

int main(int argc, char** argv)
{
    char asn[] = "Date-std ::= { year 1998 }";
    CNcbiIstrstream iss(asn);
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, iss));

    CObjectTypeInfo(CType<CDate_std>()).SetLocalReadHook(*in, new CDemoHook());

    CDate_std my_date;
    *in >> my_date;

    return 0;
}
