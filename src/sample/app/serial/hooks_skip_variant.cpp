#include <ncbi_pch.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

class CDemoHook : public CSkipChoiceVariantHook
{
public:
    virtual void SkipChoiceVariant(CObjectIStream& in,
                                   const CObjectTypeInfoCV& passed_info)
    {
        cout << in.GetStackPath() << endl;
        in.SkipObject(*passed_info);

#if 0
// get information about the variant
        // typeinfo of the parent class (Seq-annot.data)
        CObjectTypeInfo oti = passed_info.GetChoiceType();
        // typeinfo of the variant  (SET OF Seq-feat)
        CObjectTypeInfo omti = passed_info.GetVariantType();
        // index of the variant in parent class  (1)
        TMemberIndex mi = passed_info.GetVariantIndex();
        // information about the variant, including its name (ftable)
        const CVariantInfo* minfo = passed_info.GetVariantInfo();

#if 0
// or read CSeq_feat objects one by one
        for ( CIStreamContainerIterator i(in, passed_info.GetVariantType()); i; ++i ) {
            CSeq_feat feat;
            i >> feat;
            cout << MSerial_AsnText << feat << endl;
        }
#endif
#if 0
// or read CSeq_feat objects one by one and write them into stdout
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        COStreamContainer o(*out, passed_info.GetVariantType());
        for ( CIStreamContainerIterator i(in, passed_info.GetVariantType()); i; ++i ) {
            CSeq_feat feat;
            i >> feat;
// NOTE: this does not produce well formed text ASN, because of missing typeinfo name
// this would work though if we copied data into existing ASN stream
// where typeinfo name ("file header") is not required
//            o << feat;
// if we needed well formed text ASN, we could write it like this:
            cout << MSerial_AsnText << feat;
        }
#endif
#if 0
// or read the whole SET OF Seq-feat at once
        CSeq_annot::TData::TFtable ft;
        CObjectInfo oi(&ft, passed_info.GetVariantType().GetTypeInfo());
        // or, like this:
        //  CObjectInfo oi(passed_info.GetVariantType());
        in.ReadObject(oi);
        // write them one by one
        for( const auto& e: ft) {
            cout << MSerial_AsnText << *e << endl;
        }
        // or write them all at once
        // NOTE: while this works, it does not produce well formed text ASN,
        // because CSeq_annot::TData::TFtable typeinfo has no name
        unique_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, "stdout", eSerial_StdWhenStd));
        out->Write(oi);
#endif
#endif
    }
};

int main(int argc, char** argv)
{
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, "seq-entry-sample.asn"));

    (*CObjectTypeInfo(CType<CSeq_annot>()).FindMember("data"))
        .GetPointedType()
        .FindVariant("ftable")
        .SetLocalSkipHook(*in, new CDemoHook());
    in->Skip(CType<CSeq_entry>());

    return 0;
}
