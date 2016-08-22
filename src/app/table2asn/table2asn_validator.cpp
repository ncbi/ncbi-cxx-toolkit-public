#include <ncbi_pch.hpp>

#include <objtools/validator/validator.hpp>
#include <objects/valerr/ValidError.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <objmgr/annot_ci.hpp>
#include <objmgr/feat_ci.hpp>

#include "table2asn_validator.hpp"
#include "table2asn_context.hpp"

#include <misc/discrepancy/discrepancy.hpp>


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace {

    static const char* big_separator = "=================================================================";

CNcbiOfstream& InitOstream(auto_ptr<CNcbiOfstream>& ostr, const string& fname)
{
    if (ostr.get() == 0)
        ostr.reset(new CNcbiOfstream(fname.c_str()));

    return *ostr;
}

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

void CTable2AsnValidator::Cleanup(CSeq_entry_Handle& h_entry, const string& flags)
{
    CRef<CSeq_entry> entry((CSeq_entry*)(h_entry.GetEditHandle().GetCompleteSeq_entry().GetPointer()));
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
        CConstRef<CCleanupChange> changes = cleanup.ExtendedCleanup(*entry, CCleanup::eClean_SyncGenCodes | CCleanup::eClean_NoNcbiUserObjects);
        need_recalculate_index = true;
    }
    else
    {
        cleanup.BasicCleanup(*entry, CCleanup::eClean_SyncGenCodes | CCleanup::eClean_NoNcbiUserObjects);
        if (flags.find('U') != string::npos)
          cleanup.RemoveUnnecessaryGeneXrefs(h_entry); //remove unnec gen xref included in extended cleanup
    }

    if (need_recalculate_index) {
        CScope& scope = h_entry.GetScope();
        scope.RemoveTopLevelSeqEntry(h_entry);
        h_entry = scope.AddTopLevelSeqEntry(*entry);
    }
}

void CTable2AsnValidator::Validate(CRef<CSeq_submit> submit, CRef<CSeq_entry> entry, const string& flags, const string& report_name)
{
    CScope scope(*CObjectManager::GetInstance());
    validator::CValidator validator(scope.GetObjectManager());

    Uint4 opts = 0;
    CConstRef<CValidError> errors;

    if (submit.Empty())
    {
        CSeq_entry_Handle top_se = scope.AddTopLevelSeqEntry(*entry);
        errors = validator.Validate(top_se, opts);
    }
    else
    {
        ITERATE(CSeq_submit::C_Data::TEntrys, it, submit->GetData().GetEntrys())
        {
            scope.AddTopLevelSeqEntry(**it);
        }
        errors = validator.Validate(*submit, &scope, opts);
    }
    if (errors.NotEmpty())
    {
        CNcbiOfstream file(report_name.c_str());
        ReportErrors(errors, file);
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

void CTable2AsnValidator::ReportDiscrepancies(CSerialObject& obj, CScope& scope, const string& fname)
{
    CRef<NDiscrepancy::CDiscrepancySet> tests = NDiscrepancy::CDiscrepancySet::New(scope);
    vector<string> names = NDiscrepancy::GetDiscrepancyNames(NDiscrepancy::eSubmitter);
    tests->AddTests(names);
    tests->SetFile(m_context->m_current_file);

//    Tests->SetSuspectRules(m_SuspectRules);
//    Tests->SetLineage(m_Lineage);

    tests->Parse(obj);
    tests->Summarize();
    CNcbiOfstream output(fname.c_str());
    bool print_fatal = true;
    tests->OutputText(output, print_fatal, false, false);
}

void CTable2AsnValidator::UpdateECNumbers(objects::CSeq_entry_Handle seh, const string& fname, auto_ptr<CNcbiOfstream>& ostream)
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
                    InitOstream(ostream, fname) << label << "\tEC number deleted\t" << *val << '\t' << endl;
                    val = EC.erase(val);
                    continue;
                    break;
                case CProt_ref::eEC_replaced:
                {
                    xGetLabel(feat, label);
                    const string& newvalue = CProt_ref::GetECNumberReplacement(*val);
                    bool is_split = newvalue.find('\t') != string::npos;
                    InitOstream(ostream, fname) << label <<
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
                    InitOstream(ostream, fname) << label << "\tEC number invalid\t" << *val << '\t' << endl;
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

