#ifndef UNIT_TEST_VALIDATOR__HPP
#define UNIT_TEST_VALIDATOR__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin, NCBI
 *
 */

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objects/valerr/ValidErrItem.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CExpectedError
{
public:
    CExpectedError(string accession, EDiagSev severity, string err_code,
                   string err_msg);
    ~CExpectedError();

    void Test(const CValidErrItem& err_item);

    void SetAccession (string accession) { m_Accession = accession; }
    void SetSeverity (EDiagSev severity) { m_Severity = severity; }
    void SetErrCode (string err_code) { m_ErrCode = err_code; }
    void SetErrMsg (string err_msg) { m_ErrMsg = err_msg; }

    const string& GetErrMsg(void) { return m_ErrMsg; }

private:
    string m_Accession;
    EDiagSev m_Severity;
    string m_ErrCode;
    string m_ErrMsg;
};


#define CLEAR_ERRORS \
    while (expected_errors.size() > 0) { \
        if (expected_errors[expected_errors.size() - 1] != NULL) { \
            delete expected_errors[expected_errors.size() - 1]; \
        } \
        expected_errors.pop_back(); \
    }

#define STANDARD_SETUP \
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance(); \
    CScope scope(*objmgr); \
    scope.AddDefaults(); \
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry); \
    CConstRef<CValidError> eval; \
    CValidator validator(*objmgr); \
    unsigned int options = CValidator::eVal_need_isojta \
                          | CValidator::eVal_far_fetch_mrna_products \
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version \
	                      | CValidator::eVal_use_entrez; \
    vector< CExpectedError *> expected_errors;

#define STANDARD_SETUP_NAME(entry_name) \
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance(); \
    CScope scope(*objmgr); \
    scope.AddDefaults(); \
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry_name); \
    CConstRef<CValidError> eval; \
    CValidator validator(*objmgr); \
    unsigned int options = CValidator::eVal_need_isojta \
                          | CValidator::eVal_far_fetch_mrna_products \
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version \
	                      | CValidator::eVal_use_entrez; \
    vector< CExpectedError *> expected_errors;

#define STANDARD_SETUP_WITH_DATABASE \
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance(); \
    CGBDataLoader::RegisterInObjectManager(*objmgr); \
    CScope scope(*objmgr); \
    scope.AddDefaults(); \
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry); \
    CConstRef<CValidError> eval; \
    CValidator validator(*objmgr); \
    unsigned int options = CValidator::eVal_need_isojta \
                          | CValidator::eVal_far_fetch_mrna_products \
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version \
	                      | CValidator::eVal_use_entrez; \
    vector< CExpectedError *> expected_errors;

#define STANDARD_SETUP_NO_DATABASE \
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance(); \
    try { \
        objmgr->RevokeDataLoader("GBLOADER"); \
    } catch (CException ) { \
    } \
    CScope scope(*objmgr); \
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry); \
    CConstRef<CValidError> eval; \
    CValidator validator(*objmgr); \
    unsigned int options = CValidator::eVal_need_isojta \
                          | CValidator::eVal_far_fetch_mrna_products \
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version \
	                      | CValidator::eVal_use_entrez; \
    vector< CExpectedError *> expected_errors;

void CheckErrors(const CValidError& eval,
                 vector< CExpectedError* >& expected_errors);
				 
END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* UNIT_TEST_VALIDATOR__HPP */
