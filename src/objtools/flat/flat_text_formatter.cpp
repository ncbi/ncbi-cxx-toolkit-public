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
*   new (early 2003) flat-file generator -- base class for traditional
*   line-oriented formats (GenBank, EMBL, DDBJ but not GBSeq)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_ddbj_formatter.hpp>
#include <objtools/flat/flat_embl_formatter.hpp>
#include <objtools/flat/flat_ncbi_formatter.hpp>
#include <objtools/flat/flat_items.hpp>

#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Some of the code here may need to be split out to account for
// GenBank/DDBJ differences.

CFlatTextOStream::CFlatTextOStream(CNcbiOstream& stream, bool trace_topics)
    : m_Stream(stream)
{
    if (trace_topics) {
        m_TraceStream.reset(CObjectOStream::Open(eSerial_AsnText, stream));
    }
}


void CFlatTextOStream::AddParagraph(const list<string>&  lines,
                                    const IFlatItem*     item,
                                    const CSerialObject* topic)
{
    if (topic  &&  m_TraceStream.get()) {
        m_TraceStream->Write(topic, topic->GetThisTypeInfo());
        m_TraceStream->FlushBuffer();
    }
    ITERATE (list<string>, it, lines) {
        m_Stream << *it << '\n';
    }
    m_Stream << flush;
}


CFlatTextFormatter* CFlatTextFormatter::New(IFlatTextOStream& stream,
                                            CScope& scope,
                                            IFlatFormatter::EMode mode,
                                            IFlatFormatter::EDatabase db,
                                            IFlatFormatter::EStyle style,
                                            IFlatFormatter::TFlags flags)
{
    switch (db) {
    case eDB_DDBJ:
        return new CFlatDDBJFormatter(stream, scope, mode, style, flags);
    case eDB_EMBL:
        return new CFlatEMBLFormatter(stream, scope, mode, style, flags);
    case eDB_NCBI:
        return new CFlatNCBIFormatter(stream, scope, mode, style, flags);
    default:
        return 0; // shouldn't happen
    }
}


void CFlatTextFormatter::BeginSequence(CFlatContext& context)
{
    IFlatFormatter::BeginSequence(context);
    m_Stream->NewSequence();
}


void CFlatTextFormatter::FormatHead(const CFlatHead& head)
{
    list<string> l;

    {{
        CNcbiOstrstream locus_line;
        string tmp;
        locus_line << Pad(head.GetLocus(), tmp, 16) << ' ' << setw(11)
                   << m_Context->GetLength() << ' ' << m_Context->GetUnits();
        if (m_Context->IsProt()) {
            locus_line << string(12, ' ');
        } else {
            locus_line << ' ';
            switch (head.GetStrandedness()) {
            case CSeq_inst::eStrand_ss:    locus_line << "ss-"; break;
            case CSeq_inst::eStrand_ds:    locus_line << "ds-"; break;
            case CSeq_inst::eStrand_mixed: locus_line << "ms-"; break;
            default:                       locus_line << "   "; break;
            }
            locus_line << Pad(head.GetMolString(), tmp, 6) << "  ";
        }
        locus_line << (head.GetTopology() == CSeq_inst::eTopology_circular
                       ? "circular " : "linear   ")
                   << Upcase(head.GetDivision()) << ' ';
        tmp.erase();
        FormatDate(head.GetUpdateDate(), tmp);
        locus_line << tmp;
        Wrap(l, "LOCUS", CNcbiOstrstreamToString(locus_line));
        m_Stream->AddParagraph(l, &head);
        l.clear();
    }}

    // XXX - quote HTML if needed
    Wrap(l, "DEFINITION", head.GetDefinition());
    // hunt down appropriate descriptor, if any?
    m_Stream->AddParagraph(l, &head);
    l.clear();

    {{
        string acc = m_Context->GetPrimaryID().GetSeqIdString(false);
        ITERATE (list<string>, it, head.GetSecondaryIDs()) {
            acc += ' ' + *it;
        }
        Wrap(l, "ACCESSION", acc);
        Wrap(l, "VERSION",
             m_Context->GetAccession()
             + "  GI:" + NStr::IntToString(m_Context->GetGI()));
        m_Stream->AddParagraph(l, &head, &m_Context->GetPrimaryID());
    }}

    if ( !head.GetDBSource().empty() ) {
        l.clear();
        string tag = "DBSOURCE";
        ITERATE (list<string>, it, head.GetDBSource()) {
            Wrap(l, tag, *it);
            tag.erase();
        }
        if ( !l.empty() ) {
            m_Stream->AddParagraph(l, &head, head.GetProteinBlock());
        }
    }
}


