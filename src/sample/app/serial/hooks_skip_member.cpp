#include <ncbi_pch.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CSkipClassMemberHook
{
public:
    virtual void SkipClassMember(CObjectIStream& in,
                                 const CObjectTypeInfoMI& passed_info)
    {
        cout << in.GetStackPath() << endl;
        in.SkipObject(*passed_info);

#if 0
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
#if 0
// or read CSeq_annot objects one by one and write them into stdout
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        COStreamContainer o(*out, passed_info);
        for ( CIStreamContainerIterator i(in, passed_info); i; ++i ) {
            CSeq_annot annot;
            i >> annot;
// NOTE: this does not produce well formed text ASN, because of missing typeinfo name
// this would work though if we copied data into existing ASN stream
// where typeinfo name ("file header") is not required
            o << annot;
// if we needed well formed text ASN, we could write it like this:
//            cout << MSerial_AsnText << annot;
        }
#endif
#if 0
// or read the whole SET OF Seq-annot at once
        CBioseq::TAnnot annot;
        CObjectInfo oi(&annot, passed_info.GetMemberType().GetTypeInfo());
        in.ReadObject(oi);
        // write them one by one
        for( const auto& e: annot) {
            cout << MSerial_AsnText << *e << endl;
        }
        // or write them all at once
        // NOTE: while this works, it does not produce well formed text ASN,
        // because CBioseq::TAnnot typeinfo has no name
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        out->Write(oi);
#endif
    }
};

int main(int argc, char** argv)
{
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "seq-entry-sample.asn"));
    CObjectTypeInfo(CType<CBioseq>())
        .FindMember("annot")
        .SetLocalSkipHook(*in, new CDemoHook());

    in->Skip(CType<CSeq_entry>());

    return 0;
}
