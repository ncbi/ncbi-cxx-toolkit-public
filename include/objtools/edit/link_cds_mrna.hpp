#ifndef TABLE2ASN_VALIDATOR_HPP_INCLUDED
#define TABLE2ASN_VALIDATOR_HPP_INCLUDED

BEGIN_NCBI_SCOPE

namespace objects
{
    class CSeq_entry;
    class CBioseq;
    class CBioseq_set;
};

class CCDStomRNALinkBuilder
{
public:
    void Operate(objects::CSeq_entry& entry);
protected:
    void LinkCDSmRNAbyLabelAndLocation(objects::CBioseq_set& bioseq);
    void LinkCDSmRNAbyLabelAndLocation(objects::CBioseq& bioseq);
    void LinkCDSmRNAbyLabelAndLocation(objects::CSeq_entry& bioseq);
};

END_NCBI_SCOPE

#endif
