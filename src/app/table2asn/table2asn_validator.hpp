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
    class CScope;
}
namespace NDiscrepancy
{
    class CDiscrepancySet;
}

class CTable2AsnContext;
class CTable2AsnValidator: public CObject
{
public:
    CTable2AsnValidator(CTable2AsnContext& ctx);
    void Validate(CRef<objects::CSeq_submit> submit, CRef<objects::CSeq_entry> entry, const string& flags);
    void Cleanup(CRef<objects::CSeq_submit> submit, objects::CSeq_entry_Handle& entry, const string& flags);
    void UpdateECNumbers(objects::CSeq_entry& entry);
    void ReportErrors(CConstRef<objects::CValidError> errors, CNcbiOstream& out);
    void ReportErrorStats(CNcbiOstream& out);
    size_t TotalErrors() const; 

    void CollectDiscrepancies(CSerialObject& obj, bool eucariote, const string& lineage);
    void ReportDiscrepancies();

protected:
    typedef map<int, size_t> TErrorStatMap;
    class TErrorStats
    {
    public:
        TErrorStats() : m_total(0) {};
        TErrorStatMap m_individual;
        size_t m_total;
    };
    vector<TErrorStats> m_stats;
    CTable2AsnContext* m_context;
    CRef<NDiscrepancy::CDiscrepancySet> m_discrepancy;
};

END_NCBI_SCOPE

#endif
