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

#include <corelib/ncbistr_util.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/stream_utils.hpp>
#include <serial/iterator.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/readers/cigar.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>
#include <ctype.h>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_GFF

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static CRef<CFeat_id>
s_StringToFeatId( const string& str )
{
    CRef<CObject_id> objid( new CObject_id );
    objid->SetStr( str );
    CRef<CFeat_id> featid( new CFeat_id );
    featid->SetLocal( *objid );
    return featid;
}    

static string& s_URLDecode(const CTempString& s, string& out) {
    SIZE_TYPE pos = 0;
    out.erase();
    out.reserve(s.size());
    while (pos < s.size()) {
        SIZE_TYPE pos2 = s.find_first_of("%" /* "+" */, pos);
        out += s.substr(pos, pos2 - pos);
        if (pos2 == NPOS) {
            break;
        } else if (s[pos2] == '+') { // disabled -- often used literally
            out += ' ';
            pos = pos2 + 1;
        } else if (s[pos2] == '%') {
            try {
                out += (char)NStr::StringToInt(s.substr(pos2 + 1, 2), 0, 16);
                pos = pos2 + 3;
            } catch (CStringException&) {
                // some sources neglect to encode % (!)
                out += '%';
                pos = pos2 + 1;
            }
        } else {
            _TROUBLE;
        }
    }
    return out;
}


CRef<CSeq_entry> CGFFReader::Read(CNcbiIstream& in, TFlags flags)
{
    CStreamLineReader lr(in);
    return Read(lr, flags);
}

