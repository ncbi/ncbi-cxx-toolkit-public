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

#include <objects/flat/flat_embl_formatter.hpp>
#include <objects/flat/flat_items.hpp>

#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/objmgr/seq_vector.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CFlatEMBLFormatter::FormatHead(const SFlatHead& head)
{
    list<string> l;
    {{
        CNcbiOstrstream id_line;
        string          locus;
        TParent::Pad(head.m_Locus, locus, 9);
        id_line << locus << " standard; ";
        if (head.m_Topology == CSeq_inst::eTopology_circular) {
            id_line << "circular ";
        }
        id_line << head.GetMolString() << "; "
                << Upcase(head.m_Division) << "; "
                << m_Context->m_Length << ' ' << Upcase(m_Context->GetUnits())
                << '.';
        Wrap(l, "ID", CNcbiOstrstreamToString(id_line));
        m_Stream->AddParagraph(l);
        l.clear();
    }}
    x_AddXX();
    {{
        string acc = m_Context->m_PrimaryID->GetSeqIdString(false);
        ITERATE (list<string>, it, head.m_SecondaryIDs) {
            acc += "; " + *it;
        }
        Wrap(l, "AC", acc + ';');
        l.push_back("XX   ");
        Wrap(l, "SV", m_Context->m_Accession);
        m_Stream->AddParagraph(l, m_Context->m_PrimaryID);
        l.clear();
    }}
    x_AddXX();
    {{
        string date;
        FormatDate(*head.m_UpdateDate, date);
        Wrap(l, "DT", date);
        m_Stream->AddParagraph(l, head.m_UpdateDate);
        l.clear();
        date.erase();
        FormatDate(*head.m_CreateDate, date);
        Wrap(l, "DT", date);
        m_Stream->AddParagraph(l, head.m_CreateDate);
        l.clear();
    }}
    x_AddXX();
    Wrap(l, "DE", head.m_Definition);
    m_Stream->AddParagraph(l);
    // DBSOURCE for EMBLPEPT?
}


