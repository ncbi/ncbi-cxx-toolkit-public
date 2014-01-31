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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Mati Shomrat, ....
 *
 * File Description:
 *   Implementation of private parts of the validator
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>

#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/PubStatus.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>

#include <objtools/error_codes.hpp>
#include <util/sgml_entity.hpp>
#include <util/line_reader.hpp>

#include <algorithm>


#include <serial/iterator.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


void CValidError_imp::ValidatePubdesc
(const CPubdesc& pubdesc,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    int uid = 0, pmid = 0, muid = 0;
    bool conflicting_pmids = false, redundant_pmids = false, conflicting_muids = false, redundant_muids = false;

    ValidatePubHasAuthor(pubdesc, obj, ctx);

    // need to get uid (pmid or muid) in first pass for ValidatePubArticle
    FOR_EACH_PUB_ON_PUBDESC (pub_iter, pubdesc) {
        const CPub& pub = **pub_iter;

        switch( pub.Which() ) {
        case CPub::e_Muid:
            if ( muid == 0 ) {
                muid = pub.GetMuid();
            } else if ( muid != pub.GetMuid() ) {
                conflicting_muids = true;
            } else {
                redundant_muids = true;
            }
            if ( uid == 0 ) {
                uid = pub.GetMuid();
            }
            break;

        case CPub::e_Pmid:
            if ( pmid == 0 ) {
                pmid = pub.GetPmid();
            } else if ( pmid != pub.GetPmid() ) {
                conflicting_pmids = true;
            } else {
                redundant_pmids = true;
            }
            if ( uid == 0 ) {
                uid = pub.GetPmid();
            }
            break;
            
        default:
            break;
        }
    }

    if ( conflicting_pmids ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications, 
                "Multiple conflicting pmids in a single publication", obj, ctx);
    }
    if ( redundant_pmids ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications, 
                "Multiple redundant pmids in a single publication", obj, ctx);
    }
    if ( conflicting_muids ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications, 
                "Multiple conflicting muids in a single publication", obj, ctx);
    }
    if ( redundant_muids ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_CollidingPublications, 
                "Multiple redundant muids in a single publication", obj, ctx);
    }

    // second pass for remaining (non-uid) types
    FOR_EACH_PUB_ON_PUBDESC (pub_iter, pubdesc) {
        const CPub& pub = **pub_iter;

        switch( pub.Which() ) {
        case CPub::e_Gen:
            ValidatePubGen(pub.GetGen(), obj, ctx);
            break;

        case CPub::e_Sub:
            ValidateCitSub(pub.GetSub(), obj, ctx);
            break;

        case CPub::e_Medline:
            PostObjErr(eDiag_Error, eErr_GENERIC_MedlineEntryPub, 
                "Publication is medline entry", obj, ctx);
            break;

        /*
        case CPub::e_Muid:
            if ( uid == 0 ) {
                uid = pub.GetMuid();
            }
            break;

        case CPub::e_Pmid:
            if ( uid == 0 ) {
                uid = pub.GetPmid();
            }
            break;
        */
            
        case CPub::e_Article:
            ValidatePubArticle(pub.GetArticle(), uid, obj, ctx);
            if (pubdesc.IsSetComment() && !NStr::IsBlank(pubdesc.GetComment())
                && pub.GetArticle().IsSetFrom() && pub.GetArticle().GetFrom().IsJournal()
                && pub.GetArticle().GetFrom().GetJournal().IsSetImp()
                && pub.GetArticle().GetFrom().GetJournal().GetImp().IsSetPubstatus()) {
                CImprint::TPubstatus pubstatus = pub.GetArticle().GetFrom().GetJournal().GetImp().GetPubstatus();
                const string& comment = pubdesc.GetComment();
                if ((pubstatus == ePubStatus_epublish 
                     || pubstatus == ePubStatus_ppublish
                     || pubstatus == ePubStatus_aheadofprint)
                    && (NStr::Find(comment, "Publication Status") != string::npos
                        || NStr::Find(comment, "Publication-Status") != string::npos
                        || NStr::Find(comment, "Publication_Status") != string::npos)) {
                    PostObjErr(eDiag_Error, eErr_GENERIC_UnexpectedPubStatusComment,
                               "Publication status is in comment for pmid " + NStr::IntToString (uid),
                               obj, ctx);
                }
            }
            break;

        case CPub::e_Equiv:
            PostObjErr(eDiag_Warning, eErr_GENERIC_UnnecessaryPubEquiv,
                "Publication has unexpected internal Pub-equiv", obj, ctx);
            break;

        default:
            break;
        }
    }
    if (pubdesc.IsSetPub()) {
        ValidateAuthorsInPubequiv (pubdesc.GetPub(), obj, ctx);
    }

}