CRef<CSeq_entry> CGFFReader::Read(ILineReader& in, TFlags flags)
{
    x_Reset();
    m_Flags  = flags;
    m_LineReader = &in;

    if (m_Flags & fSetVersion3) {
        m_Version = 3;
    }

    TStr line;
    while ( !in.AtEOF() ) {
        ++m_LineNumber;
        char c = in.PeekChar();
        if (c == '#') {
            line = *++in;
            if (line.size() > 2  &&  line[1] == '#') {
                x_ParseStructuredComment(line);
                // ignore regular comments
            }
        } else if (c == '>') {
            // implicit ##FASTA
            x_ReadFastaSequences(in);
        } else {
            line = *++in;            
            if ( x_IsLineUcscMetaInformation(line) ) {
                // UCSC browser or track line. For now, we ignore those.
                continue;
            }
            if ( line.empty() ) {
                // too commonly used for file formatting to even warn about
                continue;
            }
            CRef<SRecord> record = x_ParseFeatureInterval(line);
            if (record) {
                
                if (record->id.empty()) {
                    x_ParseAndPlace(*record);
                } else {
                    CRef<SRecord>& match = m_DelayedRecords[ record->id ];
                    // _TRACE(id << " -> " << match.GetPointer());
                    if (match) {
                        x_MergeRecords(*match, *record);
                    } else {
                        match.Reset(record);
                    }
                }
            }
        }
    }

    NON_CONST_ITERATE (TDelayedRecords, it, m_DelayedRecords) {
        SRecord& rec = *it->second;
        /// merge mergeable ranges
        NON_CONST_ITERATE (SRecord::TLoc, loc_iter, rec.loc) {
            ITERATE (set<TSeqRange>, src_iter, loc_iter->merge_ranges) {
                TSeqRange range(*src_iter);
                set<TSeqRange>::iterator dst_iter =
                    loc_iter->ranges.begin();
                for ( ;  dst_iter != loc_iter->ranges.end();  ) {
                    TSeqRange r(range);
                    r += *dst_iter;
                    if (r.GetLength() <=
                        range.GetLength() + dst_iter->GetLength()) {
                        range += *dst_iter;
                        _TRACE("merging overlapping ranges: "
                               << range.GetFrom() << " - "
                               << range.GetTo() << " <-> "
                               << dst_iter->GetFrom() << " - "
                               << dst_iter->GetTo());
                        loc_iter->ranges.erase(dst_iter++);
                        break;
                    } else {
                        ++dst_iter;
                    }
                }
                loc_iter->ranges.insert(range);
            }
        }

        if (rec.key == "exon") {
            rec.key = "mRNA";
        }
        x_ParseAndPlace(rec);
    }

    ///
    /// remap gene refs
    /// we have built a set of gene-id -> gene-ref pairs
    ///
    if (m_TSE  &&  m_GeneRefs.size()) {
        NON_CONST_ITERATE (TGeneRefs, iter, m_GeneRefs) {
            if ( !iter->second->IsSetLocus()  &&
                 !iter->second->IsSetLocus_tag()) {
                iter->second->SetLocus(iter->first);
            } else if ( !iter->second->IsSetLocus()  ||
                        iter->second->GetLocus() != iter->first) {
                iter->second->SetSyn().push_back(iter->first);
            }
        }

        CTypeIterator<CSeq_feat> feat_iter(*m_TSE);
        for ( ;  feat_iter;  ++feat_iter) {
            const CGene_ref* ref = NULL;
            if (feat_iter->GetData().IsGene()) {
                ref = &feat_iter->GetData().GetGene();
            } else {
                ref = feat_iter->GetGeneXref();
            }
            if (ref  &&  ref->IsSetLocus()) {
                TGeneRefs::const_iterator iter =
                    m_GeneRefs.find(ref->GetLocus());
                if (iter != m_GeneRefs.end()) {
                    const_cast<CGene_ref*>(ref)->Assign(*iter->second);
                }
            }
        }
    }

    CRef<CSeq_entry> tse(m_TSE); // need to save before resetting.
    x_Reset();

    // promote transcript_id and protein_id to products
    if (flags & fSetProducts) {
        CTypeIterator<CSeq_feat> feat_iter(*tse);
        for ( ;  feat_iter;  ++feat_iter) {
            CSeq_feat& feat = *feat_iter;

            string qual_name;
            switch (feat.GetData().GetSubtype()) {
            case CSeqFeatData::eSubtype_cdregion:
                qual_name = "protein_id";
                break;

            case CSeqFeatData::eSubtype_mRNA:
                qual_name = "transcript_id";
                break;

            default:
                continue;
                break;
            }

            string id_str = feat.GetNamedQual(qual_name);
            if ( !id_str.empty() ) {
                CRef<CSeq_id> id = x_ResolveSeqName(id_str);
                feat.SetProduct().SetWhole(*id);
            }
        }
    }

    if (flags & fCreateGeneFeats) {
        CTypeIterator<CSeq_annot> annot_iter(*tse);
        for ( ;  annot_iter;  ++annot_iter) {
            CSeq_annot& annot = *annot_iter;
            if (annot.GetData().Which() != CSeq_annot::TData::e_Ftable) {
                continue;
            }

            // we work within the scope of one annotation
            CSeq_annot::TData::TFtable::iterator feat_iter = 
                annot.SetData().SetFtable().begin();
            CSeq_annot::TData::TFtable::iterator feat_end = 
                annot.SetData().SetFtable().end();

            /// we plan to create a series of gene features, one for each gene
            /// identified above
            /// genes are identified via a 'gene_id' marker
            typedef map<string, CRef<CSeq_feat> > TGeneMap;
            TGeneMap genes;
            for (bool has_genes = false;
                 feat_iter != feat_end  &&  !has_genes;  ++feat_iter) {
                CSeq_feat& feat = **feat_iter;

                switch (feat.GetData().GetSubtype()) {
                case CSeqFeatData::eSubtype_gene:
                    /// we already have genes, so don't add any more
                    has_genes = true;
                    genes.clear();
                    break;

                case CSeqFeatData::eSubtype_mRNA:
                case CSeqFeatData::eSubtype_cdregion:
                    /// for mRNA and CDS features, create a gene
                    /// this is only done if the gene_id parameter was set
                    /// in parsing, we promote gene_id to a gene xref
                    if ( !feat.GetGeneXref() ) {
                        continue;
                    }
                    {{
                        string gene_id;
                        feat.GetGeneXref()->GetLabel(&gene_id);
                        _ASSERT( !gene_id.empty() );
                        TSeqRange range = feat.GetLocation().GetTotalRange();

                        ENa_strand strand = feat.GetLocation().GetStrand();
                        const CSeq_id* id = feat.GetLocation().GetId();
                        if ( !id ) {
                            x_Error("No consistent ID found; gene feature skipped");
                            continue;
                        }

                        TGeneMap::iterator iter = genes.find(gene_id);
                        if (iter == genes.end()) {
                            /// new gene feature
                            CRef<CSeq_feat> gene(new CSeq_feat());
                            gene->SetData().SetGene().Assign(*feat.GetGeneXref());

                            gene->SetLocation().SetInt().SetFrom(range.GetFrom());
                            gene->SetLocation().SetInt().SetTo  (range.GetTo());
                            gene->SetLocation().SetId(*id);
                            gene->SetLocation().SetInt().SetStrand(strand);
                            genes[gene_id] = gene;
                        } else {
                            /// we agglomerate the old location
                            CRef<CSeq_feat> gene = iter->second;

                            TSeqRange r2 = gene->GetLocation().GetTotalRange();
                            range += r2;
                            gene->SetLocation().SetInt().SetFrom(range.GetFrom());
                            gene->SetLocation().SetInt().SetTo  (range.GetTo());
                            gene->SetLocation().InvalidateTotalRangeCache();
                        }
                    }}
                    break;

                default:
                    break;
                }
            }

            ITERATE (TGeneMap, iter, genes) {
                annot.SetData().SetFtable().push_back(iter->second);
            }
        }
    }

    return tse;
}


void CGFFReader::x_Warn(const string& message, unsigned int line)
{
    if (line) {
        ERR_POST_X(2, Warning << message << " [GFF input, line " << line << ']');
    } else {
        ERR_POST_X(3, Warning << message << " [GFF input]");
    }
}


