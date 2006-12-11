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
 * Author:  Robert Smith
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/submit/Seq_submit.hpp>


#include <objmgr/util/sequence.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

enum EChangeType {
    eChange_UNKNOWN
};

// *********************** CCleanup implementation **********************


CCleanup::CCleanup() : m_Scope(NULL)
{
}


CCleanup::~CCleanup(void)
{
}


void CCleanup::SetScope(CRef<CScope> scope)
{
    m_Scope = scope;
}


static
CRef<CCleanupChange> makeCleanupChange(Uint4 options)
{
    CRef<CCleanupChange> changes;
    if (! (options  &  CCleanup::eClean_NoReporting)) {
        changes.Reset(new CCleanupChange);        
    }
    return changes;
}

CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_entry& se, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(se);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_submit& ss, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(ss);
    return changes;
}


/// Cleanup a Bioseq. 
CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq& bs, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(bs);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_set& bss, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(bss);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_annot& sa, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(sa);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_feat& sf, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(sf);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_entry_Handle& seh, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(seh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_Handle& bsh,    Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(bsh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_set_Handle& bssh, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(bssh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_annot_Handle& sah, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(sah);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_feat_Handle& sfh,  Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CCleanup_imp clean_i(changes, m_Scope, options);
    clean_i.BasicCleanup(sfh);
    return changes;
}





// *********************** Extended Cleanup implementation ********************
CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_entry& se)
{
    CRef<CCleanupChange> changes(makeCleanupChange(0));
    CCleanup_imp clean_i(changes, m_Scope, 0);
    clean_i.ExtendedCleanup(m_Scope->GetSeq_entryHandle(se));
    
    return changes;
}


CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_submit& ss)
{
    CRef<CCleanupChange> changes(makeCleanupChange(0));
    CCleanup_imp clean_i(changes, m_Scope, 0);
    clean_i.ExtendedCleanup(ss);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_annot& sa)
{
    CRef<CCleanupChange> changes(makeCleanupChange(0));
    CCleanup_imp clean_i(changes, m_Scope, 0);
    clean_i.ExtendedCleanup(m_Scope->GetSeq_annotHandle(sa));
    return changes;
}


// *********************** CCleanupChange implementation **********************


CCleanupChange::CCleanupChange()
{
}


size_t CCleanupChange::ChangeCount() const
{
    return m_Changes.count();
}


bool CCleanupChange::IsChanged(CCleanupChange::EChanges e) const
{
    return m_Changes.test(e);
}


void CCleanupChange::SetChanged(CCleanupChange::EChanges e)
{
    m_Changes.set(e);
}


vector<CCleanupChange::EChanges> CCleanupChange::GetAllChanges() const
{
    vector<EChanges>  result;
    for (size_t i = eNoChange + 1; i < m_Changes.size(); ++i) {
        if (m_Changes.test(i)) {
            result.push_back( (EChanges) i);
        }
    }
    return result;
}


vector<string> CCleanupChange::GetAllDescriptions() const
{
    vector<string>  result;
    for (size_t i = eNoChange + 1; i < m_Changes.size(); ++i) {
        if (m_Changes.test(i)) {
            result.push_back( GetDescription((EChanges) i) );
        }
    }
    return result;
}


string CCleanupChange::GetDescription(EChanges e)
{
    if (e <= eNoChange  ||  e >= eNumberofChangeTypes) {
        return sm_ChangeDesc[eNoChange];
    }
    return sm_ChangeDesc[e];
}


