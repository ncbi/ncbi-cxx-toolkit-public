#include <ncbi_pch.hpp>

#include <objtools/validator/validator.hpp>
#include <objects/valerr/ValidError.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objects/submit/Seq_submit.hpp>

#include "table2asn_validator.hpp"


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

CConstRef<CValidError> CTable2AsnValidator::Validate(CSeq_entry_Handle& handle)
{
    validator::CValidator validator(handle.GetScope().GetObjectManager());

    return validator.Validate(handle);

}

void CTable2AsnValidator::Cleanup(CSeq_entry& entry)
{
    CCleanup cleanup;
    cleanup.BasicCleanup(entry, 0);
}

CConstRef<CValidError> CTable2AsnValidator::Validate(const CSerialObject& object)
{
    CScope scope(*CObjectManager::GetInstance());
    validator::CValidator validator(scope.GetObjectManager());

    if (CSeq_entry::GetTypeInfo() == object.GetThisTypeInfo())
    {
        const CSeq_entry& entry = (const CSeq_entry&)object;
        CSeq_entry_Handle top_se = scope.AddTopLevelSeqEntry(entry);
        return validator.Validate(top_se);
    }
    if (CSeq_submit::GetTypeInfo() == object.GetThisTypeInfo())
    {
        const CSeq_submit& submit = (const CSeq_submit&)object;
        ITERATE(CSeq_submit::C_Data::TEntrys, it, submit.GetData().GetEntrys())
        {
            scope.AddTopLevelSeqEntry(**it);
        }
        return validator.Validate(submit, &scope);
    }
    return CConstRef<CValidError>();
}

void CTable2AsnValidator::ReportErrors(CConstRef<objects::CValidError> errors, CNcbiOstream& out)
{
  out << MSerial_AsnText << *errors;
}

END_NCBI_SCOPE

