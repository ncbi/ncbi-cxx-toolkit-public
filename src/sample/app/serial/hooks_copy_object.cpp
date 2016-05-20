#include <ncbi_pch.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CCopyObjectHook
{
public:
    virtual void CopyObject(CObjectStreamCopier& copier,
                            const CObjectTypeInfo& passed_info)
    {
        cout << copier.In().GetStackPath() << endl;
#if 1
        DefaultCopy(copier, passed_info);

#else
#if 1
// or skip the object
        copier.In().SkipObject(passed_info.GetTypeInfo());
#endif
#if 0
// or read object
        CSeq_annot annot;
        copier.In().ReadObject(&annot, CSeq_annot::GetTypeInfo());
        cout << MSerial_AsnText << annot << endl;
// and maybe write it as well
        copier.Out().WriteObject(&annot, CSeq_annot::GetTypeInfo());
#endif
#if 0
// or read object and write it
        CObjectInfo oi(passed_info.GetTypeInfo());
        copier.In().ReadObject(oi);
        copier.Out().WriteObject(oi);
#endif
        
        // typeinfo of the object (Seq-annot)
        TTypeInfo ti = passed_info.GetTypeInfo();
#endif
    }
};

int main(int argc, char** argv)
{
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "seq-entry-sample.asn"));
    unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "seq-entry-sample_output.asn"));
    CObjectStreamCopier copier(*in, *out);

    CObjectTypeInfo(CType<CSeq_annot>())
        .SetLocalCopyHook(copier, new CDemoHook());

    copier.Copy(CType<CSeq_entry>());

    return 0;
}
