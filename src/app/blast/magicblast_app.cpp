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
 * Authors:  Greg Boratyn
 *
 */

/** @file blastmapper_app.cpp
 * BLASTMAPPER command line application
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/magicblast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/magicblast_options.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_asn1_input.hpp>
#include <algo/blast/blastinput/blast_sra_input.hpp>
#include <algo/blast/blastinput/magicblast_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include "blast_app_util.hpp"

#include <objtools/format/sam_formatter.hpp>

#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>

#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>

#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/api/blast_seqinfosrc_aux.hpp>

#include <unordered_set>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CMagicBlastApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CMagicBlastApp()
    {
//        CRef<CVersion> version(new CVersion());
//        version->SetVersionInfo(new CBlastVersion());
//        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// This application's command line args
    CRef<CMagicBlastAppArgs> m_CmdLineArgs;
};

void CMagicBlastApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CMagicBlastAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    CArgDescriptions* arg_desc = m_CmdLineArgs->SetCommandLine();
    arg_desc->SetCurrentGroup("Mapping options");
    arg_desc->AddDefaultKey("batch_size", "num", "Number of query sequences "
                            "read in a single batch",
                            CArgDescriptions::eInteger, "1000000");
    SetupArgDescriptions(arg_desc);
}

static char s_Complement(char c)
{
    char retval;

    switch (c) {
    case 'A':
        retval = 'T';
        break;

    case 'C':
        retval = 'G';
        break;
        
    case 'G':
        retval = 'C';
        break;

    case 'T':
        retval = 'A';
        break;

    case 'N':
        retval = 'N';
        break;

    case '-':
        retval = '-';
        break;

    default:
        retval = 'x';
    };

    return retval;
}


static CNcbiOstream& s_PrintId(CNcbiOstream& ostr, const CSeq_id& id)
{
    string string_id = id.AsFastaString();
    if (id.IsLocal()) {
        // skip 'lcl|'
        ostr << string_id.c_str() + 4;
    }
    else if (id.IsGeneral()) {
        vector<string> tokens;
        NStr::Split((CTempString)string_id, "|", tokens);
        if (tokens.size() >= 3) {
            // skip 'gnl|SRA|'
            ostr << tokens[2];
        }
        else {
            ostr << string_id; 
        }
    }
    else {
        ostr << string_id;
    }

    return ostr;
}


CNcbiOstream& PrintTabular(CNcbiOstream& ostr, const CSeq_align& align,
                           const BLAST_SequenceBlk* queries,
                           const BlastQueryInfo* query_info,
                           bool is_paired, int batch_number, int compartment,
                           const CSeq_align* mate = NULL)
{
    // if paired alignment
    if (align.GetSegs().IsDisc()) {

        const CSeq_align_set& disc = align.GetSegs().GetDisc();
        _ASSERT(disc.Get().size() == 2u);

        CSeq_align_set::Tdata::const_iterator first = disc.Get().begin();
        _ASSERT(first != disc.Get().end());
        CSeq_align_set::Tdata::const_iterator second(first);
        ++second;
        _ASSERT(second != disc.Get().end());

        PrintTabular(ostr, **first, queries, query_info, is_paired,
                     batch_number, compartment, second->GetNonNullPointer());
        ostr << endl;

        PrintTabular(ostr, **second, queries, query_info, is_paired,
                     batch_number, compartment, first->GetNonNullPointer());

        return ostr;
    }

    string sep = "\t";
    s_PrintId(ostr, align.GetSeq_id(0)) << sep;

    if (align.GetSeq_id(1).IsLocal()) {
        ostr << align.GetSeq_id(1).AsFastaString().c_str() + 4 << sep;
    }
    else {
        ostr << align.GetSeq_id(1).AsFastaString() << sep;
    }

    int score;
    double perc_identity;
    align.GetNamedScore(CSeq_align::eScore_Score, score);
    align.GetNamedScore(CSeq_align::eScore_PercentIdentity_Gapped,
                        perc_identity);

    ostr << perc_identity << sep;

    ostr << 0 << sep; // length
    ostr << 0 << sep; // mismatch
    ostr << 0 << sep; // gapopen

    int query_len = 0;

    if (align.GetSegs().IsDenseg()) {
        CRange<TSeqPos> range = align.GetSeqRange(0);
        ostr << range.GetFrom() + 1 << sep << range.GetTo() + 1 << sep;
        range = align.GetSeqRange(1);
        if (align.GetSeqStrand(0) == eNa_strand_minus) {
            ostr << range.GetTo() + 1 << sep << range.GetFrom() + 1 << sep;
        }
        else {
            ostr << range.GetFrom() + 1 << sep << range.GetTo() + 1 << sep;
        }
    }
    else if (align.GetSegs().IsSpliced()) {
        CRange<TSeqPos> range = align.GetSeqRange(0);
        if (align.GetSegs().GetSpliced().IsSetProduct_length()) {
            query_len = align.GetSegs().GetSpliced().GetProduct_length();
        }
        else {
            _ASSERT(0);
        }
        if (align.GetSeqStrand(0) == eNa_strand_minus) {
            ostr << query_len - range.GetTo() << sep
                 << query_len - range.GetFrom() << sep;

            range = align.GetSeqRange(1);
            ostr << range.GetTo() + 1 << sep << range.GetFrom() + 1 << sep;
        }
        else {
            ostr << range.GetFrom() + 1 << sep << range.GetTo() + 1 << sep;
            range = align.GetSeqRange(1);
            ostr << range.GetFrom() + 1 << sep << range.GetTo() + 1 << sep;
        }

    }

    ostr << 0.0 << sep; // e-value
    ostr << 99 << sep;  // bit score

    ostr << score << sep;

    // query is always a plus strand
    ostr << "plus" << sep
         << (align.GetSeqStrand(0) == 1 ? "plus" : "minus");

    string btop_string;
    Int4 num_hits = 0;
    Int4 pair_start = 0;
    Int4 fragment_score = 0;
    Int4 context = -1;

    CConstRef<CUser_object> ext = align.FindExt("Mapper Info");
    if (ext.NotEmpty()) {

        ITERATE (CUser_object::TData, it, ext->GetData()) {
            if (!(*it)->GetLabel().IsStr()) {
                continue;
            }

            if ((*it)->GetLabel().GetStr() == "btop" &&
                (*it)->GetData().IsStr()) {

                btop_string = (*it)->GetString();
            }
            else if ((*it)->GetLabel().GetStr() == "num_hits" &&
                     (*it)->GetData().IsInt()) {

                num_hits = (*it)->GetInt();
            }
            else if ((*it)->GetLabel().GetStr() == "context" &&
                     (*it)->GetData().IsInt()) {
                
                context = (*it)->GetInt();
            }
        }
    }


    // for alignments on the minus strand
    if (align.GetSeqStrand(0) == eNa_strand_minus) {

        // reverse btop string
        string new_btop;
        int i = btop_string.length() - 1;
        while (i >= 0) {
            int to = i;
            while (i >= 0 && (isdigit(btop_string[i]) ||
                              btop_string[i] == ')')) {
                i--;
            }
                
            new_btop += btop_string.substr(i + 1, to - i);

            if (i > 0) {
                if (isalpha(btop_string[i]) || btop_string[i] == '-') {
                    // complement bases in place
                    new_btop += s_Complement(btop_string[i - 1]);
                    new_btop += s_Complement(btop_string[i]);
                    i--;
                }
                else {
                    new_btop += btop_string[i];
                }
            }

            i--;
        }
        btop_string.swap(new_btop);
    }

    fragment_score = score;
    if (mate) {
        int mate_score = 0;
        mate->GetNamedScore(CSeq_align::eScore_Score, mate_score);
        fragment_score += mate_score; 
    }

    // nucleotide complements plus gap
    const Uint1 kComplement[BLASTNA_SIZE] = {3, 2, 1, 0,
                                             0, 0, 0, 0,
                                             0, 0, 0, 0, 
                                             0, 0, 14, 15};

    // report unaligned part of the query: 3' end and reverese complemented
    // 5' end
    _ASSERT(context >= 0);
    string left_overhang = "-";
    string right_overhang = "-";
    if (context >= 0) {
        int query_offset = query_info->contexts[context].query_offset;
        Uint1* query_seq = queries->sequence + query_offset;
        CRange<TSeqPos> range = align.GetSeqRange(0);
        int from = range.GetFrom();
        int to = range.GetToOpen();

        // reverse complemented 5' end
        if (from > 0) {
            int len = MIN(from, 30);
            left_overhang.clear();
            left_overhang.reserve(len);

            for (int i=from - 1;i >= from - len;i--) {
                left_overhang.push_back(
                            BLASTNA_TO_IUPACNA[kComplement[(int)query_seq[i]]]);
            }
        }
        
        // 3' end
        if (to < query_len) {
            int len = MIN(query_len - to, 30);

            right_overhang.clear();
            right_overhang.reserve(len);
            for (int i=to;i < to + len;i++) {
                right_overhang.push_back(BLASTNA_TO_IUPACNA[(int)query_seq[i]]);
            }
        }

        // because for negative strand alignents query sequence is reverse
        // complemented, we can simply swap the overhangs
        if (align.GetSeqStrand(0) == eNa_strand_minus) {
            left_overhang.swap(right_overhang);
        }
    }

    ostr << sep << query_len
         << sep << btop_string
         << sep << num_hits
         << sep << /*splice*/ "-"
         << sep << batch_number << ":" << compartment
         << sep << left_overhang
         << sep << right_overhang;

    if (is_paired && mate) {
        if (align.GetSeq_id(1).Match(mate->GetSeq_id(1))) {
            ostr << sep << "-";
        }
        else {
            ostr << sep << mate->GetSeq_id(1).AsFastaString();
        }

        pair_start = mate->GetSeqStart(1) + 1;
        //FIXME: for tests
        if (mate->GetSeqStrand(0) == eNa_strand_minus) {
            pair_start = mate->GetSeqStop(1) + 1;
        }
        if ((align.GetSeqStart(1) < mate->GetSeqStart(1) &&
             align.GetSeqStrand(0) == eNa_strand_minus) ||
            (mate->GetSeqStart(1) < align.GetSeqStart(1) &&
             mate->GetSeqStrand(0) == eNa_strand_minus)) {
                
            pair_start = -pair_start;
        }
        ostr << sep << pair_start;

        }
        else {
            ostr << sep << "-" << sep << "-";
        }

    ostr << sep << fragment_score;

