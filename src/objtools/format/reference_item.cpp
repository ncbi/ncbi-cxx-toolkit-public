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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   flat-file generator -- bibliographic references
*
* ===========================================================================
*/
#include <corelib/ncbistd.hpp>

#include <serial/iterator.hpp>
#include <util/static_set.hpp>
#include <objects/biblio/biblio__.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objmgr/util/sequence.hpp>

#include <algorithm>

#include <objtools/format/text_ostream.hpp>
#include <objtools/format/formatter.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);

/////////////////////////////////////////////////////////////////////////////
//
// LessEqual - predicate class for sorting references

class LessEqual
{
public:
    LessEqual(bool serial_first, bool is_refseq);
    bool operator()(const CRef<CReferenceItem>& ref1, const CRef<CReferenceItem>& ref2);
private:
    bool m_SerialFirst;
    bool m_IsRefSeq;
};

/////////////////////////////////////////////////////////////////////////////


void CReferenceItem::FormatAffil(const CAffil& affil, string& result)
{
    if (affil.IsStr()) {
        result = affil.GetStr();
    } else {
        result.erase();
        const CAffil::C_Std& std = affil.GetStd();
        if (std.IsSetDiv()) {
            result = std.GetDiv();
        }
        if (std.IsSetAffil()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetAffil();
        }
        if (std.IsSetStreet()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetStreet();
        }
        if (std.IsSetCity()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetCity();
        }
        if (std.IsSetSub()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetSub();
        }
        if (std.IsSetPostal_code()) {
            if (!result.empty()) {
                result += " ";
            }
            result += std.GetPostal_code();
        }
        if (std.IsSetCountry()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetCountry();
        }
    }
}


static void s_FixPages(string& pages)
{
    // Restore redundant leading digits of the second number if needed
    SIZE_TYPE digits1 = pages.find_first_not_of("0123456789");
    if ( digits1 != NPOS) {
        SIZE_TYPE hyphen = pages.find('-', digits1);
        if ( hyphen != NPOS ) {
            SIZE_TYPE digits2 = pages.find_first_not_of("0123456789",
                hyphen + 1);
            digits2 -= hyphen + 1;
            if ( digits2 < digits1 ) {
                // lengths of the tail portions
                SIZE_TYPE len1 = hyphen + digits2 - digits1;
                SIZE_TYPE len2 = pages.size() - hyphen - 1;
                int x = NStr::strncasecmp(&pages[digits1 - digits2],
                    &pages[hyphen + 1],
                    min(len1, len2));
                if ( x > 0  ||  (x == 0  &&  len1 >= len2) ) {
                    // complain?
                } else {
                    pages.insert(hyphen + 1, pages, 0,
                        digits1 - digits2);
                }
            }
        }
    }
}


CReferenceItem::CReferenceItem(const CSeqdesc& desc, CBioseqContext& ctx) :
    CFlatItem(&ctx), m_PMID(0), m_MUID(0), m_Category(eUnknown), m_Serial(0),
    m_JustUids(false), m_Prepub(CImprint::ePrepub_other)
{
    _ASSERT(desc.IsPub());
    
    x_SetObject(desc.GetPub());
    m_Pubdesc.Reset(&(desc.GetPub()));

    if ( ctx.GetMapper() != 0 ) {
        m_Loc.Reset(ctx.GetMapper()->Map(ctx.GetLocation()));
    } else {
        m_Loc.Reset(&ctx.GetLocation());
    }

    x_GatherInfo(ctx);
}


CReferenceItem::CReferenceItem(const CSeq_feat& feat, CBioseqContext& ctx) :
    CFlatItem(&ctx), m_PMID(0), m_MUID(0), m_Category(eUnknown), m_Serial(0),
    m_JustUids(false), m_Prepub(CImprint::ePrepub_other)
{
    _ASSERT(feat.GetData().IsPub());

    x_SetObject(feat);

    m_Pubdesc.Reset(&(feat.GetData().GetPub()));
    if ( ctx.GetMapper() != 0 ) {
        m_Loc.Reset(ctx.GetMapper()->Map(feat.GetLocation()));
    } else {
        m_Loc.Reset(&(feat.GetLocation()));
    }
}


CReferenceItem::CReferenceItem
(const CPubdesc& pub,
 CBioseqContext& ctx,
 const CSeq_loc* loc) :
    CFlatItem(&ctx), m_Pubdesc(&pub), m_Loc(loc), m_PMID(0), m_MUID(0),
    m_Category(eUnknown), m_Serial(0), m_JustUids(false),
    m_Prepub(CImprint::ePrepub_other)
{
    x_SetObject(pub);

    if ( !m_Loc ) {
        m_Loc.Reset(&ctx.GetLocation());
    }
    if ( ctx.GetMapper() != 0 ) {
        m_Loc.Reset(ctx.GetMapper()->Map(*m_Loc));
    }

    x_GatherInfo(ctx);
}


