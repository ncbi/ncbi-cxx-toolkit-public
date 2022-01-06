/* ftamed.c
 *
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
 * File Name:  ftamed.c
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      MedArch lookup and post-processing utilities.
 * Mostly the same as medutil.c written by James Ostell.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/mla/mla_client.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objtools/edit/pub_fix.hpp>

#include "index.h"

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "utilref.h"
#include "ftamed.h"
#include "ref.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "ftamed.cpp"

BEGIN_NCBI_SCOPE

// error definitions from C-toolkit (mdrcherr.h)
#define ERR_REFERENCE  1,0
#define ERR_REFERENCE_MuidNotFound  1,1
#define ERR_REFERENCE_SuccessfulMuidLookup  1,2
#define ERR_REFERENCE_OldInPress  1,3
#define ERR_REFERENCE_No_reference  1,4
#define ERR_REFERENCE_Multiple_ref  1,5
#define ERR_REFERENCE_Multiple_muid  1,6
#define ERR_REFERENCE_MedlineMatchIgnored  1,7
#define ERR_REFERENCE_MuidMissmatch  1,8
#define ERR_REFERENCE_NoConsortAuthors  1,9
#define ERR_REFERENCE_DiffConsortAuthors  1,10
#define ERR_REFERENCE_PmidMissmatch  1,11
#define ERR_REFERENCE_Multiple_pmid  1,12
#define ERR_REFERENCE_FailedToGetPub  1,13
#define ERR_REFERENCE_MedArchMatchIgnored  1,14
#define ERR_REFERENCE_SuccessfulPmidLookup  1,15
#define ERR_REFERENCE_PmidNotFound  1,16
#define ERR_REFERENCE_NoPmidJournalNotInPubMed  1,17
#define ERR_REFERENCE_PmidNotFoundInPress  1,18
#define ERR_REFERENCE_NoPmidJournalNotInPubMedInPress  1,19
#define ERR_PRINT  2,0
#define ERR_PRINT_Failed  2,1


using namespace ncbi;
using namespace objects;

static char *this_module = (char *) "medarch";
#ifdef THIS_MODULE
#undef THIS_MODULE
#endif

#define THIS_MODULE this_module

static CMLAClient mlaclient;

USING_SCOPE(objects);

IMessageListener::EPostResult 
CPubFixMessageListener::PostMessage(const IMessage& message) 
{
    static const map<EDiagSev, ErrSev> sSeverityMap
               = {{eDiag_Trace, SEV_NONE},
                  {eDiag_Info, SEV_INFO},
                  {eDiag_Warning, SEV_WARNING},
                  {eDiag_Error, SEV_ERROR},
                  {eDiag_Critical, SEV_REJECT},
                  {eDiag_Fatal, SEV_FATAL}};

    ErrPostEx(sSeverityMap.at(message.GetSeverity()),
            message.GetCode(),
            message.GetSubCode(),
            message.GetText().c_str());

    return eHandled;
}

/**********************************************************/
bool MedArchInit()
{
    CMla_back i;

    try
    {
        mlaclient.AskInit(&i);
    }
    catch(exception &)
    {
        return false;
    }

    return i.IsInit();
}

/**********************************************************/
void MedArchFini()
{
    try
    {
        mlaclient.AskFini();
    }
    catch(exception &)
    {
    }
}

/**********************************************************/
CRef<CCit_art> FetchPubPmId(Int4 pmid)
{
    return edit::CPubFix::FetchPubPmId(ENTREZ_ID_FROM(Int4, pmid));
}

END_NCBI_SCOPE
