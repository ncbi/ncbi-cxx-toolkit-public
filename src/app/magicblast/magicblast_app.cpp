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
#include <algo/blast/blast_sra_input/blast_sra_input.hpp>
#include <algo/blast/blastinput/magicblast_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include "../blast/blast_app_util.hpp"

#include <objtools/format/sam_formatter.hpp>

#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Splice_site.hpp>

#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Dbtag.hpp>

#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/api/blast_seqinfosrc_aux.hpp>
#include <algo/sequence/consensus_splice.hpp>
#include <util/sequtil/sequtil_manip.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <unordered_set>
#include <unordered_map>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

static const int   kMajorVersion = 1;
static const int   kMinorVersion = 3;
static const int   kPatchVersion = 0;

class CMagicBlastVersion : public CVersionInfo
{
public:
    CMagicBlastVersion() : CVersionInfo(kMajorVersion,
                                        kMinorVersion,
                                        kPatchVersion)
    {}
};


class CMagicBlastApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CMagicBlastApp()
    {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CMagicBlastVersion());
        SetFullVersion(version);
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

    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
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

static string s_GetBareId(const CSeq_id& id)
{
    string retval;
    // Gis are printed with the bar
    if (id.IsGi()) {
        retval = id.AsFastaString();
    }
    else if (id.IsGeneral()) {
        const CDbtag& dbt = id.GetGeneral();
        if (dbt.GetTag().IsStr()) {
            retval = dbt.GetTag().GetStr();
        }
        else if (dbt.GetTag().IsId()) {
            retval = NStr::IntToString(dbt.GetTag().GetId());
        }
    }
    else {
        retval = id.GetSeqIdString(true);
    }

    return retval;
}

typedef unordered_map<string, CRef<CSeq_entry> > TQueryMap;

static void s_CreateQueryMap(const CBioseq_set& query_batch,
                             TQueryMap& query_map)
{
    query_map.clear();
    for (auto it: query_batch.GetSeq_set()) {

        CRef<CSeq_entry> seq_entry(it);
        const CSeq_id* seq_id = seq_entry->GetSeq().GetFirstId();
        if (!seq_id) {
            NCBI_THROW(CException, eInvalid, "Missing Sequence Id");
        }
        string id = seq_id->GetSeqIdString();
        query_map[id] = seq_entry;
    }
}

static int s_GetQuerySequence(const TQueryMap& queries,
                              const CSeq_loc& seq_loc,
                              bool reverse_complement,
                              string& sequence)
{
    const CSeq_id* seqid = seq_loc.GetId();
    _ASSERT(seqid);
    TQueryMap::const_iterator it = queries.find(seqid->GetSeqIdString());
    _ASSERT(it != queries.end());
    if (it == queries.end()) {
        return 1;
    }
    CSeq_loc::TRange range = seq_loc.GetTotalRange();
    const CSeq_data& seq_data = it->second->GetSeq().GetInst().GetSeq_data();
    switch (seq_data.Which()) {
    case CSeq_data::e_Iupacna:
        sequence = seq_data.GetIupacna().Get();
        if (seq_loc.Which() != CSeq_loc::e_Whole) {
            sequence = sequence.substr(range.GetFrom(), range.GetLength());
        }
        break;

    case CSeq_data::e_Ncbi2na:
        CSeqConvert::Convert(seq_data.GetNcbi2na().Get(),
                             CSeqUtil::e_Ncbi2na, range.GetFrom(),
                             range.GetLength(),
                             sequence, CSeqUtil::e_Iupacna);
        break;
    case CSeq_data::e_Ncbi4na:
        CSeqConvert::Convert(seq_data.GetNcbi4na().Get(),
                             CSeqUtil::e_Ncbi4na, range.GetFrom(),
                             range.GetLength(),
                             sequence, CSeqUtil::e_Iupacna);
        break;

    case CSeq_data::e_Ncbi8na:
        CSeqConvert::Convert(seq_data.GetNcbi8na().Get(),
                             CSeqUtil::e_Ncbi8na, range.GetFrom(),
                             range.GetLength(),
                             sequence, CSeqUtil::e_Iupacna);
        break;

    default:
        NCBI_THROW(CException, eInvalid, "Unexpected query sequence "
                   "encoding");
    };


    if (reverse_complement) {
        string tmp(sequence);
        CSeqManip::ReverseComplement(tmp, CSeqUtil::e_Iupacna, 0, tmp.length(),
                                     sequence);
    }

    return 0;
}