void CReferenceItem::SetLoc(const CConstRef<CSeq_loc>& loc)
{
    m_Loc = loc;
}


static void s_MergeDuplicates
(CReferenceItem::TReferences& refs,
 CBioseqContext& ctx)
{
    if ( refs.size() < 2 ) {
        return;
    }

    CReferenceItem::TReferences::iterator curr = refs.begin();
    CReferenceItem::TReferences::iterator prev = curr;

    while ( curr != refs.end() ) {
        if ( !*curr ) {
            curr = refs.erase(curr);
            if ( curr == refs.end() ) {
                break;
            }
        }
        _ASSERT(*curr);
        bool remove = false;
        bool merge  = true;

        const CReferenceItem& curr_ref = **curr;
        if ( curr_ref.JustUids() ) {
            remove = true;
        } else {
            // EMBL patent records do not need author or title - A29528.1
            // do not allow no author reference to appear by itself - U07000.1
            if ( !(ctx.IsEMBL()  &&  ctx.IsPatent())  &&
                 curr_ref.GetAuthors() == 0 ) {
                remove = true;
                merge = false;
            }
        }
        if ( (prev != curr)  &&  prev->NotEmpty() ) {
            const CReferenceItem& prev_ref = **prev;
            if ( curr_ref.GetPMID() == prev_ref.GetPMID()  &&  curr_ref.GetPMID() != 0 ) {
                remove = true;
            }
            if ( remove  &&
                 prev_ref.GetReftype() == CPubdesc::eReftype_seq  &&
                 curr_ref.GetReftype() != CPubdesc::eReftype_seq ) {
                // real range trumps sites
                merge = false;
            }
            if ( prev_ref.GetLoc() == 0 ) {
                merge = false;
            }
        } else {
            merge = false;
        }
        if ( remove ) {
            CConstRef<CSeq_loc> merged_loc;
            if ( merge  &&  (curr_ref.GetLoc() != 0) ) {
                merged_loc.Reset(SeqLocMerge(ctx.GetHandle(),
                    *(*prev)->GetLoc(), *curr_ref.GetLoc(), fFuseAbutting));
            }
            (*prev)->SetLoc(merged_loc);
            curr = refs.erase(curr);
            if ( curr == refs.end() ) {
                break;
            }
        } else {
            prev = curr;
            ++curr;
        }
    }
}


void CReferenceItem::Rearrange(TReferences& refs, CBioseqContext& ctx)
{
    {{
        sort(refs.begin(), refs.end(), LessEqual(false, ctx.IsRefSeq()));
    }}

    {{
        // merge duplicate references
        s_MergeDuplicates(refs, ctx);
    }}

    {{
        // !!! add submit reference
    }}

    {{
        // re-sort, take serial number into consideration.
        sort(refs.begin(), refs.end(), LessEqual(true, ctx.IsRefSeq()));
    }}
    
    // assign final serial numbers
    size_t size = refs.size();
    for ( size_t i = 0;  i < size; ++i ) {
        refs[i]->m_Serial = i + 1;
    }
}


void CReferenceItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatReference(*this, text_os);
}


bool CReferenceItem::Matches(const CPub_set& ps) const
{
    if ( !ps.IsPub() ) {
        return false;
    }

    ITERATE (CPub_set::TPub, it, ps.GetPub()) {
        if ( x_Matches(**it) ) {
            return true;
        }
    }
    return false;
}


bool CReferenceItem::x_Matches(const CPub& pub) const
{
    switch ( pub.Which() ) {
    case CPub::e_Muid:
        return pub.GetMuid() == GetMUID();
    case CPub::e_Pmid:
        return pub.GetPmid() == GetPMID();
    case CPub::e_Equiv:
        ITERATE (CPub::TEquiv::Tdata, it, pub.GetEquiv().Get()) {
            if ( x_Matches(**it) ) {
                return true;
            }
        }
        break;
    default:
        {
            if ( !m_UniqueStr.empty() ) {
                string unique;
                pub.GetLabel(&unique, CPub::eContent, true);
                size_t len = unique.length();
                if ( len > 0  &&  unique[len - 1] == '>' ) {
                    --len;
                }
                len = min(len , m_UniqueStr.length());
                unique.resize(len);
                if ( NStr::StartsWith(m_UniqueStr, unique, NStr::eNocase) ) {
                    return true;
                }
            }
        break;
        }
    }
    return false;
}



