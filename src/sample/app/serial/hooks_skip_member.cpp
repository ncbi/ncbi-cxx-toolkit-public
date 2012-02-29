#include <ncbi_pch.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <serial/objistr.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CSkipClassMemberHook
{
public:
    virtual void SkipClassMember(CObjectIStream& in,
                                 const CObjectTypeInfoMI& passed_info)
    {
        in.SkipObject(*passed_info);
    }
};

int main(int argc, char** argv)
{
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "if"));

    CObjectTypeInfo(CType<CAuth_list>())
        .FindMember("names")
        .SetLocalSkipHook(*in, new CDemoHook);

    in->Skip(CType<CCit_art>());

    return 0;
}