CNcbiOstream& PrintTabularHeader(CNcbiOstream& ostr, const string& version,
                                 const string& cmd_line_args)
{
    string sep = "\t";

    ostr << "# MAGICBLAST " << version << endl;
    ostr << "# " << cmd_line_args << endl;

    ostr << "# Fields: ";
    ostr << "query acc." << sep;
    ostr << "reference acc." << sep;
    ostr << "% identity" << sep;
    ostr << "not used" << sep;
    ostr << "not used" << sep;
    ostr << "not used" << sep;
    ostr << "query start" << sep;
    ostr << "query end" << sep;
    ostr << "reference start" << sep;
    ostr << "reference end" << sep;
    ostr << "not used" << sep;
    ostr << "not used" << sep;
    ostr << "score" << sep;
    ostr << "query strand" << sep;
    ostr << "reference strand" << sep;
    ostr << "query length" << sep;
    ostr << "BTOP" << sep;
    ostr << "num placements" << sep;
    ostr << "not used" << sep;
    ostr << "compartment" << sep;
    ostr << "left overhang" << sep;
    ostr << "right overhang" << sep;
    ostr << "mate reference" << sep;
    ostr << "make ref. start" << sep;
    ostr << "composite score";

    ostr << endl;

    return ostr;
}


CNcbiOstream& PrintTabular(CNcbiOstream& ostr, const CSeq_align& align,
                           const TQueryMap& queries,
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

        PrintTabular(ostr, **first, queries, is_paired, batch_number,
                     compartment, second->GetNonNullPointer());
        ostr << endl;

        PrintTabular(ostr, **second, queries, is_paired, batch_number,
                     compartment, first->GetNonNullPointer());

        return ostr;
    }

    string sep = "\t";
    ostr << s_GetBareId(align.GetSeq_id(0)) << sep;

    ostr << s_GetBareId(align.GetSeq_id(1)) << sep;

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

    // report unaligned part of the query: 3' end and reverese complemented
    // 5' end
    string left_overhang = "-";
    string right_overhang = "-";
    if (query_len <= 0) {
        TQueryMap::const_iterator it = queries.find(
                                          align.GetSeq_id(0).GetSeqIdString());
        _ASSERT(it != queries.end());
        query_len = it->second->GetSeq().GetInst().GetLength();
    }


    CRange<TSeqPos> range = align.GetSeqRange(0);
    int from = range.GetFrom();
    int to = range.GetToOpen();
    if (align.GetSeqStrand(0) == eNa_strand_minus) {
        from = query_len - range.GetToOpen();
        to = query_len - range.GetFrom();
    }

    // reverse complemented 5' end
    if (from > 0) {
        CSeq_id id(align.GetSeq_id(0).AsFastaString());
        CSeq_interval seq_int(id, MAX(0, from - 30), from - 1);
        CSeq_loc seq_loc;
        seq_loc.SetInt(seq_int);
        left_overhang.clear();
        s_GetQuerySequence(queries, seq_loc, true, left_overhang);
    }
        
    // 3' end
    if (to < query_len) {
        CSeq_id id(align.GetSeq_id(0).AsFastaString());
        CSeq_interval seq_int(id, to, MIN(to + 30 - 1, query_len - 1));
        CSeq_loc seq_loc;
        seq_loc.SetInt(seq_int);
        right_overhang.clear();
        s_GetQuerySequence(queries, seq_loc, false, right_overhang);
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

    return ostr;
}

