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
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <serial/iterator.hpp>
#include <util/static_set.hpp>
#include <objects/biblio/biblio__.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <algorithm>

#include <objtools/format/flat_expt.hpp>
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


void CReferenceItem::FormatAffil(const CAffil& affil, string& result, bool gen_sub)
{
    result.erase();

    if (affil.IsStr()) {
        result = affil.GetStr();
    } else if (affil.IsStd()) {
        const CAffil::C_Std& std = affil.GetStd();
        if (gen_sub) {
            if (std.IsSetDiv()) {
                result = std.GetDiv();
            }
            if (std.IsSetAffil()) {
                if (!result.empty()) {
                    result += ", ";
                }
                result += std.GetAffil();
            }
            
        } else {
            if (std.IsSetAffil()) {
                result = std.GetAffil();
            }
            if (std.IsSetDiv()) {
                if (!result.empty()) {
                    result += ", ";
                }
                result = std.GetDiv();
            } 
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
        if (gen_sub  &&  std.IsSetPostal_code()) {
            if (!result.empty()) {
                result += ' ';
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
    if (gen_sub) {
        ConvertQuotes(result);
    }
    NStr::TruncateSpacesInPlace(result);
}


CReferenceItem::CReferenceItem(const CSeqdesc& desc, CBioseqContext& ctx) :
    CFlatItem(&ctx), m_PubType(ePub_not_set), m_Category(eUnknown),
    m_PatentId(0), m_PMID(0), m_MUID(0), m_Serial(kMax_Int),
    m_JustUids(true), m_Elect(false)
{
    _ASSERT(desc.IsPub());
    
    x_SetObject(desc.GetPub());
    m_Pubdesc.Reset(&(desc.GetPub()));

    if (ctx.GetMapper() != NULL) {
        m_Loc.Reset(ctx.GetMapper()->Map(ctx.GetLocation()));
    } else {
        m_Loc.Reset(&ctx.GetLocation());
    }

    x_GatherInfo(ctx);
}


CReferenceItem::CReferenceItem
(const CSeq_feat& feat,
 CBioseqContext& ctx,
 const CSeq_loc* loc) :
    CFlatItem(&ctx), m_PubType(ePub_not_set), m_Category(eUnknown),
    m_PatentId(0), m_PMID(0), m_MUID(0), m_Serial(kMax_Int),
    m_JustUids(true), m_Elect(false)
{
    _ASSERT(feat.GetData().IsPub());

    x_SetObject(feat);

    m_Pubdesc.Reset(&(feat.GetData().GetPub()));
    
    if (loc != NULL) {
        m_Loc.Reset(loc);
    } else if ( ctx.GetMapper() != 0 ) {
        m_Loc.Reset(ctx.GetMapper()->Map(feat.GetLocation()));
    } else {
        m_Loc.Reset(&(feat.GetLocation()));
    }
    // cleanup location
    m_Loc = Seq_loc_Merge(*m_Loc, CSeq_loc::fMerge_All, &ctx.GetScope());

    x_GatherInfo(ctx);
}


CReferenceItem::CReferenceItem(const CSubmit_block& sub, CBioseqContext& ctx) :
    CFlatItem(&ctx), m_PubType(ePub_sub), m_Category(eSubmission),
    m_PatentId(0), m_PMID(0), m_MUID(0), m_Serial(kMax_Int),
    m_JustUids(false), m_Elect(false)
{
    x_SetObject(sub);

    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetWhole(*ctx.GetPrimaryId());
    m_Loc = loc;

    if (sub.IsSetCit()) {
        m_Sub.Reset(&sub.GetCit());
        m_Title = "Direct Submission";
        m_PubType = ePub_sub;
        if (m_Sub->IsSetAuthors()) {
            m_Authors.Reset(&m_Sub->GetAuthors());
        }
        if (m_Sub->IsSetDate()) {
            m_Date.Reset(&m_Sub->GetDate());
        }
    } else {
        x_SetSkip();
    }
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
            continue;
        }
        _ASSERT(*curr);

        bool remove = false;
        bool merge  = true;

        const CReferenceItem& curr_ref = **curr;
        if ( curr_ref.IsJustUids() ) {
            remove = true;
        } else {
            // EMBL patent records do not need author or title - A29528.1
            // do not allow no author reference to appear by itself - U07000.1
            if (!(ctx.IsEMBL()  &&  ctx.IsPatent())  &&  !curr_ref.IsSetAuthors()) {
                remove = true;
                merge = false;
            }
        }
        if (prev != curr) {
            const CReferenceItem& prev_ref = **prev;
            if (curr_ref.GetPMID() == prev_ref.GetPMID()  &&
                curr_ref.GetPMID() != 0) {
                remove = true;
            } else if (curr_ref.GetMUID() == prev_ref.GetMUID()  &&
                curr_ref.GetMUID() != 0) {
                remove = true;
            } else if (curr_ref.GetPMID() == 0  &&  curr_ref.GetMUID() == 0  &&
                prev_ref.GetPMID() == 0  &&  prev_ref.GetMUID() == 0) {
                const string& curr_unique = curr_ref.GetUniqueStr();
                const string& prev_unique = prev_ref.GetUniqueStr();
                if (NStr::EqualNocase(curr_unique, prev_unique)  &&
                    !NStr::IsBlank(curr_unique)) {
                    const CSeq_loc& curr_loc = curr_ref.GetLoc();
                    const CSeq_loc& prev_loc = prev_ref.GetLoc();
                    if (Compare(curr_loc, prev_loc, &ctx.GetScope()) == eSame) {
                        string curr_auth, prev_auth;
                        CReferenceItem::FormatAuthors(curr_ref.GetAuthors(), curr_auth);
                        CReferenceItem::FormatAuthors(prev_ref.GetAuthors(), prev_auth);
                        if (NStr::EqualNocase(curr_auth, prev_auth)) {
                            remove = true;
                        }
                    }
                }
            }
            if ( remove  &&
                 prev_ref.GetReftype() == CPubdesc::eReftype_seq  &&
                 curr_ref.GetReftype() != CPubdesc::eReftype_seq ) {
                // real range trumps sites
                merge = false;
            }
        } else {
            merge = false;
        }

        if (remove) {
            if (merge) {
                CRef<CSeq_loc> merged_loc = Seq_loc_Add(
                    curr_ref.GetLoc(),
                    (*prev)->GetLoc(),
                    CSeq_loc::fSort | CSeq_loc::fMerge_All,
                    &ctx.GetScope());
                (*prev)->SetLoc(merged_loc);
            }
            curr = refs.erase(curr);
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


static bool s_IsOnlySerial(const CPub& pub)
{
    if (!pub.IsGen()) {
        return false;
    }

    const CCit_gen& gen = pub.GetGen();

    if (!gen.IsSetCit()  ||
        !NStr::StartsWith(gen.GetCit(), "BackBone id_pub", NStr::eNocase)) {
        if (!gen.IsSetJournal()  &&  !gen.IsSetDate()  &&
            gen.IsSetSerial_number()  &&  gen.GetSerial_number() > 0) {
            return true;
        }
    }

    return false;
}

    
void CReferenceItem::x_CreateUniqueStr(void) const
{
    if (!NStr::IsBlank(m_UniqueStr)) {  // already created
        return;
    }
    if (m_Pubdesc.Empty()) {  // not pub to generate from
        return;
    }

    ITERATE (CPubdesc::TPub::Tdata, it, m_Pubdesc->GetPub().Get()) {
        const CPub& pub = **it;
        if (pub.IsMuid()  ||  pub.IsPmid()  ||  pub.IsPat_id()  ||  pub.IsEquiv()) {
            continue;
        }
        if (!s_IsOnlySerial(pub)) {
            pub.GetLabel(&m_UniqueStr, CPub::eContent, true);
        }
    }
}


bool CReferenceItem::x_Matches(const CPub& pub) const
{
    switch (pub.Which()) {
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
        // compare based on unique string
        {{
            x_CreateUniqueStr();
            const string& uniquestr = m_UniqueStr;

            string pub_unique;
            pub.GetLabel(&pub_unique, CPub::eContent, true);

            size_t len = pub_unique.length();
            if (len > 0  &&  pub_unique[len - 1] == '>') {
                --len;
            }
            len = min(len , uniquestr.length());
            pub_unique.resize(len);
            if (!NStr::IsBlank(uniquestr)  &&  !NStr::IsBlank(pub_unique)) {
                if (NStr::StartsWith(uniquestr, pub_unique, NStr::eNocase)) {
                    return true;
                }
            }
        break;
        }}
    }

    return false;
}



void CReferenceItem::x_GatherInfo(CBioseqContext& ctx)
{
    _ASSERT(m_Pubdesc.NotEmpty());

    if (!m_Pubdesc->IsSetPub()) {
        NCBI_THROW(CFlatException, eInvalidParam, "Pub not set on Pubdesc");
    }

    const CPubdesc::TPub& pub = m_Pubdesc->GetPub();

    /*if (ctx.GetSubmitBlock() != NULL) {
        m_Title = "Direct Submission";
        m_Sub.Reset(&ctx.GetSubmitBlock()->GetCit());
        m_PubType = ePub_sub;
    }*/

    //CPub_equiv::Tdata::const_iterator last = m_Pubdesc->GetPub().Get().end()--;
    ITERATE (CPub_equiv::Tdata, it, pub.Get()) {
        x_Init(**it, ctx);
    }

    // gather Genbank specific fields (formats: Genbank, GBSeq, DDBJ)
    if ( ctx.IsGenbankFormat() ) {
        x_GatherRemark(ctx);
    }

    x_CleanData();
}
 

void CReferenceItem::x_Init(const CPub& pub, CBioseqContext& ctx)
{
    switch (pub.Which()) {
    case CPub::e_Gen:
        x_Init(pub.GetGen(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Sub:
        x_Init(pub.GetSub(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Medline:
        x_Init(pub.GetMedline(), ctx);
        break;

    case CPub::e_Muid:
        if (m_MUID == 0) {
            m_MUID = pub.GetMuid();
            m_Category = ePublished;
        }
        break;

    case CPub::e_Article:
        x_Init(pub.GetArticle(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Journal:
        x_Init(pub.GetJournal(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Book:
        m_PubType = ePub_book;
        x_Init(pub.GetBook(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Proc:
        m_PubType = ePub_book;
        x_InitProc(pub.GetProc().GetBook(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Patent:
        x_Init(pub.GetPatent(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Man:
        x_Init(pub.GetMan(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Equiv:
        ITERATE (CPub_equiv::Tdata, it, pub.GetEquiv().Get()) {
            x_Init(**it, ctx);
        }
        break;

    case CPub::e_Pmid:
        if (m_PMID == 0) {
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
    if (m_PubType == ePub_not_set) {
        m_PubType = ePub_gen;
    }

    const string& cit = gen.IsSetCit() ? gen.GetCit() : kEmptyStr;

    if (NStr::StartsWith(cit, "BackBone id_pub", NStr::eNocase)) {
        return;
    }

    m_Gen.Reset(&gen);

    // category
    m_Category = eUnpublished;

    // serial
    if (gen.IsSetSerial_number()  &&  gen.GetSerial_number() > 0  &&
        m_Serial == kMax_Int) {
        m_Serial = gen.GetSerial_number();
    }

    // Date
    if (gen.CanGetDate()  &&  !m_Date) {
        m_Date.Reset(&gen.GetDate());
    }

    if (!NStr::IsBlank(cit)) {
        if (!NStr::StartsWith(cit, "unpublished")      &&
            !NStr::StartsWith(cit, "submitted")        &&
            !NStr::StartsWith(cit, "to be published")  &&
            !NStr::StartsWith(cit, "in press")         &&
            !NStr::Find(cit, "Journal") == NPOS        &&
            gen.IsSetSerial_number()  &&  gen.GetSerial_number() == 0) {
            x_SetSkip();
            return;
        } 
    } else if ((!gen.IsSetJournal()  ||  !m_Date)  &&  m_Serial == 0) {
        x_SetSkip();
        return;
    }

    // title
    if (NStr::IsBlank(m_Title)) {
        if (gen.CanGetTitle()  &&  !NStr::IsBlank(gen.GetTitle())) {
            m_Title = gen.GetTitle();
        } else if (!NStr::IsBlank(cit)) {
            SIZE_TYPE pos = NStr::Find(cit, "Title=\"");
            if (pos != NPOS) {
                pos += 7;
                SIZE_TYPE end = cit.find_first_of('"', pos);
                m_Title = cit.substr(pos, end - pos);
            }
        }
    }

    // Electronic publication
    if (!NStr::IsBlank(m_Title)  &&  NStr::StartsWith(m_Title, "(er)")) {
        m_Elect = true;
    }
    
    // Authors
    if (gen.CanGetAuthors()) {
        x_AddAuthors(gen.GetAuthors());
    }

    // MUID
    if (gen.CanGetMuid()  &&  m_MUID == 0) {
        m_MUID = gen.GetMuid();
    }
    
    // PMID
    if (gen.CanGetPmid()  &&  m_PMID == 0) {
        m_PMID = gen.GetPmid();
    }
}


void CReferenceItem::x_Init(const CCit_sub& sub, CBioseqContext& ctx)
{
    m_PubType = ePub_sub;
    m_Sub.Reset(&sub);

    // Title
    m_Title = "Direct Submission";

    // Authors
    if (sub.IsSetAuthors()) {
        x_AddAuthors(sub.GetAuthors());
    }

    // Date
    if (sub.CanGetDate()) {
        m_Date.Reset(&sub.GetDate());
    } 
    if (sub.CanGetImp()) {
        x_AddImprint(sub.GetImp(), ctx);
    }

    m_Category = eSubmission;
}


void CReferenceItem::x_Init(const CMedline_entry& mle, CBioseqContext& ctx)
{
    m_Category = ePublished;

    if (mle.CanGetUid()  &&  m_MUID == 0) {
        m_MUID = mle.GetUid();
    }

    if (mle.CanGetPmid()  &&  m_PMID == 0) {
        m_PMID = mle.GetPmid();
    }

    if (mle.CanGetCit()) {
        x_Init(mle.GetCit(), ctx);
    }
}


void CReferenceItem::x_Init(const CCit_art& art, CBioseqContext& ctx)
{
    // Title
    if (art.CanGetTitle()) {
        m_Title = art.GetTitle().GetTitle();
    }

    // Authors
    if ( art.CanGetAuthors() ) {
        x_AddAuthors(art.GetAuthors());
    }

    switch (art.GetFrom().Which()) {
    case CCit_art::C_From::e_Journal:
        m_PubType = ePub_jour;
        x_Init(art.GetFrom().GetJournal(), ctx);
        break;
    case CCit_art::C_From::e_Proc:
        m_PubType = ePub_book_art;
        x_Init(art.GetFrom().GetProc(), ctx);
        break;
    case CCit_art::C_From::e_Book:
        m_PubType = ePub_book_art;
        x_Init(art.GetFrom().GetBook(), ctx);
        break;
    default:
        break;
    }

    if (art.CanGetIds()) {
        ITERATE (CArticleIdSet::Tdata, it, art.GetIds().Get()) {
            switch ((*it)->Which()) {
            case CArticleId::e_Pubmed:
                if (m_PMID == 0) {
                    m_PMID = (*it)->GetPubmed();
                }
                break;
            case CArticleId::e_Medline:
                if (m_MUID == 0) {
                    m_MUID = (*it)->GetMedline();
                }
                break;
            default:
                break;
            }
        }
    }
}

void CReferenceItem::x_Init(const CCit_proc& proc, CBioseqContext& ctx)
{
    if (proc.IsSetBook()) {
        x_Init(proc.GetBook(), ctx);
    } else if (proc.IsSetMeet()) {
        // !!!
    }
}


void CReferenceItem::x_Init(const CCit_jour& jour, CBioseqContext& ctx)
{
    m_Journal.Reset(&jour);

    if (jour.IsSetImp()) {
        x_AddImprint(jour.GetImp(), ctx);
    }

    if (jour.IsSetTitle()) {
        ITERATE (CCit_jour::TTitle::Tdata, it, jour.GetTitle().Get()) {
            if ((*it)->IsName()  &&  NStr::StartsWith((*it)->GetName(), "(er)")) {
                m_Elect = true;
                break;
            }
        }
    }
}


void CReferenceItem::x_InitProc(const CCit_book& proc, CBioseqContext& ctx)
{
    m_Book.Reset();
    if (!m_Authors  &&  proc.IsSetAuthors()) {
        x_AddAuthors(proc.GetAuthors());
    } 
    if (proc.IsSetTitle()) {
        m_Title = proc.GetTitle().GetTitle();
    }
}


void CReferenceItem::x_Init
(const CCit_book& book,
 CBioseqContext& ctx)
{
    m_Book.Reset(&book);
    if (!m_Authors  &&  book.IsSetAuthors()) {
        x_AddAuthors(book.GetAuthors());
    } 
    if (book.CanGetImp()) {
        x_AddImprint(book.GetImp(), ctx);
    }
}


void CReferenceItem::x_Init(const CCit_pat& pat, CBioseqContext& ctx)
{
    //bool embl = ctx.Config().IsFormatEMBL();
    m_Patent.Reset(&pat);
    m_PubType = ePub_pat;
    m_Category = ePublished;

    if (pat.IsSetTitle()) {
        m_Title = pat.GetTitle();
    }
    if (pat.IsSetAuthors()) {
        x_AddAuthors(pat.GetAuthors());
    }
    if (pat.IsSetDate_issue()) {
        m_Date.Reset(&pat.GetDate_issue());
    } else if (pat.IsSetApp_date()) {
        m_Date.Reset(&pat.GetApp_date());
    }

    m_PatentId = ctx.GetPatentSeqId();
}


void CReferenceItem::x_Init(const CCit_let& man, CBioseqContext& ctx)
{
    if (!man.IsSetType()  ||  man.GetType() != CCit_let::eType_thesis) {
        return;
    }

    m_PubType = ePub_thesis;
  
    if (man.IsSetCit()) {
        const CCit_book& book = man.GetCit();
        x_Init(book, ctx);
        if (book.IsSetTitle()) {
            m_Title = book.GetTitle().GetTitle();
        }
    }
}


void CReferenceItem::x_AddAuthors(const CAuth_list& auth_list)
{
    m_Authors.Reset(&auth_list);

    if (!auth_list.CanGetNames()) {
        return;
    }
    
    // also populate the consortium (if available)
    if (!NStr::IsBlank(m_Consortium)) {
        return;
    }

    const CAuth_list::TNames& names = auth_list.GetNames();
    
    if (names.IsStd()) {
        ITERATE (CAuth_list::TNames::TStd, it, names.GetStd()) {
            const CAuthor& auth = **it;
            if (auth.CanGetName()  &&  auth.GetName().IsConsortium()) {
                m_Consortium = auth.GetName().GetConsortium();
                break;
            }
        }
    }
}


void CReferenceItem::x_AddImprint(const CImprint& imp, CBioseqContext& ctx)
{
    // electronic journal
    if (imp.IsSetPubstatus()) {
        CImprint::TPubstatus pubstatus = imp.GetPubstatus();
        m_Elect = (pubstatus == 3  || pubstatus == 10);
    }

    // date
    if (!m_Date  &&  imp.IsSetDate()) {
        m_Date.Reset(&imp.GetDate());
    }

    // prepub
    if (imp.IsSetPrepub()) {
        CImprint::TPrepub prepub = imp.GetPrepub();
        //m_Prepub = imp.GetPrepub();
        m_Category = 
            prepub != CImprint::ePrepub_in_press ? eUnpublished : ePublished;
    } else {
        m_Category = ePublished;
    }
}


void CReferenceItem::GetAuthNames(const CAuth_list& alp, TStrList& authors)
{   
    authors.clear();

    const CAuth_list::TNames& names = alp.GetNames();
    string name;
    switch (names.Which()) {
    case CAuth_list::TNames::e_Std:
        ITERATE (CAuth_list::TNames::TStd, it, names.GetStd()) {
            if (!(*it)->CanGetName()) {
                continue;
            }
            const CPerson_id& pid = (*it)->GetName();
            if (pid.IsName()  ||  pid.IsMl()  ||  pid.IsStr()) {
                name.erase();
                pid.GetLabel(&name, CPerson_id::eGenbank);
                authors.push_back(name);
            }
        }
        break;
        
    case CAuth_list::TNames::e_Ml:
        authors.insert(authors.end(), names.GetMl().begin(), names.GetMl().end());
        break;
        
    case CAuth_list::TNames::e_Str:
        authors.insert(authors.end(), names.GetStr().begin(), names.GetStr().end());
        break;
        
    default:
        break;
    }
}


void CReferenceItem::FormatAuthors(const CAuth_list& alp, string& auth)
{
    TStrList authors;

    CReferenceItem::GetAuthNames(alp, authors);
    if (authors.empty()) {
        return;
    }

    CNcbiOstrstream auth_line;
    TStrList::const_iterator last = --(authors.end());

    string separator = kEmptyStr;
    //bool first = true;
    ITERATE (TStrList, it, authors) {
        auth_line << separator << *it;
        ++it;
        if (it == last) {
            if (it->size() < 7) {
                if (NStr::StartsWith(*it, "et al", NStr::eNocase)  ||
                    NStr::StartsWith(*it, "et,al", NStr::eNocase)) {
                    separator = " ";
                } else {
                    separator = " and ";
                }
            } else {
                separator = " and ";
            }
        } else {
            separator = ", ";
        }
        --it;
    }

    auth = CNcbiOstrstreamToString(auth_line);
}


// Historical relic from C version
static void s_RemovePeriod(string& title)
{
    if (NStr::EndsWith(title, '.')) {
        size_t last = title.length() - 1;
        if (last > 5) {
           if (title[last - 1] != '.'  ||  title[last - 2] != '.') {
               title.erase(last);
           }
        }
    }
}


void CReferenceItem::x_CleanData(void)
{
    // title
    ExpandTildes(m_Title, eTilde_space);
    NStr::TruncateSpacesInPlace(m_Title);
    StripSpaces(m_Title);   // internal spaces
    ConvertQuotes(m_Title);
    s_RemovePeriod(m_Title);
    // remark
    ConvertQuotes(m_Remark);
    ExpandTildes(m_Remark, eTilde_newline);
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
            if (!NStr::EndsWith(l.back(), '.')) {
                AddPeriod(l.back());
            }
        }

        // Poly_a
        if ( m_Pubdesc->IsSetPoly_a()  &&  m_Pubdesc->GetPoly_a() ) {
            l.push_back("Polyadenylate residues occurring in the figure were omitted from the sequence.");
        }

        // Maploc
        if ( m_Pubdesc->IsSetMaploc()  &&  !m_Pubdesc->GetMaploc().empty()) {
            l.push_back("Map location: " + m_Pubdesc->GetMaploc());
            if (!NStr::EndsWith(l.back(), '.')) {
                AddPeriod(l.back());
            }
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

    if (!l.empty()) {
        m_Remark = NStr::Join(l, "\n");
    }
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
    if ( std.IsSetSeason() ) {
        ++count;
    }

    return count;
}


static CDate::ECompare s_CompareDates(const CDate& d1, const CDate& d2)
{
    CDate::ECompare status = d1.Compare(d2);
    if (status != CDate::eCompare_unknown) {
        return status;
    }

    // NB: handle cases not handled by CDate::Compare(...)

    if (d1.IsStr()  &&  d2.IsStr()) {
        int diff = NStr::CompareNocase(d1.GetStr(), d2.GetStr());
        if (diff == 0) {
            return CDate::eCompare_same;
        } else {
            return (diff < 0) ? CDate::eCompare_before : CDate::eCompare_after;
        }
    }

    // arbitrary ordering (std before str)
    if (d1.Which() != d2.Which()) {
        return d1.IsStd() ? CDate::eCompare_before : CDate::eCompare_after;
    }

    _ASSERT(d1.IsStd()  &&  d2.IsStd());

    const CDate::TStd& std1 = d1.GetStd();
    const CDate::TStd& std2 = d2.GetStd();

    if (std1.IsSetMonth()  ||  std2.IsSetMonth()) {
        CDate_std::TMonth m1 = std1.IsSetMonth() ? std1.GetMonth() : 0;
        CDate_std::TMonth m2 = std2.IsSetMonth() ? std2.GetMonth() : 0;
        if (m1 < m2) {
            return CDate::eCompare_before;
        } else if (m1 > m2) {
            return CDate::eCompare_after;
        }
    }
    if (std1.IsSetDay()  ||  std2.IsSetDay()) {
        CDate_std::TDay day1 = std1.IsSetDay() ? std1.GetDay() : 0;
        CDate_std::TDay day2 = std2.IsSetDay() ? std2.GetDay() : 0;
        if (day1 < day2) {
            return CDate::eCompare_before;
        } else if (day1 > day2) {
            return CDate::eCompare_after;
        }
    }
    if (std1.IsSetSeason()  &&  !std2.IsSetSeason()) {
        return CDate::eCompare_after;
    } else if (!std1.IsSetSeason()  &&  std2.IsSetSeason()) {
        return CDate::eCompare_before;
    } else if (std1.IsSetSeason()  && std2.IsSetSeason()) {
        int diff = NStr::CompareNocase(std1.GetSeason(), std2.GetSeason());
        if (diff == 0) {
            return CDate::eCompare_same;
        } else {
            return (diff < 0) ? CDate::eCompare_before : CDate::eCompare_after;
        }
    }

    return CDate::eCompare_same;
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
    if (ref1->IsSetDate()  &&  !ref2->IsSetDate()) {
        return false;
    } else if (!ref1->IsSetDate()  &&  ref2->IsSetDate()) {
        return true;
    }
    
    if (ref1->IsSetDate()  &&  ref2->IsSetDate()) {
        CDate::ECompare status = s_CompareDates(ref1->GetDate(), ref2->GetDate());
        //const CDate& d1 = ref1->GetDate();
        //const CDate& d2 = ref2->GetDate();
        //CDate::ECompare status = d1.Compare(d2);
        //if (status == CDate::eCompare_unknown  &&  d1.IsStd()  &&  d2.IsStd()) {
        //    // one object is more specific than the other.
        //    size_t s1 = s_CountFields(d1);
        //    size_t s2 = s_CountFields(d2);
        //    return m_IsRefSeq ? s1 > s2 : s1 < s2;
        if (status != CDate::eCompare_same) {
            return m_IsRefSeq ? (status == CDate::eCompare_after) :
                                (status == CDate::eCompare_before);
        }
    }
    //}
    // after: dates are the same, or both missing.
    
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
        if ( ref1->IsJustUids()  &&  !ref2->IsJustUids() ) {
            return true;
        } else if ( !ref1->IsJustUids()  &&  ref2->IsJustUids() ) {
            return false;
        }
    }

    // put sites after pubs that refer to all or a range of bases
    if (ref1->GetReftype() != ref2->GetReftype()) {
        return ref1->GetReftype() < ref2->GetReftype();
    }

    // put pub descriptors before features, sort features by location
    const CSeq_feat* f1 = dynamic_cast<const CSeq_feat*>(ref1->GetObject());
    const CSeq_feat* f2 = dynamic_cast<const CSeq_feat*>(ref2->GetObject());
    if (f1 == NULL  &&  f2 != NULL) {
        return true;
    } else if (f1 != NULL  &&  f2 == NULL) {
        return false;
    } else if (f1 != NULL  &&  f2 != NULL) {
        CSeq_loc::TRange r1 = f1->GetLocation().GetTotalRange();
        CSeq_loc::TRange r2 = f2->GetLocation().GetTotalRange();
        if (r1 < r2) {
            return true;
        } else if (r2 < r1) {
            return false;
        }
    }

    // next use AUTHOR string
    string auth1, auth2;
    if (ref1->IsSetAuthors()) {
        CReferenceItem::FormatAuthors(ref1->GetAuthors(), auth1);
    }
    if (ref2->IsSetAuthors()) {
        CReferenceItem::FormatAuthors(ref2->GetAuthors(), auth2);
    }
    int comp = NStr::CompareNocase(auth1, auth2);
    if ( comp != 0 ) {
        return comp < 0;
    }

    // use unique label string to determine sort order
    const string& uniquestr1 = ref1->GetUniqueStr();
    const string& uniquestr2 = ref2->GetUniqueStr();
    if (!NStr::IsBlank(uniquestr1)  &&  !NStr::IsBlank(uniquestr2)) {
        comp = NStr::CompareNocase(uniquestr1, uniquestr2);
        if ( comp != 0 ) {
            return comp < 0;
        }
    }

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
* Revision 1.31  2005/04/27 17:13:47  shomrat
* Fixed reference comparison
*
* Revision 1.30  2005/03/29 18:18:09  shomrat
* Use
*
* Revision 1.29  2005/02/09 14:47:16  shomrat
* Set date for patent
*
* Revision 1.28  2005/02/07 15:01:06  shomrat
* Support for submissions
*
* Revision 1.27  2005/01/12 16:45:12  shomrat
* Code refactoring, moved journal formatting to format classes
*
* Revision 1.26  2004/11/19 15:14:43  shomrat
* Replace SeqLocMerge with new Seq_loc_Merge
*
* Revision 1.25  2004/11/18 21:27:40  grichenk
* Removed default value for scope argument in seq-loc related functions.
*
* Revision 1.24  2004/11/15 20:11:09  shomrat
* Handle electronic publications
*
* Revision 1.23  2004/10/18 18:57:25  shomrat
* Fix page numbers; Handle electronic publications
*
* Revision 1.22  2004/10/05 18:06:28  shomrat
* in place TruncateSpaces ->TruncateSpacesInPlace
*
* Revision 1.21  2004/10/05 15:47:57  shomrat
* Fixed reference formatting
*
* Revision 1.20  2004/08/30 13:43:36  shomrat
* fixed reference sorting
*
* Revision 1.19  2004/08/19 16:38:21  shomrat
* Fixed patent format
*
* Revision 1.18  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
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
