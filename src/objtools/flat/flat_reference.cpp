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
*
* File Description:
*   new (early 2003) flat-file generator -- bibliographic references
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_formatter.hpp>

#include <serial/iterator.hpp>

#include <objects/biblio/biblio__.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/pub__.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>

#include <objmgr/impl/annot_object.hpp>
#include <objmgr/util/sequence.hpp>

#include <algorithm>

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


CFlatReference::CFlatReference(const CPubdesc& pub, const CSeq_loc* loc,
                               const CFlatContext& ctx)
    : m_Pubdesc(&pub), m_Loc(loc), m_Category(eUnknown), m_Serial(0)
{
    ITERATE (CPub_equiv::Tdata, it, pub.GetPub().Get()) {
        x_Init(**it, ctx);
    }
    if (m_Date.NotEmpty()  &&  m_Date->IsStd()) {
        m_StdDate = &m_Date->GetStd();
    } else {
        // complain?
        m_StdDate = new CDate_std(CTime::eEmpty);
    }
}


CFlatReference::~CFlatReference()
{
}


void CFlatReference::Sort(vector<CRef<CFlatReference> >& v, CFlatContext& ctx)
{
    // XXX -- implement!

    // assign final serial numbers
    for (unsigned int i = 0;  i < v.size();  ++i) {
        v[i]->m_Serial = i + 1;
    }
}


