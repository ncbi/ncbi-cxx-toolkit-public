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

#include <objects/biblio/biblio__.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objmgr/util/sequence.hpp>

#include <algorithm>

#include <objtools/format/text_ostream.hpp>
#include <objtools/format/formatter.hpp>
#include <objtools/format/items/reference_item.hpp>
#include "context.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static void s_FormatAffil(const CAffil& affil, string& result)
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
#if 0 // not in C version...
        if (std.IsSetFax()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "Fax: " + std.GetFax();
        }
        if (std.IsSetPhone()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "Phone: " + std.GetPhone();
        }
        if (std.IsSetEmail()) {
            if (!result.empty()) {
                result += "; ";
            }
            result += "E-mail: " + std.GetEmail();
        }
#endif
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


CReferenceItem::CReferenceItem(const CSeqdesc&  desc, CFFContext& ctx) :
    CFlatItem(ctx), m_Category(eUnknown), m_Serial(0)
{
    _ASSERT(desc.IsPub());
    
    x_SetObject(desc.GetPub());
    m_Pubdesc.Reset(&(desc.GetPub()));

    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetWhole(*ctx.GetPrimaryId());
    m_Loc = loc;

    x_GatherInfo(ctx);
}


CReferenceItem::CReferenceItem(const CSeq_feat& feat, CFFContext& ctx) :
    CFlatItem(ctx)
{
    _ASSERT(feat.GetData().IsPub());

    m_Pubdesc.Reset(&(feat.GetData().GetPub()));
    m_Loc.Reset(&(feat.GetLocation()));
}


CReferenceItem::CReferenceItem
(const CPubdesc& pub,
 CFFContext& ctx,
 const CSeq_loc* loc)
    : CFlatItem(ctx), m_Pubdesc(&pub), m_Loc(loc), m_Category(eUnknown), m_Serial(0)
{
    x_SetObject(pub);

    x_GatherInfo(ctx);
}


void CReferenceItem::x_GatherInfo(CFFContext& ctx)
{
    if ( m_Pubdesc->CanGetPub() ) {
        ITERATE (CPub_equiv::Tdata, it, m_Pubdesc->GetPub().Get()) {
            x_Init(**it, ctx);
        }
    }
    x_CleanData();
    /*
    if (m_Date.NotEmpty()  &&  m_Date->IsStd()) {
        m_StdDate = &m_Date->GetStd();
    } else {
        // complain?
        m_StdDate = new CDate_std(CTime::eEmpty);
    }
    */
}


void CReferenceItem::Rearrange
(vector<CRef<CReferenceItem> >& refs,
 CFFContext& ctx)
{
    {{
        LessEqual l(false, ctx.IsRefSeq());
        sort(refs.begin(), refs.end(), l);
    }}

    {{
        // !!! remove duplicate references
    }}

    {{
        // !!! add submit reference
    }}

    {{
        // re-sort, take serial number into consideration.
        LessEqual l(true, ctx.IsRefSeq());
        sort(refs.begin(), refs.end(), l);
    }}
    
    // assign final serial numbers
    size_t size = refs.size();
    for ( size_t i = 0;  i < size;  ++i ) {
        refs[i]->m_Serial = i + 1;
    }
}

/*
string CReferenceItem::GetRange(CFFContext& ctx) const
{
    bool is_embl = ctx.GetFormat() == CFlatFileGenerator::eFormat_EMBL;
    if (is_embl  &&  m_Loc.Empty()) {
        return kEmptyStr;
    }
    CNcbiOstrstream oss;
    if ( !is_embl ) {
// !!!        oss << "  (" << ctx.GetUnits(false) << ' ';
    }
    string delim;
    const char* to = is_embl ? "-" : " to ";
    for (CSeq_loc_CI it(m_Loc ? *m_Loc : *ctx.GetLocation());  it;  ++it) {
        CSeq_loc_CI::TRange range = it.GetRange();
        if (it.IsWhole()) {
            range.SetTo(sequence::GetLength(it.GetSeq_id(),
                                            &ctx.GetScope())
                        - 1);
        }
        if (it.IsPoint()) {
            oss << range.GetFrom() + 1;
        } else {
            oss << delim << range.GetFrom() + 1 << to << range.GetTo() + 1;
        }
        delim = ", ";
    }
    if ( !is_embl ) {
        oss << ')';
    }
    return CNcbiOstrstreamToString(oss);
}
*/

