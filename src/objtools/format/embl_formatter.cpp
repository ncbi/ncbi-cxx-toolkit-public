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
*           
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/date_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/embl_formatter.hpp>
#include <objtools/format/context.hpp>
#include <objmgr/util/objutil.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// NB: For more complete documentation on the EMBL format see EMBL's user 
// manual (http://www.ebi.ac.uk/embl/Documentation/User_manual/usrman.html)


CEmblFormatter::CEmblFormatter(void) 
{
    SetIndent(string(5, ' '));
    //SetFeatIndent(string(21, ' '));

    string tmp;
    m_XX.push_back(Pad("XX", tmp, ePara));
}


///////////////////////////////////////////////////////////////////////////
//
// END SECTION

void CEmblFormatter::EndSection(const CEndSectionItem&, IFlatTextOStream& text_os)
{
    list<string> l;
    l.push_back("//");
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// ID (EMBL's locus line)
//

// General format:
//      ID   entryname  dataclass; molecule; division; sequencelength BP.
//
// Entryname: stable identifier.
// Dataclass: The second item on the ID line indicates the data class of the entry.
// Molecule Type: The third item on the line is the type of molecule as stored.
// Database division: This indicates to which division the entry belongs.
// Sequence length: The last item on the ID line is the length of the sequence.

void CEmblFormatter::FormatLocus
(const CLocusItem& locus, 
 IFlatTextOStream& text_os)
{
    static string embl_mol [14] = {
        "xxx", "DNA", "RNA", "RNA", "RNA", "RNA", "RNA",
        "RNA", "AA ", "DNA", "DNA", "RNA", "RNA", "RNA"
    };

    const CBioseqContext& ctx = *locus.GetContext();

    list<string> l;
    CNcbiOstrstream id_line;

    string hup = ctx.IsHup() ? " confidential" : " standard";

    string topology = (locus.GetTopology() == CSeq_inst::eTopology_circular) ?
                "circular" : kEmptyStr;
    const string& mol = ctx.Config().UseEmblMolType() ? 
        s_EmblMol[locus.GetBiomol()] : s_GenbankMol[locus.GetBiomol()];
            
    id_line.setf(IOS_BASE::left, IOS_BASE::adjustfield);
    id_line 
        << setw(9) << locus.GetName()
        << hup << "; "
        << topology << mol << "; "
        << locus.GetDivision() << "; "
        << locus.GetLength() << " BP.";

    Wrap(l, GetWidth(), "ID", CNcbiOstrstreamToString(id_line));
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// AC

void CEmblFormatter::FormatAccession
(const CAccessionItem& acc, 
 IFlatTextOStream& text_os)
{
    string acc_line = x_FormatAccession(acc, ';');

    x_AddXX(text_os);

    list<string> l;
    Wrap(l, "AC", acc_line);
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// SV

void CEmblFormatter::FormatVersion
(const CVersionItem& version,
 IFlatTextOStream& text_os)
{
    if ( version.Skip() ) {
        return;
    }

    x_AddXX(text_os);

    list<string> l;
    CNcbiOstrstream version_line;

    if ( version.GetGi() > ZERO_GI ) {
        version_line << "g" << version.GetGi();
    }

    Wrap(l, "SV", CNcbiOstrstreamToString(version_line));
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// DT

void CEmblFormatter::FormatDate
(const CDateItem& date,
 IFlatTextOStream& text_os)
{
    string date_str;
    list<string> l;

    x_AddXX(text_os);

    // Create Date
    const CDate* dp = date.GetCreateDate();
    if ( dp != 0 ) {
        DateToString(*dp, date_str);
    }
    
    if ( date_str.empty() ) {
        date_str = "01-JAN-1900";
    }
    Wrap(l, "DT", date_str);

    // Update Date
    dp = date.GetUpdateDate();
    if ( dp != 0 ) {
        date_str.erase();
        DateToString(*dp, date_str);
    }

    Wrap(l, "DT", date_str);
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// DE

void CEmblFormatter::FormatDefline
(const CDeflineItem& defline,
 IFlatTextOStream& text_os)
{
    if ( defline.Skip() ) {
        return;
    }

    x_AddXX(text_os);

    list<string> l;
    Wrap(l, "DE", defline.GetDefline());
    text_os.AddParagraph(l);
}

///////////////////////////////////////////////////////////////////////////
//
// KW

void CEmblFormatter::FormatKeywords
(const CKeywordsItem& keys,
 IFlatTextOStream& text_os)
{
    if ( keys.Skip() ) {
        return;
    }

    x_AddXX(text_os);

    list<string> l;
    x_GetKeywords(keys, "KW", l);
    text_os.AddParagraph(l);
}


///////////////////////////////////////////////////////////////////////////
//
// Source

// SOURCE + ORGANISM

void CEmblFormatter::FormatSource
(const CSourceItem& source,
 IFlatTextOStream& text_os)
{
    if ( source.Skip() ) {
        return;
    }

    list<string> l;
    x_OrganismSource(l, source);
    x_OrganisClassification(l, source);
    x_Organelle(l, source);
    text_os.AddParagraph(l); 
}


void CEmblFormatter::x_OrganismSource
(list<string>& l,
 const CSourceItem& source) const
{
    /*
    CNcbiOstrstream source_line;
    
    string prefix = source.IsUsingAnamorph() ? " (anamorph: " : " (";
    
    source_line << source.GetTaxname();
    if ( !source.GetCommon().empty() ) {
        source_line << prefix << source.GetCommon() << ")";
    }
    
    Wrap(l, GetWidth(), "SOURCE", CNcbiOstrstreamToString(source_line));
    */
}


void CEmblFormatter::x_OrganisClassification
(list<string>& l,
 const CSourceItem& source) const
{
    //Wrap(l, GetWidth(), "ORGANISM", source.GetTaxname(), eSubp);
    //Wrap(l, GetWidth(), kEmptyStr, source.GetLineage() + '.', eSubp);
}


void CEmblFormatter::x_Organelle
(list<string>& l,
 const CSourceItem& source) const
{
}


///////////////////////////////////////////////////////////////////////////
//
// REFERENCE

// The REFERENCE field consists of five parts: the keyword REFERENCE, and
// the subkeywords AUTHORS, TITLE (optional), JOURNAL, MEDLINE (optional),
// PUBMED (optional), and REMARK (optional).

void CEmblFormatter::FormatReference
(const CReferenceItem& ref,
 IFlatTextOStream& text_os)
{
    /*
    CFlatContext& ctx = const_cast<CFlatContext&>(ref.GetContext()); // !!!

    list<string> l;

    x_Reference(l, ref, ctx);
    x_Authors(l, ref, ctx);
    x_Consortium(l, ref, ctx);
    x_Title(l, ref, ctx);
    x_Journal(l, ref, ctx);
    x_Medline(l, ref, ctx);
    x_Pubmed(l, ref, ctx);
    x_Remark(l, ref, ctx);

    text_os.AddParagraph(l);
    */
}

/*
// The REFERENCE line contains the number of the particular reference and
// (in parentheses) the range of bases in the sequence entry reported in
// this citation.
void CEmblFormatter::x_Reference
(list<string>& l,
 const CReferenceItem& ref,
 CFlatContext& ctx)
{
    CNcbiOstrstream ref_line;

    // print serial number
    ref_line << ref.GetSerial() << (ref.GetSerial() < 10 ? "  " : " ");

    // print sites or range
    CPubdesc::TReftype reftype = ref.GetReftype();

    if ( reftype == CPubdesc::eReftype_sites  ||
         reftype == CPubdesc::eReftype_feats ) {
        ref_line << "(sites)";
    } else if ( reftype == CPubdesc::eReftype_no_target ) {
    } else {
        const CSeq_loc* loc = ref.GetLoc() != 0 ? ref.GetLoc() : ctx.GetLocation();
        x_FormatRefLocation(ref_line, *loc, " to ", "; ",
            ctx.IsProt(), ctx.GetScope());
    }
    Wrap(l, GetWidth(), "REFERENCE", CNcbiOstrstreamToString(ref_line));
}


void CEmblFormatter::x_Authors
(list<string>& l,
 const CReferenceItem& ref,
 CFlatContext& ctx) const
{
    Wrap(l, "AUTHORS", CReferenceItem::GetAuthString(ref.GetAuthors()), eSubp);
}


void CEmblFormatter::x_Consortium
(list<string>& l,
 const CReferenceItem& ref,
 CFlatContext& ctx) const
{
    Wrap(l, GetWidth(), "CONSRTM", ref.GetConsortium(), eSubp);
}


void CEmblFormatter::x_Title
(list<string>& l,
 const CReferenceItem& ref,
 CFlatContext& ctx) const
{
    // !!! kludge - fix it
    string title, journal;
    ref.GetTitles(title, journal, ctx);
    Wrap(l, "TITLE",   title,   eSubp);
}


void CEmblFormatter::x_Journal
(list<string>& l,
 const CReferenceItem& ref,
 CFlatContext& ctx) const
{
    // !!! kludge - fix it
    string title, journal;
    ref.GetTitles(title, journal, ctx);
    Wrap(l, "JOURNAL", journal, eSubp);
}


void CEmblFormatter::x_Medline
(list<string>& l,
 const CReferenceItem& ref,
 CFlatContext& ctx) const
{
    Wrap(l, GetWidth(), "MEDLINE", NStr::IntToString(ref.GetMUID()), eSubp);
}


void CEmblFormatter::x_Pubmed
(list<string>& l,
 const CReferenceItem& ref,
 CFlatContext& ctx) const
{
    Wrap(l, GetWidth(), " PUBMED", NStr::IntToString(ref.GetPMID()), eSubp);
}


void CEmblFormatter::x_Remark
(list<string>& l,
 const CReferenceItem& ref,
 CFlatContext& ctx) const
{
    Wrap(l, GetWidth(), "REMARK", ref.GetRemark(), eSubp);
}
*/

///////////////////////////////////////////////////////////////////////////
//
// COMMENT


void CEmblFormatter::FormatComment
(const CCommentItem& comment,
 IFlatTextOStream& text_os)
{
    /*
    list<string> l;

    if ( !comment.IsFirst() ) {
        Wrap(l, kEmptyStr, comment.GetComment(), eSubp);
    } else {
        Wrap(l, "COMMENT", comment.GetComment());
    }

    text_os.AddParagraph(l);
    */
}


///////////////////////////////////////////////////////////////////////////
//
// FEATURES

// Fetures Header

void CEmblFormatter::FormatFeatHeader
(const CFeatHeaderItem& fh,
 IFlatTextOStream& text_os)
{
    /*
    list<string> l;

    Wrap(l, "FEATURES", "Location/Qualifiers", eFeatHead);

    text_os.AddParagraph(l);
    */
}


void CEmblFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& text_os)
{ 
    /*
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
        NStr::Wrap(value, GetWidth(), l,
                   / *DoHTML() ? NStr::fWrap_HTMLPre : * /0, GetFeatIndent(),
                   GetFeatIndent() + qual);
    }
    text_os.AddParagraph(l);
    */
}


///////////////////////////////////////////////////////////////////////////
//
// BASE COUNT

void CEmblFormatter::FormatBasecount
(const CBaseCountItem& bc,
 IFlatTextOStream& text_os)
{
    /*
    list<string> l;

    CNcbiOstrstream bc_line;

    bc_line 
        << right << setw(7) << bc.GetA() << " a"
        << right << setw(7) << bc.GetC() << " c"
        << right << setw(7) << bc.GetG() << " g"
        << right << setw(7) << bc.GetT() << " t";
    if ( bc.GetOther() > 0 ) {
        bc_line << right << setw(7) << bc.GetOther() << " others";
    }
    Wrap(l, "BASE COUNT", CNcbiOstrstreamToString(bc_line));
    text_os.AddParagraph(l);
    */
}


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

void CEmblFormatter::FormatSequence
(const CSequenceItem& seq,
 IFlatTextOStream& text_os)
{
    /*
    list<string> l;
    CNcbiOstrstream seq_line;

    const CSeqVector& vec = seq.GetSequence();

    TSeqPos base_count = seq.GetFrom();
    CSeqVector::const_iterator iter = vec.begin();
    while ( iter ) {
        seq_line << setw(9) << right << base_count;
        for ( TSeqPos count = 0; count < 60  &&  iter; ++count, ++iter, ++base_count ) {
            if ( count % 10 == 0 ) {
                seq_line << ' ';
            }
            seq_line << (char)tolower((unsigned char)(*iter));
        }
        seq_line << '\n';
    }

    if ( seq.IsFirst() ) {
        l.push_back("ORIGIN      ");
    }
    NStr::Split(CNcbiOstrstreamToString(seq_line), "\n", l);
    text_os.AddParagraph(l);
    */
}


string& CEmblFormatter::Pad(const string& s, string& out,
                                EPadContext where) const
{
    switch (where) {
    case ePara:  case eSubp:  return x_Pad(s, out, 5);
    case eFeatHead:           return x_Pad(s, out, 21, "FH   ");
    case eFeat:               return x_Pad(s, out, 21, "FT   ");
    default:                  return out;
    }
}


void CEmblFormatter::x_AddXX(IFlatTextOStream& text_os) const
{
    text_os.AddParagraph(m_XX);
}


END_SCOPE(objects)
END_NCBI_SCOPE
