#include <ncbi_pch.hpp>

#include <objtools/validator/validator.hpp>
#include <objects/valerr/ValidError.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objects/submit/Seq_submit.hpp>

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

} // end anonymous namespace

void CTable2AsnValidator::Cleanup(CSeq_entry& entry)
{
    CCleanup cleanup;
    cleanup.BasicCleanup(entry, CCleanup::eClean_SyncGenCodes);
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

END_NCBI_SCOPE

