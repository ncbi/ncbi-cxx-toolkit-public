#ifndef CLEANUP___CLEANUPP__HPP
#define CLEANUP___CLEANUPP__HPP

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
*   Implementation of Basic Cleanup of CSeq_entries.
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

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

class CCleanupChange;

/// right now a slightly different cleanup is performed for EMBL/DDBJ and
/// SwissProt records. All other types are handaled as GenBank records.
enum ECleanupMode
{
    eCleanup_EMBL,
    eCleanup_DDBJ,
    eCleanup_SwissProt,
    eCleanup_GenBank
};


class CCleanup_imp
{
public:
    CCleanup_imp(CRef<CCleanupChange> changes, Uint4 options = 0);
    virtual ~CCleanup_imp();
    
    /// Cleanup a Seq-entry. 
    void BasicCleanup(CSeq_entry& se);
    /// Cleanup a Seq-submit.
    void BasicCleanup(CSeq_submit& ss);
    /// Cleanup a Bioseq. 
    void BasicCleanup(CBioseq& bs);
    /// Cleanup a Bioseq_set.
    void BasicCleanup(CBioseq_set& bss);
    /// Cleanup a Seq-Annot. 
    void BasicCleanup(CSeq_annot& sa,  ECleanupMode mode = eCleanup_GenBank);
    /// Cleanup a Seq-feat.
    void BasicCleanup(CSeq_feat& sf,   ECleanupMode mode = eCleanup_GenBank);

private:
    // Cleanup other sub-objects.
    void BasicCleanup(CSeq_descr& sdr, ECleanupMode mode = eCleanup_GenBank);
    void BasicCleanup(CSeqdesc& sd,    ECleanupMode mode = eCleanup_GenBank);
    void BasicCleanup(CSeqFeatData& data);
    void BasicCleanup(CGene_ref& gene_ref);
    void BasicCleanup(CProt_ref& prot_ref);
    void BasicCleanup(CRNA_ref& rna_ref);
    void BasicCleanup(CImp_feat& imf);
    void BasicCleanup(CGb_qual& gbq);
    void BasicCleanup(CDbtag& dbtag);
    void BasicCleanup(CGB_block& gbb);
    void BasicCleanup(CPubdesc& pd);
    void BasicCleanup(CPub_equiv& pe);
    void BasicCleanup(CPub& pub, bool fix_initials);
    void BasicCleanup(CCit_gen& cg, bool fix_initials);
    void BasicCleanup(CCit_sub& cs, bool fix_initials);
    void BasicCleanup(CCit_art& ca, bool fix_initials);
    void BasicCleanup(CCit_book& cb, bool fix_initials);
    void BasicCleanup(CCit_pat&  cp, bool fix_initials);
    void BasicCleanup(CCit_let&  cl, bool fix_initials);
    void BasicCleanup(CCit_proc& cp, bool fix_initials);
    void BasicCleanup(CAuth_list& al, bool fix_initials);
    void BasicCleanup(CAuthor& au, bool fix_initials);
    void BasicCleanup(CAffil& af);
    void BasicCleanup(CPerson_id& pid, bool fix_initials);
    void BasicCleanup(CName_std& name, bool fix_initials);
    void BasicCleanup(CBioSource& bs);
    void BasicCleanup(COrg_ref& oref);
    void BasicCleanup(COrgName& on);
    void BasicCleanup(COrgMod& om);
    void BasicCleanup(CSubSource& ss);
    void BasicCleanup(CObject_id& oid);
    void BasicCleanup(CUser_field& field);
    void BasicCleanup(CUser_object& uo);

    void BasicCleanup(CSeq_feat& feat, CSeqFeatData& data, bool is_embl_or_ddbj);
    bool BasicCleanup(CSeq_feat& feat, CGb_qual& qual,  bool is_embl_or_ddbj);

    // cleaning up Seq_feat parts.
    void x_CleanupExcept_text(string& except_text);
    void x_MoveDbxrefToFeat(CSeq_feat& feat);
    void x_CleanupRna(CSeq_feat& feat);
    void x_AddReplaceQual(CSeq_feat& feat, const string& str);
    void x_TrimParenthesesAndCommas(string& str);
    void x_CombineSplitQual(string& val, string& new_val);
    bool x_HandleGbQualOnGene(CSeq_feat& feat, const string& qual, const string& val);
    bool x_HandleGbQualOnCDS(CSeq_feat& feat, const string& qual, const string& val);
    bool x_HandleGbQualOnRna(CSeq_feat& feat, const string& qual, const string& val, bool is_embl_or_ddbj);
    bool x_HandleGbQualOnProt(CSeq_feat& feat, const string& qual, const string& val);
    void x_CleanupDbxref(CSeq_feat& feat);

    // Gb_qual cleanup.
    void x_ExpandCombinedQuals(CSeq_feat::TQual& quals);
    void x_SortUniqueQuals(CSeq_feat::TQual& quals);
    void x_CleanupConsSplice(CGb_qual& gbq);
    void x_CleanupRptUnit(CGb_qual& gbq);

    // Dbtag cleanup.
    void x_TagCleanup(CObject_id& tag);
    void x_DbCleanup(string& db);
    
    // pub axuiliary functions.
    void x_FlattenPubEquiv(CPub_equiv& pe);

    // CName_std cleanup 
    void x_FixEtAl(CName_std& name);
    void x_FixInitials(CName_std& name);
    void x_ExtractSuffixFromInitials(CName_std& name);
    void x_FixSuffix(CName_std& name);

    void x_OrgModToSubtype(CBioSource& bs);
    void x_SubtypeCleanup(CBioSource& bs);
    void x_ModToOrgMod(COrg_ref& oref);
    // cleanup strings in User objects and fields
    void x_CleanupUserString(string& str);

    // Prohibit copy constructor & assignment operator
    CCleanup_imp(const CCleanup_imp&);
    CCleanup_imp& operator= (const CCleanup_imp&);
    
    CRef<CCleanupChange>    m_Changes;
    Uint4                   m_Options;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.6  2006/04/05 14:01:22  dicuccio
 * Don't export internal classes
 *
 * Revision 1.5  2006/03/29 19:42:16  rsmith
 * Delete   x_CleanupExtName(CRNA_ref& rna_ref);
 * and x_CleanupExtTRNA(CRNA_ref& rna_ref);
 *
 * Revision 1.4  2006/03/29 16:38:14  rsmith
 * various implementation declaration changes.
 *
 * Revision 1.3  2006/03/23 18:31:40  rsmith
 * new BasicCleanup routines, particularly User Objects/Fields.
 *
 * Revision 1.2  2006/03/20 14:22:15  rsmith
 * add cleanup for CSeq_submit
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */

#endif  /* CLEANUP___CLEANUP__HPP */
