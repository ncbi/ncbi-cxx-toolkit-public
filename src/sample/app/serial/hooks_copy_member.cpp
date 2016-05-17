#include <ncbi_pch.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
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

#if 0
#if 0
// or skip member in input stream:
        copier.In().Skip(passed_info.GetMemberType().GetTypeInfo(), CObjectIStream::eNoFileHeader);
#endif

#if 0
// or read CSeq_annot objects one by one
        for ( CIStreamContainerIterator i(copier.In(), passed_info); i; ++i ) {
            CSeq_annot annot;
            i >> annot;
            cout << MSerial_AsnText << annot << endl;
        }
#endif
#if 0
// or read CSeq_annot objects one by one
        for ( CIStreamContainerIterator i(copier.In(), passed_info); i; ++i ) {
            CObjectInfo oi(CSeq_annot::GetTypeInfo());
            i.ReadElement(oi);
            cout << MSerial_AsnText << oi << endl;
        }
#endif
#if 0
// or read CSeq_annot objects one by one and write them into output stream
        COStreamContainer o(copier.Out(), passed_info);
        for ( CIStreamContainerIterator i(copier.In(), passed_info); i; ++i ) {
            CSeq_annot annot;
            i >> annot;
            o << annot;
            cout << MSerial_AsnText << annot << endl;
        }
#endif

// get information about the member
        // typeinfo of the parent class (Bioseq)
        CObjectTypeInfo oti = passed_info.GetClassType();
        // typeinfo of the member  (SET OF Seq-annot)
        CObjectTypeInfo omti = passed_info.GetMemberType();
        // index of the member in parent class  (4)
        TMemberIndex mi = passed_info.GetMemberIndex();
        // information about the member, including its name (annot)
        const CMemberInfo* minfo = passed_info.GetMemberInfo();
#endif
    }
};

int main(int argc, char** argv)
{
// read Seq-entry data
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "seq-entry-sample.asn"));
    unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "seq-entry-sample_output.asn"));
    CObjectStreamCopier copier(*in, *out);

    CObjectTypeInfo(CType<CBioseq>())
        .FindMember("annot")
        .SetLocalCopyHook(copier, new CDemoHook());

    copier.Copy(CType<CSeq_entry>());

    return 0;
}
