#ifndef NEWCLEANUP__HPP
#define NEWCLEANUP__HPP

/*
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
* Author: Robert Smith, Jonathan Kans
*
* File Description:
*   Basic and Extended Cleanup of CSeq_entries.
*
* ===========================================================================
*/

#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objtools/cleanup/cleanup_change.hpp>



BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeqFeatData;
class CSeq_descr;
class CSeqdesc;
class CSeq_loc;
class CGene_ref;
class CProt_ref;
class CRNA_ref;
class CImp_feat;
class CGb_qual;
class CDbtag;
class CUser_field;
class CUser_object;
class CObject_id;
class CGB_block;
class CEMBL_block;
class CPubdesc;
class CPub_equiv;
class CPub;
class CCit_gen;
class CCit_sub;
class CCit_art;
class CCit_book;
class CCit_pat;
class CCit_let;
class CCit_proc;
class CCit_jour;
class CPubMedId;
class CAuth_list;
class CAuthor; 
class CAffil;
class CPerson_id;
class CName_std;
class CBioSource;
class COrg_ref;
class COrgName;
class COrgMod;
class CSubSource;
class CMolInfo;
class CCdregion;
class CDate;
class CDate_std;
class CImprint;
class CSubmit_block;
class CSeq_align;
class CDense_diag;
class CDense_seg;
class CStd_seg;
class CMedline_entry;
class CPub_set;
class CTrna_ext;

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

class CObjectManager;
class CScope;


class CNewCleanup_imp
{
public:

    static const int NCBI_CLEANUP_VERSION;

    // some cleanup functions will return a value telling you what to do
    enum EAction {
        eAction_Nothing = 1,
        eAction_Erase
    };

    // Constructor
    CNewCleanup_imp (CRef<CCleanupChange> changes, Uint4 options = 0);

    // Destructor
    virtual ~CNewCleanup_imp ();

    /// Main methods

    void BasicCleanupSeqEntry (
        CSeq_entry& se
    );

    void BasicCleanupSeqSubmit (
        CSeq_submit& ss
    );

    void BasicCleanupSeqAnnot (
        CSeq_annot& sa
    );

    void ExtendedCleanupSeqEntry (
        CSeq_entry& se
    );

    void ExtendedCleanupSeqSubmit (
        CSeq_submit& ss
    );

    void ExtendedCleanupSeqAnnot (
        CSeq_annot& sa
    );

private:

    // many more methods and variables ...

    // We do not include the usual "x_" prefix for private functions
    // because we want to be able to distinguish between higher-level
    // functions like those just below, and the lower-level
    // functions like those farther below.

    void ChangeMade (CCleanupChange::EChanges e);

    void SetupBC (CSeq_entry& se);

    void SubmitblockBC (CSubmit_block& sb);

    void SeqsetBC (CBioseq_set& bss);

    void SeqIdBC( CSeq_id &seq_id );

    void GBblockBC (CGB_block& gbk);
    void EMBLblockBC (CEMBL_block& emb);

    void BiosourceBC (CBioSource& bsc);
    void OrgrefBC (COrg_ref& org);
    void OrgnameBC (COrgName& onm);
    void OrgmodBC (COrgMod& omd);
    void SubsourceBC (CSubSource& sbs);

    void DbtagBC (CDbtag& dbt);

    void PubdescBC (CPubdesc& pub);
    void PubEquivBC (CPub_equiv& pub_equiv);
    void PubBC(CPub& pub, bool fix_initials);
    void CitGenBC(CCit_gen& cg, bool fix_initials);
    void CitSubBC(CCit_sub& cs, bool fix_initials);
    void CitArtBC(CCit_art& ca, bool fix_initials);
    void CitBookBC(CCit_book& cb, bool fix_initials);
    void CitPatBC(CCit_pat& cp, bool fix_initials);
    void CitLetBC(CCit_let& cl, bool fix_initials);
    void CitProcBC(CCit_proc& cb, bool fix_initials);
    void CitJourBC(CCit_jour &j, bool fix_initials);
    void MedlineEntryBC(CMedline_entry& ml, bool fix_initials);
    void AuthListBC( CAuth_list& al, bool fix_initials );
    void AffilBC( CAffil& af );
    void ImprintBC( CImprint& imp );
    void PubSetBC( CPub_set &pub_set );

    void ImpFeatBC( CImp_feat& imf, CSeq_feat& sf );

    void SiteFeatBC( CSeqFeatData::ESite &site, CSeq_feat& sf );

    void SeqLocBC( CSeq_loc &loc );
    void ConvertSeqLocWholeToInt( CSeq_loc &loc );

    void SeqfeatBC (CSeq_feat& sf);

    void GBQualBC (CGb_qual& gbq);
    void Except_textBC (string& except_text);

    void GenerefBC (CGene_ref& gr);
    void ProtrefBC (CProt_ref& pr);
    void RnarefBC (CRNA_ref& rr);