void CFlatTextFormatter::FormatKeywords(const CFlatKeywords& keys)
{
    list<string> l, kw;
    ITERATE (list<string>, it, keys.GetKeywords()) {
        kw.push_back(*it + (&*it == &keys.GetKeywords().back() ? '.' : ';'));
    }
    if (kw.empty()) {
        kw.push_back(".");
    }
    string tag;
    NStr::WrapList(kw, m_Stream->GetWidth(), " ", l, 0, m_Indent,
                   Pad("KEYWORDS", tag, ePara));
    m_Stream->AddParagraph(l, &keys);
}


void CFlatTextFormatter::FormatSegment(const CFlatSegment& seg)
{
    list<string> l;
    Wrap(l, "SEGMENT",
         NStr::IntToString(seg.GetNum()) + " of "
         + NStr::IntToString(seg.GetCount()));
    m_Stream->AddParagraph(l, &seg);
}


void CFlatTextFormatter::FormatSource(const CFlatSource& source)
{
    list<string> l;
    {{
        string name = source.GetFormalName();
        if ( !source.GetCommonName().empty() ) {
            name += " (" + source.GetCommonName() + ')';
        }
        Wrap(l, "SOURCE", name);
    }}
    if (DoHTML()  &&  source.GetTaxID()) {
        Wrap(l, "ORGANISM",
             "<a href=\"" + source.GetTaxonomyURL() + "\">"
             + source.GetFormalName() + "</a>",
             eSubp);
    } else {
        Wrap(l, "ORGANISM", source.GetFormalName(), eSubp);
    }
    Wrap(l, kEmptyStr, source.GetLineage() + '.');
    m_Stream->AddParagraph(l, &source, &source.GetDescriptor());
}


void CFlatTextFormatter::FormatReference(const CFlatReference& ref)
{
    list<string> l;
    Wrap(l, "REFERENCE",
         NStr::IntToString(ref.GetSerial()) + ref.GetRange(*m_Context));
    
    {{
        string authors;
        ITERATE (list<string>, it, ref.GetAuthors()) {
            if (it != ref.GetAuthors().begin()) {
                authors += (&*it == &ref.GetAuthors().back()) ? " and " : ", ";
            }
            authors += *it;
        }
        Wrap(l, "AUTHORS", authors, eSubp);
    }}
    Wrap(l, "CONSRTM", ref.GetConsortium(), eSubp);

    {{
        string title, journal;
        ref.GetTitles(title, journal, *m_Context);
        Wrap(l, "TITLE",   title,   eSubp);
        Wrap(l, "JOURNAL", journal, eSubp);
    }}

    ITERATE (set<int>, it, ref.GetMUIDs()) {
        if (DoHTML()) {
            Wrap(l, "MEDLINE",
                 "<a href=\"" + ref.GetMedlineURL(*it) + "\">"
                 + NStr::IntToString(*it) + "</a>",
                 eSubp);
        } else {
            Wrap(l, "MEDLINE", NStr::IntToString(*it), eSubp);
        }
    }
    ITERATE (set<int>, it, ref.GetPMIDs()) {
        if (DoHTML()) {
            Wrap(l, " PUBMED",
                 "<a href=\"" + ref.GetPubMedURL(*it) + "\">"
                 + NStr::IntToString(*it) + "</a>",
                 eSubp);
        } else {
            Wrap(l, " PUBMED", NStr::IntToString(*it), eSubp);
        }
    }

    Wrap(l, "REMARK", ref.GetRemark(), eSubp);
    
    m_Stream->AddParagraph(l, &ref, &ref.GetPubdesc());
}


void CFlatTextFormatter::FormatComment(const CFlatComment& comment)
{
    if (comment.GetComment().empty()) {
        return;
    }
    string comment2 = ExpandTildes(comment.GetComment(), eTilde_newline);
    if ( !NStr::EndsWith(comment2, ".") ) {
        comment2 += '.';
    }
    list<string> l;
    Wrap(l, "COMMENT", comment2);
    m_Stream->AddParagraph(l, &comment);
}


void CFlatTextFormatter::FormatPrimary(const CFlatPrimary& primary)
{
    list<string> l;
    Wrap(l, "PRIMARY", primary.GetHeader());
    ITERATE (CFlatPrimary::TPieces, it, primary.GetPieces()) {
        string s;        
        Wrap(l, kEmptyStr, it->Format(s));
    }
    m_Stream->AddParagraph
        (l, &primary,
         &m_Context->GetHandle().GetBioseqCore()->GetInst().GetHist());
}


