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
* Authors:  Aaron Ucko, Wratko Hlavina
*
* File Description:
*   Reader for GFF (including GTF) files.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/readers/gff_reader.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/ncbiutil.hpp>

#include <serial/iterator.hpp>

#include <objects/general/Date.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/readfeat.hpp>

#include <algorithm>
#include <ctype.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)




CRef<CSeq_entry> CGFFReader::Read(CNcbiIstream& in, TFlags flags)
{
    x_Reset();
    m_Flags  = flags;
    m_Stream = &in;

    string line;
    while (x_GetNextLine(line)) {
        if (NStr::StartsWith(line, "##")) {
            x_ParseStructuredComment(line);
        } else if (NStr::StartsWith(line, "#")) {
            // regular comment; ignore
        } else {
            CRef<SRecord> record = x_ParseFeatureInterval(line);
            if (record) {
                string id = x_FeatureID(*record);
                if (id.empty()) {
                    x_PlaceFeature(*x_ParseRecord(*record), *record);
                } else {
                    CRef<SRecord>& match = m_DelayedFeats[id];
                    if (match) {
                        x_MergeRecords(*match, *record);
                    } else {
                        match.Reset(record);
                    }
                }
            }
        }
    }

    ITERATE (TDelayedFeats, it, m_DelayedFeats) {
        x_PlaceFeature(*x_ParseRecord(*it->second), *it->second);
    }

    CRef<CSeq_entry> tse(m_TSE); // need to save before resetting.
    x_Reset();
    return tse;
}


void CGFFReader::x_Warn(const string& message, unsigned int line)
{
    if (line) {
        ERR_POST(Warning << message << " [GFF input, line " << line << ']');
    } else {
        ERR_POST(Warning << message << " [GFF input]");
    }
}


void CGFFReader::x_Reset(void)
{
    m_TSE.Reset(new CSeq_entry);
    m_SeqNameCache.clear();
    m_SeqCache.clear();
    m_DelayedFeats.clear();
    m_DefMol.erase();
    m_LineNumber = 0;
}


bool CGFFReader::x_GetNextLine(string& line)
{
    ++m_LineNumber;
    return NcbiGetlineEOL(*m_Stream, line)  ||  !line.empty();
}


void CGFFReader::x_ParseStructuredComment(const string& line)
{
    vector<string> v;
    NStr::Tokenize(line, "# \t", v, NStr::eMergeDelims);
    if (v[0] == "date"  &&  v.size() > 1) {
        x_ParseDateComment(v[1]);
    } else if (v[0] == "type"  &&  v.size() > 1) {
        x_ParseTypeComment(v[1], v.size() > 2 ? v[2] : kEmptyStr);
    }
    // etc.
}


void CGFFReader::x_ParseDateComment(const string& date)
{
    try {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetUpdate_date().SetToTime(CTime(date, "Y-M-D"),
                                         CDate::ePrecision_day);
        m_TSE->SetSet().SetDescr().Set().push_back(desc);
    } catch (CTimeException& e) {
        x_Warn(string("Bad ISO date: ") + e.what(), x_GetLineNumber());
    }
}


void CGFFReader::x_ParseTypeComment(const string& moltype,
                                    const string& seqname)
{
    if (seqname.empty()) {
        m_DefMol = moltype;
    } else {
        // automatically adds to m_TSE if new
        x_ResolveID(*x_ResolveSeqName(seqname), moltype);
    }
}


