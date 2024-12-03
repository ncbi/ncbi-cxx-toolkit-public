#include <ncbi_pch.hpp>

#include <objtools/validator/validator.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objects/valerr/ValidErrItem.hpp>

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

#include <objtools/validator/entry_info.hpp>
#include <objtools/validator/utilities.hpp>

#include "table2asn_validator.hpp"
#include "table2asn_context.hpp"
#include "suspect_feat.hpp"
#include "visitors.hpp"

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

void CValidMessageHandler::AddValidErrItem(
    EDiagSev             sev,
    unsigned int         ec,
    const string&        msg,
    const string&        desc,
    const CSerialObject& obj,
    const string&        acc,
    const int            ver,
    const string&        location,
    const int            seq_offset)
{
    CRef<CValidErrItem> pItem(new CValidErrItem(sev, ec, msg, desc, &obj, nullptr, acc, ver, seq_offset));
    if (! NStr::IsBlank(location)) {
        pItem->SetLocation(location);
    }

    AddValidErrItem(pItem);
}


void CValidMessageHandler::AddValidErrItem(
    EDiagSev      sev,
    unsigned int  ec,
    const string& msg,
    const string& desc,
    const string& acc,
    const int     ver,
    const string& location,
    const int     seq_offset)
{
    CRef<CValidErrItem> pItem(new CValidErrItem(sev, ec, msg, desc, nullptr, nullptr, acc, ver, seq_offset));
    if (! NStr::IsBlank(location)) {
        pItem->SetLocation(location);
    }

    AddValidErrItem(pItem);
}


void CValidMessageHandler::AddValidErrItem(
    EDiagSev          sev,
    unsigned int      ec,
    const string&     msg,
    const string&     desc,
    const CSeqdesc&   seqdesc,
    const CSeq_entry& ctx,
    const string&     acc,
    const int         ver,
    const int         seq_offset)
{
    CRef<CValidErrItem> pItem(new CValidErrItem(sev, ec, msg, desc, &seqdesc, &ctx, acc, ver, seq_offset));
    AddValidErrItem(pItem);
}


void CValidMessageHandler::AddValidErrItem(EDiagSev sev, unsigned int ec, const string& msg)
{
    CRef<CValidErrItem> pItem(new CValidErrItem());
    pItem->SetSev(sev);
    pItem->SetErrIndex(ec);
    pItem->SetMsg(msg);
    pItem->SetErrorName(CValidErrItem::ConvertErrCode(ec));
    pItem->SetErrorGroup(CValidErrItem::ConvertErrGroup(ec));

    AddValidErrItem(pItem);
}


static bool s_PostponeItem(const CValidErrItem& item)
{
    const auto& errCode = item.GetErrIndex();

    if (errCode == eErr_SEQ_DESCR_NoPubFound) {
        const auto& msg = item.GetMsg();
        return NStr::StartsWith(msg, "No publications anywhere on this entire record.") ||
               NStr::StartsWith(msg, "No publications refer to this Bioseq.");
    }

    if (errCode == eErr_GENERIC_MissingPubRequirement) {
        return NStr::StartsWith(item.GetMsg(), "No submission citation anywhere on this entire record.");
    }

    if (errCode == eErr_SEQ_DESCR_NoSourceDescriptor ||
        errCode == eErr_SEQ_DESCR_TransgenicProblem ||
        errCode == eErr_SEQ_DESCR_InconsistentBioSources_ConLocation ||
        errCode == eErr_SEQ_INST_MitoMetazoanTooLong ||
        errCode == eErr_SEQ_DESCR_NoOrgFound) {
        return true;
    }

    return false;
}


void g_FormatErrItem(const CValidErrItem& item, CNcbiOstream& ostr)
{
    ostr << CValidErrItem::ConvertSeverity(EDiagSev(item.GetSev()))
         << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() << "] "
         << item.GetMsg() << " " << item.GetObjDesc() << "\n";
}