    void GeneFeatBC (CGene_ref& gr, CSeq_feat& sf);
    void ProtFeatfBC (CProt_ref& pr, CSeq_feat& sf);
    void RnaFeatBC (CRNA_ref& rr, CSeq_feat& sf);
    void CdregionFeatBC (CCdregion& cds, CSeq_feat& seqfeat);

    void DeltaExtBC( CDelta_ext & delta_ext, CSeq_inst &seq_inst );

    void UserObjectBC( CUser_object &user_object );

    // void XxxxxxBC (Cxxxxx& xxx);

    // Prohibit copy constructor & assignment operator
    CNewCleanup_imp (const CNewCleanup_imp&);
    CNewCleanup_imp& operator= (const CNewCleanup_imp&);

private:

    enum EGBQualOpt {
        eGBQualOpt_normal,
        eGBQualOpt_CDSMode
    };

    // Gb_qual cleanup.
    EAction GBQualSeqFeatBC(CGb_qual& gbq, CSeq_feat& seqfeat);

    void x_AddNcbiCleanupObject( CSeq_entry &seq_entry );

    void x_CleanupConsSplice(CGb_qual& gbq);
    bool x_CleanupRptUnit(CGb_qual& gbq);
    void x_ChangeTransposonToMobileElement(CGb_qual& gbq);
    void x_ChangeInsertionSeqToMobileElement(CGb_qual& gbq);
    void x_ExpandCombinedQuals(CSeq_feat::TQual& quals);
    EAction x_GeneGBQualBC( CGene_ref& gene, const CGb_qual& gb_qual );
    EAction x_SeqFeatCDSGBQualBC(CSeq_feat& feat, CCdregion& cds, const CGb_qual& gb_qual);
    EAction x_SeqFeatRnaGBQualBC(CSeq_feat& feat, CRNA_ref& rna, CGb_qual& gb_qual);
    EAction x_ParseCodeBreak(const CSeq_feat& feat, CCdregion& cds, const string& str);
    EAction x_ProtGBQualBC(CProt_ref& prot, const CGb_qual& gb_qual, EGBQualOpt opt );

    // publication-related cleanup
    void x_FlattenPubEquiv(CPub_equiv& pe);

    // Date-related
    void x_DateStdBC( CDate_std& date );

    // author-related
    void x_AuthorBC  ( CAuthor& au, bool fix_initials );
    void x_PersonIdBC( CPerson_id& pid, bool fix_initials );
    void x_NameStdBC ( CName_std& name, bool fix_initials );
    void x_ExtractSuffixFromInitials(CName_std& name);
    void x_FixEtAl(CName_std& name);
    void x_FixSuffix(CName_std& name);
    void x_FixInitials(CName_std& name);

    void x_AddReplaceQual(CSeq_feat& feat, const string& str);

    void x_SeqIntervalBC( CSeq_interval & seq_interval );

    void x_SplitDbtag( CDbtag &dbt, vector< CRef< CDbtag > > & out_new_dbtags );

    void x_SeqFeatTRNABC( CSeq_feat& feat, CTrna_ext & tRNA );

    // modernize PCR Primer
    void x_ModernizePCRPrimers( CBioSource &biosrc );

    void x_FixUnsetMolFromBiomol( CMolInfo& molinfo, CBioseq &bioseq );

    string x_ExtractSatelliteFromComment( string &comment );

    void x_SetFrameFromLoc( CCdregion &cdregion, const CSeq_loc &location );

    void x_CleanupECNumber( string &ec_num );

    void x_CleanupAndRepairInference( string &inference );

    void x_MendSatelliteQualifier( string &val );

    // e.g. if ends with ",..", turn into "..."
    void x_FixUpEllipsis( string &str );

    void x_MoveCdregionXrefsToProt (CCdregion& cds, CSeq_feat& seqfeat);
    bool x_InGpsGenomic( const CSeq_feat& seqfeat );

    void x_AddNonCopiedQual( 
        vector< CRef< CGb_qual > > &out_quals, 
        const char *qual, 
        const char *val );

    void x_GBQualToOrgRef( COrg_ref &org, CSeq_feat &seqfeat );
    void x_MoveSeqdescOrgToSourceOrg( COrg_ref &org, CSeqdesc &seqdesc );
    void x_MoveSeqfeatOrgToSourceOrg( COrg_ref &org, CSeq_feat &seqfeat );

    void x_CleanupStringMarkChanged( std::string &str );
    void x_ConvertDoubleQuotesMarkChanged( std::string &str );

    void x_SortUniqSeqFeat( CSeq_feat& seq_feat );
    void x_SortUniqOrgRef( COrg_ref& org );
    void x_SortUniqBiosource( CBioSource& biosrc );


protected:

    CRef<CCleanupChange>  m_Changes;
    Uint4                 m_Options;
    CRef<CObjectManager>  m_Objmgr;
    CRef<CScope>          m_Scope;
    bool                  m_IsEmblOrDdbj;
    bool                  m_StripSerial;
    bool                  m_IsGpipe;

    friend class CAutogeneratedCleanup;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* NEWCLEANUP__HPP */