//    ostr << sep << context;

    return ostr;
}


CNcbiOstream& PrintTabular(CNcbiOstream& ostr, const CSeq_align_set& aligns,
                           const BLAST_SequenceBlk* queries,
                           const BlastQueryInfo* query_info,
                           bool is_paired, int batch_number)
{
    int compartment = 0;
    ITERATE (list< CRef<CSeq_align> >, it, aligns.Get()) {

        PrintTabular(ostr, **it, queries, query_info, is_paired, batch_number,
                     compartment++);
        ostr << endl;
    }

    return ostr;
}


CNcbiOstream& PrintSAMHeader(CNcbiOstream& ostr,
                             CRef<CLocalDbAdapter> db_adapter,
                             const string& cmd_line_args)
{
    BlastSeqSrc* seq_src = db_adapter->MakeSeqSrc();
    IBlastSeqInfoSrc* seqinfo_src = db_adapter->MakeSeqInfoSrc();
    _ASSERT(seq_src && seqinfo_src);

    ostr << "@HD\t" << "VN:1.2\t" << "GO:query" << endl;
    
    Int4 num_seqs = BlastSeqSrcGetNumSeqs(seq_src);
    CRef<CSeq_id> seqid(new CSeq_id);
    Uint4 length;
    for (Int4 i=0;i < num_seqs;i++) {
        GetSequenceLengthAndId(seqinfo_src, i, seqid, &length);

        string id;
        if (seqid->IsLocal()) {
            id = seqid->AsFastaString().c_str() + 4;
        }
        else {
            id = seqid->AsFastaString();
        }

        ostr << "@SQ\t" << "SN:" << id << "\tLN:" << length << endl;
    }

    ostr << "@PG\tID:0\tPN:magicblast\tCL:" << cmd_line_args << endl;

    return ostr;
}


