#include <ncbi_pch.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CCopyClassMemberHook
{
public:
    virtual void CopyClassMember(CObjectStreamCopier& copier,
                                 const CObjectTypeInfoMI& passed_info)
    {
        DefaultCopy(copier, passed_info);
    }
};

int main(int argc, char** argv)
{
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "if"));
    auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "of"));
    CObjectStreamCopier copier(*in, *out);

    CObjectTypeInfo(CType<CBioseq>())
        .FindMember("annot")
        .SetLocalCopyHook(copier, new CDemoHook());

    copier.Copy(CType<CBioseq>());

    return 0;
}
