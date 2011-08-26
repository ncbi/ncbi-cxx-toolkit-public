#include <ncbi_pch.hpp>
#include <objects/general/Date.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CReadChoiceVariantHook
{
public:
    virtual void ReadChoiceVariant(CObjectIStream& strm,
                                   const CObjectInfoCV& passed_info)
    {
        DefaultRead(strm, passed_info);
    }
};

int main(int argc, char** argv)
{
    char asn[] = "Date ::= str \"late-spring\"";
    CNcbiIstrstream iss(asn);
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, iss));

    CObjectTypeInfo(CType<CDate>()).FindVariant("str")
                                   .SetLocalReadHook(*in, new CDemoHook());

    CDate my_date;
    *in >> my_date;

    return 0;
}