CRef<CGFFReader::SRecord>
CGFFReader::x_ParseFeatureInterval(const string& line)
{
    vector<string> v;
    bool           misdelimited = false;
    NStr::Tokenize(line, "\t", v, NStr::eNoMergeDelims);
    if (v.size() < 8) {
        NStr::Tokenize(line, " \t", v, NStr::eMergeDelims);
        if (v.size() < 8) {
            x_Warn("Skipping line due to insufficient fields",
                   x_GetLineNumber());
            return null;
        } else {
            x_Warn("Bad delimiters (should use tabs)", x_GetLineNumber());
            misdelimited = true;
        }
    } else {
        // XXX - warn about extra fields (if any), but only if they're
        // not comments
        v.resize(9);
    }

    CRef<SRecord> record(x_NewRecord());
    string        accession;
    TSeqPos       from = 0, to = numeric_limits<TSeqPos>::max();
    ENa_strand    strand = eNa_strand_unknown;
    accession      = v[0];
    record->source = v[1];
    record->key    = v[2];

    try {
        from = NStr::StringToUInt(v[3]) - 1;
    } catch (std::exception& e) {
        x_Warn(string("Bad FROM position: ") + e.what(), x_GetLineNumber());
    }

    try {
        to = NStr::StringToUInt(v[4]) - 1;
    } catch (std::exception& e) {
        x_Warn(string("Bad TO position: ") + e.what(), x_GetLineNumber());
    }

    record->score = v[5];

    if (v[6] == "+") {
        strand = eNa_strand_plus;
    } else if (v[6] == "-") {
        strand = eNa_strand_minus;
    } else if (v[6] != ".") {
        x_Warn("Bad strand " + v[6] + " (should be [+-.])", x_GetLineNumber());
    }

    if (v[7] == "0"  ||  v[7] == "1"  ||  v[7] == "2") {
        record->frame = v[7][0] - '0';
    } else if (v[7] == ".") {
        record->frame = -1;
    } else {
        x_Warn("Bad frame " + v[7] + " (should be [012.])", x_GetLineNumber());
        record->frame = -1;
    }

    {{
        SRecord::SSubLoc subloc;
        subloc.accession = accession;
        subloc.strand    = strand;
        subloc.ranges.insert(TSeqRange(from, to));

        record->loc.push_back(subloc);
    }}
   
    string         attr_last_value;
    vector<string> attr_values;
    char           quote_char = 0;
    for (SIZE_TYPE i = 8;  i < v.size();  ++i) {
        string s = v[i] + ' ';
        if ( !misdelimited  &&  i > 8) {
            SIZE_TYPE pos = s.find_first_not_of(" ");
            if (pos != NPOS) {
                if (s[pos] == '#') {
                    break;
                } else {
                    x_Warn("Extra non-comment fields", x_GetLineNumber());
                }
            } else {
                continue; // just spaces anyway...
            }
        }
        SIZE_TYPE pos = 0;
        while (pos < s.size()) {
            SIZE_TYPE pos2;
            if (quote_char) { // must be inside a value
                pos2 = s.find_first_of(" \'\"\\", pos);
                _ASSERT(pos2 != NPOS); // due to trailing space
                if (s[pos2] == quote_char) {
                    if (attr_values.empty()) {
                        x_Warn("quoted attribute tag " + attr_last_value,
                               x_GetLineNumber());
                    }
                    quote_char = 0;
                    attr_last_value += s.substr(pos, pos2 - pos);
                    try {
                        attr_values.push_back(NStr::ParseEscapes
                                              (attr_last_value));
                    } catch (CStringException& e) {
                        attr_values.push_back(attr_last_value);
                        x_Warn(e.what() + (" in value of " + attr_values[0]),
                               x_GetLineNumber());
                    }
                    attr_last_value.erase();
                } else if (s[pos2] == '\\') {
                    _VERIFY(++pos2 != s.size());
                    attr_last_value += s.substr(pos, pos2 + 1 - pos);
                } else {
                    attr_last_value += s.substr(pos, pos2 + 1 - pos);
                }
            } else {
                pos2 = s.find_first_of(" #;\"", pos); // also look for \'?
                _ASSERT(pos2 != NPOS); // due to trailing space
                if (pos != pos2) {
                    // grab and place the preceding token
                    attr_last_value += s.substr(pos, pos2 - pos);
                    attr_values.push_back(attr_last_value);
                    attr_last_value.erase();
                }

                switch (s[pos2]) {
                case ' ':
                    break;

                case '#':
                    i = v.size();
                    break;

                case ';':
                    if (attr_values.empty()) {
                        x_Warn("null attribute", x_GetLineNumber());
                    } else {
                        x_AddAttribute(*record, attr_values);
                        attr_values.clear();
                    }
                    break;

                // NB: we don't currently search for single quotes.
                case '\"': case '\'':
                    quote_char = s[pos2];
                    break;

                default:
                    _TROUBLE;
                }
            }
            pos = pos2 + 1;
        }
    }

    if ( !attr_values.empty() ) {
        x_Warn("unterminated attribute " + attr_values[0], x_GetLineNumber());
        x_AddAttribute(*record, attr_values);
    }

    return record;
}


