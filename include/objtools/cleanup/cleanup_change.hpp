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

#include <corelib/ncbiobj.hpp>
#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/**
    All the changes made during cleanup.
    A container of CCleanupChangeItem's.
*/
class NCBI_CLEANUP_EXPORT CCleanupChangeCore
{
public:
    // If you add to this, also edit sm_ChangeDesc's values in the cleanup.cpp file.
    // They must be edited together.
    enum EChanges {
        eNoChange = 0,
        // set when strings are changed.
        eTrimSpaces,
        eCleanDoubleQuotes,
        eAppendToString,
        // set when lists are sorted or uniqued.
        eCleanQualifiers,
        eCleanDbxrefs,
        eCleanCitonFeat,
        eCleanKeywords,
        eCleanSubsource,
        eCleanOrgmod,
        // Set when fields are moved or have content changes
        eRepairBioseqMol, //10
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
        eChange_tRna, //20
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
        eChangeWholeLocation, //30
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
        eRemoveKeyword, // 40
        eAddDescriptor,
        eMoveDescriptor,
        eConvertFeatureToDescriptor,
        eCollapseSet,
        eChangeFeatureLocation,
        eRemoveAnnot,
        eConvertFeature,
        eRemoveComment,
        eAddOrgMod,
        eAddSubSource, // 50
        eChangeBioSourceGenome,
        eChangeBioSourceOrigin,
        eChangeBioSourceOther,
        eChangeSeqId,
        eRemoveEmptyPub,
        eAddQualifier,
        eCleanupDate,
        eChangeBioseqInst,
        eRemoveSeqId,
        eAddProtXref, // 60
        eChangePartial,
        eChangeProtNames,
        eChangeProtActivities,
        eChangeSite,
        eChangePCRPrimers,
        eChangeRNAref,
        eMoveToProtXref,
        eCompressSpaces,
        eStripSerial,
        eRemoveOrgmod, // 70
        eRemoveSubSource,
        eCreateGeneNomenclature,
        eCleanSeqFeatXrefs,
        eCleanUserObjectOrField,
        eLetterCaseChange,
        eChangeBioseqSetClass,
        eUniqueWithoutSort,
        eAddRNAref,
        eChangeGeneRef,
        eCleanDbtag, // 80
        eChangeBiomol,
        eChangeCdregion,
        eCleanECNumber,
        eRemoveException,
        eAddNcbiCleanupObject,
        eCleanDeltaExt,
        eTrimFlankingQuotes,
        eCleanBioseqTitle,
        eDecodeXML,
        eRemoveDupBioSource, // 90
        eCleanOrgref,
        eTrimInternalSemicolons,
        eAddSeqFeatXref,
        eConvertUnstructuredOrgrefModifier,
        eChangeTaxname,
        eMoveGeneOntologyTerm,
        // set when any other change is made.
        eChangeOther,

        eNumberofChangeTypes
    };

    // all constructors are default

    bool    IsChanged(EChanges e) const  { return m_Changes.test(e); }
    void    SetChanged(EChanges e)       { m_Changes.set(e); }
    size_t  ChangeCount() const          { return m_Changes.size(); }
    bool    Empty() const                { return m_Changes.empty(); }
    operator bool() const                { return !m_Changes.empty(); }

    vector<EChanges>    GetAllChanges() const;
    vector<string_view> GetDescriptions() const;

    static string_view  GetDescription(EChanges e);

    CCleanupChangeCore& operator |= (const CCleanupChangeCore& other) {
        m_Changes += other.m_Changes;
        return *this;
    }
    CCleanupChangeCore& operator += (const CCleanupChangeCore& other) { return operator|=(other); }

private:
    using TChangeBits = ct::const_bitset<eNumberofChangeTypes, EChanges>;
    TChangeBits     m_Changes;
};

class NCBI_CLEANUP_EXPORT CCleanupChange : public CObject, public CCleanupChangeCore
{
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* CLEANUP___CLEANUP_CHANGE__HPP */
