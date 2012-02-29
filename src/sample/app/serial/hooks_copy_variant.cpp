#include <ncbi_pch.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CCopyChoiceVariantHook
{
public:
    virtual void CopyChoiceVariant(CObjectStreamCopier& copier,
                                   const CObjectTypeInfoCV& passed_info)
    {
        DefaultCopy(copier, passed_info);
    }
};

int main(int argc, char** argv)
{
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "if"));
    auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "of"));
    CObjectStreamCopier copier(*in, *out);

    (*CObjectTypeInfo(CType<CAuth_list>()).FindMember("names"))
        .GetPointedType()
        .FindVariant("std")
        .SetLocalCopyHook(copier, new CDemoHook);

    copier.Copy(CType<CCit_art>());

    return 0;
}