CNcbiOstream& PrintTabularUnaligned(CNcbiOstream& ostr,
                                    const CMagicBlastResults& results,
                                    const TQueryMap& queries,
                                    bool first_seg)
{
    string sep = "\t";
    CSeq_id id;
    if (!results.IsPaired() || first_seg) {
        id.Set(results.GetQueryId().AsFastaString());
    }
    else {
        id.Set(results.GetLastId().AsFastaString());
    }

    // query
    ostr << s_GetBareId(id) << sep;

    // subject
    ostr << "-" << sep;

    // percent identity
    ostr << 0.0 << sep;

    ostr << 0 << sep; // length
    ostr << 0 << sep; // mismatch
    ostr << 0 << sep; // gapopen

    // query start and stop
    ostr << 0 << sep << 0 << sep;

    // subject start and stop
    ostr << 0 << sep << 0 << sep;
    
    ostr << 0 << sep; // e-value
    ostr << 99 << sep;  // bit score

    ostr << 0 << sep;

    // query and subject strand
    ostr << "-" << sep << "-" << sep;

    // query length
    int query_len = 0;
    TQueryMap::const_iterator it = queries.find(id.GetSeqIdString());
    _ASSERT(it != queries.end());
    if (it != queries.end()) {
        query_len = it->second->GetSeq().GetInst().GetLength();
    }
    ostr << query_len << sep;

    // btop string
    ostr << "-" << sep;

    // number of placements
    ostr << 0 << sep;

    // splice
    ostr << "-" << sep;

    // compartment
    string compart = "-";
    // if a read did not pass filtering
    CMagicBlastResults::TResultsInfo info =
        first_seg ? results.GetFirstInfo() : results.GetLastInfo();
    if ((info & CMagicBlastResults::fFiltered) != 0) {
        compart = "F";
    }
    ostr << compart << sep;

    // left overhang
    ostr << "-" << sep;

    // right overhang
    ostr << "-" << sep;

    // mate reference
    ostr << "-" << sep;

    // mate start position
    ostr << "-" << sep;

    // composite score
    ostr << 0;

    return ostr;
}

CNcbiOstream& PrintTabular(CNcbiOstream& ostr,
                           const CMagicBlastResults& results,
                           const TQueryMap& queries,
                           bool is_paired, int batch_number,
                           int& compartment)
{
    for (auto it: results.GetSeqAlign()->Get()) {
        PrintTabular(ostr, *it, queries, is_paired, batch_number, compartment++);
        ostr << endl;
    }

    if ((results.GetFirstInfo() & CMagicBlastResults::fUnaligned) != 0) {
        PrintTabularUnaligned(ostr, results, queries, true);
        ostr << endl;
    }

    if (results.IsPaired() &&
        (results.GetLastInfo() & CMagicBlastResults::fUnaligned) != 0) {

        PrintTabularUnaligned(ostr, results, queries, false);
        ostr << endl;
    }

    return ostr;
}


