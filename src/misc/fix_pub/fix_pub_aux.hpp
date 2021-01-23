#ifndef OBJECTS_GENERAL___CLEANUP_FIX_PUB_AUX__HPP
#define OBJECTS_GENERAL___CLEANUP_FIX_PUB_AUX__HPP

/* $Id$
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
 * Author:  Alexey Dobronadezhdin
 *
 * File Description:
 *   Auxiliary code for fixing up publications. All function declarations are here
 * to make unit testing possible.
 *
 * ===========================================================================
 */
#include <corelib/ncbistd.hpp>

#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Auth_list.hpp>

BEGIN_NCBI_SCOPE

class IMessageListener;

BEGIN_SCOPE(objects)

class CPub;
class CCit_art;

namespace fix_pub
{

enum EFixPubErrorCategory
{
    err_Reference = 1,
    err_Print,
    err_AuthList
};

enum EFixPubReferenceError
{
    err_Reference_MuidNotFound = 1,
    err_Reference_SuccessfulMuidLookup,
    err_Reference_OldInPress,
    err_Reference_No_reference,
    err_Reference_Multiple_ref,
    err_Reference_Multiple_muid,
    err_Reference_MedlineMatchIgnored,
    err_Reference_MuidMissmatch,
    err_Reference_NoConsortAuthors,
    err_Reference_DiffConsortAuthors,
    err_Reference_PmidMissmatch,
    err_Reference_Multiple_pmid,
    err_Reference_FailedToGetPub,
    err_Reference_MedArchMatchIgnored,
    err_Reference_SuccessfulPmidLookup,
    err_Reference_PmidNotFound,
    err_Reference_NoPmidJournalNotInPubMed,
    err_Reference_PmidNotFoundInPress,
    err_Reference_NoPmidJournalNotInPubMedInPress
};


enum EFixPubPrintError
{
    err_Print_Failed = 1
};

enum EFixPubAuthListError
{
    err_AuthList_SignificantDrop = 1,
    err_AuthList_PreserveGB,
    err_AuthList_LowMatch
};


void MedlineToISO(CCit_art& cit_art);
void SplitMedlineEntry(CPub_equiv::Tdata& medlines);
bool IsInpress(const CCit_art& cit_art);
bool NeedToPropagateInJournal(const CCit_art& cit_art);
bool MULooksLikeISSN(const string& str);
bool MUIsJournalIndexed(const string& journal);
void PrintPub(const CCit_art& cit_art, bool found, bool auth, long muid, IMessageListener* err_log);
bool IsFromBook(const CCit_art& art);
bool TenAuthorsCompare(CCit_art& cit_old, CCit_art& cit_new);
size_t ExtractConsortiums(const CAuth_list::C_Names::TStd& names, CAuth_list::C_Names::TStr& extracted);
void GetFirstTenNames(const CAuth_list::C_Names::TStd& names, list<CTempString>& res);
bool TenAuthorsProcess(CCit_art& cit, CCit_art& new_cit, IMessageListener* err_log);
void MergeNonPubmedPubIds(const CCit_art& cit_old, CCit_art& cit_new);
void PropagateInPress(bool inpress, CCit_art& cit_art);
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_GENERAL___CLEANUP_FIX_PUB_AUX__HPP
