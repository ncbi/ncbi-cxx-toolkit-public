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
*   new (early 2003) flat-file generator -- EMBL(PEPT) output class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_embl_formatter.hpp>
#include <objtools/flat/flat_items.hpp>

#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objmgr/seq_vector.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


inline
list<string>& CFlatEMBLFormatter::Wrap(list<string>& l, const string& tag,
                                       const string& body, EPadContext where)
{
    NStr::TWrapFlags flags = DoHTML() ? NStr::fWrap_HTMLPre : 0;
    string tag2;
    Pad(tag, tag2, where);
    NStr::Wrap(body, m_Stream->GetWidth(), l, flags, tag2);
    return l;
}


inline
void CFlatEMBLFormatter::x_AddXX(void)
{
    if (m_XX.empty()) {
        string tmp;
        m_XX.push_back(Pad("XX", tmp, ePara));
    }
    m_Stream->AddParagraph(m_XX);
}


void CFlatEMBLFormatter::FormatHead(const CFlatHead& head)
{
    list<string> l;
    {{
        CNcbiOstrstream id_line;
        string          locus;
        TParent::Pad(head.GetLocus(), locus, 9);
        id_line << locus << " standard; ";
        if (head.GetTopology() == CSeq_inst::eTopology_circular) {
            id_line << "circular ";
        }
        id_line << head.GetMolString() << "; "
                << Upcase(head.GetDivision()) << "; "
                << m_Context->GetLength() << ' '
                << Upcase(m_Context->GetUnits()) << '.';
        Wrap(l, "ID", CNcbiOstrstreamToString(id_line));
        m_Stream->AddParagraph(l, &head);
        l.clear();
    }}
    x_AddXX();
    {{
        string acc = m_Context->GetPrimaryID().GetSeqIdString(false);
        ITERATE (list<string>, it, head.GetSecondaryIDs()) {
            acc += "; " + *it;
        }
        Wrap(l, "AC", acc + ';');
        l.push_back("XX   ");
        Wrap(l, "SV", m_Context->GetAccession());
        m_Stream->AddParagraph(l, &head, &m_Context->GetPrimaryID());
        l.clear();
    }}
    x_AddXX();
    {{
        string date;
        FormatDate(head.GetUpdateDate(), date);
        Wrap(l, "DT", date);
        m_Stream->AddParagraph(l, &head, &head.GetUpdateDate());
        l.clear();
        date.erase();
        FormatDate(head.GetCreateDate(), date);
        Wrap(l, "DT", date);
        m_Stream->AddParagraph(l, &head, &head.GetCreateDate());
        l.clear();
    }}
    x_AddXX();
    Wrap(l, "DE", head.GetDefinition());
    m_Stream->AddParagraph(l, &head);
    // DBSOURCE for EMBLPEPT?
}


void CFlatEMBLFormatter::FormatKeywords(const CFlatKeywords& keys)
{
    x_AddXX();
    list<string>   l, kw;
    vector<string> v;
    ITERATE (list<string>, it, keys.GetKeywords()) {
        v.push_back(*it);
    }
    sort(v.begin(), v.end());
    ITERATE (vector<string>, it, v) {
        kw.push_back(*it + (&*it == &v.back() ? '.' : ';'));
    }
    if (kw.empty()) {
        kw.push_back(".");
    }
    string tag;
    NStr::WrapList(kw, m_Stream->GetWidth(), " ", l, 0, Pad("KW", tag, ePara));
    m_Stream->AddParagraph(l, &keys);
}


void CFlatEMBLFormatter::FormatSegment(const CFlatSegment& seg)
{
    x_AddXX();
    list<string> l;
    // Done as comment (no corresponding line type)
    Wrap(l, "CC", "SEGMENT " + NStr::IntToString(seg.GetNum()) + " of "
         + NStr::IntToString(seg.GetCount()));
    m_Stream->AddParagraph(l, &seg);
}


