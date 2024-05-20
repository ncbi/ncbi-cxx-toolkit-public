#ifndef TABLE2ASN_VALIDATOR_HPP_INCLUDED
#define TABLE2ASN_VALIDATOR_HPP_INCLUDED

#include <objects/valerr/ValidErrItem.hpp>
#include <objtools/validator/validator_context.hpp>
#include <objtools/validator/huge_file_validator.hpp>
#include <objects/valerr/ValidError.hpp>

#include "utils.hpp"
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>

BEGIN_NCBI_SCOPE

namespace objects
{
    class CSeq_entry;
    class CBioseq;
    class CSeq_entry_Handle;
    class CSeq_submit;
    class CScope;
}
namespace NDiscrepancy
{
    class CDiscrepancySet;
    class CDiscrepancyProduct;
}

struct SSevStats { // This class holds error counts for a single severity level
    map<int, size_t> individual;
    size_t           total{ 0 };
};

using TValStats = array<SSevStats, objects::CValidErrItem::eSev_trace>;


class CValidMessageHandler : public objects::IValidError
{
public:
    enum class EMode {
        Asynchronous,
        Serial
    };

    CValidMessageHandler(CNcbiOstream& ostr, EMode mode = EMode::Asynchronous) :
        m_Ostr(ostr),
        m_Mode(mode) {}
    virtual ~CValidMessageHandler() = default;

    void AddValidErrItem(
        EDiagSev             sev,                    // severity
        unsigned int         ec,                     // error code
        const string&        msg,                    // specific error message
        const string&        desc,                   // offending object's description
        const CSerialObject& obj,                    // offending object
        const string&        acc,                    // accession of object.
        const int            ver,                    // version of object.
        const string&        location   = kEmptyStr, // formatted location of object
        const int            seq_offset = 0) override;

    void AddValidErrItem(
        EDiagSev      sev,                    // severity
        unsigned int  ec,                     // error code
        const string& msg,                    // specific error message
        const string& desc,                   // offending object's description
        const string& acc,                    // accession of object.
        const int     ver,                    // version of object.
        const string& location   = kEmptyStr, // formatted location of object
        const int     seq_offset = 0) override;

    void AddValidErrItem(
        EDiagSev                   sev,     // severity
        unsigned int               ec,      // error code
        const string&              msg,     // specific error message
        const string&              desc,    // offending object's description
        const objects::CSeqdesc&   seqdesc, // offending object
        const objects::CSeq_entry& ctx,     // place of packaging
        const string&              acc,     // accession of object or context.
        const int                  ver,     // version of object.
        const int                  seq_offset = 0) override;

    void AddValidErrItem(
        EDiagSev      sev, // severity
        unsigned int  ec,  // error code
        const string& msg) override;

    void AddValidErrItem(CRef<objects::CValidErrItem> pItem) override;

    void Write();
    void RequestStop();

    using TPostponed = list<CRef<objects::CValidErrItem>>;
    const TPostponed& GetPostponed() const;
    const TValStats&  GetStats() { return m_ProcessedStats; }

private:
    void x_LogStats(const objects::CValidErrItem& item);
    void x_AddItemAsync(CRef<objects::CValidErrItem> pItem);

    CNcbiOstream& m_Ostr;
    EMode         m_Mode;
    TValStats     m_ProcessedStats;

    condition_variable                  m_Cv;
    mutex                               m_Mutex;
    queue<CRef<objects::CValidErrItem>> m_Items;
    mutex                               m_PostponedMutex;
    TPostponed                          m_PostponedItems;
};


class CTable2AsnContext;
class CTable2AsnValidator : public CObject
{
public:
    CTable2AsnValidator(CTable2AsnContext& ctx);
    ~CTable2AsnValidator();

    void Clear();
    void Validate(CRef<objects::CSeq_submit> pSubmit,
                  CRef<objects::CSeq_entry>  pEntry,
                  CValidMessageHandler&      msgHandler);

    void   ValCollect(CRef<objects::CSeq_submit> submit, CRef<objects::CSeq_entry> entry, const string& flags);
    void   ValReportErrorStats(CNcbiOstream& out);
    void   ValReportErrors();
    size_t ValTotalErrors() const;

    void Cleanup(CRef<objects::CSeq_submit> submit, objects::CSeq_entry_Handle& entry, const string& flags) const;
    void UpdateECNumbers(objects::CSeq_entry& entry);

    void CollectDiscrepancies(CRef<objects::CSeq_submit> submit, objects::CSeq_entry_Handle& entry);
    void ReportDiscrepancies();
    void ReportDiscrepancies(const string& filename);

    using TGlobalInfo = objects::validator::CHugeFileValidator::TGlobalInfo;
    const TGlobalInfo& GetGlobalInfo() { return m_val_globalInfo; }
    using TValidatorContext = objects::validator::SValidatorContext;
    shared_ptr<TValidatorContext> GetContextPtr() { return m_val_context; }


protected:
    using TDiscProdRef = CRef<NDiscrepancy::CDiscrepancyProduct>;
    TDiscProdRef x_PopulateDiscrepancy(CRef<objects::CSeq_submit>  submit,
                                       objects::CSeq_entry_Handle& entry) const;
    void         x_ReportDiscrepancies(TDiscProdRef& discrepancy, std::ostream& ostr) const;
    using TErrorStatMap = map<int, size_t>;
    class TErrorStats
    {
    public:
        TErrorStats() :
            total(0){};
        TErrorStatMap individual;
        size_t        total;
    };
    CTable2AsnContext* m_context;

    std::mutex   m_discrep_mutex;
    TDiscProdRef m_discr_product;

    std::shared_ptr<objects::validator::SValidatorContext> m_val_context;
    TValStats                                              m_val_stats;
    std::list<CRef<objects::CValidError>>                  m_val_errors;
    TGlobalInfo                                            m_val_globalInfo;
    mutable std::mutex                                     m_mutex;
};

void g_FormatErrItem(const objects::CValidErrItem& item, CNcbiOstream& ostr);

void g_FormatValStats(const TValStats& stats, size_t total, CNcbiOstream& ostr);

END_NCBI_SCOPE

#endif
