// new_cleanup.hpp

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
#include <ncbi_pch.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objtools/cleanup/cleanup_change.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeq_submit;

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

class CCleanupChange;

class NCBI_CLEANUP_EXPORT CNewCleanup : public CObject 
{
public:

    /// Constructor
    CNewCleanup();

    /// Destructor
    ~CNewCleanup();

   /// User-settable flags for tuning behavior
    enum EValidOptions {
        eClean_NoReporting = 0x1,
        eClean_GpipeMode = 0x2
    };

    /// Main methods

    CConstRef<CCleanupChange> BasicCleanup (
        CSeq_entry& se,
        Uint4 options = 0
    );

    CConstRef<CCleanupChange> BasicCleanup (
        CSeq_submit& ss,
        Uint4 options = 0
    );

    CConstRef<CCleanupChange> BasicCleanup (
        CSeq_annot& sa,
        Uint4 options = 0
    );

    CConstRef<CCleanupChange> ExtendedCleanup (
        CSeq_entry& se,
        Uint4 options = 0
    );

    CConstRef<CCleanupChange> ExtendedCleanup (
        CSeq_submit& ss,
        Uint4 options = 0
    );

    CConstRef<CCleanupChange> ExtendedCleanup (
        CSeq_annot& sa,
        Uint4 options = 0
    );


private:
    // Prohibit copy constructor & assignment operator
    CNewCleanup (const CNewCleanup&);
    CNewCleanup& operator= (const CNewCleanup&);
};



END_SCOPE(objects)
END_NCBI_SCOPE

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
*   Implementation of Basic and Extended Cleanup of CSeq_entries.
*
* ===========================================================================
*/

// #include <new_cleanup.hpp>


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

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

class CCleanupChange;
class CObjectManager;
class CScope;

class CNewCleanup_imp
{
public:

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

    void BasicCleanup (
        CSeq_entry& se
    );

    void BasicCleanup (
        CSeq_submit& ss
    );

    void BasicCleanup (
        CSeq_annot& sa
    );

    void ExtendedCleanup (
        CSeq_entry& se
    );

    void ExtendedCleanup (
        CSeq_submit& ss
    );

    void ExtendedCleanup (
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

    void SeqentryBC (CSeq_entry& se);
    void SeqsetBC (CBioseq_set& bss);
    void BioseqBC (CBioseq& bs);

    void SeqdescBC (CSeqdesc& sd);

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

    void RegionFeatBC( string &region, CSeq_feat& sf );

    void SiteFeatBC( CSeqFeatData::ESite &site, CSeq_feat& sf );

    void SeqLocBC( CSeq_loc &loc );

    void UserobjBC (CUser_object& usr);
    void ObjectIdBC( CObject_id &object_id );
    void UserFieldBC( CUser_field & user_field );

    void SeqannotBC (CSeq_annot& sa);
    void SeqfeatBC (CSeq_feat& sf);
    void SeqFeatSeqfeatDataBC (CSeq_feat& sf, CSeqFeatData& sfd);

    void GBQualBC (CGb_qual& gbq);
    void Except_textBC (string& except_text);

    void GenerefBC (CGene_ref& gr);
    void ProtrefBC (CProt_ref& pr);
    void RnarefBC (CRNA_ref& rr);

    void GeneFeatBC (CGene_ref& gr, CSeq_feat& sf);
    void ProtFeatfBC (CProt_ref& pr, CSeq_feat& sf);
    void RnaFeatBC (CRNA_ref& rr, CSeq_feat& sf);

    void PubFeatBC (CPubdesc& pub, CSeq_feat& sf);
    void UserFeatBC (CUser_object& usr, CSeq_feat& sf);
    void SourceFeatBC (CBioSource& bsc, CSeq_feat& sf);

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
    void x_DateBC( CDate& date );
    void x_DateStdBC( CDate_std& date );

    // author-related
    void x_AuthorBC  ( CAuthor& au, bool fix_initials );
    void x_PersonIdBC( CPerson_id& pid, bool fix_initials );
    void x_NameStdBC ( CName_std& name, bool fix_initials );
    void x_ExtractSuffixFromInitials(CName_std& name);
    void x_FixEtAl(CName_std& name);
    void x_FixSuffix(CName_std& name);
    void x_FixInitials(CName_std& name);

    // user object related
    void x_CleanupUserString(string& str);

    void x_AddReplaceQual(CSeq_feat& feat, const string& str);

    void x_SeqIntervalBC( CSeq_interval & seq_interval );

    void x_SplitDbtag( CDbtag &dbt, vector< CRef< CDbtag > > & out_new_dbtags );

    void x_SeqFeatTRNABC( CSeq_feat& feat, CTrna_ext & tRNA );

    // modernize PCR Primer
    void x_ModernizePCRPrimers( CBioSource &biosrc );

protected:

    CRef<CCleanupChange>  m_Changes;
    Uint4                 m_Options;
    CRef<CObjectManager>  m_Objmgr;
    CRef<CScope>          m_Scope;
    bool                  m_IsEmblOrDdbj;
    bool                  m_StripSerial;
    bool                  m_IsGpipe;
};



END_SCOPE(objects)
END_NCBI_SCOPE

// new_cleanup.cpp

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

#include <ncbi_pch.hpp>

// #include <new_cleanupp.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


static CRef<CCleanupChange> makeCleanupChange (
    Uint4 options
)

{
    CRef<CCleanupChange> changes;

    if (! (options & CNewCleanup::eClean_NoReporting)) {
        changes.Reset (new CCleanupChange);        
    }

    return changes;
}

// constructor
CNewCleanup::CNewCleanup (void)

{
}

// destructor
CNewCleanup::~CNewCleanup (void)

{
}

CConstRef<CCleanupChange> CNewCleanup::BasicCleanup (
    CSeq_entry& se,
    Uint4 options
)

{
    CRef<CCleanupChange> changes (makeCleanupChange (options));
    CNewCleanup_imp clean_i (changes, options);
    clean_i.BasicCleanup (se);
    return changes;
}

CConstRef<CCleanupChange> CNewCleanup::BasicCleanup (
    CSeq_submit& ss,
    Uint4 options
)

{
    CRef<CCleanupChange> changes (makeCleanupChange (options));
    CNewCleanup_imp clean_i (changes, options);
    clean_i.BasicCleanup (ss);
    return changes;
}

CConstRef<CCleanupChange> CNewCleanup::BasicCleanup (
    CSeq_annot& sa,
    Uint4 options
)

{
    CRef<CCleanupChange> changes (makeCleanupChange (options));
    CNewCleanup_imp clean_i (changes, options);
    clean_i.BasicCleanup (sa);
    return changes;
}

CConstRef<CCleanupChange> CNewCleanup::ExtendedCleanup (
    CSeq_entry& se,
    Uint4 options
)

{
    CRef<CCleanupChange> changes (makeCleanupChange (options));
    CNewCleanup_imp clean_i (changes, options);
    clean_i.ExtendedCleanup (se);
    return changes;
}

CConstRef<CCleanupChange> CNewCleanup::ExtendedCleanup (
    CSeq_submit& ss,
    Uint4 options
)

{
    CRef<CCleanupChange> changes (makeCleanupChange (options));
    CNewCleanup_imp clean_i (changes, options);
    clean_i.ExtendedCleanup (ss);
    return changes;
}

CConstRef<CCleanupChange> CNewCleanup::ExtendedCleanup (
    CSeq_annot& sa,
    Uint4 options
)

{
    CRef<CCleanupChange> changes (makeCleanupChange (options));
    CNewCleanup_imp clean_i (changes, options);
    clean_i.ExtendedCleanup (sa);
    return changes;
}

// new_cleanupp.cpp

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
*   Implementation of Basic and Extended Cleanup of CSeq_entries.
*
* ===========================================================================
*/

// #include <ncbi_pch.hpp>

// #include <new_cleanupp.hpp>
#include "cleanup_utils.hpp"

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objects/medline/Medline_entry.hpp>

#include <util/sequtil/sequtil_convert.hpp>
#include <util/sequtil/sequtil_manip.hpp>
#include <util/xregexp/regexp.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


// Constructor
CNewCleanup_imp::CNewCleanup_imp (CRef<CCleanupChange> changes, Uint4 options)
    : m_Changes(changes),
      m_Options(options),
      m_Objmgr(NULL),
      m_Scope(NULL),
      m_IsEmblOrDdbj(false),
      m_StripSerial(false),
      m_IsGpipe(false)

{
  if (options & CNewCleanup::eClean_GpipeMode) {
      m_IsGpipe = true;
  }
}

// Destructor
CNewCleanup_imp::~CNewCleanup_imp (void)

{
}

// Main methods

void CNewCleanup_imp::BasicCleanup (
    CSeq_entry& se
)

{
    // set context for top-level Seq-entry or Seq-submit components
    SetupBC (se);

    SeqentryBC (se);
}

void CNewCleanup_imp::BasicCleanup (
    CSeq_submit& ss
)

{
    if (FIELD_IS_SET (ss, Sub)) {
        CSubmit_block& sb = GET_MUTABLE (ss, Sub);
        SubmitblockBC (sb);
    }

    SWITCH_ON_SEQSUBMIT_CHOICE (ss) {
        case NCBI_SEQSUBMIT(Entrys):
            EDIT_EACH_SEQENTRY_ON_SEQSUBMIT (ss_itr, ss) {
                CSeq_entry& se = **ss_itr;
                BasicCleanup (se);
            }
            break;
        case NCBI_SEQSUBMIT(Annots):
            EDIT_EACH_SEQANNOT_ON_SEQSUBMIT (sa_itr, ss) {
                CSeq_annot& sa = **sa_itr;
                BasicCleanup (sa);
            }
            break;
        default :
          break;
    }
}

void CNewCleanup_imp::BasicCleanup (
    CSeq_annot& sa
)

{
    // no Seq-entry context, so skip setup function
    SeqannotBC (sa);
}

// Implementation methods

void CNewCleanup_imp::ChangeMade (CCleanupChange::EChanges e)
{
    if (m_Changes) {
        m_Changes->SetChanged (e);
    }
}

void CNewCleanup_imp::SetupBC (
    CSeq_entry& se
)

{
    // for cleanup Seq-entry and Seq-submit, set scope and parentize
    m_Objmgr = CObjectManager::GetInstance ();
    m_Scope.Reset (new CScope (*m_Objmgr));
    m_Scope->AddTopLevelSeqEntry (se);
    se.Parentize ();

    // a few differences based on sequence identifier type
    m_IsEmblOrDdbj = false;
    m_StripSerial = false;
    VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY (bs_itr, se) {
        const CBioseq& bs = *bs_itr;
        FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, bs) {
            const CSeq_id& sid = **sid_itr;
            SWITCH_ON_SEQID_CHOICE (sid) {
                case NCBI_SEQID(Genbank):
                case NCBI_SEQID(Tpg):
                    {
                        const CTextseq_id& tsid = *GET_FIELD (sid, Textseq_Id);
                        if (FIELD_IS_SET (tsid, Accession)) {
                            const string& acc = GET_FIELD (tsid, Accession);
                            if (acc.length() != 6) {
                                m_StripSerial = true;
                            }
                        }
                    }
                    break;
                case NCBI_SEQID(Embl):
                case NCBI_SEQID(Ddbj):
                case NCBI_SEQID(Tpe):
                case NCBI_SEQID(Tpd):
                    m_IsEmblOrDdbj = true;
                    break;
               case NCBI_SEQID(not_set):
               case NCBI_SEQID(Local):
               case NCBI_SEQID(Other):
               case NCBI_SEQID(General):
                    m_StripSerial = true;
                    break;
                default:
                    break;
            }
        }
        // bail after examining all Seq-ids in first sequence
        return;
    }
}

void CNewCleanup_imp::SubmitblockBC (
    CSubmit_block& sb
)

{
    // !!! still needs to be implemented !!!
}

void CNewCleanup_imp::SeqentryBC (
    CSeq_entry& se
)

{
    // recursive exploration from top Seq-entry

    SWITCH_ON_SEQENTRY_CHOICE (se) {
        case NCBI_SEQENTRY(Seq) :
            {
                CBioseq& bs = GET_MUTABLE (se, Seq);
                BioseqBC (bs);
            }
            break;
        case NCBI_SEQENTRY(Set) :
            {
                CBioseq_set& bss = GET_MUTABLE (se, Set);
                SeqsetBC (bss);
            }
            break;
        default :
            break;
    }
}

void CNewCleanup_imp::SeqsetBC (
    CBioseq_set& bss
)

{
    EDIT_EACH_SEQDESC_ON_SEQSET (sd_itr, bss) {
        CSeqdesc& sd = **sd_itr;
        SeqdescBC (sd);
    }

    EDIT_EACH_ANNOT_ON_SEQSET (sa_itr, bss) {
        CSeq_annot& sa = **sa_itr;
        SeqannotBC (sa);
    }

    EDIT_EACH_SEQENTRY_ON_SEQSET (se_itr, bss) {
        CSeq_entry& se = **se_itr;
        SeqentryBC (se);
    }
}

void CNewCleanup_imp::BioseqBC (
    CBioseq& bs
)

{
    EDIT_EACH_SEQDESC_ON_BIOSEQ (sd_itr, bs) {
        CSeqdesc& sd = **sd_itr;
        SeqdescBC (sd);
    }

    EDIT_EACH_ANNOT_ON_BIOSEQ (sa_itr, bs) {
        CSeq_annot& sa = **sa_itr;
        SeqannotBC (sa);
    }

    // TODO: test this
    /* EDIT_EACH_SEQID_ON_BIOSEQ( seqid_itr, bs ) {
        CSeq_id &seq_id = **seqid_itr;
        // also, use macros here
        if( SEQID_CHOICE_IS( seq_id, NCBI_SEQID(Local) ) ) {
            if( CleanString( seq_id.SetLocal().SetStr() ) ) {
                ChangeMade (CCleanupChange::eTrimSpaces);
            }
        }
    } */

    // !!! TODO: cleanup instance and ids !!!
}

void CNewCleanup_imp::SeqdescBC (
    CSeqdesc& sd
)

{
    SWITCH_ON_SEQDESC_CHOICE (sd) {
        case NCBI_SEQDESC(Mol_type) :
            break;
        case NCBI_SEQDESC(Modif) :
            break;
        case NCBI_SEQDESC(Method) :
            break;
        case NCBI_SEQDESC(Name) :
            {
                string& str = GET_MUTABLE (sd, Name);
                if (CleanString (str)) {
                    ChangeMade (CCleanupChange::eTrimSpaces);
                }
            }
            break;
        case NCBI_SEQDESC(Title) :
            {
                string& str = GET_MUTABLE (sd, Title);
                if (CleanString (str), false) {
                    ChangeMade (CCleanupChange::eTrimSpaces);
                }
            }
            break;
        case NCBI_SEQDESC(Comment) :
            {
                string& str = GET_MUTABLE (sd, Comment);
                if (CleanString (str)) {
                    ChangeMade (CCleanupChange::eTrimSpaces);
                }
            }
            break;
        case NCBI_SEQDESC(Num) :
            break;
        case NCBI_SEQDESC(Maploc) :
            break;
        case NCBI_SEQDESC(Pir) :
            break;
        case NCBI_SEQDESC(Genbank) :
            {
                CGB_block& gbk = GET_MUTABLE (sd, Genbank);
                GBblockBC (gbk);
            }
            break;
        case NCBI_SEQDESC(Pub) :
            {
                CPubdesc& pub = GET_MUTABLE (sd, Pub);
                PubdescBC (pub);
            }
            break;
        case NCBI_SEQDESC(Region) :
            {
                string& str = GET_MUTABLE (sd, Region);
                if (CleanString (str)) {
                    ChangeMade (CCleanupChange::eTrimSpaces);
                }
            }
            break;
        case NCBI_SEQDESC(User) :
            {
                CUser_object& usr = GET_MUTABLE (sd, User);
                UserobjBC (usr);
            }
            break;
        case NCBI_SEQDESC(Sp) :
            break;
        case NCBI_SEQDESC(Dbxref) :
            break;
        case NCBI_SEQDESC(Embl) :
            {
                CEMBL_block& emb = GET_MUTABLE (sd, Embl);
                EMBLblockBC (emb);
            }
            break;
        case NCBI_SEQDESC(Create_date) :
            break;
        case NCBI_SEQDESC(Update_date) :
            break;
        case NCBI_SEQDESC(Prf) :
            break;
        case NCBI_SEQDESC(Pdb) :
            break;
        case NCBI_SEQDESC(Het) :
            break;
        case NCBI_SEQDESC(Org) :
            {
                // wrap Org_ref in BioSource
                CRef <COrg_ref> org (&sd.SetOrg());
                sd.SetSource().SetOrg(*org);
                ChangeMade (CCleanupChange::eRemoveDescriptor);
                ChangeMade (CCleanupChange::eAddDescriptor);
            }
            // fall through to do BioSource cleanup
        case NCBI_SEQDESC(Source) :
            {
                CBioSource& bsc = GET_MUTABLE (sd, Source);
                BiosourceBC (bsc);
            }
            break;
        case NCBI_SEQDESC(Molinfo) :
            {
            }
            break;
        default :
            break;
    }
}

static bool s_AccessionCompare (
    const string& str1,
    const string& str2
)

{
    return ( NStr::CompareNocase( str1, str2 ) < 0 );
}

static bool s_AccessionEqual (
    const string& str1,
    const string& str2
)

{
    if (NStr::EqualNocase (str1, str2)) return true;

    return false;
}

void CNewCleanup_imp::GBblockBC (
    CGB_block& gbk
)

{
    CLEAN_STRING_LIST_JUNK (gbk, Extra_accessions);

    if (! EXTRAACCN_ON_GENBANKBLOCK_IS_SORTED (gbk, s_AccessionCompare)) {
        SORT_EXTRAACCN_ON_GENBANKBLOCK (gbk, s_AccessionCompare);
        ChangeMade (CCleanupChange::eCleanQualifiers);
    }

    if (! EXTRAACCN_ON_GENBANKBLOCK_IS_UNIQUE (gbk, s_AccessionEqual)) {
        UNIQUE_EXTRAACCN_ON_GENBANKBLOCK (gbk, s_AccessionEqual);
        ChangeMade (CCleanupChange::eCleanQualifiers);
    }

    CLEAN_STRING_LIST (gbk, Keywords);

    EDIT_EACH_KEYWORD_ON_GENBANKBLOCK (kw_itr, gbk) {
        string& str = *kw_itr;
        for (KEYWORD_ON_GENBANKBLOCK_Type::const_iterator tmp_itr = KEYWORD_ON_GENBANKBLOCK_Get(gbk).begin();
             tmp_itr != kw_itr; ++tmp_itr) {
            const string& prv = *tmp_itr;
            if (! NStr::EqualNocase (str, prv)) continue;
            ERASE_KEYWORD_ON_GENBANKBLOCK (kw_itr, gbk);
            ChangeMade (CCleanupChange::eCleanKeywords);
            break;
        }
    }

    CLEAN_STRING_MEMBER (gbk, Source);
    CLEAN_STRING_MEMBER (gbk, Origin);

    CLEAN_STRING_MEMBER (gbk, Date);
    CLEAN_STRING_MEMBER (gbk, Div);
}

void CNewCleanup_imp::EMBLblockBC (
    CEMBL_block& emb
)

{
    CLEAN_STRING_LIST_JUNK (emb, Extra_acc);

    if (! EXTRAACCN_ON_EMBLBLOCK_IS_SORTED (emb, s_AccessionCompare)) {
        SORT_EXTRAACCN_ON_EMBLBLOCK (emb, s_AccessionCompare);
        ChangeMade (CCleanupChange::eCleanQualifiers);
    }

    if (! EXTRAACCN_ON_EMBLBLOCK_IS_UNIQUE (emb, s_AccessionEqual)) {
        UNIQUE_EXTRAACCN_ON_EMBLBLOCK (emb, s_AccessionEqual);
        ChangeMade (CCleanupChange::eCleanQualifiers);
    }

    CLEAN_STRING_LIST (emb, Keywords);

    EDIT_EACH_KEYWORD_ON_EMBLBLOCK (kw_itr, emb) {
        string& str = *kw_itr;
        for (KEYWORD_ON_EMBLBLOCK_Type::const_iterator tmp_itr = KEYWORD_ON_EMBLBLOCK_Get(emb).begin();
             tmp_itr != kw_itr; ++tmp_itr) {
            const string& prv = *tmp_itr;
            if (! NStr::Equal(str, prv)) continue;
            ERASE_KEYWORD_ON_EMBLBLOCK (kw_itr, emb);
            ChangeMade (CCleanupChange::eCleanKeywords);
            break;
        }
    }
}

static CSubSource* s_StringToSubSource (
    const string& str
)

{
    size_t pos = str.find('=');
    if (pos == NPOS) {
        pos = str.find(' ');
    }
    string subtype = str.substr(0, pos);

    try {
        TSUBSOURCE_SUBTYPE val = CSubSource::GetSubtypeValue(subtype);
        
        string name;
        if (pos != NPOS) {
            name = str.substr(pos + 1);
        }
        NStr::TruncateSpacesInPlace(name);
        
        if (CSubSource::NeedsNoText (val) ) {
            name = "";
        } else if (NStr::IsBlank (name)) {
            return NULL;
        }
        
        size_t num_spaces = 0;
        bool has_comma = false;
        FOR_EACH_CHAR_IN_STRING (it, name) {
            const char& ch = *it;
            if (isspace((unsigned char) ch)) {
                ++num_spaces;
            } else if (ch == ',') {
                has_comma = true;
                break;
            }
        }
        
        if (num_spaces > 4  ||  has_comma) {
            return NULL;
        }
        return new CSubSource(val, name);
    } catch (CSerialException&) {}

    return NULL;
}

// is st1 < st2

static bool s_SubsourceCompare (
    const CRef<CSubSource>& st1,
    const CRef<CSubSource>& st2
)

{
    const CSubSource& sbs1 = *(st1);
    const CSubSource& sbs2 = *(st2);

    TSUBSOURCE_SUBTYPE chs1 = GET_FIELD (sbs1, Subtype);
    TSUBSOURCE_SUBTYPE chs2 = GET_FIELD (sbs2, Subtype);

    if (chs1 < chs2) return true;
    if (chs1 > chs2) return false;

    if (FIELD_IS_SET (sbs2, Name)) {
        if (! FIELD_IS_SET (sbs1, Name)) return true;
        if (NStr::CompareNocase (GET_FIELD (sbs1, Name), GET_FIELD (sbs2, Name)) < 0) return true;
    }

    return false;
}

// Two SubSource's are equal and duplicates if:
// they have the same subtype
// and the same name (or don't require a name).

static bool s_SubsourceEqual (
    const CRef<CSubSource>& st1,
    const CRef<CSubSource>& st2
)

{
    const CSubSource& sbs1 = *(st1);
    const CSubSource& sbs2 = *(st2);

    TSUBSOURCE_SUBTYPE chs1 = GET_FIELD (sbs1, Subtype);
    TSUBSOURCE_SUBTYPE chs2 = GET_FIELD (sbs2, Subtype);

    if (chs1 != chs2) return false;
    if (CSubSource::NeedsNoText (chs2)) return true;

    if (FIELD_IS_SET (sbs1, Name) && FIELD_IS_SET (sbs2, Name)) {
        if (NStr::EqualNocase (GET_FIELD (sbs1, Name), GET_FIELD (sbs2, Name))) return true;
    }
    if (! FIELD_IS_SET (sbs1, Name) && ! FIELD_IS_SET (sbs2, Name)) return true;

    return false;
}

void CNewCleanup_imp::SubsourceBC (
    CSubSource& sbs
)

{
    CLEAN_STRING_MEMBER (sbs, Name);
    CLEAN_STRING_MEMBER (sbs, Attrib);

    TSUBSOURCE_SUBTYPE chs = GET_FIELD (sbs, Subtype);
    if (CSubSource::NeedsNoText (chs)) {
        if (! FIELD_IS_SET (sbs, Name)) {
            // name is required - set it to empty string
            SET_FIELD (sbs, Name, "");
        }
    }
}

