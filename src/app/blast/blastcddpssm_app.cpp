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
 * Authors:  Christiam Camacho
 *
 */

/** @file blastp_app.cpp
 * BLASTP command line application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
        "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blastp_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>

#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>

#include <objtools/align_format/showdefline.hpp>

#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/cdd_pssm_input.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/psiblast.hpp>

#include <util/xregexp/regexp.hpp>

#include "blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
USING_SCOPE(align_format);
#endif

class CBlastpApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastpApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// This application's command line args
    CRef<CBlastpAppArgs> m_CmdLineArgs;
};

void CBlastpApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CBlastpAppArgs());

    CArgDescriptions* args = m_CmdLineArgs->SetCommandLine();
    
    args->AddKey("rpsdb", "rpsdb", "RPS database for PSSM computation",
                 CArgDescriptions::eString);

    args->AddDefaultKey("rps_evalue", "evalue", "Maximum evalue for "
                        "included cdds", CArgDescriptions::eDouble, "10");

    args->AddOptionalKey("min_rps_evalue", "evalue", "Minimum evalue for "
                         "CD hits to be included in PSSM computation"
                         "(this option was needed for experiments)",
                         CArgDescriptions::eDouble);

    args->AddDefaultKey("query_num_obsr", "val", "Number of independent "
                        "observations for query, if < 0, query not used "
                        "directly", CArgDescriptions::eDouble, "-1.0");

    args->AddDefaultKey("std_num_obsr", "val", "Number of independent "
                        "observations for standard frequencies to be used as "
                        "another CD match in PSSM computation, (if < 0, "
                        "option not used)",
                        CArgDescriptions::eDouble, "-1.0");
    
    args->AddFlag("boost_query", "Boost query residues so that they have the "
                  " largest frequency in each column for PSSM computation");

    args->AddFlag("short_rps_hits", "Use one-hit ungapped alignment with very "
                  "low xdropoff nad high minimum e-value in RPS-BLAST search "
                  "in order to obtain short RPS-BLAST matches");

    args->AddOptionalKey("rps_xdrop_ungap", "val", "ungapped X-Dropoff "
                         "parameter for RPS-BLAST search",
                         CArgDescriptions::eDouble);

    args->AddOptionalKey("rps_best_hit_overhang", "val", "Best hit overhang "
                         "parameter for RPS-BLAST search",
                         CArgDescriptions::eDouble);

    args->AddOptionalKey("rps_best_hit_score_edge", "val", "Best hit score edge"
                         " parameter for RPS-BLAST search",
                         CArgDescriptions::eDouble);

    args->AddOptionalKey("out_pssm", "file", "Filename to store PSSM",
                         CArgDescriptions::eOutputFile);

    args->AddOptionalKey("out_ascii_pssm", "file", "Filename to store ASCII "
                         "version of PSSM", CArgDescriptions::eOutputFile);

    args->AddFlag("show_cdd_hits", "Display CDD information used for computing "
                  "PSSM");

    args->AddFlag("no_search", "Do not search database, only compute PSSM");

    args->AddOptionalKey("negative_cd_list", "file", "File with list of CD "
                         "accessions that will not be included in PSSM "
                         "calculation", CArgDescriptions::eInputFile);

    args->AddOptionalKey("include_cdd_hits", "regexp", "Regular expression for"
                         " selecting CDD hits", CArgDescriptions::eString);

    args->AddOptionalKey("exclude_cdd_hits", "regexp", "Regular expression for"
                         " excluding CDD hits", CArgDescriptions::eString);

    args->SetDependency("include_cdd_hits", CArgDescriptions::eExcludes,
                        "exclude_cdd_hits");

    args->AddOptionalKey("cdclusters", "file", "File with list of pairs: CD "
                         "accession and cluster index",
                         CArgDescriptions::eInputFile);


    args->SetCurrentGroup("Pre BLASTP search options");

    args->AddFlag("pre_blastp", "Do a fast BLASTP search to find 'obvious "
                  "hits'");

    args->AddDefaultKey("pre_blastp_evalue", "val", "Evalue for preliminary "
                        "BLASTP search", CArgDescriptions::eDouble, "0.01");

    args->AddDefaultKey("pre_blastp_threshold", "val", "Word threshold for "
                        "preliminary BLASTP search", CArgDescriptions::eDouble,
                        "20.0");

    args->AddDefaultKey("pre_blastp_identity", "val", "Precent identity "
                        "threshold for preliminary BLASTP search",
                        CArgDescriptions::eDouble, "30");


    args->SetCurrentGroup("GLOBAL computed alignments options");

    args->AddOptionalKey("global_hits", "file", "File with GLOBAL alignments",
                         CArgDescriptions::eInputFile);

    args->AddOptionalKey("global_pssm", "directory", "Directory with GLOBAL's "
                         "PSSMs and removed columns maps",
                         CArgDescriptions::eString);

    args->AddOptionalKey("rpsid", "file", "File with map: CD seq-id to CD "
                         "accession", CArgDescriptions::eInputFile);

    args->AddOptionalKey("cdd_size", "num", "Number of CDs in CDD",
                         CArgDescriptions::eDouble);


    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
    SetupArgDescriptions(args);
}

const int kAlphabetSize = 28;

static CSearchResultSet s_FindRPSHits(CRef<IQueryFactory> queries,
                                      CRef<CBlastQueryVector> query_vector,
                                      const CBlastOptions& blastopt,
                                      const CArgs& args)

{
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eRPSBlast));

    double rps_evalue = args["rps_evalue"].AsDouble();

    opts->SetEvalueThreshold(rps_evalue);
    opts->SetFilterString("F");

    if (args["short_rps_hits"]) {
        opts->SetWindowSize(0);
        opts->SetGapXDropoff(0.0);
        opts->SetGapXDropoffFinal(0.0);
        opts->SetOptions().SetXDropoff(0.1);
    }

    if (args["rps_xdrop_ungap"]) {
        opts->SetOptions().SetXDropoff(args["rps_xdrop_ungap"].AsDouble());
    }

    if (args["rps_best_hit_overhang"]) {
        opts->SetOptions().SetBestHitOverhang(
                                    args["rps_best_hit_overhang"].AsDouble());
    }

    if (args["rps_best_hit_score_edge"]) {
        opts->SetOptions().SetBestHitScoreEdge(
                                  args["rps_best_hit_score_edge"].AsDouble());
    }

    if (!opts->Validate()) {
        NCBI_THROW(CException, eInvalid, "Validation for RPS-BLAST options "
                   "failed");
    }

    // run RPS blast
    const string rps_db = args["rpsdb"].AsString();
    CSearchDatabase database(rps_db, CSearchDatabase::eBlastDbIsProtein);

    CLocalBlast blaster(queries, opts, database);
    CSearchResultSet results = *blaster.Run();

    if (args["show_cdd_hits"]) {
        CLocalDbAdapter db_adapter(database);
        CRef<CSeqDB> seqdb(new CSeqDB(rps_db, CSeqDB::eProtein));
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        scope->AddDataLoader(RegisterOMDataLoader(seqdb));
        scope->AddScope(*query_vector->GetBlastSearchQuery(0)->GetScope());
        CBlastFormat formatter(blastopt, db_adapter,
                               CFormattingArgs::eFlatQueryAnchoredNoIdentities,
                               false, /*believe query*/
                               NcbiCout,
                               500,   /*num summary*/
                               500,   /*num alignments*/
                               *scope,
                               blastopt.GetMatrixName(),
                               true,  /*show gis*/
                               false  /*display html*/,
                               blastopt.GetQueryGeneticCode(),
                               blastopt.GetDbGeneticCode(),
                               blastopt.GetSumStatisticsMode(),
                               false, /*remote search*/
                               db_adapter.GetFilteringAlgorithm());

        formatter.PrintProlog();
        ITERATE(CSearchResultSet, result, results) {
            formatter.PrintOneResultSet(**result, query_vector);
        }            
        formatter.PrintEpilog(blastopt);
    }

    return results;
}