CRef<CSeq_feat> CGFFReader::x_ParseRecord(const SRecord& record)
{
    CRef<CSeq_feat> feat(CFeature_table_reader::CreateSeqFeat
                         (record.key, *x_ResolveLoc(record.loc),
                          CFeature_table_reader::fKeepBadKey));
    if (record.frame >= 0  &&  feat->GetData().IsCdregion()) {
        feat->SetData().SetCdregion().SetFrame
            (static_cast<CCdregion::EFrame>(record.frame + 1));
    }
    ITERATE (SRecord::TAttrs, it, record.attrs) {
        string tag = it->front();
        string value;
        switch (it->size()) {
        case 1:
            break;
        case 2:
            value = (*it)[1];
            break;
        default:
            x_Warn("Ignoring extra fields in value of " + tag, record.line_no);
            value = (*it)[1];
            break;
        }
        if (x_GetFlags() & fGBQuals) {
            if ( !(x_GetFlags() & fNoGTF) ) { // translate
                if (tag == "transcript_id") {
                    //continue;
                } else if (tag == "gene_id") {
                    tag = "gene";
                    SIZE_TYPE colon = value.find(':');
                    if (colon != NPOS) {
                        value.erase(0, colon + 1);
                    }
                } else if (tag == "exon_number") {
                    tag = "number";
                }
            }
            CFeature_table_reader::AddFeatQual
                (feat, tag, value, CFeature_table_reader::fKeepBadKey);
        } else { // don't attempt to parse, just treat as imported
            CRef<CGb_qual> qual(new CGb_qual);
            qual->SetQual(tag);
            qual->SetVal(value);
            feat->SetQual().push_back(qual);
        }
    }
    return feat;
}


CRef<CSeq_loc> CGFFReader::x_ResolveLoc(const SRecord::TLoc& loc)
{
    CRef<CSeq_loc> seqloc(new CSeq_loc);
    ITERATE (SRecord::TLoc, it, loc) {
        CRef<CSeq_id> id = x_ResolveSeqName(it->accession);
        ITERATE (set<TSeqRange>, range, it->ranges) {
            CRef<CSeq_loc> segment(new CSeq_loc);
            if (range->GetLength() == 1) {
                CSeq_point& pnt = segment->SetPnt();
                pnt.SetId   (*id);
                pnt.SetPoint(range->GetFrom());
                if (it->strand != eNa_strand_unknown) {
                    pnt.SetStrand(it->strand);
                }
            } else {
                CSeq_interval& si = segment->SetInt();
                si.SetId  (*id);
                si.SetFrom(range->GetFrom());
                si.SetTo  (range->GetTo());
                if (it->strand != eNa_strand_unknown) {
                    si.SetStrand(it->strand);
                }
            }
            if (IsReverse(it->strand)) {
                seqloc->SetMix().Set().push_front(segment);
            } else {
                seqloc->SetMix().Set().push_back(segment);
            }
        }
    }

    if (seqloc->GetMix().Get().size() == 1) {
        return seqloc->SetMix().Set().front();
    } else {
        return seqloc;
    }
}


void CGFFReader::x_AddAttribute(SRecord& record, vector<string>& attr)
{
    if (x_GetFlags() & fGBQuals) {
        if (attr[0] == "gbkey"  &&  attr.size() == 2) {
            record.key = attr[1];
            return;
        }
    }
    record.attrs.insert(attr);
}


string CGFFReader::x_FeatureID(const SRecord& record)
{
    if (x_GetFlags() & fNoGTF) {
        return kEmptyStr;
    }

    static const vector<string> sc_GeneId(1, "gene_id");
    SRecord::TAttrs::const_iterator gene_it
        = record.attrs.lower_bound(sc_GeneId);

    static const vector<string> sc_TranscriptId(1, "transcript_id");
    SRecord::TAttrs::const_iterator transcript_it
        = record.attrs.lower_bound(sc_TranscriptId);

    static const vector<string> sc_ProteinId(1, "protein_id");
    SRecord::TAttrs::const_iterator protein_it
        = record.attrs.lower_bound(sc_ProteinId);

    // concatenate our IDs from above, if found
    string id;
    if (gene_it != record.attrs.end()  &&
        gene_it->front() == "gene_id") {
        id += (*gene_it)[1];
    }

    if (transcript_it != record.attrs.end()  &&
        transcript_it->front() == "transcript_id") {
        if ( !id.empty() ) {
            id += ' ';
        }
        id += (*transcript_it)[1];
    }

    if (protein_it != record.attrs.end()  &&
        protein_it->front() == "protein_id") {
        if ( !id.empty() ) {
            id += ' ';
        }
        id += (*protein_it)[1];
    }

    // look for db xrefs
    static const vector<string> sc_Dbxref(1, "db_xref");
    SRecord::TAttrs::const_iterator dbxref_it
        = record.attrs.lower_bound(sc_Dbxref);
    for ( ; dbxref_it != record.attrs.end()  &&
            dbxref_it->front() == "db_xref";  ++dbxref_it) {
        if ( !id.empty() ) {
            id += ' ';
        }
        id += (*dbxref_it)[1];
    }

    if ( id.empty() ) {
        return id;
    }

    if (record.key == "start_codon" ||  record.key == "stop_codon") {
        //id += " " + record.key;
        id += "CDS";
    } else if (record.key == "CDS"
               ||  NStr::FindNoCase(record.key, "rna") != NPOS) {
        //id += " " + record.key;
        id += record.key;
    } else { // probably an intron, exon, or single site
        return kEmptyStr;
    }
    _TRACE("id = " << id);
    return id;
}