void CNewCleanup_imp::BiosourceBC (
    CBioSource& biosrc
)
{
    if( FIELD_EQUALS( biosrc, Genome, CBioSource::eGenome_virion ) ) {
        RESET_FIELD( biosrc, Genome );
        ChangeMade ( CCleanupChange::eChangeBioSourceGenome );
    }

    // TODO: see 10585 of sqnutil1.c (Jan 25, 2011)
    if (BIOSOURCE_HAS_ORGREF (biosrc)) {
        COrg_ref& org = GET_MUTABLE (biosrc, Org);
        OrgrefBC (org);

        // convert COrg_reg.TMod string to SubSource objects
        EDIT_EACH_MOD_ON_ORGREF (it, org) {
            string& str = *it;
            CRef<CSubSource> sbs (s_StringToSubSource (str));
            if (! sbs) continue;
            ADD_SUBSOURCE_TO_BIOSOURCE (biosrc, sbs);
            ERASE_MOD_ON_ORGREF (it, org);
            ChangeMade (CCleanupChange::eChangeSubsource);
        }

        if( MOD_ON_ORGREF_IS_EMPTY(org) ) {
            RESET_FIELD(org, Mod);
        }
    }

    if (BIOSOURCE_HAS_SUBSOURCE (biosrc)) {
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, biosrc) {
            CSubSource& sbs = **it;
            SubsourceBC (sbs);
        }

        // remove those with no name unless it has a subtype that doesn't need a name.
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, biosrc) {
            CSubSource& sbs = **it;
            if (FIELD_IS_SET (sbs, Name)) continue;
            TSUBSOURCE_SUBTYPE chs = GET_FIELD (sbs, Subtype);
            if (CSubSource::NeedsNoText (chs)) continue;
            ERASE_SUBSOURCE_ON_BIOSOURCE (it, biosrc);
            ChangeMade (CCleanupChange::eCleanSubsource);
        }

        // remove plastid-name qual if the value is the same as the biosource location
        string plastid_name = "";
        SWITCH_ON_BIOSOURCE_GENOME (biosrc) {
            case NCBI_GENOME(apicoplast):
                plastid_name = "apicoplast";
                break;
            case NCBI_GENOME(chloroplast):
                plastid_name = "chloroplast";
                break;
            case NCBI_GENOME(chromoplast):
                plastid_name = "chromoplast";
                break;
            case NCBI_GENOME(kinetoplast):
                plastid_name = "kinetoplast";
                break;
            case NCBI_GENOME(leucoplast):
                plastid_name = "leucoplast";
                break;
            case NCBI_GENOME(plastid):
                plastid_name = "plastid";
                break;
            case NCBI_GENOME(proplastid):
                plastid_name = "proplastid";
                break;
            default:
                break;
        }

        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, biosrc) {
          CSubSource& sbs = **it;
          TSUBSOURCE_SUBTYPE chs = GET_FIELD (sbs, Subtype);
          if (CSubSource::NeedsNoText (chs)) {
              RESET_FIELD (sbs, Name);
              SET_FIELD (sbs, Name, "");
              ChangeMade (CCleanupChange::eCleanSubsource);
          } else if (chs == NCBI_SUBSOURCE(plastid_name)) {
              if (NStr::EqualNocase (GET_FIELD (sbs, Name), plastid_name)) {
                  ERASE_SUBSOURCE_ON_BIOSOURCE (it, biosrc);
                  ChangeMade (CCleanupChange::eCleanSubsource);
              }
          }
        }

        // remove spaces and convert to lowercase in fwd_primer_seq and rev_primer_seq.
        // TODO: subsources should actually be loaded into the pcr-primers structures
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, biosrc) {
          CSubSource& sbs = **it;
          if (SUBSOURCE_CHOICE_IS (sbs, NCBI_SUBSOURCE(fwd_primer_seq)) ||
              SUBSOURCE_CHOICE_IS (sbs, NCBI_SUBSOURCE(rev_primer_seq))) {
              string before = GET_FIELD (sbs, Name);
              NStr::ToLower (GET_MUTABLE (sbs, Name));
              const string& after = NStr::Replace (GET_FIELD (sbs, Name), " ", kEmptyStr);
              SET_FIELD (sbs, Name, after);
              if (NStr::Equal (before, after)) continue;
              ChangeMade (CCleanupChange::eCleanSubsource);
          }
        }

        // sort and remove duplicates.
        // Do not sort before merging primer_seq's above.
    
        if (! SUBSOURCE_ON_BIOSOURCE_IS_SORTED (biosrc, s_SubsourceCompare)) {
            SORT_SUBSOURCE_ON_BIOSOURCE (biosrc, s_SubsourceCompare);
            ChangeMade (CCleanupChange::eCleanSubsource);
        }

        if (! SUBSOURCE_ON_BIOSOURCE_IS_UNIQUE (biosrc, s_SubsourceEqual)) {
            UNIQUE_SUBSOURCE_ON_BIOSOURCE (biosrc, s_SubsourceEqual);
            ChangeMade (CCleanupChange::eCleanSubsource);
        }
    }

    // Modernize PCR Primers
    x_ModernizePCRPrimers( biosrc );
}

static COrgMod* s_StringToOrgMod (
    const string& str
)

{
    try {
        size_t pos = str.find('=');
        if (pos == NPOS) {
            pos = str.find(' ');
        }
        if (pos == NPOS) {
            return NULL;
        }
        
        string subtype = str.substr(0, pos);
        string subname = str.substr(pos + 1);
        NStr::TruncateSpacesInPlace(subname);
        
        
        size_t num_spaces = 0;
        bool has_comma = false;
        FOR_EACH_CHAR_IN_STRING (it, subname) {
            const char& ch = *it;
            if (isspace((unsigned char) ch)) {
                ++num_spaces;
            } else if (ch == ',') {
                has_comma = true;
                break;
            }
        }
        if (num_spaces > 4  ||  has_comma) {
            return NULL;
        }
        
        return new COrgMod(subtype, subname);
    } catch (CSerialException&) {}

    return NULL;
}

static bool s_DbtagIsBad (
    CDbtag& dbt
)

{
    if (! FIELD_IS_SET (dbt, Db)) return true;
    string& db = GET_MUTABLE (dbt, Db);
    if (NStr::IsBlank (db)) return true;
    if( NStr::EqualNocase(db, "PID") ||
        NStr::EqualNocase(db, "PIDg") ||
        NStr::EqualNocase(db, "NID") ) {
            return true;
    }

    if (! FIELD_IS_SET( dbt, Tag)) return true;
    CObject_id& oid = GET_MUTABLE (dbt, Tag);

    if (FIELD_IS (oid, Id)) {
        if (GET_FIELD (oid, Id) == 0) return true;
    } else if (FIELD_IS (oid, Str)) {
        const string& str = GET_FIELD (oid, Str);
        if (NStr::IsBlank (str)) return true;
    } else return true;

    return false;
}

void CNewCleanup_imp::OrgrefBC (
    COrg_ref& org
)

{
    CLEAN_STRING_MEMBER (org, Taxname);
    CLEAN_STRING_MEMBER (org, Common);
    CLEAN_STRING_LIST (org, Mod);
    CLEAN_STRING_LIST (org, Syn);

    EDIT_EACH_MOD_ON_ORGREF (it, org) {
        string& str = *it;
        CRef<COrgMod> omd (s_StringToOrgMod (str));
        if (! omd) continue;
        ADD_ORGMOD_TO_ORGREF (org, omd);
        ERASE_MOD_ON_ORGREF (it, org);
        ChangeMade (CCleanupChange::eChangeOrgmod);
    }
    if (MOD_ON_ORGREF_Set (org).empty()) {
        RESET_FIELD (org, Mod);
        ChangeMade (CCleanupChange::eChangeOther);
    }

    if (FIELD_IS_SET (org, Orgname)) {
        COrgName& onm = GET_MUTABLE (org, Orgname);
        OrgnameBC (onm);
    }


    if (ORGREF_HAS_DBXREF (org)) {
        
        vector< CRef< CDbtag > > new_dbtags;
        EDIT_EACH_DBXREF_ON_ORGREF (it, org) {
            CDbtag& dbt = **it;
            DbtagBC (dbt);

            x_SplitDbtag(dbt, new_dbtags );

            if (s_DbtagIsBad (dbt)) {
                ERASE_DBXREF_ON_ORGREF (it, org);
                ChangeMade (CCleanupChange::eCleanDbxrefs);
            }
        }
        copy( new_dbtags.begin(), new_dbtags.end(), back_inserter( org.SetDb() ) );

        // sort/unique db_xrefs

        if (! DBXREF_ON_ORGREF_IS_SORTED (org, s_DbtagCompare)) {
            SORT_DBXREF_ON_ORGREF (org, s_DbtagCompare);
            ChangeMade (CCleanupChange::eCleanDbxrefs);
        }

        if (! DBXREF_ON_ORGREF_IS_UNIQUE (org, s_DbtagEqual)) {
            UNIQUE_DBXREF_ON_ORGREF (org, s_DbtagEqual);
            ChangeMade (CCleanupChange::eCleanDbxrefs);
        }
    }
}

// is om1 < om2
// to sort subtypes together.

static bool s_OrgModCompare (
    const CRef<COrgMod>& om1,
    const CRef<COrgMod>& om2
)

{
    const COrgMod& omd1 = *(om1);
    const COrgMod& omd2 = *(om2);

    // subtype comparison
    TORGMOD_SUBTYPE subtype1 = GET_FIELD (omd1, Subtype);
    TORGMOD_SUBTYPE subtype2 = GET_FIELD (omd2, Subtype);
    if (subtype1 < subtype2) return true;
    if (subtype1 > subtype2) return false;

    // subname comparison
    const string& subname1 = GET_FIELD (omd1, Subname);
    const string& subname2 = GET_FIELD (omd2, Subname);
    const int subname_comparison = NStr::CompareNocase( subname1, subname2 );
    if( subname_comparison < 0 ) {
        return true;
    } else if( subname_comparison > 0 ) {
        return false;
    }

    // attrib comparison (realistically, we don't expect to fall back to this)
    const string& attrib1 = GET_FIELD (omd1, Attrib);
    const string& attrib2 = GET_FIELD (omd2, Attrib);
    return NStr::CompareNocase( attrib1, attrib2 ) < 0;
}

// Two OrgMod's are equal and duplicates if:
// they have the same subname and same subtype
// or one has subtype 'other'.

static bool s_OrgModEqual (
    const CRef<COrgMod>& om1,
    const CRef<COrgMod>& om2
)

{
    const COrgMod& omd1 = *(om1);
    const COrgMod& omd2 = *(om2);

    const string& subname1 = GET_FIELD (omd1, Subname);
    const string& subname2 = GET_FIELD (omd2, Subname);
    if (! NStr::EqualNocase (subname1, subname2)) return false;

    const string& attrib1 = GET_FIELD (omd1, Attrib);
    const string& attrib2 = GET_FIELD (omd2, Attrib);
    if (! NStr::EqualNocase (attrib1, attrib2)) return false;

    TORGMOD_SUBTYPE chs1 = GET_FIELD (omd1, Subtype);
    TORGMOD_SUBTYPE chs2 = GET_FIELD (omd2, Subtype);
    if (chs1 == chs2) return true;
    if ( chs1 == NCBI_ORGMOD(other) || chs2 == NCBI_ORGMOD(other)) return true;

    return false;
}

void CNewCleanup_imp::OrgnameBC (
    COrgName& onm
)

{
    CLEAN_STRING_MEMBER (onm, Attrib);
    CLEAN_STRING_MEMBER_JUNK (onm, Lineage);
    CLEAN_STRING_MEMBER_JUNK (onm, Div);

    EDIT_EACH_ORGMOD_ON_ORGNAME (it, onm) {
        COrgMod& omd = **it;
        OrgmodBC (omd);
        if (! FIELD_IS_SET (omd, Subname) || NStr::IsBlank (GET_FIELD (omd, Subname))) {
            ERASE_ORGMOD_ON_ORGNAME (it, onm);
        }
    }

    // erase structured notes that already match value
    // (Note: This is O(N^2).  Maybe worth converting to a faster algo?)
    EDIT_EACH_ORGMOD_ON_ORGNAME (it, onm) {
        COrgMod& omd = **it;
        if (omd.GetSubtype() == NCBI_ORGMOD(other)) {
            bool do_erase = false;
            string val_name, otherval;
            NStr::SplitInTwo( omd.GetSubname(), " =:", val_name, otherval );
            try {
                COrgMod::TSubtype subtype = COrgMod::GetSubtypeValue(val_name);
                NStr::TruncateSpacesInPlace(otherval);                
                FOR_EACH_ORGMOD_ON_ORGNAME (match_it, onm) {
                    if ((*match_it)->GetSubtype() == subtype
                        && NStr::EqualCase((*match_it)->GetSubname(), otherval)) {
                        do_erase = true;
                        break;
                    }
                }
            } catch (CSerialException& ) {
            }

            if (do_erase) {
                ERASE_ORGMOD_ON_ORGNAME (it, onm);
                ChangeMade (CCleanupChange::eCleanOrgmod);
            }
        }
    }

    if (! ORGMOD_ON_ORGNAME_IS_SORTED (onm, s_OrgModCompare)) {
        SORT_ORGMOD_ON_ORGNAME (onm, s_OrgModCompare);
        ChangeMade (CCleanupChange::eCleanOrgmod);
    }

    if (! ORGMOD_ON_ORGNAME_IS_UNIQUE (onm, s_OrgModEqual)) {
        UNIQUE_ORGMOD_ON_ORGNAME (onm, s_OrgModEqual);
        ChangeMade (CCleanupChange::eCleanOrgmod);
    }
}

static bool RemoveSpaceBeforeAndAfterColon (
    string& str
)

{
    size_t pos, prev_pos, next_pos;
    bool changed = false;

    pos = NStr::Find (str, ":");
    while (pos != string::npos) {
        if (pos > 0) {
            // clean up spaces before
            prev_pos = pos - 1;
            while (prev_pos > 0 && isspace (str[prev_pos])) {
                prev_pos--;
            }
            if ( !isspace (str[prev_pos])) {
                prev_pos++;
            }
        } else {
          prev_pos = 0;
        }
            
        next_pos = pos + 1;
        while ( next_pos < str.length() && 
            (isspace (str[next_pos]) || str[next_pos] == ':') ) {
                next_pos++;
        }
        string before = "";
        if (prev_pos > 0) {
            before = str.substr(0, prev_pos);
        }
        string after = str.substr(next_pos);
        string tmp = before + ":" + after;
        if (!NStr::Equal (str, tmp)) {
            str = tmp;
            changed = true;
        }
        pos = NStr::Find (str, ":", prev_pos + 1);
    }

    return changed;
}

void CNewCleanup_imp::OrgmodBC (
    COrgMod& omd
)

{
    CLEAN_STRING_MEMBER (omd, Subname);
    CLEAN_STRING_MEMBER (omd, Attrib);

    TORGMOD_SUBTYPE subtype = GET_FIELD (omd, Subtype);

    if (subtype == NCBI_ORGMOD(specimen_voucher) ||
        subtype == NCBI_ORGMOD(culture_collection) ||
        subtype == NCBI_ORGMOD(bio_material)) {
        if (FIELD_IS_SET (omd, Subname)) {
            if (RemoveSpaceBeforeAndAfterColon (GET_MUTABLE (omd, Subname))) {
                ChangeMade (CCleanupChange::eTrimSpaces);
            }
        }
    }
}

void CNewCleanup_imp::DbtagBC (
    CDbtag& dbtag
)

{
    if (! FIELD_IS_SET (dbtag, Db)) return;
    if (! FIELD_IS_SET (dbtag, Tag)) return;

    string& db = GET_MUTABLE (dbtag, Db);
    if (NStr::IsBlank (db)) return;

    if (CleanString (db, true)) {
        ChangeMade (CCleanupChange::eTrimSpaces);
    }

    if (NStr::EqualNocase(db, "Swiss-Prot")
        || NStr::EqualNocase (db, "SWISSPROT")) {
        db = "UniProtKB/Swiss-Prot";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "SPTREMBL")  ||
               NStr::EqualNocase(db, "TrEMBL") ) {
        db = "UniProtKB/TrEMBL";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "SUBTILIS")) {
        db = "SubtiList";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "LocusID")) {
        db = "GeneID";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "MaizeDB")) {
        db = "MaizeGDB";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "GeneW")) {
        db = "HGNC";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "MGD")) {
        db = "MGI";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "IFO")) {
        db = "NBRC";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "BHB") ||
        NStr::EqualNocase(db, "BioHealthBase")) {
        db = "IRD";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::Equal(db, "GENEDB")) {
        db = "GeneDB";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::Equal(db, "cdd")) {
        db = "CDD";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::Equal(db, "FlyBase")) {
        db = "FLYBASE";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "GreengenesID")) {
        db = "Greengenes";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "HMPID")) {
        db = "HMP";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    }

    CObject_id& oid = GET_MUTABLE (dbtag, Tag);
    if (! FIELD_IS (oid, Str)) return;

    string& str = GET_MUTABLE(oid, Str);
    if (NStr::IsBlank (str)) return;

    if (CleanString (str, true)) {
        ChangeMade (CCleanupChange::eTrimSpaces);
    }

    if (NStr::EqualNocase(dbtag.GetDb(), "HPRD") && NStr::StartsWith (dbtag.GetTag().GetStr(), "HPRD_")) {
        dbtag.SetTag().SetStr (dbtag.GetTag().GetStr().substr (5));
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase (dbtag.GetDb(), "MGI") ) {
        if(NStr::StartsWith (dbtag.GetTag().GetStr(), "MGI:") || NStr::StartsWith (dbtag.GetTag().GetStr(), "MGD:")) {
            dbtag.SetTag().SetStr (dbtag.GetTag().GetStr().substr (4));
            ChangeMade(CCleanupChange::eChangeDbxrefs);
        } else if( NStr::StartsWith( dbtag.GetTag().GetStr(), "J:", NStr::eNocase ) ) {
            dbtag.SetTag().SetStr("J");
        }
    }

    bool all_zero = true;
    FOR_EACH_CHAR_IN_STRING (it, str) {
        const char& ch = *it;
        if (isdigit((unsigned char)(ch))) {
            if (ch != '0') {
                all_zero = false;
            }
        } else if (!isspace((unsigned char)(ch))) {
            return;
        }
    }
    
    if (str[0] != '0'  ||  all_zero) {
        try {
            // extract the part before the first space for conversion
            string::size_type pos_of_first_space = 0;
            while( pos_of_first_space < str.length() && ! isspace(str[pos_of_first_space]) ) {
                ++pos_of_first_space;
            }
            SET_FIELD ( oid, Id, NStr::StringToUInt(str.substr(0, pos_of_first_space)) );
            ChangeMade (CCleanupChange::eChangeDbxrefs);
        } catch (CStringException&) {
            // just leave things as are
        }
    }
}

void CNewCleanup_imp::PubdescBC (
    CPubdesc& pubdesc
)
{
    if ( FIELD_IS_SET(pubdesc, Pub) ) {
        PubEquivBC( GET_MUTABLE(pubdesc, Pub) );
    }
    
    // TODO: do we clean it? Looks like C doesn't.
    // CLEAN_STRING_MEMBER(pubdesc, Name);
    
    if ( FIELD_IS_SET(pubdesc, Comment)) {
        CleanDoubleQuote( GET_MUTABLE(pubdesc, Comment) );
    }
    if (IsOnlinePub(pubdesc)) {
        TRUNCATE_SPACES(pubdesc, Comment);
    } else {
        CLEAN_STRING_MEMBER(pubdesc, Comment);
    }
}

static bool s_FixInitials(const CPub_equiv& equiv)
{
    bool has_id  = false, 
    has_art = false;
    
    FOR_EACH_PUB_ON_PUBEQUIV(pub_iter, equiv) {
        if ((*pub_iter)->IsPmid()  ||  (*pub_iter)->IsMuid()) {
            has_id = true;
        } else if ((*pub_iter)->IsArticle()) {
            has_art = true;
        }
    }
    return !(has_art  &&  has_id);
}

void CNewCleanup_imp::PubEquivBC (CPub_equiv& pub_equiv)
{
    x_FlattenPubEquiv(pub_equiv);
    
    bool fix_initials = s_FixInitials(pub_equiv);
    EDIT_EACH_PUB_ON_PUBEQUIV(it, pub_equiv) {
        PubBC(**it, fix_initials);
    }
}

void CNewCleanup_imp::PubBC(CPub& pub, bool fix_initials)
{
#define PUBBC_CASE(cit_type, func) \
    case NCBI_PUB(cit_type): \
        func( GET_MUTABLE(pub, cit_type), fix_initials); \
        break

    switch (pub.Which()) {
    PUBBC_CASE(Gen, CitGenBC);
    PUBBC_CASE(Sub, CitSubBC);
    PUBBC_CASE(Article, CitArtBC);
    PUBBC_CASE(Book, CitBookBC);
    PUBBC_CASE(Patent, CitPatBC);
    PUBBC_CASE(Man, CitLetBC);
    PUBBC_CASE(Medline, MedlineEntryBC);
    default:
        break;
    }
#undef PUBBC_CASE
}

void CNewCleanup_imp::CitGenBC(CCit_gen& cg, bool fix_initials)
{
    if( FIELD_IS_SET(cg, Authors) ) {
        AuthListBC( GET_MUTABLE(cg, Authors), fix_initials );
    }
    if ( FIELD_IS_SET(cg, Cit) ) {
        CCit_gen::TCit& cit = GET_MUTABLE( cg, Cit );
        if (NStr::StartsWith(cit, "unpublished", NStr::eNocase) && cit[0] != 'U' ) {
            cit[0] = 'U';
            ChangeMade(CCleanupChange::eChangePublication);
        }
        if (! FIELD_IS_SET(cg, Journal) 
            && ( FIELD_IS_SET(cg, Volume) || FIELD_IS_SET(cg, Pages) || FIELD_IS_SET(cg, Issue))) 
        {
            RESET_FIELD(cg, Volume);
            RESET_FIELD(cg, Pages);
            RESET_FIELD(cg, Issue);
            ChangeMade(CCleanupChange::eChangePublication);
        }
        const size_t old_cit_size = cit.size();
        NStr::TruncateSpacesInPlace(cit);
        if (old_cit_size != cit.size()) {
            ChangeMade(CCleanupChange::eChangePublication);
        }
    }
    if ( FIELD_IS_SET(cg, Pages) ) {
        if (RemoveSpaces( GET_MUTABLE(cg, Pages) ) ) {
            ChangeMade(CCleanupChange::eChangePublication);
        }
    }

    //!!! TO DO: serial
    /*
     if (stripSerial) {
         cgp->serial_number = -1;
     }
     
     compare labels before and after doing the above.
     if label changed : (sqnutil1.c, 6156)
     ValNodeCopyStr (publist, 1, buf1);
     ValNodeCopyStr (publist, 2, buf2);

     */

    if ( FIELD_IS_SET(cg, Date) ) {
        x_DateBC( GET_MUTABLE(cg, Date) );
    }
}

void CNewCleanup_imp::CitSubBC(CCit_sub& citsub, bool fix_initials)
{
   CRef<CCit_sub::TAuthors> authors;
    if ( FIELD_IS_SET(citsub, Authors) ) {
        authors.Reset(& GET_MUTABLE(citsub, Authors) );
        AuthListBC( *authors, fix_initials);
    }
    
    if ( FIELD_IS_SET(citsub, Imp) ) {
        CCit_sub::TImp& imp =  GET_MUTABLE(citsub, Imp);
        if (authors  &&  ! FIELD_IS_SET(*authors, Affil)  &&  FIELD_IS_SET(imp, Pub) ) {
            SET_FIELD(*authors, Affil, GET_MUTABLE(imp, Pub) );
            RESET_FIELD(imp, Pub);
            ChangeMade(CCleanupChange::eChangePublication);
        }
        if (! FIELD_IS_SET(citsub, Date)  &&  FIELD_IS_SET(imp, Date) ) {
             GET_MUTABLE(citsub, Date).Assign( GET_FIELD(imp, Date) );
            RESET_FIELD(imp, Date);
            ChangeMade(CCleanupChange::eChangePublication);
        }
        if ( ! FIELD_IS_SET(imp, Pub) ) {
            RESET_FIELD(citsub, Imp);
            ChangeMade(CCleanupChange::eChangePublication);
        }
    }
    if (authors  &&  FIELD_IS_SET(*authors, Affil) ) {
        //!!! TO DO
        CCit_sub::TAuthors::TAffil& affil = GET_MUTABLE(*authors, Affil);
        if ( FIELD_IS(affil, Str) ) {
            string str = GET_MUTABLE(affil, Str);
            if (NStr::StartsWith(str, "to the ", NStr::eNocase) &&
                str.size() >= 34 &&
                NStr::StartsWith(str.substr(24), " databases", NStr::eNocase) ) {
                if ( str.size() > 35 && str[35] == '.') {
                    str = str.substr(35);
                } else {
                    str = str.substr(34);
                }
                SET_FIELD(affil, Str, str);
                ChangeMade(CCleanupChange::eChangePublication);
                AffilBC(affil);
            }
        }
    }

    if ( FIELD_IS_SET(citsub, Date) ) {
        x_DateBC( GET_MUTABLE(citsub, Date) );
    }
}

void CNewCleanup_imp::CitArtBC(CCit_art& citart, bool fix_initials)
{
    if ( FIELD_IS_SET(citart, Authors) ) {
        AuthListBC( GET_MUTABLE(citart, Authors), fix_initials);
    }
    if ( FIELD_IS_SET(citart, From) ) {
        CCit_art::TFrom& from = GET_MUTABLE(citart, From);
        if ( FIELD_IS(from, Book) ) {
            CitBookBC(GET_MUTABLE(from, Book), fix_initials);
        } else if ( FIELD_IS(from, Proc) ) {
            CitProcBC( GET_MUTABLE(from, Proc), fix_initials);
        } else if (FIELD_IS(from, Journal) ) {
            CitJourBC(GET_MUTABLE(from, Journal), fix_initials);
        }
    }
}