void CValidMessageHandler::AddValidErrItem(CRef<CValidErrItem> pItem)
{
    if (m_Mode == EMode::Serial) {
        if (pItem) {
            x_LogStats(*pItem);
            g_FormatErrItem(*pItem, m_Ostr);
        }
        return;
    }
    // else
    x_AddItemAsync(pItem);
}


void CValidMessageHandler::x_AddItemAsync(CRef<CValidErrItem> pItem)
{
    if (pItem && s_PostponeItem(*pItem)) {
        lock_guard<mutex> lock(m_PostponedMutex);
        m_PostponedItems.push_back(pItem);
        return;
    }

    {
        lock_guard<mutex> lock(m_Mutex);
        m_Items.push(pItem);
        if (pItem) {
            x_LogStats(*pItem);
        }
    }
    m_Cv.notify_all();
}


void CValidMessageHandler::x_LogStats(const objects::CValidErrItem& item)
{
    const auto sev = item.GetSev();
    m_ProcessedStats[sev].total++;
    m_ProcessedStats[sev].individual[item.GetErrIndex()]++;
}


void CValidMessageHandler::Write()
{
    if (m_Mode == EMode::Serial) {
        return;
    }

    while (true) {
        unique_lock<mutex> lock(m_Mutex);
        m_Cv.wait(lock, [this]() -> bool {
            return ! m_Items.empty();
        });
        auto pItem = m_Items.front();
        m_Items.pop();
        if (! pItem) {
            break;
        }
        g_FormatErrItem(*pItem, m_Ostr);
    }
}


void CValidMessageHandler::RequestStop()
{
    if (m_Mode == EMode::Serial) {
        return;
    }

    AddValidErrItem(CRef<CValidErrItem>());
}


const CValidMessageHandler::TPostponed& CValidMessageHandler::GetPostponed() const
{
    return m_PostponedItems;
}


namespace
{

    static const char* big_separator = "=================================================================";

    void xGetLabel(const CSeq_feat& feat, string& label)
    {
        if (label.empty()) {
            if (feat.GetLocation().IsInt())
                feat.GetLocation().GetInt().GetId().GetLabel(&label, CSeq_id::eFasta);
            else
                feat.GetLocation().GetLabel(&label);
        }
    }

} // end anonymous namespace

CTable2AsnValidator::CTable2AsnValidator(CTable2AsnContext& ctx) :
    m_context(&ctx),
    m_val_context(make_shared<validator::SValidatorContext>())
{
    m_val_context->m_taxon_update = m_context->m_remote_updater->GetUpdateFunc();
}

CTable2AsnValidator::~CTable2AsnValidator()
{
}