void CReferenceItem::x_GatherInfo(CBioseqContext& ctx)
{
    if ( !m_Pubdesc->CanGetPub() ) {
        x_SetSkip();
    }

    if ( ctx.GetSubmitBlock() != 0 ) {
        m_Title = "Direct Submission";
        m_Category = eSubmission;
    }
    CPub_equiv::Tdata::const_iterator last = m_Pubdesc->GetPub().Get().end()--;
    ITERATE (CPub_equiv::Tdata, it, m_Pubdesc->GetPub().Get()) {
        x_Init(**it, ctx);

        // set unique str
        // skip over just serial number
        if ( (*it)->IsGen()  &&  it != last ) {
            const CCit_gen& gen = (*it)->GetGen();

            if ( !gen.CanGetCit()  ||
                 !NStr::StartsWith(gen.GetCit(), "BackBone id_pub", NStr::eNocase) ) {
                if ( !gen.CanGetCit()  &&
                      !gen.CanGetJournal()  &&
                      !gen.CanGetDate()  &&
                      gen.CanGetSerial_number()  &&
                      gen.GetSerial_number() > 0 ) {
                     continue;
                }
            }
        }
        if ( m_UniqueStr.empty() ) {
            (*it)->GetLabel(&m_UniqueStr, CPub::eContent, true);
        }
    }
    x_CleanData();

    // gather Genbank specific fields (formats: Genbank, GBSeq, DDBJ)
    if ( ctx.IsGenbankFormat() ) {
        x_GatherRemark(ctx);
    }
}
 

void CReferenceItem::x_Init(const CPub& pub, CBioseqContext& ctx)
{
    switch (pub.Which()) {
    case CPub::e_Gen:
        x_Init(pub.GetGen(), ctx);
        break;

    case CPub::e_Sub:
        x_Init(pub.GetSub(), ctx);
        break;

    case CPub::e_Medline:
        x_Init(pub.GetMedline(), ctx);
        break;

    case CPub::e_Muid:
        if ( m_MUID == 0 ) {
            m_MUID = pub.GetMuid();
            m_Category = ePublished;
        }
        break;

    case CPub::e_Article:
        x_Init(pub.GetArticle(), ctx);
        break;

    case CPub::e_Journal:
        x_Init(pub.GetJournal(), ctx);
        break;

    case CPub::e_Book:
        x_Init(pub.GetBook(), ctx);
        break;

    case CPub::e_Proc:
        x_Init(pub.GetProc().GetBook(), ctx);
        break;

    case CPub::e_Patent:
        x_Init(pub.GetPatent(), ctx);
        break;

    case CPub::e_Man:
        x_Init(pub.GetMan(), ctx);
        break;

    case CPub::e_Equiv:
        ITERATE (CPub_equiv::Tdata, it, pub.GetEquiv().Get()) {
            x_Init(**it, ctx);
        }
        break;

    case CPub::e_Pmid:
        if ( m_PMID == 0 ) {
            m_PMID = pub.GetPmid();
            m_Category = ePublished;
        }
        break;

    default:
        break;
    }
}


void CReferenceItem::x_Init(const CCit_gen& gen, CBioseqContext& ctx)
{
    // serial
    if ( gen.CanGetSerial_number()  &&  m_Serial == 0 ) {
        m_Serial = gen.GetSerial_number();
    }

    // title
    if ( m_Title.empty() ) {
        if ( gen.CanGetTitle()  &&  !gen.GetTitle().empty() ) {
            m_Title = gen.GetTitle();
        } else {
            if ( gen.CanGetCit()  &&  !gen.GetCit().empty() ) {
                const string& cit = gen.GetCit();
                SIZE_TYPE pos = NStr::Find(cit, "Title=\"");
                if ( pos != NPOS ) {
                    pos += 7;
                    SIZE_TYPE end = cit.find_first_of('"', pos);
                    m_Title = cit.substr(pos, end - pos);
                }
            }
        }
    }
    
    // Journal
    x_SetJournal(gen, ctx);
    
    // Authors
    if ( gen.CanGetAuthors() ) {
        x_AddAuthors(gen.GetAuthors());
    }

    // MUID
    if ( gen.CanGetMuid()  &&  m_MUID == 0 ) {
        m_MUID = gen.GetMuid();
    }
    
    // PMID
    if ( gen.CanGetPmid()  &&  m_PMID == 0 ) {
        m_PMID = gen.GetPmid();
    }

    // Volume
    if ( gen.CanGetVolume()  &&  m_Volume.empty() ) {
        m_Volume = gen.GetVolume();
    }

    // Issue
    if ( gen.CanGetIssue()  &&  m_Issue.empty() ) {
        m_Issue = gen.GetIssue();
    }

    // Pages
    if ( gen.CanGetPages()  &&  m_Pages.empty() ) {
        m_Pages = gen.GetPages();
        s_FixPages(m_Pages);
    }

    // Date
    if ( gen.CanGetDate()  &&  !m_Date ) {
        m_Date = &gen.GetDate();
    }
}


