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
 *`
 * Author:  Colleen Bollin, Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   For validating individual features
 *   .......
 *
 */

#ifndef VALIDATOR___GO_TERM_VALIDATION_AND_CLEANUP__HPP
#define VALIDATOR___GO_TERM_VALIDATION_AND_CLEANUP__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/valerr/ValidErrItem.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



BEGIN_SCOPE(validator)

bool IsGeneOntology(const CUser_object& user_object);

typedef pair<EErrType, string> TGoTermError;
vector<TGoTermError> NCBI_VALIDATOR_EXPORT GetGoTermErrors(const CSeq_feat& feat);
bool NCBI_VALIDATOR_EXPORT RemoveDuplicateGoTerms(CSeq_feat& feat);

void NCBI_VALIDATOR_EXPORT SetGoTermId(CUser_field& field, const string& val);
void NCBI_VALIDATOR_EXPORT SetGoTermText(CUser_field& field, const string& val);
void NCBI_VALIDATOR_EXPORT SetGoTermPMID(CUser_field& field, int pmid);
void NCBI_VALIDATOR_EXPORT AddGoTermEvidence(CUser_field& field, const string& val);
void NCBI_VALIDATOR_EXPORT ClearGoTermEvidence(CUser_field& field);
void NCBI_VALIDATOR_EXPORT ClearGoTermPMID(CUser_field& field);

void NCBI_VALIDATOR_EXPORT AddProcessGoTerm(CSeq_feat& feat, CRef<CUser_field> field);
void NCBI_VALIDATOR_EXPORT AddComponentGoTerm(CSeq_feat& feat, CRef<CUser_field> field);
void NCBI_VALIDATOR_EXPORT AddFunctionGoTerm(CSeq_feat& feat, CRef<CUser_field> field);

size_t NCBI_VALIDATOR_EXPORT CountProcessGoTerms(const CSeq_feat& feat);
size_t NCBI_VALIDATOR_EXPORT CountComponentGoTerms(const CSeq_feat& feat);
size_t NCBI_VALIDATOR_EXPORT CountFunctionGoTerms(const CSeq_feat& feat);


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___GO_TERM_VALIDATION_AND_CLEANUP__HPP */