CNcbiOstream& PrintTabular(CNcbiOstream& ostr,
                           const CMagicBlastResultSet& results,
                           const CBioseq_set& query_batch,
                           bool is_paired, int batch_number)
{
    TQueryMap queries;
    s_CreateQueryMap(query_batch, queries);

    int compartment = 0;
    for (auto it: results) {
        PrintTabular(ostr, *it, queries, is_paired, batch_number,
                     compartment);
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

    ostr << "@HD\t" << "VN:1.0\t" << "GO:query" << endl;
    
    BlastSeqSrcResetChunkIterator(seq_src);
    BlastSeqSrcIterator* it = BlastSeqSrcIteratorNew();
    CRef<CSeq_id> seqid(new CSeq_id);
    Uint4 length;
    Int4 oid;
    while ((oid = BlastSeqSrcIteratorNext(seq_src, it)) != BLAST_SEQSRC_EOF) {
        GetSequenceLengthAndId(seqinfo_src, oid, seqid, &length);
        ostr << "@SQ\t" << "SN:" << s_GetBareId(*seqid) << "\tLN:" << length
             << endl;
    }
    BlastSeqSrcIteratorFree(it);

    ostr << "@PG\tID:magicblast\tPN:magicblast\tCL:" << cmd_line_args << endl;

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
    ostr << "@HD\t" << "VN:1.0\t" << "GO:query" << endl;

    TSeq_idHashSet subjects;
    ITERATE (CSeq_align_set::Tdata, it, results->Get()) {
        subjects.insert(&(*it)->GetSeq_id(1));
    }

    ITERATE (TSeq_idHashSet, it, subjects) {
        TSeqPos length = scope.GetSequenceLength(**it);
        ostr << "@SQ\t" << "SN:" << s_GetBareId(**it) << "\tLN:" << length
             << endl;
    }
    ostr << "@PG\tID:magicblast\tPN:magicblast\tCL:" << cmd_line_args << endl;
    return ostr;
}

static ENa_strand
s_GetSpliceSiteOrientation(const CSpliced_seg::TExons::const_iterator& exon,
                           const CSpliced_seg::TExons::const_iterator& next_exon)
{
    ENa_strand result = eNa_strand_unknown;

    // orientation is unknown if exons align on different strands or a exon's
    // genomic strand is unknown
    if ((*exon)->GetGenomic_strand() != 
        (*next_exon)->GetGenomic_strand() ||
        (*exon)->GetGenomic_strand() == eNa_strand_unknown) {
        
        return eNa_strand_unknown;
    }

    // orientation is unknown if splice signal is not set
    if (!(*exon)->IsSetDonor_after_exon() ||
        !(*next_exon)->IsSetAcceptor_before_exon()) {

        return eNa_strand_unknown;
    }

    // get splice signal
    string donor = (*exon)->GetDonor_after_exon().GetBases();
    string acceptor = (*next_exon)->GetAcceptor_before_exon().GetBases();

    // if the signal is recognised then the splice orientation is the same as
    // genomic strand
    if (IsConsensusSplice(donor, acceptor) ||
        IsKnownNonConsensusSplice(donor, acceptor)) {

        result = (*exon)->GetGenomic_strand();
    }
    else {
        // otherwise try to recognise reverse complemented splice signals

        string rc_donor;
        string rc_acceptor;

        CSeqManip::ReverseComplement(donor,
                                     CSeqUtil::e_Iupacna,
                                     0, donor.length(),
                                     rc_donor);

        CSeqManip::ReverseComplement(acceptor,
                                     CSeqUtil::e_Iupacna,
                                     0, acceptor.length(),
                                     rc_acceptor);
                        
        // if reverse complemented signals are recognised then splice
        // orientation is opposite to genomic strand
        if (IsConsensusSplice(rc_acceptor, rc_donor) ||
            IsKnownNonConsensusSplice(rc_acceptor, rc_donor)) {

            if ((*exon)->GetGenomic_strand() == eNa_strand_plus) {
                result = eNa_strand_minus;
            }
            else if ((*exon)->GetGenomic_strand() == eNa_strand_minus) {
                result = eNa_strand_plus;
            }
            else {
                result = eNa_strand_unknown;
            }
        }
        else {
            // if neither signals are recognised then splice orientation is
            // unknown
            result = eNa_strand_unknown;
        }
                            
    }

    return result;
}


#define SAM_FLAG_MULTI_SEGMENTS  0x1
#define SAM_FLAG_SEGS_ALIGNED    0x2
#define SAM_FLAG_SEG_UNMAPPED    0x4
#define SAM_FLAG_NEXT_SEG_UNMAPPED 0x8
#define SAM_FLAG_SEQ_REVCOMP    0x10
#define SAM_FLAG_NEXT_REVCOMP   0x20
#define SAM_FLAG_FIRST_SEGMENT  0x40
#define SAM_FLAG_LAST_SEGMENT   0x80
#define SAM_FLAG_SECONDARY      0x100

CNcbiOstream& PrintSAM(CNcbiOstream& ostr, const CSeq_align& align,
                       const TQueryMap& queries,
                       const BlastQueryInfo* query_info,
                       int batch_number, bool& first_secondary, 
                       bool& last_secondary, bool trim_read_ids,
                       const CSeq_align* mate = NULL)
{
    string sep = "\t";

    string btop_string;
    int query_len = 0;
    int num_hits = 0;
    int context = -1;
    int sam_flags = 0;
    
    // if paired alignment
    if (align.GetSegs().IsDisc()) {

        _ASSERT(align.GetSegs().GetDisc().Get().size() == 2);

        const CSeq_align_set& disc = align.GetSegs().GetDisc();
        CSeq_align_set::Tdata::const_iterator first = disc.Get().begin();
        _ASSERT(first != disc.Get().end());
        CSeq_align_set::Tdata::const_iterator second(first);
        ++second;
        _ASSERT(second != disc.Get().end());

        PrintSAM(ostr, **first, queries, query_info, batch_number,
                 first_secondary, last_secondary,
                 trim_read_ids, second->GetNonNullPointer());
        ostr << endl;

        PrintSAM(ostr, **second, queries, query_info, batch_number,
                 first_secondary, last_secondary,
                 trim_read_ids, first->GetNonNullPointer());

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

    vector<ENa_strand> orientation;
    if (align.GetSegs().Which() == CSeq_align::TSegs::e_Spliced) {
        const CSpliced_seg& spliced = align.GetSegs().GetSpliced();

        query_len = spliced.GetProduct_length();
    }

    // FIXME: if subject is on a minus strand we need to reverse
    // complement both
    if (align.GetSeqStrand(0) == eNa_strand_minus) {
        sam_flags |= SAM_FLAG_SEQ_REVCOMP;
    }

    if (context >= 0 && query_info->contexts[context].segment_flags != 0) {
        sam_flags |= SAM_FLAG_MULTI_SEGMENTS;

        if ((query_info->contexts[context].segment_flags & fFirstSegmentFlag)
            != 0) {
            sam_flags |= SAM_FLAG_FIRST_SEGMENT;
        }

        if ((query_info->contexts[context].segment_flags & fLastSegmentFlag)
            != 0) {
            sam_flags |= SAM_FLAG_LAST_SEGMENT;
        }
        
        if ((query_info->contexts[context].segment_flags & fPartialFlag) != 0
            || !mate) {

            sam_flags |= SAM_FLAG_NEXT_SEG_UNMAPPED;
        }

        if (mate) {
            // only concordantly aligned pairs have this bit set
            // FIXME: it is assumed that subject is always in plus strand
            // (BLAST way)
            if ((align.GetSeqStart(1) < mate->GetSeqStart(1) &&
                 align.GetSeqStrand(0) == eNa_strand_plus &&
                 mate->GetSeqStrand(0) == eNa_strand_minus) ||
                (mate->GetSeqStart(1) < align.GetSeqStart(1) &&
                 mate->GetSeqStrand(0) == eNa_strand_plus &&
                 align.GetSeqStrand(0) == eNa_strand_minus)) {

                sam_flags |= SAM_FLAG_SEGS_ALIGNED;
            }

            if (mate->GetSeqStrand(0) == eNa_strand_minus) {
                sam_flags |= SAM_FLAG_NEXT_REVCOMP;
            }
        }
    }

    // set secondary alignment bit
    if ((sam_flags & SAM_FLAG_FIRST_SEGMENT) != 0) {
        if (first_secondary) {
            sam_flags |= SAM_FLAG_SECONDARY;
        }
        else {
            first_secondary = true;
        }
    }
    else {
        if (last_secondary) {
            sam_flags |= SAM_FLAG_SECONDARY;
        }
        else {
            last_secondary = true;
        }
    }

    // read id
    string read_id = s_GetBareId(align.GetSeq_id(0));
    if (trim_read_ids &&
        (NStr::EndsWith(read_id, ".1") || NStr::EndsWith(read_id, ".2") ||
         NStr::EndsWith(read_id, "/1") || NStr::EndsWith(read_id, "/2"))) {

        read_id.resize(read_id.length() - 2);
    }
    ostr << read_id << sep;

    // flag
    ostr << sam_flags << sep;

    // reference sequence id
    ostr << s_GetBareId(align.GetSeq_id(1)) << sep;

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
                    cigar += "I";
                }
                edit_distance += query_gap;

                int intron = (*next_exon)->GetGenomic_start() -
                    (*exon)->GetGenomic_end() - 1;
                cigar += NStr::IntToString(intron);
                cigar += "N";

                // get intron orientation
                orientation.push_back(
                               s_GetSpliceSiteOrientation(exon, next_exon));
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
            ostr << s_GetBareId(mate->GetSeq_id(1));
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
    string sequence;
    CSeq_id id(align.GetSeq_id(0).AsFastaString());
    CSeq_loc seq_loc;
    seq_loc.SetWhole(id);
    int status = s_GetQuerySequence(queries, seq_loc,
                           (sam_flags & SAM_FLAG_SEQ_REVCOMP) != 0, sequence);

    if (!status) {
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

    // splice site orientation
    // The final splice orientation is positive or negative, if all introns in
    // the alignment have the same orientation, or unknown if orientation
    // changes.
    if (!orientation.empty()) {
        char ori;

        switch (orientation[0]) {
        case eNa_strand_plus:
            ori = '+';
            break;

        case eNa_strand_minus:
            ori = '-';
            break;

        default:
            ori = '?';
        }

        for (size_t i=1;i < orientation.size();i++) {
            if (orientation[i] != orientation[0]) {
                ori = '?';
            }
        }

        ostr << sep << "XS:A:" << ori;
    }

    return ostr;
}


CNcbiOstream& PrintSAMUnaligned(CNcbiOstream& ostr,
                                const CMagicBlastResults& results,
                                const TQueryMap& queries,
                                bool first_seg,
                                bool trim_read_ids)
{
    string sep = "\t";

    CSeq_id id;
    if (!results.IsPaired() || first_seg) {
        id.Set(results.GetQueryId().AsFastaString());
    }
    else {
        id.Set(results.GetLastId().AsFastaString());
    }

    // read id
    string read_id = s_GetBareId(id);
    if (trim_read_ids &&
        (NStr::EndsWith(read_id, ".1") || NStr::EndsWith(read_id, ".2") ||
         NStr::EndsWith(read_id, "/1") || NStr::EndsWith(read_id, "/2"))) {

        read_id.resize(read_id.length() - 2);
    }
    ostr << read_id << sep;

    // SAM flags
    int flags = SAM_FLAG_SEG_UNMAPPED;
    if (results.IsPaired()) {
        flags |= SAM_FLAG_MULTI_SEGMENTS;
        if ((first_seg && !results.LastAligned()) ||
            (!first_seg && !results.FirstAligned())) {

            flags |= SAM_FLAG_NEXT_SEG_UNMAPPED;
        }

        if (first_seg) {
            flags |= SAM_FLAG_FIRST_SEGMENT;
        }
        else {
            flags |= SAM_FLAG_LAST_SEGMENT;
        }
    }
    ostr << flags << sep;

    // reference sequence id
    ostr << "*" << sep;

    // mapping position
    ostr << "0" << sep;
    
    // mapping quality
    ostr << "255" << sep;

    // CIGAR
    ostr << "*" << sep;

    // mate reference sequence id
    ostr << "*" << sep;

    // mate postition
    ostr << "0" << sep;

    // template length
    ostr << "0" << sep;

    // sequence
    string sequence;
    CSeq_loc seq_loc;
    seq_loc.SetWhole(id);
    int status = s_GetQuerySequence(queries, seq_loc, false, sequence);
    if (status) {
        ostr << "*" << sep;
    }
    else {
        ostr << sequence << sep;
    }

    // quality string
    ostr << "*";

    // read did not pass filtering
    CMagicBlastResults::TResultsInfo info =
        first_seg ? results.GetFirstInfo() : results.GetLastInfo();
    if ((info & CMagicBlastResults::fFiltered) != 0) {
        ostr << sep << "YF:Z:F";
    }

    return ostr;
}


CNcbiOstream& PrintSAM(CNcbiOstream& ostr, CMagicBlastResults& results,
                       const TQueryMap& queries,
                       const BlastQueryInfo* query_info, int batch_number,
                       bool trim_read_id)
{
    bool first_secondary = false;
    bool last_secondary = false;

    for (auto it: results.GetSeqAlign()->Get()) {
        PrintSAM(ostr, *it, queries, query_info, batch_number, first_secondary,
                 last_secondary, trim_read_id);
        ostr << endl;
    }

    if ((results.GetFirstInfo() & CMagicBlastResults::fUnaligned) != 0) {
        PrintSAMUnaligned(ostr, results, queries, true, trim_read_id);
        ostr << endl;
    }

    if (results.IsPaired() &&
        (results.GetLastInfo() & CMagicBlastResults::fUnaligned) != 0) {
        PrintSAMUnaligned(ostr, results, queries, false, trim_read_id);
        ostr << endl;
    }

    return ostr;
}


CNcbiOstream& PrintSAM(CNcbiOstream& ostr, const CMagicBlastResultSet& results,
                       const CBioseq_set& query_batch,
                       const BlastQueryInfo* query_info,
                       int batch_number,
                       bool trim_read_id)
{
    TQueryMap bioseqs;
    s_CreateQueryMap(query_batch, bioseqs);

    for (auto it: results) {
        PrintSAM(ostr, *it, bioseqs, query_info, batch_number, trim_read_id);
    }

    return ostr;
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
                    CRef<CMagicBlastAppArgs> cmd_line_args)
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
                    s_QueryOptsInFmtToFastaInFmt(query_opts->GetInputFormat())));
        }
        else {
            retval.Reset(new CShortReadFastaInputSource(
                    cmd_line_args->GetInputStream(),
                    s_QueryOptsInFmtToFastaInFmt(query_opts->GetInputFormat()),
                    query_opts->IsPaired()));
        }
        break;

    case CMapperQueryOptionsArgs::eASN1text:
    case CMapperQueryOptionsArgs::eASN1bin:

        if (query_opts->HasMateInputStream()) {
            retval.Reset(new CASN1InputSourceOMF(
                                    cmd_line_args->GetInputStream(),
                                    *query_opts->GetMateInputStream(),
                                    infmt == CMapperQueryOptionsArgs::eASN1bin));
        }
        else {
            retval.Reset(new CASN1InputSourceOMF(
                                    cmd_line_args->GetInputStream(),
                                    infmt == CMapperQueryOptionsArgs::eASN1bin,
                                    query_opts->IsPaired()));
        }
        break;

    case CMapperQueryOptionsArgs::eSra:
        retval.Reset(new CSraInputSource(query_opts->GetSraAccessions(),
                                         query_opts->IsPaired()));
        break;
        

    default:
        NCBI_THROW(CException, eInvalid, "Unrecognized input format");
    };

    return retval;
}