/*
void CReferenceItem::GetTitles(string& title, string& journal,
                               const CFFContext& ctx) const
{
    // XXX - kludged for now (should move more logic from x_Init to here)
    title = m_Title;
    if ( NStr::EndsWith(title, ".") ) {
        title.erase(title.length() - 1);
    }
    if (m_Journal.empty()) {
        // complain?
        return;
    }

    if (ctx.GetFormat() == CFlatFileGenerator::eFormat_EMBL) {
        if (m_Category == CReferenceItem::eSubmission) {
            journal = "Submitted ";
            if (m_Date) {
                journal += '(';
                // !!!ctx.GetFormatter().FormatDate(*m_Date, journal);
                journal += ") ";
            }
            journal += "to the EMBL/GenBank/DDBJ databases.\n" + m_Journal;
        } else {
            journal = m_Journal;
            if ( !m_Volume.empty() ) {
                journal += " " + m_Volume;
            }
            if ( !m_Pages.empty()) {
                journal += ":" + m_Pages;
            }
            if (m_Date) {
                journal += '(';
                m_Date->GetDate(&journal, "%Y");
                journal += ").";
            }
        }
    } else { // NCBI or DDBJ
        if (m_Category == CReferenceItem::eSubmission) {
            journal = "Submitted ";
            if (m_Date) {
                journal += '(';
                // !!!ctx.GetFormatter().FormatDate(*m_Date, journal);
                journal += ") ";
            }
            journal += m_Journal;
        } else {
            journal = m_Journal;
            if ( !m_Volume.empty() ) {
                journal += " " + m_Volume;
            }
            if ( !m_Issue.empty() ) {
                journal += " (" + m_Issue + ')';
            }
            if ( !m_Pages.empty()) {
                journal += ", " + m_Pages;
            }
            if (m_Date) {
                journal += " (";
                m_Date->GetDate(&journal, "%Y");
                journal += ')';
            }
        }
    }
}
*/

void CReferenceItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatReference(*this, text_os);
}


/*
bool CReferenceItem::Matches(const CPub_set& ps) const
{
    // compare IDs
    CTypesConstIterator it;
    CType<CCit_gen>::AddTo(it);
    CType<CMedlineUID>::AddTo(it);
    CType<CMedline_entry>::AddTo(it);
    CType<CPub>::AddTo(it);
    CType<CPubMedId>::AddTo(it);

    for (it = ps;  it;  ++it) {
        if (CType<CCit_gen>::Match(it)) {
            const CCit_gen& gen = *CType<CCit_gen>::Get(it);
            if ((gen.IsSetMuid()  &&  HasMUID(gen.GetMuid()))
                / * ||  gen.GetSerial_number() == m_Serial * /) {
                return true;
            }
        } else if (CType<CMedlineUID>::Match(it)) {
            if (HasMUID(CType<CMedlineUID>::Get(it)->Get())) {
                return true;
            }
        } else if (CType<CMedline_entry>::Match(it)) {
            if (HasMUID(CType<CMedline_entry>::Get(it)->GetUid())) {
                return true;
            }
        } else if (CType<CPub>::Match(it)) {
            const CPub& pub = *CType<CPub>::Get(it);
            if (pub.IsMuid()  &&  HasMUID(pub.GetMuid())) {
                return true;
            }
        } else if (CType<CPubMedId>::Match(it)) {
            if (HasPMID(CType<CPubMedId>::Get(it)->Get())) {
                return true;
            }
        }
    }

    return false;
}
*/
 

void CReferenceItem::x_Init(const CPub& pub, CFFContext& ctx)
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
        }
        break;

    default:
        break;
    }
}


