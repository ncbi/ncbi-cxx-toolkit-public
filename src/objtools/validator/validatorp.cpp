/*  $Id:
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
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include "validatorp.hpp"
#include "utilities.hpp"

#include <serial/iterator.hpp>

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

#include <objects/util/feature.hpp>
#include <objects/util/sequence.hpp>

#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/scope.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/PubStatus.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// =============================================================================
//                            CValidError_imp Public
// =============================================================================

// Constructor
CValidError_imp::CValidError_imp
(CObjectManager&     objmgr,
 TErrs&              errs,
 unsigned int        options)
    : m_ObjMgr(&objmgr),
      m_Scope(0),
      m_Errors(&errs),
      m_NonASCII((options & CValidError::eVal_non_ascii) != 0),
      m_SuppressContext((options & CValidError::eVal_no_context) != 0),
      m_ValidateAlignments((options & CValidError::eVal_val_align) != 0),
      m_ValidateExons((options & CValidError::eVal_val_exons) != 0),
      m_SpliceErr((options & CValidError::eVal_splice_err) != 0),
      m_OvlPepErr((options & CValidError::eVal_ovl_pep_err) != 0),
      m_RequireTaxonID((options & CValidError::eVal_need_taxid) != 0),
      m_RequireISOJTA((options & CValidError::eVal_need_isojta) != 0),
      m_NoPubs(false),
      m_NoBioSource(false),
      m_IsGPS(false),
      m_IsGED(false),
      m_IsPDB(false),
      m_IsTPA(false),
      m_IsPatent(false),
      m_IsRefSeq(false),
      m_IsNC(false),
      m_IsNG(false),
      m_IsNM(false),
      m_IsNP(false),
      m_IsNR(false),
      m_IsNS(false),
      m_IsNT(false),
      m_IsNW(false),
      m_IsXR(false),
      m_IsGI(false)
{
    InitializeSourceQualTags();
}


// Destructor
CValidError_imp::~CValidError_imp()
{
}


// Error post methods
void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj)
{
    const CSeqdesc* desc = dynamic_cast < const CSeqdesc* > (&obj);
    if (desc != 0) {
        PostErr (sv, et, msg, *desc);
        return;
    }
    const CSeq_feat* feat = dynamic_cast < const CSeq_feat* > (&obj);
    if (feat != 0) {
        PostErr (sv, et, msg, *feat);
        return;
    }
    const CBioseq* seq = dynamic_cast < const CBioseq* > (&obj);
    if (seq != 0) {
        PostErr (sv, et, msg, *seq);
        return;
    }
    const CBioseq_set* set = dynamic_cast < const CBioseq_set* > (&obj);
    if (set != 0) {
        PostErr (sv, et, msg, *set);
        return;
    }
    const CSeq_annot* annot = dynamic_cast < const CSeq_annot* > (&obj);
    if (annot != 0) {
        PostErr (sv, et, msg, *annot);
        return;
    }
}


void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TDesc    ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel (&msg, CSeqdesc::eBoth);

    m_Errors->push_back(CRef<CValidErrItem>
        (new CValidErrItem(sv, et, msg, ds)));
}


void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TFeat    ft)
{
    // Add feature part of label
    string msg(message + " FEATURE: ");
    feature::GetLabel(ft, &msg, feature::eBoth, m_Scope.GetPointer());

    // Add feature location part of label
    string loc_label;
    if (m_SuppressContext) {
        CSeq_loc loc;
        SerialAssign(loc, ft.GetLocation());
        ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
        loc.GetLabel(&loc_label);
    } else {
        ft.GetLocation().GetLabel(&loc_label);
    }
    if (loc_label.size() > 800) {
        loc_label = loc_label.substr(0, 797) + "...";
    }
    if (!loc_label.empty()) {
        loc_label = string("[") + loc_label + "]";
        msg += loc_label;
    }

    // Append label for bioseq of feature location
    if (!m_SuppressContext) {
        try {
            const CSeq_id& id = GetId(ft.GetLocation(), m_Scope.GetPointer());
            CBioseq_Handle hnd = m_Scope.GetPointer()->GetBioseqHandle(id);
            CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
            msg += "[";
            bc->GetLabel(&msg, CBioseq::eBoth);
            msg += "]";
        } catch (...){};
    }

    // Append label for product of feature
    loc_label.erase();
    if (ft.IsSetProduct()) {
        if (m_SuppressContext) {
            CSeq_loc loc;
            SerialAssign(loc, ft.GetProduct());
            ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
            loc.GetLabel(&loc_label);
        } else {
            ft.GetProduct().GetLabel(&loc_label);
        }
        if (loc_label.size() > 800) {
            loc_label = loc_label.substr(0, 797) + "...";
        }
        if (!loc_label.empty()) {
            loc_label = string("[") + loc_label + "]";
            msg += loc_label;
        }
    }
    m_Errors->push_back(CRef<CValidErrItem>
        (new CValidErrItem(sv, et, msg, ft)));
}


void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TBioseq  sq)
{
    // Append bioseq label
    string msg(message + " BIOSEQ: ");
    if (m_SuppressContext) {
        sq.GetLabel(&msg, CBioseq::eContent, true);
    } else {
        sq.GetLabel(&msg, CBioseq::eBoth, false);
    }
    m_Errors->push_back(CRef<CValidErrItem>
        (new CValidErrItem(sv, et, msg, sq)));
}


void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TBioseq  sq,
 TDesc    ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel(&msg, CSeqdesc::eBoth);
    PostErr(sv, et, msg, sq);
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& message,
 TSet          set)
{
    // Append Bioseq_set label
    string msg(message + " BIOSEQ-SET: ");
    if (m_SuppressContext) {
        set.GetLabel(&msg, CBioseq_set::eContent);
    } else {
        set.GetLabel(&msg, CBioseq_set::eBoth);
    }
    m_Errors->push_back(CRef<CValidErrItem>
                        (new CValidErrItem(sv, et, msg, set)));
}


void CValidError_imp::PostErr
(EDiagSev        sv,
 EErrType        et,
 const string&   message,
 TSet            set,
 TDesc           ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel(&msg, CSeqdesc::eBoth);
    PostErr(sv, et, msg, set);
}


void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TAnnot    an)
{
    // Append Annotation label
    string msg(message + " ANNOTATION: ");

    // !!! need to decide on the message

    m_Errors->push_back(CRef<CValidErrItem>
        (new CValidErrItem(sv, et, msg, an)));
}


void CValidError_imp::Validate(const CSeq_entry& se, const CCit_sub* cs)
{
    // Check that CSeq_entry has data
    if (se.Which() == CSeq_entry::e_not_set) {
        ERR_POST(Warning << "Seq_entry not set");
        return;
    }

    // Get first CBioseq object pointer for PostErr below.
    CTypeConstIterator<CBioseq> seq(ConstBegin(se));
    if (!seq) {
        ERR_POST("No Bioseq anywhere on this Seq-entry");
        return;
    }
    
    Setup(se);
 
    // If m_NonASCII is true, then this flag was set by the caller
    // of validate to indicate that a non ascii character had been
    // read from a file being used to create a CSeq_entry, that the
    // error had been corrected, but that the error needs to be reported
    // by Validate. Note, Validate is not doing anything other than
    // reporting an error if m_NonASCII is true;
    if (m_NonASCII) {
        PostErr(eDiag_Critical, eErr_GENERIC_NonAsciiAsn,
                  "Non-ascii chars in input ASN.1 strings", *seq);
        // Only report the error once
        m_NonASCII = false;
    }

    // Iterate thru components of record and validate each

    CValidError_feat feat_validator(*this);
    for (CTypeConstIterator <CSeq_feat> fi (se); fi; ++fi) {
        feat_validator.ValidateSeqFeat(*fi);
    }

    CValidError_desc desc_validator(*this);
    for (CTypeConstIterator <CSeqdesc> di (se); di; ++di) {
        desc_validator.ValidateSeqDesc(*di);
    }

    CValidError_bioseq bioseq_validator(*this);
    for (CTypeConstIterator <CBioseq> bi (se); bi; ++bi) {
        bioseq_validator.ValidateSeqIds(*bi);
        bioseq_validator.ValidateInst(*bi);
        bioseq_validator.ValidateBioseqContext(*bi);
        bioseq_validator.ValidateHistory(*bi);
    }

    CValidError_bioseqset bioseqset_validator(*this);
    for (CTypeConstIterator <CBioseq_set> si (se); si; ++si) {
        bioseqset_validator.ValidateBioseqSet(*si);
    }

    CValidError_align align_validator(*this);
    for (CTypeConstIterator <CSeq_align> ai (se); ai; ++ai) {
        align_validator.ValidateSeqAlign(*ai);
    }

    CValidError_graph graph_validator(*this);
    for (CTypeConstIterator <CSeq_graph> gi (se); gi; ++gi) {
        graph_validator.ValidateSeqGraph(*gi);
    }

    CValidError_annot annot_validator(*this);
    for (CTypeConstIterator <CSeq_annot> ni (se); ni; ++ni) {
        annot_validator.ValidateSeqAnnot(*ni);
    }

    CValidError_descr descr_validator(*this);
    for (CTypeConstIterator <CSeq_descr> ei (se); ei; ++ei) {
        descr_validator.ValidateSeqDescr(*ei);
    }
}


void CValidError_imp::Validate(const CSeq_submit& ss)
{
    // Check that ss is type e_Entrys
    if ( ss.GetData().Which() != CSeq_submit::C_Data::e_Entrys ) {
        return;
    }

    // Get CCit_sub pointer
    const CCit_sub* cs = &ss.GetSub().GetCit();

    // Just loop thru CSeq_entrys
    iterate( list< CRef< CSeq_entry > >, i, ss.GetData().GetEntrys() ) {
        const CSeq_entry* se = i->GetPointer();
        Validate(*se, cs);
    }
}


void CValidError_imp::ValidatePubdesc
(const CPubdesc& pubdesc,
 const CSerialObject& obj)
{
    int uid = 0;

    iterate( CPub_equiv::Tdata, pub, pubdesc.GetPub().Get() ) {
        switch( (*pub)->Which() ) {
        case CPub::e_Gen:
            ValidatePubGen((*pub)->GetGen(), obj);
            break;

        case CPub::e_Muid:
            uid = (*pub)->GetMuid();
            break;

        case CPub::e_Pmid:
            uid = (*pub)->GetPmid().Get();
            break;
            
        case CPub::e_Article:
            ValidatePubArticle((*pub)->GetArticle(), obj);
            break;

        case CPub::e_Equiv:
            PostErr(eDiag_Warning, eErr_GENERIC_UnnecessaryPubEquiv,
                "Publication has unexpected internal Pub-equiv", obj);
            break;

        default:
            break;
        }
    }
}


void CValidError_imp::ValidatePubGen
(const CCit_gen& gen,
 const CSerialObject& obj)
{
    if ( gen.IsSetCit()  &&  !gen.GetCit().empty() ) {
        const string& cit = gen.GetCit();
        if ( (NStr::CompareNocase(cit, 0, 8,  "submitted") != 0)          &&
             (NStr::CompareNocase(cit, 0, 11, "unpublished") != 0)        &&
             (NStr::CompareNocase(cit, 0, 18, "Online Publication") == 0) &&
             (NStr::CompareNocase(cit, 0, 26, "Published Only in DataBase") != 0) ) {
            PostErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
                "Unpublished citation text invalid", obj);
        }
    }
}


void CValidError_imp::ValidatePubArticle
(const CCit_art& art,
 const CSerialObject& obj)
{
    if ( !art.IsSetTitle()  ||  !HasTitle(art.GetTitle()) ) { 
        PostErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Publication has no title", obj);
    }
    
    if ( art.IsSetAuthors() ) {
        const CAuth_list::C_Names& names = art.GetAuthors().GetNames();

        bool has_name = false;
        if ( names.IsStd() ) {
            has_name = HasName(names.GetStd());
        } 
        if ( names.IsMl() ) {
            has_name = !IsBlankStringList(names.GetMl());
        }
        if ( names.IsStr() ) {
            has_name = !IsBlankStringList(names.GetStr());
        }

        if ( !has_name ) {
            PostErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
                "Publication has no author names", obj);
        }
    }
    
    
    if ( art.GetFrom().IsJournal() ) {
        const CCit_jour& jour = art.GetFrom().GetJournal();
        
        bool has_iso_jta = HasIsoJTA(jour.GetTitle());
        bool in_press = false;

        if ( !HasTitle(jour.GetTitle()) ) {
            PostErr(eDiag_Error, eErr_GENERIC_MissingPubInfo,
            "Journal title missing", obj);
        }

        const CImprint& imp = jour.GetImp();
        if ( imp.IsSetPrepub() ) {
            if ( imp.GetPrepub() == CImprint::ePrepub_in_press ) {
                in_press = true;
            }

            if ( !imp.IsSetPrepub()   &&  
                 imp.IsSetPubstatus() && 
                 imp.GetPubstatus() != ePubStatus_aheadofprint ) {
                bool no_vol = imp.IsSetVolume()  &&  
                    !IsBlankString(imp.GetVolume());
                bool no_pages = imp.IsSetPages()  &&
                    !IsBlankString(imp.GetPages());

                EDiagSev sev = IsNC() ? eDiag_Warning : eDiag_Error;

                if ( no_vol  &&  no_pages ) {
                    PostErr(sev, eErr_GENERIC_MissingPubInfo, 
                        "Journal volume and pages missing", obj);
                } else if ( no_vol ) {
                    PostErr(sev, eErr_GENERIC_MissingPubInfo,
                        "Journal volume missing", obj);
                } else if ( no_pages ) {
                    PostErr(sev, eErr_GENERIC_MissingPubInfo,
                        "Journal pages missing", obj);
                }
                
                if ( !no_pages ) {
                    string pages = imp.GetPages();
                    size_t pos = pages.find('-');
                    if ( pos != string::npos ) {
                        try {
                            int start = NStr::StringToInt(pages.substr(0, pos));
                            
                            try {
                                int stop = NStr::StringToInt(
                                    pages.substr(pos + 1, pages.length()));
                                
                                if ( start == 0  ||  stop == 0 ) {
                                    PostErr(sev, eErr_GENERIC_BadPageNumbering,
                                        "Page numbering has zero value", obj);
                                } else if ( start < 0  ||  stop < 0 ) {
                                    PostErr(sev, eErr_GENERIC_BadPageNumbering,
                                        "Page numbering has negative value", obj);
                                } else if ( start > stop ) {
                                    PostErr(sev, eErr_GENERIC_BadPageNumbering,
                                        "Page numbering out of order", obj);
                                } else if ( stop - start > 50 ) {
                                    PostErr(sev, eErr_GENERIC_BadPageNumbering,
                                        "Page numbering greater than 50", obj);
                                }
                            } catch ( CStringException& e ) {
                                PostErr(sev, eErr_GENERIC_BadPageNumbering,
                                    "Page numbering stop looks strange", obj);
                            }
                        } catch ( CStringException& e ) {
                            PostErr(sev, eErr_GENERIC_BadPageNumbering,
                                "Page numbering start looks strange", obj);
                        }
                    }
                }
            }
        }
        if ( !has_iso_jta  &&  (in_press  ||  IsRequireISOJTA()) ) {
            PostErr(eDiag_Warning, eErr_GENERIC_MissingPubInfo,
                "ISO journal title abbreviation missing", obj);
        }
    }
}


bool CValidError_imp::HasTitle(const CTitle& title)
{
    iterate (CTitle::Tdata, item, title.Get() ) {
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
        if ( IsBlankString(*str) ) {
            return false;
        }
    }
    return true;
}


bool CValidError_imp::HasIsoJTA(const CTitle& title)
{
    iterate (CTitle::Tdata, item, title.Get() ) {
        if ( (*item)->IsIso_jta() ) {
            return true;
        }
    }
    return false;
}


bool CValidError_imp::HasName(const list< CRef< CAuthor > >& authors)
{
    iterate ( list< CRef< CAuthor > >, auth, authors ) {
        const CPerson_id& pid = (*auth)->GetName();
        if ( pid.IsName() ) {
            if ( !IsBlankString(pid.GetName().GetLast()) ) {
                return true;
            }
        }
    }
    return false;
}


static const string s_LegalRefSeqDbXrefs[] = {
  "GenBank",
  "EMBL",
  "DDBJ",
};


static const string s_LegalDbXrefs[] = {
    "PIDe", "PIDd", "PIDg", "PID",
    "AceView/WormGenes",
    "ATCC",
    "ATCC(in host)",
    "ATCC(dna)",
    "BDGP_EST",
    "BDGP_INS",
    "CDD",
    "CK",
    "COG",
    "dbEST",
    "dbSNP",
    "dbSTS",
    "ENSEMBL",
    "ESTLIB",
    "FANTOM_DB",
    "FLYBASE",
    "GABI",
    "GDB",
    "GeneID",
    "GI",
    "GO",
    "GOA",
    "IFO",
    "IMGT/LIGM",
    "IMGT/HLA",
    "InterimID",
    "ISFinder",
    "JCM",
    "LocusID",
    "MaizeDB",
    "MGD",
    "MGI",
    "MIM",
    "NextDB",
    "niaEST",
    "PIR",
    "PSEUDO",
    "RATMAP",
    "RiceGenes",
    "REMTREMBL",
    "RGD",
    "RZPD",
    "SGD",
    "SoyBase",
    "SPTREMBL",
    "SWISS-PROT",
    "taxon",
    "UniGene",
    "UniSTS",
    "WorfDB",
    "WormBase",
};


void CValidError_imp::ValidateDbxref
(const CDbtag&        xref,
 const CSerialObject& obj)
{
    const string& str = xref.GetDb();
    for ( size_t i = 0;  i < sizeof(s_LegalDbXrefs) / sizeof(string); i++ ) {
        if (NStr::Compare(str, s_LegalDbXrefs[i]) == 0) {
            return;
        }
    }
    if ( m_IsRefSeq ) {
        for ( size_t i = 0; 
              i < sizeof(s_LegalRefSeqDbXrefs) / sizeof (string);
              i++) {
            if ( NStr::Compare(str, s_LegalRefSeqDbXrefs [i]) == 0 ) {
                return;
            }
        }
    }
    PostErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
        "Illegal db_xref type " + str, obj);
}


void CValidError_imp::ValidateDbxref (
    const list< CRef< CDbtag > > &xref_list,
    const CSerialObject& obj
)
{
    iterate( list< CRef< CDbtag > >, xref, xref_list) {
        ValidateDbxref(**xref, obj);
    }
}


void CValidError_imp::ValidateBioSource
(const CBioSource&    bsrc,
 const CSerialObject& obj)
{
	const COrg_ref& orgref = bsrc.GetOrg();
  
	// Organism must have a name.
	if ( orgref.GetTaxname().empty() && orgref.GetCommon().empty() ) {
		PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
			     "No organism name has been applied to this Bioseq.", obj);
	}

	// validate legal locations.
	if ( bsrc.GetGenome() == CBioSource_Base::eGenome_transposon  ||
		 bsrc.GetGenome() == CBioSource_Base::eGenome_insertion_seq ) {
		PostErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceLocation,
			     "Transposon and insertion sequence are no longer legal locations.", obj);
	}

	int chrom_count = 0;
	bool chrom_conflict = false;
    const CSubSource *chromosome = 0;
	string countryname;
	iterate( CBioSource::TSubtype, ssit, bsrc.GetSubtype() ) {
		switch ( (**ssit).GetSubtype() ) {

		case CSubSource::eSubtype_country:
			countryname = (**ssit).GetName();
            if ( !CCountries::IsValid(countryname) ) {
				if ( countryname.empty() ) {
					countryname = "?";
				}
				PostErr(eDiag_Warning, eErr_SEQ_DESCR_BadCountryCode,
						 "Bad country name " + countryname, obj);
			}
			break;

		case CSubSource::eSubtype_chromosome:
			++chrom_count;
			if ( chromosome != 0 ) {
				if ( NStr::CompareNocase((**ssit).GetName(), chromosome->GetName()) != 0) {
					chrom_conflict = true;
				}          
			} else {
				chromosome = ssit->GetPointer();
			}
			break;

		case CSubSource::eSubtype_transposon_name:
		case CSubSource::eSubtype_insertion_seq_name:
			PostErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceQual,
					 "Transposon name and insertion sequence name are no longer legal qualifiers.", obj);
		break;

		case 0:
			PostErr(eDiag_Warning, eErr_SEQ_DESCR_BadSubSource,
               "Unknown subsource subtype 0.", obj);
			break;
    
		case CSubSource::eSubtype_other:
			ValidateSourceQualTags((**ssit).GetName(), obj);
			break;
		}
	}
	if ( chrom_count > 1 ) {
		string msg = chrom_conflict ? "Multiple conflicting chromosome qualifiers" :
									  "Multiple identical chromosome qualifiers";
		PostErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleChromosomes, msg, obj);
	}

    if ( !orgref.IsSetOrgname()  ||
         orgref.GetOrgname().GetLineage().empty() ) {
		PostErr(eDiag_Error, eErr_SEQ_DESCR_MissingLineage, 
			     "No lineage for this BioSource.", obj);
	} else {
        const string& lineage = orgref.GetOrgname().GetLineage();
		if ( bsrc.GetGenome() == CBioSource_Base::eGenome_kinetoplast ) {
			if ( lineage.find("Kinetoplastida") == string::npos ) {
				PostErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
						 "Only Kinetoplastida have kinetoplasts", obj);
			}
		} 
		if ( bsrc.GetGenome() == CBioSource_Base::eGenome_nucleomorph ) {
			if ( lineage.find("Chlorarchniophyta") == string::npos  &&
				lineage.find("Cryptophyta") == string::npos) {
				PostErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
						 "Only Chlorarchniophyta and Cryptophyta have nucleomorphs", obj);
			}
		}
	}
    if ( !orgref.IsSetOrgname() ) {
        return;
    }
    const COrgName& orgname = orgref.GetOrgname();

	iterate ( COrgName::TMod, omit, orgname.GetMod() ) {
		int subtype = (**omit).GetSubtype();

		if ( (subtype == 0) || (subtype == 1) ) {
			PostErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
				     "Unknown orgmod subtype " + subtype, obj);
		}
		if ( subtype == COrgMod::eSubtype_variety ) {
			if ( NStr::CompareNocase( orgname.GetDiv(), "PLN" ) != 0 ) {
				PostErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
						 "Orgmod variety should only be in plants or fungi", obj);
			}
		}
		if ( subtype == COrgMod::eSubtype_other ) {
			ValidateSourceQualTags( (**omit).GetSubname(), obj);
		}
	}

    if ( orgref.IsSetDb() ) {
        ValidateDbxref(orgref.GetDb(), obj);
    }

    if ( IsRequireTaxonID() ) {
        bool found = false;
        if ( orgref.IsSetDb() ) {
            iterate( COrg_ref::TDb, dbt, orgref.GetDb() ) {
                if ( NStr::CompareNocase((*dbt)->GetDb(), "taxon") == 0 ) {
                    found = true;
                    break;
                }
            }
        }
        if ( !found ) {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_NoTaxonID,
                "BioSource is missing taxon ID", obj);
        }
    }
}


void CValidError_imp::ValidateSeqLoc
(const CSeq_loc& loc,
 const CBioseq&  seq,
 const string&   prefix)
{
    bool circular = false;
    if ( seq.GetInst().IsSetTopology() ) {
        circular =
            seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular;
    }
    
    
    bool ordered = true, adjacent = false, chk = true,
        unmarked_strand = false, mixed_strand = false;
    const CSeq_id* id_cur = 0, *id_prv = 0;
    const CSeq_interval *int_cur = 0, *int_prv = 0;
    ENa_strand strand_cur = eNa_strand_unknown,
        strand_prv = eNa_strand_unknown;

    CTypeConstIterator<CSeq_loc> lit = ConstBegin(loc);
    for (; lit; ++lit) {
        try {
            switch (lit->Which()) {
            case CSeq_loc::e_Int:
                int_cur = &lit->GetInt();
                strand_cur = int_cur->IsSetStrand() ?
                    int_cur->GetStrand() : eNa_strand_unknown;
                id_cur = &int_cur->GetId();
                chk = IsValid(*int_cur, m_Scope);
                if (chk  &&  int_prv  && ordered  &&
                    !circular  && id_prv) {
                    if (IsSameBioseq(*id_prv, *id_cur, m_Scope)) {
                        if (strand_cur == eNa_strand_minus) {
                            if (int_prv->GetTo() < int_cur->GetTo()) {
                                ordered = false;
                            }
                            if (int_cur->GetTo() + 1 == int_prv->GetFrom()) {
                                adjacent = true;
                            }
                        } else {
                            if (int_prv->GetTo() > int_cur->GetTo()) {
                                ordered = false;
                            }
                            if (int_prv->GetTo() + 1 == int_cur->GetFrom()) {
                                adjacent = true;
                            }
                        }
                    }
                }
                if (int_prv) {
                    if (IsSameBioseq(int_prv->GetId(), int_cur->GetId(), m_Scope)){
                        if (strand_prv == strand_cur  &&
                            int_prv->GetFrom() == int_cur->GetFrom()  &&
                            int_prv->GetTo() == int_cur->GetTo()) {
                            PostErr(eDiag_Error,
                                eErr_SEQ_FEAT_DuplicateInterval,
                                "Duplicate exons in location", seq);
                        }
                    }
                }
                int_prv = int_cur;
                break;
            case CSeq_loc::e_Pnt:
                strand_cur = lit->GetPnt().IsSetStrand() ?
                    lit->GetPnt().GetStrand() : eNa_strand_unknown;
                id_cur = &lit->GetPnt().GetId();
                chk = IsValid(lit->GetPnt(), m_Scope);
                int_prv = 0;
                break;
            case CSeq_loc::e_Packed_pnt:
                strand_cur = lit->GetPacked_pnt().IsSetStrand() ?
                    lit->GetPacked_pnt().GetStrand() : eNa_strand_unknown;
                id_cur = &lit->GetPacked_pnt().GetId();
                chk = IsValid(lit->GetPacked_pnt(), m_Scope);
                int_prv = 0;
                break;
            case CSeq_loc::e_Null:
                break;
            default:
                strand_cur = eNa_strand_other;
                id_cur = 0;
                int_prv = 0;
                break;
            }
            if (!chk) {
                string lbl;
                lit->GetLabel(&lbl);
                PostErr(eDiag_Critical, eErr_SEQ_FEAT_Range,
                    prefix + ": Seq-loc " + lbl + " out of range", seq);
            }
            
            if (lit->Which() != CSeq_loc::e_Null) {
                if (strand_prv != eNa_strand_other  &&
                    strand_cur != eNa_strand_other) {
                    if (id_cur  &&  id_prv  &&
                        IsSameBioseq(*id_cur, *id_prv, m_Scope)) {
                        if (strand_prv != strand_cur) {
                            if ((strand_prv == eNa_strand_plus  &&
                                strand_cur == eNa_strand_unknown)  ||
                                (strand_prv == eNa_strand_unknown  &&
                                strand_cur == eNa_strand_plus)) {
                                unmarked_strand = true;
                            } else {
                                mixed_strand = true;
                            }
                        }
                    }
                }
                strand_prv = strand_cur;
                id_prv = id_cur;
            }
        } catch( runtime_error& rt ) {
            string label;
            lit->GetLabel(&label);
            PostErr(eDiag_Error, eErr_SEQ_FEAT_Range,  // !!! need to chenge error type
                "Exception caught while validating location " +
                label + ". exception: " + rt.what(), seq);
                
            strand_cur = eNa_strand_other;
            id_cur = 0;
            int_prv = 0;
        }
    }

    string loc_lbl;
    if (adjacent) {
        loc.GetLabel(&loc_lbl);
        PostErr(eDiag_Error, eErr_SEQ_FEAT_AbuttingIntervals,
            prefix + ": Adjacent intervals in SeqLoc [" +
            loc_lbl + "]", seq);
    }
    if (mixed_strand  ||  unmarked_strand  ||  !ordered) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        if (mixed_strand) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
                prefix + ": Mixed strands in SeqLoc [" +
                loc_lbl + "]", seq);
        } else if (unmarked_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                prefix + ": Mixed plus and unknown strands in SeqLoc "
                " [" + loc_lbl + "]", seq);
        }
        if (!ordered) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
                prefix + ": Intervals out of order in SeqLoc [" +
                loc_lbl + "]", seq);
        }
        return;
    }

    if (seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
        return;
    }

    // Check for intervals out of order on segmented Bioseq
    if ( BadSeqLocSortOrder(seq, loc, m_Scope) ) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
            prefix + "Intervals out of order in SeqLoc [" +
            loc_lbl + "]", seq);
    }

    // Check for mixed strand on segmented Bioseq
    if ( IsMixedStrands(seq, loc) ) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        PostErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
            prefix + ": Mixed strands in SeqLoc [" +
            loc_lbl + "]", seq);
    }
}


CValidError_imp::TFeatAnnotMap CValidError_imp::GetFeatAnnotMap(void)
{
    return m_FeatAnnotMap;
}


void CValidError_imp::AddBioseqWithNoPub(const CBioseq& seq)
{
    m_BioseqWithNoPubs.push_back(CConstRef<CBioseq>(&seq));
}


void CValidError_imp::AddBioseqWithNoBiosource(const CBioseq& seq)
{
    m_BioseqWithNoSource.push_back(CConstRef<CBioseq>(&seq));
}


void CValidError_imp::AddProtWithoutFullRef(const CBioseq& seq)
{
    m_ProtWithNoFullRef.push_back(CConstRef<CBioseq>(&seq));
}


void CValidError_imp::ReportMissingPubs(const CBioseq& seq, const CCit_sub* cs)
{
    if (m_NoPubs  &&  !m_IsGPS  &&  !m_IsRefSeq  &&  !cs) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoPubFound, 
            "No publications anywhere on this entire record", seq);
    } else {
        size_t num_no_pubs = m_BioseqWithNoPubs.size();
        if ( num_no_pubs > 10 ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_NoPubFound, 
                NStr::IntToString(num_no_pubs) + 
                " Bioseqs without publication in this record  (first reported)",
                *(m_BioseqWithNoPubs[0]));
        } else {
            string msg;
            for ( size_t i =0; i < num_no_pubs; ++i ) {
                msg = NStr::IntToString(i + 1) + " of " + 
                    NStr::IntToString(num_no_pubs) + 
                    " Bioseqs without publication";
                PostErr(eDiag_Error, eErr_SEQ_DESCR_NoPubFound, msg, 
                    *(m_BioseqWithNoPubs[0]));
            }
        }
    }        
}


void CValidError_imp::ReportMissingBiosource(const CBioseq& seq)
{
    if(m_NoBioSource  &&  !m_IsPatent  &&  !m_IsPDB) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism name anywhere on this entire record", seq);
    } else {
        size_t num_no_source = m_BioseqWithNoSource.size();
        if ( num_no_source > 10 ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound, 
                NStr::IntToString(num_no_source) + 
                " Bioseqs without source in this record (first reported)",
                *(m_BioseqWithNoSource[0]));
        } else {
            string msg;
            for ( size_t i =0; i < num_no_source; ++i ) {
                msg = NStr::IntToString(i + 1) + " of " + 
                    NStr::IntToString(num_no_source) + 
                    " Bioseqs without publication";
                PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound, msg, 
                    *(m_BioseqWithNoSource[0]));
            }
        }
    }        
}


void CValidError_imp::ReportProtWithoutFullRef(void)
{
    size_t num = m_ProtWithNoFullRef.size();
    
    if ( num > 10 ) {
        PostErr(eDiag_Error, eErr_SEQ_FEAT_NoProtRefFound, 
            NStr::IntToString(num) + " Bioseqs with no full length " 
            "Prot-ref feature (first reported)",
            *(m_ProtWithNoFullRef[0]));
    } else {
        string msg;
        for ( size_t i = 0; i < num; ++i ) {
            msg = NStr::IntToString(i + 1) + " of " + 
                NStr::IntToString(num) + 
                " Bioseqs without full length Prot-ref feature";
            PostErr(eDiag_Error, eErr_SEQ_FEAT_NoProtRefFound, msg, 
                *(m_ProtWithNoFullRef[0]));
        }
    }
}   


bool CValidError_imp::IsFarLocation(const CSeq_loc& loc) const 
{
    // !!! requires a binary search implementation
    for ( CSeq_loc_CI citer(loc); citer; ++citer ) {
        bool found = false;
        const CSeq_id& id = citer.GetSeq_id();
        iterate( set< CConstRef<CSeq_id> >, i,  m_InitialSeqIds ) {
            if ( (*i)->Match(id) ) {
                found = true;
                break;
            }
        }
        if ( !found ) {
            return true;
        }
    }
    return false;
}


const CSeq_feat* CValidError_imp::GetCDSGivenProduct(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( bsh ) {
        CFeat_CI fi(bsh, 
                    0, 0,
                    CSeqFeatData::e_Cdregion,
                    CAnnot_CI::eOverlap_Intervals,
                    CFeat_CI::eResolve_TSE,
                    CFeat_CI::e_Product);
        if ( fi ) {
            // return the first one (should be the one packaged on the
            // nuc-prot set).
            return &(*fi);
        }
    }

    return 0;
}


const CSeq_entry* CValidError_imp::GetAncestor
(const CBioseq& seq,
 CBioseq_set::EClass clss)
{
    const CSeq_entry* parent = 0;
    for ( parent = seq.GetParentEntry(); 
          parent != 0;
          parent = parent->GetParentEntry() ) {
        if ( parent->IsSet() ) {
            const CBioseq_set& set = parent->GetSet();
            if ( set.IsSetClass()  &&  set.GetClass() == clss ) {
                break;
            }
        }
    }
    return parent;
}


// =============================================================================
//                                  Private
// =============================================================================


bool CValidError_imp::IsMixedStrands(const CBioseq& seq, const CSeq_loc& loc)
{
    if ( SeqLocCheck(loc, m_Scope) == eSeqLocCheck_warning ) {
        return false;
    }

    CSeq_loc_CI curr(loc);
    if ( !curr ) {
        return false;
    }
    CSeq_loc_CI prev = curr;
    ++curr;
    

    while ( curr ) {
        ENa_strand curr_strand = curr.GetStrand();
        ENa_strand prev_strand = prev.GetStrand();

        if ( (prev_strand == eNa_strand_minus  &&  
              curr_strand != eNa_strand_minus)   ||
             (prev_strand != eNa_strand_minus  &&  
              curr_strand == eNa_strand_minus) ) {
            return true;
        }

        prev = curr;
        ++curr;
    }

    return true;
}


void CValidError_imp::Setup(const CSeq_entry& se) 
{
    // Set scope to CSeq_entry
    SetScope(se);

    // If no Pubs/BioSource in CSeq_entry, post only one error
    m_NoPubs = !AnyObj<CPub, CSeq_entry>(se);
    m_NoBioSource = !AnyObj<CBioSource, CSeq_entry>(se);

    // Look for genomic product set
    for (CTypeConstIterator <CBioseq_set> si (se); si; ++si) {
        if (si->IsSetClass ()) {
            if (si->GetClass () == CBioseq_set::eClass_gen_prod_set) {
                m_IsGPS = true;
            }
        }
    }

    // Examine all Seq-ids on Bioseqs
    for (CTypeConstIterator <CBioseq> bi (se); bi; ++bi) {
        iterate (CBioseq::TId, id, bi->GetId()) {
            CSeq_id::E_Choice typ = (**id).Which();
            switch (typ) {
                case CSeq_id::e_not_set:
                    break;
                case CSeq_id::e_Local:
                    break;
                case CSeq_id::e_Gibbsq:
                    break;
                case CSeq_id::e_Gibbmt:
                    break;
                case CSeq_id::e_Giim:
                    break;
                case CSeq_id::e_Genbank:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Embl:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Pir:
                    break;
                case CSeq_id::e_Swissprot:
                    break;
                case CSeq_id::e_Patent:
                    m_IsPatent = true;
                    break;
                case CSeq_id::e_Other:
                    m_IsRefSeq = true;
                    // and do RefSeq subclasses up front as well
                    if ((**id).GetOther().IsSetAccession()) {
                        string acc = (**id).GetOther().GetAccession().substr(0, 3);
                        if (acc == "NC_") {
                            m_IsNC = true;
                        } else if (acc == "NG_") {
                            m_IsNG = true;
                        } else if (acc == "NM_") {
                            m_IsNM = true;
                        } else if (acc == "NP_") {
                            m_IsNP = true;
                        } else if (acc == "NR_") {
                            m_IsNR = true;
                        } else if (acc == "NS_") {
                            m_IsNS = true;
                        } else if (acc == "NT_") {
                            m_IsNT = true;
                        } else if (acc == "NW_") {
                            m_IsNW = true;
                        } else if (acc == "XR_") {
                            m_IsXR = true;
                        }
                    }
                    break;
                case CSeq_id::e_General:
                    if (!NStr::CompareCase((**id).GetGeneral().GetDb(), "BankIt")) {
                        m_IsTPA = true;
                    }
                    break;
                case CSeq_id::e_Gi:
                    m_IsGI = true;
                    break;
                case CSeq_id::e_Ddbj:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Prf:
                    break;
                case CSeq_id::e_Pdb:
                    m_IsPDB = true;
                    break;
                case CSeq_id::e_Tpg:
                    m_IsTPA = true;
                    break;
                case CSeq_id::e_Tpe:
                    m_IsTPA = true;
                    break;
                case CSeq_id::e_Tpd:
                    m_IsTPA = true;
                    break;
                default:
                    break;
            }
            // store the seq_id in the initial seq_entry
            m_InitialSeqIds.insert(*id);
        }
    }

    // Map features to their enclosing Seq_annot
    for ( CTypeConstIterator<CSeq_annot> ai(se); ai; ++ai ) {
        if ( ai->GetData().IsFtable() ) {
            iterate( CSeq_annot::C_Data::TFtable, fi, ai->GetData().GetFtable() ) {
                m_FeatAnnotMap[&(**fi)] = &(*ai);
            }
        }
    }
}


void CValidError_imp::SetScope(const CSeq_entry& se)
{
    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddTopLevelSeqEntry(*const_cast<CSeq_entry*>(&se));
    m_Scope->AddDefaults();
}


const string CValidError_imp::sm_SourceQualPrefixes[] = {
  "acronym:",
  "anamorph:",
  "authority:",
  "biotype:",
  "biovar:",
  "breed:",
  "cell_line:",
  "cell_type:",
  "chemovar:",
  "chromosome:",
  "clone:",
  "clone_lib:",
  "common:",
  "country:",
  "cultivar:",
  "dev_stage:",
  "dosage:",
  "ecotype:",
  "endogenous_virus_name:",
  "environmental_sample:",
  "forma:",
  "forma_specialis:",
  "frequency:",
  "genotype:",
  "germline:",
  "group:",
  "haplotype:",
  "insertion_seq_name:",
  "isolate:",
  "isolation_source:",
  "lab_host:",
  "map:",
  "nat_host:",
  "pathovar:",
  "plasmid_name:",
  "plastid_name:",
  "pop_variant:",
  "rearranged:",
  "segment:",
  "serogroup:",
  "serotype:",
  "serovar:",
  "sex:",
  "specimen_voucher:",
  "strain:",
  "subclone:",
  "subgroup:",
  "substrain:",
  "subtype:",
  "sub_species:",
  "synonym:",
  "taxon:",
  "teleomorph:",
  "tissue_lib:",
  "tissue_type:",
  "transgenic:",
  "transposon_name:",
  "type:",
  "variety:",
};


void CValidError_imp::InitializeSourceQualTags() 
{
    size_t size = sizeof(sm_SourceQualPrefixes) / sizeof(string);

    for (size_t i = 0; i < size; ++i ) {
        m_SourceQualTags.AddWord(sm_SourceQualPrefixes[i]);
    }

    m_SourceQualTags.Prime();
}


void CValidError_imp::ValidateSourceQualTags
(const string& str,
 const CSerialObject& obj)
{
    if ( str.empty() ) return;

    size_t str_len = str.length();

    int state = m_SourceQualTags.GetInitialState();
    for ( size_t i = 0; i < str_len; ++i ) {
        state = m_SourceQualTags.GetNextState(state, str[i]);
        if ( m_SourceQualTags.IsMatchFound(state) ) {
            string match = m_SourceQualTags.GetMatches(state)[0];
            if ( match.empty() ) {
                match = "?";
            }

            bool okay = true;
            if ( i - str_len >= 0 ) {
                char ch = str[i - str_len];
                if ( !isspace(ch) || ch != ';' ) {
                    okay = false;
                }
            }
            if ( okay ) {
                PostErr(eDiag_Warning, eErr_SEQ_DESCR_StructuredSourceNote,
                    "Source note has structured tag " + match, obj);
            }
        }
    }
}


// =============================================================================
//                         CValidError_base Implementation
// =============================================================================


CValidError_base::CValidError_base(CValidError_imp& imp) :
    m_Imp(imp), m_Scope(imp.GetScope())
{
}


CValidError_base::~CValidError_base()
{
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSerialObject& obj) 
{
    m_Imp.PostErr(sv, et, msg, obj);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TDesc ds)
{
    m_Imp.PostErr(sv, et, msg, ds);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TFeat ft)
{
    m_Imp.PostErr(sv, et, msg, ft);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TBioseq sq)
{
    m_Imp.PostErr(sv, et, msg, sq);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TBioseq sq,
 TDesc ds)
{
    m_Imp.PostErr(sv, et, msg, sq, ds);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg, 
 TSet set)
{
    m_Imp.PostErr(sv, et, msg, set);
}


void CValidError_base::PostErr
(EDiagSev sv, 
 EErrType et, 
 const string& msg, 
 TSet set,
 TDesc ds)
{
    m_Imp.PostErr(sv, et, msg, set, ds);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TAnnot annot)
{
    m_Imp.PostErr(sv, et, msg, annot);
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2003/01/24 20:38:51  shomrat
* Added call to ValidateHistory for each bioseq
*
* Revision 1.7  2003/01/24 19:14:47  ucko
* Catch exceptions by reference rather than value; fixes a build error
* with WorkShop in release mode.
*
* Revision 1.6  2003/01/21 20:19:07  shomrat
* Implemented ValidatePubDesc
*
* Revision 1.5  2003/01/08 18:35:39  shomrat
* Added mapping features to their enclosing annotation
*
* Revision 1.4  2003/01/02 22:01:20  shomrat
* Added GetCDSGivenProduct and GetAncestor
*
* Revision 1.3  2002/12/26 16:35:11  shomrat
* Typo
*
* Revision 1.2  2002/12/24 16:51:54  shomrat
* Changes to include directives
*
* Revision 1.1  2002/12/23 20:14:37  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