void CTable2AsnValidator::Cleanup(CRef<CSeq_submit> submit, CSeq_entry_Handle& h_entry, const string& flags) const
{
    if (flags.find('w') != string::npos) {
        CCleanup::WGSCleanup(h_entry, true, CCleanup::eClean_NoNcbiUserObjects, false, true);
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
        cleanupflags |= (CCleanup::eClean_KeepTopSet | CCleanup::eClean_KeepSingleSeqSet);

    if (submit) {
        CCleanup cleanup(&(h_entry.GetScope()), CCleanup::eScope_UseInPlace); // RW-1070 - CCleanup::eScope_UseInPlace is essential
        cleanup.ExtendedCleanup(*submit, cleanupflags);
    } else {
        CCleanup::ExtendedCleanup(h_entry, cleanupflags);
    }

    if (flags.find('f') != string::npos) {
        m_context->m_suspect_rules->FixSuspectProductNames(h_entry);
    }

    // SQD-4386
    {
        for (CBioseq_CI it(h_entry); *it; ++it) {
            CCleanup::AddProteinTitle(*it);
        }
    }

    if (flags.find('s') != string::npos) {
        CCleanup::AddLowQualityException(h_entry);
    }

    if (flags.find('T') != string::npos) {
        validator::CTaxValidationAndCleanup tval(m_val_context->m_taxon_update);
        tval.DoTaxonomyUpdate(h_entry, true);
    }
}


void CTable2AsnValidator::Validate(CRef<CSeq_submit>     pSubmit,
                                   CRef<CSeq_entry>      pEntry,
                                   CValidMessageHandler& msgHandler)
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    m_val_context->PostprocessHugeFile = false;
    if (m_context->m_huge_files_mode) {
        std::lock_guard<std::mutex> g{ m_mutex };
        m_val_context->PostprocessHugeFile = true;
        if (m_val_context->HugeSetId.empty()) {
            int unneeded_version;
            // The following is incorrect.
            // Can't guarantee that HugeSeqId will be set correctly.
            m_val_context->HugeSetId = validator::GetAccessionFromObjects(nullptr, pEntry, scope, &unneeded_version);
        }
    }

    validator::CValidator validator(scope.GetObjectManager(), m_val_context);

    Uint4 options = 0;
    if (m_context->m_master_genome_flag == "n")
        options |= validator::CValidator::eVal_genome_submission;

    CRef<CValidError> errors;
    if (pSubmit.Empty()) {
        CSeq_entry_Handle top_se = scope.AddTopLevelSeqEntry(*pEntry);
        validator.Validate(top_se, options, msgHandler);
    } else {
        for (auto& it : pSubmit->GetData().GetEntrys()) {
            scope.AddTopLevelSeqEntry(*it);
        }
        validator.Validate(*pSubmit, &scope, options, msgHandler);
    }


    {
        std::lock_guard<std::mutex> g{ m_mutex };

        // Accumulate global info
        const auto& entryInfo = validator.GetEntryInfo();
        m_val_globalInfo.NoPubsFound &= entryInfo.IsNoPubs();
        m_val_globalInfo.NoCitSubsFound &= entryInfo.IsNoCitSubPubs();
        m_val_globalInfo.NoBioSource &= entryInfo.IsNoBioSource();
    }
}


void CTable2AsnValidator::ValCollect(CRef<CSeq_submit> submit, CRef<CSeq_entry> entry, const string& flags)
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    if (m_context->m_huge_files_mode) {
        std::lock_guard<std::mutex> g{ m_mutex };
        m_val_context->PostprocessHugeFile = true;
        //        if (m_val_context->HugeSetId.empty()) {
        //            int unneded_version;
        //            m_val_context->HugeSetId = validator::GetAccessionFromObjects(nullptr, entry, scope, &unneded_version);
        //        }
    }

    validator::CValidator validator(scope.GetObjectManager(), m_val_context);

    Uint4 options = 0;
    if (m_context->m_master_genome_flag == "n")
        options |= validator::CValidator::eVal_genome_submission;

    CRef<CValidError> errors;

    if (submit.Empty()) {
        CSeq_entry_Handle top_se = scope.AddTopLevelSeqEntry(*entry);
        errors                   = validator.Validate(top_se, options);
    } else {
        for (auto& it : submit->GetData().GetEntrys()) {
            scope.AddTopLevelSeqEntry(*it);
        }
        errors = validator.Validate(*submit, &scope, options);
    }

    if (errors.NotEmpty()) {
        std::lock_guard<std::mutex> g{ m_mutex };
        m_val_errors.push_back(errors);
        // Accumulate global info
        const auto& entryInfo = validator.GetEntryInfo();
        m_val_globalInfo.NoPubsFound &= entryInfo.IsNoPubs();
        m_val_globalInfo.NoCitSubsFound &= entryInfo.IsNoCitSubPubs();
        m_val_globalInfo.NoBioSource &= entryInfo.IsNoBioSource();
    }
}

void CTable2AsnValidator::Clear()
{
    std::lock_guard<std::mutex> g{ m_mutex };
    m_val_errors.clear();
    m_val_globalInfo.Clear();
    // m_discrepancy.Reset();
}