void CGFFReader::x_Error(const string& message, unsigned int line)
{
    if (line) {
        ERR_POST_X(1, Error << message << " [GFF input, line " << line << ']');
    } else {
        ERR_POST_X(1, Error << message << " [GFF input]");
    }
}


void CGFFReader::x_Info(const string& message, unsigned int line)
{
    if (line) {
        ERR_POST_X(1, Info << message << " [GFF input, line " << line << ']');
    } else {
        ERR_POST_X(1, Info << message << " [GFF input]");
    }
}


void CGFFReader::x_Reset(void)
{
    m_TSE.Reset(new CSeq_entry);
    m_SeqNameCache.clear();
    m_SeqCache.clear();
    m_DelayedRecords.clear();
    m_GeneRefs.clear();
    m_DefMol.erase();
    m_LineNumber = 0;
    m_Version = 2;
}


bool CGFFReader::x_ParseStructuredComment(const TStr& line)
{
    if ( line.empty() || line[0] != '#' || line[1] != '#' ) {
        return false;
    }
    TStrVec v;
    // NStr::Tokenize(line, "# \t", v, NStr::eMergeDelims);
    typedef CStrTokenize<TStr, TStrVec> TTokenizer;
    TTokenizer::TPosContainer pos_container;
    TTokenizer::Do(line, "# \t", v, TTokenizer::eMergeDelims, pos_container);
    if (v.empty()) {
        return true;
    }
    if (v[0] == "date"  &&  v.size() > 1) {
        x_ParseDateComment(v[1]);
    } else if (v[0] == "Type"  &&  v.size() > 1) {
        x_ParseTypeComment(v[1], v.size() > 2 ? v[2] : TStr());
    } else if (v[0] == "gff-version"  &&  v.size() > 1) {
        m_Version = NStr::StringToInt(v[1]);
    } else if (v[0] == "FASTA") {
        x_ReadFastaSequences(*m_LineReader);
    }
    // etc.
    return true;
}


void CGFFReader::x_ParseDateComment(const TStr& date)
{
    try {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetUpdate_date().SetToTime(CTime(date, "Y-M-D"),
                                         CDate::ePrecision_day);
        m_TSE->SetSet().SetDescr().Set().push_back(desc);
    } catch (exception& e) {
        x_Error(string("Bad ISO date: ") + e.what(), x_GetLineNumber());
    }
}


void CGFFReader::x_ParseTypeComment(const TStr& moltype, const TStr& seqname)
{
    if (seqname.empty()) {
        m_DefMol = moltype;
    } else {
        // automatically adds to m_TSE if new
        x_ResolveID(*x_ResolveSeqName(seqname), moltype);
    }
}


void CGFFReader::x_ReadFastaSequences(ILineReader& in)
{
    CFastaReader reader(in, fReadFasta_AssumeNuc);
    CRef<CSeq_entry> seqs = reader.ReadSet();
    for (CTypeIterator<CBioseq> it(*seqs);  it;  ++it) {
        if (it->GetId().empty()) { // can this happen?
            CRef<CSeq_entry> parent(new CSeq_entry);
            parent->SetSeq(*it);
            m_TSE->SetSet().SetSeq_set().push_back(parent);
            continue;
        }
        CRef<CBioseq> our_bs = x_ResolveID(*it->GetId().front(), kEmptyStr);
        // keep our annotations, but replace everything else.
        // (XXX - should also keep mol)
        our_bs->SetId() = it->GetId();
        if (it->IsSetDescr()) {
            our_bs->SetDescr(it->SetDescr());
        }
        our_bs->SetInst(it->SetInst());
    }
}


