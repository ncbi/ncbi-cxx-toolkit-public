#ifndef __SUSPECT_FEATURE_RULES_HPP_INCLUDED__
#define __SUSPECT_FEATURE_RULES_HPP_INCLUDED__


#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CSeq_loc;
    class CSuspect_rule_set;
    class CSuspect_rule;
    class CScope;
    namespace feature
    {
        class CFeatTree;
    }

    class CFixSuspectProductName
    {
    public:
        CFixSuspectProductName();
        ~CFixSuspectProductName();

        void SetRulesFilename(const string& filename);
        void SetupOutput(std::function<std::ostream&()> f);
        void FixSuspectProductNames(objects::CSeq_entry& entry, CScope& scope) const;
        bool FixSuspectProductNames(objects::CSeq_feat& feature, CScope& scope, feature::CFeatTree& feattree) const;

    protected:
        void ReportFixedProduct(const string& oldproduct, const string& newproduct, const objects::CSeq_loc& loc, const string& locustag) const;
        CConstRef<CSuspect_rule> x_FixSuspectProductName(string& product_name) const;
        std::function<std::ostream&()> m_output;

        string m_rules_filename;
        CConstRef<CSuspect_rule_set> m_rules;
    };
}

END_NCBI_SCOPE

#endif