// hash function for pointers to Seq_id
struct hash_seqid
{
    size_t operator()(const CSeq_id* s) const {
        std::hash<string> h;
        return h(s->AsFastaString());
    }    
};


// equal_to function for pointers to Seq_id
struct eq_seqid
{
    bool operator()(const CSeq_id* a, const CSeq_id* b) const {
        return a->Match(*b);
    }
};

// hash_set of pointers to Seq_ids
typedef unordered_set<const CSeq_id*, hash_seqid, eq_seqid> TSeq_idHashSet;

CNcbiOstream& PrintSAMHeader(CNcbiOstream& ostr,
                             CRef<CSeq_align_set> results,
                             CScope& scope,
                             const string& cmd_line_args)
{
    ostr << "@HD\t" << "VN:1.2\t" << "GO:query" << endl;

    TSeq_idHashSet subjects;
    ITERATE (CSeq_align_set::Tdata, it, results->Get()) {
        subjects.insert(&(*it)->GetSeq_id(1));
    }

    ITERATE (TSeq_idHashSet, it, subjects) {
        TSeqPos length = scope.GetSequenceLength(**it);
        string id;
        if ((*it)->IsLocal()) {
            // skip lcl| for local ids
            id = (*it)->AsFastaString().c_str() + 4;
        }
        else {
            id  = (*it)->AsFastaString();
        }
        ostr << "@SQ\t" << "SN:" << id << "\tLN:" << length << endl;
    }
    ostr << "@PG\tID:0\tPN:magicblast\tCL:" << cmd_line_args << endl;
    return ostr;
}