CRef<CGFFReader::SRecord>
CGFFReader::x_ParseFeatureInterval(const TStr& line)
{
    typedef CStrTokenize<TStr, TStrVec> TTokenizer;
    TTokenizer::TPosContainer           pos_container;
    TStrVec                             v;
    bool                                misdelimited = false;

    TTokenizer::Do(line, "\t", v, TTokenizer::eNoMergeDelims, pos_container);
    if (v.size() < 8) {
        v.clear();
        TTokenizer::Do(line, " \t", v, TTokenizer::eMergeDelims, pos_container);
        if (v.size() < 8) {
            x_Error("Skipping line due to insufficient fields",
                   x_GetLineNumber());
            return null;
        } else if (m_Version < 3) {
            x_Info("(Recovered) Bad delimiters (should use tabs)", x_GetLineNumber());
            misdelimited = true;
        }
    } else {
        // XXX - warn about extra fields (if any), but only if they're
        // not comments
        // v.resize(9);
    }

    CRef<SRecord> record(x_NewRecord());
    string        accession;
    TSeqPos       from = 0, to = numeric_limits<TSeqPos>::max();
    ENa_strand    strand = eNa_strand_unknown;
    s_URLDecode(v[0], accession);
    record->source = v[1];
    record->key = v[2];

    try {
        from = NStr::StringToUInt(v[3]) - 1;
    } catch (std::exception& e) {
        x_Error(string("Bad FROM position: ") + e.what(), x_GetLineNumber());
    }

    try {
        to = NStr::StringToUInt(v[4]) - 1;
    } catch (std::exception& e) {
        x_Error(string("Bad TO position: ") + e.what(), x_GetLineNumber());
    }

    record->score = v[5];

    if (v[6] == "+") {
        strand = eNa_strand_plus;
    } else if (v[6] == "-") {
        strand = eNa_strand_minus;
    } else if ( !(v[6] == ".") ) {
        x_Warn("Bad strand " + string(v[6]) + " (should be [+-.])",
               x_GetLineNumber());
    }

    if (v[7] == "0"  ||  v[7] == "1"  ||  v[7] == "2") {
        record->frame = v[7][0] - '0';
    } else if (v[7] == ".") {
        record->frame = -1;
    } else {
        x_Warn("Bad frame " + string(v[7]) + " (should be [012.])",
               x_GetLineNumber());
        record->frame = -1;
    }

    {{
        SRecord::SSubLoc subloc;
        subloc.accession = accession;
        subloc.strand    = strand;
        subloc.ranges.insert(TSeqRange(from, to));

        record->loc.push_back(subloc);
    }}

    SIZE_TYPE i = 8;
    if (m_Version >= 3) {
        x_ParseV3Attributes(*record, v, i);
    } else {
        x_ParseV2Attributes(*record, v, i);
    }

    if ( !misdelimited  &&  (i > 9  ||  (i == 9  &&  v.size() > 9
                                         &&  !NStr::StartsWith(v[9], "#") ))) {
        x_Warn("Extra non-comment fields", x_GetLineNumber());
    }

    if (record->FindAttribute("Target") != record->attrs.end()) {
        record->type = SRecord::eAlign;
    } else {
        record->type = SRecord::eFeat;
    } 

    // extracting additional gff3 attributes
    if (m_Version == 3) {
        SRecord::TAttrs::const_iterator id_it = record->FindAttribute("ID");
        if (id_it != record->attrs.end()) {
            record->id = (*id_it)[1];
        }
    
        SRecord::TAttrs::const_iterator parent_it = record->FindAttribute("Parent");
        if (parent_it != record->attrs.end()) {
            record->parent = (*parent_it)[1];
        }

        SRecord::TAttrs::const_iterator name_it = record->FindAttribute("Name");
        if (name_it != record->attrs.end()) {
            record->name = (*name_it)[1];
        }        
    }

    record->line_no = m_LineNumber;
    record->id = x_FeatureID(*record);
    return record;
}


CRef<CSeq_feat> CGFFReader::x_ParseFeatRecord(const SRecord& record)
{
    CRef<CSeq_feat> feat(CFeature_table_reader::CreateSeqFeat
                         (record.key, *x_ResolveLoc(record.loc),
                          CFeature_table_reader::fTranslateBadKey));
    if (record.frame >= 0  &&  feat->GetData().IsCdregion()) {
        feat->SetData().SetCdregion().SetFrame
            (static_cast<CCdregion::EFrame>(record.frame + 1));
    }
    if ( m_Version == 3 ) {
        ITERATE (SRecord::TAttrs, it, record.attrs) {
            string tag = it->front();
            if (tag == "ID") {
                feat->SetId( *s_StringToFeatId( (*it)[1] ) );
            }
            if (tag == "Parent") {
                CRef<CSeqFeatXref> xref( new CSeqFeatXref );
                xref->SetId( *s_StringToFeatId( (*it)[1] ) );
                feat->SetXref().push_back( xref );
            }
        }
    }

    if ( record.source != "." ) {
        CRef<CGb_qual> source( new CGb_qual );
        source->SetQual( "source" );
        source->SetVal( record.source );
        feat->SetQual().push_back( source );
    }

    string gene_id;
    string gene;
    string locus_tag;
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
            if (tag == "transcript_id") {
                //continue;
            } else if (tag == "gene_id") {
                gene_id = value;
                continue;
            } else if (tag == "gene") {
                gene = value;
                continue;
            } else if (tag == "locus_tag") {
                locus_tag = value;
                continue;
            } else if (tag == "exon_number") {
                tag = "number";
            } else if (NStr::StartsWith(tag, "insd_")) {
                tag.erase(0, 5);
            }

            CFeature_table_reader::AddFeatQual
                (feat, kEmptyStr, tag, value, CFeature_table_reader::fKeepBadKey);
        } else { // don't attempt to parse, just treat as imported
            CRef<CGb_qual> qual(new CGb_qual);
            qual->SetQual(tag);
            qual->SetVal(value);
            feat->SetQual().push_back(qual);
        }
    }

    if ( !gene_id.empty() ) {
        SIZE_TYPE colon = gene_id.find(':');
        if (colon != NPOS) {
            gene_id.erase(0, colon + 1);
        }

        TGeneRefs::value_type val(gene_id, CRef<CGene_ref>());
        TGeneRefs::iterator iter = m_GeneRefs.insert(val).first;
        if ( !iter->second ) {
            iter->second.Reset(new CGene_ref);
        }
        if ( !gene.empty() ) {
            if (iter->second->IsSetLocus()  &&
                iter->second->GetLocus() != gene) {
                LOG_POST_X(4, Warning << "CGFFReader::x_ParseFeatRecord(): "
                           << "inconsistent gene name: "
                           << gene << " != " << iter->second->GetLocus()
                           << ", ignoring second");
            } else if ( !iter->second->IsSetLocus() ) {
                iter->second->SetLocus(gene);
            }
        }
        if ( !locus_tag.empty() ) {
            if (iter->second->IsSetLocus_tag()  &&
                iter->second->GetLocus_tag() != locus_tag) {
                LOG_POST_X(5, Warning << "CGFFReader::x_ParseFeatRecord(): "
                           << "inconsistent locus tag: "
                           << locus_tag << " != " << iter->second->GetLocus_tag()
                           << ", ignoring second");
            } else if ( !iter->second->IsSetLocus_tag() ) {
                iter->second->SetLocus_tag(locus_tag);
            }
        }

        // translate
        CFeature_table_reader::AddFeatQual
            (feat, kEmptyStr, "gene_id", gene_id,
             CFeature_table_reader::fKeepBadKey);
        if (x_GetFlags() & fGBQuals) {
            CFeature_table_reader::AddFeatQual
                (feat, kEmptyStr, "gene", gene_id,
                 CFeature_table_reader::fKeepBadKey);
        }
    }

    return feat;
}