void CNewCleanup_imp::CitBookBC(CCit_book& citbook, bool fix_initials)
{
    if ( FIELD_IS_SET(citbook, Authors) ) {
        AuthListBC( GET_MUTABLE(citbook, Authors), fix_initials);
    }
    if ( FIELD_IS_SET(citbook, Imp) ) {
        ImprintBC( GET_MUTABLE(citbook, Imp) );
    }
}

void CNewCleanup_imp::CitPatBC(CCit_pat& citpat, bool fix_initials)
{
    if ( FIELD_IS_SET(citpat, Authors) ) {
        AuthListBC( GET_MUTABLE(citpat, Authors), fix_initials);
    }
    if ( FIELD_IS_SET(citpat, Applicants) ) {
        AuthListBC( GET_MUTABLE(citpat, Applicants), fix_initials);
    }
    if ( FIELD_IS_SET(citpat, Assignees) ) {
        AuthListBC( GET_MUTABLE(citpat, Assignees), fix_initials);
    }
    if ( FIELD_IS_SET(citpat, App_date) ) {
        x_DateBC( GET_MUTABLE(citpat, App_date) );
    }
    if ( FIELD_IS_SET(citpat, Date_issue) ) {
        x_DateBC( GET_MUTABLE(citpat, Date_issue) );
    }
}

void CNewCleanup_imp::CitLetBC(CCit_let& citlet, bool fix_initials)
{
    if ( FIELD_IS_SET(citlet, Cit) && FIELD_EQUALS( citlet, Type, CCit_let::eType_thesis ) ) {
        CitBookBC( GET_MUTABLE(citlet, Cit), fix_initials);
    }
}

void CNewCleanup_imp::CitProcBC(CCit_proc& citproc, bool fix_initials)
{
    if ( FIELD_IS_SET(citproc, Book) ) {
        CitBookBC( GET_MUTABLE(citproc, Book), fix_initials);
    }
}

void CNewCleanup_imp::CitJourBC(CCit_jour &citjour, bool fix_initials)
{
    if ( FIELD_IS_SET(citjour, Imp) ) {
        ImprintBC( GET_MUTABLE(citjour, Imp) );
    }
}

void CNewCleanup_imp::MedlineEntryBC(CMedline_entry& medline, bool fix_initials)
{
    if ( ! FIELD_IS_SET(medline, Cit) || ! FIELD_IS_SET(medline.GetCit(), Authors) ) {
        return;
    }
    AuthListBC( GET_MUTABLE(medline.SetCit(), Authors), fix_initials );
}

static bool s_IsEmpty(const CAuth_list::TAffil& affil)
{
    if ( FIELD_IS(affil, Str) ) {
        return NStr::IsBlank( GET_FIELD(affil, Str) );
    } else if ( FIELD_IS(affil, Std) ) {
        const CAuth_list::TAffil::TStd& std = GET_FIELD(affil, Std);
        return !(std.IsSetAffil()  ||  std.IsSetDiv()      ||  std.IsSetCity()    ||
                 std.IsSetSub()    ||  std.IsSetCountry()  ||  std.IsSetStreet()  ||
                 std.IsSetEmail()  ||  std.IsSetFax()      ||  std.IsSetPhone()   ||
                 std.IsSetPostal_code());
    }
    return true;
}

static bool s_IsEmpty(const CAuthor& auth)
{
    if (! FIELD_IS_SET(auth, Name)) {
        return true;
    }
    
    const CAuthor::TName& name = GET_FIELD(auth, Name);
    
    const string* str = NULL;
    switch (name.Which()) {
        case CAuthor::TName::e_not_set:
            return true;
            
        case CAuthor::TName::e_Name:
        {{
            const CName_std& nstd = name.GetName();
            if ((!nstd.IsSetLast()      ||  NStr::IsBlank(nstd.GetLast()))      &&
                (!nstd.IsSetFirst()     ||  NStr::IsBlank(nstd.GetFirst()))     &&
                (!nstd.IsSetMiddle()    ||  NStr::IsBlank(nstd.GetMiddle()))    &&
                (!nstd.IsSetFull()      ||  NStr::IsBlank(nstd.GetFull()))      &&
                (!nstd.IsSetInitials()  ||  NStr::IsBlank(nstd.GetInitials()))  &&
                (!nstd.IsSetSuffix()    ||  NStr::IsBlank(nstd.GetSuffix()))    &&
                (!nstd.IsSetTitle()     ||  NStr::IsBlank(nstd.GetTitle()))) {
                return true;
            }
            break;
        }}
            
        case CAuthor::TName::e_Ml:
            str = &GET_FIELD(name, Ml);
            break;
        case CAuthor::TName::e_Str:
            str = &GET_FIELD(name, Str);
            break;
        case CAuthor::TName::e_Consortium:
            str = &GET_FIELD(name, Consortium);
            break;
            
        default:
            break;
    };
    if (str != NULL  &&  NStr::IsBlank(*str)) {
        return true;
    }
    return false;
}

// when we reset author names, we need to put in a place holder - otherwise the ASN.1 becomes invalid
static
void s_ResetAuthorNames (CAuth_list::TNames& names) 
{
    names.Reset();
    list< string > &auth_list = names.SetStr();
    auth_list.clear();
    auth_list.push_back("?");
}

void CNewCleanup_imp::AuthListBC( CAuth_list& al, bool fix_initials )
{
    if ( FIELD_IS_SET(al, Affil) ) {
        AffilBC( GET_MUTABLE(al, Affil) );
        if (s_IsEmpty( GET_FIELD(al, Affil) )) {
            RESET_FIELD(al, Affil);
            ChangeMade(CCleanupChange::eChangePublication);
        }
    }
    if ( FIELD_IS_SET(al, Names) ) {
        typedef CAuth_list::TNames TNames;
        TNames& names = GET_MUTABLE(al, Names);
        switch (names.Which()) {
            case TNames::e_Std:
            {{
                // call BasicCleanup for each CAuthor
                EDIT_EACH_AUTHOR_ON_AUTHLIST( it, al ) {
                    x_AuthorBC(**it, fix_initials);
                    if( s_IsEmpty(**it) ) {
                        ERASE_AUTHOR_ON_AUTHLIST( it, al );
                        ChangeMade(CCleanupChange::eChangePublication);
                    }
                }
                if ( AUTHOR_ON_AUTHLIST_IS_EMPTY(al) ) {
                    s_ResetAuthorNames (names);
                    ChangeMade(CCleanupChange::eChangePublication);
                }
                break;
            }}
            case TNames::e_Ml:
            {{
                if (ConvertAuthorContainerMlToStd(al)) {
                    ChangeMade(CCleanupChange::eChangePublication);
                }
                break;
            }}
            case TNames::e_Str:
            {{
                if (CleanStringContainer( GET_MUTABLE(names, Str) )) {
                    ChangeMade(CCleanupChange::eChangePublication);
                }
                if (names.GetStr().empty()) {
                    s_ResetAuthorNames (names);
                    ChangeMade(CCleanupChange::eChangePublication);
                }
                break;
            }}
            default:
                break;
        }
    }
    // if no remaining authors, put in default author for legal ASN.1
    if (! FIELD_IS_SET(al, Names) ) {
        al.SetNames().SetStr().push_back("?");
        ChangeMade(CCleanupChange::eChangePublication);
    }
}

void CNewCleanup_imp::AffilBC( CAffil& af )
{
    switch (af.Which()) {
        case CAffil::e_Str:
        {{
            CLEAN_STRING_CHOICE(af, Str);
            break;
        }}
        case CAffil::e_Std:
        {{
            CAffil::TStd& std = GET_MUTABLE(af, Std);
            CLEAN_STRING_MEMBER(std, Affil);
            CLEAN_STRING_MEMBER(std, Div);
            CLEAN_STRING_MEMBER(std, City);
            CLEAN_STRING_MEMBER(std, Sub);
            CLEAN_STRING_MEMBER(std, Country);
            if (std.CanGetCountry() ) {
                if ( NStr::EqualNocase(std.GetCountry(), "U.S.A.") ) {
                    SET_FIELD( std, Country, "USA");
                    ChangeMade (CCleanupChange::eChangePublication);
                }
            }
            CLEAN_STRING_MEMBER(std, Street);
            CLEAN_STRING_MEMBER(std, Email);
            CLEAN_STRING_MEMBER(std, Fax);
            CLEAN_STRING_MEMBER(std, Phone);
            CLEAN_STRING_MEMBER(std, Postal_code);
            break;
        }}
        default:
            break;
    }
}

void CNewCleanup_imp::ImprintBC( CImprint& imprint )
{
    if ( FIELD_IS_SET(imprint, Date) ) {
        x_DateBC( GET_MUTABLE(imprint, Date) );
    }

    if ( FIELD_EQUALS(imprint, Pubstatus, ePubStatus_aheadofprint) &&
        (! FIELD_EQUALS(imprint, Prepub, CImprint::ePrepub_in_press) ) )
    {
        if (!imprint.IsSetVolume() || NStr::IsBlank (imprint.GetVolume())
            || !imprint.IsSetPages() || NStr::IsBlank (imprint.GetPages())) {
            SET_FIELD(imprint, Prepub, CImprint::ePrepub_in_press);
            ChangeMade (CCleanupChange::eChangePublication);
        }
    }
    if (FIELD_EQUALS(imprint, Pubstatus, ePubStatus_aheadofprint) &&
        FIELD_EQUALS(imprint, Prepub, CImprint::ePrepub_in_press) )
    {
        if (imprint.IsSetVolume() && !NStr::IsBlank (imprint.GetVolume())
            && imprint.IsSetPages() && !NStr::IsBlank (imprint.GetPages())) {
            RESET_FIELD(imprint, Prepub);
            ChangeMade (CCleanupChange::eChangePublication);
        }
    }

    if (FIELD_EQUALS(imprint, Pubstatus, ePubStatus_epublish) &&
        FIELD_EQUALS(imprint, Prepub, CImprint::ePrepub_in_press) ) {
        RESET_FIELD(imprint, Prepub);
    }
}

typedef pair<string, CRef<CPub> >   TCit;
struct TCitSort {
    bool operator ()(const TCit& c1, const TCit& c2) {
        return ( c1.first < c2.first ) &&  // labels match.
            CitGenTitlesMatch(*c1.second, *c2.second);
    }
    bool CitGenTitlesMatch(const CPub& p1, const CPub& p2) {
        if ( ! p1.IsGen()  || ! p2.IsGen() ) {
            return true;
        }
        const CCit_gen& g1 = p1.GetGen();
        const CCit_gen& g2 = p2.GetGen();
        if ( ! g1.IsSetTitle()  ||  ! g2.IsSetTitle() ) {
            return true;
        }
        return g1.GetTitle() == g2.GetTitle();
    }
};

static
bool cmpSortedvsOld(const TCit& e1, const CRef<CPub>& e2) {
    return e1.second == e2;
}

void CNewCleanup_imp::PubSetBC( CPub_set &pub_set )
{
    // The Pub-set should always be pub. Ignore if not.
    if( ! FIELD_IS( pub_set, Pub ) ) {
        return;
    }

    // sort and unique by putting everything into a set
    // indexed by a label generated for each CPub.
    typedef set<TCit, TCitSort> TCitSet;
    TCitSet cit_set;
    ITERATE (CPub_set::TPub, cit_it, pub_set.GetPub()) {
        string label;
        (*cit_it)->GetLabel(&label, CPub::eBoth, true);
        // the following line may fail due to dups (that's okay)
        cit_set.insert( TCit(label, *cit_it) );
    }
    // Has anything been deleted, or has the order changed?
    if ( cit_set.size() != pub_set.SetPub().size() ||
        ! equal(cit_set.begin(), cit_set.end(), pub_set.SetPub().begin(), cmpSortedvsOld) ) 
    {
        // put everything left back into the feature's citation list.
        pub_set.SetPub().clear();
        ITERATE (TCitSet, citset_it, cit_set) {
            pub_set.SetPub().push_back(citset_it->second);
        }
        ChangeMade(CCleanupChange::eCleanCitonFeat);
    }
}

void CNewCleanup_imp::ImpFeatBC( CImp_feat& imf, CSeq_feat& feat )
{
    CLEAN_STRING_MEMBER_JUNK(imf, Key);
    CLEAN_STRING_MEMBER(imf, Loc);
    CLEAN_STRING_MEMBER(imf, Descr);
    
    if ( FIELD_IS_SET(imf, Key) ) {
        const CImp_feat::TKey& key = GET_FIELD(imf, Key);
        if (key == "allele"  ||  key == "mutation") {
            SET_FIELD(imf, Key, "variation");
            ChangeMade(CCleanupChange::eChangeKeywords);
        } else if ( key == "Import" || key == "virion" ) {
            SET_FIELD(imf, Key, "misc_feature");
            ChangeMade(CCleanupChange::eChangeKeywords);
        } else if ( key == "repeat_unit" ) {
            SET_FIELD(imf, Key, "repeat_region");
            ChangeMade(CCleanupChange::eChangeKeywords);
        } else if ( key == "misc_bind" ) {
            SET_FIELD(imf, Key, "misc_binding");
            ChangeMade(CCleanupChange::eChangeKeywords);
        }
    }

    if (imf.IsSetLoc()  &&  (NStr::Find(imf.GetLoc(), "replace") != NPOS)) {
        x_AddReplaceQual(feat, imf.GetLoc());
        imf.ResetLoc();
        ChangeMade(CCleanupChange::eChangeQualifiers);
    }
}

void CNewCleanup_imp::RegionFeatBC( string &region, CSeq_feat& sf )
{
    if (CleanString(region)) {
        ChangeMade(CCleanupChange::eTrimSpaces);
    }
    if (ConvertDoubleQuotes(region)) {
        ChangeMade(CCleanupChange::eCleanDoubleQuotes);
    }
}

typedef pair<const string, CSeqFeatData::TSite>  TSiteElem;
static const TSiteElem sc_site_map[] = {
    TSiteElem("acetylation", CSeqFeatData::eSite_acetylation),
    TSiteElem("active", CSeqFeatData::eSite_active),
    TSiteElem("amidation", CSeqFeatData::eSite_amidation),
    TSiteElem("binding", CSeqFeatData::eSite_binding),
    TSiteElem("blocked", CSeqFeatData::eSite_blocked),
    TSiteElem("cleavage", CSeqFeatData::eSite_cleavage),
    TSiteElem("dna binding", CSeqFeatData::eSite_dna_binding),
    TSiteElem("dna-binding", CSeqFeatData::eSite_dna_binding),
    TSiteElem("gamma carboxyglutamic acid", CSeqFeatData::eSite_gamma_carboxyglutamic_acid),
    TSiteElem("gamma-carboxyglutamic-acid", CSeqFeatData::eSite_gamma_carboxyglutamic_acid),
    TSiteElem("glycosylation", CSeqFeatData::eSite_glycosylation),
    TSiteElem("hydroxylation", CSeqFeatData::eSite_hydroxylation),
    TSiteElem("inhibit", CSeqFeatData::eSite_inhibit),
    TSiteElem("lipid binding", CSeqFeatData::eSite_lipid_binding),
    TSiteElem("lipid-binding", CSeqFeatData::eSite_lipid_binding),
    TSiteElem("metal binding", CSeqFeatData::eSite_metal_binding),
    TSiteElem("metal-binding", CSeqFeatData::eSite_metal_binding),
    TSiteElem("methylation", CSeqFeatData::eSite_methylation),
    TSiteElem("modifi", CSeqFeatData::eSite_modified),
    TSiteElem("mutagenized", CSeqFeatData::eSite_mutagenized),
    TSiteElem("myristoylation", CSeqFeatData::eSite_myristoylation),
    TSiteElem("nitrosylation", CSeqFeatData::eSite_nitrosylation),
    TSiteElem("np binding", CSeqFeatData::eSite_np_binding),
    TSiteElem("np-binding", CSeqFeatData::eSite_np_binding),
    TSiteElem("oxidative deamination", CSeqFeatData::eSite_oxidative_deamination),
    TSiteElem("oxidative-deamination", CSeqFeatData::eSite_oxidative_deamination),
    TSiteElem("phosphorylation", CSeqFeatData::eSite_phosphorylation),
    TSiteElem("pyrrolidone carboxylic acid", CSeqFeatData::eSite_pyrrolidone_carboxylic_acid),
    TSiteElem("pyrrolidone-carboxylic-acid", CSeqFeatData::eSite_pyrrolidone_carboxylic_acid),
    TSiteElem("signal peptide", CSeqFeatData::eSite_signal_peptide),
    TSiteElem("signal-peptide", CSeqFeatData::eSite_signal_peptide),
    TSiteElem("sulfatation", CSeqFeatData::eSite_sulfatation),
    TSiteElem("transit peptide", CSeqFeatData::eSite_transit_peptide),
    TSiteElem("transit-peptide", CSeqFeatData::eSite_transit_peptide),
    TSiteElem("transmembrane region", CSeqFeatData::eSite_transmembrane_region),
    TSiteElem("transmembrane-region", CSeqFeatData::eSite_transmembrane_region)
};
typedef CStaticArrayMap<const string, CSeqFeatData::TSite, PNocase> TSiteMap;
DEFINE_STATIC_ARRAY_MAP(TSiteMap, sc_SiteMap, sc_site_map);

void CNewCleanup_imp::SiteFeatBC( CSeqFeatData::ESite &site, CSeq_feat& feat )
{
    // If site set to "other", try to extract it from the comment
    if ( FIELD_IS_SET(feat, Comment)  &&
        (site == CSeqFeatData::TSite(0)  ||  site == CSeqFeatData::eSite_other)) 
    {
        // extract if comment starts with any informative possibilities listed in sc_SiteMap
        const string& comment = GET_FIELD(feat, Comment);
        TSiteMap::const_iterator it = sc_SiteMap.lower_bound( comment );
        if( it != sc_SiteMap.begin() && ( it == sc_SiteMap.end() || ! NStr::EqualNocase(comment, it->first) ) ) {
            --it;
        }
        if ( it != sc_SiteMap.end() && NStr::StartsWith(comment, it->first, NStr::eNocase)) {
            feat.SetData().SetSite(it->second);
            ChangeMade(CCleanupChange::eChangeSite);
            // erase the comment if it contains no further useful info aside from the site
            if (NStr::IsBlank(comment, it->first.length())  ||
                NStr::EqualNocase(comment, it->first.length(), NPOS, " site")) {
                    feat.ResetComment();
                    ChangeMade(CCleanupChange::eChangeComment);
            }
        }
    }
}

