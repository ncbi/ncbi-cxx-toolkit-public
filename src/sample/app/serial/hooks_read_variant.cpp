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
#if 0
// or skip it
//        strm.SkipAnyContentVariant();
// or like this
        strm.Skip(passed_info.GetVariantType().GetTypeInfo(), CObjectIStream::eNoFileHeader);

// get information about the member
        // typeinfo of the parent class (Date)
        CObjectTypeInfo oti = passed_info.GetChoiceType();
        // typeinfo and data of the parent class
        const CObjectInfo& oi = passed_info.GetChoiceObject();
        // typeinfo of the variant (string)
        CObjectTypeInfo omti = passed_info.GetVariantType();
        // typeinfo and data of the variant
        CObjectInfo om = passed_info.GetVariant();
        // index of the variant in parent class
        TMemberIndex mi = passed_info.GetVariantIndex();
        // information about the variant, including its name (str)
        const CVariantInfo* minfo = passed_info.GetVariantInfo();
#endif
    }
};

int main(int argc, char** argv)
{
    char asn[] = "Date ::= str \"late-spring\"";
    CNcbiIstrstream iss(asn);
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, iss));

    CObjectTypeInfo(CType<CDate>()).FindVariant("str")
                                   .SetLocalReadHook(*in, new CDemoHook());

    CDate my_date;
    *in >> my_date;

    return 0;
}