void CReferenceItem::x_Init(const CCit_gen& gen, CFFContext& ctx)
{
    if ( gen.CanGetCit() ) {
        const CCit_gen::TCit& cit = gen.GetCit();
        if ( NStr::StartsWith(cit, "BackBone id_pub", NStr::eNocase) ) {
            return;
        }

        if ( NStr::CompareNocase(cit, "unpublished") ) {
            m_Category = eUnpublished;
        }
    }

    if ( gen.CanGetAuthors() ) {
        x_AddAuthors(gen.GetAuthors());
    }

    if ( gen.CanGetMuid()  &&  m_MUID == 0 ) {
        m_MUID = gen.GetMuid();
    }
    
    if ( gen.CanGetVolume()  &&  m_Volume.empty() ) {
        m_Volume = gen.GetVolume();
    }

    if ( gen.CanGetIssue()  &&  m_Issue.empty() ) {
        m_Issue = gen.GetIssue();
    }

    if ( gen.CanGetPages()  &&  m_Pages.empty() ) {
        m_Pages = gen.GetPages();
        s_FixPages(m_Pages);
    }

    if ( gen.CanGetDate()  &&  !m_Date ) {
        m_Date = &gen.GetDate();
    }

    if ( gen.CanGetSerial_number()  &&  m_Serial == 0 ) {
        m_Serial = gen.GetSerial_number();
    }

    if ( m_Title.empty() ) {
        if ( gen.CanGetTitle() ) {
            m_Title = gen.GetTitle();
        } else if ( gen.CanGetCit()  &&  !gen.GetCit().empty() ) {
            static const string pattern = "Title=\"";
            const CCit_gen::TCit& cit = gen.GetCit();
            SIZE_TYPE pos = NStr::Find(cit, pattern);
            if ( pos != NPOS ) {
                pos += pattern.length();
                SIZE_TYPE end = NStr::Find(cit, "\"", pos);
                m_Title = cit.substr(pos, end - pos);
            }
        }
    }

    if ( gen.CanGetPmid()  &&  m_PMID == 0 ) {
        m_PMID = gen.GetPmid();
    }

    x_SetJournal(gen, ctx);
}


void CReferenceItem::x_Init(const CCit_sub& sub, CFFContext& ctx)
{
    m_Category = eSubmission;
    x_AddAuthors(sub.GetAuthors());
    if ( sub.GetAuthors().CanGetAffil() ) {
        s_FormatAffil(sub.GetAuthors().GetAffil(), m_Journal);
    }
    if ( sub.CanGetDate() ) {
        m_Date = &sub.GetDate();
    } else {
        // complain?
        if ( sub.CanGetImp() ) {
            m_Date = &sub.GetImp().GetDate();
        }
    }
}


void CReferenceItem::x_Init(const CMedline_entry& mle, CFFContext& ctx)
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


