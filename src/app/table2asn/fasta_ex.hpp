#ifndef TABLE2ASN_FASTA_EX_HPP
#define TABLE2ASN_FASTA_EX_HPP

#include <objtools/readers/fasta.hpp>
#include <objtools/edit/gaps_edit.hpp>
#include "huge_file.hpp"

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class ILineErrorListener;
class CIdMapper;
class CBioseq;
class CSeq_annot;
class CSourceModParser;
}

class CTable2AsnContext;
class CSerialObject;
class CAnnotationLoader;

class CFastaReaderEx : public objects::CFastaReader
{
public:
    CFastaReaderEx(CTable2AsnContext& context, std::istream& instream, TFlags flags);
    void AssignMolType(objects::ILineErrorListener * pMessageListener) override;
    void AssembleSeq(objects::ILineErrorListener * pMessageListener) override;
    virtual CRef<objects::CSeq_entry> ReadDeltaFasta(objects::ILineErrorListener * pMessageListener);
protected:
    CTable2AsnContext& m_context;
    objects::CGapsEditor m_gap_editor;
};

class CHugeFastaReader: public IHugeAsnSource
{
public:
    CHugeFastaReader(CTable2AsnContext& context);
    ~CHugeFastaReader();
    void Open(CHugeFile* file, objects::ILineErrorListener * pMessageListener) override;
    bool GetNextBlob() override;
    CRef<objects::CSeq_entry> GetNextSeqEntry() override;
    bool IsMultiSequence() override { return m_is_multi; }
    CConstRef<objects::CSeq_submit> GetSubmit() override { return {}; };
protected:
    bool m_is_multi = false;
    CRef<objects::CSeq_entry> xLoadNextSeq();
    CTable2AsnContext& m_context;
    unique_ptr<CFastaReaderEx> m_fasta;
    unique_ptr<std::istream> m_instream;
    deque<CRef<objects::CSeq_entry>> m_seqs;
};

END_NCBI_SCOPE

#endif