// Structures for string GLOBAL hits and alignments

struct SRange
{
    int from;
    int to;

    SRange(int f, int t) : from(f), to(t) {}
};

struct SSegment
{
    SRange query;
    SRange subject;

    SSegment(const SRange& q, const SRange& s) : query(q), subject(s) {}
};

struct SGlobalHit 
{
    string subject;
    double pvalue;

    vector<SSegment> alignment;    
};

// The function works only for a single query
static CSearchResultSet s_GetGlobalHits(CRef<IQueryFactory> queries,
                                        CRef<CBlastQueryVector> query_vector,
                                        const CBlastOptions& blastopt,
                                        const CArgs& args)
{
    CRef<CSeq_align_set> seq_align_set(new CSeq_align_set());

    vector<SGlobalHit> global_hits;
    vector<string> tokens;

    const int buff_size = 256;
    char* buff = new char[buff_size];

    if (args["show_cdd_hits"]) {
        NcbiCout << "MSA columns removed by GLOBAL:" << NcbiEndl;
    }

    // Read GLOBAL alignments
    CNcbiIstream& aln_stream = args["global_hits"].AsInputFile();
    while (!aln_stream.eof()) {
        aln_stream.getline(buff, buff_size);
        
        if (buff[0] == 0) {
            continue;
        }

        tokens.clear();
        NStr::Tokenize(buff, " \t", tokens, NStr::eMergeDelims);

        // each hit starts with line: CD: accession ...
        if (tokens.size() < 2 || tokens[0] != "CD:") {
            NCBI_THROW(CException, eInvalid, "Error reading GLOBAL hits");
        }
        global_hits.push_back(SGlobalHit());
        SGlobalHit& hit = global_hits.back();
        hit.subject = tokens[1];
        hit.pvalue = NStr::StringToDouble(tokens[5]);

        // read removed columns map for PSSM
        string fname = args["global_pssm"].AsString() + "/" + hit.subject
            + ".txt.pos";

        auto_ptr<CNcbiIfstream> pos_stream(new CNcbiIfstream(fname.c_str()));

        if (!*pos_stream) {
            NCBI_THROW(CException, eInvalid, "GLOBAL's columns map file "
                       + fname + " not found");
        }

        if (args["show_cdd_hits"]) {
            NcbiCout << hit.subject << ": ";
        }

        vector<int> num_removed_cols;
        int num_cols;
        *pos_stream >> num_cols;
        int num_removed = 0;
        for (int i=0;i < num_cols;i++) {
            int col;
            *pos_stream >> col;
            if (col) {
                num_removed_cols.push_back(num_removed);
            }
            else {
                num_removed++;

                if (args["show_cdd_hits"]) {
                    NcbiCout << i << ", ";
                }
            }
        }
        if (args["show_cdd_hits"]) {
            NcbiCout << NcbiEndl;
        }
        
        // skip header for alignment columns
        aln_stream.getline(buff, buff_size);
        if (buff[0] != 'B') {
            NCBI_THROW(CException, eInvalid, "Error reading GLOBAL hits");
        }
        
        aln_stream.getline(buff, buff_size);
        for (; buff[0] != 0 && !aln_stream.eof();
             aln_stream.getline(buff, buff_size)) {

            tokens.clear();
            NStr::Tokenize(buff, "\t ", tokens);

            if (tokens[5] != "S" && tokens[5] != "G") {
                NCBI_THROW(CException, eInvalid, "Error reading GLOBAL hits");
            }

            if (tokens[5] == "G") {
                continue;
            }

            int query_start = NStr::StringToInt(tokens[3]);
            int subject_start = NStr::StringToInt(tokens[2]);
            string q_end = tokens[3];
            string s_end = tokens[2];

            aln_stream.getline(buff, buff_size);
            tokens.clear();
            NStr::Tokenize(buff, "\t ", tokens);
            while (buff[0] != 0 && !aln_stream.eof() && tokens[5] == "S") {
                if (tokens.size() < 6) {
                    NCBI_THROW(CException, eInvalid,
                               "Error reading GLOBAL hits");                    
                }

                q_end = tokens[3];
                s_end = tokens[2];

                aln_stream.getline(buff, buff_size);
                tokens.clear();
                NStr::Tokenize(buff, "\t ", tokens);
            }

            int query_end = NStr::StringToInt(q_end);
            int subject_end = NStr::StringToInt(s_end);

            if (query_end < query_start || subject_end < subject_start) {
                NCBI_THROW(CException, eInvalid,
                           "Error reading GLOBAL alignment");
            }

            //apply offsets due to removed columns in GLOBAL's PSSMs
            subject_start += num_removed_cols[subject_start];
            subject_end += num_removed_cols[subject_end];

            // create alignment segment
            hit.alignment.push_back(SSegment(SRange(query_start, query_end),
                                            SRange(subject_start, subject_end)));

        }        

    }
    if (args["show_cdd_hits"]) {
        NcbiCout << NcbiEndl;
    }

    map<string, string> cdid;
    CNcbiIstream& id_stream = args["rpsid"].AsInputFile();
    while (!id_stream.eof()) {
        id_stream.getline(buff, buff_size);

        if (buff[0] == 0) {
            continue;
        }

        tokens.clear();
        NStr::Tokenize(buff, "\t ", tokens);
        cdid[tokens[1]] = tokens[0];
    }

    const double kNumCds = args["cdd_size"].AsDouble();
    // 37014.0 for CDD ver 2.21;

    // Create Seq-aligns and CSearchResultSet
    ITERATE (vector<SGlobalHit>, hit, global_hits) {
        CRef<CSeq_align> seq_align(new CSeq_align());

        seq_align->SetType(CSeq_align::eType_partial);
        seq_align->SetDim(2);

        // assign e-value
        CRef<CScore> score(new CScore());
        score->SetId().SetStr("e_value");
        score->SetValue().SetReal(hit->pvalue * kNumCds);
        seq_align->SetScore().push_back(score);

        // set denseg
        CDense_seg& denseg = seq_align->SetSegs().SetDenseg();
        denseg.SetDim(2);

        // set query seq-id
        CSeq_id* query_id = const_cast<CSeq_id*>(
                        (*query_vector)[0]->GetQueryId().GetNonNullPointer());

        denseg.SetIds().push_back(CRef<CSeq_id>(query_id));

        // set subject seq-id
        denseg.SetIds().push_back(CRef<CSeq_id>(
                                            new CSeq_id(cdid[hit->subject])));


        // set alignment segments
        CDense_seg::TStarts& starts = denseg.SetStarts();
        CDense_seg::TLens& lens = denseg.SetLens();

        ITERATE (vector<SSegment>, it, hit->alignment) {

            if (starts.size() > 0) {
                vector<SSegment>::const_iterator prev(it - 1);
                int q_gaps = it->query.from - prev->query.to - 1;
                if (q_gaps > 0) {
                    starts.push_back(prev->query.to + 1);
                    starts.push_back(-1);
                    lens.push_back(q_gaps);
                }

                int s_gaps = it->subject.from - prev->subject.to - 1;
                if (s_gaps > 0) {
                    starts.push_back(-1);
                    starts.push_back(prev->subject.to + 1);
                    lens.push_back(s_gaps);
                }
            }

            starts.push_back(it->query.from);
            starts.push_back(it->subject.from);

            if (it->query.to - it->query.from
                != it->subject.to - it->subject.from) {
                NCBI_THROW(CException, eInvalid, "Bad GLOBAL alignment");
            }

            lens.push_back(it->query.to - it->query.from + 1);
        }
        denseg.SetNumseg(lens.size());
        seq_align_set->Set().push_back(seq_align);
    }

    delete [] buff;


    // If there were hits create standard search results
    if (!seq_align_set->IsEmpty()) {
        TSeqAlignVector seq_align_vector;
        seq_align_vector.push_back(seq_align_set);
        TSearchMessages msg;
        msg.push_back(TQueryMessages());
        CSearchResultSet result = CSearchResultSet(seq_align_vector, msg);

        return result;
    }
    // If there are no hits create empty results with query-id
    else {
        CRef<CSearchResults> search_result(new CSearchResults(
                                            (*query_vector)[0]->GetQueryId(),
                                            seq_align_set,
                                            TQueryMessages(),
                                            CRef<CBlastAncillaryData>()));

        CSearchResultSet result;
        result.push_back(search_result);
        
        return result;
    }
}