void CFlatTextFormatter::FormatFeatHeader(const CFlatFeatHeader& fh)
{
    list<string> l;
    Wrap(l, "FEATURES", "Location/Qualifiers", eFeatHead);
    m_Stream->AddParagraph(l, &fh);
}


void CFlatTextFormatter::FormatFeature(const IFlattishFeature& f)
{
    const CFlatFeature& feat = *f.Format();
    list<string>        l;
    Wrap(l, feat.GetKey(), feat.GetLoc().GetString(), eFeat);
    ITERATE (vector<CRef<CFlatQual> >, it, feat.GetQuals()) {
        string qual = '/' + (*it)->GetName(), value = (*it)->GetValue();
        switch ((*it)->GetStyle()) {
        case CFlatQual::eEmpty:                    value.erase();  break;
        case CFlatQual::eQuoted:   qual += "=\"";  value += '"';   break;
        case CFlatQual::eUnquoted: qual += '=';                    break;
        }
        // Call NStr::Wrap directly to avoid unwanted line breaks right
        // before the start of the value (in /translation, e.g.)
        NStr::Wrap(value, m_Stream->GetWidth(), l,
                   DoHTML() ? NStr::fWrap_HTMLPre : 0, m_FeatIndent,
                   m_FeatIndent + qual);
    }
    m_Stream->AddParagraph(l, &f, &feat.GetFeat());
}


void CFlatTextFormatter::FormatDataHeader(const CFlatDataHeader& dh)
{
    list<string> l;
    if ( !m_Context->IsProt() ) {
        TSeqPos a, c, g, t, other;
        dh.GetCounts(a, c, g, t, other);
        CNcbiOstrstream oss;
        oss << ' ' << setw(6) << a << " a " << setw(6) << c << " c "
                   << setw(6) << g << " g " << setw(6) << t << " t";
        if (other) {
            oss << ' ' << setw(6) << other << " other";
        }
        Wrap(l, "BASE COUNT", CNcbiOstrstreamToString(oss));
    }
    l.push_back("ORIGIN");
    m_Stream->AddParagraph(l, &dh);
}


void CFlatTextFormatter::FormatData(const CFlatData& data)
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
        for (TSeqPos i = 0;  i < l;  ++i) {
            if (i % 60 == 0) {
                if (i > 0) {
                    oss << '\n';
                }
                oss << setw(9) << pos + i + 1;
            }
            if (i % 10 == 0) {
                oss << ' ';
            }
            oss.put(tolower(buf[i]));
        }
        NStr::Split(CNcbiOstrstreamToString(oss), "\n", lines);
        // should use narrower location
        m_Stream->AddParagraph(lines, &data, &data.GetLoc());
    }
}


void CFlatTextFormatter::FormatContig(const CFlatContig& contig)
{
    list<string> l;
    Wrap(l, "CONTIG", CFlatLoc(contig.GetLoc(), *m_Context).GetString());
    m_Stream->AddParagraph
        (l, &contig,
         &m_Context->GetHandle().GetBioseqCore()->GetInst().GetExt());
}


void CFlatTextFormatter::FormatWGSRange(const CFlatWGSRange& range)
{
    list<string> l;
    if (DoHTML()) {
        Wrap(l, "WGS",
             "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?"
             "db=Nucleotide&cmd=Search&term=" + range.GetFirstID() + ':'
             + range.GetLastID() + "[ACCN]\">"
             + range.GetFirstID() + "-" + range.GetLastID() + "</a>");
    } else {
        Wrap(l, "WGS", range.GetFirstID() + "-" + range.GetLastID());
    }
    m_Stream->AddParagraph(l, &range, &range.GetUserObject());
}


void CFlatTextFormatter::FormatGenomeInfo(const CFlatGenomeInfo& g)
{
    list<string> l;
    string s = g.GetAccession();
    if ( !g.GetMoltype().empty()) {
        s += " (" + g.GetMoltype() + ')';
    }
    Wrap(l, "GENOME", s);
    m_Stream->AddParagraph(l, &g, &g.GetUserObject());
}


void CFlatTextFormatter::EndSequence(void)
{
    list<string> l;
    l.push_back("//");
    m_Stream->AddParagraph(l);
}


string& CFlatTextFormatter::Pad(const string& s, string& out,
                                EPadContext where)
{
    switch (where) {
    case ePara:      return Pad(s, out, 12);
    case eSubp:      return Pad(s, out, 12, string(2, ' '));
    case eFeatHead:  return Pad(s, out, 21);
    case eFeat:      return Pad(s, out, 21, string(5, ' '));
    default:         return out; // shouldn't happen, but some compilers whine
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2003/06/02 16:06:42  dicuccio
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
* Revision 1.4  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
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
