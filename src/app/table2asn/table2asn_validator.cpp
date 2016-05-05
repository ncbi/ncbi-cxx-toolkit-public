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

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace {

static const string str_sev[] = {
    "INFO", "WARNING", "ERROR", "REJECT", "FATAL", "MAX"
};

static const string s_none = "NONE";

static const string& s_GetSeverityLabel (EDiagSev sev)
{
    if (sev < 0 || sev > eDiagSevMax) {
        return s_none;
    }

    return str_sev[sev];
}

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

void CTable2AsnValidator::Cleanup(CSeq_entry_Handle h_entry, const string& flags)
{
    CRef<CSeq_entry> entry((CSeq_entry*)(h_entry.GetEditHandle().GetCompleteSeq_entry().GetPointer()));

    CCleanup cleanup;
    if (flags.find('e') != string::npos)
    {
        cleanup.ExtendedCleanup(*entry, CCleanup::eClean_SyncGenCodes | CCleanup::eClean_NoNcbiUserObjects);
    }
    else
    {
        cleanup.BasicCleanup(*entry, CCleanup::eClean_SyncGenCodes | CCleanup::eClean_NoNcbiUserObjects);
        if (flags.find('U') != string::npos)
          cleanup.RemoveUnnecessaryGeneXrefs(h_entry); //remove unnec gen xref included in extended cleanup
    }
}

CConstRef<CValidError> CTable2AsnValidator::Validate(const CSerialObject& object, Uint4 opts)
{
    CScope scope(*CObjectManager::GetInstance());
    validator::CValidator validator(scope.GetObjectManager());

    if (CSeq_entry::GetTypeInfo() == object.GetThisTypeInfo())
    {
        const CSeq_entry& entry = (const CSeq_entry&)object;
        CSeq_entry_Handle top_se = scope.AddTopLevelSeqEntry(entry);
        return validator.Validate(top_se, opts);
    }
    if (CSeq_submit::GetTypeInfo() == object.GetThisTypeInfo())
    {
        const CSeq_submit& submit = (const CSeq_submit&)object;
        ITERATE(CSeq_submit::C_Data::TEntrys, it, submit.GetData().GetEntrys())
        {
            scope.AddTopLevelSeqEntry(**it);
        }
        return validator.Validate(submit, &scope, opts);
    }
    return CConstRef<CValidError>();
}

void CTable2AsnValidator::ReportErrors(CConstRef<CValidError> errors, CNcbiOstream& out)
{
    ITERATE(CValidError::TErrs, it, errors->GetErrs())
    {
        const CValidErrItem& item = **it;
        out << s_GetSeverityLabel(item.GetSeverity())
               << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() <<"] "
               << item.GetMsg() << " " << item.GetObjDesc() << endl;
    }
    //out << MSerial_AsnText << *errors;
}

void CTable2AsnValidator::UpdateECNumbers(objects::CSeq_entry_Handle seh, const string& fname, auto_ptr<CNcbiOfstream>& ostream)
{
    string label;
    for (CFeat_CI feat_it(seh); feat_it; ++feat_it)
    {
        const CSeq_feat& feat = feat_it->GetOriginalFeature();

        label.clear();

        if (feat.IsSetData() && feat.GetData().IsProt())
        {
            CProt_ref::TEc& EC = (CProt_ref::TEc&)feat.GetData().GetProt().GetEc();
            NON_CONST_ITERATE(CProt_ref::TEc, val, EC)
            {
                switch (CProt_ref::GetECNumberStatus(*val))
                {
                case CProt_ref::eEC_deleted:
                    xGetLabel(feat, label);
                    InitOstream(ostream, fname) << label << "\tEC number deleted\t" << *val << '\t' << endl;
                    break;
                case CProt_ref::eEC_replaced:
                {
                    xGetLabel(feat, label);
                    const string& newvalue = CProt_ref::GetECNumberReplacement(*val);
                    bool is_split = newvalue.find('\t') != string::npos;
                    InitOstream(ostream, fname) << label <<
                        (is_split ? "\tEC number split\t" : "\tEC number changed\t")
                        << *val << '\t' << newvalue << endl;
                    if (!is_split)
                        *val = newvalue;
                }
                break;
                case CProt_ref::eEC_unknown:
                    xGetLabel(feat, label);
                    InitOstream(ostream, fname) << label << "\tEC number invalid\t" << *val << '\t' << endl;
                    break;
                default:
                    break;
                }
            }
        }
    }
}

END_NCBI_SCOPE

