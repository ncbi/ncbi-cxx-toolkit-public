#ifndef TABLE2ASN_VALIDATOR_HPP_INCLUDED
#define TABLE2ASN_VALIDATOR_HPP_INCLUDED

BEGIN_NCBI_SCOPE

namespace objects
{
    class CSeq_entry;
    class CBioseq;
};

class CTable2AsnValidator
{
public:
    void Validate(const objects::CSeq_entry& entry);
    void Cleanup(objects::CSeq_entry& entry);
    void LinkCDSmRNAbyLabelAndLocation(objects::CBioseq_set& bioseq);
    void LinkCDSmRNAbyLabelAndLocation(objects::CBioseq& bioseq);
    void LinkCDSmRNAbyLabelAndLocation(objects::CSeq_entry& bioseq);
    void LinkCDSmRNAbyLabelAndLocation(objects::CBioseq::TAnnot& annot);
};

END_NCBI_SCOPE

#endif