void CGFFReader::x_MergeRecords(SRecord& dest, const SRecord& src)
{
    // XXX - perform sanity checks and warn on mismatch

    bool merge_overlaps = false;
    if ((src.key == "start_codon"  ||  src.key == "stop_codon")  &&
        dest.key == "CDS") {
        // start_codon and stop_codon features should be contained inside of
        // existing CDS locations
        merge_overlaps = true;
    }

    ITERATE (SRecord::TLoc, slit, src.loc) {
        bool merged = false;
        NON_CONST_ITERATE (SRecord::TLoc, dlit, dest.loc) {
            if (slit->accession != dlit->accession) {
                if (dest.loc.size() == 1) {
                    x_Warn("Multi-accession feature", src.line_no);
                }
                continue;
            } else if (slit->strand != dlit->strand) {
                if (dest.loc.size() == 1) {
                    x_Warn("Multi-orientation feature", src.line_no);
                }
                continue;
            } else {
                if (merge_overlaps) {
                    ITERATE (set<TSeqRange>, src_iter, slit->ranges) {
                        TSeqRange range(*src_iter);
                        set<TSeqRange>::iterator dst_iter =
                            dlit->ranges.begin();
                        for ( ;  dst_iter != dlit->ranges.end();  ) {
                            if (dst_iter->IntersectingWith(range)) {
                                range += *dst_iter;
                                _TRACE("merging overlapping ranges: "
                                       << range.GetFrom() << " - "
                                       << range.GetTo() << " <-> "
                                       << dst_iter->GetFrom() << " - "
                                       << dst_iter->GetTo());
                                dlit->ranges.erase(dst_iter++);
                            } else {
                                ++dst_iter;
                            }
                        }
                        dlit->ranges.insert(range);
                    }
                } else {
                    ITERATE (set<TSeqRange>, set_iter, slit->ranges) {
                        dlit->ranges.insert(*set_iter);
                    }
                }
                merged = true;
                break;
            }
        }
        if ( !merged ) {
            dest.loc.push_back(*slit);
        }
    }

    if (src.key != dest.key) {
        if (dest.key == "CDS"  &&  NStr::EndsWith(src.key, "_codon")
            &&  !(x_GetFlags() & fNoGTF) ) {
            // ok
        } else if (src.key == "CDS" &&  NStr::EndsWith(dest.key, "_codon")
            &&  !(x_GetFlags() & fNoGTF) ) {
            dest.key == "CDS";
        } else {
            x_Warn("Merging features with different keys: " + dest.key
                   + " != " + src.key, src.line_no);
        }
    }

    x_MergeAttributes(dest, src);
}


void CGFFReader::x_MergeAttributes(SRecord& dest, const SRecord& src)
{
    SRecord::TAttrs::iterator dait     = dest.attrs.begin();
    SRecord::TAttrs::iterator dait_end = dest.attrs.end();
    SRecord::TAttrs::iterator dait_tag = dait_end;
    ITERATE (SRecord::TAttrs, sait, src.attrs) {
        const string& tag = sait->front();
        while (dait != dait_end  &&  dait->front() < tag) {
            ++dait;
        }
        if (dait->front() == tag) {
            if (dait_tag == dait_end  ||  dait_tag->front() != tag) {
                dait_tag = dait;
            }
            while (dait != dait_end  &&  *dait < *sait) {
                ++dait;
            }
        }
        if (dait != dait_end  &&  *dait == *sait) {
            continue; // identical
        } else if ( !(x_GetFlags() & fNoGTF)  &&  tag == "exon_number") {
            if (dait_tag != dait_end) {
                while (dait != dait_end  &&  dait->front() == tag) {
                    ++dait;
                }
                dest.attrs.erase(dait_tag, dait);
            }
        } else {
            dest.attrs.insert(dait, *sait);
        }
    }
}