void CReferenceItem::x_Init(const CCit_sub& sub, CBioseqContext& ctx)
{
    // Title
    m_Title = "Direct Submission";

    // Authors
    x_AddAuthors(sub.GetAuthors());
    if ( sub.GetAuthors().CanGetAffil() ) {
        FormatAffil(sub.GetAuthors().GetAffil(), m_Journal);
    }

    // Date
    if ( sub.CanGetDate() ) {
        m_Date = &sub.GetDate();
    } else {
        // complain?
        if ( sub.CanGetImp() ) {
            m_Date = &sub.GetImp().GetDate();
        }
    }

    m_Category = eSubmission;
}


void CReferenceItem::x_Init(const CMedline_entry& mle, CBioseqContext& ctx)
{
    m_Category = ePublished;

    if ( mle.CanGetUid()  &&  m_MUID == 0 ) {
        m_MUID = mle.GetUid();
    }

    if ( !m_Date ) {
        m_Date = &mle.GetEm();
    }

    if ( mle.CanGetPmid()  &&  m_PMID == 0 ) {
        m_PMID = mle.GetPmid();
    }

    if ( mle.CanGetCit() ) {
        x_Init(mle.GetCit(), ctx);
    }
}


void CReferenceItem::x_Init(const CCit_art& art, CBioseqContext& ctx)
{
    // Title
    if ( art.CanGetTitle()  &&  !art.GetTitle().Get().empty() ) {
        m_Title = art.GetTitle().GetTitle();
    }

    // Authors
    if ( art.CanGetAuthors() ) {
        x_AddAuthors(art.GetAuthors());
    }

    // Journal
    switch ( art.GetFrom().Which() ) {
    case CCit_art::C_From::e_Journal:
        x_Init(art.GetFrom().GetJournal(), ctx);
        break;
    case CCit_art::C_From::e_Book:
        x_Init(art.GetFrom().GetBook(), ctx, true);
        break;
    case CCit_art::C_From::e_Proc:
        x_Init(art.GetFrom().GetProc().GetBook(), ctx, true);
        break;
    default:
        break;
    }

    if ( art.CanGetIds() ) {
        ITERATE (CArticleIdSet::Tdata, it, art.GetIds().Get()) {
            switch ( (*it)->Which() ) {
            case CArticleId::e_Pubmed:
                if ( m_PMID == 0 ) {
                    m_PMID = (*it)->GetPubmed();
                }
                break;
            case CArticleId::e_Medline:
                if ( m_MUID == 0 ) {
                    m_MUID = (*it)->GetMedline();
                }
                break;
            default:
                break;
            }
        }
    }
}


void CReferenceItem::x_Init(const CCit_jour& jour, CBioseqContext& ctx)
{
    x_SetJournal(jour.GetTitle(), ctx);
    x_AddImprint(jour.GetImp(), ctx);
}





void CReferenceItem::x_Init
(const CCit_book& book,
 CBioseqContext& ctx,
 bool for_art)
{
    if ( !for_art ) {
        if ( book.CanGetImp() ) {
            x_AddImprint(book.GetImp(), ctx);
        }
        if ( book.CanGetTitle()  &&  !book.GetTitle().Get().empty() ) {
            m_Journal = book.GetTitle().GetTitle();
        }
        if ( m_Authors.Empty()  &&  book.CanGetAuthors() ) {
            x_AddAuthors(book.GetAuthors());
        }
    }  else if ( book.CanGetTitle()  &&  book.CanGetImp() ) {
        m_Book.Reset(&book);
        x_AddImprint(book.GetImp(), ctx);
    }
}


void CReferenceItem::x_Init(const CCit_pat& pat, CBioseqContext& ctx)
{
    bool embl = ctx.Config().IsFormatEMBL();
    m_Category = ePublished;

    m_Title = pat.GetTitle();
    x_AddAuthors(pat.GetAuthors());

    m_Journal = (embl ? "Patent number " : "Patent: ") + pat.GetCountry() + ' ';
    if ( pat.CanGetNumber()  &&  !pat.GetNumber().empty() ) {
        m_Journal += pat.GetNumber();
    } else if ( pat.CanGetApp_number()  &&  !pat.GetApp_number().empty() ) {
        m_Journal += pat.GetApp_number();
    }
    if ( pat.CanGetDoc_type()  &&  !pat.GetDoc_type().empty() ) {
        m_Journal += '-';
        m_Journal += pat.GetDoc_type();
    }

    CPatent_seq_id::TSeqid seqid = 0;
    ITERATE (CBioseq::TId, it, ctx.GetBioseqIds()) {
        if ((*it)->IsPatent()) {
            seqid = (*it)->GetPatent().GetSeqid();
        }
    }
    if ( seqid > 0 ) {
        m_Journal += embl ? '/' : ' ';
        m_Journal += NStr::IntToString(seqid);
        if ( embl ) {
            m_Journal += ", ";
        }
    } else {
        m_Journal += ' ';
    }
    if ( pat.CanGetDate_issue() ) {
        m_Journal += ' ';
        DateToString(pat.GetDate_issue(), m_Journal);
    } else if ( pat.CanGetApp_date() ) {
        m_Journal += ' ';
        DateToString(pat.GetApp_date(), m_Journal);
    }
    m_Journal += (embl ? '.' : ';');
}


