#ifndef TABLE2ASN_VALIDATOR_HPP_INCLUDED
#define TABLE2ASN_VALIDATOR_HPP_INCLUDED

#include <objtools/validator/validator_context.hpp>
#include <objtools/validator/huge_file_validator.hpp>

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
    ~CTable2AsnValidator();

    void Clear();
    void ValCollect(CRef<objects::CSeq_submit> submit, CRef<objects::CSeq_entry> entry, const string& flags);
    void ValReportErrorStats(CNcbiOstream& out);
    void ValReportErrors();
    size_t ValTotalErrors() const;

    void Cleanup(CRef<objects::CSeq_submit> submit, objects::CSeq_entry_Handle& entry, const string& flags) const;
    void UpdateECNumbers(objects::CSeq_entry& entry);

    void CollectDiscrepancies(const CSerialObject& obj, bool eukaryote, const string& lineage);
    void ReportDiscrepancy(const CSerialObject& obj, bool eukaryote, const string& lineage);
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
    std::shared_ptr<objects::validator::SValidatorContext> m_validator_ctx;
    std::list<CRef<objects::CValidError>> m_val_errors;
    objects::validator::CHugeFileValidator::TGlobalInfo m_val_globalInfo;
    mutable std::mutex m_mutex;
};

END_NCBI_SCOPE

#endif