string CFlatReference::GetRange(const CFlatContext& ctx) const
{
    bool is_embl = ctx.GetFormatter().GetDatabase() == IFlatFormatter::eDB_EMBL;
    if (is_embl  &&  m_Loc.Empty()) {
        return kEmptyStr;
    }
    CNcbiOstrstream oss;
    if ( !is_embl ) {
        oss << "  (" << ctx.GetUnits(false) << ' ';
    }
    string delim;
    const char* to = is_embl ? "-" : " to ";
    for (CSeq_loc_CI it(m_Loc ? *m_Loc : ctx.GetLocation());  it;  ++it) {
        CSeq_loc_CI::TRange range = it.GetRange();
        if (it.IsWhole()) {
            range.SetTo(sequence::GetLength(it.GetSeq_id(),
                                            &ctx.GetHandle().GetScope())
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


void CFlatReference::GetTitles(string& title, string& journal,
                               const CFlatContext& ctx) const
{
    // XXX - kludged for now (should move more logic from x_Init to here)
    title = m_Title;
    if (m_Journal.empty()) {
        // complain?
        return;
    }

    if (ctx.GetFormatter().GetDatabase() == IFlatFormatter::eDB_EMBL) {
        if (m_Category == CFlatReference::eSubmission) {
            journal = "Submitted ";
            if (m_Date) {
                journal += '(';
                ctx.GetFormatter().FormatDate(*m_Date, journal);
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
        if (m_Category == CFlatReference::eSubmission) {
            journal = "Submitted ";
            if (m_Date) {
                journal += '(';
                ctx.GetFormatter().FormatDate(*m_Date, journal);
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


// can't go in the header, as IFlatFormatter isn't yet known
void CFlatReference::Format(IFlatFormatter& f) const
{
    f.FormatReference(*this);
}


bool CFlatReference::Matches(const CPub_set& ps) const
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
                /* ||  gen.GetSerial_number() == m_Serial */) {
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


void CFlatReference::x_Init(const CPub& pub, const CFlatContext& ctx)
{
    switch (pub.Which()) {
    case CPub::e_Gen:      x_Init(pub.GetGen(), ctx);             break;
    case CPub::e_Sub:      x_Init(pub.GetSub(), ctx);             break;
    case CPub::e_Medline:  x_Init(pub.GetMedline(), ctx);         break;
    case CPub::e_Muid:     m_MUIDs.insert(pub.GetMuid());         break;
    case CPub::e_Article:  x_Init(pub.GetArticle(), ctx);         break;
    case CPub::e_Journal:  x_Init(pub.GetJournal(), ctx);         break;
    case CPub::e_Book:     x_Init(pub.GetBook(), ctx);            break;
    case CPub::e_Proc:     x_Init(pub.GetProc().GetBook(), ctx);  break;
    case CPub::e_Patent:   x_Init(pub.GetPatent(), ctx);          break;
    case CPub::e_Man:      x_Init(pub.GetMan(), ctx);             break;
    case CPub::e_Equiv:
        ITERATE (CPub_equiv::Tdata, it, pub.GetEquiv().Get()) {
            x_Init(**it, ctx);
        }
        break;
    case CPub::e_Pmid:     m_PMIDs.insert(pub.GetPmid());    break;
    default:               break;
    }
}


void CFlatReference::x_Init(const CCit_gen& gen, const CFlatContext& ctx)
{
    if (gen.IsSetCit()
        &&  !NStr::CompareNocase(gen.GetCit(), "unpublished") ) {
        m_Category = eUnpublished;
        m_Journal  = "Unpublished";
    }
    if (gen.IsSetAuthors()) {
        x_AddAuthors(gen.GetAuthors());
    }
    if (gen.IsSetMuid()) {
        m_MUIDs.insert(gen.GetMuid());
    }
    if (gen.IsSetJournal()) {
        x_SetJournal(gen.GetJournal(), ctx);
    }
    if (gen.IsSetVolume()  &&  m_Volume.empty()) {
        m_Volume = gen.GetVolume();
    }
    if (gen.IsSetIssue()  &&  m_Issue.empty()) {
        m_Issue = gen.GetIssue();
    }
    if (gen.IsSetPages()  &&  m_Pages.empty()) {
        m_Pages = gen.GetPages();
    }
    if (gen.IsSetDate()  &&  !m_Date) {
        m_Date = &gen.GetDate();
    }
    if (gen.IsSetSerial_number()  &&  !m_Serial) {
        m_Serial = gen.GetSerial_number();
    }
    if (gen.IsSetTitle()  &&  m_Title.empty()) {
        m_Title = gen.GetTitle();
    }
    if (gen.IsSetPmid()) {
        m_PMIDs.insert(gen.GetPmid());
    }
}


void CFlatReference::x_Init(const CCit_sub& sub, const CFlatContext& /* ctx */)
{
    m_Category = eSubmission;
    x_AddAuthors(sub.GetAuthors());
    if (sub.GetAuthors().IsSetAffil()) {
        s_FormatAffil(sub.GetAuthors().GetAffil(), m_Journal);
    }
    if (sub.IsSetDate()) {
        m_Date = &sub.GetDate();
    } else {
        // complain?
        if (sub.IsSetImp()) {
            m_Date = &sub.GetImp().GetDate();
        }
    }
}


void CFlatReference::x_Init(const CMedline_entry& mle, const CFlatContext& ctx)
{
    m_Category = ePublished;
    if (mle.IsSetUid()) {
        m_MUIDs.insert(mle.GetUid());
    }
    m_Date = &mle.GetEm();
    if (mle.IsSetPmid()) {
        m_PMIDs.insert(mle.GetPmid());
    }
    x_Init(mle.GetCit(), ctx);
}


void CFlatReference::x_Init(const CCit_art& art, const CFlatContext& ctx)
{
    if (art.IsSetTitle()  &&  !art.GetTitle().Get().empty() ) {
        m_Title = art.GetTitle().GetTitle();
    }
    if (art.IsSetAuthors()) {
        x_AddAuthors(art.GetAuthors());
    }
    switch (art.GetFrom().Which()) {
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
    if (art.IsSetIds()) {
        ITERATE (CArticleIdSet::Tdata, it, art.GetIds().Get()) {
            switch ((*it)->Which()) {
            case CArticleId::e_Pubmed:
                m_PMIDs.insert((*it)->GetPubmed());
                break;
            case CArticleId::e_Medline:
                m_MUIDs.insert((*it)->GetMedline());
                break;
            default:
                break;
            }
        }
    }
}


void CFlatReference::x_Init(const CCit_jour& jour, const CFlatContext& ctx)
{
    x_SetJournal(jour.GetTitle(), ctx);
    x_AddImprint(jour.GetImp(), ctx);
}


void CFlatReference::x_Init(const CCit_book& book, const CFlatContext& ctx)
{
    // XXX -- should add more stuff to m_Journal (exactly what depends on
    // the format and whether this is for an article)
    if ( !book.GetTitle().Get().empty() ) {
        m_Journal = book.GetTitle().GetTitle();
    }
    if (m_Authors.empty()) {
        x_AddAuthors(book.GetAuthors());
    }
    x_AddImprint(book.GetImp(), ctx);
}


void CFlatReference::x_Init(const CCit_pat& pat, const CFlatContext& ctx)
{
    m_Title = pat.GetTitle();
    x_AddAuthors(pat.GetAuthors());
    int seqid = 0;
    ITERATE (CBioseq::TId, it, ctx.GetHandle().GetBioseqCore()->GetId()) {
        if ((*it)->IsPatent()) {
            seqid = (*it)->GetPatent().GetSeqid();
        }
    }
    // XXX - same for EMBL?
    if (pat.IsSetNumber()) {
        m_Category = ePublished;
        m_Journal = "Patent: " + pat.GetCountry() + ' ' + pat.GetNumber()
            + '-' + pat.GetDoc_type() + ' ' + NStr::IntToString(seqid);
        if (pat.IsSetDate_issue()) {
            ctx.GetFormatter().FormatDate(pat.GetDate_issue(), m_Journal);
        }
        m_Journal += ';';
    } else {
        // ...
    }
}


void CFlatReference::x_Init(const CCit_let& man, const CFlatContext& ctx)
{
    x_Init(man.GetCit(), ctx);
    if (man.IsSetType()  &&  man.GetType() == CCit_let::eType_thesis) {
        const CImprint& imp = man.GetCit().GetImp();
        m_Journal = "Thesis (";
        imp.GetDate().GetDate(&m_Journal, "%Y");
        m_Journal += ')';
        if (imp.IsSetPrepub()
            &&  imp.GetPrepub() == CImprint::ePrepub_in_press) {
            m_Journal += ", In press";
        }
        if (imp.IsSetPub()) {
            string affil;
            s_FormatAffil(imp.GetPub(), affil);
            replace(affil.begin(), affil.end(), '\"', '\'');
            m_Journal += ' ' + affil;
        }
    }
}


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


void CFlatReference::x_AddAuthors(const CAuth_list& auth)
{
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
}


void CFlatReference::x_SetJournal(const CTitle& title, const CFlatContext& ctx)
{
    ITERATE (CTitle::Tdata, it, title.Get()) {
        if ((*it)->IsIso_jta()) {
            m_Journal = (*it)->GetIso_jta();
            return;
        }
    }
    if (ctx.GetFormatter().GetMode() == IFlatFormatter::eMode_Release) {
        // complain
    } else if ( !title.Get().empty() ) {
        m_Journal = title.GetTitle();
    }
}


void CFlatReference::x_AddImprint(const CImprint& imp, const CFlatContext& ctx)
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
        // Restore redundant leading digits of the second number if needed
        SIZE_TYPE digits1 = m_Pages.find_first_not_of("0123456789");
        if (digits1 != 0) {
            SIZE_TYPE hyphen = m_Pages.find('-', digits1);
            if (hyphen != NPOS) {
                SIZE_TYPE digits2 = m_Pages.find_first_not_of("0123456789",
                                                              hyphen + 1);
                digits2 -= hyphen + 1;
                if (digits2 < digits1) {
                    // lengths of the tail portions
                    SIZE_TYPE len1 = hyphen + digits2 - digits1;
                    SIZE_TYPE len2 = m_Pages.size() - hyphen - 1;
                    int x = NStr::strncasecmp(&m_Pages[digits1 - digits2],
                                              &m_Pages[hyphen + 1],
                                              min(len1, len2));
                    if (x > 0  ||  (x == 0  &&  len1 >= len2)) {
                        // complain?
                    } else {
                        m_Pages.insert(hyphen + 1, m_Pages, 0,
                                       digits1 - digits2);
                    }
                }
            }
        }
    }
    if (imp.IsSetPrepub()  &&  imp.GetPrepub() != CImprint::ePrepub_in_press) {
        m_Category = eUnpublished;
    } else {
        m_Category = ePublished;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.12  2005/03/31 13:11:33  dicuccio
* Added export specifiers.  Implemented dtor, added hidden copy ctor/alignment
* operator to satisfy requirements of predeclaration.
*
* Revision 1.11  2004/05/21 21:42:53  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.10  2003/12/02 19:22:17  ucko
* Properly detect unpublished references, and give them a pseudo-journal
* of Unpublished.
*
* Revision 1.9  2003/06/02 16:06:42  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.8  2003/04/24 16:15:58  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.7  2003/04/09 20:03:11  ucko
* Fix unsafe assumptions in CFlatReference::Matches.
*
* Revision 1.6  2003/03/28 17:46:21  dicuccio
* Added missing include for CAnnotObject_Info because MSVC gets confused
* otherwise....
*
* Revision 1.5  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.4  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.3  2003/03/10 22:06:04  ucko
* Explicitly call NotEmpty to avoid a bogus MSVC error.
* Fix a typo that interpreted some MUIDs as PMIDs.
*
* Revision 1.2  2003/03/10 20:18:16  ucko
* Fix broken call to string::insert (caught by Compaq's compiler).
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