CRef<CSeq_align> CGFFReader::x_ParseAlignRecord(const SRecord& record)
{
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_partial);
    align->SetDim(2);
    SRecord::TAttrs::const_iterator tgit = record.FindAttribute("Target");
    vector<string> target;
    if (tgit != record.attrs.end()) {
        NStr::Tokenize((*tgit)[1], " +-", target, NStr::eMergeDelims);
    }
    if (target.size() != 3) {
        x_Warn("Bad Target attribute", record.line_no);
        return align;
    }
    CRef<CSeq_id> tgid    = x_ResolveSeqName(target[0]);
    TSeqPos       tgstart = NStr::StringToUInt(target[1]) - 1;
    TSeqPos       tgstop  = NStr::StringToUInt(target[2]) - 1;
    TSeqPos       tglen   = tgstop - tgstart + 1;

    CRef<CSeq_loc> refloc = x_ResolveLoc(record.loc);
    CRef<CSeq_id>  refid(&refloc->SetInt().SetId());
    TSeqPos        reflen = 0;
    for (CSeq_loc_CI it(*refloc);  it;  ++it) {
        reflen += it.GetRange().GetLength();
    }

    CRef<CSeq_loc> tgloc(new CSeq_loc);
    tgloc->SetInt().SetId(*tgid);
    tgloc->SetInt().SetFrom(tgstart);
    tgloc->SetInt().SetTo(tgstop);

    SRecord::TAttrs::const_iterator gap_it = record.FindAttribute("Gap");
    if (gap_it == record.attrs.end()) {
        // single ungapped alignment
        if (reflen == tglen  &&  refloc->IsInt()) {
            CDense_seg& ds = align->SetSegs().SetDenseg();
            ds.SetNumseg(1);
            ds.SetIds().push_back(refid);
            ds.SetIds().push_back(tgid);
            ds.SetStarts().push_back(refloc->GetInt().GetFrom());
            ds.SetStarts().push_back(tgstart);
            ds.SetLens().push_back(reflen);
            if (refloc->GetInt().IsSetStrand()) {
                ds.SetStrands().push_back(refloc->GetInt().GetStrand());
                ds.SetStrands().push_back(eNa_strand_plus);
            }
        } else {
            if (reflen != tglen  &&  reflen != 3 * tglen) {
                x_Warn("Reference and target locations have an irregular"
                       " ratio.", record.line_no);
            }
            CRef<CStd_seg> ss(new CStd_seg);
            ss->SetLoc().push_back(refloc);
            ss->SetLoc().push_back(tgloc);
            align->SetSegs().SetStd().push_back(ss);
        }
    } else {
        SCigarAlignment cigar
            ((*gap_it)[1], SCigarAlignment::eOpFirstIfAmbiguous);
        align = cigar(refloc->GetInt(), tgloc->GetInt());
    }

    try {
        CRef<CScore> score(new CScore);
        score->SetValue().SetReal(NStr::StringToDouble(record.score));
        align->SetScore().push_back(score);
    } catch (...) {
    }

    return align;
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