static void s_LoadIdList(CNcbiIstream& istr,
                         vector<string>& seqids)
{
    while (!istr.eof()) {
        string id = "";
        istr >> id;
        if (id.empty() || istr.eof()) {
            break;
        }
        seqids.push_back(id);
    }
}

static void s_LoadCdClusters(CNcbiIstream& istr, map<string, int>& clusters)
{
    while (!istr.eof()) {
        string acc = "";
        int index;

        istr >> acc;
        if (acc.empty() || istr.eof()) {
            break;
        }
        istr >> index;
        clusters[acc] = index;
    }
}

static CConstRef<CSeq_align_set> s_RemoveNegativeCdHits(
                                     CConstRef<CSeq_align_set> hits,
                                     vector<string>& negative_list,
                                     CRef<CScope> scope,
                                     bool verbose)
{
    CSeq_align_set* retval = new CSeq_align_set();
    list< CRef<CSeq_align> >& seqaligns = retval->Set();

    if (verbose) {
        NcbiCout << NcbiEndl;
    }
    
    sort(negative_list.begin(), negative_list.end());
    ITERATE(list< CRef<CSeq_align> >, it, hits->Get()) {
       const CSeq_id& seqid = *(*it)->GetSegs().GetDenseg().GetIds()[1];

       CBioseq_Handle bhandle = scope->GetBioseqHandle(seqid);
       list< CRef<CSeq_id> > ids;
       CShowBlastDefline::GetSeqIdList(bhandle, ids);

       // CDD accessions have a comma at the end
       string accession = CShowBlastDefline::GetSeqIdListString(ids, true);    
       accession = accession.substr(0, accession.length() - 1);

       bool match = binary_search(negative_list.begin(), negative_list.end(),
                                  accession);

       if (!match) {
           seqaligns.push_back(*it);
       }
       else if (verbose) {
               NcbiCout << "Removed CDD hit to: "
                        << accession << NcbiEndl;
       }
    }
    return CConstRef<CSeq_align_set>(retval);
}