static bool s_CitGenIsJustBackBoneIDNumber (const CCit_gen& gen)
{
    if (gen.IsSetCit() 
        && NStr::StartsWith (gen.GetCit(), "BackBone id_pub = ") 
        && !gen.IsSetJournal() 
        && !gen.IsSetDate() 
        && !gen.IsSetSerial_number()) {
        return true;
    } else {
        return false;
    }
}


void CValidError_imp::ValidatePubGen
(const CCit_gen& gen,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    bool is_unpub = false;
    if ( gen.IsSetCit()  &&  !gen.GetCit().empty() ) {
        const string& cit = gen.GetCit();
        // skip if just BackBone id number
        if (s_CitGenIsJustBackBoneIDNumber(gen)) {
            return;
        }

        if (NStr::StartsWith (cit, "submitted", NStr::eNocase)
            || NStr::StartsWith (cit, "unpublished", NStr::eNocase)
            || NStr::StartsWith (cit, "Online Publication", NStr::eNocase)
            || NStr::StartsWith (cit, "Published Only in DataBase", NStr::eNocase)
            || NStr::StartsWith (cit, "(er) ", NStr::eNocase)) {
            is_unpub = true;
        } else {            
            PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
                "Unpublished citation text invalid", obj, ctx);
        }

        if (NStr::FindCase (cit, "Title=") != string::npos) {
            PostObjErr(eDiag_Error, eErr_GENERIC_StructuredCitGenCit, 
                    "Unpublished citation has embedded Title", obj, ctx);
        }
        if (NStr::FindCase (cit, "Journal=") != string::npos) {
            PostObjErr(eDiag_Error, eErr_GENERIC_StructuredCitGenCit, 
                    "Unpublished citation has embedded Journal", obj, ctx);
        }

    }
    if (gen.IsSetSerial_number()) {
        m_PubSerialNumbers.push_back(gen.GetSerial_number());
        /* date not required if just serial number */
        if (!gen.IsSetCit() && !gen.IsSetJournal() && !gen.IsSetDate()) {
            return;
        }
    }
    if (!gen.IsSetDate()) {
        if (!is_unpub) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_MissingPubInfo, "Publication date missing", obj, ctx);
        }
    } else if (gen.GetDate().IsStr()) {
        if (NStr::Equal(gen.GetDate().GetStr(), "?")) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_MissingPubInfo, "Publication date marked as '?'", obj, ctx);
        }
    } else if (gen.GetDate().IsStd() && (!gen.GetDate().GetStd().IsSetYear() || gen.GetDate().GetStd().GetYear() == 0)) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_MissingPubInfo, "Publication date not set", obj, ctx);
    } else {
        int rval = CheckDate (gen.GetDate());
        if (rval != eDateValid_valid) {
            PostBadDateError (eDiag_Error, "Publication date has error", rval, obj, ctx);
        }
    }
}


