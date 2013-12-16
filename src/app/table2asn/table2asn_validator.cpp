#include <ncbi_pch.hpp>

#include <objtools/validator/validator.hpp>
#include <objects/valerr/ValidError.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include "table2asn_validator.hpp"


//CTable2AsnValidator::CTable2AsnValidator()

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

void CTable2AsnValidator::Validate(const CSeq_entry& entry)
{

//    validator::CValidator validator(*CObjectManager::GetInstance());

//   CScope scope(*CObjectManager::GetInstance());
    //scope.AddTopLevelSeqEntry(entry);

//    CConstRef<CValidError> errors = validator.Validate(entry, &scope);
}

END_NCBI_SCOPE