static
bool s_IsOneMinusStrand(const CSeq_loc& sl)
{
    switch ( sl.Which() ) {
        default:
            return false;
        case CSeq_loc::e_Int:
        case CSeq_loc::e_Pnt:
            return sl.IsReverseStrand();

        case CSeq_loc::e_Packed_int:
            ITERATE(CSeq_loc::TPacked_int::Tdata, i, sl.GetPacked_int().Get()) {
                if (IsReverse((*i)->GetStrand())) {
                    return true;
                }
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            return IsReverse(sl.GetPacked_pnt().GetStrand());
        case CSeq_loc::e_Mix:
            ITERATE(CSeq_loc::TMix::Tdata, i, sl.GetMix().Get()) {
                if (s_IsOneMinusStrand(**i)) {
                    return true;
                }
            }
            break;
        case CSeq_loc::e_Equiv:
            ITERATE(CSeq_loc::TEquiv::Tdata, i, sl.GetEquiv().Get()) {
                if (s_IsOneMinusStrand(**i)) {
                    return true;
                }
            }
            break;
        case CSeq_loc::e_Bond:
            if (sl.GetBond().IsSetA() && sl.GetBond().GetA().IsSetStrand() && IsReverse(sl.GetBond().GetA().GetStrand())) {
                return true;
            }
            if (sl.GetBond().IsSetB()) {
                if (sl.GetBond().IsSetB() && sl.GetBond().GetB().IsSetStrand() && IsReverse(sl.GetBond().GetB().GetStrand())) {
                    return true;
                }
            }
            break;
    }
    return false;
}

void CNewCleanup_imp::SeqLocBC( CSeq_loc &loc )
{
    if (loc.IsWhole()  &&  m_Scope) {
        // change the Seq-loc/whole to a Seq-loc/interval which covers the whole sequence.
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(loc.GetWhole());
        CBioseq_Handle bsh;

        // TODO: this used to be wrapped in a try-catch.
        // could an exception actually occur?
        // we always prefer not to trigger exceptions since
        // they can be *very* slow.
        if( id ) {
            bsh = m_Scope->GetBioseqHandle(*id);
        }
        if (bsh) {
            TSeqPos bs_len = bsh.GetBioseqLength();
            
            loc.SetInt().SetId(*id);
            loc.SetInt().SetFrom(0);
            loc.SetInt().SetTo(bs_len - 1);
            ChangeMade(CCleanupChange::eChangeWholeLocation);
        }
    }
            
    switch (loc.Which()) {
    case CSeq_loc::e_Int :
        x_SeqIntervalBC( GET_MUTABLE(loc, Int) );
        break;
    case CSeq_loc::e_Packed_int :
        {
            CSeq_loc::TPacked_int::Tdata& ints = loc.SetPacked_int().Set();
            NON_CONST_ITERATE(CSeq_loc::TPacked_int::Tdata, interval_it, ints) {
                x_SeqIntervalBC(**interval_it);
            }
            if (ints.size() == 1) {
                CRef<CSeq_interval> int_ref = ints.front();
                loc.SetInt(*int_ref);
                ChangeMade(CCleanupChange::eChangeSeqloc);
            }
        }
        break;
    case CSeq_loc::e_Pnt :
        {
            CSeq_loc::TPnt& pnt = loc.SetPnt();
            
            // change both and both-rev to plus and minus, respectively
            if (pnt.CanGetStrand()) {
                ENa_strand strand = pnt.GetStrand();
                if (strand == eNa_strand_both) {
                    pnt.SetStrand(eNa_strand_plus);
                    ChangeMade(CCleanupChange::eChangeStrand);
                } else if (strand == eNa_strand_both_rev) {
                    pnt.SetStrand(eNa_strand_minus);
                    ChangeMade(CCleanupChange::eChangeStrand);
                }                
            }
        }
        break;
    case CSeq_loc::e_Mix :
        {
            typedef CSeq_loc::TMix::Tdata TMixList;
            // delete Null type Seq-locs from beginning and end of Mix list.

            // deleting from beginning:
            TMixList& sl_list = loc.SetMix().Set();
            TMixList::iterator sl_it = sl_list.begin();
            while (sl_it != sl_list.end()) {
                if ((*sl_it)->IsNull()) {
                    sl_it = sl_list.erase(sl_it);
                    ChangeMade(CCleanupChange::eChangeSeqloc);
                } else {
                    break;
                }
            }

            // deleting from end:
            if( sl_list.size() > 0 ) {
                sl_it = sl_list.end();
                while (sl_it != sl_list.begin()) {
                    --sl_it;
                    if ( ! (*sl_it)->IsNull()) {
                        break;
                    }
                }
                ++sl_it;
                if (sl_it != sl_list.end()) {
                    sl_list.erase(sl_it, sl_list.end());
                    ChangeMade(CCleanupChange::eChangeSeqloc);            
                }

                NON_CONST_ITERATE(TMixList, sl_it, sl_list) {
                    SeqLocBC(**sl_it);
                }
            }

            if (sl_list.size() == 0) {
                loc.SetNull();
                ChangeMade(CCleanupChange::eChangeSeqloc);
            } else if (sl_list.size() == 1) {
                CRef<CSeq_loc> only_sl = sl_list.front();
                loc.Assign(*only_sl);
                ChangeMade(CCleanupChange::eChangeSeqloc);
            }
        }
        break;
    default:
        break;
    }

    /* don't allow negative strand on protein sequences */
    {
        CBioseq_Handle bsh;
        if (m_Scope) {
            bsh = m_Scope->GetBioseqHandle(loc);
        }
        if (bsh  &&  bsh.IsProtein()  && s_IsOneMinusStrand(loc) ) {
            loc.ResetStrand();
            ChangeMade(CCleanupChange::eChangeSeqloc);
        }
    }
}

void CNewCleanup_imp::UserobjBC (
    CUser_object& userobj
)
{
    if ( FIELD_IS_SET(userobj, Type) ) {
        ObjectIdBC( GET_MUTABLE(userobj, Type) );
    }
    
    EDIT_EACH_USERFIELD_ON_USEROBJECT(it, userobj) {
        UserFieldBC(**it);
    }
}

void CNewCleanup_imp::ObjectIdBC( CObject_id &object_id )
{
    if ( FIELD_IS(object_id, Str) ) {
        x_CleanupUserString( GET_MUTABLE(object_id, Str) );
    }
}

void CNewCleanup_imp::UserFieldBC( CUser_field & field )
{
    if ( FIELD_IS_SET(field, Label) ) {
        ObjectIdBC( GET_MUTABLE(field, Label) );
    }
    
    if ( FIELD_IS_SET(field, Data) ) {
        CUser_field::TData& data = GET_MUTABLE(field, Data);
        switch (data.Which()) {
            case NCBI_USERFIELD(Str):
                x_CleanupUserString( GET_MUTABLE(data, Str) );
                break;
            case NCBI_USERFIELD(Strs):
                EDIT_EACH_STRING_IN_VECTOR( it, data.SetStrs()) {
                    x_CleanupUserString(*it);
                }
                break;
            case NCBI_USERFIELD(Object):
                UserobjBC(data.SetObject());
                break;
            case NCBI_USERFIELD(Objects):
                NON_CONST_ITERATE (CUser_field::TData::TObjects, it, data.SetObjects()) {
                    UserobjBC(**it);
                }
                break;
            case NCBI_USERFIELD(Fields):
                NON_CONST_ITERATE (CUser_field::TData::TFields, it, data.SetFields()) {
                    UserFieldBC(**it);
                }
                break;
            default:
                break;
        };
    }
}

void CNewCleanup_imp::SeqannotBC (
    CSeq_annot& sa
)

{
    SWITCH_ON_SEQANNOT_CHOICE (sa) {
        case NCBI_SEQANNOT(Ftable) :
            EDIT_EACH_SEQFEAT_ON_SEQANNOT (sf_itr, sa) {
                CSeq_feat& sf = **sf_itr;
                SeqfeatBC (sf);
            }
            break;
        case NCBI_SEQANNOT(Align) :
            {
            }
            break;
        case NCBI_SEQANNOT(Graph) :
            {
            }
            break;
        case NCBI_SEQANNOT(Ids) :
            {
            }
            break;
        case NCBI_SEQANNOT(Locs) :
            {
            }
            break;
        case NCBI_SEQANNOT(Seq_table) :
            {
            }
            break;
        default :
            break;
    }
}

static bool s_IsJustQuotes (const string& str)

{
    FOR_EACH_CHAR_IN_STRING (str_itr, str) {
        const char& ch = *str_itr;
        if (ch > ' ' && ch != '"' && ch != '\'') return false;
    }
    return true;
}

void CNewCleanup_imp::GBQualBC (
    CGb_qual& gbq
)

{
    CLEAN_STRING_MEMBER_JUNK (gbq, Qual);
    if (! FIELD_IS_SET (gbq, Qual)) {
        SET_FIELD (gbq, Qual, kEmptyStr);
    }

    CLEAN_STRING_MEMBER (gbq, Val);
    if (FIELD_IS_SET (gbq, Val) && s_IsJustQuotes (GET_FIELD (gbq, Val))) {
        RESET_FIELD (gbq, Val);
        ChangeMade (CCleanupChange::eCleanDoubleQuotes);
    }
    if (! FIELD_IS_SET (gbq, Val)) {
        SET_FIELD (gbq, Val, kEmptyStr);
    }

    _ASSERT (FIELD_IS_SET (gbq, Qual) && FIELD_IS_SET (gbq, Val));

    if (NStr::EqualNocase(GET_FIELD(gbq, Qual), "rpt_unit")  ||
               NStr::EqualNocase(GET_FIELD(gbq, Qual), "rpt_unit_range")  ||
               NStr::EqualNocase(GET_FIELD(gbq, Qual), "rpt_unit_seq")) {
        bool range_qual = x_CleanupRptUnit(gbq);
        if (NStr::EqualNocase(GET_FIELD(gbq, Qual), "rpt_unit")) {
            if (range_qual) {
                SET_FIELD( gbq, Qual, "rpt_unit_range" );
            } else {
                SET_FIELD( gbq, Qual, "rpt_unit_seq" );
            }
            ChangeMade(CCleanupChange::eChangeQualifiers);
        }
        if( NStr::EqualNocase(GET_FIELD(gbq, Qual), "rpt_unit_seq") ) {
            string val_no_u = NStr::Replace( GET_FIELD(gbq, Val), "u", "t");
            if (val_no_u != GET_FIELD(gbq, Val)) {
                SET_FIELD( gbq, Val, val_no_u );
                ChangeMade(CCleanupChange::eChangeQualifiers);
            }
        }
    }
    x_ChangeTransposonToMobileElement(gbq);
    x_ChangeInsertionSeqToMobileElement(gbq);
}

CNewCleanup_imp::EAction CNewCleanup_imp::GBQualSeqFeatBC(CGb_qual& gb_qual, CSeq_feat& feat)
{
    if( ! FIELD_IS_SET(feat, Data) ) {
        return eAction_Nothing;
    }
    CSeqFeatData &data = GET_MUTABLE(feat, Data);

    string& qual = GET_MUTABLE(gb_qual, Qual);
    string& val  = GET_MUTABLE(gb_qual, Val);

    if (NStr::EqualNocase(qual, "cons_splice")) {
        return eAction_Erase;
    } else if (NStr::EqualNocase(qual, "replace")) {
        if ( FIELD_IS(data, Imp)  &&
             STRING_FIELD_MATCH( data.GetImp(), Key, "variation") )
        {
                NStr::ToLower(val);
        }
        if ( ! NStr::IsBlank(val) &&  val.find_first_not_of("ACGTUacgtu") == NPOS) {
            NStr::ToLower(val);
            string val_no_u = NStr::Replace(val, "u", "t");
            if (val_no_u != val) {
                SET_FIELD( gb_qual, Val, val_no_u );
                ChangeMade(CCleanupChange::eChangeQualifiers);
            }
        }
    } else if (NStr::EqualNocase(qual, "partial")) {
        feat.SetPartial(true);
        ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "evidence")) {
        /*
            if (NStr::EqualNocase(val, "experimental")) {
                if (!feat.IsSetExp_ev()  ||  feat.GetExp_ev() != CSeq_feat::eExp_ev_not_experimental) {
                    feat.SetExp_ev(CSeq_feat::eExp_ev_experimental);
                }
            } else if (NStr::EqualNocase(val, "not_experimental")) {
                feat.SetExp_ev(CSeq_feat::eExp_ev_not_experimental);
            }
        */
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "exception")) {
        if( ! FIELD_EQUALS(feat, Except, true ) ) {
            SET_FIELD(feat, Except, true);
            ChangeMade(CCleanupChange::eChangeQualifiers);
        }
        if (!NStr::IsBlank(val)  &&  !NStr::EqualNocase(val, "true")) {
            if (!feat.IsSetExcept_text()) {
                feat.SetExcept_text(val);
                ChangeMade(CCleanupChange::eChangeQualifiers);
            }
        }
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "experiment")) {
        if (NStr::EqualNocase(val, "experimental evidence, no additional details recorded")) {
            ChangeMade(CCleanupChange::eChangeQualifiers);
            return eAction_Erase;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "inference")) {
        if (NStr::EqualNocase(val, "non-experimental evidence, no additional details recorded")) {
            ChangeMade(CCleanupChange::eChangeQualifiers);
            return eAction_Erase;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "note")  ||
               NStr::EqualNocase(qual, "notes")  ||
               NStr::EqualNocase(qual, "comment")) {
        if (!feat.IsSetComment()) {
            feat.SetComment(val);
        } else {
            (feat.SetComment() += "; ") += val;
        }
        ChangeMade(CCleanupChange::eChangeComment);
        ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "db_xref")) {
        string tag, db;
        if (NStr::SplitInTwo(val, ":", db, tag)) {
            CRef<CDbtag> dbp(new CDbtag);
            dbp->SetDb(db);
            dbp->SetTag().SetStr(tag);
            feat.SetDbxref().push_back(dbp);
            ChangeMade(CCleanupChange::eChangeDbxrefs);
            return eAction_Erase;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "gdb_xref")) {
        CRef<CDbtag> dbp(new CDbtag);
        dbp->SetDb("GDB");
        dbp->SetTag().SetStr(val);
        feat.SetDbxref().push_back(dbp);
        ChangeMade(CCleanupChange::eChangeDbxrefs);
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "pseudo")) {
        feat.SetPseudo(true);
        ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;  // mark qual for deletion
    } else if ( FIELD_IS(data, Gene)  &&  x_GeneGBQualBC( GET_MUTABLE(data, Gene), gb_qual) == eAction_Erase) {
        return eAction_Erase;  // mark qual for deletion
    } else if ( FIELD_IS(data, Cdregion)  &&  x_SeqFeatCDSGBQualBC(feat, GET_MUTABLE(data, Cdregion), gb_qual) == eAction_Erase ) {
        return eAction_Erase;  // mark qual for deletion
    } else if (data.IsRna()  &&  x_SeqFeatRnaGBQualBC(feat, data.SetRna(), gb_qual) == eAction_Erase) {
        return eAction_Erase;  // mark qual for deletion
    } else if (data.IsProt()  &&  x_ProtGBQualBC(data.SetProt(), gb_qual, eGBQualOpt_normal) == eAction_Erase) {
        return eAction_Erase;  // mark qual for deletion
    } else if (NStr::EqualNocase(qual, "gene")) {
        if (!NStr::IsBlank(val)) {
            CRef<CSeqFeatXref> xref(new CSeqFeatXref);
            xref->SetData().SetGene().SetLocus(val);
            feat.SetXref().push_back(xref);
            ChangeMade(CCleanupChange::eCopyGeneXref);
            return eAction_Erase;  // mark qual for deletion
        }
    } else if (NStr::EqualNocase(qual, "codon_start")) {
        if (!data.IsCdregion()) {
            // not legal on anything but CDS, so remove it
            return eAction_Erase;  // mark qual for deletion
        }
    }

    return eAction_Nothing;
}

// This code is not used since we now erase cons_splice quals,
// but I'm leaving it here in case we change our minds later.
void CNewCleanup_imp::x_CleanupConsSplice(CGb_qual& gbq)

{
    string& val = GET_MUTABLE(gbq, Val);
    
    if (!NStr::StartsWith(val, "(5'site:")) {
        return;
    }
    
    size_t pos = val.find(",3'site:");
    if (pos != NPOS) {
        val.insert(pos + 1, " ");
        ChangeMade(CCleanupChange::eChangeQualifiers);
    }
}

static
bool s_HasUpper (const string &val)
{
    FOR_EACH_CHAR_IN_STRING( str_itr, val ) {
        if( isupper(*str_itr) ) {
            return true;
        }
    }
    return false;
}

// return true if the val indicates this a range qualifier "[0-9]+..[0-9]+"

bool CNewCleanup_imp::x_CleanupRptUnit(CGb_qual& gbq)
{
    CGb_qual::TVal& val = GET_MUTABLE(gbq, Val);
    
    if (NStr::IsBlank(val)) {
        return false;
    }
    if( string::npos != val.find_first_not_of("ACGTUNacgtun0123456789()") ) {
        if (s_HasUpper(val)) {
            val = NStr::ToLower(val);
            ChangeMade(CCleanupChange::eChangeQualifiers);
            return false;
        }
    } 
    bool digits1 = false, sep = false, digits2 = false;
    string cleaned_val;
    string::const_iterator it = val.begin();
    string::const_iterator end = val.end();
    while (it != end) {
        while (it != end  &&  (*it == '('  ||  *it == ')'  ||  *it == ',')) {
            cleaned_val += *it++;
        }
        while (it != end  &&  isspace((unsigned char)(*it))) {
            ++it;
        }
        while (it != end  &&  isdigit((unsigned char)(*it))) {
            cleaned_val += *it++;
            digits1 = true;
        }
        if (it != end  &&  (*it == '.'  ||  *it == '-')) {
            while (it != end  &&  (*it == '.'  ||  *it == '-')) {
                ++it;
            }
            cleaned_val += "..";
            sep = true;
        }
        while (it != end  &&  isspace((unsigned char)(*it))) {
            ++it;
        }
        while (it != end  &&  isdigit((unsigned char)(*it))) {
            cleaned_val += *it++;
            digits2 = true;
        }
        while (it != end  &&  isspace((unsigned char)(*it))) {
            ++it;
        }
        if (it != end) {
            char c = *it;
            if (c != '('  &&  c != ')'  &&  c != ','  &&  c != '.'  &&
                !isspace((unsigned char) c)  &&  !isdigit((unsigned char) c)) {
                if (s_HasUpper(val)) {
                    val = NStr::ToLower(val);
                    ChangeMade(CCleanupChange::eChangeQualifiers);
                }
                return false;
            }
        }
    }
    if (val != cleaned_val) {
        val = cleaned_val;
        ChangeMade(CCleanupChange::eChangeQualifiers);
    }
    
    return  (digits1 && sep && digits2);
}

void CNewCleanup_imp::x_ChangeTransposonToMobileElement(CGb_qual& gbq)
//
//  As of Dec 2006, "transposon" is no longer legal as a qualifier. The replacement
//  qualifier is "mobile_element". In addition, the value has to be massaged to
//  indicate "integron" or "transposon".
//
{
    static const string integronValues[] = {
        "class I integron",
        "class II integron",
        "class III integron",
        "class 1 integron",
        "class 2 integron",
        "class 3 integron"
    };
    static const string* endIntegronValues 
        = integronValues + sizeof(integronValues)/sizeof(*integronValues);

    if (NStr::EqualNocase( GET_FIELD(gbq, Qual), "transposon")) {
        SET_FIELD( gbq, Qual, "mobile_element");

        // If the value is one of the IntegronValues, change it to "integron: class XXX":
        const string* pValue = std::find(integronValues, endIntegronValues, GET_FIELD(gbq, Val) );
        if ( pValue != endIntegronValues ) {
            string::size_type cutoff = pValue->find( " integron" );
            _ASSERT( cutoff != string::npos ); // typo in IntegronValues?
            SET_FIELD( gbq, Val, string("integron: ") + pValue->substr(0, cutoff) );
        }
        // Otherwise, just prefix it with "transposon: ":
        else {
            SET_FIELD( gbq, Val, string("transposon: ") + GET_FIELD(gbq, Val) );
        }
        
        ChangeMade(CCleanupChange::eChangeQualifiers);
    }
}

void CNewCleanup_imp::x_ChangeInsertionSeqToMobileElement(CGb_qual& gbq)
//
//  As of Dec 2006, "insertion_seq" is no longer legal as a qualifier. The replacement
//  qualifier is "mobile_element". In addition, the value has to be massaged to
//  reflect the "insertion_seq".
//
{
    if (NStr::EqualNocase( GET_FIELD(gbq, Qual), "insertion_seq")) {
        gbq.SetQual("mobile_element");
        gbq.SetVal( string("insertion sequence:") + GET_FIELD(gbq, Val) );
        ChangeMade(CCleanupChange::eChangeQualifiers);
    }
}

static bool s_IsCompoundRptTypeValue( 
    const string& value )
//
//  Format of compound rpt_type values: (value[,value]*)
//
//  These are internal to sequin and are in theory cleaned up before the material
//  is released. However, some compound values have escaped into the wild and have 
//  not been retro-fixed yet (as of 2006-03-17).
//
{
    return ( NStr::StartsWith( value, '(' ) && NStr::EndsWith( value, ')' ) );
};

static void s_ExpandThisQual( 
    CSeq_feat::TQual& quals,        // the list of CGb_qual's.
    CSeq_feat::TQual::iterator& it, // points to the one qual we might expand.
    CSeq_feat::TQual& new_quals )    // new quals that will need to be inserted
//
//  Rules for "rpt_type" qualifiers (as of 2006-03-07):
//
//  There can be multiple occurrences of this qualifier, and we need to keep them 
//  all.
//  The value of this qualifier can also be a *list of values* which is *not* 
//  conforming to the ASN.1 and thus needs to be cleaned up. 
//
//  The cleanup entails turning the list of values into multiple occurrences of the 
//  given qualifier, each occurrence taking one of the values in the original 
//  list.
//
{
    CGb_qual& qual = **it;
    string  qual_type = qual.GetQual();
    string& val = qual.SetVal();
    if ( ! s_IsCompoundRptTypeValue( val ) ) {
        //
        //  nothing to do ...
        //
        return;
    }

    //
    //  Generate list of cleaned up values. Fix original qualifier and generate 
    //  list of new qualifiers to be added to the original list:
    //    
    vector< string > newValues;
    string valueList = val.substr(1, val.length() - 2);
    NStr::Tokenize(valueList, ",", newValues);
    
    qual.SetVal( newValues[0] );
    
    for ( size_t i=1; i < newValues.size(); ++i ) {
        CRef< CGb_qual > newQual( new CGb_qual() );
        newQual->SetQual( qual_type );
        newQual->SetVal( newValues[i] );
        new_quals.push_back( newQual ); 
    }
};

void CNewCleanup_imp::x_ExpandCombinedQuals(CSeq_feat::TQual& quals)
{
    CSeq_feat::TQual    new_quals;
    NON_CONST_ITERATE (CSeq_feat::TQual, it, quals) {
        CGb_qual& gb_qual = **it;
                
        string& qual = GET_MUTABLE(gb_qual, Qual);
        
        if (NStr::EqualNocase(qual, "rpt_type")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        } else if (NStr::EqualNocase(qual, "rpt_unit")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        } else if (NStr::EqualNocase(qual, "usedin")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        } else if (NStr::EqualNocase(qual, "old_locus_tag")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        } else if (NStr::EqualNocase(qual, "compare")) {
            s_ExpandThisQual( quals, it, new_quals ); 
        }
    }
    
    if ( ! new_quals.empty() ) {
        quals.insert(quals.end(), new_quals.begin(), new_quals.end());
        ChangeMade(CCleanupChange::eChangeQualifiers);
    }
}

CNewCleanup_imp::EAction 
CNewCleanup_imp::x_GeneGBQualBC( CGene_ref& gene, const CGb_qual& gb_qual )
{
    const string& qual = GET_FIELD(gb_qual, Qual);
    const string& val  = GET_FIELD(gb_qual, Val);

    bool change_made = false;
    if (NStr::EqualNocase(qual, "map")) {
        if (! (gene.IsSetMaploc()  ||  NStr::IsBlank(val)) ) {
            change_made = true;
            gene.SetMaploc(val);
        }
    } else if (NStr::EqualNocase(qual, "allele")) {
        if ( ! (gene.IsSetAllele()  ||  NStr::IsBlank(val)) ) {
            change_made = true;
            gene.SetAllele(val);
        }
    } else if (NStr::EqualNocase(qual, "locus_tag")) {
        if ( ! (gene.IsSetLocus_tag()  ||  NStr::IsBlank(val)) ) {
            change_made = true;
            gene.SetLocus_tag(val);
        }
    } else if (NStr::EqualNocase(qual, "gene")  &&  ! NStr::IsBlank(val)) {
        change_made = true;
        if ( ! gene.IsSetLocus() ) {
            gene.SetLocus(val);
        } else if (gene.GetLocus() != val) {
            CGene_ref::TSyn::const_iterator syn_it = 
                find(gene.GetSyn().begin(), gene.GetSyn().end(), val);
            if (syn_it == gene.GetSyn().end()) {
                gene.SetSyn().push_back(val);
            }            
        }
    }
    if (change_made) {
        ChangeMade(CCleanupChange::eChangeQualifiers);
    }

    return ( change_made ? eAction_Erase : eAction_Nothing );
}

CNewCleanup_imp::EAction
CNewCleanup_imp::x_SeqFeatCDSGBQualBC(CSeq_feat& feat, CCdregion& cds, const CGb_qual& gb_qual)
{
    const string& qual = gb_qual.GetQual();
    const string& val  = gb_qual.GetVal();
    
    // transl_except qual -> Cdregion.code_break
    if (NStr::EqualNocase(qual, "transl_except")) {
        return x_ParseCodeBreak(feat, cds, val);
    }

    // codon_start qual -> Cdregion.frame
    if (NStr::EqualNocase(qual, "codon_start")) {
        CCdregion::TFrame frame = GET_FIELD(cds, Frame);
        CCdregion::TFrame new_frame = CCdregion::TFrame(NStr::StringToNumeric(val));
        if (new_frame == CCdregion::eFrame_one  ||
            new_frame == CCdregion::eFrame_two  ||
            new_frame == CCdregion::eFrame_three) {
            if (frame == CCdregion::eFrame_not_set  ||
                ( FIELD_IS_SET( feat, Pseudo) && GET_FIELD( feat, Pseudo) && ! FIELD_IS_SET(feat, Product) )) {
                cds.SetFrame(new_frame);
                ChangeMade(CCleanupChange::eChangeQualifiers);
            }
            return eAction_Erase;
        }
    }

    // transl_table qual -> Cdregion.code
    if (NStr::EqualNocase(qual, "transl_table")) {
        if ( FIELD_IS_SET(cds, Code) ) {
            const CCdregion::TCode& code = GET_FIELD(cds, Code);
            int transl_table = 1;
            ITERATE (CCdregion::TCode::Tdata, it, code.Get()) {
                if ( FIELD_IS(**it, Id)  &&  GET_FIELD(**it, Id) != 0) {
                    transl_table = GET_FIELD(**it, Id);
                    break;
                }
            }
            
            if (NStr::EqualNocase(NStr::UIntToString(transl_table), val)) {
                return eAction_Erase;
            }
        } else {
            int new_val = NStr::StringToNumeric(val);
            if (new_val > 0) {
                CRef<CGenetic_code::C_E> gc(new CGenetic_code::C_E);
                SET_FIELD(*gc, Id, new_val);
                cds.SetCode().Set().push_back(gc);
                
                ChangeMade(CCleanupChange::eChangeGeneticCode);
                return eAction_Erase;
            }
        }
    }

    // look for qualifiers that should be applied to protein feature
    // note - this should be moved to the "indexed" portion of basic cleanup,
    // because it needs to locate another sequence and feature
    if (NStr::Equal(qual, "product") || NStr::Equal (qual, "function") || NStr::Equal (qual, "EC_number")
        || NStr::Equal(qual, "activity") || NStr::Equal (qual, "prot_note")) 
    {

        // get protein sequence for product
        CRef<CSeq_feat> prot_feat;
        CRef<CProt_ref> prot_ref;
        CFeat_CI feat_ci;
        // try to get existing prot_feat
        CBioseq_Handle prot_handle;
        if ( FIELD_IS_SET(feat, Product) ) {
            prot_handle = m_Scope->GetBioseqHandle(feat.GetProduct());
        }
        if( prot_handle ) {
            // find main protein feature
            SAnnotSelector sel(CSeqFeatData::eSubtype_prot);
            feat_ci = CFeat_CI(prot_handle, sel);
            if( feat_ci ) {
                prot_feat.Reset( new CSeq_feat );
                prot_feat->Assign(feat_ci->GetOriginalFeature());
                prot_ref.Reset( &prot_feat->SetData().SetProt() );
            }
        }
        bool push_back_xref_on_success = false;
        CRef<CSeqFeatXref> xref;
        if ( ! prot_ref ) {
            // otherwise make cross reference
            prot_ref.Reset( new CProt_ref );

            // see if this seq-feat already has a prot xref
            EDIT_EACH_SEQFEATXREF_ON_SEQFEAT( xref_iter, feat ) {
                if( (*xref_iter)->IsSetData() && (*xref_iter)->GetData().IsProt() ) {
                    xref = *xref_iter;
                }
            }
            // seq-feat has no prot xref. We make our own.
            if ( ! xref ) {
                xref.Reset( new CSeqFeatXref );
                xref->SetData().SetProt( *prot_ref );
                // we will push the xref onto the feat if the add was successful
                push_back_xref_on_success = true;
            }
            prot_ref.Reset( &xref->SetData().SetProt() );
        }

        // replacement prot feature
        EAction action = eAction_Nothing;

        if (NStr::Equal(qual, "prot_note") && prot_feat ) {
            if (!prot_feat->IsSetComment() || NStr::IsBlank (prot_feat->GetComment())) {
                SET_FIELD( *prot_feat, Comment, val);
            } else {
                SET_FIELD( *prot_feat, Comment, (prot_feat->GetComment() + "; " + val) );
            }
            ChangeMade (CCleanupChange::eChangeComment);
            action = eAction_Erase;
        } else {
            action = x_ProtGBQualBC( *prot_ref, gb_qual, eGBQualOpt_CDSMode );
        }

        if (action == eAction_Erase) {
            if( feat_ci && prot_feat ) {
                CSeq_feat_EditHandle edit_feature_handle(feat_ci->GetSeq_feat_Handle());
                edit_feature_handle.Replace(*prot_feat);
            }
            if( push_back_xref_on_success ) {
                feat.SetXref().push_back( xref );
            }
            return eAction_Erase;
        }
        /* else if (!NStr::Equal(qual, "prot_note")) {
        bool found = false;
        bool change_made = false;
        // find or create prot xref for feature
        EDIT_EACH_SEQFEATXREF_ON_SEQFEAT (it, feat) {
        if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
        found = true;
        change_made = x_ProtGBQualBC ((*it)->SetData().SetProt(), gb_qual);
        }
        }
        if (!found) {
        change_made = x_ProtGBQualBC (feat.SetProtXref(), gb_qual);
        ChangeMade (CCleanupChange::eAddProtXref);                  
        }
        if (change_made) {
        return eAction_Erase;
        }
        } */

        return eAction_Nothing;
    }

    // TODO: should this be uncommented?
    /*if (NStr::EqualNocase(qual, "translation")) {
        return eAction_Erase;
    }*/
    return eAction_Nothing;
}

