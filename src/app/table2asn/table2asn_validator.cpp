#include <ncbi_pch.hpp>

#include <objtools/validator/validator.hpp>
#include <objects/valerr/ValidError.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_ci.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/loc_edit.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objtools/validator/tax_validation_and_cleanup.hpp>
#include <objtools/edit/remote_updater.hpp>

#include "table2asn_validator.hpp"
#include "table2asn_context.hpp"

#include "visitors.hpp"

#include <misc/discrepancy/discrepancy.hpp>


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace {

    static const char* big_separator = "=================================================================";

void xGetLabel(const CSeq_feat& feat, string& label)
{
    if (label.empty())
    {
        if (feat.GetLocation().IsInt())
            feat.GetLocation().GetInt().GetId().GetLabel(&label, CSeq_id::eFasta);
        else
            feat.GetLocation().GetLabel(&label);
    }
}

} // end anonymous namespace

CTable2AsnValidator::CTable2AsnValidator(CTable2AsnContext& ctx) : m_stats(CValidErrItem::eSev_trace), m_context(&ctx)
{
}

void CTable2AsnValidator::Cleanup(CRef<CSeq_submit> submit, CSeq_entry_Handle& h_entry, const string& flags)
{
    if (flags.find('w') != string::npos)
    {
        CCleanup::WGSCleanup(h_entry, true, CCleanup::eClean_NoNcbiUserObjects, false);
    }

    // ignore 'e' flag, run ExtendedCleanup() uncoditionally - but only after 'x'

    if (flags.find('d') != string::npos) {
        CCleanup::CleanupCollectionDates(h_entry, true);
    } else if (flags.find('D') != string::npos) {
        CCleanup::CleanupCollectionDates(h_entry, false);
    }

    if (flags.find('x') != string::npos) {
        CBioseq_CI bi(h_entry, CSeq_inst::eMol_na);
        while (bi) {
            if (edit::ExtendPartialFeatureEnds(*bi)) {
            }
            ++bi;
        }
    }
    auto cleanupflags = CCleanup::eClean_SyncGenCodes | CCleanup::eClean_NoNcbiUserObjects;
    if (m_context->m_huge_files_mode)
        cleanupflags |= ( CCleanup::eClean_KeepTopSet | CCleanup::eClean_KeepSingleSeqSet);

    if (submit) {
        CCleanup cleanup(&(h_entry.GetScope()), CCleanup::eScope_UseInPlace); // RW-1070 - CCleanup::eScope_UseInPlace is essential
        cleanup.ExtendedCleanup(*submit, cleanupflags);
    }
    else {
        CCleanup::ExtendedCleanup(h_entry, cleanupflags);
    }

    if (flags.find('f') != string::npos)
    {
        m_context->m_suspect_rules.FixSuspectProductNames(*const_cast<CSeq_entry*>(h_entry.GetCompleteSeq_entry().GetPointer()), h_entry.GetScope());
    }

    // SQD-4386
    {
        for (CBioseq_CI it(h_entry); *it; ++it)
        {
            CCleanup::AddProteinTitle(*it);
        }
    }

    if (flags.find('s') != string::npos)
    {
        CCleanup::AddLowQualityException(h_entry);
    }

    if (flags.find('T') != string::npos)
    {
        validator::CTaxValidationAndCleanup tval(
            [&remote = m_context->m_remote_updater](const vector< CRef<COrg_ref> >& query) -> CRef<CTaxon3_reply>
            { // we need to make a copy of record to prevent changes put back to cache
                CConstRef<CTaxon3_reply> res = remote->SendOrgRefList(query);
                CRef<CTaxon3_reply> copied (new CTaxon3_reply);
                copied->Assign(*res);
                return copied;
            }
        );
        tval.DoTaxonomyUpdate(h_entry, true);
    }
}

void CTable2AsnValidator::Validate(CRef<CSeq_submit> submit, CRef<CSeq_entry> entry, const string& flags)
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    validator::CValidator validator(scope.GetObjectManager());

    Uint4 options = 0;
    if (m_context->m_master_genome_flag == "n")
        options |= validator::CValidator::eVal_genome_submission;

    CConstRef<CValidError> errors;

    if (submit.Empty())
    {
        CSeq_entry_Handle top_se = scope.AddTopLevelSeqEntry(*entry);
        errors = validator.Validate(top_se, options);
    }
    else
    {
        ITERATE(CSeq_submit::C_Data::TEntrys, it, submit->GetData().GetEntrys())
        {
            scope.AddTopLevelSeqEntry(**it);
        }
        errors = validator.Validate(*submit, &scope, options);
    }
    if (errors.NotEmpty())
    {
        ReportErrors(errors, m_context->GetOstream(".val"));
    }
}

void CTable2AsnValidator::ReportErrors(CConstRef<CValidError> errors, CNcbiOstream& out)
{
    ITERATE(CValidError::TErrs, it, errors->GetErrs())
    {
        const CValidErrItem& item = **it;
        out << CValidErrItem::ConvertSeverity(EDiagSev(item.GetSev()))
               << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() <<"] "
               << item.GetMsg() << " " << item.GetObjDesc() << endl;

        m_stats[item.GetSev()].m_total++;
        m_stats[item.GetSev()].m_individual[item.GetErrIndex()]++;
    }
    //out << MSerial_AsnText << *errors;
}

size_t CTable2AsnValidator::TotalErrors() const
{
    size_t result = 0;
    ITERATE(vector<TErrorStats>, stats, m_stats)
    {
        result += stats->m_total;
    }
    return result;
}