void CFlatEMBLFormatter::FormatSource(const CFlatSource& source)
{
    x_AddXX();
    list<string> l;
    {{
        string name = source.GetFormalName();
        if ( !source.GetCommonName().empty() ) {
            name += " (" + source.GetCommonName() + ')';
        }
        Wrap(l, "OS", name);
    }}
    Wrap(l, "OC", source.GetLineage() + '.');
    m_Stream->AddParagraph(l, &source, &source.GetDescriptor());
    // XXX -- OG (Organelle)?
}


void CFlatEMBLFormatter::FormatReference(const CFlatReference& ref)
{
    x_AddXX();
    list<string> l;
    Wrap(l, "RN", '[' + NStr::IntToString(ref.GetSerial()) + ']');
    Wrap(l, "RC", ref.GetRemark(), eSubp);
    // Wrap(l, "RC", "CONSORTIUM: " + ref.GetConsortium(), eSubp);
    Wrap(l, "RP", ref.GetRange(*m_Context), eSubp);
    ITERATE (set<int>, it, ref.GetMUIDs()) {
        if (DoHTML()) {
            Wrap(l, "RX",
                 "MEDLINE; <a href=\"" + ref.GetMedlineURL(*it) + "\">"
                 + NStr::IntToString(*it) + "</a>.",
                 eSubp);
        } else {
            Wrap(l, "RX", "MEDLINE; " + NStr::IntToString(*it) + '.', eSubp);
        }
    }
    
    {{
        const list<string>& raw_authors = ref.GetAuthors();
        list<string> authors;
        ITERATE (list<string>, it, raw_authors) {
            authors.push_back(NStr::Replace(*it, ",", " ")
                              + (&*it == &raw_authors.back() ? ';' : ','));
        }
        string tag;
        NStr::WrapList(authors, m_Stream->GetWidth(), " ", l, 0,
                       Pad("RA", tag, ePara));
    }}

    {{
        string title, journal;
        ref.GetTitles(title, journal, *m_Context);
        Wrap(l, "RT", title + ';', eSubp);
        Wrap(l, "RL", journal,     eSubp);
    }}

    m_Stream->AddParagraph(l, &ref, &ref.GetPubdesc());
}


void CFlatEMBLFormatter::FormatComment(const CFlatComment& comment)
{
    if (comment.GetComment().empty()) {
        return;
    }
    x_AddXX();
    string comment2 = ExpandTildes(comment.GetComment(), eTilde_newline);
    if ( !NStr::EndsWith(comment2, ".") ) {
        comment2 += '.';
    }
    list<string> l;
    Wrap(l, "CC", comment2);
    m_Stream->AddParagraph(l, &comment);
}


void CFlatEMBLFormatter::FormatPrimary(const CFlatPrimary& primary)
{
    x_AddXX();
    list<string> l;
    Wrap(l, "AH", primary.GetHeader());
    ITERATE (CFlatPrimary::TPieces, it, primary.GetPieces()) {
        string s;        
        Wrap(l, "AS", it->Format(s));
    }
    m_Stream->AddParagraph
        (l, &primary,
         &m_Context->GetHandle().GetBioseqCore()->GetInst().GetHist());
}


void CFlatEMBLFormatter::FormatFeatHeader(const CFlatFeatHeader& fh)
{
    list<string> l;
    x_AddXX();
    Wrap(l, "Key", "Location/Qualifiers", eFeatHead);
    l.push_back("FH   ");
    m_Stream->AddParagraph(l, &fh);
}


void CFlatEMBLFormatter::FormatDataHeader(const CFlatDataHeader& dh)
{
    x_AddXX();
    list<string> l;
    CNcbiOstrstream oss;
    oss << "Sequence " << m_Context->GetLength() << ' '
        << Upcase(m_Context->GetUnits()) << ';';
    if ( !m_Context->IsProt() ) {
        TSeqPos a, c, g, t, other;
        dh.GetCounts(a, c, g, t, other);
        oss << ' ' << a << " A; " << c << " C; " << g << " G; " << t << " T; "
            << other << " other;";
    }
    Wrap(l, "SQ", CNcbiOstrstreamToString(oss));
    m_Stream->AddParagraph(l, &dh);
}


