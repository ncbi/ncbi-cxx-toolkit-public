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
* Authors:  Justin Foley
*
* File Description:
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/readers/message_listener.hpp>
#include "descr_apply.hpp"

BEGIN_NCBI_SCOPE
using namespace objects;


static void sReportError(
        ILineErrorListener* pEC,
        EDiagSev severity,
        const string& message,
        ILineError::EProblem problemType=ILineError::eProblem_GeneralParsingError)
{
    if (!pEC) {
         // throw an exception
        return;
    }


    AutoPtr<CLineError> pErr(
            CLineError::Create(
                problemType,
                severity,
                "",
                0,
                "", 
                "", 
                "",
                message));

    pEC->PutError(*pErr);
}


static bool s_IsDBLink(const CSeqdesc& descriptor)
{
    return descriptor.IsUser() && descriptor.GetUser().IsDBLink();
}



// Need an error listener
static bool sReportInvalidDescriptor(
        const CBioseq_set::EClass setClass,
        const CSeqdesc& descriptor,
        ILineErrorListener* pEC)
{
    if (descriptor.IsMolinfo()) {
        string msg = "Molinfo is not a valid set descriptor. Skipping.";
        sReportError(pEC, eDiag_Warning, msg);
        return true;
    }


    if (setClass != CBioseq_set::eClass_nuc_prot &&
        (s_IsDBLink(descriptor) ||
        descriptor.IsSource())) {

        string descName = descriptor.IsSource() ?
                          "Biosource " :
                          "DBLink";
        string msg = descName +
                     "descriptors can only reside on a bioseq or a nuc-prot set. Skipping.";
        sReportError(pEC, eDiag_Warning, msg);
        return true;
    }

    return false;
}




static void sApplyDescriptors(const list<CRef<CSeqdesc>>& descriptors, 
        CBioseq_set& bioseqSet, 
        ILineErrorListener* pEC) 
{  
    if (descriptors.empty()) {
        return;
    }

    if (!bioseqSet.IsSetClass() ||
         bioseqSet.GetClass() == CBioseq_set::eClass_not_set) {
        string msg = "Cannot apply descriptors to set of unknown class";
        sReportError(pEC, eDiag_Warning, msg);
        return;
    }

    const auto setClass = bioseqSet.GetClass();
    auto src_it = descriptors.begin();
    auto& dest = bioseqSet.SetDescr().Set();

    while (src_it != descriptors.end()) {
        auto pDescriptor = *src_it;
        if (sReportInvalidDescriptor(setClass, *pDescriptor, pEC)) {
            ++src_it;
        }
        else {
            dest.push_back(*src_it);
        }
    }
}


void g_ApplyDescriptors(const list<CRef<CSeqdesc>>& descriptors,
        CSeq_entry& seqEntry,
        ILineErrorListener* pEC) {
    
    if (seqEntry.IsSet()) {
        sApplyDescriptors(descriptors, seqEntry.SetSet(), pEC);
    }
    else 
    if (seqEntry.IsSeq()) {
        for (auto pDescriptor : descriptors) {
            seqEntry.SetDescr().Set().emplace_back(pDescriptor);
        }
    }
}

END_NCBI_SCOPE