#define SAM_FLAG_MULTI_SEGMENTS  0x1
#define SAM_FLAG_SEGS_ALIGNED    0x2
#define SAM_FLAG_SEQ_REVCOMP    0x10
#define SAM_FLAG_NEXT_REVCOMP   0x20
#define SAM_FLAG_FIRST_SEGMENT  0x40
#define SAM_FLAG_LAST_SEGMENT   0x80

CNcbiOstream& PrintSAM(CNcbiOstream& ostr, const CSeq_align& align,
                       const BLAST_SequenceBlk* queries,
                       const BlastQueryInfo* query_info,
                       int batch_number, int compartment,
                       int sam_flags = 0,
                       const CSeq_align* mate = NULL)
{
    string sep = "\t";

    string btop_string;
    int query_len = 0;
    int num_hits;
    int context = -1;
    
    // if paired alignment
    if (align.GetSegs().IsDisc()) {

        _ASSERT(align.GetSegs().GetDisc().Get().size() == 2);

        const CSeq_align_set& disc = align.GetSegs().GetDisc();
        CSeq_align_set::Tdata::const_iterator first = disc.Get().begin();
        _ASSERT(first != disc.Get().end());
        CSeq_align_set::Tdata::const_iterator second(first);
        ++second;
        _ASSERT(second != disc.Get().end());

        int first_flags = sam_flags | SAM_FLAG_FIRST_SEGMENT |
            SAM_FLAG_SEGS_ALIGNED;
        if ((*second)->GetSeqStrand(0) == eNa_strand_minus) {
            first_flags |= SAM_FLAG_NEXT_REVCOMP; 
        }

        int second_flags = sam_flags | SAM_FLAG_LAST_SEGMENT |
            SAM_FLAG_SEGS_ALIGNED;

        PrintSAM(ostr, **first, queries, query_info, batch_number,
                 compartment, first_flags, second->GetNonNullPointer());
        ostr << endl;

        PrintSAM(ostr, **second, queries, query_info, batch_number,
                 compartment, second_flags, first->GetNonNullPointer());

        return ostr;
    }


    // get align data saved in the user object
    CConstRef<CUser_object> ext = align.FindExt("Mapper Info");
    if (ext.NotEmpty()) {

        ITERATE (CUser_object::TData, it, ext->GetData()) {
            if (!(*it)->GetLabel().IsStr()) {
                continue;
            }

            if ((*it)->GetLabel().GetStr() == "btop" &&
                (*it)->GetData().IsStr()) {

                btop_string = (*it)->GetString();
            }
            else if ((*it)->GetLabel().GetStr() == "num_hits" &&
                     (*it)->GetData().IsInt()) {

                num_hits = (*it)->GetInt();
            }
            else if ((*it)->GetLabel().GetStr() == "context" &&
                     (*it)->GetData().IsInt()) {

                context = (*it)->GetInt();
            }
        }
            
    }

    if (align.GetSegs().Which() == CSeq_align::TSegs::e_Spliced) {
        const CSpliced_seg& spliced = align.GetSegs().GetSpliced();

        query_len = spliced.GetProduct_length();
    }

    // FIXME: if subject is on a minus strand we need to reverse
    // complement both
    if (align.GetSeqStrand(0) == eNa_strand_minus) {
        sam_flags |= SAM_FLAG_SEQ_REVCOMP;
    }

    if (!mate) {
        sam_flags |= SAM_FLAG_FIRST_SEGMENT | SAM_FLAG_LAST_SEGMENT;
    }

    // read id
    // plus 4 to skip 'lcl|' in query id
    s_PrintId(ostr, align.GetSeq_id(0)) << sep;

    // flag
    ostr << sam_flags << sep;

    // reference sequence id
    if (align.GetSeq_id(1).IsLocal()) {
        ostr << align.GetSeq_id(1).AsFastaString().c_str() + 4 << sep;
    }
    else {
        ostr << align.GetSeq_id(1).AsFastaString() << sep;
    }

    // mapping position
    CRange<TSeqPos> range = align.GetSeqRange(1);
    ostr << range.GetFrom() + 1 << sep;
    
    // mapping quality
    ostr << "255" << sep;

    // CIGAR string
    string cigar;
    int edit_distance = 0;
    if (align.GetSegs().Which() == CSeq_align::TSegs::e_Denseg) {
        const CDense_seg& denseg = align.GetSegs().GetDenseg();
        const CDense_seg::TStarts& starts = denseg.GetStarts();
        const CDense_seg::TLens& lens = denseg.GetLens();
        CRange<TSeqPos> qrange = align.GetSeqRange(0);

        if (align.GetSeqStrand(0) == eNa_strand_plus) {
            if (qrange.GetFrom() > 0) {
                cigar += NStr::IntToString(qrange.GetFrom());
                cigar += "S";
            }
        }
        else {
            if ((int)qrange.GetToOpen() < query_len) {
                cigar += NStr::IntToString(query_len - qrange.GetToOpen());
                cigar += "S";
            }
        }
        for (size_t i=0;i < starts.size();i+=2) {
            cigar += NStr::IntToString(lens[i/2]);
            if (starts[i] >= 0 && starts[i + 1] >= 0) {
                cigar += "M";
            }
            else if (starts[i] < 0) {
                if (lens[i/2] < 10) {
                    cigar += "D";
                }
                else {
                    cigar += "N";
                }
            }
            else {
                cigar += "I";
            }
        }
        if (align.GetSeqStrand(0) == eNa_strand_plus) {
            if ((int)qrange.GetToOpen() < query_len) {
                cigar += NStr::IntToString(query_len - qrange.GetToOpen());
                cigar += "S";
            }
        }
        else {
            if (qrange.GetFrom() > 0) {
                cigar += NStr::IntToString(qrange.GetFrom());
                cigar += "S";
            }
        }
    }
    else if (align.GetSegs().Which() == CSeq_align::TSegs::e_Spliced) {
        const CSpliced_seg& spliced = align.GetSegs().GetSpliced();
        CRange<TSeqPos> qrange = align.GetSeqRange(0);

        if (qrange.GetFrom() > 0) {
            cigar += NStr::IntToString(qrange.GetFrom());
            cigar += "S";
        }

        ITERATE (CSpliced_seg::TExons, exon, spliced.GetExons()) {
            int num = 0;
            char op = 0;
            ITERATE(CSpliced_exon::TParts, it, (*exon)->GetParts()) {
                switch ((*it)->Which()) {
                case CSpliced_exon_chunk::e_Match:
                    if (op && op != 'M') {
                        cigar += NStr::IntToString(num);
                        cigar += op;
                        num = 0;
                    }
                    num += (*it)->GetMatch();
                    op = 'M';
                    break;

                case CSpliced_exon_chunk::e_Mismatch:
                    if (op && op != 'M') {
                        cigar += NStr::IntToString(num);
                        cigar += op;
                        num = 0;
                    }
                    edit_distance += (*it)->GetMismatch();
                    num += (*it)->GetMismatch();
                    op = 'M';
                    break;

                case CSpliced_exon_chunk::e_Product_ins:
                    if (op && op != 'I') {
                        cigar += NStr::IntToString(num);
                        cigar += op;
                        num = 0;
                    }
                    edit_distance += (*it)->GetProduct_ins();
                    num += (*it)->GetProduct_ins();
                    op = 'I';
                    break;

                case CSpliced_exon_chunk::e_Genomic_ins:
                    if (op && op != 'D') {
                        cigar += NStr::IntToString(num);
                        cigar += op;
                        num = 0;
                    }
                    edit_distance += (*it)->GetGenomic_ins();
                    num += (*it)->GetGenomic_ins();
                    op = 'D';
                    break;

                default:
                    NCBI_THROW(CException, eInvalid, "Unsupported "
                               "CSpliced_exon_chunk::TPart value");
                }
            }
            if (num > 0) {
                cigar += NStr::IntToString(num);
                cigar += op;
                
            }

            CSpliced_seg::TExons::const_iterator next_exon(exon);
            ++next_exon;
            if (next_exon != spliced.GetExons().end()) {
                int query_gap = (*next_exon)->GetProduct_start().GetNucpos() -
                    (*exon)->GetProduct_end().GetNucpos() - 1;
                if (query_gap > 0) {
                    cigar += NStr::IntToString(query_gap);
                    cigar += "D";
                }
                edit_distance += query_gap;

                int intron = (*next_exon)->GetGenomic_start() -
                    (*exon)->GetGenomic_end() - 1;
                cigar += NStr::IntToString(intron);
                cigar += "N";
            }
        }

        if ((int)qrange.GetToOpen() < query_len) {
            cigar += NStr::IntToString(query_len - qrange.GetToOpen());
            cigar += "S";
        }
    }
    else {
        NCBI_THROW(CSeqalignException, eUnsupported, "The SAM formatter does "
                   "does not support this alignment structure");
    }

    ostr << cigar << sep;

    // reference name of the mate
    if (mate) {
        if (align.GetSeq_id(1).Match(mate->GetSeq_id(1))) {
            ostr << "=";
        }
        else {
            s_PrintId(ostr, mate->GetSeq_id(1));
        }
    }
    else {
        ostr << "*";
    }
    ostr << sep;

    // position of the mate
    if (mate) {
        ostr << MIN(mate->GetSeqStart(1), mate->GetSeqStop(1)) + 1;
    }
    else {
        ostr << "0";
    }
    ostr << sep;

    // observed template length
    if (mate && align.GetSeq_id(1).Match(mate->GetSeq_id(1))) {
        CRange<TSeqPos> mate_range = mate->GetSeqRange(1);
        if (align.GetSeqStrand(0) == eNa_strand_plus &&
            align.GetSeqStrand(1) == eNa_strand_plus) {

            ostr << (int)mate_range.GetTo() - (int)range.GetFrom() + 1;
        }
        else {
            ostr << -((int)range.GetTo() - (int)mate_range.GetFrom() + 1);
        }
    }
    else {
        ostr << "0";
    }
    ostr << sep;

    // read sequence
    if (context >= 0) {
        int query_offset = query_info->contexts[context].query_offset;
        Uint1* query_seq = queries->sequence + query_offset;
        string sequence;
        for (int i=0;i < query_len;i++) {
            sequence += BLASTNA_TO_IUPACNA[query_seq[i]];
        }
        ostr << sequence << sep;
    }
    else {
        ostr << "*" << sep;
    }

    // quality string
    ostr << "*";

    // optional fields
    // number of hits reported for the query
    ostr << sep << "NH:i:" << num_hits;

    // score
    int score = 0;
    align.GetNamedScore(CSeq_align::eScore_Score, score);
    ostr << sep << "AS:i:" << score;

    // edit distance
    ostr << sep << "NM:i:" << edit_distance;

    ostr << sep << "Zb:i:" << batch_number;
    ostr << sep << "Zc:i:" << compartment;

    return ostr;
}


