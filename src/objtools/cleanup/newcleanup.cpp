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
#include <objmgr/scope.hpp>
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
class CMedline_entry;
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
    void UserobjBC (CUser_object& usr);

    void SeqannotBC (CSeq_annot& sa);
    void SeqfeatBC (CSeq_feat& sf);
    void SeqfeatDataBC (CSeq_feat& sf, CSeqFeatData& sfd);

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

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!TODO

    /* EDIT_EACH_SEQID_ON_BIOSEQ( seqid_itr, bs ) {
        CSeq_id &seq_id = **seqid_itr;
        //check for null
            // also, use macros here
            if( SEQID_CHOICE_IS( seq_id, "Local" ) ) {
                if( CleanString( seq_id.GetLocal().SetStr() ) ) {
                    ChangeMade (CCleanupChange::eTrimSpaces);
                }

            }
    } */ 

    // !!! cleanup instance and ids !!!
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
    if (str1 < str2) return true;
    if (str1 > str2) return false;

    return false;
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
    CLEAN_STRING_MEMBER_JUNK (gbk, Div);

    RESET_FIELD (gbk, Taxonomy);
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

    CLEAN_STRING_LIST (emb, Keywords);

    EDIT_EACH_KEYWORD_ON_EMBLBLOCK (kw_itr, emb) {
        string& str = *kw_itr;
        for (KEYWORD_ON_EMBLBLOCK_Type::const_iterator tmp_itr = KEYWORD_ON_EMBLBLOCK_Get(emb).begin();
             tmp_itr != kw_itr; ++tmp_itr) {
            const string& prv = *tmp_itr;
            if (! NStr::EqualNocase (str, prv)) continue;
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
            if (NStr::IsBlank (name)) {
                name = "";
            }
        }
        
        if (NStr::IsBlank (name)) {
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
    CBioSource& bsc
)

{
    if (BIOSOURCE_HAS_ORGREF (bsc)) {
        COrg_ref& org = GET_MUTABLE (bsc, Org);
        OrgrefBC (org);

        // convert COrg_reg.TMod string to SubSource objects
        EDIT_EACH_MOD_ON_ORGREF (it, org) {
            string& str = *it;
            CRef<CSubSource> sbs (s_StringToSubSource (str));
            if (! sbs) continue;
            ADD_SUBSOURCE_TO_BIOSOURCE (bsc, sbs);
            ERASE_MOD_ON_ORGREF (it, org);
            ChangeMade (CCleanupChange::eChangeSubsource);
        }
    }

    if (BIOSOURCE_HAS_SUBSOURCE (bsc)) {
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, bsc) {
            CSubSource& sbs = **it;
            SubsourceBC (sbs);
        }

        // remove those with no name unless it has a subtype that doesn't need a name.
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, bsc) {
            CSubSource& sbs = **it;
            if (FIELD_IS_SET (sbs, Name)) continue;
            TSUBSOURCE_SUBTYPE chs = GET_FIELD (sbs, Subtype);
            if (CSubSource::NeedsNoText (chs)) continue;
            ERASE_SUBSOURCE_ON_BIOSOURCE (it, bsc);
            ChangeMade (CCleanupChange::eCleanSubsource);
        }

        // remove plastid-name qual if the value is the same as the biosource location
        string plastid_name = "";
        SWITCH_ON_BIOSOURCE_GENOME (bsc) {
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

        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, bsc) {
          CSubSource& sbs = **it;
          TSUBSOURCE_SUBTYPE chs = GET_FIELD (sbs, Subtype);
          if (CSubSource::NeedsNoText (chs)) {
              RESET_FIELD (sbs, Name);
              SET_FIELD (sbs, Name, "");
              ChangeMade (CCleanupChange::eCleanSubsource);
          } else if (chs == NCBI_SUBSOURCE(plastid_name)) {
              if (NStr::Equal (GET_FIELD (sbs, Name), plastid_name)) {
                  ERASE_SUBSOURCE_ON_BIOSOURCE (it, bsc);
                  ChangeMade (CCleanupChange::eCleanSubsource);
              }
          }
        }

        // remove spaces and convert to lowercase in fwd_primer_seq and rev_primer_seq.
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, bsc) {
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
    
        if (! SUBSOURCE_ON_BIOSOURCE_IS_SORTED (bsc, s_SubsourceCompare)) {
            SORT_SUBSOURCE_ON_BIOSOURCE (bsc, s_SubsourceCompare);
            ChangeMade (CCleanupChange::eCleanSubsource);
        }

        if (! SUBSOURCE_ON_BIOSOURCE_IS_UNIQUE (bsc, s_SubsourceEqual)) {
            UNIQUE_SUBSOURCE_ON_BIOSOURCE (bsc, s_SubsourceEqual);
            ChangeMade (CCleanupChange::eCleanSubsource);
        }
    }
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
        EDIT_EACH_DBXREF_ON_ORGREF (it, org) {
            CDbtag& dbt = **it;
            DbtagBC (dbt);

            if (s_DbtagIsBad (dbt)) {
                ERASE_DBXREF_ON_ORGREF (it, org);
                ChangeMade (CCleanupChange::eCleanDbxrefs);
            }
        }

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

    TORGMOD_SUBTYPE chs1 = GET_FIELD (omd1, Subtype);
    TORGMOD_SUBTYPE chs2 = GET_FIELD (omd2, Subtype);

    if (chs1 < chs2) return true;
    if (chs1 > chs2) return false;

    const string& str1 = GET_FIELD (omd1, Subname);
    const string& str2 = GET_FIELD (omd2, Subname);

    if (NStr::CompareNocase (str1, str2) < 0) return true;

    return false;
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

    const string& str1 = GET_FIELD (omd1, Subname);
    const string& str2 = GET_FIELD (omd2, Subname);

    if (! NStr::EqualNocase (str1, str2)) return false;

    TORGMOD_SUBTYPE chs1 = GET_FIELD (omd1, Subtype);
    TORGMOD_SUBTYPE chs2 = GET_FIELD (omd2, Subtype);

    if (chs1 == chs2) return true;
    if (chs2 == NCBI_ORGMOD(other)) return true;

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
            if (!isspace (str[prev_pos])) {
                prev_pos++;
            }
        } else {
          prev_pos = 0;
        }
            
        next_pos = pos + 1;
        while (isspace (str[next_pos]) || str[next_pos] == ':') {
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
    CDbtag& dbt
)