void CValidError_imp::ValidatePubArticle
(const CCit_art& art,
 int uid,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( !art.IsSetTitle()  ||  !HasTitle(art.GetTitle()) ) { 
        PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Publication has no title", obj, ctx);
    }
        
    if ( art.GetFrom().IsJournal() ) {
        const CCit_jour& jour = art.GetFrom().GetJournal();
        
        bool has_iso_jta = HasIsoJTA(jour.GetTitle());
        bool in_press = false;
        bool is_electronic_journal = false;

        if ( !HasTitle(jour.GetTitle()) ) {
            PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Journal title missing", obj, ctx);
        }

        ITERATE (CTitle::Tdata, item, jour.GetTitle().Get() ) {
            if ((*item)->Which() == CTitle::C_E::e_Name
                && NStr::StartsWith((*item)->GetName(), "(er)")) {
                is_electronic_journal = true;
                break;
            }
        }

        if ( jour.CanGetImp() ) {
            const CImprint& imp = jour.GetImp();

            if ( imp.CanGetPrepub() ) {
                in_press =  imp.GetPrepub() == CImprint::ePrepub_in_press;
                if ( in_press ) {
                    if ( imp.IsSetPages() ) {
                        if (! NStr::IsBlank(imp.GetPages()) ) {
                            PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, 
                                     "In-press is not expected to have page numbers", obj, ctx);
                        }
                    }
                    if ( (! imp.IsSetDate()) || (imp.GetDate().IsStr() && NStr::Equal (imp.GetDate().GetStr(), "?"))) {
                        PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                    "In-press is missing the date", obj, ctx);
                    }
                }
            }

            if (imp.IsSetPubstatus() 
                && (imp.GetPubstatus() == ePubStatus_epublish || imp.GetPubstatus() == ePubStatus_aheadofprint)) {
                is_electronic_journal = true;
            }
            if ( !imp.IsSetPrepub()        &&  
                 (!imp.CanGetPubstatus()   ||  
                  imp.GetPubstatus() != ePubStatus_aheadofprint) ) {
                bool no_vol = !imp.IsSetVolume()  || 
                              NStr::IsBlank(imp.GetVolume());
                bool no_pages = !imp.IsSetPages()  ||
                                NStr::IsBlank(imp.GetPages());

                if ( no_vol ) {
                    if (is_electronic_journal) {
                        PostObjErr(eDiag_Info, eErr_GENERIC_MissingVolumeEpub,
                            "Electronic journal volume missing", obj, ctx);
                    } else {
                        PostObjErr(eDiag_Warning, eErr_GENERIC_MissingVolume,
                            "Journal volume missing", obj, ctx);
                    }
                }
                if ( no_pages ) {
                    if (is_electronic_journal) {
                        PostObjErr(eDiag_Info, eErr_GENERIC_MissingPagesEpub,
                            "Electronic journal pages missing", obj, ctx);
                    } else {
                        PostObjErr(eDiag_Warning, eErr_GENERIC_MissingPages,
                            "Journal pages missing", obj, ctx);
                    }
                }
                
                if ( !no_pages ) {
                    x_ValidatePages(imp.GetPages(), obj, ctx);
                }
                if (imp.IsSetDate() && imp.GetDate().Which() != CDate::e_not_set) {
                    if (imp.GetDate().IsStr() && NStr::Equal (imp.GetDate().GetStr(), "?")) {
                        PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                    "Publication date marked as '?'", obj, ctx);
                    } else if (imp.GetDate().IsStd()) {
                        if (!imp.GetDate().GetStd().IsSetYear()) {
                            PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                        "Publication date missing", obj, ctx);
                        } else if (imp.GetDate().GetStd().GetYear() == 0) {
                            PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                        "Publication date not set", obj, ctx);
                        } else {
                            int rval = CheckDate (imp.GetDate());
                            if (rval != eDateValid_valid) {
                                PostBadDateError (eDiag_Error, "Publication date has error", rval, obj, ctx);
                            }
                        }
                    }
                } else {
                    PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                                "Publication date missing", obj, ctx);
                }
            }
            if (imp.IsSetPubstatus()) {
                CImprint::TPubstatus pubstatus = imp.GetPubstatus();
                if (pubstatus == ePubStatus_aheadofprint
                    && (!imp.IsSetPrepub() || imp.GetPrepub() != CImprint::ePrepub_in_press)) {
                    bool noVol = !imp.IsSetVolume() || NStr::IsBlank(imp.GetVolume());
                    bool noPages = !imp.IsSetPages() || NStr::IsBlank(imp.GetPages());
                    if (!noVol && !noPages && uid == 0) {
                        PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, 
                                 "Ahead-of-print without in-press", obj, ctx);
                    }
                }  
                if (pubstatus == ePubStatus_epublish 
                    && imp.IsSetPrepub() && imp.GetPrepub() == CImprint::ePrepub_in_press) {
                    PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, 
                             "Electronic-only publication should not also be in-press", obj, ctx);
                }
            }
        }
        if ( !has_iso_jta  &&  (uid > 0  ||  in_press  ||  IsRequireISOJTA()) && !is_electronic_journal ) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_MissingISOJTA,
                "ISO journal title abbreviation missing", obj, ctx);
        }
    }
}


bool s_GetDigits(const string& pages, string& digits)
{
    string::size_type pos = 0;
    string::size_type len = pages.length();

    digits.erase();

    // skip alpha at the begining 
    while (pos < len  &&  !isdigit((unsigned char) pages[pos])) {
        ++pos;
    }

    while (pos < len  &&  isdigit((unsigned char) pages[pos])) {
        digits += pages[pos];
        ++pos;
    }

    _ASSERT (pos >= len  ||  !isdigit((unsigned char) pages[pos]));

    while (pos < len) {
        if (isdigit((unsigned char) pages[pos])) {
            digits.erase();
            return false;
        }
        ++pos;
    }

    return true;
}


