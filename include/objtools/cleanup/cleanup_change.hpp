#ifndef CLEANUP___CLEANUP_CHANGE__HPP
#define CLEANUP___CLEANUP_CHANGE__HPP

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
* Author:  Robert Smith
*
* File Description:
*   Basic Cleanup of CSeq_entries.
*   .......
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/mswin_export.h>
#include <bitset>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/**
    All the changes made during cleanup.
    A container of CCleanupChangeItem's.
*/
class NCBI_CLEANUP_EXPORT CCleanupChange : public CObject
{
public:
    enum EChanges {
        eNoChange = 0,
        // set when strings are changed.
        eTrimSpaces,
        eCleanDoubleQuotes,
        // set when lists are sorted or uniqued.
        eCleanQualifiers,
        eCleanDbxrefs,
        eCleanCitonFeat,
        eCleanKeywords,
        eCleanSubsource,
        eCleanOrgmod,
        // Set when fields are moved or have content changes
        eRepairBioseqMol,
        eChangeFeatureKey,
        eNormalizeAuthors,
        eChangePublication,
        eChangeQualifiers,
        eChangeDbxrefs,
        eChangeKeywords,
        eChangeSubsource,
        eChangeOrgmod,
        eChangeException,
        eChangeComment,
        // Set when fields are rescued
        eChange_tRna,
        eChange_rRna,
        eChangeITS,
        eChangeAnticodon,
        eChangeCodeBreak,
        eChangeGeneticCode,
        eCopyGeneXref,
        eCopyProtXref,
        // set when locations are repaired
        eChangeSeqloc,
        eChangeStrand,
        eChangeWholeLocation,
        // set when MolInfo descriptors are affected
        eChangeMolInfo,
        // set when prot-xref is removed
        eRemoveProtXref,
        // set when gene-xref is removed
        eRemoveGeneXref,
        // set when protein feature is added
        eAddProtFeat,
        // set when feature is removed
        eRemoveFeat,
        // set when feature is moved,
        eMoveFeat,
        // set when qualifier is removed
        eRemoveQualifier,
        // set when gene xref is created
        eCreateGeneXref,
        // set when descriptor is removed
        eRemoveDescriptor,
        // set when keyword is removed
        eRemoveKeyword,
        eAddDescriptor,
        eConvertFeatureToDescriptor,
        eCollapseSet,
        eChangeFeatureLocation,
        eRemoveAnnot,
        eConvertFeature,
        eRemoveComment,
        eAddOrgMod,
        eAddSubSource,
        eChangeBioSourceGenome,
        eChangeBioSourceOrigin,
        eChangeBioSourceOther,
        // set when any other change is made.
        eChangeOther,
        
        eNumberofChangeTypes
    };
    
    // constructors
    CCleanupChange();
    
    bool    IsChanged(EChanges e) const;
    void    SetChanged(EChanges e);
    
    vector<EChanges>    GetAllChanges() const;
    vector<string>      GetAllDescriptions() const;
    
    size_t  ChangeCount() const;
    
    static string  GetDescription(EChanges e);
private:
    static const char* sm_ChangeDesc[];
    
    typedef bitset<eNumberofChangeTypes> TChangeBits;
    TChangeBits     m_Changes;
    
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2006/10/24 12:12:19  bollin
 * Added more change flags for ExtendedCleanup
 *
 * Revision 1.4  2006/10/11 14:46:39  bollin
 * added more flags for ExtendedCleanup
 *
 * Revision 1.3  2006/10/10 13:45:51  bollin
 * added change flags for Extended Cleanup
 *
 * Revision 1.2  2006/07/26 19:35:27  rsmith
 * add new change types.
 *
 * Revision 1.1  2006/06/23 17:09:20  rsmith
 * moved from cleanup.hpp
 *
 */ 

#endif  /* CLEANUP___CLEANUP_CHANGE__HPP */