{
    if (! FIELD_IS_SET (dbt, Db)) return;
    if (! FIELD_IS_SET (dbt, Tag)) return;

    string& db = GET_MUTABLE (dbt, Db);
    if (NStr::IsBlank (db)) return;

    if (CleanString (db, true)) {
        ChangeMade (CCleanupChange::eTrimSpaces);
    }

    if (NStr::EqualNocase(db, "Swiss-Prot")
        || NStr::EqualNocase (db, "SWISSPROT")) {
        db = "UniProtKB/Swiss-Prot";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "SPTREMBL")  ||
               NStr::EqualNocase(db, "TrEMBL")
               || NStr::EqualNocase(db, "UniProt/Swiss-prot")) {
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
    } else if (NStr::EqualNocase(db, "BHB")) {
        db = "BioHealthBase";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    } else if (NStr::EqualNocase(db, "GeneDB")) {
        db = "BioHealthBase";
        ChangeMade(CCleanupChange::eChangeDbxrefs);
    }

    CObject_id& oid = GET_MUTABLE (dbt, Tag);
    if (! FIELD_IS (oid, Str)) return;

    const string& str = GET_FIELD (oid, Str);
    if (NStr::IsBlank (str)) return;

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
            SET_FIELD (oid, Id, NStr::StringToUInt (str));
            ChangeMade (CCleanupChange::eChangeDbxrefs);
        } catch (CStringException&) {
            // just leave things as are
        }
    }
}