void CReferenceItem::x_Init(const CCit_let& man, CBioseqContext& ctx)
{
    x_Init(man.GetCit(), ctx);

    if (man.CanGetType()  &&  man.GetType() == CCit_let::eType_thesis) {
        const CImprint& imp = man.GetCit().GetImp();
        if ( !m_Journal.empty() ) {
            m_Title = m_Journal;
            m_Journal.erase();
        }
        m_Journal = "Thesis (";
        imp.GetDate().GetDate(&m_Journal, "%Y");
        m_Journal += ')';
        if (imp.CanGetPrepub()
            &&  imp.GetPrepub() == CImprint::ePrepub_in_press) {
            m_Journal += ", In press";
        }
        if (imp.CanGetPub()) {
            string affil;
            FormatAffil(imp.GetPub(), affil);
            replace(affil.begin(), affil.end(), '\"', '\'');
            m_Journal += ' ' + affil;
        }
        m_Date.Reset();
    }
}


void CReferenceItem::x_AddAuthors(const CAuth_list& auth_list)
{
    m_Authors.Reset(&auth_list);

    if ( !auth_list.CanGetNames() ) return;
    
    // also populate the consortium (if available)
    if ( !m_Consortium.empty() ) return;

    const CAuth_list::TNames& names = auth_list.GetNames();
    
    if ( names.IsStd() ) {
        ITERATE (CAuth_list::TNames::TStd, it, names.GetStd()) {
            const CAuthor& auth = **it;
            if ( auth.CanGetName()  &&  
                 auth.GetName().IsConsortium() ) {
                m_Consortium = auth.GetName().GetConsortium();
                break;
            }
        }
    }
}


void CReferenceItem::x_SetJournal(const CTitle& title, CBioseqContext& ctx)
{
    // Only GenBank/EMBL/DDBJ require ISO JTA in ENTREZ/RELEASE modes.
    bool require_iso_jta = ctx.Config().CitArtIsoJta();
    if ( !ctx.IsGED()  &&  !ctx.IsTPA() ) {
        require_iso_jta = false;
    }
    // always use iso_jta title if present
    const CTitle::C_E* ttl = 0;
    ITERATE (CTitle::Tdata, it, title.Get()) {
        if ( (*it)->IsIso_jta() ) {
            ttl = *it;
            break;
        }
    }

    bool electronic_journal = false;
    if ( ttl == 0 ) {
        ttl = title.Get().front();
        if ( ttl != 0  &&  ttl->IsName() ) {
            if ( NStr::StartsWith(ttl->GetName(), "(er)", NStr::eNocase) ) {
                electronic_journal = true;
            }
        }
        if ( require_iso_jta  &&  !electronic_journal ) {
            return;
        }

        m_Journal = title.GetTitle();
    } else {
        m_Journal = ttl->GetIso_jta();
    }
}