const char* CCleanupChange::sm_ChangeDesc[] = {
    "Invalid Change Code",
    // set when strings are changed.
    "Trim Spaces",
    "Clean Double Quotes",
    // set when lists are sorted or uniqued.
    "Clean Qualifiers List",
    "Clean Dbxrefs List",
    "Clean CitonFeat List",
    "Clean Keywords List",
    "Clean Subsource List",
    "Clean Orgmod List",
    // Set when fields are moved or have content changes
    "Repair BioseqMol",
    "Change Feature Key", //10
    "Normalize Authors",
    "Change Publication",
    "Change Qualifiers",
    "Change Dbxrefs",
    "Change Keywords",
    "Change Subsource",
    "Change Orgmod",
    "Change Exception",
    "Change Comment",
    // Set when fields are rescued
    "Change tRna", //20
    "Change rRna",
    "Change ITS",
    "Change Anticodon",
    "Change Code Break",
    "Change Genetic Code",
    "Copy GeneXref",
    "Copy ProtXref",
    // set when locations are repaired
    "Change Seqloc",
    "Change Strand",
    "Change WholeLocation", //30
    // set when MolInfo descriptors are affected
    "Change MolInfo Descriptor",
    // set when prot-xref is removed
    "Remove ProtXref",
    // set when gene-xref is removed
    "Remove GeneXref",
    // set when protein feature is added
    "Add Protein Feature",
    // set when feature is removed
    "Remove Feature",
    // set when feature is moved
    "Move Feature",
    // set when qualifier is removed
    "Remove Qualifier",
    // set when Gene Xref is created
    "Add GeneXref",
    // set when descriptor is removed
    "Remove Descriptor",
    "Remove Keyword", //40
    "Add Descriptor",
    "Convert Feature to Descriptor", 
    "Collapse Set",
    "Change Feature Location",
    "Remove Annotation",
    "Convert Feature",
    "Remove Comment",
    "Add BioSource OrgMod",
    "Add BioSource SubSource",
    "Change BioSource Genome", //50
    "Change BioSource Origin", 
    "Change BioSource Other",
    "Change SeqId", 
    // set when any other change is made.
    "Change Other", 
    "Invalid Change Code"
};

END_SCOPE(objects)
END_NCBI_SCOPE

 
/*
* ===========================================================================
*
* $Log$
* Revision 1.19  2006/12/11 17:14:43  bollin
* Made changes to ExtendedCleanup per the meetings and new document describing
* the expected behavior for BioSource features and descriptors.  The behavior
* for BioSource descriptors on GenBank, WGS, Mut, Pop, Phy, and Eco sets has
* not been implemented yet because it has not yet been agreed upon.
*
* Revision 1.18  2006/10/24 12:13:02  bollin
* Added more flags for ExtendedCleanup.
*
* Revision 1.17  2006/10/11 14:46:05  bollin
* Record more changes made by ExtendedCleanup.
*
* Revision 1.16  2006/10/10 13:49:23  bollin
* record changes from ExtendedCleanup
*
* Revision 1.15  2006/07/26 19:37:04  rsmith
* add cleanup w/Handles
*
* Revision 1.14  2006/07/03 12:34:38  bollin
* use handles instead of operating directly on the data for extended cleanup
*
* Revision 1.13  2006/06/23 17:27:43  bollin
* fixed ExtendedCleanup functions to use new constructor for CCleanupChange
*
* Revision 1.12  2006/06/23 17:10:37  rsmith
* totally rewrite CCleanupChange class.
*
* Revision 1.11  2006/06/22 13:28:29  bollin
* added step to remove empty features to ExtendedCleanup
*
* Revision 1.10  2006/06/20 19:43:39  bollin
* added MolInfoUpdate to ExtendedCleanup
*
* Revision 1.9  2006/06/19 13:00:15  bollin
* empty ExtendedCleanup function
*
* Revision 1.8  2006/05/17 17:39:36  bollin
* added parsing and cleanup of anticodon qualifiers on tRNA features and
* transl_except qualifiers on coding region features
*
* Revision 1.7  2006/03/20 14:22:15  rsmith
* add cleanup for CSeq_submit
*
* Revision 1.6  2006/03/14 20:21:50  rsmith
* Move BasicCleanup functionality from objects to objtools/cleanup
*
* Revision 1.5  2005/10/18 22:46:34  vakatov
* Static class members sm_Verbose[] and sm_Terse[] to become static
* in-function s_Verbose[] and s_Terse[], respectively (didn't work on
* MSVC otherwise; didn't have time to try EXPORT'ing them)
*
* Revision 1.4  2005/10/18 14:40:50  rsmith
* define sm_Terse and sm_Verbose
*
* Revision 1.3  2005/10/18 14:22:25  rsmith
* BasicCleanup entry points for more classes.
*
* Revision 1.2  2005/10/18 13:34:15  rsmith
* add change reporting classes
*
* Revision 1.1  2005/10/17 12:22:12  rsmith
* initial checkin
*
*
* ===========================================================================
*/