void CGFFReader::x_ParseV2Attributes(SRecord& record, const TStrVec& v,
                                     SIZE_TYPE& i)
{
    string         attr_last_value;
    vector<string> attr_values;
    char           quote_char = 0;

    for (;  i < v.size();  ++i) {
        string s = string(v[i]) + ' ';
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
                    if (pos2 == s.size() - 1) {
                        x_AddAttribute(record, attr_values);
                        attr_values.clear();
                    }
                    break;

                case '#':
                    return;

                case ';':
                    if (attr_values.empty()) {
                        x_Warn("null attribute", x_GetLineNumber());
                    } else {
                        x_AddAttribute(record, attr_values);
                        attr_values.clear();
                    }
                    break;

                // NB: we don't currently search for single quotes.
                case '\"':
                case '\'':
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
        x_AddAttribute(record, attr_values);
    }
}

bool CGFFReader::x_SplitKeyValuePair( const string& pair, string& key, string& value )
{
    if ( NStr::SplitInTwo( pair, "=", key, value ) ) {
        return true;
    }
    if ( NStr::SplitInTwo( pair, " ", key, value ) ) {
        x_Info("(recovered) missdelimited attribute/value pair: " + key, x_GetLineNumber());
        return true;
    }
    x_Warn("attribute without value: " + key, x_GetLineNumber());
    return false;
}
            

void CGFFReader::x_ParseV3Attributes(SRecord& record, const TStrVec& v,
                                     SIZE_TYPE& i)
{
    vector<string> v2, attr;
    NStr::Tokenize(v[i], ";", v2, NStr::eMergeDelims);
    ITERATE (vector<string>, it, v2) {
        attr.clear();
        string key, values;
        if (x_SplitKeyValuePair( *it, key, values )) {
            vector<string> vals;
            attr.resize(2);
            s_URLDecode(key, attr[0]);
            NStr::Tokenize(values, ",", vals);
            ITERATE (vector<string>, it2, vals) {
                string value( *it2 );
                if ( NStr::MatchesMask(value, "\"*\"") ) {
                    //
                    //  Note: The GFF3 spec is ambiguous on whether quoting is
                    //  required for free text values.
                    //
                    value = value.substr(1, value.length()-2);
                }
                s_URLDecode(value, attr[1]);
                x_AddAttribute(record, attr);
            }
        } else {
            x_Warn("attribute without value: " + key, x_GetLineNumber());
            attr.resize(1);
            s_URLDecode(*it, attr[0]);
            x_AddAttribute(record, attr);
            continue;
        }
    }
}


void CGFFReader::x_AddAttribute(SRecord& record, vector<string>& attr)
{
    if (attr.size() == 0) {
        return;
    }

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
    if (record.type != SRecord::eFeat  ||  x_GetFlags() & fNoGTF) {
        return kEmptyStr;
    }

    // has been retrieved in initial interval parsing
    if (m_Version == 3) {
        if (!record.id.empty()) {
            return  record.id;   
        }        
        else if (!record.parent.empty()) {
            return record.source + record.key + record.parent;
        }
        else {
            return "";
        }
    }    
    
    SRecord::TAttrs::const_iterator gene_it = record.FindAttribute("gene_id");
    SRecord::TAttrs::const_iterator transcript_it
        = record.FindAttribute("transcript_id");

    // concatenate our IDs from above, if found
    string id;
    if (gene_it != record.attrs.end()) {
        id += (*gene_it)[1];
    }

    if (transcript_it != record.attrs.end()) {
        if ( !id.empty() ) {
            id += ' ';
        }
        id += (*transcript_it)[1];
    }

    // look for db xrefs
    SRecord::TAttrs::const_iterator dbxref_it
        = record.FindAttribute("db_xref");
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
    } else if (record.key == "exon") {
        // normally separate intervals, but may want to merge.
        if (x_GetFlags() & fMergeExons) {
            id += record.key;
        } else {
            SRecord::TAttrs::const_iterator it
                = record.FindAttribute("exon_number");
            if (it == record.attrs.end()) {
                return kEmptyStr;
            } else {
                id += record.key + ' ' + (*it)[1];
            }
        }
    } else if (x_GetFlags() & fMergeOnyCdsMrna) {
        return kEmptyStr;
    }
    return id;
}