void CValidError_imp::x_ValidatePages
(const string& pages,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    static const string kRoman = "IVXLCDM";

    if (pages.empty()) {
        return;
    }

    EDiagSev sev = eDiag_Warning;
    
    string start, stop;
    if (!NStr::SplitInTwo(pages, "-", start, stop) || start.empty() || stop.empty()) {
        return;
    }

    NStr::ReplaceInPlace(start, " ", "");
    NStr::ReplaceInPlace(stop, " ", "");

    int p1 = 0, p2 = 0;
    bool start_good = false, stop_good = false;
    size_t num_digits = 0, num_chars = 0;

    if (start.c_str()[0] == '-') {
        num_chars++;
    }
    while (isdigit (start.c_str()[num_chars])) {
        num_digits++;
        num_chars++;
    }
    if (num_digits == 0) {
        if (!isalpha(start.c_str()[0])) {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering, "Page numbering start looks strange", obj, ctx);
        }
    } else {
        start_good = true;
        p1 = NStr::StringToInt (start.substr(0, num_digits), NStr::fConvErr_NoThrow);

        num_digits = 0;
        num_chars = 0;
        if (stop.c_str()[0] == '-') {
            num_chars++;
        }
        while (isdigit (stop.c_str()[num_chars])) {
          num_digits++;
          num_chars++;
        }
        if (num_digits == 0) {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering, "Page numbering stop looks strange", obj, ctx);
        } else {
            stop_good = true;
            p2 = NStr::StringToInt (stop.substr(0, num_digits), NStr::fConvErr_NoThrow);
        }

        if ((start_good && p1 == 0) || (stop_good && p2 == 0)) {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering, "Page numbering has zero value", obj, ctx);
        } else if ((start_good && p1 < 0) || (stop_good && p2 < 0)) {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering, "Page numbering has negative value", obj, ctx);
        } else if (start_good && stop_good && p1 > p2) {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering, "Page numbering out of order", obj, ctx);
        } else if (start_good && stop_good && p2 > p1 + 50) {
            PostObjErr(sev, eErr_GENERIC_BadPageNumbering, "Page numbering greater than 50", obj, ctx);
        }
    }
}


bool CValidError_imp::HasTitle(const CTitle& title)
{
    ITERATE (CTitle::Tdata, item, title.Get() ) {
        const string *str = 0;
        switch ( (*item)->Which() ) {
        case CTitle::C_E::e_Name:
            str = &(*item)->GetName();
            break;

        case CTitle::C_E::e_Tsub:
            str = &(*item)->GetTsub();
            break;

        case CTitle::C_E::e_Trans:
            str = &(*item)->GetTrans();
            break;

        case CTitle::C_E::e_Jta:
            str = &(*item)->GetJta();
            break;

        case CTitle::C_E::e_Iso_jta:
            str = &(*item)->GetIso_jta();
            break;

        case CTitle::C_E::e_Ml_jta:
            str = &(*item)->GetMl_jta();
            break;

        case CTitle::C_E::e_Coden:
            str = &(*item)->GetCoden();
            break;

        case CTitle::C_E::e_Issn:
            str = &(*item)->GetIssn();
            break;

        case CTitle::C_E::e_Abr:
            str = &(*item)->GetAbr();
            break;

        case CTitle::C_E::e_Isbn:
            str = &(*item)->GetIsbn();
            break;

        default:
            break;
        };
        if ( !NStr::IsBlank(*str) ) {
            return true;
        }
    }
    return false;
}


bool CValidError_imp::HasIsoJTA(const CTitle& title)
{
    ITERATE (CTitle::Tdata, item, title.Get() ) {
        if ( (*item)->IsIso_jta() ) {
            return true;
        }
    }
    return false;
}


bool CValidError_imp::HasName(const CAuth_list& authors)
{
    if ( authors.CanGetNames() ) {
        const CAuth_list::TNames& names = authors.GetNames();
        switch ( names.Which() ) {
            case CAuth_list::TNames::e_Std:
                ITERATE ( list< CRef< CAuthor > >, auth, names.GetStd() ) {
                    const CPerson_id& pid = (*auth)->GetName();
                    if ( pid.IsName() ) {
                        if ( ! NStr::IsBlank(pid.GetName().GetLast()) ) {
                            return true;
                        }
                    } else if ( pid.IsConsortium() ) {
                        if ( ! NStr::IsBlank (pid.GetConsortium()) ) {
                            return true;
                        }
                    }
                }
                break;
                
            case CAuth_list::TNames::e_Ml:
                if ( ! IsBlankStringList(names.GetMl()) ) {
                    return true;
                }
                break;
                
            case CAuth_list::TNames::e_Str:
                if ( ! IsBlankStringList(names.GetStr()) ) {
                    return true;
                }
                break;
                
            default:
                break;
        }
    }
    return false;
}


