#include <ncbi_pch.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CWriteClassMemberHook
{
public:
    virtual void WriteClassMember(CObjectOStream& out,
                                  const CConstObjectInfoMI& passed_info)
    {
#if 1
        DefaultWrite(out, passed_info);

#else
// get information about the member
        // typeinfo of the parent class (Date-std)
        CObjectTypeInfo oti = passed_info.GetClassType();
        // typeinfo and data of the parent class
        const CConstObjectInfo& oi = passed_info.GetClassObject();
        // typeinfo of the member (Int4)
        CObjectTypeInfo omti = passed_info.GetMemberType();
        // typeinfo and data of the member
        CConstObjectInfo om = passed_info.GetMember();
        // index of the member in parent class (1)
        TMemberIndex mi = passed_info.GetMemberIndex();
        // information about the member, including its name (year)
        const CMemberInfo* minfo = passed_info.GetMemberInfo();
        // ePrimitiveValueInteger
        EPrimitiveValueType t = omti.GetPrimitiveValueType();
        // 4
        size_t sz = omti.GetTypeInfo()->GetSize();
        // true
        bool s = omti.IsPrimitiveValueSigned();

// call DefaultWrite (above) or write directly
#if 1
        Int4 y = 2001;
        out.WriteClassMember( minfo->GetId(), minfo->GetTypeInfo(), &y);
#endif
#if 0
        // create class object
        CObjectInfo coi(passed_info.GetClassType());
        // set its member
        coi.FindClassMember("year").GetMember().SetPrimitiveValueInt4(2001);
        // create member iterator
        CConstObjectInfoMI member(coi, passed_info.GetMemberIndex());
        // write this member only
        DefaultWrite(out, member);
#endif
#endif
    }
};

int main(int argc, char** argv)
{
    unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
    CObjectTypeInfo(CType<CDate_std>())
        .FindMember("year")
        .SetLocalWriteHook(*out, new CDemoHook);

    CDate date;
    date.SetStd().SetYear(1999);
    *out << date;

    return 0;
}
