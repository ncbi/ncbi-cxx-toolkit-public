#include <ncbi_pch.hpp>
#include <objects/general/Date.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CReadChoiceVariantHook
{
public:
    virtual void ReadChoiceVariant(CObjectIStream& strm,
                                   const CObjectInfoCV& passed_info)
    {
#if 1
        DefaultRead(strm, passed_info);
#else
#if 1
// or skip it
        DefaultSkip(strm, passed_info);
#endif
#if 0
// read the object into local buffer
// this data will be discarded when this function terminates
// so the choice variant of the object being read will be invalid
        CObjectInfo obj(passed_info.GetVariantType());
        strm.ReadObject(obj);
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        out->WriteObject(obj);
#endif
#if 0
// or read it into delay buffer
        strm.StartDelayBuffer();
        DefaultSkip(strm, passed_info);
        CRef<CByteSource> data = strm.EndDelayBuffer();
#endif

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
