#include <ncbi_pch.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CWriteObjectHook
{
public:
    virtual void WriteObject(CObjectOStream& out,
                             const CConstObjectInfo& passed_info)
    {
#if 1
        DefaultWrite(out, passed_info);

#else
#if 1
// or write it directly
        CDate_std s;
        s.SetYear(2001);
        s.SetMonth(1);
        CObjectInfo oi(&s, passed_info.GetTypeInfo());
        DefaultWrite(out, oi);
#endif
#if 0
// or like this
        CDate_std s;
        s.SetYear(2001);
        s.SetMonth(1);
        passed_info.GetTypeInfo()->DefaultWriteData(out, &s);
        // we may not use the following because it will call this same hook again recursively
        //out.WriteObject(&s, passed_info.GetTypeInfo());
#endif
#endif

    }
};

int main(int argc, char** argv)
{
    unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
    CDate date;

    CObjectTypeInfo(CType<CDate_std>())
        .SetLocalWriteHook(*out, new CDemoHook);
    date.SetStd().SetYear(1999);
// when NOT using DefaultWrite in the hook, there is no need to assign actual data
// the following is enough
//    date.SetStd();

    *out << date;
    return 0;
}