typedef pair <const char *, const int> TTrnaKey;

static const TTrnaKey trna_key_to_subtype [] = {
    TTrnaKey ( "Ala",            'A' ),
    TTrnaKey ( "Alanine",        'A' ),
    TTrnaKey ( "Arg",            'R' ),
    TTrnaKey ( "Arginine",       'R' ),
    TTrnaKey ( "Asn",            'N' ),
    TTrnaKey ( "Asp",            'D' ),
    TTrnaKey ( "Asp or Asn",     'B' ),
    TTrnaKey ( "Asparagine",     'N' ),
    TTrnaKey ( "Aspartate",      'D' ),
    TTrnaKey ( "Aspartic Acid",  'D' ),
    TTrnaKey ( "Asx",            'B' ),
    TTrnaKey ( "Cys",            'C' ),
    TTrnaKey ( "Cysteine",       'C' ),
    TTrnaKey ( "fMet",           'M' ),
    TTrnaKey ( "Gln",            'Q' ),
    TTrnaKey ( "Glu",            'E' ),
    TTrnaKey ( "Glu or Gln",     'Z' ),
    TTrnaKey ( "Glutamate",      'E' ),
    TTrnaKey ( "Glutamic Acid",  'E' ),
    TTrnaKey ( "Glutamine",      'Q' ),
    TTrnaKey ( "Glx",            'Z' ),
    TTrnaKey ( "Gly",            'G' ),
    TTrnaKey ( "Glycine",        'G' ),
    TTrnaKey ( "His",            'H' ),
    TTrnaKey ( "Histidine",      'H' ),
    TTrnaKey ( "Ile",            'I' ),
    TTrnaKey ( "Isoleucine",     'I' ),
    TTrnaKey ( "Leu",            'L' ),
    TTrnaKey ( "Leu or Ile",     'J' ),
    TTrnaKey ( "Leucine",        'L' ),
    TTrnaKey ( "Lys",            'K' ),
    TTrnaKey ( "Lysine",         'K' ),
    TTrnaKey ( "Met",            'M' ),
    TTrnaKey ( "Methionine",     'M' ),
    TTrnaKey ( "OTHER",          'X' ),
    TTrnaKey ( "Phe",            'F' ),
    TTrnaKey ( "Phenylalanine",  'F' ),
    TTrnaKey ( "Pro",            'P' ),
    TTrnaKey ( "Proline",        'P' ),
    TTrnaKey ( "Pyl",            'O' ),
    TTrnaKey ( "Pyrrolysine",    'O' ),
    TTrnaKey ( "Sec",            'U' ),
    TTrnaKey ( "Selenocysteine", 'U' ),
    TTrnaKey ( "Ser",            'S' ),
    TTrnaKey ( "Serine",         'S' ),
    TTrnaKey ( "Ter",            '*' ),
    TTrnaKey ( "TERM",           '*' ),
    TTrnaKey ( "Termination",    '*' ),
    TTrnaKey ( "Thr",            'T' ),
    TTrnaKey ( "Threonine",      'T' ),
    TTrnaKey ( "Trp",            'W' ),
    TTrnaKey ( "Tryptophan",     'W' ),
    TTrnaKey ( "Tyr",            'Y' ),
    TTrnaKey ( "Tyrosine",       'Y' ),
    TTrnaKey ( "Val",            'V' ),
    TTrnaKey ( "Valine",         'V' ),
    TTrnaKey ( "Xle",            'J' ),
    TTrnaKey ( "Xxx",            'X' )
};

typedef CStaticArrayMap <const char*, const int, PNocase_CStr> TTrnaMap;
DEFINE_STATIC_ARRAY_MAP(TTrnaMap, sm_TrnaKeys, trna_key_to_subtype);

class PNocase_Char
{
public:
    bool operator()( const char ch1, const char ch2 ) const {
        return toupper(ch1) < toupper(ch2);
    }
};

// This maps in the opposite direction of sm_TrnaKeys
class CAminoAcidCharToSymbol : public multimap<char, const char*, PNocase_Char> 
{
public:
    CAminoAcidCharToSymbol( const TTrnaKey keys[], int num_keys )
    {
        int ii = 0;
        for( ; ii < num_keys; ++ii ) {
            insert(pair<char, const char*>( keys[ii].second, keys[ii].first ));
        }
    }
};
const static CAminoAcidCharToSymbol sm_TrnaInverseKeys
    ( trna_key_to_subtype, 
      (sizeof(trna_key_to_subtype) / sizeof(trna_key_to_subtype[0])) );

// str is input AND output
static CRef<CTrna_ext> s_ParseTRnaString (string &str)
{
    CRef<CTrna_ext> trna (new CTrna_ext());
    
    if (NStr::IsBlank (str)) return trna;

    /* try the whole string for product */
    TTrnaMap::const_iterator t_iter = sm_TrnaKeys.find (str.c_str ());
    if (t_iter != sm_TrnaKeys.end ()) {
        CRef<CTrna_ext::TAa> aa(new CTrna_ext::TAa);
        aa->SetIupacaa (t_iter->second);
        trna->SetAa(*aa);
        str = "";
        return trna;
    }
    
    if (!NStr::StartsWith (str, "tRNA-")) {
        return trna;
    } else {
        str = str.substr (5);
        string::size_type aa_end = NStr::Find(str, "(");
        

        string abbrev = "";
        if (aa_end == string::npos) {
            abbrev = str;
        } else {
            abbrev = str.substr (0, aa_end);
        }
        NStr::TruncateSpacesInPlace (abbrev);
        TTrnaMap::const_iterator t_iter = sm_TrnaKeys.find (abbrev.c_str ());
        if (t_iter == sm_TrnaKeys.end ()) {
            // couldn't find abbreviation in list
            return trna;
        }
        CRef<CTrna_ext::TAa> aa(new CTrna_ext::TAa);
        aa->SetIupacaa (t_iter->second);

        trna->SetAa( *aa);
        if (aa_end == string::npos) {
            // abbreviation was all there was to find
            str = "";
            return trna;
        }
        // continue parsing
        str = str.substr(aa_end + 1);
        
        string::size_type codons_end = NStr::Find (str, ")");
        if (codons_end != string::npos) {
            string codon_list = str.substr (0, codons_end);
            vector<string> codons;
            vector<int> codon_vals;
            NStr::Tokenize(codon_list, ",", codons);
            bool codons_ok = true;
            for (unsigned int i = 0; i < codons.size() && codons_ok; i++) {
                NStr::TruncateSpacesInPlace (codons[i]);
                if (codons[i].length() != 3) {
                    codons_ok = false;
                } else {
                    NStr::ToUpper (codons[i]);
                    NStr::ReplaceInPlace (codons[i], "T", "U");
                    if( codons[i].find_first_not_of( "AUGC" ) != string::npos ) {
                        codons_ok = false;
                    }
                    if (codons_ok) {
                        int residue = CGen_code_table::CodonToIndex (codons[i]);
                        if (residue < 0) {
                            codons_ok = false;
                        } else {
                            codon_vals.push_back (residue);
                        }
                    }
                }
            }
            if (codons_ok) {
                CTrna_ext::TCodon& real_codons = trna->SetCodon();
                copy( codon_vals.begin(), codon_vals.end(), back_inserter(real_codons) );
                str = str.substr (codons_end + 1);
            }
        }
    }
    return trna;
}

static CRef<CTrna_ext> s_ParseTRnaFromAnticodonString (const string &str, CSeq_feat& feat, CScope *scope)
{
    CRef<CTrna_ext> trna (new CTrna_ext());
    
    if (NStr::IsBlank (str)) return trna;

    if (NStr::StartsWith (str, "(pos:")) {
        string::size_type pos_end = NStr::Find (str, ")");
        if (pos_end != string::npos) {
            string pos_str = str.substr (5, pos_end - 5);
            string::size_type aa_start = NStr::FindNoCase (pos_str, "aa:");
            if (aa_start != string::npos) {
                string abbrev = pos_str.substr (aa_start + 3);
                TTrnaMap::const_iterator t_iter = sm_TrnaKeys.find (abbrev.c_str ());
                if (t_iter == sm_TrnaKeys.end ()) {
                    // unable to parse
                    return trna;
                }
                CRef<CTrna_ext::TAa> aa(new CTrna_ext::TAa);
                aa->SetIupacaa (t_iter->second);
                trna->SetAa(*aa);
                pos_str = pos_str.substr (0, aa_start);
                NStr::TruncateSpacesInPlace (pos_str);
                if (NStr::EndsWith (pos_str, ",")) {
                    pos_str = pos_str.substr (0, pos_str.length() - 1);
                }
            }
            CRef<CSeq_loc> anticodon = ReadLocFromText (pos_str, feat.GetLocation().GetId(), scope);
            if (anticodon == NULL) {
                trna->ResetAa();
            } else {
                trna->SetAnticodon(*anticodon);
            }
        }
    }
    return trna;        
}

CNewCleanup_imp::EAction 
CNewCleanup_imp::x_SeqFeatRnaGBQualBC(CSeq_feat& feat, CRNA_ref& rna, CGb_qual& gb_qual)
{
    const string& qual = GET_FIELD(gb_qual, Qual);

    bool is_std_name = NStr::EqualNocase(qual, "standard_name");
    if (NStr::EqualNocase(qual, "product")  ||  (is_std_name  &&  ! m_IsEmblOrDdbj )) {
        if (! FIELD_IS_SET(gb_qual, Val) ) {
            return eAction_Nothing;
        }
        if (  FIELD_IS_SET(rna, Type) ) {
            if ( GET_FIELD(rna, Type) == NCBI_RNAREF(unknown)) {
                SET_FIELD( rna, Type, NCBI_RNAREF(other) );
                ChangeMade(CCleanupChange::eChangeKeywords);
            }
        } else {
            SET_FIELD( rna, Type, NCBI_RNAREF(other));
            ChangeMade(CCleanupChange::eChangeKeywords);
        }
        _ASSERT(rna.IsSetType());

        CRNA_ref::TType type = GET_FIELD(rna, Type);
        
        if (type == NCBI_RNAREF(other)  &&  is_std_name) {
            return eAction_Nothing;
        }

        if (type == NCBI_RNAREF(other) && FIELD_IS_SET(rna, Ext) && rna.GetExt().IsName()
            && NStr::Equal (rna.GetExt().GetName(), "misc_RNA")
            && NStr::Equal(qual, "product")) {
            if (STRING_FIELD_MATCH( gb_qual, Val, "its1")
                || STRING_FIELD_MATCH( gb_qual, Val, "its 1")
                || STRING_FIELD_MATCH( gb_qual, Val, "Ribosomal DNA internal transcribed spacer 1")
                || STRING_FIELD_MATCH( gb_qual, Val, "internal transcribed spacer 1 (ITS1)")) {
                SET_FIELD( gb_qual, Val, "internal transcribed spacer 1");
                ChangeMade(CCleanupChange::eChangeQualifiers);
            } else if ( STRING_FIELD_MATCH( gb_qual, Val, "its2")
                || STRING_FIELD_MATCH( gb_qual, Val, "its 2")
                || STRING_FIELD_MATCH( gb_qual, Val, "Ribosomal DNA internal transcribed spacer 2")
                || STRING_FIELD_MATCH( gb_qual, Val, "internal transcribed spacer 2 (ITS2)")) {
                SET_FIELD( gb_qual, Val, "internal transcribed spacer 2");
                ChangeMade(CCleanupChange::eChangeQualifiers);
            } else if ( STRING_FIELD_MATCH( gb_qual, Val, "its3")
                || STRING_FIELD_MATCH( gb_qual, Val, "its 3")
                || STRING_FIELD_MATCH( gb_qual, Val, "Ribosomal DNA internal transcribed spacer 3")
                || STRING_FIELD_MATCH( gb_qual, Val, "internal transcribed spacer 3 (ITS3)")) {
                SET_FIELD( gb_qual, Val, "internal transcribed spacer 3");
                ChangeMade(CCleanupChange::eChangeQualifiers);
            }
        }


        if (type == NCBI_RNAREF(tRNA) ) {
            if (rna.IsSetExt() && rna.GetExt().IsName() ) {
                string comment = rna.GetExt().GetName();
                CRef<CTrna_ext> trna_from_name = s_ParseTRnaString (comment);
                x_SeqFeatTRNABC( feat, *trna_from_name );
                if (trna_from_name->IsSetAa() && NStr::IsBlank (comment)) {
                    rna.SetExt().SetTRNA (*trna_from_name);
                    ChangeMade (CCleanupChange::eChange_tRna);
                    if (trna_from_name->GetAa().GetIupacaa() == 'M'
                        && NStr::Find (rna.GetExt().GetName(), "fMet") != string::npos) {
                            if (!feat.IsSetComment()) {
                                feat.SetComment ("fMet");
                            } else if (NStr::Find (feat.GetComment(), "fMet") == string::npos) {
                                feat.SetComment (feat.GetComment() + "; fMet");
                            }
                            ChangeMade (CCleanupChange::eChangeComment);
                    }
                }
            } else {                
                string comment = gb_qual.GetVal();
                CRef<CTrna_ext> trna_from_name = s_ParseTRnaString (comment);
                x_SeqFeatTRNABC( feat, *trna_from_name );
                if (trna_from_name->IsSetAa() && NStr::IsBlank (comment)) {
                    rna.SetExt().SetTRNA (*trna_from_name);
                    ChangeMade (CCleanupChange::eChange_tRna);
                    if (trna_from_name->GetAa().GetIupacaa() == 'M' 
                        && NStr::Find (gb_qual.GetVal(), "fMet") != string::npos) {
                        if (!feat.IsSetComment()) {
                            feat.SetComment ("fMet");
                        } else if (NStr::Find (feat.GetComment(), "fMet") == string::npos) {
                            feat.SetComment (feat.GetComment() + "; fMet");
                        }
                        ChangeMade (CCleanupChange::eChangeComment);
                    }
                    return eAction_Erase;
                }
            }
        } else if (rna.IsSetExt() && !rna.GetExt().IsName()) {
            return eAction_Nothing;
        /* } else if (type == NCBI_RNAREF(other)) { // TODO?
            // new convention follows ASN.1 spec comments, allows new RNA types
            return eAction_Nothing; */
        } else {
            // subsequent /product now added to comment
            const string *p_rna_name = &kEmptyStr;
            const string &val = gb_qual.GetVal();
            if (rna.IsSetExt() && rna.GetExt().IsName()) {
                p_rna_name = &rna.GetExt().GetName();
            }
            if (NStr::IsBlank (*p_rna_name)) {
                rna.SetExt().SetName(val);
                ChangeMade (CCleanupChange::eChange_rRna);
                return eAction_Erase;
            } else {
                if (NStr::EqualNocase(*p_rna_name, val)) {
                    return eAction_Erase;
                } else if (type == NCBI_RNAREF(miscRNA)) {
                    return eAction_Nothing;
                } else {
                    if (!feat.IsSetComment() || NStr::IsBlank (feat.GetComment())) {
                        feat.SetComment(val);
                    } else {
                        feat.SetComment (feat.GetComment() + "; " + val);
                    }
                    ChangeMade (CCleanupChange::eChangeComment);
                    return eAction_Erase;
                }
            }
        }

    }
    if (NStr::EqualNocase(qual, "anticodon")) {
        if (!rna.IsSetType()) {
            rna.SetType(CRNA_ref::eType_tRNA);
            ChangeMade(CCleanupChange::eChangeKeywords);
        }
        _ASSERT(rna.IsSetType());
        CRNA_ref::TType type = rna.GetType();
        if (type == CRNA_ref::eType_unknown) {
            rna.SetType(CRNA_ref::eType_tRNA);
            ChangeMade(CCleanupChange::eChangeKeywords);
        } else if (type != CRNA_ref::eType_tRNA) {
            return eAction_Nothing;
        }
        if (!rna.IsSetExt()) {
            rna.SetExt().SetTRNA();
        }
        if ( rna.IsSetExt()  &&
             rna.GetExt().Which() == NCBI_RNAEXT(TRNA) ) {
            
            CRef<CTrna_ext> trna = s_ParseTRnaFromAnticodonString (gb_qual.GetVal(), feat, m_Scope);
            x_SeqFeatTRNABC( feat, *trna );
            if (trna->IsSetAa() || trna->IsSetAnticodon()) {
                /* don't apply at all if there are conflicts */
                bool apply_aa = false;
                bool apply_anticodon = false;
                bool ok_to_apply = true;
                
                /* look for conflict with aa */
                if (trna->IsSetAa()) {
                    if (rna.GetExt().GetTRNA().IsSetAa()) {
                        if (trna->GetAa().GetIupacaa() != rna.GetExt().GetTRNA().GetAa().GetIupacaa()) {
                            ok_to_apply = false;
                        }
                    } else {
                        apply_aa = true;
                    }
                }
                /* look for conflict with anticodon */
                if (trna->IsSetAnticodon()) {
                    if (rna.GetExt().GetTRNA().IsSetAnticodon()) {
                        if (sequence::Compare(rna.GetExt().GetTRNA().GetAnticodon(), trna->GetAnticodon(), m_Scope) != sequence::eSame) {
                            ok_to_apply = false;
                        }
                    } else {
                        apply_anticodon = true;
                    }
                }

                if (ok_to_apply) {
                    if (apply_aa) {
                        rna.SetExt().SetTRNA().SetAa().SetIupacaa(trna->GetAa().GetIupacaa());
                        ChangeMade (CCleanupChange::eChange_tRna);
                    }
                    if (apply_anticodon) {
                        CRef<CSeq_loc> anticodon(new CSeq_loc());
                        anticodon->Add (trna->GetAnticodon());
                        rna.SetExt().SetTRNA().SetAnticodon(*anticodon);
                        ChangeMade (CCleanupChange::eChangeAnticodon);
                    }
                    return eAction_Erase;
                }
            }
        }
    }
    return eAction_Nothing;
}

CNewCleanup_imp::EAction CNewCleanup_imp::x_ParseCodeBreak(const CSeq_feat& feat, CCdregion& cds, const string& str)
{
    string::size_type aa_pos = NStr::Find(str, "aa:");
    string::size_type len = 0;
    string::size_type loc_pos, end_pos;
    char protein_letter = 'X';
    CRef<CSeq_loc> break_loc;
    
    if (aa_pos == string::npos) {
        aa_pos = NStr::Find (str, ",");
        if (aa_pos != string::npos) {
            aa_pos = NStr::Find (str, ":", aa_pos);
        }
        if (aa_pos != string::npos) {
            aa_pos ++;
        }
    } else {
        aa_pos += 3;
    }

    if (aa_pos != string::npos) {    
        while (aa_pos < str.length() && isspace (str[aa_pos])) {
            aa_pos++;
        }
        while (aa_pos + len < str.length() && isalpha (str[aa_pos + len])) {
            len++;
        }
        if (len != 0) {    
            protein_letter = ValidAminoAcid(str.substr(aa_pos, len));
        }
    }
    
    loc_pos = NStr::Find (str, "(pos:");
    if (loc_pos == string::npos) {
        return eAction_Nothing;
    }
    loc_pos += 5;
    while (loc_pos < str.length() && isspace (str[loc_pos])) {
        loc_pos++;
    }
    end_pos = NStr::Find (str, ",", loc_pos);
    if (end_pos == string::npos) {
        break_loc = ReadLocFromText (str.substr(loc_pos), feat.GetLocation().GetId(), m_Scope);
    } else {
        break_loc = ReadLocFromText (str.substr(loc_pos, end_pos - loc_pos), feat.GetLocation().GetId(), m_Scope);
    }
    
    if (break_loc == NULL 
        || sequence::Compare (*break_loc, feat.GetLocation(), m_Scope) != sequence::eContained
        || (break_loc->IsInt() && sequence::GetLength(*break_loc, m_Scope) != 3)) {
        return eAction_Nothing;
    }
    
    // need to build code break object and add it to coding region
    CRef<CCode_break> newCodeBreak(new CCode_break());
    CCode_break::TAa& aa = newCodeBreak->SetAa();
    aa.SetNcbieaa(protein_letter);
    newCodeBreak->SetLoc (*break_loc);

    CCdregion::TCode_break& orig_list = cds.SetCode_break();
    orig_list.push_back(newCodeBreak);
    
    ChangeMade(CCleanupChange::eChangeCodeBreak);
    
    return eAction_Erase;
}

CNewCleanup_imp::EAction CNewCleanup_imp::
x_ProtGBQualBC(CProt_ref& prot, const CGb_qual& gb_qual, EGBQualOpt opt )
{
    const string& qual = gb_qual.GetQual();
    const string& val  = gb_qual.GetVal();

    REMOVE_IF_EMPTY_NAME_ON_PROTREF(prot);

    if (NStr::EqualNocase(qual, "product")  ||  NStr::EqualNocase(qual, "standard_name")) {
        if ( opt == eGBQualOpt_CDSMode || !prot.IsSetName()  ||  NStr::IsBlank(prot.GetName().front())) {
            if( opt == eGBQualOpt_normal ) {
                prot.SetName().push_back(val);
            } else {
                prot.SetName().push_front(val);
            }
            ChangeMade(CCleanupChange::eChangeQualifiers);
            if (prot.IsSetDesc()) {
                const CProt_ref::TDesc& desc = prot.GetDesc();
                FOR_EACH_NAME_ON_PROTREF (it, prot) {
                    if (NStr::EqualNocase(desc, *it)) {
                        prot.ResetDesc();
                        ChangeMade(CCleanupChange::eChangeQualifiers);
                        break;
                    }
                }
            }
            return eAction_Erase;
        }
    } else if (NStr::EqualNocase(qual, "function")) {
        ADD_STRING_TO_LIST( prot.SetActivity(), val );
        ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;
    } else if (NStr::EqualNocase(qual, "EC_number")) {
        ADD_STRING_TO_LIST( prot.SetEc(), val );
        ChangeMade(CCleanupChange::eChangeQualifiers);
        return eAction_Erase;
    }

    return eAction_Nothing;
}

void CNewCleanup_imp::x_FlattenPubEquiv(CPub_equiv& pub_equiv)
{
    CPub_equiv::Tdata& data = pub_equiv.Set();
    
    EDIT_EACH_PUB_ON_PUBEQUIV(pub_iter, pub_equiv ) {
        if( FIELD_IS(**pub_iter, Equiv) ) {
            CPub_equiv& equiv = GET_MUTABLE(**pub_iter, Equiv);
            x_FlattenPubEquiv(equiv);
            copy(equiv.Set().begin(), equiv.Set().end(), back_inserter(data));
            ERASE_PUB_ON_PUBEQUIV( pub_iter, pub_equiv );
            ChangeMade(CCleanupChange::eChangePublication);
        }
    }
}

void CNewCleanup_imp::x_DateBC( CDate& date )
{
    if( FIELD_IS(date, Std) ) {
        x_DateStdBC( GET_MUTABLE(date, Std) );
    }
}

void CNewCleanup_imp::x_DateStdBC( CDate_std& date )
{
    if ( FIELD_UNSET_OR_OUT_OF_RANGE(date, Month, 1, 12) ) {
        RESET_FIELD(date, Month);
        ChangeMade(CCleanupChange::eCleanupDate);
    }

    // Maybe we should have the max range set on a per-month basis? (e.g. 30 days for April).
    // ( This could get complex with leap years and such. )
    if ( FIELD_UNSET_OR_OUT_OF_RANGE(date, Day, 1, 31) ) {
        RESET_FIELD(date, Day);
        ChangeMade(CCleanupChange::eCleanupDate);
    }

    if ( FIELD_UNSET_OR_OUT_OF_RANGE(date, Second, 0, 59) ) {
        RESET_FIELD(date, Second);
        ChangeMade(CCleanupChange::eCleanupDate);
    }

    if ( FIELD_UNSET_OR_OUT_OF_RANGE(date, Minute, 0, 59) ) {
        RESET_FIELD(date, Minute);
        RESET_FIELD(date, Second);
        ChangeMade(CCleanupChange::eCleanupDate);
    }
    
    if ( FIELD_UNSET_OR_OUT_OF_RANGE(date, Hour, 0, 23) ) {
        RESET_FIELD(date, Hour);
        RESET_FIELD(date, Minute);
        RESET_FIELD(date, Second);
        ChangeMade(CCleanupChange::eCleanupDate);
    }
}

void CNewCleanup_imp::x_AuthorBC( CAuthor& au, bool fix_initials )
{
    if ( FIELD_IS_SET(au, Name) ) {
        x_PersonIdBC( GET_MUTABLE(au, Name), fix_initials);
    }
}

