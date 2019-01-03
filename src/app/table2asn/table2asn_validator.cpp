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

void CTable2AsnValidator::Cleanup(CRef<objects::CSeq_submit> submit, CSeq_entry_Handle& h_entry, const string& flags)
{
    bool need_recalculate_index = false;

    CCleanup cleanup;
    if (flags.find('w') != string::npos)
    {
        CCleanup::WGSCleanup(h_entry);
        need_recalculate_index = true;
    }
    else
    if (flags.find('e') != string::npos)
    {
        CConstRef<CCleanupChange> changes = cleanup.ExtendedCleanup(h_entry, CCleanup::eClean_SyncGenCodes);
        need_recalculate_index = true;
    }
    else
    {
        if (submit)
            cleanup.BasicCleanup(*submit, CCleanup::eClean_SyncGenCodes);
        else
            cleanup.BasicCleanup(h_entry, CCleanup::eClean_SyncGenCodes);

        if (flags.find('U') != string::npos)
            cleanup.RemoveUnnecessaryGeneXrefs(h_entry); //remove unnec gen xref included in extended cleanup
    }
    if (flags.find('x') != string::npos) {
        CBioseq_CI bi(h_entry, CSeq_inst::eMol_na);
        while (bi) {
            edit::ExtendPartialFeatureEnds(*bi);
            ++bi;
        }
    }

    if (flags.find('d') != string::npos) {
        CCleanup::CleanupCollectionDates(h_entry, true);
    } else if (flags.find('D') != string::npos) {
        CCleanup::CleanupCollectionDates(h_entry, false);
    }
    

    CRef<CSeq_entry> entry((CSeq_entry*)(h_entry.GetEditHandle().GetCompleteSeq_entry().GetPointer()));

    if (need_recalculate_index) {
        CScope& scope = h_entry.GetScope();
        scope.RemoveTopLevelSeqEntry(h_entry);
        h_entry = scope.AddTopLevelSeqEntry(*entry);
    }

    if (flags.find('f') != string::npos)
    {        
        m_context->m_suspect_rules.FixSuspectProductNames((*(CSeq_entry*)h_entry.GetCompleteSeq_entry().GetPointer()), h_entry.GetScope());
    }

    // SQD-4386
    {
        for (CBioseq_CI it(h_entry); *it; ++it)
        {
            cleanup.AddProteinTitle(*it);
        }
    }

    if (flags.find('s') != string::npos)
    {
        CCleanup::AddLowQualityException(h_entry.GetEditHandle());
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
        ReportErrors(errors, m_context->GetOstream(".val", m_context->m_base_name));
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

void CTable2AsnValidator::InitDisrepancyReport(objects::CScope& scope)
{
    if (m_discrepancy.NotEmpty())
        return;

    m_discrepancy = NDiscrepancy::CDiscrepancySet::New(scope);
    vector<string> names = NDiscrepancy::GetDiscrepancyNames(NDiscrepancy::eSubmitter);
    m_discrepancy->AddTests(names);
}

void CTable2AsnValidator::CollectDiscrepancies(CSerialObject& obj, bool eucariote, const string& lineage)
{
    CFile nm(m_context->GenerateOutputFilename(m_context->m_asn1_suffix));
    m_discrepancy->SetFile(nm.GetName());
    m_discrepancy->SetLineage(lineage);
    m_discrepancy->SetEucariote(eucariote);
    m_discrepancy->Parse(obj);
}

void CTable2AsnValidator::ReportDiscrepancies()
{
    if (m_context->m_discrepancy) {
        m_discrepancy->Summarize();
        bool print_fatal = true;
        m_discrepancy->OutputText(m_context->GetOstream(".dr", m_context->m_base_name), print_fatal, false, true);
    }
}

void CTable2AsnValidator::UpdateECNumbers(objects::CSeq_entry_Handle seh)
{
    string label;
    for (CFeat_CI feat_it(seh); feat_it; ++feat_it)
    {
        CSeq_feat& feat = (CSeq_feat&) feat_it->GetOriginalFeature();

        label.clear();

        if (feat.IsSetData() && feat.GetData().IsProt() && feat.GetData().GetProt().IsSetEc())
        {
            CProt_ref::TEc& EC = feat.SetData().SetProt().SetEc();
            CProt_ref::TEc::iterator val = EC.begin();
            for (; val != EC.end();)
            {
                switch (CProt_ref::GetECNumberStatus(*val))
                {
                case CProt_ref::eEC_deleted:
                    xGetLabel(feat, label);
                    m_context->GetOstream(".ecn") << label << "\tEC number deleted\t" << *val << '\t' << endl;
                    val = EC.erase(val);
                    continue;
                    break;
                case CProt_ref::eEC_replaced:
                {
                    xGetLabel(feat, label);
                    const string& newvalue = CProt_ref::GetECNumberReplacement(*val);
                    bool is_split = newvalue.find('\t') != string::npos;
                    m_context->GetOstream(".ecn") << label <<
                        (is_split ? "\tEC number split\t" : "\tEC number changed\t")
                        << *val << '\t' << newvalue << endl;
                    if (is_split) {
                        val = EC.erase(val);
                        continue;
                    } else {
                        *val = newvalue;
                    }
                }
                break;
                case CProt_ref::eEC_unknown:
                    xGetLabel(feat, label);
                    m_context->GetOstream(".ecn") << label << "\tEC number invalid\t" << *val << '\t' << endl;
                    break;
                default:
                    break;
                }
                val++;
            }
            if (EC.empty())
            {
                feat.SetData().SetProt().ResetEc();
            }
        }
    }
}


END_NCBI_SCOPE