void CReferenceItem::x_Init(const CCit_art& art, CFFContext& ctx)
{
    if ( art.CanGetTitle()  &&  !art.GetTitle().Get().empty() ) {
        m_Title = art.GetTitle().GetTitle();
    }

    if ( art.CanGetAuthors() ) {
        x_AddAuthors(art.GetAuthors());
    }

    switch ( art.GetFrom().Which() ) {
    case CCit_art::C_From::e_Journal:
        x_Init(art.GetFrom().GetJournal(), ctx);
        break;
    case CCit_art::C_From::e_Book:
        x_Init(art.GetFrom().GetBook(), ctx);
        break;
    case CCit_art::C_From::e_Proc:
        x_Init(art.GetFrom().GetProc().GetBook(), ctx);
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


void CReferenceItem::x_Init(const CCit_jour& jour, CFFContext& ctx)
{
    x_SetJournal(jour.GetTitle(), ctx);
    x_AddImprint(jour.GetImp(), ctx);
}


void CReferenceItem::x_Init(const CCit_book& book, CFFContext& ctx)
{
    // !!!XXX -- should add more stuff to m_Journal (exactly what depends on
    // the format and whether this is for an article)
    if ( !book.GetTitle().Get().empty() ) {
        m_Journal = book.GetTitle().GetTitle();
    }
    if ( m_Authors.Empty() ) {
        x_AddAuthors(book.GetAuthors());
    }
    x_AddImprint(book.GetImp(), ctx);
}


void CReferenceItem::x_Init(const CCit_pat& pat, CFFContext& ctx)
{
    m_Title = pat.GetTitle();
    x_AddAuthors(pat.GetAuthors());
    int seqid = 0;
    ITERATE (CBioseq::TId, it, ctx.GetActiveBioseq().GetId()) {
        if ((*it)->IsPatent()) {
            seqid = (*it)->GetPatent().GetSeqid();
        }
    }
    // !!!XXX - same for EMBL?
    if (pat.CanGetNumber()) {
        m_Category = ePublished;
        m_Journal = "Patent: " + pat.GetCountry() + ' ' + pat.GetNumber()
            + '-' + pat.GetDoc_type() + ' ' + NStr::IntToString(seqid);
        if (pat.CanGetDate_issue()) {
            // !!!ctx.GetFormatter().FormatDate(pat.GetDate_issue(), m_Journal);
        }
        m_Journal += ';';
    } else {
        // !!!...
    }
}


void CReferenceItem::x_Init(const CCit_let& man, CFFContext& ctx)
{
    x_Init(man.GetCit(), ctx);
    if (man.CanGetType()  &&  man.GetType() == CCit_let::eType_thesis) {
        const CImprint& imp = man.GetCit().GetImp();
        m_Journal = "Thesis (";
        imp.GetDate().GetDate(&m_Journal, "%Y");
        m_Journal += ')';
        if (imp.CanGetPrepub()
            &&  imp.GetPrepub() == CImprint::ePrepub_in_press) {
            m_Journal += ", In press";
        }
        if (imp.CanGetPub()) {
            string affil;
            s_FormatAffil(imp.GetPub(), affil);
            replace(affil.begin(), affil.end(), '\"', '\'');
            m_Journal += ' ' + affil;
        }
    }
}


/*
static string& s_FixMedlineName(string& s)
{
    SIZE_TYPE space = s.find(' ');
    if (space) {
        s[space] = ',';
        for (SIZE_TYPE i = space + 1;  i < s.size();  ++i) {
            if (s[i] == ' ') {
                break;
            } else if (isupper(s[i])) {
                s.insert(++i, 1, ',');
            }
        }
    }
    return s;
}
*/


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

    /*
    typedef CAuth_list::C_Names TNames;
    const TNames& names = auth.GetNames();
    switch (names.Which()) {
    case TNames::e_Std:
        ITERATE (TNames::TStd, it, names.GetStd()) {
            const CPerson_id& name = (*it)->GetName();
            switch (name.Which()) {
            case CPerson_id::e_Name:
            {
                const CName_std& ns = name.GetName();
                string           s  = ns.GetLast();
                if (ns.IsSetInitials()) {
                    s += ',' + ns.GetInitials();
                } else if (ns.IsSetFirst()) {
                    s += ',' + ns.GetFirst()[0] + '.';
                    if (ns.IsSetMiddle()) {
                        s += ns.GetMiddle()[0] + '.';
                    }
                }
                // suffix?
                m_Authors.push_back(s);
                break;
            }
            case CPerson_id::e_Ml:
            {
                string s = name.GetMl();
                m_Authors.push_back(s_FixMedlineName(s));
                break;
            }
            case CPerson_id::e_Str:
                m_Authors.push_back(name.GetStr());
                break;
            case CPerson_id::e_Consortium:
                m_Consortium = name.GetConsortium();
                break;
            default:
                // complain?
                break;
            }
        }
        break;

    case TNames::e_Ml:
        ITERATE (TNames::TMl, it, names.GetMl()) {
            string s = *it;
            m_Authors.push_back(s_FixMedlineName(s));
        }
        break;

    case TNames::e_Str:
        m_Authors.insert(m_Authors.end(),
                         names.GetStr().begin(), names.GetStr().end());
        break;

    default:
        break;
    }
    */
}


void CReferenceItem::x_SetJournal(const CTitle& title, CFFContext& ctx)
{

    ITERATE (CTitle::Tdata, it, title.Get()) {
        if ( (*it)->IsIso_jta() ) {
            m_Journal = (*it)->GetIso_jta();
            return;
        }
    }
    if ( ctx.GetMode() == CFlatFileGenerator::eMode_Release ) {
        // complain
    } else if ( !title.Get().empty() ) {
        m_Journal = title.GetTitle();
    }
}


void CReferenceItem::x_SetJournal(const CCit_gen& gen, CFFContext& ctx)
{
    if ( !gen.CanGetJournal()  &&
          gen.CanGetCit()  &&  
          NStr::StartsWith(gen.GetCit(), "unpublished", NStr::eNocase) ) {
        const string& cit = gen.GetCit();

        if ( ctx.NoAffilOnUnpub() ) {
            if ( ctx.DropBadCitGens() ) {
                // !!! temporary ???
            }
            m_Journal = "Unpublished";
            return;
        }

        if ( gen.CanGetAuthors()  &&  gen.GetAuthors().CanGetAffil() ) {
            s_FormatAffil(gen.GetAuthors().GetAffil(), m_Journal);
            return;
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
            if ( !ctx.DropBadCitGens()  ||  !result.empty() ) {
                in_press = cit;
            } else {
                in_press = "Unpublished";
            }
        } else if ( NStr::StartsWith(cit, "Online Publication", NStr::eNocase)  ||
                    NStr::StartsWith(cit, "Published Only in DataBase", NStr::eNocase) ) {
            in_press = cit;
        } else if ( !ctx.DropBadCitGens()  &&  result.empty() ) {
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


void CReferenceItem::x_AddImprint(const CImprint& imp, CFFContext& ctx)
{
    if ( !m_Date ) {
        m_Date.Reset(&imp.GetDate());
    }
    if (imp.IsSetVolume()) {
        // add part-sup?
        m_Volume = imp.GetVolume();
    }
    if (imp.IsSetIssue()) {
        // add part-supi?
        m_Issue = imp.GetIssue();
    }
    if (imp.IsSetPages()) {
        m_Pages = imp.GetPages();
        s_FixPages(m_Pages);
    }
    if (imp.IsSetPrepub()  &&  imp.GetPrepub() != CImprint::ePrepub_in_press) {
        m_Category = eUnpublished;
    } else {
        m_Category = ePublished;
    }
}


string CReferenceItem::GetAuthString(const CAuth_list* alp)
{
    list<string> authors;

    if ( alp == 0 ) {
        authors.push_back(".");
    } else {
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

    CNcbiOstrstream auth_line;
    list<string>::const_iterator last = --(authors.end());

    string separator = kEmptyStr;
    bool first = true;
    ITERATE(list<string>, it, authors) {
        if ( it == last ) {
            separator = " and ";
        }
        auth_line << (first ? "" : separator) << *it;
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

    // sort by date, older publication comes first (except RefSeq).
    if ( (ref1->GetDate() != 0)  &&  (ref2->GetDate() != 0) ) {
        CDate::ECompare status = ref1->GetDate()->Compare(*ref2->GetDate());
        if ( (status == CDate::eCompare_before)  ||
             (status == CDate::eCompare_after) ) {
            return m_IsRefSeq ? (status == CDate::eCompare_after) :
                                (status == CDate::eCompare_before);
        }
    }

    // distinguish by uids
    if ( ref1->GetPMID() != 0  &&  ref2->GetPMID() != 0  &&
         ref1->GetPMID() != ref2->GetPMID() ) {
        return ref1->GetPMID() < ref2->GetPMID();
    }
    if ( ref1->GetMUID() != 0  &&  ref2->GetMUID() != 0  &&
         ref1->GetMUID() != ref2->GetMUID() ) {
        return ref1->GetMUID() < ref2->GetMUID();
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
* Revision 1.1  2003/12/17 20:24:12  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