void CReferenceItem::x_SetJournal(const CCit_gen& gen, CBioseqContext& ctx)
{
    if ( !gen.CanGetCit()  &&  !gen.CanGetJournal()  &&  !gen.CanGetDate()  &&
         gen.CanGetSerial_number()  &&  gen.GetSerial_number() != 0 ) {
        return;
    }

    if ( !gen.CanGetJournal()  &&
          gen.CanGetCit()  &&  
          NStr::StartsWith(gen.GetCit(), "unpublished", NStr::eNocase) ) {
        const string& cit = gen.GetCit();

        if ( ctx.Config().NoAffilOnUnpub() ) {
            m_Category = eUnpublished;
            m_Journal = "Unpublished";
            return;
        }

        if ( gen.CanGetAuthors()  &&  gen.GetAuthors().CanGetAffil() ) {
            string affil;
            (gen.GetAuthors().GetAffil(), affil);
            if ( !affil.empty() ) {
                m_Category = eUnpublished;
                m_Journal = "Unpublished " + affil;
                return;
            }
        }

        m_Journal = cit;
        return;
    }
    
    string result, in_press;
    if ( gen.CanGetJournal() ) {
        x_SetJournal(gen.GetJournal(), ctx);
        result = m_Journal;
    }

    if ( gen.CanGetCit() ) {
        const string& cit = gen.GetCit();

        SIZE_TYPE pos = NStr::FindNoCase(cit, "Journal=\"");
        if ( pos != NPOS ) {
            result = cit.substr(pos + 9);
        } else if ( NStr::StartsWith(cit, "submitted", NStr::eNocase)  ||
                    NStr::StartsWith(cit, "unpublished", NStr::eNocase) ) {
            if ( !ctx.Config().DropBadCitGens()  ||  !result.empty() ) {
                in_press = cit;
            } else {
                in_press = "Unpublished";
            }
        } else if ( NStr::StartsWith(cit, "Online Publication", NStr::eNocase)  ||
                    NStr::StartsWith(cit, "Published Only in DataBase", NStr::eNocase) ) {
            in_press = cit;
        } else if ( !ctx.Config().DropBadCitGens()  &&  result.empty() ) {
            result = cit;
        }
    }

    if ( !result.empty() ) {
        SIZE_TYPE pos = result.find_first_of("=\"");
        if ( pos != NPOS ) {
            result.erase(pos);
        }
    }
    
    m_Journal = result;
    if ( !in_press.empty() ) {
        m_Journal += ' ';
        m_Journal += in_press;
    }
}


void CReferenceItem::x_AddImprint(const CImprint& imp, CBioseqContext& ctx)
{
    if ( !m_Date ) {
        m_Date.Reset(&imp.GetDate());
    }
    if ( imp.IsSetVolume()  &&  !imp.GetVolume().empty()  ) {
        m_Volume = imp.GetVolume();
        if ( imp.IsSetPart_sup()  &&  !imp.GetPart_sup().empty() ) {
            m_Volume += " (" + imp.GetPart_sup() + ")";
        }
    }
    
    if ( imp.IsSetIssue()  &&  !imp.GetIssue().empty() ) {
        m_Issue = imp.GetIssue();
        if ( imp.IsSetPart_supi()  &&  !imp.GetPart_supi().empty() ) {
            m_Issue += " (" + imp.GetPart_supi() + ")";
        }
    }
    if ( imp.IsSetPages()  &&  !imp.GetPages().empty() ) {
        m_Pages = imp.GetPages();
        s_FixPages(m_Pages);
    }
    if ( imp.IsSetPrepub() ) {
        m_Prepub = imp.GetPrepub();
        m_Category = 
            m_Prepub != CImprint::ePrepub_in_press ? eUnpublished : ePublished;
    } else {
        m_Category = ePublished;
    }
}



void CReferenceItem::GetAuthNames
(list<string>& authors,
 const CAuth_list* alp)
{   
    authors.clear();

    if ( alp == 0 ) {
        return;
    }

    const CAuth_list::TNames& names = alp->GetNames();
    switch ( names.Which() ) {
    case CAuth_list::TNames::e_Std:
        ITERATE (CAuth_list::TNames::TStd, it, names.GetStd()) {
            if ( !(*it)->CanGetName() ) {
                continue;
            }
            const CPerson_id& pid = (*it)->GetName();
            string name;
            pid.GetLabel(&name, CPerson_id::eGenbank);
            authors.push_back(name);
        }
        break;
        
    case CAuth_list::TNames::e_Ml:
        authors.insert(authors.end(),
            names.GetMl().begin(), names.GetMl().end());
        break;
        
    case CAuth_list::TNames::e_Str:
        authors.insert(authors.end(),
            names.GetStr().begin(), names.GetStr().end());
        break;
        
    default:
        break;
    }
}


string CReferenceItem::GetAuthString(const CAuth_list* alp)
{
    list<string> authors;

    if ( alp == 0 ) {
        authors.push_back(".");
    } else {
        GetAuthNames(authors, alp);
    }

    CNcbiOstrstream auth_line;
    list<string>::const_iterator last = --(authors.end());

    string separator = kEmptyStr;
    bool first = true;
    ITERATE(list<string>, it, authors) {
        if ( it == last ) {
            separator = " and ";
        }
        auth_line << (first ? kEmptyStr : separator) << *it;
        separator = ", ";
        first = false;
    }

    return CNcbiOstrstreamToString(auth_line);
}


void CReferenceItem::x_CleanData(void)
{
    // !!!
    // remove spaces from title etc.
}


/////////////////////////////////////////////////////////////////////////////
//
// Genbank Format Specific

