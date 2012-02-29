#include <ncbi_pch.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/general/Date.hpp>
#include <serial/objistr.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CSkipChoiceVariantHook
{
public:
    virtual void SkipChoiceVariant(CObjectIStream& in,
                                   const CObjectTypeInfoCV& passed_info)
    {
        in.SkipObject(*passed_info);
    }
};

int main(int argc, char** argv)
{
    char asn[] = "Imprint ::= { date std { year 2010 } }";
    CNcbiIstrstream iss(asn);
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, iss));

    CObjectTypeInfo(CType<CDate>()).FindVariant("std")
                                   .SetLocalSkipHook(*in, new CDemoHook());

    in->Skip(CType<CImprint>());

    return 0;
}
