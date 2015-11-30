#ifndef TABLE2ASN_VALIDATOR_HPP_INCLUDED
#define TABLE2ASN_VALIDATOR_HPP_INCLUDED

BEGIN_NCBI_SCOPE

namespace objects
{
    class CSeq_entry;
    class CBioseq;
    class CSeq_entry_Handle;
    class CValidError;
};

class CTable2AsnValidator
{
public:
    CConstRef<objects::CValidError> Validate(const CSerialObject& object, Uint4 opts);
    void Cleanup(objects::CSeq_entry& entry, const string& flags);
    void ReportErrors(CConstRef<objects::CValidError> errors, CNcbiOstream& out);
};

END_NCBI_SCOPE

#endif