void CValidError_imp::ValidatePubHasAuthor
(const CPubdesc& pubdesc,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    bool has_name = false;
    FOR_EACH_PUB_ON_PUBDESC (pub_iter, pubdesc) {
        const CPub& pub = **pub_iter;
        switch (pub.Which() ) {
            case CPub::e_Gen:
                // don't check if just serial number
                if (!pub.GetGen().IsSetCit() 
                    && !pub.GetGen().IsSetJournal()
                    && !pub.GetGen().IsSetDate()
                    && pub.GetGen().IsSetSerial_number()
                    && pub.GetGen().GetSerial_number() > -1) {
                    // skip
                } else if (s_CitGenIsJustBackBoneIDNumber(pub.GetGen())) {
                    // just BackBoneID, skip
                } else {
                    has_name = false;
                    if ( pub.GetGen().IsSetAuthors() 
                         && HasName(pub.GetGen().GetAuthors())) {
                         has_name = true;
                    }
                    if (!has_name) {
                        PostObjErr(IsRefSeq() ? eDiag_Warning : eDiag_Error,
                                   eErr_GENERIC_MissingPubInfo,
                                   "Publication has no author names", obj, ctx);
                    }
                }
                break;
            case CPub::e_Article:
                has_name = false;
                if ( pub.GetArticle().IsSetAuthors() 
                    && HasName(pub.GetArticle().GetAuthors())) {
                     has_name = true;
                }
                if (!has_name) {
                    PostObjErr(IsRefSeq() ? eDiag_Warning : eDiag_Error,
                               eErr_GENERIC_MissingPubInfo,
                               "Publication has no author names", obj, ctx);
                }
                break;
            default:
                break;
        }
        
    }    
}


static bool s_BadCharsInAuthorName (string str, string& badauthor, bool allowcomma, bool allowperiod, bool last)
{
    if (NStr::IsBlank (str)) {
        return false;
    }


    size_t stp = string::npos;
    if (last) {
        if (NStr::StartsWith (str, "St.")) {
            stp = 2;
        } else if (NStr::StartsWith (str, "de M.")) {
            stp = 4;
        }
    }

    size_t pos = 0;
    const char *ptr = str.c_str();

    while (*ptr != 0) {
        if (isalpha (*ptr)
            || *ptr == '-'
            || *ptr == '\''
            || *ptr == ' '
            || (*ptr == ',' && allowcomma)
            || (*ptr == '.' && (allowperiod || pos == stp))) {
            // all these are ok
            ptr++;
            pos++;
        } else {
            badauthor = str;
            return true;
        }
    }
    return false;
}


static bool s_BadCharsInAuthor (const CAuthor& author, string& badauthor, bool& last_is_bad)
{
    badauthor = "";
    last_is_bad = false;

    if (author.IsSetName() && author.GetName().IsName()) {
        if (author.GetName().GetName().IsSetLast()
            && NStr::EqualNocase (author.GetName().GetName().GetLast(), "et al.")) {
            return false;
        }
        if (author.GetName().GetName().IsSetLast()
            && s_BadCharsInAuthorName (author.GetName().GetName().GetLast(), badauthor, false, false, true)) {
            last_is_bad = true;
            return true;
        }
        if (author.GetName().GetName().IsSetFirst()
            && s_BadCharsInAuthorName (author.GetName().GetName().GetFirst(), badauthor, false, true, false)) {
            return true;
        }
        // for testing, remove
        if (author.GetName().GetName().IsSetInitials() 
            && NStr::StartsWith(author.GetName().GetName().GetInitials(), "J.S.R")) {
            string foo = "";
            foo = author.GetName().GetName().GetInitials();
        }

        if (author.GetName().GetName().IsSetInitials()
            && s_BadCharsInAuthorName (author.GetName().GetName().GetInitials(), badauthor, false, true, false)) {
            return true;
        }
    }
    return false;
}