// these must be in "ASCIIbetical" order; beware of the fact that
// closing quotes sort after spaces.
static const string sc_RemarkText[] = {
  "full automatic",
  "full staff_entry",
  "full staff_review",
  "simple automatic",
  "simple staff_entry",
  "simple staff_review",
  "unannotated automatic",
  "unannotated staff_entry",
  "unannotated staff_review"
};
static const CStaticArraySet<string> sc_Remarks(sc_RemarkText, sizeof(sc_RemarkText));


void CReferenceItem::x_GatherRemark(CBioseqContext& ctx)
{
    list<string> l;

    // comment
    if ( m_Pubdesc->IsSetComment()  &&  !m_Pubdesc->GetComment().empty() ) {
        const string& comment = m_Pubdesc->GetComment();
        
        if ( sc_Remarks.find(comment) == sc_Remarks.end() ) {
            l.push_back(comment);
        }
    }

    // for GBSeq format collect remarks only from comments.
    if ( ctx.Config().IsFormatGBSeq() ) {
        if ( !l.empty() ) {
            m_Remark = l.front();
        }
        return;
    }

    // GIBBSQ
    CSeq_id::TGibbsq gibbsq = 0;
    ITERATE (CBioseq::TId, it, ctx.GetBioseqIds()) {
        if ( (*it)->IsGibbsq() ) {
            gibbsq = (*it)->GetGibbsq();
        }
    }
    if ( gibbsq > 0 ) {
        static const string str1 = 
            "GenBank staff at the National Library of Medicine created this entry [NCBI gibbsq ";
        static const string str2 = "] from the original journal article.";
        l.push_back(str1 + NStr::IntToString(gibbsq) + str2);

        // Figure
        if ( m_Pubdesc->IsSetFig()  &&  !m_Pubdesc->GetFig().empty()) {
            l.push_back("This sequence comes from " + m_Pubdesc->GetFig());
        }

        // Poly_a
        if ( m_Pubdesc->IsSetPoly_a()  &&  m_Pubdesc->GetPoly_a() ) {
        l.push_back("Polyadenylate residues occurring in the figure were \
            omitted from the sequence.");
        }

        // Maploc
        if ( m_Pubdesc->IsSetMaploc()  &&  !m_Pubdesc->GetMaploc().empty()) {
            l.push_back("Map location: " + m_Pubdesc->GetMaploc());
        }
    }
    
    if ( m_Pubdesc->CanGetPub() ) {
        ITERATE (CPubdesc::TPub::Tdata, it, m_Pubdesc->GetPub().Get()) {
            if ( (*it)->IsArticle() ) {
                if ( (*it)->GetArticle().GetFrom().IsJournal() ) {
                    const CCit_jour& jour = 
                        (*it)->GetArticle().GetFrom().GetJournal();
                    if ( jour.IsSetImp() ) {
                        const CCit_jour::TImp& imp = jour.GetImp();
                        if ( imp.IsSetRetract() ) {
                            const CCitRetract& ret = imp.GetRetract();
                            if ( ret.IsSetType()  &&
                                ret.GetType() == CCitRetract::eType_in_error  &&
                                ret.IsSetExp()  &&
                                !ret.GetExp().empty() ) {
                                l.push_back("Erratum:[" + ret.GetExp() + "]");
                            }
                        }
                    }
                }
            } else if ( (*it)->IsSub() ) {
                const CCit_sub& sub = (*it)->GetSub();
                if ( sub.IsSetDescr()  &&  !sub.GetDescr().empty() ) {
                    l.push_back(sub.GetDescr());
                }
            }
        }
    }

    m_Remark = NStr::Join(l, "\n");
}


/////////////////////////////////////////////////////////////////////////////
//
// Reference Sorting


static size_t s_CountFields(const CDate& date)
{
    _ASSERT(date.IsStd());

    const CDate::TStd& std = date.GetStd();
    size_t count = 0;

    if ( std.IsSetYear() ) {
        ++count;
    }
    if ( std.IsSetMonth() ) {
        ++count;
    }
    if ( std.IsSetDay() ) {
        ++count;
    }
    if ( std.IsSetHour() ) {
        ++count;
    }
    if ( std.IsSetMinute() ) {
        ++count;
    }
    if ( std.IsSetSecond() ) {
        ++count;
    }

    return count;
}


LessEqual::LessEqual(bool serial_first, bool is_refseq) :
    m_SerialFirst(serial_first), m_IsRefSeq(is_refseq)
{}


