#ifndef __SUSPECT_FEATURE_RULES_HPP_INCLUDED__
#define __SUSPECT_FEATURE_RULES_HPP_INCLUDED__


#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CSeq_loc;
    class CSuspect_rule_set;

    class CFixSuspectProductName
    {
    public:
        CFixSuspectProductName();
        ~CFixSuspectProductName();

        void FixProductNames(objects::CBioseq& bioseq);
        bool FixProductNames(objects::CSeq_feat& feature);

        void ReportFixedProduct(const string& oldproduct, const string& newproduct, const objects::CSeq_loc& loc, const string& locustag);
        bool FixSuspectProductName(string& product_name);

        string m_fixed_product_report_filename;
        auto_ptr<CNcbiOfstream> m_report_ostream;
    protected:
        string m_rules_filename;
        CRef<CSuspect_rule_set> m_rules;
    };
};

END_NCBI_SCOPE

#endif
