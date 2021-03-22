#ifndef TABLE2ASN_FASTA_EX_HPP
#define TABLE2ASN_FASTA_EX_HPP

#include <objtools/readers/fasta.hpp>
#include <objtools/edit/gaps_edit.hpp>

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

class CFastaReaderEx : public objects::CFastaReader {
public:
    CFastaReaderEx(CTable2AsnContext& context, ILineReader& reader, TFlags flags);
    virtual void AssignMolType(objects::ILineErrorListener * pMessageListener) override;
    virtual void AssembleSeq(objects::ILineErrorListener * pMessageListener) override;
    virtual CRef<objects::CSeq_entry> ReadDeltaFasta(objects::ILineErrorListener * pMessageListener);
protected:
    CTable2AsnContext& m_context;
    objects::CGapsEditor m_gap_editor;
};


END_NCBI_SCOPE

#endif
