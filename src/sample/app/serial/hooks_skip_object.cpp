#include <ncbi_pch.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/objcopy.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CSkipObjectHook
{
public:
    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& passed_info)
    {
        cout << in.GetStackPath() << endl;
#if 1
        DefaultSkip(in, passed_info);

#else
#if 1
// or read
        CObjectInfo oi(passed_info);
        DefaultRead(in, oi);
        cout << MSerial_AsnText << oi << endl;
#endif
#if 0
// or copy it into stdout
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        CObjectStreamCopier copier(in, *out);
        copier.CopyObject(passed_info.GetTypeInfo());
#endif
#if 0
// or read it into delay buffer
        in.StartDelayBuffer();
        DefaultSkip(in, passed_info);
        CRef<CByteSource> data = in.EndDelayBuffer();
        // now all data is in CByteSource and unparsed
        // we can parse the buffer:
        unique_ptr<CObjectIStream> ib(CObjectIStream::Create(eSerial_AsnText, *data));
#if 1
// like this
        CSeq_annot annot;
        ib->ReadObject(&annot, CSeq_annot::GetTypeInfo());
        // now the object is valid
        cout << MSerial_AsnText << annot;
#else
// or like this
        CObjectInfo oi(passed_info);
        ib->ReadObject(oi);
        // now the object is valid
        cout << MSerial_AsnText << oi;
#endif
#endif
#endif

    }
};

int main(int argc, char** argv)
{
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "seq-entry-sample.asn"));

    CObjectTypeInfo(CType<CSeq_annot>()).SetLocalSkipHook(*in, new CDemoHook);

    in->Skip(CType<CSeq_entry>());

    return 0;
}