CNcbiOstream& PrintSAM(CNcbiOstream& ostr, const CSeq_align_set& aligns,
                       const BLAST_SequenceBlk* queries,
                       const BlastQueryInfo* query_info,
                       int batch_number, bool is_paired = false)
{
    int sam_flags = 0;
    int compartment = 0;

    if (is_paired) {
        sam_flags |= SAM_FLAG_MULTI_SEGMENTS;
    }

    ITERATE (list< CRef<CSeq_align> >, it, aligns.Get()) {

        PrintSAM(ostr, **it, queries, query_info, batch_number, compartment++,
                 sam_flags);
        ostr << endl;
    }

    return ostr;
}

static int s_GetNumberOfSubjects(CRef<CLocalDbAdapter> db_adapter)
{
    if (db_adapter->IsBlastDb()) {
        CSeqDB seqdb(db_adapter->GetDatabaseName(),
                     db_adapter->IsProtein() ? CSeqDB::eProtein :
                     CSeqDB::eNucleotide);

        return seqdb.GetNumSeqs();
    }

    BlastSeqSrc* seq_src = db_adapter->MakeSeqSrc();
    IBlastSeqInfoSrc* seqinfo_src = db_adapter->MakeSeqInfoSrc();
    _ASSERT(seq_src && seqinfo_src);

    int num_seqs = BlastSeqSrcGetNumSeqs(seq_src);

    return num_seqs;
}