void CGFFReader::x_MergeRecords(SRecord& dest, const SRecord& src)
{
    // XXX - perform sanity checks and warn on mismatch

    bool merge_overlaps = false;
    if (dest.key == "CDS"  &&
        (src.key == "start_codon"  ||  src.key == "stop_codon")) {
        // start_codon and stop_codon features should be merged into
        // existing CDS locations
        merge_overlaps = true;
    }

    if ((dest.key == "start_codon"  ||  dest.key == "stop_codon") &&
        src.key == "CDS") {
        // start_codon and stop_codon features should be merged into
        // existing CDS locations
        merge_overlaps = true;
        dest.key = "CDS";
    }

    // adjust the frame as needed
    int best_frame = dest.frame;

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
                if (slit->strand == eNa_strand_plus) {
                    if (slit->ranges.begin()->GetFrom() <
                        dlit->ranges.begin()->GetFrom()) {
                        best_frame = src.frame;
                    }
                } else {
                    if (slit->ranges.begin()->GetTo() >
                        dlit->ranges.begin()->GetTo()) {
                        best_frame = src.frame;
                    }
                }
                if (merge_overlaps) {
                    ITERATE (set<TSeqRange>, set_iter, slit->ranges) {
                        dlit->merge_ranges.insert(*set_iter);
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

    dest.frame = best_frame;
    if (src.key != dest.key) {
        if (dest.key == "CDS"  &&  NStr::EndsWith(src.key, "_codon")
            &&  !(x_GetFlags() & fNoGTF) ) {
            // ok
        } else if (src.key == "CDS" &&  NStr::EndsWith(dest.key, "_codon")
            &&  !(x_GetFlags() & fNoGTF) ) {
            dest.key = "CDS";
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

        if (dait_tag == dait_end  ||  dait_tag->front() != tag) {
            dait_tag = dait;
        }
        if (dait != dait_end  &&  dait->front() == tag) {
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
                dait_tag = dait_end;
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


void CGFFReader::x_PlaceAlignment(CSeq_align& align, const SRecord& record)
{
    CRef<CBioseq> seq;
    try {
        seq = x_ResolveID(align.GetSeq_id(0), kEmptyStr);
    } catch (...) {
    }
    CBioseq::TAnnot& annots
        = seq ? seq->SetAnnot() : m_TSE->SetSet().SetAnnot();
    NON_CONST_ITERATE (CBioseq::TAnnot, it, annots) {
        if ((*it)->GetData().IsAlign()) {
            (*it)->SetData().SetAlign().push_back(CRef<CSeq_align>(&align));
            return;
        }
    }
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetAlign().push_back(CRef<CSeq_align>(&align));
    annots.push_back(annot);
}


void CGFFReader::x_ParseAndPlace(const SRecord& record)
{
    switch (record.type) {
    case SRecord::eFeat:
        x_PlaceFeature(*x_ParseFeatRecord(record), record);
        break;
    case SRecord::eAlign:
        x_PlaceAlignment(*x_ParseAlignRecord(record), record);
        break;
    default:
        x_Warn("Unknown record type " + NStr::IntToString(record.type),
               record.line_no);
    }
}


CRef<CSeq_id> CGFFReader::x_ResolveSeqName(const string& name)
{
    CRef<CSeq_id>& id = m_SeqNameCache[name];
    if (id.NotEmpty()
        &&  (id->Which() == CSeq_id::e_not_set
             ||  static_cast<int>(id->Which()) >= CSeq_id::e_MaxChoice)) {
        x_Warn("x_ResolveSeqName: invalid cache entry for " + name);
        id.Reset();
    }
    if ( !id ) {
        id.Reset(x_ResolveNewSeqName(name));
    }
    if ( !id ||  id->Which() == CSeq_id::e_not_set
        ||  static_cast<int>(id->Which()) >= CSeq_id::e_MaxChoice) {
        x_Warn("x_ResolveNewSeqName returned null or invalid ID for " + name);
        id.Reset(new CSeq_id(CSeq_id::e_Local, name, name));
    }
    return id;
}


CRef<CSeq_id> CGFFReader::x_ResolveNewSeqName(const string& name)
{
    if (m_Flags & fAllIdsAsLocal) {
        if (NStr::StartsWith(name, "lcl|")) {
            return CRef<CSeq_id>(new CSeq_id(name));
        } else {
            return CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local, name));
        }
    }

    if (m_Flags & fNumericIdsAsLocal) {
        if (name.find_first_not_of("0123456789") == string::npos) {
            return CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local, name));
        }
    }
    try {
        CRef<CSeq_id> pId(new CSeq_id(name));
        if (!pId || (pId->IsGi() && pId->GetGi() < 500) ) {
            pId = new CSeq_id(CSeq_id::e_Local, name);
        }
        return pId;
    }
    catch (CSeqIdException&) {
        return CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local, name));
    }
}


CRef<CBioseq> CGFFReader::x_ResolveID(const CSeq_id& id, const TStr& mol)
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

void CGFFReader::x_SetProducts( CRef<CSeq_entry>& tse )
{
    CTypeIterator<CSeq_feat> feat_iter(*tse);
    for ( ;  feat_iter;  ++feat_iter) {
        CSeq_feat& feat = *feat_iter;

        string qual_name;
        switch (feat.GetData().GetSubtype()) {
        case CSeqFeatData::eSubtype_cdregion:
            qual_name = "protein_id";
            break;

        case CSeqFeatData::eSubtype_mRNA:
            qual_name = "transcript_id";
            break;

        default:
            continue;
            break;
        }

        string id_str = feat.GetNamedQual(qual_name);
        if ( !id_str.empty() ) {
            CRef<CSeq_id> id = x_ResolveSeqName(id_str);
            feat.SetProduct().SetWhole(*id);
        }
    }
}