static void
s_InitializeSubject(CRef<blast::CBlastDatabaseArgs> db_args, 
                    CRef<blast::CBlastOptionsHandle> opts_hndl,
                    CRef<blast::CLocalDbAdapter>& db_adapter,
                    Uint8& subject_length,
                    int& num_sequences)
{
    db_adapter.Reset();

    _ASSERT(db_args.NotEmpty());
    CRef<CSearchDatabase> search_db = db_args->GetSearchDatabase();
    CRef<CScope> scope;

    // Initialize the scope... 
    if (scope.Empty()) {
        scope.Reset(new CScope(*CObjectManager::GetInstance()));
    }
    _ASSERT(scope.NotEmpty());

    // ... and then the subjects
    CRef<IQueryFactory> subjects;
    if ( (subjects = db_args->GetSubjects(scope)) ) {
        _ASSERT(search_db.Empty());
        db_adapter.Reset(new CLocalDbAdapter(subjects, opts_hndl, true));

        BlastSeqSrc* seq_src = db_adapter->MakeSeqSrc();
        _ASSERT(seq_src);
        subject_length = BlastSeqSrcGetTotLen(seq_src);
        num_sequences = BlastSeqSrcGetNumSeqs(seq_src);
    } else {
        _ASSERT(search_db.NotEmpty());
        subject_length = search_db->GetSeqDb()->GetTotalLength();
        num_sequences = search_db->GetSeqDb()->GetNumSeqs();
        db_adapter.Reset(new CLocalDbAdapter(*search_db));
    }
}


