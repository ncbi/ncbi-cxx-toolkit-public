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

        void SetFilename(const string& filename);
        void FixSuspectProductNames(objects::CSeq_entry& entry);
        bool FixSuspectProductNames(objects::CSeq_feat& feature);
        bool FixSuspectProductName(string& product_name);

        void ReportFixedProduct(const string& oldproduct, const string& newproduct, const objects::CSeq_loc& loc, const string& locustag);

        string m_fixed_product_report_filename;
        auto_ptr<CNcbiOfstream> m_report_ostream;
    protected:
        string m_rules_filename;
        CConstRef<CSuspect_rule_set> m_rules;
    };
};

END_NCBI_SCOPE

#endif