/// Remove CDD hits based on matching/not-matching regular expressions
static CConstRef<CSeq_align_set> s_RemoveCdHits(CConstRef<CSeq_align_set> hits,
                                                CRegexp& re,
                                                CRef<CScope> scope,
                                                bool exclude,
                                                bool verbose)
{
    CSeq_align_set* retval = new CSeq_align_set();
    list< CRef<CSeq_align> >& seqaligns = retval->Set();

    if (verbose) {
        NcbiCout << NcbiEndl;
    }

    ITERATE(list< CRef<CSeq_align> >, it, hits->Get()) {
       const CSeq_id& seqid = *(*it)->GetSegs().GetDenseg().GetIds()[1];

       CBioseq_Handle bhandle = scope->GetBioseqHandle(seqid);
       list< CRef<CSeq_id> > ids;
       CShowBlastDefline::GetSeqIdList(bhandle, ids);

       // CDD accessions have a comma at the end
       string accession = CShowBlastDefline::GetSeqIdListString(ids, true);    
       accession = accession.substr(0, accession.length() - 1);

       bool match = re.IsMatch(accession);

       if (match && !exclude || !match && exclude) {
           seqaligns.push_back(*it);           
       }
       else if (verbose) {
               NcbiCout << "Removed CDD hit to: "
                        << accession << NcbiEndl;
       }
    }
    
    return CConstRef<CSeq_align_set>(retval);
}