void CFlatEMBLFormatter::FormatKeywords(const SFlatKeywords& keys)
{
    x_AddXX();
    list<string>   l, kw;
    vector<string> v;
    ITERATE (list<string>, it, keys.m_Keywords) {
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
    m_Stream->AddParagraph(l);
}


void CFlatEMBLFormatter::FormatSegment(const SFlatSegment& seg)
{
    x_AddXX();
    list<string> l;
    // Done as comment (no corresponding line type)
    Wrap(l, "CC", "SEGMENT " + NStr::IntToString(seg.m_Num) + " of "
         + NStr::IntToString(seg.m_Count));
    m_Stream->AddParagraph(l);
}


void CFlatEMBLFormatter::FormatSource(const SFlatSource& source)
{
    x_AddXX();
    list<string> l;
    {{
        string name = source.m_FormalName;
        if ( !source.m_CommonName.empty() ) {
            name += " (" + source.m_CommonName + ')';
        }
        Wrap(l, "OS", name);
    }}
    Wrap(l, "OC", source.m_Lineage + '.');
    m_Stream->AddParagraph(l, source.m_Descriptor);
    // XXX -- OG (Organelle)?
}


void CFlatEMBLFormatter::FormatReference(const SFlatReference& ref)
{
    x_AddXX();
    list<string> l;
    Wrap(l, "RN", '[' + NStr::IntToString(ref.m_Serial) + ']');
    Wrap(l, "RC", ref.m_Remark, eSubp);
    // Wrap(l, "RC", "CONSORTIUM: " + ref.m_Consortium, eSubp);
    Wrap(l, "RP", ref.GetRange(*m_Context), eSubp);
    ITERATE (set<int>, it, ref.m_MUIDs) {
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
        list<string> authors;
        ITERATE (list<string>, it, ref.m_Authors) {
            authors.push_back(NStr::Replace(*it, ",", " ")
                              + (&*it == &ref.m_Authors.back() ? ';' : ','));
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

    m_Stream->AddParagraph(l, ref.m_Pubdesc);
}


void CFlatEMBLFormatter::FormatComment(const SFlatComment& comment)
{
    if (comment.m_Comment.empty()) {
        return;
    }
    x_AddXX();
    string comment2 = ExpandTildes(comment.m_Comment, eTilde_newline);
    if ( !NStr::EndsWith(comment2, ".") ) {
        comment2 += '.';
    }
    list<string> l;
    Wrap(l, "CC", comment2);
    m_Stream->AddParagraph(l);
}


void CFlatEMBLFormatter::FormatPrimary(const SFlatPrimary& primary)
{
    x_AddXX();
    list<string> l;
    Wrap(l, "AH", primary.GetHeader());
    ITERATE (SFlatPrimary::TPieces, it, primary.m_Pieces) {
        string s;        
        Wrap(l, "AS", it->Format(s));
    }
    m_Stream->AddParagraph
        (l, &m_Context->m_Handle.GetBioseqCore()->GetInst().GetHist());
}


void CFlatEMBLFormatter::FormatFeatHeader(void)
{
    list<string> l;
    x_AddXX();
    Wrap(l, "Key", "Location/Qualifiers", eFeatHead);
    l.push_back("FH   ");
    m_Stream->AddParagraph(l);
}


void CFlatEMBLFormatter::FormatDataHeader(const SFlatDataHeader& dh)
{
    x_AddXX();
    list<string> l;
    CNcbiOstrstream oss;
    oss << "Sequence " << m_Context->m_Length << ' '
        << Upcase(m_Context->GetUnits()) << ';';
    if ( !dh.m_IsProt ) {
        TSeqPos a, c, g, t, other;
        dh.GetCounts(a, c, g, t, other);
        oss << ' ' << a << " A; " << c << " C; " << g << " G; " << t << " T; "
            << other << " other;";
    }
    Wrap(l, "SQ", CNcbiOstrstreamToString(oss));
    m_Stream->AddParagraph(l);
}


void CFlatEMBLFormatter::FormatData(const SFlatData& data)
{
    static const TSeqPos kChunkSize = 1200; // 20 lines

    string     buf;
    CSeqVector v = m_Context->m_Handle.GetSequenceView
        (*data.m_Loc, CBioseq_Handle::eViewConstructed,
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
        m_Stream->AddParagraph(lines, data.m_Loc);
    }
}


void CFlatEMBLFormatter::FormatContig(const SFlatContig& contig)
{
    x_AddXX();
    list<string> l;
    Wrap(l, "CO", SFlatLoc(*contig.m_Loc, *m_Context).m_String);
    m_Stream->AddParagraph
        (l, &m_Context->m_Handle.GetBioseqCore()->GetInst().GetExt());
}


void CFlatEMBLFormatter::FormatWGSRange(const SFlatWGSRange& range)
{
    // Done as comment (no corresponding line type)
    x_AddXX();
    list<string> l;
    if (DoHTML()) {
        Wrap(l, "CC",
             "WGS: <a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?"
             "db=Nucleotide&cmd=Search&term=" + range.m_First + ':'
             + range.m_Last + "[ACCN]\">" + range.m_First + "-" + range.m_Last
             + "</a>");
    } else {
        Wrap(l, "CC", "WGS: " + range.m_First + "-" + range.m_Last);
    }
    m_Stream->AddParagraph(l, range.m_UO);
}


void CFlatEMBLFormatter::FormatGenomeInfo(const SFlatGenomeInfo& g)
{
    // Done as comment (no corresponding line type)
    x_AddXX();
    list<string> l;
    string s = "GENOME: " + g.m_Accession;
    if ( !g.m_Moltype.empty()) {
        s += " (" + g.m_Moltype + ')';
    }
    Wrap(l, "CC", s);
    m_Stream->AddParagraph(l, g.m_UO);
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
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
