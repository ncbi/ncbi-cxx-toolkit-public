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

#include <objects/flat/flat_ddbj_formatter.hpp>
#include <objects/flat/flat_embl_formatter.hpp>
#include <objects/flat/flat_ncbi_formatter.hpp>
#include <objects/flat/flat_items.hpp>

#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/objmgr/seq_vector.hpp>

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


void CFlatTextOStream::AddParagraph(const list<string>& lines,
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


void CFlatTextFormatter::BeginSequence(SFlatContext& context)
{
    IFlatFormatter::BeginSequence(context);
    m_Stream->NewSequence();
}


void CFlatTextFormatter::FormatHead(const SFlatHead& head)
{
    list<string> l;

    {{
        CNcbiOstrstream locus_line;
        string tmp;
        locus_line << Pad(head.m_Locus, tmp, 16) << ' ' << setw(11)
                   << m_Context->m_Length << ' ' << m_Context->GetUnits();
        if (m_Context->m_IsProt) {
            locus_line << string(12, ' ');
        } else {
            locus_line << ' ';
            switch (head.m_Strandedness) {
            case CSeq_inst::eStrand_ss:    locus_line << "ss-"; break;
            case CSeq_inst::eStrand_ds:    locus_line << "ds-"; break;
            case CSeq_inst::eStrand_mixed: locus_line << "ms-"; break;
            default:                       locus_line << "   "; break;
            }
            locus_line << Pad(head.GetMolString(), tmp, 6) << "  ";
        }
        locus_line << (head.m_Topology == CSeq_inst::eTopology_circular
                       ? "circular " : "linear   ")
                   << Upcase(head.m_Division) << ' ';
        tmp.erase();
        FormatDate(*head.m_UpdateDate, tmp);
        locus_line << tmp;
        Wrap(l, "LOCUS", CNcbiOstrstreamToString(locus_line));
        m_Stream->AddParagraph(l);
        l.clear();
    }}

    // XXX - quote HTML if needed
    Wrap(l, "DEFINITION", head.m_Definition);
    // hunt down appropriate descriptor, if any?
    m_Stream->AddParagraph(l);
    l.clear();

    {{
        string acc = m_Context->m_PrimaryID->GetSeqIdString(false);
        ITERATE (list<string>, it, head.m_SecondaryIDs) {
            acc += ' ' + *it;
        }
        Wrap(l, "ACCESSION", acc);
        Wrap(l, "VERSION",
             m_Context->m_Accession
             + "  GI:" + NStr::IntToString(m_Context->m_GI));
        m_Stream->AddParagraph(l, m_Context->m_PrimaryID);
    }}

    if ( !head.m_DBSource.empty() ) {
        l.clear();
        string tag = "DBSOURCE";
        ITERATE (list<string>, it, head.m_DBSource) {
            Wrap(l, tag, *it);
            tag.erase();
        }
        if ( !l.empty() ) {
            m_Stream->AddParagraph(l, head.m_ProteinBlock);
        }
    }
}


void CFlatTextFormatter::FormatKeywords(const SFlatKeywords& keys)
{
    list<string> l, kw;
    ITERATE (list<string>, it, keys.m_Keywords) {
        kw.push_back(*it + (&*it == &keys.m_Keywords.back() ? '.' : ';'));
    }
    if (kw.empty()) {
        kw.push_back(".");
    }
    string tag;
    NStr::WrapList(kw, m_Stream->GetWidth(), " ", l, 0, m_Indent,
                   Pad("KEYWORDS", tag, ePara));
    m_Stream->AddParagraph(l);
}


void CFlatTextFormatter::FormatSegment(const SFlatSegment& seg)
{
    list<string> l;
    Wrap(l, "SEGMENT",
         NStr::IntToString(seg.m_Num) +" of "+ NStr::IntToString(seg.m_Count));
    m_Stream->AddParagraph(l);
}


void CFlatTextFormatter::FormatSource(const SFlatSource& source)
{
    list<string> l;
    {{
        string name = source.m_FormalName;
        if ( !source.m_CommonName.empty() ) {
            name += " (" + source.m_CommonName + ')';
        }
        Wrap(l, "SOURCE", name);
    }}
    if (DoHTML()  &&  source.m_TaxID) {
        Wrap(l, "ORGANISM",
             "<a href=\"" + source.GetTaxonomyURL() + "\">"
             + source.m_FormalName + "</a>",
             eSubp);
    } else {
        Wrap(l, "ORGANISM", source.m_FormalName, eSubp);
    }
    Wrap(l, kEmptyStr, source.m_Lineage + '.');
    m_Stream->AddParagraph(l, source.m_Descriptor);
}


void CFlatTextFormatter::FormatReference(const SFlatReference& ref)
{
    list<string> l;
    Wrap(l, "REFERENCE",
         NStr::IntToString(ref.m_Serial) + ref.GetRange(*m_Context));
    
    {{
        string authors;
        ITERATE (list<string>, it, ref.m_Authors) {
            if (it != ref.m_Authors.begin()) {
                authors += (&*it == &ref.m_Authors.back()) ? " and " : ", ";
            }
            authors += *it;
        }
        Wrap(l, "AUTHORS", authors, eSubp);
    }}
    Wrap(l, "CONSRTM", ref.m_Consortium, eSubp);

    {{
        string title, journal;
        ref.GetTitles(title, journal, *m_Context);
        Wrap(l, "TITLE",   title,   eSubp);
        Wrap(l, "JOURNAL", journal, eSubp);
    }}

    ITERATE (set<int>, it, ref.m_MUIDs) {
        if (DoHTML()) {
            Wrap(l, "MEDLINE",
                 "<a href=\"" + ref.GetMedlineURL(*it) + "\">"
                 + NStr::IntToString(*it) + "</a>",
                 eSubp);
        } else {
            Wrap(l, "MEDLINE", NStr::IntToString(*it), eSubp);
        }
    }
    ITERATE (set<int>, it, ref.m_PMIDs) {
        if (DoHTML()) {
            Wrap(l, " PUBMED",
                 "<a href=\"" + ref.GetPubMedURL(*it) + "\">"
                 + NStr::IntToString(*it) + "</a>",
                 eSubp);
        } else {
            Wrap(l, " PUBMED", NStr::IntToString(*it), eSubp);
        }
    }

    Wrap(l, "REMARK", ref.m_Remark, eSubp);
    
    m_Stream->AddParagraph(l, ref.m_Pubdesc);
}


void CFlatTextFormatter::FormatComment(const SFlatComment& comment)
{
    if (comment.m_Comment.empty()) {
        return;
    }
    string comment2 = ExpandTildes(comment.m_Comment, eTilde_newline);
    if ( !NStr::EndsWith(comment2, ".") ) {
        comment2 += '.';
    }
    list<string> l;
    Wrap(l, "COMMENT", comment2);
    m_Stream->AddParagraph(l);
}


void CFlatTextFormatter::FormatPrimary(const SFlatPrimary& primary)
{
    list<string> l;
    Wrap(l, "PRIMARY", primary.GetHeader());
    ITERATE (SFlatPrimary::TPieces, it, primary.m_Pieces) {
        string s;        
        Wrap(l, kEmptyStr, it->Format(s));
    }
    m_Stream->AddParagraph
        (l, &m_Context->m_Handle.GetBioseqCore()->GetInst().GetHist());
}


void CFlatTextFormatter::FormatFeatHeader(void)
{
    list<string> l;
    Wrap(l, "FEATURES", "Location/Qualifiers", eFeatHead);
    m_Stream->AddParagraph(l);
}


void CFlatTextFormatter::FormatFeature(const SFlatFeature& feat)
{
    list<string> l;
    Wrap(l, feat.m_Key, feat.m_Loc->m_String, eFeat);
    ITERATE (vector<CRef<SFlatQual> >, it, feat.m_Quals) {
        string qual = '/' + (*it)->m_Name, value = (*it)->m_Value;
        switch ((*it)->m_Style) {
        case SFlatQual::eEmpty:                    value.erase();  break;
        case SFlatQual::eQuoted:   qual += "=\"";  value += '"';   break;
        case SFlatQual::eUnquoted: qual += '=';                    break;
        }
        // Call NStr::Wrap directly to avoid unwanted line breaks right
        // before the start of the value (in /translation, e.g.)
        NStr::Wrap(value, m_Stream->GetWidth(), l,
                   DoHTML() ? NStr::fWrap_HTMLPre : 0, m_FeatIndent,
                   m_FeatIndent + qual);
    }
    m_Stream->AddParagraph(l, feat.m_Feat);
}


void CFlatTextFormatter::FormatDataHeader(const SFlatDataHeader& dh)
{
    list<string> l;
    if ( !dh.m_IsProt ) {
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
    m_Stream->AddParagraph(l);
}


void CFlatTextFormatter::FormatData(const SFlatData& data)
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
        m_Stream->AddParagraph(lines, data.m_Loc);
    }
}


void CFlatTextFormatter::FormatContig(const SFlatContig& contig)
{
    list<string> l;
    Wrap(l, "CONTIG", SFlatLoc(*contig.m_Loc, *m_Context).m_String);
    m_Stream->AddParagraph
        (l, &m_Context->m_Handle.GetBioseqCore()->GetInst().GetExt());
}


void CFlatTextFormatter::FormatWGSRange(const SFlatWGSRange& range)
{
    list<string> l;
    if (DoHTML()) {
        Wrap(l, "WGS",
             "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?"
             "db=Nucleotide&cmd=Search&term=" + range.m_First + ':'
             + range.m_Last + "[ACCN]\">" + range.m_First + "-" + range.m_Last
             + "</a>");
    } else {
        Wrap(l, "WGS", range.m_First + "-" + range.m_Last);
    }
    m_Stream->AddParagraph(l, range.m_UO);
}


void CFlatTextFormatter::FormatGenomeInfo(const SFlatGenomeInfo& g)
{
    list<string> l;
    string s = g.m_Accession;
    if ( !g.m_Moltype.empty()) {
        s += " (" + g.m_Moltype + ')';
    }
    Wrap(l, "GENOME", s);
    m_Stream->AddParagraph(l, g.m_UO);
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
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