void CNewCleanup_imp::x_PersonIdBC( CPerson_id& pid, bool fix_initials )
{
    switch (pid.Which()) {
        case NCBI_PERSONID(Name):
            x_NameStdBC( GET_MUTABLE(pid, Name), fix_initials );
            break;
        case NCBI_PERSONID(Ml):
            TRUNCATE_CHOICE_SPACES(pid, Ml);
            break;
        case NCBI_PERSONID(Str):
            TRUNCATE_CHOICE_SPACES(pid, Str);
            break;
        case NCBI_PERSONID(Consortium):
            TRUNCATE_CHOICE_SPACES(pid, Consortium);
            break;
        default:
            break;
    }
}

void CNewCleanup_imp::x_NameStdBC ( CName_std& name, bool fix_initials )
{
    // before cleanup, get information from initials
    TRUNCATE_SPACES(name, Last);
    TRUNCATE_SPACES(name, First);
    TRUNCATE_SPACES(name, Middle);
    TRUNCATE_SPACES(name, Full);
    TRUNCATE_SPACES(name, Initials);
    TRUNCATE_SPACES(name, Suffix);
    TRUNCATE_SPACES(name, Title);
    if ( FIELD_IS_SET(name, Initials) ) {
        if (! FIELD_IS_SET(name, Suffix) ) {
            x_ExtractSuffixFromInitials(name);
        }
        x_FixEtAl(name);
    }

    // do strings cleanup
    CLEAN_STRING_MEMBER(name, Last);
    CLEAN_STRING_MEMBER(name, First);
    CLEAN_STRING_MEMBER(name, Middle);
    CLEAN_STRING_MEMBER(name, Full);
    CLEAN_STRING_MEMBER(name, Initials);
    CLEAN_STRING_MEMBER(name, Suffix);
    CLEAN_STRING_MEMBER(name, Title);

    if ( FIELD_IS_SET(name, Suffix) ) {
        x_FixSuffix(name);
    }

    // regenerate initials from first name
    if (fix_initials) {
        x_FixInitials(name);
    }
}

// mapping of wrong suffixes to the correct ones.
typedef pair<string, string> TStringPair;
static const TStringPair bad_sfxs[] = {
    TStringPair("1d"  , "I"),
    TStringPair("1st" , "I"),
    TStringPair("2d"  , "II"),
    TStringPair("2nd" , "II"),
    TStringPair("3d"  , "III"),
    TStringPair("3rd" , "III"),
    TStringPair("4th" , "IV"),
    TStringPair("5th" , "V"),
    TStringPair("6th" , "VI"),
    //TStringPair("I."  , "I"), // presumably commented out since it resembles initials
    TStringPair("II." , "II"),
    TStringPair("III.", "III"),
    TStringPair("IV." , "IV"),
    TStringPair("Jr"  , "Jr."),
    TStringPair("Sr"  , "Sr."),    
    //TStringPair("V."  , "V"), // presumably commented out since it resembles initials
    TStringPair("VI." , "VI")
};
typedef CStaticArrayMap<string, string> TSuffixMap;
DEFINE_STATIC_ARRAY_MAP(TSuffixMap, sc_BadSuffixes, bad_sfxs);

void CNewCleanup_imp::x_ExtractSuffixFromInitials(CName_std& name)
{
    _ASSERT( FIELD_IS_SET(name, Initials)  &&  ! FIELD_IS_SET(name, Suffix) );

    string& initials = GET_MUTABLE(name, Initials);
    const size_t len = initials.length();

    if (initials.find('.') == NPOS) {
        return;
    }

    // look for standard suffixes in initials
    typedef CName_std::TSuffixes TSuffixes;
    const TSuffixes& suffixes = CName_std::GetStandardSuffixes();
    TSuffixes::const_iterator best = suffixes.end();
    ITERATE (TSuffixes, it, suffixes) {
        if (NStr::EndsWith(initials, *it)) {
            if (best == suffixes.end()) {
                best = it;
            } else if (best->length() < it->length()) {
                best = it;
            }
        }
    }
    if (best != suffixes.end()) {
        initials.resize(len - best->length());
        SET_FIELD(name, Suffix, *best);
        ChangeMade(CCleanupChange::eNormalizeAuthors);
        return;
    }

    // look for bad suffixes in initials
    ITERATE (TSuffixMap, it, sc_BadSuffixes) {
        if (NStr::EndsWith(initials, it->first)  &&
            initials.length() > it->first.length()) {
            initials.resize(len - it->first.length());
            SET_FIELD(name, Suffix, it->second);
            ChangeMade(CCleanupChange::eNormalizeAuthors);
            return;
        }
    }
}

void CNewCleanup_imp::x_FixEtAl(CName_std& name)
{
    _ASSERT(name.IsSetInitials());

    if ( STRING_FIELD_MATCH(name, Last, "et") &&
        (NStr::Equal(name.GetInitials(), "al")  ||  NStr::Equal(name.GetInitials(), "al."))  &&
        (!name.IsSetFirst()  ||  NStr::IsBlank(name.GetFirst()))) 
    {
        RESET_FIELD( name, First );
        RESET_FIELD( name, Initials );
        SET_FIELD( name, Last, "et al." );
        ChangeMade(CCleanupChange::eNormalizeAuthors);
    }
}

void CNewCleanup_imp::x_FixSuffix(CName_std& name)
{
    _ASSERT( FIELD_IS_SET(name, Suffix) );

    CName_std::TSuffix& suffix = GET_MUTABLE(name, Suffix);

    ITERATE (TSuffixMap, it, sc_BadSuffixes) {
        if (NStr::EqualNocase(suffix, it->first)) {
            SET_FIELD(name, Suffix, it->second);
            ChangeMade(CCleanupChange::eNormalizeAuthors);
            break;
        }
    }
}

static bool s_FixInitials(string& initials)
{
    string fixed;

    string::iterator it = initials.begin();
    while (it != initials.end()  &&  !isalpha((unsigned char)(*it))) {  // skip junk
        ++it;
    }
    char prev = '\0';
    while (it != initials.end()) {
        char c = *it;
    
        // commas become periods
        if (c == ',') {
            c = '.';
        }

        if (isalpha((unsigned char) c)) {
            if (prev != '\0'  &&  isupper((unsigned char) c)  &&  isupper((unsigned char) prev)) {
                fixed += '.';
            }
        } else if (c == '-') {
            if (prev != '\0'  &&  prev != '.') {
                fixed += '.';
            }
        } else if (c == '.') {
            if (prev == '-'  ||  prev == '.') {
                ++it;
                continue;
            }
        } else {
            ++it;
            continue;
        }
        fixed += c;

        prev = c;
        ++it;
    }
    // cut off extraneous dash at end
    if ( NStr::EndsWith( fixed, '-') ) {
        fixed.resize(fixed.length() - 1);
    }
    if (initials != fixed) {
        initials = fixed;
        return true;
    }
    return false;
}

CName_std::TInitials s_GetInitialsFromFirst(const CName_std& name)
{
    _ASSERT( FIELD_IS_SET(name, First) );
    
    const CName_std::TFirst& first = GET_FIELD(name, First);
    if (NStr::IsBlank(first)) {
        return kEmptyStr;
    }
    
    bool up = FIELD_IS_SET(name, Initials)  &&  
        ! GET_FIELD(name, Initials).empty()  &&  
        isupper((unsigned char) name.GetInitials()[0]);
    
    string initials;
    string::const_iterator end = first.end();
    string::const_iterator it = first.begin();
    while (it != end) {
        // skip "junk"
        for ( ; it != end  &&  !isalpha((unsigned char)(*it)) ; ++it ) {
            // regard words in parentheses as nick names. Nick names do not contribute
            // to the initials
            if( *it == '(' ) {
                it = find( it, end, ')' );
            }
        }
        // copy the first character of each word
        if (it != end) {
            if (!initials.empty()) {
                char c = *initials.rbegin();
                if (isupper((unsigned char)(*it))  &&  c != '.'  &&  c != '-') {
                    initials += '.';
                }
            }
            initials += up ? toupper((unsigned char)(*it)) : *it;
        }
        // skip the rest of the word
        while (it != end  &&  !isspace((unsigned char)(*it))  &&  *it != ','  &&  *it != '-') {
            ++it;
        }
        if (it != end  &&  *it == '-') {
            initials += '.';
            initials += *it++;
        }
        up = false;
    }
    if (!initials.empty()  &&  !(*initials.rbegin() == '-')) {
        initials += '.';
    }
    
    return initials;
}

void CNewCleanup_imp::x_FixInitials(CName_std& name)
{
    if (name.IsSetInitials()) {
        if (s_FixInitials(name.SetInitials())) {
            ChangeMade(CCleanupChange::eNormalizeAuthors);
        }
        
        if (name.SetInitials().empty()) {
            RESET_FIELD(name, Initials);
            ChangeMade(CCleanupChange::eNormalizeAuthors);
        }
    }
    if ( FIELD_IS_SET(name, First)  &&  ! FIELD_IS_SET(name, Initials) ) {
        string new_initials = s_GetInitialsFromFirst(name);
        if ( ! new_initials.empty() ) {
            SET_FIELD(name, Initials, new_initials);
            ChangeMade(CCleanupChange::eNormalizeAuthors);
        }
        return;
    }
    if (! FIELD_IS_SET(name, Initials) ) {
        return;
    }

    if (! FIELD_IS_SET(name, Suffix) ) {
        CName_std::TInitials& initials = GET_MUTABLE(name, Initials);
        if (NStr::EndsWith (initials, ".Jr.")) {
            initials.erase (initials.end() - 3);
            SET_FIELD(name, Suffix, "Jr.");
        } else if (NStr::EndsWith (initials, ".Sr.")) {
            initials.erase (initials.end() - 3);
            SET_FIELD(name, Suffix, "Sr.");
        }
    }

    if (! FIELD_IS_SET(name, First) ) {
        return;
    }
    _ASSERT(FIELD_IS_SET(name, First)  &&  FIELD_IS_SET(name, Initials));

    CName_std::TInitials& initials = GET_MUTABLE(name, Initials);
    string f_initials = s_GetInitialsFromFirst(name);
    if (initials == f_initials) {
        return;
    }
    string::iterator it1 = f_initials.begin();
    string::iterator it2 = initials.begin();
    while (it1 != f_initials.end()  &&  it2 != initials.end()  &&
        toupper((unsigned char)(*it1)) == toupper((unsigned char)(*it2))) {
        ++it1;
        ++it2;
    }
    if (it2 != initials.end()  &&  it1 != f_initials.end()  &&  *it1 == '.'  &&  islower((unsigned char)(*it2))) {
        f_initials.erase(it1);
    }
    while (it2 != initials.end()) {
        f_initials += *it2++;
    }
    if (f_initials != initials) {
        initials.swap(f_initials);
        ChangeMade(CCleanupChange::eNormalizeAuthors);
    }
}

void CNewCleanup_imp::x_CleanupUserString(string& str)
{
    if (!NStr::IsBlank(str)) {
        const size_t old_str_size = str.size();
        NStr::TruncateSpacesInPlace(str);
        if (old_str_size != str.size()) {
            ChangeMade(CCleanupChange::eTrimSpaces);
        }
    }
}

void CNewCleanup_imp::x_AddReplaceQual(CSeq_feat& feat, const string& str)
{
    if (!NStr::EndsWith(str, ')')) {
        return;
    }

    SIZE_TYPE start = str.find_first_of('\"');
    if (start != NPOS) {
        SIZE_TYPE end = str.find_first_of('\"', start + 1);
        if (end != NPOS) {
            string replace_val = str.substr(start + 1, (end - start) - 1);
            NStr::ToLower(replace_val);
            feat.AddQualifier("replace", replace_val );
            ChangeMade(CCleanupChange::eChangeQualifiers);
        }
    }
}

void CNewCleanup_imp::x_SeqIntervalBC( CSeq_interval & seq_interval )
{
    // Fix backwards intervals
    if ( seq_interval.CanGetFrom()  &&  seq_interval.CanGetTo()  &&  seq_interval.GetFrom() > seq_interval.GetTo()) {
        swap(seq_interval.SetFrom(), seq_interval.SetTo());
        ChangeMade(CCleanupChange::eChangeSeqloc);
    }
    // change bad strand values.
    if (seq_interval.CanGetStrand()) {
        ENa_strand strand = seq_interval.GetStrand();
        if (strand == eNa_strand_both) {
            seq_interval.SetStrand(eNa_strand_plus);
            ChangeMade(CCleanupChange::eChangeStrand);
        } else if (strand == eNa_strand_both_rev) {
            seq_interval.SetStrand(eNa_strand_minus);
            ChangeMade(CCleanupChange::eChangeStrand);
        }        
    }
}

void CNewCleanup_imp::x_SplitDbtag( CDbtag &dbt, vector< CRef< CDbtag > > & out_new_dbtags )
{
    // check the common case of nothing to split
    if( ! dbt.IsSetTag() || ! dbt.GetTag().IsStr() ) {
        return;
    }
    if( dbt.GetTag().GetStr().find(":") == string::npos ) {
        return;
    }

    // split by colon and generate new tags
    vector<string> tags;
    NStr::Tokenize( dbt.GetTag().GetStr(), ":", tags );
    _ASSERT( tags.size() >= 2 );

    // treat the CDbtag argument as the first of the new CDbtags
    dbt.SetTag().SetStr( tags.front() );
    vector<string>::const_iterator str_iter = tags.begin() + 1;
    for( ; str_iter != tags.end(); ++str_iter ) {
        CRef<CDbtag> new_tag( new CDbtag );
        new_tag->Assign( dbt );
        new_tag->SetTag().SetStr( *str_iter );

        // just to make sure
        DbtagBC( *new_tag );

        out_new_dbtags.push_back( new_tag );
    }
}

class CCharInSet {
public:
    CCharInSet( const string &list_of_characters ) {
        copy( list_of_characters.begin(), list_of_characters.end(),
            inserter( char_set, char_set.begin() ) );
    }

    bool operator()( const char ch ) {
        return ( char_set.find(ch) != char_set.end() );
    }

private:
    set<char> char_set;
};

static
void s_TokenizeTRnaString (const string &tRNA_string, list<string> &out_string_list )
{
    out_string_list.clear();
    if ( tRNA_string.empty() ) return;

    // SGD Tx(NNN)c or Tx(NNN)c#, where x is the amino acid, c is the chromosome (A-P, Q for mito),
    // and optional # is presumably for individual tRNAs with different anticodons and the same
    // amino acid.
    static CRegexp valid_sgd_regex("^[Tt][A-Za-z]\\(...\\)[A-Za-z]\\d?\\d?$");
    if ( valid_sgd_regex.IsMatch(tRNA_string) ) {
        // parse SGD tRNA anticodon
        out_string_list.push_back(kEmptyStr);
        string &new_SGD_tRNA_anticodon = out_string_list.back();
        string raw_codon_part = tRNA_string.substr(3,3);
        NStr::ToUpper( raw_codon_part );
        string reverse_complement;
        CSeqManip::ReverseComplement( raw_codon_part, CSeqUtil::e_Iupacna, 0, 3, reverse_complement );
        new_SGD_tRNA_anticodon = string("(") + reverse_complement + ')';

        // parse SGD tRNA amino acid
        out_string_list.push_back(tRNA_string.substr(1,1));
        return;
    }

    string tRNA_string_copy = tRNA_string;
    // Note that we do NOT remove "*", since it might be a terminator tRNA symbol
    replace_if( tRNA_string_copy.begin(), tRNA_string_copy.end(), 
        CCharInSet("-,;:()=\'_~"), ' ' );

    vector<string> tRNA_tokens;
    // " \t\n\v\f\r" are the standard whitespace chars
    // ( source: http://www.cplusplus.com/reference/clibrary/cctype/isspace/ )
    NStr::Tokenize( tRNA_string_copy, " \t\n\v\f\r", tRNA_tokens, NStr::eMergeDelims );

    EDIT_EACH_STRING_IN_VECTOR( tRNA_token_iter, tRNA_tokens ) {
        string &tRNA_token = *tRNA_token_iter;
        // remove initial "tRNA", if any
        if ( NStr::StartsWith(tRNA_token, "tRNA", NStr::eNocase) ) {
            tRNA_token = tRNA_token.substr(3);
        }
        static CRegexp threeLettersPlusDigits("^[A-Za-z][A-Za-z][A-Za-z]\\d*$");
        if (! tRNA_token.empty() ) {
            if ( threeLettersPlusDigits.IsMatch(tRNA_token) ) {
                tRNA_token = tRNA_token.substr(0, 3);
            }
            out_string_list.push_back(tRNA_token);
        }
    }
}

static
char s_FindTrnaAA( const string &str )
{
    if ( str.empty() ) return '\0';
    string tmp = str;
    NStr::TruncateSpacesInPlace(tmp);
    
    if( tmp.length() == 1 ) {
        // if the string is a valid one-letter code, just return that
        const char aminoAcidLetter = toupper(tmp[0]);
        if( sm_TrnaInverseKeys.find(aminoAcidLetter) != sm_TrnaInverseKeys.end() ) {
            return aminoAcidLetter;
        }
    } else {
        // translate 3-letter codes and full-names to one-letter codes
        TTrnaMap::const_iterator trna_iter = sm_TrnaKeys.find (tmp.c_str ());
        if( trna_iter != sm_TrnaKeys.end() ) {
            return trna_iter->second;
        }
    }

    return '\0';
}

static const char *codonLetterExpand [] =
{
  "?", "A", "C", "AC",
  "G", "AG", "CG", "ACG",
  "T", "AT", "CT", "ACT",
  "GT", "AGT", "CGT", "ACGT",
  NULL
};

static
bool s_ParseDegenerateCodon( CTrna_ext & tRNA, string & codon )
{
  const static string intToChr = "?ACMGRSVTWYHKDBN";

  if( codon.length() < 3 ) {
      return false;
  }

  // the first two have to be real nucleotides
  const string::size_type first_bad_char = codon.find_first_not_of("ACGT");
  if( first_bad_char != string::npos && first_bad_char < 2 ) {
      return false;
  }

  int idx = intToChr.find( codon [2] );
  if (idx == (int)string::npos ) return false;

  const char *expanded_codon_letter = codonLetterExpand [idx];
  const char *iter = expanded_codon_letter;
  char ch = *iter;
  int tRNA_codon_idx = 0;
  codon.erase(3);
  while ( *iter != '\0' && tRNA_codon_idx < 6 ) {
    codon [2] = ch;
    tRNA.SetCodon().push_back( CGen_code_table::CodonToIndex(codon) ); // TODO: make sure Seq_code_iupacna

    // prepare for next iteration
    iter++;
    ch = *iter;
    tRNA_codon_idx++;
  }

  return true;
}

// based on C's ParseTRnaString (TODO: remove that comment)
static 
char s_ParseSeqFeatTRnaString( const string &comment, bool *out_justTrnaText, string &tRNA_codon, bool noSingleLetter )
{
    if (out_justTrnaText != NULL) {
        *out_justTrnaText = false;
    }
    tRNA_codon.clear();

    if ( comment.empty() ) return '\0';

    CRef<CTrna_ext> tr( new CTrna_ext );

    char aa = '\0';
    list<string> head;
    s_TokenizeTRnaString (comment, head);
    bool justt = true;
    FOR_EACH_STRING_IN_LIST( head_iter, head ) {
        if( aa != 0 && aa != 'A' ) {
            break;
        }
        const string &str = *head_iter;
        char curraa = '\0';
        if (noSingleLetter && str.length() == 1) {
            curraa = '\0';
        } else {
            curraa = s_FindTrnaAA (str);
        }
        if (curraa != '\0') {
            if (aa == '\0' || aa == 'A') {
                aa = curraa;
            }
        } else if ( ! NStr::EqualNocase ("tRNA", str) &&
            ! NStr::EqualNocase ("transfer", str) &&
            ! NStr::EqualNocase ("RNA", str) &&
            ! NStr::EqualNocase ("product", str) ) 
        {
            if ( str.length() == 3) {
                tRNA_codon = str;
                NStr::ReplaceInPlace( tRNA_codon, "U", "T" );
                if (s_ParseDegenerateCodon ( *tr, tRNA_codon)) {
                    tRNA_codon.clear();
                    copy( tr->GetCodon().begin(), tr->GetCodon().end(), back_inserter(tRNA_codon) );
                } else {
                    justt = false;
                }
            } else {
                justt = false;
            }
        }
    }
    FOR_EACH_STRING_IN_LIST( head_iter, head ) {
        const string &str = *head_iter;
        char curraa = s_FindTrnaAA (str);
        if (curraa != '\0') {
        } else if ( ! NStr::EqualNocase ("tRNA", str) &&
            ! NStr::EqualNocase ("transfer", str) &&
            ! NStr::EqualNocase ("RNA", str) &&
            ! NStr::EqualNocase ("product", str) ) {
                if ( str.length() == 3) {
                    tRNA_codon = str;
                    NStr::ReplaceInPlace( tRNA_codon, "U", "T" );
                    if (s_ParseDegenerateCodon( *tr, tRNA_codon) ) {
                        tRNA_codon.clear();
                        copy( tr->GetCodon().begin(), tr->GetCodon().end(), back_inserter(tRNA_codon) );
                    } else {
                        justt = false;
                    }
                } else {
                    justt = false;
                }
        }
    }

    if (justt) {
        if( comment.find_first_of("0123456789") != string::npos ) {
            justt = false;
        }
    }
    if (out_justTrnaText != NULL) {
        *out_justTrnaText = justt;
    }
    return aa;
}

static
bool s_CodonCompare( int codon1, int codon2 ) {
    return (codon1 < codon2);
}

static
bool s_CodonEqual( int codon1, int codon2 ) {
    return (codon1 == codon2);
}

void CNewCleanup_imp::x_SeqFeatTRNABC( CSeq_feat& feat, CTrna_ext & tRNA )
{
    const string &comment = ( FIELD_IS_SET(feat, Comment) ? GET_FIELD(feat, Comment) : kEmptyStr );

    // look for tRNA-OTHER with actual amino acid in comment
    if ( FIELD_IS_SET(feat, Comment) && CODON_ON_TRNAEXT_IS_EMPTY_OR_UNSET(tRNA) ) {
        bool okayToFree = true;
        string codon;

        if ( NStr::StartsWith(comment, "codon recognized: ", NStr::eNocase) ) {
            codon = comment.substr( 18 );
        } else if (NStr::StartsWith (comment, "codons recognized: ", NStr::eNocase) ) {
            codon = comment.substr( 19 );
        }
        NStr::ToUpper( codon );
        if ( ! codon.empty() ) {
            if ( codon.length() > 3 && codon [3] == ';') {
                codon.erase( 3 );
                okayToFree = false;
            }
            if ( codon.length() == 3) {
                NStr::ReplaceInPlace( codon, "U", "T" );
                if (s_ParseDegenerateCodon (tRNA, codon)) {
                    if (okayToFree) {
                        RESET_FIELD(feat, Comment);
                        ChangeMade(CCleanupChange::eChangeComment);
                    } else {
                        string new_comment = GET_FIELD(feat, Comment).substr(22);
                        NStr::TruncateSpacesInPlace(new_comment);
                        RESET_FIELD(feat, Comment);
                        if ( ! new_comment.empty() ) {
                            SET_FIELD(feat, Comment, new_comment );
                        }
                        ChangeMade(CCleanupChange::eChangeComment);
                    }
                }
            }
        }
    }

    if( tRNA.IsSetAa() && tRNA.GetAa().IsIupacaa() ) {
        const int old_value = tRNA.GetAa().GetIupacaa();
        tRNA.SetAa().SetNcbieaa( old_value );
    }

    if ( FIELD_IS_SET(feat, Comment) ) {
        char aa = '\0';
        char new_aa = '\0';
        if( tRNA.IsSetAa() ) {
            switch( tRNA.GetAa().Which() ) {
            case CTrna_ext::C_Aa::e_Iupacaa:
                aa = tRNA.GetAa().GetIupacaa();
                CSeqConvert::Convert( &aa, CSeqUtil::e_Iupacaa, 0, 1, &new_aa, CSeqUtil::e_Ncbieaa );
                break;
            case CTrna_ext::C_Aa::e_Ncbieaa:
                aa = tRNA.GetAa().GetNcbieaa();
                CSeqConvert::Convert( &aa, CSeqUtil::e_Ncbieaa, 0, 1, &new_aa, CSeqUtil::e_Ncbieaa );
                break;
            case CTrna_ext::C_Aa::e_Ncbi8aa:
                aa = tRNA.GetAa().GetNcbi8aa();
                CSeqConvert::Convert( &aa, CSeqUtil::e_Ncbi8aa, 0, 1, &new_aa, CSeqUtil::e_Ncbieaa );
                break;
            case CTrna_ext::C_Aa::e_Ncbistdaa:
                aa = tRNA.GetAa().GetNcbistdaa();
                CSeqConvert::Convert( &aa, CSeqUtil::e_Ncbistdaa, 0, 1, &new_aa, CSeqUtil::e_Ncbieaa );
                break;
            default:
                break;
            }
        }
        bool justTrnaText = false;
        string tRNA_codon;
        if (aa != 'X') {
            new_aa = s_ParseSeqFeatTRnaString ( comment, &justTrnaText, tRNA_codon, true);
            if (aa == '\0' && new_aa != '\0') {
                aa = new_aa;
                tRNA.SetAa().SetNcbieaa( new_aa );
            }
            if (aa != '\0' && aa == new_aa) {
                if (justTrnaText) {
                    CTrna_ext::TCodon & arg_codon = GET_MUTABLE( tRNA, Codon );
                    copy( tRNA_codon.begin() + arg_codon.size(), tRNA_codon.end(), back_inserter(arg_codon) );
                    if ( comment != "fMet" ) {
                        RESET_FIELD(feat, Comment);
                    }
                }
            }
        } else {
            aa = s_ParseSeqFeatTRnaString ( comment, &justTrnaText, tRNA_codon, true);
            if (aa != '\0') {
                tRNA.SetAa().SetNcbieaa( aa );
                if (justTrnaText) {
                    CTrna_ext::TCodon & arg_codon = tRNA.SetCodon();
                    copy( tRNA_codon.begin() + arg_codon.size(), tRNA_codon.end(), back_inserter(arg_codon) );
                    if ( comment != "fMet" ) {
                        RESET_FIELD(feat, Comment);
                    }
                }
            }
        }
    }

    if (! CODON_ON_TRNAEXT_IS_SORTED(tRNA, s_CodonCompare)) {
        SORT_CODON_ON_TRNAEXT(tRNA, s_CodonCompare);
        ChangeMade(CCleanupChange::eChange_tRna);
    }

    if( ! CODON_ON_TRNAEXT_IS_UNIQUE(tRNA, s_CodonEqual) ) {
        UNIQUE_CODON_ON_TRNAEXT(tRNA, s_CodonEqual);
        ChangeMade(CCleanupChange::eChange_tRna);
    }

    REMOVE_IF_EMPTY_CODON_ON_TRNAEXT(tRNA);
}

