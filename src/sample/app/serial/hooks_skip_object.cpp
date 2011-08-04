#include <ncbi_pch.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <serial/objistr.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CSkipObjectHook
{
public:
    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& passed_info)
    {
    }
};

int main(int argc, char** argv)
{
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "if"));

    CObjectTypeInfo(CType<CCit_art>()).SetLocalSkipHook(*in, new CDemoHook);

    in->Skip(CType<CCit_art>());

    return 0;
}