void CValidError_imp::ValidateAuthorList 
(const CAuth_list::C_Names& names,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if (names.IsStd()) {
        list<string> consortium_list;

        ITERATE ( CAuth_list::C_Names::TStd, name, names.GetStd() ) {
            string badauthor = "";
            bool   last_is_bad = false;
            if (s_BadCharsInAuthor (**name, badauthor, last_is_bad)) {
                if (last_is_bad) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadCharInAuthorLastName, 
                                "Bad characters in author " + badauthor, obj, ctx);
                } else {
                    PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadCharInAuthorName,
                                "Bad characters in author " + badauthor, obj, ctx);
                }
            }
            if ( (*name)->GetName().IsName() ) {
                const CName_std& nstd = (*name)->GetName().GetName();
                string last = "";
                if (nstd.IsSetLast()) {
                    last = nstd.GetLast();
                    NStr::ReplaceInPlace (last, ".", " ");
                    NStr::ReplaceInPlace (last, "  ", " ");
                    NStr::TruncateSpacesInPlace (last);
                }
                string initials = "";
                if (nstd.IsSetInitials()) {
                    initials = nstd.GetInitials();
                    NStr::ReplaceInPlace (initials, ".", " ");
                    NStr::ReplaceInPlace (initials, "  ", " ");
                    NStr::TruncateSpacesInPlace (initials);
                }
                if ( (NStr::CompareNocase(last, "et al") == 0)  ||
                     (NStr::CompareNocase(last, "et") == 0  
                      &&  NStr::CompareNocase(initials, "al") == 0
                      &&  (!nstd.IsSetFirst() || nstd.GetFirst().empty()))) {
                    CAuth_list::C_Names::TStd::const_iterator temp = name;
                    if ( ++temp == names.GetStd().end() ) {
                        PostObjErr(eDiag_Warning, eErr_GENERIC_AuthorListHasEtAl,
                            "Author list ends in et al.", obj, ctx);
                    } else {
                        PostObjErr(eDiag_Warning, eErr_GENERIC_AuthorListHasEtAl,
                            "Author list contains et al.", obj, ctx);
                    }
                }
                // validate suffix, if set and nonempty
                if (nstd.IsSetSuffix() && !NStr::IsBlank (nstd.GetSuffix())) {
                    string suffix = nstd.GetSuffix();

                    typedef CName_std::TSuffixes TSuffixes;
                    const TSuffixes& suffixes = CName_std::GetStandardSuffixes();
                    bool found = false;
                    ITERATE (TSuffixes, it, suffixes) {
                        if (NStr::EqualNocase (suffix, *it)) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadAuthorSuffix, 
                                 "Bad author suffix " + suffix,
                                 obj, ctx);
                    }
                }
            } else if ( (*name)->GetName().IsConsortium() ) {
                const string& consortium = (*name)->GetName().GetConsortium();
                if (NStr::IsBlank (consortium)) {
                    PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, "Empty consortium", obj, ctx);
                } else {
                    bool found = false;
                    ITERATE (list<string>, cons_str, consortium_list) {
                        if (NStr::EqualNocase (consortium, *cons_str)) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        PostObjErr (eDiag_Warning, eErr_GENERIC_PublicationInconsistency, 
                                 "Duplicate consortium '" + consortium + "'", obj, ctx);
                    } else {
                        consortium_list.push_back(consortium);
                    }
                }
            }
        }
    } else if (names.IsMl()) {
        ITERATE ( list< string >, str, names.GetMl()) {
            string badauthor = "";
            if (s_BadCharsInAuthorName (*str, badauthor, true, true, false)) {
                if (NStr::IsBlank (badauthor)) {
                    badauthor = "?";
                }
                PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadCharInAuthorName, 
                            "Bad characters in author " + badauthor, obj, ctx);
            }
        }           
    } else if (names.IsStr()) {

        ITERATE ( list< string >, str, names.GetStr()) {
            string badauthor = "";
            if (s_BadCharsInAuthorName (*str, badauthor, true, true, false)) {
                if (NStr::IsBlank (badauthor)) {
                    badauthor = "?";
                }
                PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_BadCharInAuthorName, 
                            "Bad characters in author " + badauthor, obj, ctx);
            }
        }           
    }
}


void CValidError_imp::ValidateAuthorsInPubequiv 
(const CPub_equiv& pe,
 const CSerialObject& obj,
 const CSeq_entry* ctx)
{
    FOR_EACH_PUB_ON_PUBEQUIV (pub_iter, pe) {
        const CPub& pub = **pub_iter;
        const CAuth_list* authors = 0;
        switch ( pub.Which() ) {
        case CPub::e_Gen:
            if ( pub.GetGen().IsSetAuthors() ) {
                authors = &(pub.GetGen().GetAuthors());
            }
            break;
        case CPub::e_Sub:
            authors = &(pub.GetSub().GetAuthors());
            break;
        case CPub::e_Article:
            if ( pub.GetArticle().IsSetAuthors() ) {
                authors = &(pub.GetArticle().GetAuthors());
            }
            break;
        case CPub::e_Book:
            authors = &(pub.GetBook().GetAuthors());
            break;
        case CPub::e_Proc:
            authors = &(pub.GetProc().GetBook().GetAuthors());
            break;
        case CPub::e_Man:
            authors = &(pub.GetMan().GetCit().GetAuthors());
            break;
        case CPub::e_Patent:
            authors = &(pub.GetPatent().GetAuthors());
            break;
        case CPub::e_Equiv:
            ValidateAuthorsInPubequiv (pub.GetEquiv(), obj, ctx);
            break;
        default:
            break;
        }

        if ( !authors ) {
            continue;
        }

        const CAuth_list::C_Names& names = authors->GetNames();
        ValidateAuthorList (names, obj, ctx);
    }
}


