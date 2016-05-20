#include <ncbi_pch.hpp>
#include <objects/general/Date_std.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CReadClassMemberHook
{
public:
    virtual void ReadClassMember(CObjectIStream& strm,
                                 const CObjectInfoMI& passed_info)
    {
#if 1
        DefaultRead(strm, passed_info);

#else
#if 1
// call DefaultRead to read member data, or DefaultSkip to skip it
        DefaultSkip(strm, passed_info);
#endif
#if 0
// read the object into local buffer
// this data will be discarded when this function terminates
// so the class member of the object being read will be invalid
        CObjectInfo obj(passed_info.GetMemberType());
        strm.ReadObject(obj);
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        out->WriteObject(obj);
#endif
#if 0
// or copy it into stdout
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        CObjectStreamCopier copier(strm, *out);
        copier.CopyObject(passed_info.GetMemberType().GetTypeInfo());
#endif

// get information about the member
        // typeinfo of the parent class (Date-std)
        CObjectTypeInfo oti = passed_info.GetClassType();
        // typeinfo and data of the parent class
        const CObjectInfo& oi = passed_info.GetClassObject();
        // typeinfo of the member (Int4)
        CObjectTypeInfo omti = passed_info.GetMemberType();
        // typeinfo and data of the member
        CObjectInfo om = passed_info.GetMember();
        // index of the member in parent class
        TMemberIndex mi = passed_info.GetMemberIndex();
        // information about the member, including its name (year)
        const CMemberInfo* minfo = passed_info.GetMemberInfo();
#endif
    }
};

int main(int argc, char** argv)
{
    char asn[] = "Date-std ::= { year 1998, month 1, day 2, season \"winter\" }";
    CNcbiIstrstream iss(asn);
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, iss));

    CObjectTypeInfo(CType<CDate_std>()).FindMember("year")
                                       .SetLocalReadHook(*in, new CDemoHook);

    CDate_std my_date;
    *in >> my_date;

    return 0;
}