void CTable2AsnValidator::ValReportErrors()
{
    std::lock_guard<std::mutex> g{ m_mutex };

    CNcbiOstream& out = m_context->GetOstream(eFiles::val);

    for (auto& errors : m_val_errors) {
        if (m_context->m_huge_files_mode)
            g_PostprocessErrors(m_val_globalInfo, m_val_context->HugeSetId, errors);
        for (auto& it : errors->GetErrs()) {
            const CValidErrItem& item = *it;
            out << CValidErrItem::ConvertSeverity(EDiagSev(item.GetSev()))
                << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() << "] "
                << item.GetMsg() << " " << item.GetObjDesc() << endl;

            m_val_stats[item.GetSev()].total++;
            m_val_stats[item.GetSev()].individual[item.GetErrIndex()]++;
        }
        // out << MSerial_AsnText << *errors;
    }
}

size_t CTable2AsnValidator::ValTotalErrors() const
{
    std::lock_guard<std::mutex> g(m_mutex);

    size_t result = 0;
    for (auto& stats : m_val_stats) {
        result += stats.total;
    }
    return result;
}

void g_FormatValStats(const TValStats& stats, size_t total, CNcbiOstream& ostr)
{
    ostr << "Total messages:\t\t" << NStr::NumericToString(total) << "\n\n"
         << big_separator << "\n";

    for (size_t sev = 0; sev < stats.size(); ++sev) {
        if (stats[sev].total > 0) {
            auto severity = CValidErrItem::ConvertSeverity(EDiagSev(sev));
            NStr::ToUpper(severity);
            ostr << NStr::NumericToString(stats[sev].total) << " " << severity << "-level messages exist"
                 << "\n\n";
            for (auto indivEntry : stats[sev].individual) {
                ostr << CValidErrItem::ConvertErrGroup(indivEntry.first) << "."
                     << CValidErrItem::ConvertErrCode(indivEntry.first) << ":\t"
                     << NStr::NumericToString(indivEntry.second) << "\n";
            }
            ostr << "\n"
                 << big_separator << "\n";
        }
    }
}


void CTable2AsnValidator::ValReportErrorStats(CNcbiOstream& out)
{
    out << "Total messages:\t\t" << NStr::NumericToString(ValTotalErrors()) << "\n\n"
        << big_separator << "\n";

    std::lock_guard<std::mutex> g(m_mutex);

    for (size_t sev = 0; sev < m_val_stats.size(); sev++) {
        if (m_val_stats[sev].individual.empty())
            continue;

        string severity = CValidErrItem::ConvertSeverity(EDiagSev(sev));
        NStr::ToUpper(severity);
        out << NStr::NumericToString(m_val_stats[sev].total) << " " << severity << "-level messages exist"
            << "\n\n";

        for_each(m_val_stats[sev].individual.begin(), m_val_stats[sev].individual.end(), [&out](const TErrorStatMap::const_iterator::value_type& it) {
            out << CValidErrItem::ConvertErrGroup(it.first) << "." << CValidErrItem::ConvertErrCode(it.first) << ":\t" << NStr::NumericToString(it.second) << "\n";
        });
        out << "\n"
            << big_separator << "\n";
    }
}

CRef<NDiscrepancy::CDiscrepancyProduct> CTable2AsnValidator::x_PopulateDiscrepancy(CRef<objects::CSeq_submit> submit, objects::CSeq_entry_Handle& entry) const
{
    CRef<NDiscrepancy::CDiscrepancySet> discrepancy = NDiscrepancy::CDiscrepancySet::New(entry.GetScope());

    CSeq_entry_Handle        seh;
    CConstRef<CSerialObject> obj;
    if (submit) {
        // seh = m_discrep_scope->AddSeq_submit(*submit);
        obj = submit;
    } else if (entry) {
        obj = entry.GetCompleteSeq_entry();
    }

    auto names = NDiscrepancy::GetDiscrepancyTests(m_context->m_discrepancy_group);

    CFile nm(m_context->GenerateOutputFilename(eFiles::asn));
    discrepancy->SetLineage(m_context->m_disc_lineage);
    // discrepancy->SetEukaryote(m_context->m_eukaryote);
    auto product = discrepancy->RunTests(names, *obj, nm.GetName());
    return product;
}