static bool s_IsRefSeqInSep(const CSeq_entry& se, CScope& scope)
{
    for (CBioseq_CI it(scope, se); it; ++it) {
        FOR_EACH_SEQID_ON_BIOSEQ (id, *(it->GetCompleteBioseq())) {
            if ((*id)->IsOther()) {
                const CTextseq_id* tsip = (*id)->GetTextseq_Id();
                if (tsip != NULL  &&  tsip->IsSetAccession()) {
                    return true;
                }
            }
        }
    }
    return false;
}


static bool s_IsHtgInSep(const CSeq_entry& se)
{
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY (it, se) {
        if ((*it)->Which() == CSeqdesc::e_Molinfo) {
            CMolInfo::TTech tech = (*it)->GetMolinfo().GetTech();
            if (tech == CMolInfo::eTech_htgs_0  ||
                tech == CMolInfo::eTech_htgs_1  ||
                tech == CMolInfo::eTech_htgs_2  ||
                tech == CMolInfo::eTech_htgs_3) {
                return true;
            }
        }
    }
    if (se.IsSet()) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            if (s_IsHtgInSep(**it)) {
                return true;
            }
        }
    }
    return false;
}


void CValidError_imp::ValidateSubAffil
(const CAffil::TStd& std,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if (!std.IsSetCountry() || NStr::IsBlank(std.GetCountry())) {
        PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                    "Submission citation affiliation has no country",
                    obj, ctx);
    } else if (NStr::EqualCase (std.GetCountry(), "USA")) {
        if (!std.IsSetSub() || NStr::IsBlank (std.GetSub())) {
            PostObjErr (eDiag_Warning, eErr_GENERIC_MissingPubInfo, 
                        "Submission citation affiliation has no state",
                        obj, ctx);
        }
    }
}


void CValidError_imp::ValidateCitSub
(const CCit_sub& cs,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    bool has_name  = false,
         has_affil = false;

    if ( cs.CanGetAuthors() ) {
        const CAuth_list& authors = cs.GetAuthors();
        has_name = HasName(authors);

        if ( authors.CanGetAffil() ) {
            const CAffil& affil = authors.GetAffil();

            switch ( affil.Which() ) {
            case CAffil::e_Str:
                {{
                    if ( !NStr::IsBlank(affil.GetStr()) ) {
                        has_affil = true;
                    }
                }}
                break;

            case CAffil::e_Std:
                {{
                    const CAffil::TStd& std = affil.GetStd();
#define HAS_VALUE(x) (std.CanGet##x()  &&  !NStr::IsBlank(std.Get##x()))
                    if ( HAS_VALUE(Affil)    ||  HAS_VALUE(Div)      ||
                         HAS_VALUE(City)     ||  HAS_VALUE(Sub)      ||
                         HAS_VALUE(Country)  ||  HAS_VALUE(Street)   ||
                         HAS_VALUE(Email)    ||  HAS_VALUE(Fax)      ||
                         HAS_VALUE(Phone)    ||  HAS_VALUE(Postal_code) ) {
                        has_affil = true;
                        ValidateSubAffil (std, obj, ctx);
                    }
                }}
#undef HAS_VALUE
                break;

            default:
                break;
            }
            
        }
    }

    if ( !has_name ) {
        PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Submission citation has no author names", obj, ctx);
    }
    if ( !has_affil ) {
        EDiagSev sev = 
            s_IsRefSeqInSep(GetTSE(), *m_Scope)  ||  s_IsHtgInSep(GetTSE()) ?
                eDiag_Warning : eDiag_Error;
        PostObjErr(sev, eErr_GENERIC_MissingPubInfo,
            "Submission citation has no affiliation", obj, ctx);
    }

    if (cs.IsSetDate()) {
        int rval = CheckDate (cs.GetDate());
        if (rval != eDateValid_valid) {
            PostBadDateError (eDiag_Error, "Submission citation date has error", rval, obj, ctx);
        }
    } else {
        PostObjErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Submission citation has no date", obj, ctx);
    }
}


static bool s_IsNoncuratedRefSeq (const CBioseq& seq) 
{
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
        if ((*id_it)->IsOther()) {
            if ((*id_it)->GetOther().IsSetAccession()) {
                string accession = (*id_it)->GetOther().GetAccession();
                if (NStr::StartsWith(accession, "NM_")
                    || NStr::StartsWith(accession, "NP_")
                    || NStr::StartsWith(accession, "NG_")
                    || NStr::StartsWith(accession, "NR_")) {
                    return false;
                } else {
                    return true;
                }
            }
        }
    }
    return false;
}