static bool
s_IsIStreamEmpty(CNcbiIstream & in)
{
	char c;
	CNcbiStreampos orig_p = in.tellg();
	// Piped input
	if(orig_p < 0)
		return false;

	IOS_BASE::iostate orig_state = in.rdstate();
	IOS_BASE::fmtflags orig_flags = in.setf(ios::skipws);

	if(! (in >> c))
		return true;

	in.seekg(orig_p);
	in.flags(orig_flags);
	in.clear();
	in.setstate(orig_state);

	return false;
}


static string
s_GetCmdlineArgs(const CNcbiArguments & a)
{
	string cmd = kEmptyStr;
	for(unsigned int i=0; i < a.Size(); i++) {
		cmd += a[i] + " ";
	}
	return cmd;
}


int CMagicBlastApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;
    const int kSamLargeNumSubjects = INT4_MAX;
    const Uint8 kLargeDb = 1UL << 29;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);
        SetDiagPostPrefix("magicblast");

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
        CRef<CBlastOptionsHandle> opts_hndl;
        opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));
        const CBlastOptions& opt = opts_hndl->GetOptions();
        
        /*** Initialize the database/subject ***/
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());

        CRef<CLocalDbAdapter> db_adapter;
        Uint8 db_size;
        int num_db_sequences;
        s_InitializeSubject(db_args, opts_hndl, db_adapter, db_size,
                            num_db_sequences);
        _ASSERT(db_adapter);


        /*** Get the query sequence(s) ***/
        CRef<CMapperQueryOptionsArgs> query_opts( 
            dynamic_cast<CMapperQueryOptionsArgs*>(
                   m_CmdLineArgs->GetQueryOptionsArgs().GetNonNullPointer()));

        if(!args["sra"] && s_IsIStreamEmpty(m_CmdLineArgs->GetInputStream()))
           	NCBI_THROW(CArgException, eNoValue, "Query is Empty!");

        /*** Get the formatting options ***/
        CRef<CMapperFormattingArgs> fmt_args(
             dynamic_cast<CMapperFormattingArgs*>(
                   m_CmdLineArgs->GetFormattingArgs().GetNonNullPointer()));

        if (fmt_args->GetFormattedOutputChoice() == CFormattingArgs::eSAM) {
            if (num_db_sequences < kSamLargeNumSubjects) {
                PrintSAMHeader(m_CmdLineArgs->GetOutputStream(), db_adapter,
                               s_GetCmdlineArgs(GetArguments()));
            }
        }
        else if (fmt_args->GetFormattedOutputChoice() ==
                 CFormattingArgs::eTabular) {

            PrintTabularHeader(m_CmdLineArgs->GetOutputStream(),
                               GetVersion().Print(),
                               s_GetCmdlineArgs(GetArguments()));
        }

        // FIXME: If subject is given as FASTA, number of threads is changed
        // to 1 with a warning. The application should quit with an error
        // message.
        int num_query_threads = 1;
        int num_db_threads = 1;
        int batch_size = m_CmdLineArgs->GetQueryBatchSize();

        // we thread searches against smaller databases by queries and against
        // larger database by database
        if (db_size < kLargeDb) {
            num_query_threads = m_CmdLineArgs->GetNumThreads();
        }
        else {
            num_db_threads = m_CmdLineArgs->GetNumThreads();
            batch_size *= 2;
        }

        /*** Process the input ***/
        CRef<CBlastInputSourceOMF> fasta = s_CreateInputSource(query_opts,
                                                               m_CmdLineArgs);
        CBlastInputOMF input(fasta, batch_size);


        int index;
        vector<ostringstream> os_vector(num_query_threads);
        int batch_number = 0;
        bool print_warning = false;
        const bool kTrimReadIdForSAM =
            query_opts->IsPaired() && fmt_args->TrimReadIds();

        while (!input.End()) {

            // scope should not be shared, but it is only used with a
            // single thread

            #pragma omp parallel for if (num_query_threads > 1) \
              num_threads(num_query_threads) default(none) shared(opt, input, \
              fmt_args, os_vector, batch_number, print_warning, \
              db_adapter, num_query_threads, num_db_threads, db_args) \
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

                    CRef<CMagicBlastResultSet> results;
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

                        CRef<CSeqDBGiList> gilist =
                            db_args->GetSearchDatabase()->GetGiList();

                        if (gilist.NotEmpty()) {
                            search_db->SetGiList(gilist.GetNonNullPointer());
                        }

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
                    results = magicblast.RunEx();

                    // format ouput
                    if (fmt_args->GetFormattedOutputChoice() ==
                        CFormattingArgs::eTabular) {

                        CRef<ILocalQueryData> query_data =
                            queries->MakeLocalQueryData(
                                            options.GetNonNullPointer());

                        PrintTabular(os_vector[thread_index],
                                     *results,
                                     *query_batch,
                                     magic_opts->GetPaired(),
                                     thread_batch_number);
                    }
                    else if (fmt_args->GetFormattedOutputChoice() ==
                             CFormattingArgs::eAsnText) {

                        os_vector[thread_index] << MSerial_AsnText <<
                            *results->GetFlatResults();
                    }
                    else {
                
/*
                        if (num_query_threads == 1 &&
                            kNumSubjects >= kSamLargeNumSubjects) {

                            PrintSAMHeader(os_vector[thread_index], results,
                                      *scope, GetCmdlineArgs(GetArguments()));
                        }
*/

                        CRef<ILocalQueryData> query_data =
                            queries->MakeLocalQueryData(
                                               options.GetNonNullPointer());
                        
                        PrintSAM(os_vector[thread_index],
                                 *results,
                                 *query_batch,
                                 query_data->GetQueryInfo(),
                                 batch_number,
                                 kTrimReadIdForSAM);
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