void CTable2AsnValidator::ReportErrorStats(CNcbiOstream& out)
{
    out << "Total messages:\t\t" << NStr::NumericToString(TotalErrors()) << endl << endl << big_separator << endl;

    for (size_t sev = 0; sev < m_stats.size(); sev++)
    {
        if (m_stats[sev].m_individual.empty())
            continue;

        string severity = CValidErrItem::ConvertSeverity(EDiagSev(sev));
        NStr::ToUpper(severity);
        out << NStr::NumericToString(m_stats[sev].m_total) << " " << severity << "-level messages exist" << endl << endl;

        for_each(m_stats[sev].m_individual.begin(), m_stats[sev].m_individual.end(), [&out](const TErrorStatMap::const_iterator::value_type& it)
        {
            out <<
                CValidErrItem::ConvertErrGroup(it.first) << "." <<
                CValidErrItem::ConvertErrCode(it.first) << ":\t" << NStr::NumericToString(it.second) << endl;
        });
        out << endl << big_separator << endl;
    }
}

void CTable2AsnValidator::CollectDiscrepancies(const CSerialObject& obj, bool eukaryote, const string& lineage)
{
    if (!m_discrepancy)
        m_discrepancy = NDiscrepancy::CDiscrepancySet::New(*m_context->m_scope);
    vector<string> names = NDiscrepancy::GetDiscrepancyNames(m_context->m_discrepancy_group);
    m_discrepancy->AddTests(names);

    CFile nm(m_context->GenerateOutputFilename(m_context->m_asn1_suffix));
    m_discrepancy->SetLineage(lineage);
    //m_discrepancy->SetEukaryote(eukaryote);
    m_discrepancy->Parse(obj, nm.GetName());
}

void CTable2AsnValidator::ReportDiscrepancy(const CSerialObject& obj, bool eukaryote, const string& lineage)
{
    CRef<NDiscrepancy::CDiscrepancySet> discrepancy = NDiscrepancy::CDiscrepancySet::New(*m_context->m_scope);
    vector<string> names = NDiscrepancy::GetDiscrepancyNames(m_context->m_discrepancy_group);
    discrepancy->AddTests(names);

    CFile nm(m_context->GenerateOutputFilename(m_context->m_asn1_suffix));
    discrepancy->SetLineage(lineage);
    //discrepancy->SetEukaryote(eukaryote);
    discrepancy->Parse(obj, nm.GetName());

    discrepancy->Summarize();
    unsigned short output_flags = NDiscrepancy::CDiscrepancySet::eOutput_Files;
    if (!m_context->m_master_genome_flag.empty()) {
        output_flags |= NDiscrepancy::CDiscrepancySet::eOutput_Fatal;
    }
    discrepancy->OutputText(m_context->GetOstream(".dr"), output_flags);
    m_context->ClearOstream(".dr");
}

void CTable2AsnValidator::ReportDiscrepancies()
{
    if (m_discrepancy.NotEmpty())
    {
        m_discrepancy->Summarize();
        unsigned short output_flags = NDiscrepancy::CDiscrepancySet::eOutput_Files;
        if (!m_context->m_master_genome_flag.empty()) {
            output_flags |= NDiscrepancy::CDiscrepancySet::eOutput_Fatal;
        }
        m_discrepancy->OutputText(m_context->GetOstream(".dr", m_context->m_base_name), output_flags);
    }
}


class CUpdateECNumbers
{
public:
    CUpdateECNumbers(CTable2AsnContext& context)
        : m_Context(context) {}

    void operator()(CSeq_feat& feat);
private:
    CTable2AsnContext& m_Context;
};


void CUpdateECNumbers::operator()(CSeq_feat& feat)
{
    if (!feat.IsSetData() ||
        !feat.GetData().IsProt() ||
        !feat.GetData().GetProt().IsSetEc()) {
        return;
    }

    string label;
    auto& EC = feat.SetData().SetProt().SetEc();
    auto it = EC.begin();
    while(it != EC.end())
    {
        switch (CProt_ref::GetECNumberStatus(*it))
        {
        case CProt_ref::eEC_deleted:
            xGetLabel(feat, label);
            m_Context.GetOstream(".ecn") << label << "\tEC number deleted\t" << *it << '\t' << endl;
            it = EC.erase(it);
            continue;
            break;
        case CProt_ref::eEC_replaced:
        {
            xGetLabel(feat, label);
            const string& newvalue = CProt_ref::GetECNumberReplacement(*it);
            bool is_split = newvalue.find('\t') != string::npos;
            m_Context.GetOstream(".ecn") << label <<
            (is_split ? "\tEC number split\t" : "\tEC number changed\t")
            << *it << '\t' << newvalue << endl;
            if (is_split) {
                it = EC.erase(it);
                continue;
            }
            *it = newvalue;
        }
        break;
        case CProt_ref::eEC_unknown:
            xGetLabel(feat, label);
            m_Context.GetOstream(".ecn") << label << "\tEC number invalid\t" << *it << '\t' << endl;
            break;
        default:
            break;
        }
        ++it;
    }
    if (EC.empty())
    {
        feat.SetData().SetProt().ResetEc();
    }
}


void CTable2AsnValidator::UpdateECNumbers(CSeq_entry& entry)
{
    VisitAllFeatures(entry, CUpdateECNumbers(*m_context));
}

END_NCBI_SCOPE