bool LessEqual::operator()
(const CRef<CReferenceItem>& ref1,
 const CRef<CReferenceItem>& ref2)
{
    if ( m_SerialFirst  &&  ref1->GetSerial() != ref2->GetSerial() ) {
        return ref1->GetSerial() < ref2->GetSerial();
    }

    // sort by category (published / unpublished / submission)
    if ( ref1->GetCategory() != ref2->GetCategory() ) {
        return ref1->GetCategory() < ref2->GetCategory();
    }

    // sort by date:
    // - publications with date come before those without one.
    // - more specific dates come before less specific ones.
    // - older publication comes first (except RefSeq).
    const CDate* d1 = ref1->GetDate();
    const CDate* d2 = ref2->GetDate();
    if ( (d1 != 0)  &&  (d2 == 0) ) {
        return true;
    } else if ( (d1 == 0)  &&  (d2 != 0) ) {
        return false;
    }
    if ( (d1 != 0)  &&  (d2 != 0) ) {
        CDate::ECompare status = ref1->GetDate()->Compare(*ref2->GetDate());
        if ( status == CDate::eCompare_unknown  &&
             d1->IsStd()  &&  d2->IsStd() ) {
            // one object is more specific than the other.
            size_t s1 = s_CountFields(*d1);
            size_t s2 = s_CountFields(*d2);
            return m_IsRefSeq ? s1 > s2 : s1 < s2;
        } else if ( status != CDate::eCompare_same ) {
            return m_IsRefSeq ? (status == CDate::eCompare_after) :
                                (status == CDate::eCompare_before);
        }
    }
    // dates are the same, or both missing.
    
    // distinguish by uids (swap order for RefSeq)
    if ( ref1->GetPMID() != 0  &&  ref2->GetPMID() != 0  &&
         !(ref1->GetPMID() == ref2->GetPMID()) ) {
        return m_IsRefSeq ? (ref1->GetPMID() > ref2->GetPMID()) :
            (ref1->GetPMID() < ref2->GetPMID());
    }
    if ( ref1->GetMUID() != 0  &&  ref2->GetMUID() != 0  &&
         !(ref1->GetMUID() == ref2->GetMUID()) ) {
        return m_IsRefSeq ? (ref1->GetMUID() > ref2->GetMUID()) :
            (ref1->GetMUID() < ref2->GetMUID());
    }

    // just uids goes last
    if ( (ref1->GetPMID() != 0  &&  ref2->GetPMID() != 0)  ||
         (ref1->GetMUID() != 0  &&  ref2->GetMUID() != 0) ) {
        if ( ref1->JustUids()  &&  !ref2->JustUids() ) {
            return true;
        } else if ( !ref1->JustUids()  &&  ref2->JustUids() ) {
            return false;
        }
    }

    if ( ref1->GetReftype() != CPubdesc::eReftype_seq ) {
        return true;
    } else if ( ref2->GetReftype() != CPubdesc::eReftype_seq ) {
        return false;
    }
    
    // next use AUTHOR string
    string auth1 = CReferenceItem::GetAuthString(ref1->GetAuthors());
    string auth2 = CReferenceItem::GetAuthString(ref2->GetAuthors());
    int comp = NStr::CompareNocase(auth1, auth2);
    if ( comp != 0 ) {
        return comp < 0;
    }

    // !!! unique string ???

    if ( !m_SerialFirst ) {
        return ref1->GetSerial() < ref2->GetSerial();
    }

    return true;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.17  2004/05/20 17:11:45  ucko
* Reset strings with erase() rather than clear(), which isn't 100% portable.
*
* Revision 1.16  2004/05/20 13:47:54  shomrat
* Fixed Pub-set match
*
* Revision 1.15  2004/05/15 13:22:45  ucko
* Sort sc_RemarkText, and note the requirement.
*
* Revision 1.14  2004/05/14 13:15:25  shomrat
* Fixed use of CStaticArraySet
*
* Revision 1.13  2004/05/06 17:59:30  shomrat
* Handle duplicate references
*
* Revision 1.12  2004/04/27 15:14:04  shomrat
* Added logic for partial range formatting
*
* Revision 1.11  2004/04/22 15:56:30  shomrat
* Changes in context
*
* Revision 1.10  2004/04/13 16:49:36  shomrat
* GBSeq format changes
*
* Revision 1.9  2004/03/26 17:26:34  shomrat
* Set category to unpublished where needed
*
* Revision 1.8  2004/03/18 15:44:21  shomrat
* Fixes to REFERENCE formatting
*
* Revision 1.7  2004/03/10 16:21:17  shomrat
* Fixed reference sorting, Added REMARK gathering
*
* Revision 1.6  2004/03/05 18:42:59  shomrat
* Set category to Published if title exist
*
* Revision 1.5  2004/02/24 17:24:27  vasilche
* Added missing include <Pub.hpp>
*
* Revision 1.4  2004/02/11 22:55:44  shomrat
* use IsModeRelease method
*
* Revision 1.3  2004/02/11 17:00:46  shomrat
* minor changes to Matches method
*
* Revision 1.2  2003/12/18 17:43:36  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:24:12  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
