#include <ncbi_pch.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CReadObjectHook
{
public:
    virtual void ReadObject(CObjectIStream& strm,
                            const CObjectInfo& passed_info)
    {
        cout << strm.GetStackPath() << endl;
#if 1
        DefaultRead(strm, passed_info);

#else
        // typeinfo of the object (Seq-annot)
        TTypeInfo ti = passed_info.GetTypeInfo();

#if 1
// or skip the object
        DefaultSkip(strm, passed_info);
#endif
#if 0
// or copy it into stdout
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        CObjectStreamCopier copier(strm, *out);
        copier.CopyObject(passed_info.GetTypeInfo());
#endif
#if 0
// or read it into delay buffer
        strm.StartDelayBuffer();
        DefaultSkip(strm, passed_info);
        CRef<CByteSource> data = strm.EndDelayBuffer();
        
        // now all data is in CByteSource and unparsed, so the following will fail:
        //cout << MSerial_AsnText << passed_info;
        // FYI: insertion operator calls
        // WriteObject(cout,passed_info.GetObjectPtr(),passed_info.GetTypeInfo());
        
        // we can parse the buffer:
        unique_ptr<CObjectIStream> in(CObjectIStream::Create(eSerial_AsnText, *data));
#if 0
// like this
        CSeq_annot annot;
        in->ReadObject(&annot, CSeq_annot::GetTypeInfo());
        // now the object is valid
        cout << MSerial_AsnText << annot;
#else
// or like this
        in->ReadObject(passed_info);
        // now the object is valid
        cout << MSerial_AsnText << passed_info;
#endif
#endif
#endif
    }
};

int main(int argc, char** argv)
{
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "seq-entry-sample.asn"));

    CObjectTypeInfo(CType<CSeq_annot>()).SetLocalReadHook(*in, new CDemoHook());

    CSeq_entry entry;
    *in >> entry;

    return 0;
}
