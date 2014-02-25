#ifndef OBJTOOLS_EDIT___PROMOTE__HPP
#define OBJTOOLS_EDIT___PROMOTE__HPP

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
* Author: Mati Shomrat, NCBI
*
* File Description:
*   Utility class for fleshing out NCBI data model from features.
*/
#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_data.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// forward declerations
class CBioseq;
class CSeq_annot;
class CScope;
class CBioseq_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;
class CProt_ref;

BEGIN_SCOPE(edit)

NCBI_XOBJEDIT_EXPORT bool SetMolInfoCompleteness (CMolInfo& mi, bool partial5, bool partial3);

class NCBI_XOBJEDIT_EXPORT CPromote
{
public:

    // flags
    enum EFlags
    {
        fPromote_GenProdSet      = 0x1,
        fPromote_IncludeStop     = 0x2,
        fPromote_RemoveTrailingX = 0x4,
        fCopyCdregionTomRNA      = 0x8
    };
    typedef Uint4 TFlags;  // Binary OR of EFleshOutFlags
    static const Uint4 kDefaultFlags = (fPromote_IncludeStop | fCopyCdregionTomRNA);

    // supported feature types
    enum EFeatType
    {
        eFeatType_Cdregion = 0x1,
        eFeatType_Rna      = 0x2,
        eFeatType_Pub      = 0x4
    };
    typedef Uint4 TFeatTypes;  // Binary OR of EFeatType
    static const Uint4 kAllFeats = (eFeatType_Cdregion | eFeatType_Rna | eFeatType_Pub);
    static const Uint4 kXrefFeats = (eFeatType_Cdregion | eFeatType_Rna);

    // constructor
    CPromote(CBioseq_Handle& seq,
             TFlags flags = kDefaultFlags,
             TFeatTypes types = kXrefFeats);

    // Set / Get promote flags
    TFlags GetFlags(void) const;
    void SetFlags(TFlags flags);

    // Set / Get feature types to promote
    TFeatTypes GetFeatTypes(void) const;
    void SetFeatTypes(TFeatTypes types);

    // Promote features from all attached Seq-annots
    void PromoteFeatures(void) const;
    // Promote features from a specific Seq-annot
    void PromoteFeatures(const CSeq_annot_Handle& annot) const;

    // Promote a single coding region feature
    void PromoteCdregion(CSeq_feat_Handle& feat) const;
    // Promote a single mRNA feature
    void PromoteRna(CSeq_feat_Handle& feat) const;
    // Prompte a single publication feature
    void PromotePub(CSeq_feat_Handle& feat) const;

private:

    typedef map<CBioseq_Handle, CConstRef<CSeq_feat> > TRnaMap;
    typedef objects::CSeq_data::E_Choice               TCoding;

    void x_PromoteFeatures(CSeq_annot& annot) const;
    void x_PromoteCdregion(CSeq_feat& feat, TRnaMap* rna_map = 0) const;
    void x_PromoteRna(CSeq_feat& feat) const;
    void x_PromotePub(CSeq_feat& feat) const;

    bool x_DoPromoteCdregion(void) const;
    bool x_DoPromoteRna(void) const;
    bool x_DoPromotePub(void) const;
    
    CBioseq_EditHandle x_MakeNewBioseq(CSeq_id& id, CSeq_inst::TMol mol, 
        const string& data, TCoding code, size_t length) const;
    CBioseq_EditHandle x_MakeNewRna(CSeq_id& id,
        const string& data, TCoding code, size_t length) const;
    CBioseq_EditHandle x_MakeNewProtein(CSeq_id& id, const string& data) const;
    CSeq_id* x_GetProductId(CSeq_feat& feat, const string& qval) const;
    CSeq_id* x_GetTranscriptId(CSeq_feat& feat) const;
    CSeq_id* x_GetProteinId(CSeq_feat& feat) const;
    CSeqdesc* x_MakeMolinfoDesc(const CSeq_feat& feat) const;
    void x_SetSeqFeatProduct(CSeq_feat& feat, const CBioseq_Handle& prod) const;
    void x_AddProtFeature(CBioseq_EditHandle pseq, CProt_ref& prp,
        bool partial_left, bool partial_right) const;
    void x_CopyCdregionToRNA(const CSeq_feat& cds, CBioseq_Handle& mrna,
        TRnaMap* rna_map = 0) const;
    CScope& x_Scope(void) const;

    // data
    CBioseq_Handle  m_Seq;
    TFlags          m_Flags;
    TFeatTypes      m_Types;
};


/////////////////////////////////////////////////////////////////////////////
// static functions

// Promote features from all attached Seq-annots
NCBI_XOBJEDIT_EXPORT
void PromoteFeatures(CBioseq_Handle& seq,
                     CPromote::TFlags flags = CPromote::kDefaultFlags,
                     CPromote::TFeatTypes types = CPromote::kXrefFeats);

// Promote features from a specific Seq-annot
NCBI_XOBJEDIT_EXPORT
void PromoteFeatures(CBioseq_Handle& seq, CSeq_annot_Handle& annot,
                     CPromote::TFlags flags = CPromote::kDefaultFlags,
                     CPromote::TFeatTypes types = CPromote::kXrefFeats);

// Promote a single coding region feature
NCBI_XOBJEDIT_EXPORT
void PromoteCdregion(CBioseq_Handle& seq, CSeq_feat_Handle& feat,
                     bool include_stop = true, bool remove_trailingX = false);

// Promote a single mRNA feature
// NB: The sequence must be part of a gen-prod set
NCBI_XOBJEDIT_EXPORT
void PromoteRna(CBioseq_Handle& seq, CSeq_feat_Handle& feat);

// Prompte a single publication feature
NCBI_XOBJEDIT_EXPORT
void PromotePub(CBioseq_Handle& seq, CSeq_feat_Handle& feat);


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_EDIT___PROMOTE__HPP */
