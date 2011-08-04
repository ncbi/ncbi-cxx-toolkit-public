#include <ncbi_pch.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CWriteChoiceVariantHook
{
public:
    virtual void WriteChoiceVariant(CObjectOStream& out,
                                    const CConstObjectInfoCV& passed_info)
    {
        DefaultWrite(out, passed_info);
    }
};

int main(int argc, char** argv)
{
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "if"));
    auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "of"));

    CObjectTypeInfo typeInfo = CType<CAuth_list>();
    CObjectTypeInfoMI memberInfo = typeInfo.FindMember("names");
    CObjectTypeInfoVI variantInfo = (*memberInfo).GetPointedType()
                                                 .FindVariant("std");
    variantInfo.SetLocalWriteHook(*out, new CDemoHook);

    (*CObjectTypeInfo(CType<CAuth_list>()).FindMember("names"))
        .GetPointedType()
        .FindVariant("std")
        .SetLocalWriteHook(*out, new CDemoHook);

    CCit_art article;
    *in >> article;
    *out << article;

    return 0;
}
