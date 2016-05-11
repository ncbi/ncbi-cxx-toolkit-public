#ifndef TABLE2ASN_VALIDATOR_HPP_INCLUDED
#define TABLE2ASN_VALIDATOR_HPP_INCLUDED

BEGIN_NCBI_SCOPE

namespace objects
{
    class CSeq_entry;
    class CBioseq;
    class CSeq_entry_Handle;
    class CValidError;
    class CSeq_submit;
};

class CTable2AsnValidator
{
public:
    void Validate(CRef<objects::CSeq_submit> submit, CRef<objects::CSeq_entry> entry, const string& flags, const string& report_name);
    void Cleanup(objects::CSeq_entry_Handle entry, const string& flags);
    void UpdateECNumbers(objects::CSeq_entry_Handle seh, const string& fname, auto_ptr<CNcbiOfstream>& ostream);
    void ReportErrors(CConstRef<objects::CValidError> errors, CNcbiOstream& out);
};

END_NCBI_SCOPE

#endif