static Uint8 s_GetDbSize(CRef<CLocalDbAdapter> db_adapter)
{
    if (db_adapter->IsBlastDb()) {
        CSeqDB seqdb(db_adapter->GetDatabaseName(),
                     db_adapter->IsProtein() ? CSeqDB::eProtein :
                     CSeqDB::eNucleotide);

        return seqdb.GetTotalLength();
    }

    BlastSeqSrc* seq_src = db_adapter->MakeSeqSrc();
    IBlastSeqInfoSrc* seqinfo_src = db_adapter->MakeSeqInfoSrc();
    _ASSERT(seq_src && seqinfo_src);

    return BlastSeqSrcGetTotLen(seq_src);
}


static CShortReadFastaInputSource::EInputFormat
s_QueryOptsInFmtToFastaInFmt(CMapperQueryOptionsArgs::EInputFormat infmt)
{
    CShortReadFastaInputSource::EInputFormat retval;
    switch (infmt) {
    case CMapperQueryOptionsArgs::eFasta:
        retval = CShortReadFastaInputSource::eFasta;
        break;

    case CMapperQueryOptionsArgs::eFastc:
        retval = CShortReadFastaInputSource::eFastc;
        break;

    case CMapperQueryOptionsArgs::eFastq:
        retval = CShortReadFastaInputSource::eFastq;
        break;

    default:
        NCBI_THROW(CException, eInvalid, "Invalid input format, "
                   "should be Fasta, Fastc, or Fastq");
    };

    return retval;
}