void CTable2AsnValidator::CollectDiscrepancies(CRef<objects::CSeq_submit> submit, objects::CSeq_entry_Handle& entry)
{
    if (! m_context->m_run_discrepancy)
        return;

    auto product = x_PopulateDiscrepancy(submit, entry);
    if (m_context->m_split_discrepancy && ! m_context->m_huge_files_mode) {
        x_ReportDiscrepancies(product, m_context->GetOstream(eFiles::dr));
    } else {
        std::lock_guard<std::mutex> g{ m_discrep_mutex };
        if (m_discr_product)
            m_discr_product->Merge(*product);
        else
            m_discr_product = product;
    }
}

void CTable2AsnValidator::x_ReportDiscrepancies(CRef<NDiscrepancy::CDiscrepancyProduct>& product, std::ostream& ostr) const
{
    unsigned short output_flags = NDiscrepancy::CDiscrepancySet::eOutput_Files;
    if (! m_context->m_master_genome_flag.empty()) {
        output_flags |= NDiscrepancy::CDiscrepancySet::eOutput_Fatal;
    }
    product->Summarize();
    product->OutputText(ostr, output_flags);
    product.Reset();
}

void CTable2AsnValidator::ReportDiscrepancies()
{
    if (m_discr_product) {
        x_ReportDiscrepancies(m_discr_product, m_context->GetOstream(eFiles::dr));
    }
}

void CTable2AsnValidator::ReportDiscrepancies(const string& filename)
{
    if (m_discr_product) {
        std::ofstream ostr;
        ostr.exceptions(ios::failbit | ios::badbit);
        ostr.open(filename);
        x_ReportDiscrepancies(m_discr_product, ostr);
    }
}

class CUpdateECNumbers
{
public:
    CUpdateECNumbers(CTable2AsnContext& context) :
        m_Context(context) {}

    void operator()(CSeq_feat& feat);

private:
    CTable2AsnContext& m_Context;
};


void CUpdateECNumbers::operator()(CSeq_feat& feat)
{
    if (! feat.IsSetData() ||
        ! feat.GetData().IsProt() ||
        ! feat.GetData().GetProt().IsSetEc()) {
        return;
    }

    string label;
    auto&  EC = feat.SetData().SetProt().SetEc();
    auto   it = EC.begin();
    while (it != EC.end()) {
        switch (CProt_ref::GetECNumberStatus(*it)) {
        case CProt_ref::eEC_deleted:
            xGetLabel(feat, label);
            m_Context.GetOstream(eFiles::ecn) << label << "\tEC number deleted\t" << *it << "\t\n";
            it = EC.erase(it);
            continue;
            break;
        case CProt_ref::eEC_replaced: {
            xGetLabel(feat, label);
            const string& newvalue = CProt_ref::GetECNumberReplacement(*it);
            bool          is_split = newvalue.find('\t') != string::npos;
            m_Context.GetOstream(eFiles::ecn) << label << (is_split ? "\tEC number split\t" : "\tEC number changed\t")
                                              << *it << '\t' << newvalue << "\n";
            if (is_split) {
                it = EC.erase(it);
                continue;
            }
            *it = newvalue;
        } break;
        case CProt_ref::eEC_unknown:
            xGetLabel(feat, label);
            m_Context.GetOstream(eFiles::ecn) << label << "\tEC number invalid\t" << *it << "\t\n";
            break;
        default:
            break;
        }
        ++it;
    }
    if (EC.empty()) {
        feat.SetData().SetProt().ResetEc();
    }
}


void CTable2AsnValidator::UpdateECNumbers(CSeq_entry& entry)
{
    VisitAllFeatures(entry, CUpdateECNumbers(*m_context));
}

END_NCBI_SCOPE
