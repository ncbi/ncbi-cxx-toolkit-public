#ifndef TABLE2ASN_VALIDATOR_HPP_INCLUDED
#define TABLE2ASN_VALIDATOR_HPP_INCLUDED

BEGIN_NCBI_SCOPE

namespace objects
{
    class CSeq_entry;
};

class CTable2AsnValidator
{
public:
    void Validate(const objects::CSeq_entry& entry);
};

END_NCBI_SCOPE

#endif