void CGFFReader::x_PlaceFeature(CSeq_feat& feat, const SRecord&)
{
    CRef<CBioseq> seq;
    if ( !feat.IsSetProduct() ) {
        for (CTypeConstIterator<CSeq_id> it(feat.GetLocation());  it;  ++it) {
            CRef<CBioseq> seq2 = x_ResolveID(*it, kEmptyStr);
            if ( !seq ) {
                seq.Reset(seq2);
            } else if ( seq2.NotEmpty()  &&  seq != seq2) {
                seq.Reset();
                BREAK(it);
            }
        }
    }

    CBioseq::TAnnot& annots
        = seq ? seq->SetAnnot() : m_TSE->SetSet().SetAnnot();
    NON_CONST_ITERATE (CBioseq::TAnnot, it, annots) {
        if ((*it)->GetData().IsFtable()) {
            (*it)->SetData().SetFtable().push_back(CRef<CSeq_feat>(&feat));
            return;
        }
    }
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable().push_back(CRef<CSeq_feat>(&feat));
    annots.push_back(annot);
}


CRef<CSeq_id> CGFFReader::x_ResolveSeqName(const string& name)
{
    CRef<CSeq_id>& id = m_SeqNameCache[name];
    if ( !id ) {
        id.Reset(x_ResolveNewSeqName(name));
    }
    if ( !id ) {
        x_Warn("x_ResolveNewSeqName returned null for " + name);
        id.Reset(new CSeq_id(CSeq_id::e_Local, name, name));
    }
    return id;
}


CRef<CSeq_id> CGFFReader::x_ResolveNewSeqName(const string& name)
{
    return CRef<CSeq_id>(new CSeq_id(name));
}


CRef<CBioseq> CGFFReader::x_ResolveID(const CSeq_id& id, const string& mol)
{
    CRef<CBioseq>& seq = m_SeqCache[CConstRef<CSeq_id>(&id)];
    if ( !seq ) {
        seq.Reset(x_ResolveNewID(id, mol));
        // Derived versions of x_ResolveNewID may legimately return null
        // results....
        if (seq) {
            x_PlaceSeq(*seq);
            ITERATE (CBioseq::TId, it, seq->GetId()) {
                m_SeqCache.insert(make_pair(CConstRef<CSeq_id>(*it), seq));
            }
        }
    }
    return seq;
}


CRef<CBioseq> CGFFReader::x_ResolveNewID(const CSeq_id& id, const string& mol0)
{
    CRef<CBioseq> seq(new CBioseq);
    CRef<CSeq_id> id_copy(new CSeq_id);

    id_copy->Assign(id);
    seq->SetId().push_back(id_copy);
    seq->SetInst().SetRepr(CSeq_inst::eRepr_virtual);

    const string& mol = mol0.empty() ? m_DefMol : mol0;
    if (mol.empty()  ||  mol == "dna") {
        seq->SetInst().SetMol(CSeq_inst::eMol_dna);
    } else if (mol == "rna")  {
        seq->SetInst().SetMol(CSeq_inst::eMol_rna);
    } else if (mol == "protein")  {
        seq->SetInst().SetMol(CSeq_inst::eMol_aa);
    } else {
        x_Warn("unrecognized sequence type " + mol + "; assuming DNA");
        seq->SetInst().SetMol(CSeq_inst::eMol_dna);
    }

    return seq;
}


void CGFFReader::x_PlaceSeq(CBioseq& seq)
{
    bool found = false;
    for (CTypeConstIterator<CBioseq> it(*m_TSE);  it;  ++it) {
        if (&*it == &seq) {
            found = true;
            BREAK(it);
        }
    }
    if ( !found ) {
        CRef<CSeq_entry> se(new CSeq_entry);
        se->SetSeq(seq);
        m_TSE->SetSet().SetSeq_set().push_back(se);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2004/05/21 21:42:55  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/05/08 12:13:24  dicuccio
* Switched away from using CRangeCollection<> for locations, as this lead to
* inadvertent merging of correctly overlapping exons
*
* Revision 1.4  2004/01/06 17:02:40  dicuccio
* (From Aaron Ucko): Fixed ordering of intervals in a seq-loc-mix on the negative
* strand
*
* Revision 1.3  2003/12/31 12:48:29  dicuccio
* Fixed interpretation of positions - GFF/GTF is 1-based, ASN.1 is 0-based
*
* Revision 1.2  2003/12/04 00:58:24  ucko
* Fix for WorkShop's context-insensitive make_pair.
*
* Revision 1.1  2003/12/03 20:56:36  ucko
* Initial commit.
*
*
* ===========================================================================
*/
