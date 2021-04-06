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
        void SetupOutput(std::function<CNcbiOstream&()> f);
        void FixSuspectProductNames(objects::CSeq_entry& entry, CScope& scope);
        bool FixSuspectProductNames(objects::CSeq_feat& feature);
        CRef<feature::CFeatTree>& SetFeatTree()
        {
            return m_feattree;
        }

        void ReportFixedProduct(const string& oldproduct, const string& newproduct, const objects::CSeq_loc& loc, const string& locustag);

    protected:
        CConstRef<CSuspect_rule> x_FixSuspectProductName(string& product_name);
        std::function<CNcbiOstream&()> m_output;

        string m_rules_filename;
        CConstRef<CSuspect_rule_set> m_rules;
        CRef<feature::CFeatTree> m_feattree;
        CRef<CScope> m_scope;
    };
}

END_NCBI_SCOPE

#endif