void CGFFReader::x_CreateGeneFeatures( CRef<CSeq_entry>& tse )
{
    CTypeIterator<CSeq_annot> annot_iter(*tse);
    for ( ;  annot_iter;  ++annot_iter) {
        CSeq_annot& annot = *annot_iter;
        if (annot.GetData().Which() != CSeq_annot::TData::e_Ftable) {
            continue;
        }

        // we work within the scope of one annotation
        CSeq_annot::TData::TFtable::iterator feat_iter = 
            annot.SetData().SetFtable().begin();
        CSeq_annot::TData::TFtable::iterator feat_end = 
            annot.SetData().SetFtable().end();

        /// we plan to create a series of gene features, one for each gene
        /// identified above
        /// genes are identified via a 'gene_id' marker
        typedef map<string, CRef<CSeq_feat> > TGeneMap;
        TGeneMap genes;
        for (bool has_genes = false;
             feat_iter != feat_end  &&  !has_genes;  ++feat_iter) {
            CSeq_feat& feat = **feat_iter;

            switch (feat.GetData().GetSubtype()) {
            case CSeqFeatData::eSubtype_gene:
                /// we already have genes, so don't add any more
                has_genes = true;
                genes.clear();
                break;

            case CSeqFeatData::eSubtype_mRNA:
            case CSeqFeatData::eSubtype_cdregion:
                /// for mRNA and CDS features, create a gene
                /// this is only done if the gene_id parameter was set
                /// in parsing, we promote gene_id to a gene xref
                if ( !feat.GetGeneXref() ) {
                    continue;
                }
                {{
                    string gene_id;
                    feat.GetGeneXref()->GetLabel(&gene_id);
                    _ASSERT( !gene_id.empty() );
                    TSeqRange range = feat.GetLocation().GetTotalRange();

                    ENa_strand strand = feat.GetLocation().GetStrand();
                    const CSeq_id* id = feat.GetLocation().GetId();
                    if ( !id ) {
                        x_Error("No consistent ID found; gene feature skipped");
                        continue;
                    }

                    TGeneMap::iterator iter = genes.find(gene_id);
                    if (iter == genes.end()) {
                        /// new gene feature
                        CRef<CSeq_feat> gene(new CSeq_feat());
                        gene->SetData().SetGene().Assign(*feat.GetGeneXref());

                        gene->SetLocation().SetInt().SetFrom(range.GetFrom());
                        gene->SetLocation().SetInt().SetTo  (range.GetTo());
                        gene->SetLocation().SetId(*id);
                        gene->SetLocation().SetInt().SetStrand(strand);
                        genes[gene_id] = gene;
                    } else {
                        /// we agglomerate the old location
                        CRef<CSeq_feat> gene = iter->second;

                        TSeqRange r2 = gene->GetLocation().GetTotalRange();
                        range += r2;
                        gene->SetLocation().SetInt().SetFrom(range.GetFrom());
                        gene->SetLocation().SetInt().SetTo  (range.GetTo());
                        gene->SetLocation().InvalidateTotalRangeCache();
                    }
                }}
                break;

            default:
                break;
            }
        }

        ITERATE (TGeneMap, iter, genes) {
            annot.SetData().SetFtable().push_back(iter->second);
        }
    }
}

void CGFFReader::x_RemapGeneRefs( CRef<CSeq_entry>& tse, TGeneRefs& gene_refs )
{
    if ( !tse  ||  gene_refs.empty() ) {
        return;
    }
    NON_CONST_ITERATE (TGeneRefs, iter, gene_refs) {
        if ( !iter->second->IsSetLocus()  &&
             !iter->second->IsSetLocus_tag()) {
            iter->second->SetLocus(iter->first);
        } else if ( !iter->second->IsSetLocus()  ||
                    iter->second->GetLocus() != iter->first) {
            iter->second->SetSyn().push_back(iter->first);
        }
    }

    CTypeIterator<CSeq_feat> feat_iter(*tse);
    for ( ;  feat_iter;  ++feat_iter) {
        const CGene_ref* ref = NULL;
        if (feat_iter->GetData().IsGene()) {
            ref = &feat_iter->GetData().GetGene();
        } else {
            ref = feat_iter->GetGeneXref();
        }
        if (ref  &&  ref->IsSetLocus()) {
            TGeneRefs::const_iterator iter =
                gene_refs.find(ref->GetLocus());
            if (iter != gene_refs.end()) {
                const_cast<CGene_ref*>(ref)->Assign(*iter->second);
            }
        }
    }
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


CGFFReader::SRecord::TAttrs::const_iterator
CGFFReader::SRecord::FindAttribute(const string& att_name, size_t min_values)
const
{
    SRecord::TAttrs::const_iterator it
        = attrs.lower_bound(vector<string>(1, att_name));
    while (it != attrs.end()  &&  it->front() == att_name
           &&  it->size() <= min_values) {
        ++it;
    }
    return (it == attrs.end() || it->front() == att_name) ? it : attrs.end();
}


bool
CGFFReader::x_IsLineUcscMetaInformation(const TStr& line)
{
    // line starts with keyword "browser" or "track"
    return (NStr::StartsWith(line, "browser ") || NStr::StartsWith(line, "track ") );
}
    

END_SCOPE(objects)
END_NCBI_SCOPE
