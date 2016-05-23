#include <ncbi_pch.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook_std : public CWriteChoiceVariantHook
{
public:
    virtual void WriteChoiceVariant(CObjectOStream& out,
                                    const CConstObjectInfoCV& passed_info)
    {
#if 1
        DefaultWrite(out, passed_info);
#else
// get information about the member
        // typeinfo of the parent class (Date)
        CObjectTypeInfo oti = passed_info.GetChoiceType();
        // typeinfo and data of the parent class
        const CConstObjectInfo& oi = passed_info.GetChoiceObject();
        // typeinfo of the variant (Date-std)
        CObjectTypeInfo omti = passed_info.GetVariantType();
        // typeinfo and data of the variant
        CConstObjectInfo om = passed_info.GetVariant();
        // index of the variant in parent class (2)
        TMemberIndex mi = passed_info.GetVariantIndex();
        // information about the member, including its name (std)
        const CVariantInfo* minfo = passed_info.GetVariantInfo();

#if 1
// call DefaultWrite (above) or write directly
        CDate_std s;
        s.SetYear(2001);
        CConstObjectInfo coi(&s, passed_info.GetVariantType().GetTypeInfo());
        CustomWrite(out, passed_info, coi);
#endif
#endif
    }
};

class CDemoHook_str : public CWriteChoiceVariantHook
{
public:
    virtual void WriteChoiceVariant(CObjectOStream& out,
                                    const CConstObjectInfoCV& passed_info)
    {
        DefaultWrite(out, passed_info);

#if 0
// get information about the member
        // typeinfo of the parent class (Date)
        CObjectTypeInfo oti = passed_info.GetChoiceType();
        // typeinfo and data of the parent class
        const CConstObjectInfo& oi = passed_info.GetChoiceObject();
        // typeinfo of the variant (str)
        CObjectTypeInfo omti = passed_info.GetVariantType();
        // typeinfo and data of the variant
        CConstObjectInfo om = passed_info.GetVariant();
        // index of the variant in parent class (1)
        TMemberIndex mi = passed_info.GetVariantIndex();
        // information about the member, including its name (str)
        const CVariantInfo* minfo = passed_info.GetVariantInfo();
        // ePrimitiveValueString
        EPrimitiveValueType t = omti.GetPrimitiveValueType();

// call DefaultWrite (above) or write directly
        string s("2001-01-02");
        out.WriteObject(&s, minfo->GetTypeInfo());
#endif
    }
};

int main(int argc, char** argv)
{
    unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));

    {
        CDate date;
        CObjectTypeInfo(CType<CDate>())
            .FindVariant("std")
            .SetLocalWriteHook(*out, new CDemoHook_std);
        date.SetStd().SetYear(1999);
        *out << date;
    }
    {
        CDate date;
        CObjectTypeInfo(CType<CDate>())
            .FindVariant("str")
            .SetLocalWriteHook(*out, new CDemoHook_str);
        date.SetStr("date");
// when NOT using DefaultWrite in the hook, there is no need to assign actual data
// the following is enough
//      date.SetStr();
        *out << date;
    }

    return 0;
}