// Create input source object for reading query sequences
static CRef<CBlastInputSourceOMF>
s_CreateInputSource(CRef<CMapperQueryOptionsArgs> query_opts,
                    CRef<CMagicBlastAppArgs> cmd_line_args,
                    int batch_size)
{
    CRef<CBlastInputSourceOMF> retval;

    CMapperQueryOptionsArgs::EInputFormat infmt = query_opts->GetInputFormat();
    
    switch (infmt) {
    case CMapperQueryOptionsArgs::eFasta:
    case CMapperQueryOptionsArgs::eFastc:
    case CMapperQueryOptionsArgs::eFastq:

        if (query_opts->HasMateInputStream()) {
            retval.Reset(new CShortReadFastaInputSource(
                    cmd_line_args->GetInputStream(),
                    *query_opts->GetMateInputStream(),
                    batch_size,
                    s_QueryOptsInFmtToFastaInFmt(query_opts->GetInputFormat()),
                    query_opts->DoQualityFilter()));
        }
        else {
            retval.Reset(new CShortReadFastaInputSource(
                    cmd_line_args->GetInputStream(),
                    batch_size,
                    s_QueryOptsInFmtToFastaInFmt(query_opts->GetInputFormat()),
                    query_opts->IsPaired(),
                    query_opts->DoQualityFilter()));
        }
        break;

    case CMapperQueryOptionsArgs::eASN1text:
    case CMapperQueryOptionsArgs::eASN1bin:

        if (query_opts->HasMateInputStream()) {
            retval.Reset(new CASN1InputSourceOMF(
                                    cmd_line_args->GetInputStream(),
                                    *query_opts->GetMateInputStream(),
                                    batch_size,
                                    infmt == CMapperQueryOptionsArgs::eASN1bin,
                                    query_opts->DoQualityFilter()));
        }
        else {
            retval.Reset(new CASN1InputSourceOMF(
                                    cmd_line_args->GetInputStream(),
                                    batch_size,
                                    infmt == CMapperQueryOptionsArgs::eASN1bin,
                                    query_opts->IsPaired(),
                                    query_opts->DoQualityFilter()));
        }
        break;

    case CMapperQueryOptionsArgs::eSra:
        retval.Reset(new CSraInputSource(query_opts->GetSraAccessions(),
                                         batch_size, query_opts->IsPaired(),
                                         query_opts->DoQualityFilter()));
        break;
        

    default:
        NCBI_THROW(CException, eInvalid, "Unrecognized input format");
    };

    return retval;
}


int CMagicBlastApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;
    const int kSamLargeNumSubjects = 10000;
    const Uint8 kLargeDb = 1UL << 29;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);
        SetDiagPostPrefix("magicblast");

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
        CRef<CBlastOptionsHandle> opts_hndl;
        if(RecoverSearchStrategy(args, m_CmdLineArgs)){
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptionsForSavedStrategy(args));
        }
        else {
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));
        }
        const CBlastOptions& opt = opts_hndl->GetOptions();
        
        /*** Initialize the database/subject ***/
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());

        CRef<CLocalDbAdapter> db_adapter;
        CRef<CScope> scope;
        InitializeSubject(db_args, opts_hndl, m_CmdLineArgs->ExecuteRemotely(),
                          db_adapter, scope);
        _ASSERT(db_adapter && scope);


        /*** Get the query sequence(s) ***/
        CRef<CMapperQueryOptionsArgs> query_opts( 
            dynamic_cast<CMapperQueryOptionsArgs*>(
                   m_CmdLineArgs->GetQueryOptionsArgs().GetNonNullPointer()));

        if(!args["sra"] && IsIStreamEmpty(m_CmdLineArgs->GetInputStream()))
           	NCBI_THROW(CArgException, eNoValue, "Query is Empty!");

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());


        const int kNumSubjects = s_GetNumberOfSubjects(db_adapter);
        if (fmt_args->GetFormattedOutputChoice() == CFormattingArgs::eSAM) {
            if (kNumSubjects > 1 || kNumSubjects < kSamLargeNumSubjects) {
                PrintSAMHeader(m_CmdLineArgs->GetOutputStream(), db_adapter,
                               GetCmdlineArgs(GetArguments()));
            }
        }

        // FIXME: If subject is given as FASTA, number of threads is changed
        // to 1 with a warning. The application should quit with an error
        // message.
        int num_query_threads = 1;
        int num_db_threads = 1;

        // we thread searches against smaller databases by queries and against
        // larger database by database
        Uint8 db_size = s_GetDbSize(db_adapter);
        if (db_size < kLargeDb) {
            num_query_threads = m_CmdLineArgs->GetNumThreads();
        }
        else {
            num_db_threads = m_CmdLineArgs->GetNumThreads();
        }

        /*** Process the input ***/
