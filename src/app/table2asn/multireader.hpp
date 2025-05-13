#ifndef TABLE2ASN_MULTIREADER_HPP
#define TABLE2ASN_MULTIREADER_HPP

#include <util/format_guess.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <shared_mutex>

BEGIN_NCBI_SCOPE

namespace objects
{
class CScope;
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class ILineErrorListener;
class CIdMapper;
class CBioseq;
class CSeq_annot;
class CGff3Reader;
class CGff3LocationMerger;
namespace edit
{
    class CHugeFile;
};

}

class CTable2AsnContext;
class CSerialObject;
union CFileContentInfo;
struct CFileContentInfoGenbank;
class IIndexedFeatureReader;

USING_SCOPE(objects);
class CGff3LocationMerger;

//  ============================================================================
class CMultiReader
//  ============================================================================
{
public:
    CMultiReader(CTable2AsnContext& context);
    ~CMultiReader();

    using TAnnots = list<CRef<objects::CSeq_annot>>;

    static const set<TTypeInfo> kSupportedTypes;

    void LoadTemplate(const string& ifname);
    void LoadGFF3Fasta(istream& in, TAnnots& annots);

    void AddAnnots(IIndexedFeatureReader* reader, CBioseq& bioseq) const;

    std::unique_ptr<IIndexedFeatureReader> LoadIndexedAnnot(const string& filename);
    void GetIndexedAnnot(std::unique_ptr<IIndexedFeatureReader>& reader, TAnnots& annots);

    void LoadDescriptors(const string& ifname, CRef<objects::CSeq_descr> & out_desc) const;
    void ApplyDescriptors(objects::CSeq_entry & obj, const objects::CSeq_descr & source) const;
    void WriteObject(const CSerialObject&, ostream&);
    CRef<objects::CSeq_entry> ReadAlignment(CNcbiIstream& instream, const CArgs& args);
    CRef<CSerialObject> ReadNextEntry();
    CFormatGuess::EFormat OpenFile(const string& filename, CRef<CSerialObject>& input_sequence, TAnnots& annots);
    CRef<CSerialObject> FetchEntry(const CFormatGuess::EFormat& format, const string& objectType,
            unique_ptr<CNcbiIstream>& pIstr, TAnnots& annots);
    static
    void GetSeqEntry(CRef<objects::CSeq_entry>& entry, CRef<objects::CSeq_submit>& submit, CRef<CSerialObject> obj);
protected:
private:
    void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeq_descr & source) const;
    void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeqdesc & source) const;

    bool AtSeqenceData() const { return mAtSequenceData; };

    CRef<objects::CSeq_entry> xReadFasta(CNcbiIstream& instream);
    CRef<CSerialObject> xApplyTemplate(CRef<CSerialObject> obj, bool merge_template_descriptors) const;
    CRef<CSerialObject> xReadASN1Text(CObjectIStream& pObjIstrm) const;
    CRef<CSerialObject> xReadASN1Binary(CObjectIStream& pObjIstrm, const string& content_type) const;
    TAnnots xReadGFF3(CNcbiIstream& instream, bool post_process);
    TAnnots xReadGTF(CNcbiIstream& instream) const;
    CRef<objects::CSeq_entry> xReadFlatfile(CFormatGuess::EFormat format, const string& filename, CNcbiIstream& instream);
    void x_PostProcessAnnots(TAnnots& annots, CFormatGuess::EFormat format=CFormatGuess::eUnknown) const;

    unique_ptr<CObjectIStream> xCreateASNStream(const string& filename) const;
    unique_ptr<CObjectIStream> xCreateASNStream(CFormatGuess::EFormat format, unique_ptr<istream>& instream) const;
    CFormatGuess::EFormat xInputGetFormat(CNcbiIstream&, CFileContentInfo* = nullptr) const;
    void xAnnotGetFormat(objects::edit::CHugeFile& file) const;

    int  m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;
    CTable2AsnContext& m_context;
    unique_ptr<CObjectIStream> m_obj_stream;
    shared_ptr<objects::CGff3LocationMerger> m_gff3_merger;
    bool mAtSequenceData;
    mutable shared_mutex m_Mutex;
};

void g_ModifySeqIds(CSeq_annot& annot, const CSeq_id& match, CRef<CSeq_id> new_id);

END_NCBI_SCOPE

#endif