bool CValidError_imp::IsNoncuratedRefSeq(const CBioseq& seq, EDiagSev& sev)
{
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
        if ((*id_it)->IsOther()
            && (*id_it)->GetOther().IsSetAccession()) {
            const string& accession = (*id_it)->GetOther().GetAccession();
            if (NStr::StartsWith (accession, "NM_")
                || NStr::StartsWith(accession, "NP_")
                || NStr::StartsWith(accession, "NG_")
                || NStr::StartsWith(accession, "NR_")) {
                sev = eDiag_Warning;
                return false;
            }
            return true;
        }
    }

    return false;
}


void CValidError_imp::AddBioseqWithNoPub(const CBioseq& seq)
{
    EDiagSev sev = eDiag_Error;

    if (!m_NoPubs) {
        if (seq.IsAa()) {
            CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
            if (bsh) {
                bsh = GetNucBioseq (bsh);
                if (bsh) {
                    const CBioseq& nuc = *(bsh.GetCompleteBioseq());
                    if(!IsNoncuratedRefSeq (nuc, sev)
                        && !IsWGSIntermediate(nuc)) {
                        PostErr (sev, eErr_SEQ_DESCR_NoPubFound, "No publications refer to this Bioseq.", seq);
                    }
                    return;
                }
            }
        }
        if (!IsNoncuratedRefSeq (seq, sev)
            && !IsWGSIntermediate(seq)) {
            PostErr (sev, eErr_SEQ_DESCR_NoPubFound, "No publications refer to this Bioseq.", seq);
        }
    }
}


static bool s_IsGpipe (const CBioseq& seq) 
{
    bool is_gpipe = false;

    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
        if ((*id_it)->IsGpipe()) {
            is_gpipe = true;
            break;
        }
    }
    return is_gpipe;
}


static bool s_CuratedRefSeqLowerToWarning (const CBioseq& seq)
{
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
        if ((*id_it)->IsOther() && (*id_it)->GetOther().IsSetAccession()) {
            const string& str = (*id_it)->GetOther().GetAccession();
            if (NStr::StartsWith(str, "NM_")
                || NStr::StartsWith(str, "NP_")
                || NStr::StartsWith(str, "NG_")
                || NStr::StartsWith(str, "NR_")) {
                return true;
            }
        }
    }
    return false;
}

static bool s_IsWgs_or_TSA_Contig (const CBioseq& seq) 
{
    CSeq_inst::ERepr rp = seq.GetInst().GetRepr();
    if (rp == CSeq_inst::eRepr_virtual) return false;
    IF_EXISTS_CLOSEST_MOLINFO (mi_ref, seq, NULL) {
        const CMolInfo& molinf = (*mi_ref).GetMolinfo();
        if (molinf.GetTech() == NCBI_TECH(wgs)) return true;
        if (molinf.GetTech() == NCBI_TECH(tsa)) return true;
    }
    return false;
}

void CValidError_imp::ReportMissingPubs(const CSeq_entry& se, const CCit_sub* cs)
{
    if ( m_NoPubs ) {
        if ( !m_IsGPS  &&  !cs) {
            CBioseq_CI b_it(m_Scope->GetSeq_entryHandle(se));
            if (b_it)
            {
                CConstRef<CBioseq> bioseq = b_it->GetCompleteBioseq();
                if (   !s_IsNoncuratedRefSeq(*bioseq)  
                    && !s_IsGpipe(*bioseq)
                    && !s_IsWgs_or_TSA_Contig(*bioseq) ) {
                        EDiagSev sev = eDiag_Error;
                        if (s_CuratedRefSeqLowerToWarning(*bioseq)) {
                            sev = eDiag_Warning;
                        }
                        PostErr(sev, eErr_SEQ_DESCR_NoPubFound, 
                            "No publications anywhere on this entire record.", se);
                }
            }
        } 
    }
}


void CValidError_imp::FindCollidingSerialNumbers (const CSerialObject& obj)
{
    if (m_PubSerialNumbers.size() < 2) {
        return;
    }
    sort (m_PubSerialNumbers.begin(), m_PubSerialNumbers.end());

    vector<int>::iterator it1 = m_PubSerialNumbers.begin();
    vector<int>::iterator it2 = it1;
    ++it2;
    while (it2 != m_PubSerialNumbers.end()) {
        if (*it1 == *it2) {
            PostErr (eDiag_Warning, eErr_GENERIC_CollidingSerialNumbers,
              "Multiple publications have serial number " + NStr::IntToString(*it1),
              obj);
            while (it2 != m_PubSerialNumbers.end() && *it2 == *it1) {
                ++it2;
            }
            if (it2 != m_PubSerialNumbers.end()) {
                it1 = it2;
                ++it2;
            }
        } else {
            it1 = it2;
            ++it2;
        }
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