void CFlatEMBLFormatter::FormatData(const CFlatData& data)
{
    static const TSeqPos kChunkSize = 1200; // 20 lines

    string     buf;
    CSeqVector v = m_Context->GetHandle().GetSequenceView
        (data.GetLoc(), CBioseq_Handle::eViewConstructed,
         CBioseq_Handle::eCoding_Iupac);
    for (TSeqPos pos = 0;  pos < v.size();  pos += kChunkSize) {
        list<string>    lines;
        CNcbiOstrstream oss;
        TSeqPos l = min(kChunkSize, v.size() - pos);
        v.GetSeqData(pos, pos + l, buf);
        oss << "    ";
        for (TSeqPos i = 0;  i < l;  ++i) {
            if (i > 0  &&  i % 60 == 0) {
                oss << ' ' << setw(9) << pos + i << "\n    ";
            }
            if (i % 10 == 0) {
                oss << ' ';
            }
            oss.put(tolower(buf[i]));
        }
        if (l % 60) {
            oss << string((60 - (l % 60)) * 11 / 10 + 1, ' ')
                << setw(9) << pos + l;
        }
        NStr::Split(CNcbiOstrstreamToString(oss), "\n", lines);
        // should use narrower location
        m_Stream->AddParagraph(lines, &data, &data.GetLoc());
    }
}


void CFlatEMBLFormatter::FormatContig(const CFlatContig& contig)
{
    x_AddXX();
    list<string> l;
    Wrap(l, "CO", CFlatLoc(contig.GetLoc(), *m_Context).GetString());
    m_Stream->AddParagraph
        (l, &contig,
         &m_Context->GetHandle().GetBioseqCore()->GetInst().GetExt());
}


void CFlatEMBLFormatter::FormatWGSRange(const CFlatWGSRange& range)
{
    // Done as comment (no corresponding line type)
    x_AddXX();
    list<string> l;
    if (DoHTML()) {
        Wrap(l, "CC",
             "WGS: <a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?"
             "db=Nucleotide&cmd=Search&term=" + range.GetFirstID() + ':'
             + range.GetLastID() + "[ACCN]\">"
             + range.GetFirstID() + "-" + range.GetLastID() + "</a>");
    } else {
        Wrap(l, "CC", "WGS: " + range.GetFirstID() + "-" + range.GetLastID());
    }
    m_Stream->AddParagraph(l, &range, &range.GetUserObject());
}


void CFlatEMBLFormatter::FormatGenomeInfo(const CFlatGenomeInfo& g)
{
    // Done as comment (no corresponding line type)
    x_AddXX();
    list<string> l;
    string s = "GENOME: " + g.GetAccession();
    if ( !g.GetMoltype().empty()) {
        s += " (" + g.GetMoltype() + ')';
    }
    Wrap(l, "CC", s);
    m_Stream->AddParagraph(l, &g, &g.GetUserObject());
}


string& CFlatEMBLFormatter::Pad(const string& s, string& out,
                                EPadContext where)
{
    switch (where) {
    case ePara:  case eSubp:  return TParent::Pad(s, out, 5);
    case eFeatHead:           return TParent::Pad(s, out, 21, "FH   ");
    case eFeat:               return TParent::Pad(s, out, 21, "FT   ");
    default:                  return out;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2004/05/21 21:42:53  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.9  2003/10/11 19:29:02  ucko
* Made the XX paragraph a normal member to avoid trouble when statically
* linked into multiple plugins.  (The old code wasn't threadsafe anyway.)
*
* Revision 1.8  2003/06/02 16:06:42  dicuccio
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
* Revision 1.7  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.6  2003/03/31 19:52:06  ucko
* Fixed typo that made last commit ineffective.
*
* Revision 1.5  2003/03/31 16:25:14  ucko
* Kludge: move the static "XX" paragraph to file scope, as it otherwise
* becomes a common symbol on Darwin, preventing inclusion in shared libs.
*
* Revision 1.4  2003/03/29 04:14:23  ucko
* Move private inline methods from .hpp to .cpp.
*
* Revision 1.3  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