static
void s_ParsePCRComponent(vector<string> &out_list, const string *component)
{
    out_list.clear();

    if( component == NULL ) return;
    if ( component->empty() ) return;

    string component_copy = *component; //copy so we can modify it
    // Remove enclosing parens, if any
    const string::size_type len = component_copy.length();
    if ( len > 1 && component_copy[0] == '(' && component_copy[len - 1] == ')' && component_copy.find('(', 1) == string::npos ) {
        component_copy = component_copy.substr( 1, component_copy.length() - 2 );
    }

    NStr::Tokenize( component_copy, string(","), out_list );
    EDIT_EACH_STRING_IN_VECTOR( str_iter, out_list ) {
        NStr::TruncateSpacesInPlace( *str_iter );
    }
}

class CPCRParsedSet {
public:
    CPCRParsedSet( 
        const string * fwd_seq,
        const string * rev_seq,
        const string * fwd_name,
        const string * rev_name ) :
    m_Fwd_seq(      fwd_seq  == NULL ? kEmptyStr : *fwd_seq),
        m_Rev_seq(  rev_seq  == NULL ? kEmptyStr : *rev_seq ),
        m_Fwd_name( fwd_name == NULL ? kEmptyStr : *fwd_name ),
        m_Rev_name( rev_name == NULL ? kEmptyStr : *rev_name ),
        m_Original_order( ++ms_Next_original_order ) { }

    const string &GetFwdSeq() const { return m_Fwd_seq; }
    const string &GetRevSeq() const { return m_Rev_seq; }
    const string &GetFwdName() const { return m_Fwd_name; }
    const string &GetRevName() const { return m_Rev_name; }

    bool operator <( const CPCRParsedSet &rhs ) {
        const int fwd_seq_comparison = NStr::CompareNocase( m_Fwd_seq, rhs.m_Fwd_seq );
        if( fwd_seq_comparison != 0 ) return fwd_seq_comparison;
        const int rev_seq_comparison = NStr::CompareNocase( m_Rev_seq, rhs.m_Rev_seq );
        if( rev_seq_comparison != 0 ) return rev_seq_comparison;
        const int fwd_name_comparison = NStr::CompareNocase( m_Fwd_name, rhs.m_Fwd_name );
        if( fwd_name_comparison != 0 ) return fwd_name_comparison;
        const int rev_name_comparison = NStr::CompareNocase( m_Rev_name, rhs.m_Rev_name );
        if( rev_name_comparison != 0 ) return rev_name_comparison;
        // last resort
        return m_Original_order - rhs.m_Original_order;
    }

private:
    string m_Fwd_seq;
    string m_Rev_seq;
    string m_Fwd_name;
    string m_Rev_name;
    int m_Original_order;

    static int ms_Next_original_order;
};

int CPCRParsedSet::ms_Next_original_order = 0;

static
void s_ParsePCRSet( CBioSource &biosrc, list<CPCRParsedSet> &out_pcr_set )
{
    out_pcr_set.clear();

    const string* fwd_primer_seq = NULL;
    const string* rev_primer_seq = NULL;
    const string* fwd_primer_name = NULL;
    const string* rev_primer_name = NULL;

// convenience macro
#define PARSEPCRSET_CASE(Subtype) \
            case NCBI_SUBSOURCE(Subtype): \
            if( (*subsrc_iter)->IsSetName() ) { \
                Subtype = &((*subsrc_iter)->GetName()); \
            } \
            break;


    FOR_EACH_SUBSOURCE_ON_BIOSOURCE( subsrc_iter, biosrc ) {
        SWITCH_ON_SUBSOURCE_CHOICE( **subsrc_iter ) {
        PARSEPCRSET_CASE(fwd_primer_seq)
        PARSEPCRSET_CASE(rev_primer_seq)
        PARSEPCRSET_CASE(fwd_primer_name)
        PARSEPCRSET_CASE(rev_primer_name)
        default:
            // ignore
            break;
        }
    }
#undef PARSEPCRSET_CASE

    // ParsePCRStrings 
    vector<string> fwd_seq_list;
    s_ParsePCRComponent(fwd_seq_list, fwd_primer_seq);
    vector<string> rev_seq_list;
    s_ParsePCRComponent(rev_seq_list, rev_primer_seq);
    vector<string> fwd_name_list;
    s_ParsePCRComponent(fwd_name_list, fwd_primer_name);
    vector<string> rev_name_list;
    s_ParsePCRComponent(rev_name_list, rev_primer_name);

    vector<string>::iterator curr_fwd_seq = fwd_seq_list.begin();
    vector<string>::iterator curr_rev_seq = rev_seq_list.begin();
    vector<string>::iterator curr_fwd_name = fwd_name_list.begin();
    vector<string>::iterator curr_rev_name = rev_name_list.begin();

    while (curr_fwd_seq != fwd_seq_list.end() || 
        curr_rev_seq != rev_seq_list.end()    || 
        curr_fwd_name != fwd_name_list.end()  || 
        curr_rev_name != rev_name_list.end() ) 
    {
        const string *fwd_seq = ( curr_fwd_seq != fwd_seq_list.end() ? &*curr_fwd_seq++ : NULL );
        const string *rev_seq = ( curr_rev_seq != rev_seq_list.end() ? &*curr_rev_seq++ : NULL );
        const string *fwd_name = ( curr_fwd_name != fwd_name_list.end() ? &*curr_fwd_name++ : NULL );
        const string *rev_name = ( curr_rev_name != rev_name_list.end() ? &*curr_rev_name++ : NULL );

        out_pcr_set.push_back( CPCRParsedSet(fwd_seq, rev_seq, fwd_name, rev_name) );
    }
}

// split by colon and trim spaces off the pieces
static
void s_ParsePCRColonString( vector<string> &out_list, const string &str ) 
{
    NStr::Tokenize( str, ":", out_list );
    EDIT_EACH_STRING_IN_VECTOR(str_iter, out_list ) {
        NStr::TruncateSpacesInPlace( *str_iter );
    }
}

static 
CRef<CPCRPrimerSet> s_ModernizePCRPrimerHalf (const string &seq, const string &name)
{
    // Construct the value we will return
    // ( and extract its primer set for easy access )
    CRef<CPCRPrimerSet> return_value( new CPCRPrimerSet );
    list< CRef< CPCRPrimer > > &primer_list = return_value->Set();

    vector<string> seq_list;
    s_ParsePCRColonString (seq_list, seq);
    vector<string> name_list;
    s_ParsePCRColonString (name_list, name);

    vector<string>::const_iterator name_iter = name_list.begin();

    CRef<CPCRPrimer> last_primer;

    // create a PCRPrimer for each seq (and attach its name, if possible)
    FOR_EACH_STRING_IN_VECTOR( seq_iter, seq_list ) {

        const string *curr_name = NULL;
        if ( name_iter != name_list.end() ) {
            curr_name = &*name_iter;
            ++name_iter;
        }

        CRef<CPCRPrimer> curr_primer( new CPCRPrimer );
        curr_primer->SetSeq().Set( *seq_iter );
        if( curr_name != NULL ) {
            curr_primer->SetName().Set( *curr_name );
        }
        primer_list.push_back( curr_primer );
        last_primer = curr_primer;
    }

    if( last_primer ) {
        // attach any leftover names to the end of the name of the last seq
        for ( ; name_iter != name_list.end() ; ++name_iter ) {
            last_primer->SetName().Set() += ":" + *name_iter;
        }
    } else {
        // TODO: This differs from C.  C breaks as soon as it's looked at the
        // first name, but this version will create CPCRPrimer for all names.
        for ( ; name_iter != name_list.end() ; ++name_iter ) {
            CRef<CPCRPrimer> curr_primer( new CPCRPrimer );
            curr_primer->SetName().Set( *name_iter );
            primer_list.push_back( curr_primer );
        }
    }

    // If the CPCRPrimerSet contains nothing inside, return a null ref
    if( primer_list.empty() ) {
        return CRef<CPCRPrimerSet>();
    } else {
        return return_value;
    }
}

class CIsBadCRefPCRSubSource {
public:
    bool operator()( const CRef<CSubSource> &subsource ) {
        if( ! subsource ) {
            return true;
        }

        SWITCH_ON_SUBSOURCE_CHOICE( *subsource ) {
        case NCBI_SUBSOURCE(fwd_primer_seq):
        case NCBI_SUBSOURCE(rev_primer_seq):
        case NCBI_SUBSOURCE(fwd_primer_name):
        case NCBI_SUBSOURCE(rev_primer_name):
            return true;
        }

        return false;
    }
};

void CNewCleanup_imp::x_ModernizePCRPrimers( CBioSource &biosrc )
{
    list<CPCRParsedSet> pcr_parsed_list;
    s_ParsePCRSet( biosrc, pcr_parsed_list );
    if( pcr_parsed_list.empty() ) {
        return;
    }

    CRef<CPCRReactionSet> pcr_reaction_set( new CPCRReactionSet );
    list< CRef< CPCRReaction > > &pcr_reaction_list = pcr_reaction_set->Set();

    FOR_EACH_PCRPARSEDSET_IN_LIST( pcr_parsed_list_iter, pcr_parsed_list) {

        CRef<CPCRPrimerSet> forward = 
            s_ModernizePCRPrimerHalf (pcr_parsed_list_iter->GetFwdSeq(), 
            pcr_parsed_list_iter->GetFwdName());
        CRef<CPCRPrimerSet> reverse = 
            s_ModernizePCRPrimerHalf (pcr_parsed_list_iter->GetRevSeq(), 
            pcr_parsed_list_iter->GetRevName());

        if ( forward || reverse ) {
            CRef<CPCRReaction> curr_reaction( new CPCRReaction );
            if( forward ) {
                SET_FIELD( *curr_reaction, Forward, *forward );
            }
            if( reverse ) {
                SET_FIELD( *curr_reaction, Reverse, *reverse );
            }
            pcr_reaction_list.push_back( curr_reaction );
        }
    }

    // only add PCR reaction set if there's something in it
    if ( ! pcr_reaction_list.empty() ) {

        // copy the existing reaction set (if any) to the end of ours
        copy( GET_MUTABLE(biosrc, Pcr_primers).Set().begin(), 
            GET_MUTABLE(biosrc, Pcr_primers).Set().end(), 
            back_inserter(pcr_reaction_list) );
        // we are now the real pcr reaction set
        SET_FIELD( biosrc, Pcr_primers, *pcr_reaction_set  );

        // remove all old-style PCR primer subsources ( fwd_primer_seq, etc. ) 
        if( FIELD_IS_SET(biosrc, Subtype) ) {
            list< CRef< CSubSource > > &subsources = GET_MUTABLE(biosrc, Subtype);
            list< CRef< CSubSource > >::iterator first_bad_element = 
                remove_if( subsources.begin(), subsources.end(), CIsBadCRefPCRSubSource() );
            subsources.erase( first_bad_element, subsources.end() );
        }
    }
}

static bool s_GbQualCompare (
    const CRef<CGb_qual>& gb1,
    const CRef<CGb_qual>& gb2
)

{
    const CGb_qual& gbq1 = *(gb1);
    const CGb_qual& gbq2 = *(gb2);

    const string& ql1 = GET_FIELD (gbq1, Qual);
    const string& ql2 = GET_FIELD (gbq2, Qual);

    int comp = NStr::CompareNocase (ql1, ql2);
    if (comp < 0) return true;
    if (comp > 0) return false;

    const string& vl1 = GET_FIELD (gbq1, Val);
    const string& vl2 = GET_FIELD (gbq2, Val);

    if (NStr::CompareNocase (vl1, vl2) < 0) return true;

    return false;
}

static bool s_GbQualEqual (
    const CRef<CGb_qual>& gb1,
    const CRef<CGb_qual>& gb2
)

{
    const CGb_qual& gbq1 = *(gb1);
    const CGb_qual& gbq2 = *(gb2);

    const string& ql1 = GET_FIELD (gbq1, Qual);
    const string& ql2 = GET_FIELD (gbq2, Qual);

    if (! NStr::EqualNocase (ql1, ql2)) return false;

    const string& vl1 = GET_FIELD (gbq1, Val);
    const string& vl2 = GET_FIELD (gbq2, Val);

    if (! NStr::EqualNocase (vl1, vl2)) return false;

    return true;
}

void CNewCleanup_imp::Except_textBC (
    string& except_text
)

{
    if (NStr::Find (except_text, "ribosome slippage") == NPOS &&
        NStr::Find (except_text, "trans splicing") == NPOS &&
        NStr::Find (except_text, "alternate processing") == NPOS &&
        NStr::Find (except_text, "adjusted for low quality genome") == NPOS &&
        NStr::Find (except_text, "non-consensus splice site") == NPOS) {
        return;
    }

    vector<string> exceptions;
    NStr::Tokenize (except_text, ",", exceptions);

    EDIT_EACH_STRING_IN_VECTOR (it, exceptions) {
        string& text = *it;
        size_t tlen = text.length();
        NStr::TruncateSpacesInPlace (text);
        if (text.length() != tlen) {
            ChangeMade (CCleanupChange::eTrimSpaces);
        }
        if (! text.empty()) {
            if (text == "ribosome slippage") {
                text = "ribosomal slippage";
                ChangeMade (CCleanupChange::eChangeException);
            } else if (text == "trans splicing") {
                text = "trans-splicing";
                ChangeMade (CCleanupChange::eChangeException);
            } else if (text == "alternate processing") {
                text = "alternative processing";
                ChangeMade (CCleanupChange::eChangeException);
            } else if (text == "adjusted for low quality genome") {
                text = "adjusted for low-quality genome";
                ChangeMade (CCleanupChange::eChangeException);
            } else if (text == "non-consensus splice site") {
                text = "nonconsensus splice site";
                ChangeMade (CCleanupChange::eChangeException);
            }
        }
    }

    except_text = NStr::Join (exceptions, ",");
}

static
bool s_SeqLocAllEmpty( const CSeq_loc & loc )
{
    CSeq_loc_CI completeIter( loc, CSeq_loc_CI::eEmpty_Allow);
    CSeq_loc_CI gapSkippingIter( loc, CSeq_loc_CI::eEmpty_Skip);

    return ( completeIter && ! gapSkippingIter );
}

void CNewCleanup_imp::SeqfeatBC (
    CSeq_feat& sf
)

{
    // note - need to clean up GBQuals before dbxrefs, because they may be converted to populate other fields

    if ( FIELD_IS_SET(sf, Qual) ) {
        x_ExpandCombinedQuals( GET_MUTABLE(sf, Qual) );
    }

    // sort/unique gbquals

    if (! GBQUAL_ON_SEQFEAT_IS_SORTED (sf, s_GbQualCompare)) {
        SORT_GBQUAL_ON_SEQFEAT (sf, s_GbQualCompare);
        ChangeMade (CCleanupChange::eCleanQualifiers);
    }

    if (! GBQUAL_ON_SEQFEAT_IS_UNIQUE (sf, s_GbQualEqual)) {
        UNIQUE_GBQUAL_ON_SEQFEAT (sf, s_GbQualEqual);
        ChangeMade (CCleanupChange::eRemoveQualifier);
    }

    EDIT_EACH_GBQUAL_ON_SEQFEAT (gbq_it, sf) {
        CGb_qual& gbq = **gbq_it;
        GBQualBC(gbq);
        if( GBQualSeqFeatBC(gbq, sf) == eAction_Erase ) 
        {
            ERASE_GBQUAL_ON_SEQFEAT (gbq_it, sf);
            ChangeMade (CCleanupChange::eRemoveQualifier);
        }
    }

    // sort/unique gbquals (yes, must do before *and* after )

    if (! GBQUAL_ON_SEQFEAT_IS_SORTED (sf, s_GbQualCompare)) {
        SORT_GBQUAL_ON_SEQFEAT (sf, s_GbQualCompare);
        ChangeMade (CCleanupChange::eCleanQualifiers);
    }

    if (! GBQUAL_ON_SEQFEAT_IS_UNIQUE (sf, s_GbQualEqual)) {
        UNIQUE_GBQUAL_ON_SEQFEAT (sf, s_GbQualEqual);
        ChangeMade (CCleanupChange::eRemoveQualifier);
    }

    REMOVE_IF_EMPTY_GBQUAL_ON_SEQFEAT(sf);

    CLEAN_STRING_MEMBER (sf, Comment);
    if (FIELD_IS_SET (sf, Comment)) {
        CleanDoubleQuote (GET_MUTABLE (sf, Comment));        
    }
    if (FIELD_IS_SET (sf, Comment) && GET_FIELD (sf, Comment) == ".") {
        RESET_FIELD (sf, Comment);
        ChangeMade (CCleanupChange::eChangeComment);
    }

    CLEAN_STRING_MEMBER (sf, Title);

    if( FIELD_EQUALS( sf, Except, false ) ) {
        RESET_FIELD( sf, Except );
    }

    CLEAN_STRING_MEMBER (sf, Except_text);
    if (FIELD_IS_SET (sf, Except_text)) {
        string &et = GET_MUTABLE (sf, Except_text);
        Except_textBC (et);
    }

    if (FIELD_IS_SET (sf, Data)) {
        CSeqFeatData& sfd = GET_MUTABLE (sf, Data);
        SeqFeatSeqfeatDataBC (sf, sfd);
    }

    vector< CRef< CDbtag > > new_dbtags;
    EDIT_EACH_DBXREF_ON_SEQFEAT (dbx_it, sf) {
        CDbtag& dbt = **dbx_it;
        DbtagBC (dbt);

        x_SplitDbtag(dbt, new_dbtags );

        if (s_DbtagIsBad (dbt)) {
            ERASE_DBXREF_ON_SEQFEAT (dbx_it, sf);
            ChangeMade (CCleanupChange::eCleanDbxrefs);
        }
    }
    copy( new_dbtags.begin(), new_dbtags.end(), back_inserter(sf.SetDbxref()) );


    // sort/unique db_xrefs

    if (! DBXREF_ON_SEQFEAT_IS_SORTED (sf, s_DbtagCompare)) {
        SORT_DBXREF_ON_SEQFEAT (sf, s_DbtagCompare);
        ChangeMade (CCleanupChange::eCleanDbxrefs);
    }

    if (! DBXREF_ON_SEQFEAT_IS_UNIQUE (sf, s_DbtagEqual)) {
        UNIQUE_DBXREF_ON_SEQFEAT (sf, s_DbtagEqual);
        ChangeMade (CCleanupChange::eCleanDbxrefs);
    }

    REMOVE_IF_EMPTY_SEQFEATXREF_ON_SEQFEAT( sf );
    REMOVE_IF_EMPTY_DBXREF_ON_SEQFEAT( sf );

    if( FIELD_IS_SET( sf, Cit ) ) {
        PubSetBC( GET_MUTABLE( sf, Cit ) );
    }

    SeqLocBC( GET_MUTABLE( sf, Location ) );

    // clean up partial flag
    const unsigned int partial_loc_mask = ( 
        sequence::eSeqlocPartial_Start      | 
        sequence::eSeqlocPartial_Stop       |
        sequence::eSeqlocPartial_Internal   |
        sequence::eSeqlocPartial_Other      |
        sequence::eSeqlocPartial_Nostart    |
        sequence::eSeqlocPartial_Nostop     |
        sequence::eSeqlocPartial_Nointernal );
    const unsigned int partial_loc = 
        sequence::SeqLocPartialCheck( GET_FIELD( sf, Location ), m_Scope );
    if ( FIELD_EQUALS(sf, Partial, true) ) {
        // do nothing, will not change partial if already set
    } else if ( (partial_loc & partial_loc_mask) || s_SeqLocAllEmpty( GET_FIELD( sf, Location ) ) ) {
        SET_FIELD( sf, Partial, true );
        ChangeMade (CCleanupChange::eChangePartial);
    }
}

static bool
s_GeneSynCompare(
    const string &syn1,
    const string &syn2 )
{
    return ( syn1 < syn2 );
}

static bool
s_GeneSynEqual(
    const string &syn1,
    const string &syn2 )
{
    return syn1 == syn2;
}

void CNewCleanup_imp::GenerefBC (
    CGene_ref& gr
)

{
    CLEAN_STRING_MEMBER (gr, Locus);
    CLEAN_STRING_MEMBER (gr, Allele);
    CLEAN_STRING_MEMBER (gr, Desc);
    CLEAN_STRING_MEMBER (gr, Maploc);
    CLEAN_STRING_MEMBER (gr, Locus_tag);
    CLEAN_STRING_LIST (gr, Syn);
    
    if( ! SYNONYM_ON_GENEREF_IS_SORTED(gr, s_GeneSynCompare) ) {
        SORT_SYNONYM_ON_GENEREF( gr, s_GeneSynCompare );
        ChangeMade (CCleanupChange::eChangeQualifiers);
    }

    if (! SYNONYM_ON_GENEREF_IS_UNIQUE (gr, s_GeneSynEqual)) {
        UNIQUE_SYNONYM_ON_GENEREF(gr, s_GeneSynEqual);
        ChangeMade (CCleanupChange::eCleanQualifiers);
    }

    // remove synonyms equal to locus
    if (! FIELD_IS_SET (gr, Locus)) return;
    const string& locus = GET_FIELD (gr, Locus);

    EDIT_EACH_SYNONYM_ON_GENEREF (syn_itr, gr) {
        string& syn = *syn_itr;
        if (NStr::EqualNocase (locus, syn)) {
            ERASE_SYNONYM_ON_GENEREF (syn_itr, gr);
            ChangeMade (CCleanupChange::eChangeQualifiers);
        }
    }
}

static bool s_IsEmptyGeneRef (const CGene_ref& gr)

{
    if (FIELD_IS_SET (gr, Locus)) return false;
    if (FIELD_IS_SET (gr, Allele)) return false;
    if (FIELD_IS_SET (gr, Desc)) return false;
    if (FIELD_IS_SET (gr, Maploc)) return false;
    if (FIELD_IS_SET (gr, Db)) return false;
    if (FIELD_IS_SET (gr, Syn)) return false;
    if (FIELD_IS_SET (gr, Locus_tag)) return false;

    return true;
}

static bool s_CommentRedundantWithGeneRef (
    CGene_ref& gene_ref,
    const string& comm
)

{
    if (STRING_FIELD_MATCH (gene_ref, Locus,     comm)) return true;
    if (STRING_FIELD_MATCH (gene_ref, Locus_tag, comm)) return true;
    if (STRING_FIELD_MATCH (gene_ref, Desc,      comm)) return true;
    if (STRING_FIELD_MATCH (gene_ref, Locus_tag, comm)) return true;
    if (STRING_SET_MATCH   (gene_ref, Syn,       comm)) return true;

    return false;
}

void CNewCleanup_imp::GeneFeatBC (
    CGene_ref& gene_ref,
    CSeq_feat& seq_feat
)