void CNewCleanup_imp::PubdescBC (
    CPubdesc& pub
)

{
}

void CNewCleanup_imp::UserobjBC (
    CUser_object& usr
)

{
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

    // !!! remainder not yet implemented !!!
}

static bool s_GBQualIsBad (
    CGb_qual& gbq
)

{
    // !!! not yet implemented !!!

    return false;
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
        NStr::Find (except_text, "non-consensus splice site") == NPOS &&
        NStr::Find (except_text, "reasons cited in publication") == NPOS) {
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
            } else if (NStr::Equal(except_text, "reasons cited in publication")) {
                text = "reasons given in citation";
                ChangeMade (CCleanupChange::eChangeException);
            }
        }
    }

    except_text = NStr::Join (exceptions, ",");
}

void CNewCleanup_imp::SeqfeatBC (
    CSeq_feat& sf
)

{
    // note - need to clean up GBQuals before dbxrefs, because they may be converted to populate other fields

    EDIT_EACH_GBQUAL_ON_SEQFEAT (gbq_it, sf) {
        CGb_qual& gbq = **gbq_it;
        GBQualBC (gbq);

        if (s_GBQualIsBad (gbq)) {
            ERASE_GBQUAL_ON_SEQFEAT (gbq_it, sf);
            ChangeMade (CCleanupChange::eRemoveQualifier);
        }
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

    CLEAN_STRING_MEMBER (sf, Comment);

    if (FIELD_IS_SET (sf, Comment)) {
        CleanDoubleQuote (GET_MUTABLE (sf, Comment));        
    }
    if (FIELD_IS_SET (sf, Comment) && GET_FIELD (sf, Comment) == ".") {
        RESET_FIELD (sf, Comment);
        ChangeMade (CCleanupChange::eChangeComment);
    }

    CLEAN_STRING_MEMBER (sf, Title);

    CLEAN_STRING_MEMBER (sf, Except_text);

    if (FIELD_IS_SET (sf, Except_text)) {
        string &et = GET_MUTABLE (sf, Except_text);
        Except_textBC (et);
    }

    if (FIELD_IS_SET (sf, Data)) {
        CSeqFeatData& sfd = GET_MUTABLE (sf, Data);
        SeqfeatDataBC (sf, sfd);
    }

    EDIT_EACH_DBXREF_ON_SEQFEAT (dbx_it, sf) {
        CDbtag& dbt = **dbx_it;
        DbtagBC (dbt);

        if (s_DbtagIsBad (dbt)) {
            ERASE_DBXREF_ON_SEQFEAT (dbx_it, sf);
            ChangeMade (CCleanupChange::eCleanDbxrefs);
        }
    }

    // sort/unique db_xrefs

    if (! DBXREF_ON_SEQFEAT_IS_SORTED (sf, s_DbtagCompare)) {
        SORT_DBXREF_ON_SEQFEAT (sf, s_DbtagCompare);
        ChangeMade (CCleanupChange::eCleanDbxrefs);
    }

    if (! DBXREF_ON_SEQFEAT_IS_UNIQUE (sf, s_DbtagEqual)) {
        UNIQUE_DBXREF_ON_SEQFEAT (sf, s_DbtagEqual);
        ChangeMade (CCleanupChange::eCleanDbxrefs);
    }
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
    CGene_ref& gr,
    const string& comm
)

{
    if (STRING_FIELD_MATCH (gr, Locus, comm)) return true;
    if (STRING_FIELD_MATCH (gr, Desc, comm)) return true;
    if (STRING_FIELD_MATCH (gr, Locus_tag, comm)) return true;
    if (STRING_SET_MATCH (gr, Syn, comm)) return true;

    return false;
}

void CNewCleanup_imp::GeneFeatBC (
    CGene_ref& gr,
    CSeq_feat& sf
)

{
    // move gene.pseudo to feat.pseudo
    if (FIELD_IS_SET (gr, Pseudo)) {
        SET_FIELD (sf, Pseudo, true);
        RESET_FIELD (gr, Pseudo);
        ChangeMade (CCleanupChange::eChangeQualifiers);
    }

    // remove feat.comment if equal to various gene fields
    if (FIELD_IS_SET (sf, Comment)) {
        if (s_CommentRedundantWithGeneRef (gr, GET_FIELD (sf, Comment))) {
            RESET_FIELD (sf, Comment);
            ChangeMade (CCleanupChange::eChangeComment);
        }
    }
        
    // move gene.db to feat.dbxref
    if (GENEREF_HAS_DBXREF (gr)) {
        FOR_EACH_DBXREF_ON_GENEREF (db_itr, gr) {
            CRef <CDbtag> dbc (*db_itr);
            ADD_DBXREF_TO_SEQFEAT (sf, dbc);
        }
        RESET_FIELD (gr, Db);
        ChangeMade (CCleanupChange::eChangeDbxrefs);
    }
        
    // move feat.xref.gene.db to feat.dbxref
    if (SEQFEAT_HAS_SEQFEATXREF (sf)) {
        EDIT_EACH_SEQFEATXREF_ON_SEQFEAT (xr_itr, sf) {
            CSeqFeatXref& sfx = **xr_itr;
            if (! FIELD_IS_SET (sfx, Data)) continue;
            CSeqFeatData& sfd = GET_MUTABLE (sfx, Data);
            if (! FIELD_IS (sfd, Gene)) continue;
            CGene_ref& gr = GET_MUTABLE (sfd, Gene);
            if (GENEREF_HAS_DBXREF (gr)) {
                FOR_EACH_DBXREF_ON_GENEREF (db_itr, gr) {
                    CRef <CDbtag> dbc (*db_itr);
                    ADD_DBXREF_TO_SEQFEAT (sf, dbc);
                }
                RESET_FIELD (gr, Db);
                ChangeMade (CCleanupChange::eChangeDbxrefs);
            }
            if (s_IsEmptyGeneRef (gr)) {
                ERASE_SEQFEATXREF_ON_SEQFEAT (xr_itr, sf);
                ChangeMade (CCleanupChange::eChangeDbxrefs);
            }
        }
    }
}

void CNewCleanup_imp::ProtrefBC (
    CProt_ref& pr
)

{
    CLEAN_STRING_MEMBER (pr, Desc);
    if (CleanStringList (GET_MUTABLE (pr, Name))) {
        ChangeMade (CCleanupChange::eChangeProtNames);
    }
    CLEAN_STRING_LIST_JUNK (pr, Ec);
    CLEAN_STRING_LIST (pr, Activity);
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
    return sc_UninfNames.find(name) == sc_UninfNames.end();
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
    if (FIELD_IS_SET (pr, Processed)) {
        TPROTREF_PROCESSED processed = GET_FIELD (pr, Processed);

        // move putative from comment to protein name for mat peptide
        if (FIELD_IS_SET (sf, Comment) && ! FIELD_IS_SET (pr, Name) &&
            processed != NCBI_PROTREF(signal_peptide) &&
            processed != NCBI_PROTREF(transit_peptide)) {
            if (NStr::EqualNocase ("putative", GET_FIELD (sf, Comment))) {
                ADD_NAME_TO_PROTREF (pr, "putative");
                ChangeMade (CCleanupChange::eChangeQualifiers);
                RESET_FIELD (sf, Comment);
                ChangeMade (CCleanupChange::eRemoveComment);
            }
        }

        // move putative to comment, remove uninformative name of signal peptide
        if (FIELD_IS_SET (pr, Name)) {
            if (processed == NCBI_PROTREF(signal_peptide) ||
                processed == NCBI_PROTREF(transit_peptide)) {
                EDIT_EACH_NAME_ON_PROTREF (nm_itr, pr) {
                    string& str = *nm_itr;
                    if (NStr::Find (str, "putative") != NPOS ||
                        NStr::Find (str, "put.") != NPOS) {
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
        }

        // add unnamed as default protein name
        if (! FIELD_IS_SET (pr, Name)) {
            if (processed == NCBI_PROTREF(preprotein)  ||  
                processed == NCBI_PROTREF(mature)) {
                ADD_NAME_TO_PROTREF (pr, "unnamed");
                ChangeMade (CCleanupChange::eChangeQualifiers);
            }
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
    return sc_NcrnafNames.find(name.c_str()) == sc_NcrnafNames.end();
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
                    string& str = GET_MUTABLE (ext, Name);
                    if (CleanString (str)) {
                        ChangeMade (CCleanupChange::eTrimSpaces);
                    }
                    if (NStr::IsBlank (str)) {
                        RESET_FIELD (rr, Ext);
                    }
                }
                break;
            case NCBI_RNAEXT(TRNA):
                {
                    CTrna_ext& trn = GET_MUTABLE (ext, TRNA);
                    if (FIELD_IS_SET (trn, Aa)) {
                        const CTrna_ext::C_Aa& aa = GET_FIELD (trn, Aa);
                        if (aa.Which() == CTrna_ext::C_Aa::e_not_set) {
                            RESET_FIELD (trn, Aa);
                        }
                    }
                    if (! FIELD_IS_SET (trn, Aa) &&
                        ! FIELD_IS_SET (trn, Codon) &&
                        ! FIELD_IS_SET (trn, Anticodon)) {
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
                            const string& str = GET_FIELD (qual, Qual);
                            if (NStr::IsBlank (str)) {
                                RESET_FIELD (gen, Quals);
                            } else {
                                const string& str = GET_FIELD (qual, Val);
                                if (NStr::IsBlank (str)) {
                                    RESET_FIELD (gen, Quals);
                                }
                            }
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
                }
                break;
            case NCBI_RNAREF(rRNA):
                {
                }
                break;
            case NCBI_RNAREF(other):
                {
                    if (FIELD_IS_SET (rr, Ext)) {
                        CRNA_ref::C_Ext& ext = GET_MUTABLE (rr, Ext);
                        TRNAREF_EXT chs = ext.Which();
                        if (chs == NCBI_RNAEXT(Name)) {
                            string& str = GET_MUTABLE (ext, Name);
                            if (NStr::EqualNocase (str, "miscRNA") || NStr::EqualNocase (str, "misc_RNA")) {
                            } else if (NStr::EqualNocase (str, "ncRNA")) {
                            } else if (NStr::EqualNocase (str, "tmRNA")) {
                            } else if (s_IsNcrnaName (str)) {
                            } else {
                            }
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
}

static bool s_CommentRedundantWithRnaRef (
    CRNA_ref& rr,
    const string& comm
)

{
    // !!! need to implement !!!

    return false;
}

void CNewCleanup_imp::RnaFeatBC (
    CRNA_ref& rr,
    CSeq_feat& sf
)

{
}

void CNewCleanup_imp::PubFeatBC (
    CPubdesc& pub,
    CSeq_feat& sf
)

{
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

void CNewCleanup_imp::SeqfeatDataBC (
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
                RnarefBC (rr);
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
        /*
        case NCBI_SEQFEAT(Seq):
            break;
        case NCBI_SEQFEAT(Imp):
            BasicCleanup (GET_MUTABLE (sfd, Imp));
            break;
        case NCBI_SEQFEAT(Region):
            break;
        case NCBI_SEQFEAT(Comment):
            break;
        case NCBI_SEQFEAT(Bond):
            break;
        case NCBI_SEQFEAT(Site):
            break;
        case NCBI_SEQFEAT(Rsite):
            break;
        */
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

