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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/line_reader.hpp>

#include <algo/sequence/adapter_search.hpp>

USING_NCBI_SCOPE;


class CAdapterSearchApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    typedef NAdapterSearch::CSimpleUngappedAligner TAligner;
    static double s_GetCoverage(
        const TAligner& aligner,
        const TAligner::SMatch& match,
        size_t query_len);

    static double s_GetIdentity(
        const TAligner::SMatch& match,
        const char* query,
        size_t query_len,
        const char* subject,
        size_t subject_len);

    //return true iff matched something
    static bool s_MaskQuery(
        const TAligner& aligner,
        char* query_seq, // non-const; will lowercase matched region
        size_t query_len,
        double min_coverage,
        double min_identity);

    // Discover adapter from a set of reads
    void x_DetectAdapter(const CArgs& args);

    // Mask adapters in a set of reads
    void x_MaskAdaptersSingle(const CArgs& args);

    // Similar to above, except uses two aligners
    // to process fwd ard rev reads in a spot independently
    void x_MaskAdaptersPaired(const CArgs& args);
};


void CAdapterSearchApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Discover or lowercase-mask adapter sequences in a set of reads");
    arg_desc->AddDefaultKey
        ("i",
         "input",
         "Seqs in fasta format. NO WRAPPING",
         CArgDescriptions::eInputFile,
         "-",
         CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("o",
         "output",
         "If targets are not specified, the output is the "
             "inferred adapter seq. Otherwise, the output is "
             "the original input with matched adapters lowercased.",
         CArgDescriptions::eOutputFile,
         "-",
         CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("targets",
         "input",
         "Fasta seq(s) of adapters. In paired-end mode, fasta is "
             "'-'-delimited pair of 5'-adapter and 3'-adapter",
         CArgDescriptions::eInputFile,
         CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("min_coverage",
         "float",
         "Minimum coverage, computed relative to alignable range of an adapter [0..1]",
         CArgDescriptions::eDouble,
         "0.8");

    arg_desc->AddDefaultKey
        ("min_identity",
         "float",
         "Minimum coverage, computed relative to alignable range of an adapter [0..1]",
         CArgDescriptions::eDouble,
         "0.9");

    arg_desc->AddFlag
        ("paired_end",
         "Seq is presumed to be a pair of fwd_read+rev_read of equal lengths"
            " in original ->....<- orientation");

    SetupArgDescriptions(arg_desc.release());
}


/// To boost specificity (avoid false positives), will accept only
/// high-coverage matches, where coverage is computed relative to
/// min(adapter length, distance to query's end), i.e. 3'-partialness
/// is expected and not considered low-coverage
double CAdapterSearchApplication::s_GetCoverage(
        const TAligner& aligner,
        const TAligner::SMatch& match,
        size_t query_len)
{
    TAligner::TRange r = aligner.GetSeqRange(match.second);
    size_t subj_len = r.second - r.first;
    size_t distance_to_end = query_len - match.first;
    size_t d = min(subj_len, distance_to_end);
    return d == 0 ? 0 : match.len * 1.0 / d;
}


double CAdapterSearchApplication::s_GetIdentity(
    const TAligner::SMatch& match,
    const char* query,
    size_t query_len,
    const char* subj,
    size_t subj_len)
{
    size_t match_count(0);
    for(int i = 0; i < match.len; i++) {
        int qpos = match.first + i;
        int spos = match.second + i;
        match_count += (qpos >= 0 && qpos < (int)query_len)
                    && (spos >= 0 && spos < (int)subj_len)
                    && query[qpos] == subj[spos];
    }
    return match.len == 0 ? 0 : match_count*1.0/match.len;
}


bool CAdapterSearchApplication::s_MaskQuery(
        const TAligner& aligner,
        char* query_seq,
        size_t query_len,
        double min_coverage,
        double min_identity)
{
    TAligner::SMatch match = aligner.FindSingleBest(query_seq, query_len);
    
    double identity = s_GetIdentity(
            match, 
            query_seq, 
            query_len, 
            aligner.GetSeq().c_str(), 
            aligner.GetSeq().size());

    double coverage = s_GetCoverage(aligner, match, query_len);

    if(coverage >= min_coverage && identity >= min_identity) {
        char* begin = query_seq + match.first;
        transform(begin, begin + match.len, begin, ::tolower);
        return true;
    } else {
        return false;
    }
}


void CAdapterSearchApplication::x_MaskAdaptersSingle(const CArgs& args)
{
    string db_seq;
    {{
        string delim = "";
        for(CBufferedLineReader blr(args["targets"].AsInputFile()); !blr.AtEOF();) {
            const string& seq = *(++blr);
            if(seq.size() > 0 && seq[0] != '>') {
                db_seq += delim + seq;
                delim = "-";
            }
        }
    }}

    NAdapterSearch::CSimpleUngappedAligner aligner;
    aligner.Init(db_seq.c_str(), db_seq.size());

    double min_coverage = args["min_coverage"].AsDouble();
    double min_identity = args["min_identity"].AsDouble();
    size_t masked_count(0), total_count(0);
    for(CBufferedLineReader blr(args["i"].AsInputFile()); !blr.AtEOF();) {
        string query_seq = *(++blr);

        if(query_seq.size() > 0 && query_seq[0] != '>') {
            masked_count += s_MaskQuery(
                    aligner, 
                    &query_seq[0], 
                    query_seq.size(), 
                    min_coverage, 
                    min_identity);
            total_count += 1;
        }
        args["o"].AsOutputFile() << query_seq << "\n";
    }

    LOG_POST("Masked " << masked_count << " reads out of " << total_count);
}


void CAdapterSearchApplication::x_MaskAdaptersPaired(const CArgs& args)
{
    // db of seqs of forward adapters and reverse adapters for paired-end 
    // reads. This is not to be confused with reverse-complemented seq.
    // In this mode an adapter sequence(s) are presumed to be formatted as
    // dash-delimited concatenation of forward and reverse adapters. E.g.
    /*
     * >lcl|adapter
     * AGATCGGAAGAGCGGTTCAGCAGGAATGCCGAGA-AGATCGGAAGAGCGTCGTGTAGGGAAAGAGTG
     */
    string fwd_adapters_seq;
    string rev_adapters_seq; 
    {{
        string delim = "";
        for(CBufferedLineReader blr(args["targets"].AsInputFile()); !blr.AtEOF();) {
            const string& seq = *(++blr);
            if(seq.size() > 0 && seq[0] != '>') {
                string fwd_seq, rev_seq;
                NStr::SplitInTwo(seq, "-", fwd_seq, rev_seq);
                fwd_adapters_seq += delim + fwd_seq;
                rev_adapters_seq += delim + rev_seq;
                delim = "-";
            }
        }
    }}

    // adapters. We'll have separate pattern-finders for 5' and 3' adapters.
    NAdapterSearch::CSimpleUngappedAligner fwd_aligner, rev_aligner;
    fwd_aligner.Init(fwd_adapters_seq.c_str(), fwd_adapters_seq.size());
    rev_aligner.Init(rev_adapters_seq.c_str(), rev_adapters_seq.size());

    double min_coverage = args["min_coverage"].AsDouble();
    double min_identity = args["min_identity"].AsDouble();

    /// seq = fwd_read + rev_read; |fwd_read|=|rev_read|=len/2
    /// Reads are in original orientation ->...<- (as in fastq-dump without
    size_t fwd_count(0), rev_count(0), total_count(0);
    for(CBufferedLineReader blr(args["i"].AsInputFile()); !blr.AtEOF();) {
        string seq = *(++blr);
        
        if(seq.size() > 0 && seq[0] != '>') {
            size_t len = seq.size() / 2;
            char* fwd_read = &seq[0];
            char* rev_read = fwd_read + len;

            fwd_count += s_MaskQuery(
                    fwd_aligner, 
                    fwd_read,
                    len,
                    min_coverage, 
                    min_identity);

            rev_count += s_MaskQuery(
                    rev_aligner, 
                    rev_read,
                    len,
                    min_coverage, 
                    min_identity);

            total_count += 1;
        }

        args["o"].AsOutputFile() << seq << "\n";
    }
    LOG_POST("Masked " << fwd_count << " forward reads and " << rev_count << " reverse reads out of " << total_count);
}


void CAdapterSearchApplication::x_DetectAdapter(const CArgs& args)
{
    bool paired_end_mode = args["paired_end"];

    auto_ptr<NAdapterSearch::IAdapterDetector> detector;
    if(paired_end_mode) {
        detector.reset(new NAdapterSearch::CPairedEndAdapterDetector());
    } else {
        detector.reset(new NAdapterSearch::CUnpairedAdapterDetector());
    }

    for(CBufferedLineReader blr(args["i"].AsInputFile()); !blr.AtEOF();) {
        string seq = *(++blr);
        if(seq.size() > 0 && seq[0] != '>') {
            detector->AddExemplar(seq.c_str(), seq.size());
        }
    }

    string res = detector->InferAdapterSeq();
    if(res != "" && res != "-") {  
        // note: if nothing is detected in paired-end mode,
        // the returned value is ("" + "-" + ""), hence check for "-"
        args["o"].AsOutputFile() << ">lcl|adapter\n" << res << "\n";
    }
}


int CAdapterSearchApplication::Run(void)
{
    const CArgs& args = GetArgs();

    if(args["targets"]) {
        if(args["paired_end"]) {
            x_MaskAdaptersPaired(args);
        } else {
            x_MaskAdaptersSingle(args);
        }
    } else {
        x_DetectAdapter(args);
    }
    return 0;
}


void CAdapterSearchApplication::Exit(void)
{}


int main(int argc, const char* argv[])
{
    return CAdapterSearchApplication().AppMain(argc, argv);
}