{
    // move gene.pseudo to feat.pseudo
    if (FIELD_IS_SET (gene_ref, Pseudo)) {
        SET_FIELD (seq_feat, Pseudo, true);
        RESET_FIELD (gene_ref, Pseudo);
        ChangeMade (CCleanupChange::eChangeQualifiers);
    }

    // remove feat.comment if equal to various gene fields
    if (FIELD_IS_SET (seq_feat, Comment)) {
        if (s_CommentRedundantWithGeneRef (gene_ref, GET_FIELD (seq_feat, Comment))) {
            RESET_FIELD (seq_feat, Comment);
            ChangeMade (CCleanupChange::eChangeComment);
        }
    }
        
    // move gene.db to feat.dbxref
    if (GENEREF_HAS_DBXREF (gene_ref)) {
        FOR_EACH_DBXREF_ON_GENEREF (db_itr, gene_ref) {
            CRef <CDbtag> dbc (*db_itr);
            ADD_DBXREF_TO_SEQFEAT (seq_feat, dbc);
        }
        RESET_FIELD (gene_ref, Db);
        ChangeMade (CCleanupChange::eChangeDbxrefs);
    }
        
    // move feat.xref.gene.db to feat.dbxref
    if (SEQFEAT_HAS_SEQFEATXREF (seq_feat)) {
        EDIT_EACH_SEQFEATXREF_ON_SEQFEAT (xr_itr, seq_feat) {
            CSeqFeatXref& sfx = **xr_itr;
            if (! FIELD_IS_SET (sfx, Data)) continue;
            CSeqFeatData& sfd = GET_MUTABLE (sfx, Data);
            if (! FIELD_IS (sfd, Gene)) continue;
            CGene_ref& gene_ref = GET_MUTABLE (sfd, Gene);
            if (GENEREF_HAS_DBXREF (gene_ref)) {
                FOR_EACH_DBXREF_ON_GENEREF (db_itr, gene_ref) {
                    CRef <CDbtag> dbc (*db_itr);
                    ADD_DBXREF_TO_SEQFEAT (seq_feat, dbc);
                }
                RESET_FIELD (gene_ref, Db);
                ChangeMade (CCleanupChange::eChangeDbxrefs);
            }
            if (s_IsEmptyGeneRef (gene_ref)) {
                ERASE_SEQFEATXREF_ON_SEQFEAT (xr_itr, seq_feat);
                ChangeMade (CCleanupChange::eChangeDbxrefs);
            }
        }
    }

    REMOVE_IF_EMPTY_SEQFEATXREF_ON_SEQFEAT(seq_feat);
}

void CNewCleanup_imp::ProtrefBC (
    CProt_ref& prot_ref
)

{
    CLEAN_STRING_MEMBER (prot_ref, Desc);
    if (CleanStringList (GET_MUTABLE (prot_ref, Name))) {
        ChangeMade (CCleanupChange::eChangeProtNames);
    }
    CLEAN_STRING_LIST_JUNK (prot_ref, Ec);
    CLEAN_STRING_LIST (prot_ref, Activity);

    const int old_num_activities = GET_FIELD( prot_ref, Activity ).size();
    UNIQUE_WITHOUT_SORT_ACTIVITY_ON_PROTREF( prot_ref, PNocase );
    if( (int)GET_FIELD( prot_ref, Activity ).size() != old_num_activities ) {
        ChangeMade (CCleanupChange::eChangeProtActivities);
    }

    REMOVE_IF_EMPTY_ACTIVITY_ON_PROTREF(prot_ref);

    // rubisco cleanup
    if( m_IsEmblOrDdbj ) {
        EDIT_EACH_NAME_ON_PROTREF (it, prot_ref) {
            if (NStr::EqualNocase (*it, "RbcL") || NStr::EqualNocase(*it, "rubisco large subunit")) {
                *it = "ribulose-1,5-bisphosphate carboxylase/oxygenase large subunit";
                ChangeMade (CCleanupChange::eChangeQualifiers);
                if (prot_ref.IsSetDesc() && NStr::EqualNocase(prot_ref.GetDesc(), "RbcL")) {
                    prot_ref.ResetDesc();
                }
                continue;
            } else if (NStr::EqualNocase (*it, "RbcS") || NStr::EqualNocase(*it, "rubisco small subunit")) {
                *it = "ribulose-1,5-bisphosphate carboxylase/oxygenase small subunit";
                ChangeMade (CCleanupChange::eChangeQualifiers);
                if (prot_ref.IsSetDesc() && NStr::EqualNocase(prot_ref.GetDesc(), "RbcS")) {
                    prot_ref.ResetDesc();
                }
                continue;
            } else if (prot_ref.IsSetDesc() && NStr::EqualCase (*it, prot_ref.GetDesc())) {
                prot_ref.ResetDesc();
            }

            if (NStr::Find (*it, "ribulose") != string::npos
                && NStr::Find (*it, "bisphosphate") != string::npos
                && NStr::Find (*it, "methyltransferase") == string::npos
                && !NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase/oxygenase large subunit")
                && !NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase/oxygenase small subunit")
                && (NStr::EqualNocase (*it, "ribulose 1,5-bisphosphate carboxylase/oxygenase large subunit")
                || NStr::EqualNocase (*it, "ribulose 1,5-bisphosphate carboxylase large subunit")
                || NStr::EqualNocase (*it, "ribulose bisphosphate carboxylase large subunit")
                || NStr::EqualNocase (*it, "ribulose-bisphosphate carboxylase large subunit")
                || NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase large subunit")
                || NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase, large subunit")
                || NStr::EqualNocase (*it, "large subunit of ribulose-1,5-bisphosphate carboxylase/oxygenase")
                || NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase oxygenase large subunit")
                || NStr::EqualNocase (*it, "ribulose bisphosphate carboxylase large chain")
                || NStr::EqualNocase (*it, "ribulose 1,5-bisphosphate carboxylase-oxygenase large subunit")
                || NStr::EqualNocase (*it, "ribulose bisphosphate carboxylase oxygenase large subunit")
                || NStr::EqualNocase (*it, "ribulose 1,5 bisphosphate carboxylase large subunit")
                || NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase/oxygenase, large subunit")
                || NStr::EqualNocase (*it, "large subunit of ribulose-1,5-bisphosphate carboxylase/oxgenase")
                || NStr::EqualNocase (*it, "ribulose bisphosphate carboxylase/oxygenase large subunit")
                || NStr::EqualNocase (*it, "ribulose-1,5-bisphosphate carboxylase oxygenase, large subunit")
                || NStr::EqualNocase (*it, "ribulose 5-bisphosphate carboxylase, large subunit")
                || NStr::EqualNocase (*it, "ribulosebisphosphate carboxylase large subunit")
                || NStr::EqualNocase (*it, "ribulose bisphosphate large subunit")
                || NStr::EqualNocase (*it, "ribulose 1,5 bisphosphate carboxylase/oxygenase large subunit")
                || NStr::EqualNocase (*it, "ribulose 1,5-bisphosphate carboxylase/oxygenase large chain")
                || NStr::EqualNocase (*it, "large subunit ribulose-1,5-bisphosphate carboxylase/oxygenase")
                || NStr::EqualNocase (*it, "ribulose-bisphosphate carboxylase, large subunit")
                || NStr::EqualNocase (*it, "ribulose-1, 5-bisphosphate carboxylase/oxygenase large-subunit")) ) {
                    *it = "ribulose-1,5-bisphosphate carboxylase/oxygenase large subunit";
                    ChangeMade (CCleanupChange::eChangeQualifiers);
            }
        }
    }
}

static const string uninf_names [] = {
    "peptide",
    "putative",
    "signal",
    "signal peptide",
    "signal-peptide",
    "signal_peptide",
    "transit",
    "transit peptide",
    "transit-peptide",
    "transit_peptide",
    "unknown",
    "unnamed"
};

typedef CStaticArraySet<string, PNocase> TUninformative;
DEFINE_STATIC_ARRAY_MAP(TUninformative, sc_UninfNames, uninf_names);

static bool s_IsInformativeName (
    const string& name
)

{
    return ! name.empty() && sc_UninfNames.find(name) == sc_UninfNames.end();
}

static bool s_CommentRedundantWithProtRef (
    CProt_ref& pr,
    const string& comm
)

{
    if (STRING_SET_MATCH (pr, Name, comm)) return true;
    if (STRING_FIELD_MATCH (pr, Desc, comm)) return true;
    if (STRING_SET_MATCH (pr, Ec, comm)) return true;

    return false;
}

void CNewCleanup_imp::ProtFeatfBC (
    CProt_ref& pr,
    CSeq_feat& sf
)

{
    TPROTREF_PROCESSED processed = ( FIELD_IS_SET (pr, Processed) ? GET_FIELD (pr, Processed) : NCBI_PROTREF(not_set) );

    // move putative from comment to protein name for mat peptide
    if (FIELD_IS_SET (sf, Comment) && 
        ( ! FIELD_IS_SET (pr, Name) || NAME_ON_PROTREF_IS_EMPTY(pr) )&&
        processed != NCBI_PROTREF(signal_peptide) &&
        processed != NCBI_PROTREF(transit_peptide)) {
            if (! NStr::EqualNocase ("putative", GET_FIELD (sf, Comment))) {
                ADD_NAME_TO_PROTREF ( pr, GET_FIELD (sf, Comment) );
                RESET_FIELD (sf, Comment);
            }
    }

    // move putative to comment, remove uninformative name of signal peptide
    if (FIELD_IS_SET (pr, Name)) {
        if (processed == NCBI_PROTREF(signal_peptide) ||
            processed == NCBI_PROTREF(transit_peptide)) {
                EDIT_EACH_NAME_ON_PROTREF (nm_itr, pr) {
                    string& str = *nm_itr;
                    if (NStr::Find (str, "putative") != NPOS ||
                        NStr::Find (str, "put. ") != NPOS) {
                            if (! FIELD_IS_SET (sf, Comment)) {
                                SET_FIELD (sf, Comment, "putative");
                                ChangeMade (CCleanupChange::eChangeComment);
                            }
                    }
                    if (! s_IsInformativeName (str)) {
                        ERASE_NAME_ON_PROTREF (nm_itr, pr);
                        ChangeMade (CCleanupChange::eChangeProtNames);
                    }
                }
        }

        EDIT_EACH_NAME_ON_PROTREF (nm_itr, pr) {
            string& str = *nm_itr;
            // rubisco
            if (NStr::EqualNocase (str, "RbcL") || NStr::EqualNocase(str, "rubisco large subunit")) {
                str = "ribulose-1,5-bisphosphate carboxylase/oxygenase large subunit";
                ChangeMade (CCleanupChange::eChangeQualifiers);
            } else if (NStr::EqualNocase (str, "RbcS") || NStr::EqualNocase(str, "rubisco small subunit")) {
                str = "ribulose-1,5-bisphosphate carboxylase/oxygenase small subunit";
                ChangeMade (CCleanupChange::eChangeQualifiers);
            }
        }
    }

    // add unnamed as default protein name
    if (! FIELD_IS_SET (pr, Name) || NAME_ON_PROTREF_IS_EMPTY(pr) ) {
        if (processed == NCBI_PROTREF(preprotein)  ||  
            processed == NCBI_PROTREF(mature)) {
                ADD_NAME_TO_PROTREF (pr, "unnamed");
                ChangeMade (CCleanupChange::eChangeQualifiers);
        }
    }

    // remove description if same as protein name
    if (FIELD_IS_SET (pr, Desc)) {
        const string& desc = GET_FIELD (pr, Desc);
        FOR_EACH_NAME_ON_PROTREF (nm_itr, pr) {
            const string& str = *nm_itr;
            if (NStr::Equal (desc, str)) {
                RESET_FIELD (pr, Desc);
                ChangeMade (CCleanupChange::eRemoveQualifier);
                break;
            }
        }
    }
        
    // remove feat.comment if equal to various protein fields
    if (FIELD_IS_SET (sf, Comment)) {
        if (s_CommentRedundantWithProtRef (pr, GET_FIELD (sf, Comment))) {
            RESET_FIELD (sf, Comment);
            ChangeMade (CCleanupChange::eChangeComment);
        }
    }
        
    // move prot.db to feat.dbxref
    if (PROTREF_HAS_DBXREF (pr)) {
        FOR_EACH_DBXREF_ON_PROTREF (db_itr, pr) {
            CRef <CDbtag> dbc (*db_itr);
            ADD_DBXREF_TO_SEQFEAT (sf, dbc);
        }
        RESET_FIELD (pr, Db);
        ChangeMade (CCleanupChange::eChangeDbxrefs);
    }

    REMOVE_IF_EMPTY_NAME_ON_PROTREF(pr);
}

static const char* const ncrna_names [] = {
    "antisense_RNA",
    "autocatalytically_spliced_intron",
    "guide_RNA",
    "hammerhead_ribozyme",
    "miRNA",
    "other",
    "piRNA",
    "rasiRNA",
    "ribozyme",
    "RNase_MRP_RNA",
    "RNase_P_RNA",
    "scRNA",
    "siRNA",
    "snoRNA",
    "snRNA",
    "SRP_RNA",
    "telomerase_RNA",
    "vault_RNA",
    "Y_RNA"
};

typedef CStaticArraySet<const char*, PNocase_CStr> TNcrna;
DEFINE_STATIC_ARRAY_MAP(TNcrna, sc_NcrnafNames, ncrna_names);

static bool s_IsNcrnaName (
    const string& name
)

{
    return sc_NcrnafNames.find(name.c_str()) != sc_NcrnafNames.end();
}

void CNewCleanup_imp::RnarefBC (
    CRNA_ref& rr
)

{
    if (FIELD_IS_SET (rr, Ext)) {
        CRNA_ref::C_Ext& ext = GET_MUTABLE (rr, Ext);
        TRNAREF_EXT chs = ext.Which();
        switch (chs) {
            case NCBI_RNAEXT(Name):
                {
                    string& name = GET_MUTABLE (ext, Name);
                    if (CleanString (name, true)) {
                        ChangeMade (CCleanupChange::eTrimSpaces);
                    }
                    if (NStr::IsBlank (name)) {
                        RESET_FIELD (rr, Ext);
                    }

                    static const string rRNA = " rRNA";
                    static const string kRibosomalrRna = " ribosomal RNA";

                    if (rr.IsSetType()) {
                        switch (rr.GetType()) {
                            case CRNA_ref::eType_rRNA:
                            {{
                                size_t len = name.length();
                                if (len >= rRNA.length()                       &&
                                    NStr::EndsWith(name, rRNA, NStr::eNocase)  &&
                                    !NStr::EndsWith(name, kRibosomalrRna, NStr::eNocase)) {
                                    name.replace(len - rRNA.length(), name.size(), kRibosomalrRna);
                                    ChangeMade(CCleanupChange::eChangeQualifiers);
                                }
                                break;
                            }}
                            case CRNA_ref::eType_tRNA:
                            {{
                                // !!! TODO parse tRNA string. lines 6791:6827, api/sqnutil1.c
                                break;
                            }}
                            case CRNA_ref::eType_other:
                            {{
                                // TODO
                            }}
                                break;
                            default:
                                // TODO ???
                                break;
                        }
                    }
                }
                break;
            case NCBI_RNAEXT(TRNA):
                {
                    CTrna_ext& tRNA = GET_MUTABLE (ext, TRNA);
                    if (FIELD_IS_SET (tRNA, Aa)) {
                        const CTrna_ext::C_Aa& aa = GET_FIELD (tRNA, Aa);
                        if (aa.Which() == CTrna_ext::C_Aa::e_not_set) {
                            RESET_FIELD (tRNA, Aa);
                        }
                    }

                    if (! CODON_ON_TRNAEXT_IS_SORTED(tRNA, s_CodonCompare)) {
                        SORT_CODON_ON_TRNAEXT(tRNA, s_CodonCompare);
                        ChangeMade(CCleanupChange::eChange_tRna);
                    }

                    if( ! CODON_ON_TRNAEXT_IS_UNIQUE(tRNA, s_CodonEqual) ) {
                        UNIQUE_CODON_ON_TRNAEXT(tRNA, s_CodonEqual);
                        ChangeMade(CCleanupChange::eChange_tRna);
                    }

                    REMOVE_IF_EMPTY_CODON_ON_TRNAEXT(tRNA);

                    if (! FIELD_IS_SET (tRNA, Aa) &&
                        ! FIELD_IS_SET (tRNA, Codon) &&
                        ! FIELD_IS_SET (tRNA, Anticodon)) {
                        RESET_FIELD (rr, Ext);
                    }
                }
                break;
            case NCBI_RNAEXT(Gen):
                {
                    CRNA_gen& gen = GET_MUTABLE (ext, Gen);
                    if (FIELD_IS_SET (gen, Class)) {
                        const string& str = GET_FIELD (gen, Class);
                        if (NStr::IsBlank (str)) {
                            RESET_FIELD (gen, Class);
                        }
                    }
                    if (FIELD_IS_SET (gen, Product)) {
                        const string& str = GET_FIELD (gen, Product);
                        if (NStr::IsBlank (str)) {
                            RESET_FIELD (gen, Product);
                        }
                    }
                    if (FIELD_IS_SET (gen, Quals)) {
                        CRNA_qual_set& qset = GET_MUTABLE (gen, Quals);
                        EDIT_EACH_QUAL_ON_RNAQSET (qitr, qset) {
                            CRNA_qual& qual = **qitr;
                            CLEAN_STRING_MEMBER (qual, Qual);
                            CLEAN_STRING_MEMBER (qual, Val);
                            if( ! FIELD_IS_SET(qual, Qual) || ! FIELD_IS_SET(qual, Val) ) {
                                ERASE_QUAL_ON_RNAQSET( qitr, qset );
                            }
                        }

                        if( QUAL_ON_RNAQSET_IS_EMPTY(qset)  ) {
                            RESET_FIELD(gen, Quals);
                        }
                    }

                    if (! FIELD_IS_SET (gen, Class) &&
                        ! FIELD_IS_SET (gen, Product) &&
                        ! FIELD_IS_SET (gen, Quals)) {
                        RESET_FIELD (rr, Ext);
                    }
                }
                break;
            default:
                break;
        }
    }

    if (FIELD_IS_SET (rr, Type)) {
        TRNAREF_TYPE typ = GET_FIELD (rr, Type);
        switch (typ) {
            case NCBI_RNAREF(mRNA):
                {
                }
                break;
            case NCBI_RNAREF(tRNA):
                {
                    cerr << ""; // TODO
                }
                break;
            case NCBI_RNAREF(rRNA):
                {
                }
                break;
            case NCBI_RNAREF(other):
                {
                    // TODO
                    if (FIELD_IS_SET (rr, Ext)) {
                        CRNA_ref::C_Ext& ext = GET_MUTABLE (rr, Ext);
                        TRNAREF_EXT chs = ext.Which();
                        if (chs == NCBI_RNAEXT(Name)) {
                            string& str = GET_MUTABLE (ext, Name);
                            if ( str.empty() || NStr::EqualNocase (str, "misc_RNA")) {
                                rr.SetType( CRNA_ref::eType_miscRNA );
                                rr.ResetExt();
                            } else if (NStr::EqualNocase (str, "ncRNA")) {
                                rr.SetType( CRNA_ref::eType_ncRNA );
                                rr.ResetExt();
                            } else if (NStr::EqualNocase (str, "tmRNA")) {
                                rr.SetType( CRNA_ref::eType_tmRNA );
                                rr.ResetExt();
                            } else if (s_IsNcrnaName (str)) {
                                rr.SetType( CRNA_ref::eType_ncRNA );
                                string new_class = str;
                                rr.SetExt().SetGen().SetClass( new_class );
                            } else {
                                rr.SetType( CRNA_ref::eType_miscRNA );
                                string new_product = str;
                                rr.SetExt().SetGen().SetProduct( new_product );
                            }
                        }
                    } else {
                        rr.SetType( CRNA_ref::eType_miscRNA );
                    }
                }
                break;
            default:
                break;
        }
    }
}

void CNewCleanup_imp::RnaFeatBC (
    CRNA_ref& rna,
    CSeq_feat& seq_feat
)

{
    // move rna.pseudo to feat.pseudo
    if ( FIELD_IS_SET(rna, Pseudo) ) {
        SET_FIELD(seq_feat, Pseudo, true);
        RESET_FIELD(rna, Pseudo);
    }

    if ( rna.IsSetExt() &&
        rna.GetExt().IsTRNA() ) 
    {                
        CTrna_ext &tRNA = rna.SetExt().SetTRNA();
        x_SeqFeatTRNABC( seq_feat, tRNA );

        if( seq_feat.IsSetLocation() && 
            tRNA.IsSetAnticodon() &&
            tRNA.GetAnticodon().IsInt() ) 
        {
            const CSeq_id *loc_id = seq_feat.GetLocation().GetId();
            const CSeq_id *ac_id  = tRNA.GetAnticodon().GetId();
            if( loc_id && ac_id && loc_id->Compare( *ac_id ) == CSeq_id::e_YES ) {
                const ENa_strand loc_strand = seq_feat.GetLocation().GetStrand();
                const ENa_strand ac_strand = tRNA.GetAnticodon().GetStrand();
                if (loc_strand == eNa_strand_minus && ac_strand != eNa_strand_minus) {
                    tRNA.SetAnticodon().SetInt().SetStrand(eNa_strand_minus);
                    ChangeMade (CCleanupChange::eChangeAnticodon);
                }
            }
        }
    }
}

void CNewCleanup_imp::PubFeatBC (
    CPubdesc& pub,
    CSeq_feat& sf
)
{
    // currently no cleanup that uses CPubdesc and CSeq_feat
}

void CNewCleanup_imp::UserFeatBC (
    CUser_object& usr,
    CSeq_feat& sf
)

{
}

void CNewCleanup_imp::SourceFeatBC (
    CBioSource& bsc,
    CSeq_feat& sf
)

{
}

void CNewCleanup_imp::SeqFeatSeqfeatDataBC (
    CSeq_feat& sf,
    CSeqFeatData& sfd
)

{
    switch (sfd.Which()) {
        case NCBI_SEQFEAT(Gene):
            {
                CGene_ref& gr = GET_MUTABLE (sfd, Gene);
                GenerefBC (gr);
                GeneFeatBC (gr, sf);
            }
            break;
        case NCBI_SEQFEAT(Cdregion):
            break;
        case NCBI_SEQFEAT(Prot):
            {
                CProt_ref& pr = GET_MUTABLE (sfd, Prot);
                ProtrefBC (pr);
                ProtFeatfBC (pr, sf);
            }
            break;
        case NCBI_SEQFEAT(Rna):
            {
                CRNA_ref& rr = GET_MUTABLE (sfd, Rna);
                RnarefBC(rr);
                RnaFeatBC (rr, sf);
            }
            break;
        case NCBI_SEQFEAT(Pub):
            {
                CPubdesc& pub = GET_MUTABLE (sfd, Pub);
                PubdescBC (pub);
                PubFeatBC (pub, sf);
            }
            break;
        case NCBI_SEQFEAT(Seq):
            break;
        case NCBI_SEQFEAT(Imp):
            {
                ImpFeatBC(GET_MUTABLE (sfd, Imp), sf);
            }
            break;
        case NCBI_SEQFEAT(Region):
            {
                RegionFeatBC( GET_MUTABLE (sfd, Region), sf );
            }
            break;
        case NCBI_SEQFEAT(Comment):
            break;
        case NCBI_SEQFEAT(Bond):
            break;
        case NCBI_SEQFEAT(Site):
            {
                SiteFeatBC( GET_MUTABLE (sfd, Site), sf );
            }
            break;
        case NCBI_SEQFEAT(Rsite):
            break;
        case NCBI_SEQFEAT(User):
            {
                CUser_object& usr = GET_MUTABLE (sfd, User);
                UserobjBC (usr);
                UserFeatBC (usr, sf);
            }
            break;
        /*
        case NCBI_SEQFEAT(Txinit):
            break;
        case NCBI_SEQFEAT(Num):
            break;
        case NCBI_SEQFEAT(Psec_str):
            break;
        case NCBI_SEQFEAT(Non_std_residue):
            break;
        case NCBI_SEQFEAT(Het):
            break;
        */
        case NCBI_SEQFEAT(Org):
            {
                // wrap Org_ref in BioSource
                CRef <COrg_ref> org (&sfd.SetOrg());
                sfd.SetBiosrc().SetOrg(*org);
                ChangeMade (CCleanupChange::eConvertFeature);
            }
            // fall through to do BioSource cleanup
        case NCBI_SEQFEAT(Biosrc):
            {
                CBioSource& bsc = GET_MUTABLE (sfd, Biosrc);
                BiosourceBC (bsc);
                SourceFeatBC (bsc, sf);
            }
            break;
        /*
        case NCBI_SEQFEAT(Clone):
            break;
        */
        default:
            break;
    }
}



void CNewCleanup_imp::ExtendedCleanup (
    CSeq_entry& se
)

{
}

void CNewCleanup_imp::ExtendedCleanup (
    CSeq_submit& ss
)

{
}

void CNewCleanup_imp::ExtendedCleanup (
    CSeq_annot& sa
)

{
}