static unsigned int s_GetAlignmentLength(const CSeq_align& align)
{
    int result = 0;

    ITERATE (vector<unsigned int>, it, align.GetSegs().GetDenseg().GetLens()) {
        result += *it;
    }

    return result;
}

static bool compare_seqaligns_by_id(const CRef<CSeq_align>* a,
                                    const CRef<CSeq_align>* b)
{
    const int kSubjectRow = 1;
    return (*a)->GetSeq_id(kSubjectRow).CompareOrdered(
                                      (*b)->GetSeq_id(kSubjectRow)) < 0;
}

static bool compare_seqaligns_by_id_length(const CRef<CSeq_align>* a,
                                           const CRef<CSeq_align>* b)
{
    const int kSubjectRow = 1;
    int id = (*a)->GetSeq_id(kSubjectRow).CompareOrdered(
                                           (*b)->GetSeq_id(kSubjectRow));

    if (id == 0) {
        unsigned int len_a = s_GetAlignmentLength(**a);
        unsigned int len_b = s_GetAlignmentLength(**b);

        return len_a > len_b;
    }
    else {
        return id < 0;
    }
}

static bool compare_seqaligns_by_evalue(const CRef<CSeq_align>& a,
                                        const CRef<CSeq_align>& b)
{
    double evalue_a, evalue_b;
    if (!a->GetNamedScore(CSeq_align::eScore_EValue, evalue_a)
        || !b->GetNamedScore(CSeq_align::eScore_EValue, evalue_b)) {

        NCBI_THROW(CException, eInvalid, "E-value not set in Seq-align");
    }

    return evalue_a < evalue_b;
}


// Combine search results from blastp and deltablast
static CRef<CSearchResultSet> s_CombineResults(
                                 CRef<CSearchResultSet> pre_results_set,
                                 CRef<CSearchResultSet> results_set)
{
    const int kSubjectRow = 1;

    // TO DO: Multiple hits hits to the same subject need to be analyzed

    CSeq_align_set* pre_seqalignset = const_cast<CSeq_align_set*>(
                      (*pre_results_set)[0].GetSeqAlign().GetNonNullPointer());


    // remove mutliple blastp hits to the same subject
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, it, pre_seqalignset->Set()) {
        list< CRef<CSeq_align> >::iterator from(it);
        from++;

        if (from != pre_seqalignset->Set().end()
            && (*it)->GetSeq_id(kSubjectRow)
            .Match((*from)->GetSeq_id(kSubjectRow))) {

            list< CRef<CSeq_align> >::iterator to(from);
            to++;
            while (to != pre_seqalignset->Set().end()
                   && (*it)->GetSeq_id(kSubjectRow)
                   .Match((*to)->GetSeq_id(kSubjectRow))) {

                to++;
            }
            pre_seqalignset->Set().erase(from, to);
        }

    }

    CSeq_align_set* seqalignset = const_cast<CSeq_align_set*>(
                        (*results_set)[0].GetSeqAlign().GetNonNullPointer());

    // create vector of pointers to deltablast CRef<CSeq_align> and sort them
    // by 
    // subject id
    vector< CRef<CSeq_align>* > aligns;
    aligns.reserve(seqalignset->Get().size());
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, it, seqalignset->Set()) {

        aligns.push_back(&(*it));
    }
    sort(aligns.begin(), aligns.end(), compare_seqaligns_by_id_length);

    // for each blastp hit
    ITERATE (list< CRef<CSeq_align> >, pre_hit,
             (*pre_results_set)[0].GetSeqAlign()->Get()) {

        // find deltablast hit with the same subject id
        vector< CRef<CSeq_align>* >::iterator hit
            = lower_bound(aligns.begin(), aligns.end(), &(*pre_hit),
                          compare_seqaligns_by_id);

        // if subject id matches in both hits
        if (hit != aligns.end() && (**hit)->GetSeq_id(kSubjectRow)
            .Match((*pre_hit)->GetSeq_id(kSubjectRow))) {

            // TO DO: what if there are multiple hits to the same subject?

            // TO DO: first check if hit ranges match

            // find alignment lengths for both hits
            unsigned int pre_len = s_GetAlignmentLength(**pre_hit);
            unsigned int len = s_GetAlignmentLength(***hit);

            // if blastp hit has longer alignment
            if (pre_len > len) {

                // then replace deltablast hit with the blastp hit

                // this replacement does not affect ordering of aligns, since
                // both hits have the same subject id

                (*hit)->Reset(
                      const_cast<CSeq_align*>(pre_hit->GetNonNullPointer()));

            }
        }
        else {

            // if there is no deltablast hit with blastp's subject id, then
            // add blastp hit to deltablast results

            seqalignset->Set().push_front(*pre_hit);
        }
    }

    // TO DO: group results by subject id

    // sort results by e-value
    seqalignset->Set().sort(compare_seqaligns_by_evalue);

    return results_set;
}

int CBlastpApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
        RecoverSearchStrategy(args, m_CmdLineArgs);
        CRef<CBlastOptionsHandle> opts_hndl(&*m_CmdLineArgs->SetOptions(args));
        const CBlastOptions& opt = opts_hndl->GetOptions();

        /*** Get the query sequence(s) ***/
        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();
        SDataLoaderConfig dlconfig(query_opts->QueryIsProtein());
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        CBlastInputSourceConfig iconfig(dlconfig, query_opts->GetStrand(),
                                     query_opts->UseLowercaseMasks(),
                                     query_opts->GetParseDeflines(),
                                     query_opts->GetRange(),
                                     !m_CmdLineArgs->ExecuteRemotely());
        CBlastFastaInputSource fasta(m_CmdLineArgs->GetInputStream(), iconfig);
        CBlastInput input(&fasta, m_CmdLineArgs->GetQueryBatchSize());

        /*** Initialize the database/subject ***/
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
        CRef<CLocalDbAdapter> db_adapter;
        CRef<CScope> scope;
        InitializeSubject(db_args, opts_hndl, m_CmdLineArgs->ExecuteRemotely(),
                         db_adapter, scope);
        _ASSERT(db_adapter && scope);

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        int num_descriptions = fmt_args->GetNumDescriptions();
        int num_alignments = fmt_args->GetNumAlignments();
        if (args["pre_blastp"]) {
            num_descriptions *= 2;
            num_alignments *= 2;
        }
        CBlastFormat formatter(opt, *db_adapter,
                               fmt_args->GetFormattedOutputChoice(),
                               query_opts->GetParseDeflines(),
                               m_CmdLineArgs->GetOutputStream(),
                               num_descriptions,
                               num_alignments,
                               *scope,
                               opt.GetMatrixName(),
                               fmt_args->ShowGis(),
                               fmt_args->DisplayHtmlOutput(),
                               opt.GetQueryGeneticCode(),
                               opt.GetDbGeneticCode(),
                               opt.GetSumStatisticsMode(),
                               m_CmdLineArgs->ExecuteRemotely(),
                               db_adapter->GetFilteringAlgorithm(),
                               fmt_args->GetCustomOutputFormatSpec());
        
        if (!args["no_search"]) {
            formatter.PrintProlog();
        }

        // variables for string run times
        double time_rpsblast = 0.0;
        double time_rpsfilter = 0.0;
        double time_pssm_engine = 0.0;
        double time_preblastp = 0.0;
        double time_psiblast = 0.0;
        double time_combine = 0.0;
        double time_format = 0.0;

        CRef<objects::CPssmWithParameters> pssm;

        /*** Process the input ***/
        for (; !input.End(); formatter.ResetScopeHistory()) {

            CRef<CBlastQueryVector> query_batch(input.GetNextSeqBatch(*scope));
            CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(*query_batch));

            SaveSearchStrategy(args, m_CmdLineArgs, queries, opts_hndl);

            CRef<CSearchResultSet> results;

            if (m_CmdLineArgs->ExecuteRemotely()) {

                NCBI_THROW(CException, eInvalid, "CDD-based PSSM computation "
                           "is currently not supported for remote BLAST");

                CRef<CRemoteBlast> rmt_blast = 
                    InitializeRemoteBlast(queries, db_args, opts_hndl,
                          m_CmdLineArgs->ProduceDebugRemoteOutput(),
                          m_CmdLineArgs->GetClientId());
                results = rmt_blast->GetResultSet();
            } else {
                

                // set names of all data files (as rps database name with
                // appropriate extensions)
                string rps_db = args["rpsdb"].AsString();

                // Run rps-blast
                CSearchResultSet rps_hits;
                if (!args["global_hits"]) {

                    CStopWatch timer;
                    timer.Start();

                    rps_hits = s_FindRPSHits(queries, query_batch,
                                             opt, args);

                    time_rpsblast = timer.Elapsed();
                }
                else {
                    rps_hits = s_GetGlobalHits(queries, query_batch, opt,
                                               args);
                }

                CDeltaBlastOptions cdd_opts;
                // this sets default options
                DeltaBlastOptionsNew(&cdd_opts);
                cdd_opts->inclusion_ethresh = args["rps_evalue"].AsDouble();
                cdd_opts->query_num_observations
                    = args["query_num_obsr"].AsDouble();
                cdd_opts->std_num_observations
                    = args["std_num_obsr"].AsDouble();
                cdd_opts->boost_query = args["boost_query"];

                // Create pssm input data for CDs

                // iterate over rps-blast queries and create separate input
                // for each
                CSearchResultSet::const_iterator it = rps_hits.begin();
                
                // set up scope for CDD
                CRef<CSeqDB> seqdb(new CSeqDB(args["rpsdb"].AsString(),
                                              CSeqDB::eProtein));

                CRef<CScope> cdd_scope(
                                new CScope(*CObjectManager::GetInstance()));

                cdd_scope->AddDataLoader(RegisterOMDataLoader(seqdb));
                cdd_scope->AddScope(
                          *query_batch->GetBlastSearchQuery(0)->GetScope());


                CStopWatch timer;
                timer.Start();

                // load negative CD list and remove RPS-BLAST hits that match
                CConstRef<CSeq_align_set> seqalignset;
                if (args["negative_cd_list"]) {
                    vector<string> negative_list;
                    s_LoadIdList(args["negative_cd_list"].AsInputFile(),
                                 negative_list);

                    seqalignset = s_RemoveNegativeCdHits((*it)->GetSeqAlign(),
                                                         negative_list,
                                                         cdd_scope,
                                                         args["show_cdd_hits"]);
                }
                else {
                    seqalignset = (*it)->GetSeqAlign().GetNonNullPointer();
                }                
                
                // remove CDD hits based on regular expressions
                if (args["include_cdd_hits"] || args["exclude_cdd_hits"]) {
                    bool exclude = args["exclude_cdd_hits"];
                    CRegexp re(exclude ? args["exclude_cdd_hits"].AsString()
                               : args["include_cdd_hits"].AsString());

                    seqalignset = s_RemoveCdHits(seqalignset,
                                                 re,
                                                 cdd_scope,
                                                 exclude,
                                                 args["show_cdd_hits"]);
                }
                time_rpsfilter = timer.Elapsed();

                CPSIDiagnosticsRequest diags;
                diags.Reset(PSIDiagnosticsRequestNewEx(true));
                diags->independent_observations = true;

                CRef<CCddInputData> pssm_input(
                                     new CCddInputData((*it)->GetSeqId(),
                                                       seqalignset,
                                                       cdd_scope,
                                                       *cdd_opts.Get(),
                                                       rps_db,
                                                       opt.GetMatrixName(),
                                                       diags,
                                                       args["show_cdd_hits"]));

                pssm_input->SetMinCddEvalue(-1.0);

                if (args["short_rps_hits"]) {
                    pssm_input->SetMinCddEvalue(10.0);
                }

                if (args["min_rps_evalue"]) {
                    pssm_input->SetMinCddEvalue(
                                        args["min_rps_evalue"].AsDouble());
                }

                // set CD clusters
                if (args["cdclusters"]) {
                    s_LoadCdClusters(args["cdclusters"].AsInputFile(),
                                     pssm_input->SetCdClusters());
                }

                CRef<CPssmEngine> pssm_engine(
                          new CPssmEngine(pssm_input.GetNonNullPointer()));

                timer.Start();                

                // compute pssm
                pssm = pssm_engine->Run();
                
                time_pssm_engine = timer.Elapsed();

                if (args["out_pssm"]) {
                    args["out_pssm"].AsOutputFile() << MSerial_AsnText << *pssm;
                }

                if (args["show_cdd_hits"]) {

                    const list<double>& information_content = pssm->GetPssm()
                        .GetIntermediateData().GetInformationContent();

                    double sum = 0.0;
                    int num = 0;
                    ITERATE (list<double>, it, information_content) {
                        sum += *it;
                        num++;
                    }
                    double average_information = sum / (double)num;

                    m_CmdLineArgs->GetOutputStream() << NcbiEndl
                               << "PSSM average information content: "
                               << average_information
                               << NcbiEndl;
                }


                
                // Save pssm
                if (args["out_ascii_pssm"]) {
                    // extract ancillaru data for printing ascii pssm
                    // (from psiblast_app.cpp)
                    _ASSERT(pssm->CanGetPssm());
                    pair<double, double> lambda, k, h;
                    lambda.first = pssm->GetPssm().GetLambdaUngapped();
                    lambda.second = pssm->GetPssm().GetLambda();
                    k.first = pssm->GetPssm().GetKappaUngapped();
                    k.second = pssm->GetPssm().GetKappa();
                    h.first = pssm->GetPssm().GetHUngapped();
                    h.second = pssm->GetPssm().GetH();
                    CRef<CBlastAncillaryData> ancillary_data(
                          new CBlastAncillaryData(lambda, k, h, 0, true));

                    // information content trigers computation of other
                    // statistics in PrintAsciiPssm that cannot be computed
                    // for PSSM computed from CDs
                    pssm->SetPssm().SetIntermediateData()
                        .SetInformationContent().clear();

                    CBlastFormatUtil::PrintAsciiPssm(*pssm, 
                                        ancillary_data,
                                        args["out_ascii_pssm"].AsOutputFile());
                }

                if (!args["no_search"]) {

                    // Run stringent blastp to find 'obvious hits'
                    CRef<CSearchResultSet> pre_results;
                    if (args["pre_blastp"]) {
                        CRef<CBlastOptionsHandle> pre_opts(
                              new CBlastProteinOptionsHandle(
                                                 CBlastOptions::eLocal));

                        pre_opts->SetEvalueThreshold(
                                        args["pre_blastp_evalue"].AsDouble());

                        pre_opts->SetOptions().SetWordThreshold(
                                      args["pre_blastp_threshold"].AsDouble());

                        pre_opts->SetOptions().SetPercentIdentity(
                                       args["pre_blastp_identity"].AsDouble());
                        
                        CStopWatch timer;
                        timer.Start();

                        CLocalBlast lcl_blast(queries, pre_opts, db_adapter);
                        pre_results = lcl_blast.Run();

                        time_preblastp = timer.Elapsed();
                    }

                    // Run psiblast with computed pssm
                    CRef<CPSIBlastOptionsHandle> psiblast_opts(
                            new CPSIBlastOptionsHandle(CBlastOptions::eLocal));

                    psiblast_opts->SetOptions().SetWordThreshold(
                                    opts_hndl->GetOptions().GetWordThreshold());

                    psiblast_opts->SetOptions().SetCompositionBasedStats(
                         opts_hndl->GetOptions().GetCompositionBasedStats());

                    psiblast_opts->SetOptions().SetHitlistSize(
                                   opts_hndl->GetOptions().GetHitlistSize());

                    CRef<CPsiBlast> psiblast(new CPsiBlast(pssm, db_adapter,
                                                           psiblast_opts));

                    timer.Start();

                    results = psiblast->Run();

                    time_psiblast = timer.Elapsed();

                    if (args["pre_blastp"]) {

                        CStopWatch timer;
                        timer.Start();

                        results = s_CombineResults(pre_results, results);

                        time_combine = timer.Elapsed();
                    }

                // Run blastp
//                CLocalBlast lcl_blast(queries, opts_hndl, db_adapter);
//                lcl_blast.SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
//                results = lcl_blast.Run();
                }

            }

            CStopWatch timer;
            timer.Start();
            if (!args["no_search"]) {
                ITERATE(CSearchResultSet, result, *results) {
                    formatter.PrintOneResultSet(**result, query_batch);
                }            
            }
            time_format = timer.Elapsed();
            
            // Work only with the first query
            break;
        }

        if (!args["no_search"]) {
            formatter.PrintEpilog(opt);

            // print number effective number of independent observations for 
            // PSSM columns
            CNcbiOstream& out_stream = m_CmdLineArgs->GetOutputStream();
            if (pssm->GetPssm().GetIntermediateData().CanGetNumIndeptObsr()) {
                out_stream << NcbiEndl << "PSSM_ind_obsr:";
                out_stream << setiosflags(ios::fixed) << setprecision(2);
                ITERATE (list<double>, it,
                  pssm->GetPssm().GetIntermediateData().GetNumIndeptObsr()) {

                    out_stream << setw(8) << *it;
                }
                out_stream << NcbiEndl;
            }
        }

        NcbiCerr << "DELTA-BLAST Times [s]:" << NcbiEndl
                 << "RPS-BLAST search: " << time_rpsblast << NcbiEndl
                 << "RPS-BLAST filtering: " << time_rpsfilter << NcbiEndl
                 << "PSSM engine: " << time_pssm_engine << NcbiEndl
                 << "PSI-BLAST search: " << time_psiblast << NcbiEndl
                 << "Forrmatting: " << time_format << NcbiEndl;

        if (args["pre_blastp"]) {
                NcbiCerr << "Pre-BLASTP: " << time_preblastp << NcbiEndl
                         << "Combine results: " << time_combine << NcbiEndl;
        }

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

    } CATCH_ALL(status)
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastpApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