//        int batch_size = m_CmdLineArgs->GetQueryBatchSize() / num_query_threads;
        int batch_size = args["batch_size"].AsInteger() / num_query_threads;
        CRef<CBlastInputSourceOMF> fasta = s_CreateInputSource(query_opts,
                                                               m_CmdLineArgs,
                                                               batch_size);
        CBlastInputOMF input(fasta, batch_size);


        int index;
        vector<ostringstream> os_vector(num_query_threads);
        int batch_number = 0;
        bool print_warning = false;

        while (!input.End()) {

            // scope should not be shared, but it is only used with a
            // single thread

            #pragma omp parallel for if (num_query_threads > 1) \
              num_threads(num_query_threads) default(none) shared(opt, input, \
              fmt_args, os_vector, batch_number, print_warning, \
              db_adapter, scope, num_query_threads, num_db_threads)  \
              private(index) schedule(dynamic, 1)
            for (index = 0;index < num_query_threads;index++) {

                CRef<CBioseq_set> query_batch(new CBioseq_set);
                const string kDbName = db_adapter->GetDatabaseName();
                int thread_batch_number;
                #pragma omp critical (read_input)
                {
                    input.GetNextSeqBatch(*query_batch);
                    batch_number++;
                    thread_batch_number = batch_number;
                }

                int thread_index = (thread_batch_number - 1) % num_query_threads;

                if (query_batch->IsSetSeq_set() &&
                    !query_batch->GetSeq_set().empty()) {
                
                    CRef<IQueryFactory> queries(
                                  new CObjMgrFree_QueryFactory(query_batch));

                    CRef<CSeq_align_set> results;
                    CRef<CBlastOptions> options = opt.Clone();
                    CRef<CMagicBlastOptionsHandle> magic_opts(
                                   new CMagicBlastOptionsHandle(options));

                    CRef<CSearchDatabase> search_db;
                    CRef<CLocalDbAdapter> thread_db_adapter;

                    // thread_db_adapter holds either blastdb or FASTA
                    // sequences
                    if (!kDbName.empty()) {
                        search_db.Reset(
                              new CSearchDatabase(kDbName,
                                    CSearchDatabase::eBlastDbIsNucleotide));

                        thread_db_adapter.Reset(
                                           new CLocalDbAdapter(*search_db));
                    }
                    else {
                        thread_db_adapter = db_adapter;
                    }

                    if (search_db.NotEmpty() && num_query_threads > 1) {
                        search_db->GetSeqDb()->SetNumberOfThreads(1, true);
                    }

                    // do mapping
                    CMagicBlast magicblast(queries, thread_db_adapter, magic_opts);
                    // these are threads by database chunks
                    magicblast.SetNumberOfThreads(num_db_threads);
                    results = magicblast.Run();

                    // format ouput
                    if (fmt_args->GetFormattedOutputChoice() ==
                        CFormattingArgs::eTabular) {

                        CRef<ILocalQueryData> query_data =
                            queries->MakeLocalQueryData(
                                            options.GetNonNullPointer());

                        PrintTabular(os_vector[thread_index],
                                 *results,
                                 query_data->GetSequenceBlk(),
                                 query_data->GetQueryInfo(),
                                 magic_opts->GetPaired(),
                                 thread_batch_number);
                    }
                    else if (fmt_args->GetFormattedOutputChoice() ==
                             CFormattingArgs::eAsnText) {

                        os_vector[thread_index] << MSerial_AsnText << *results;
                    }
                    else {
                
                        if (num_query_threads == 1 &&
                            kNumSubjects >= kSamLargeNumSubjects) {

                            PrintSAMHeader(os_vector[thread_index], results,
                                      *scope, GetCmdlineArgs(GetArguments()));
                        }

                        CRef<ILocalQueryData> query_data =
                            queries->MakeLocalQueryData(
                                               options.GetNonNullPointer());
                        
                        PrintSAM(os_vector[thread_index],
                                 *results,
                                 query_data->GetSequenceBlk(),
                                 query_data->GetQueryInfo(),
                                 batch_number,
                                 magic_opts->GetPaired());
                    }


                    query_batch.Reset();
                }
                // print a warning message if all queries were rejected by
                // input reader
                else if (batch_number == 1) {
                    #pragma omp critical (warning)
                    {
                        print_warning = true;
                    }

                }
            }

            if (print_warning) {
                ERR_POST(Warning << "All query sequences in the last batch "
                         "were rejected");
            }

            // write formatted ouput to stream
            for (index = 0;index < num_query_threads;index++) {
                m_CmdLineArgs->GetOutputStream() << os_vector[index].str();
                // flush string
                os_vector[index].str("");
            }

        }

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

    } CATCH_ALL(status)
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CMagicBlastApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
