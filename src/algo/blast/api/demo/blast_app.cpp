static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_app.cpp

Author: Ilya Dondoshansky

Contents: C++ driver for running BLAST

******************************************************************************/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

#include <objects/seq/seq__.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_setup.hpp>
#include <algo/blast/api/blast_seq.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_input.hpp>
#include <algo/blast/api/seqsrc_readdb.h>
#include <algo/blast/api/blast_seqalign.hpp>
#include <algo/blast/api/blast_format.hpp>

#ifndef NCBI_C_TOOLKIT
#define NCBI_C_TOOLKIT
#endif

// C include files
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_engine.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CBlastApplication : public CNcbiApplication
{
 public:
     virtual void Init(void);
     virtual int Run(void);
private:
};

void CBlastApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "basic local alignment search tool");

    arg_desc->AddKey("program", "program", "Type of BLAST program",
        CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("program", &(*new CArgAllow_Strings, 
                "blastp", "blastn", "blastx", "tblastn", "tblastx"));
    arg_desc->AddDefaultKey("query", "query", "Query file name",
                     CArgDescriptions::eInputFile, "stdin");
    arg_desc->AddKey("db", "database", "BLAST database name",
                     CArgDescriptions::eString);
    arg_desc->AddDefaultKey("strand", "strand", 
        "Query strands to search: 1 forward, 2 reverse, 0,3 both",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("filter", "filter", "Filtering option",
                            CArgDescriptions::eString, "T");
    arg_desc->AddDefaultKey("lcase", "lcase", "Should lower case be masked?",
                            CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("lookup", "lookup", 
        "Type of lookup table: 0 default, 1 megablast",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("matrix", "matrix", "Scoring matrix name",
                            CArgDescriptions::eString, "BLOSUM62");
    arg_desc->AddDefaultKey("mismatch", "penalty", "Penalty score for a mismatch",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("match", "reward", "Reward score for a match",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("word", "wordsize", "Word size",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("templen", "templen", 
        "Discontiguous word template length",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("templtype", "templtype", 
        "Discontiguous word template type",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("thresh", "threshold", 
        "Score threshold for neighboring words",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("window","window", "Window size for two-hit extension",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("ag", "ag", 
        "Should AG method be used for scanning the database?",
        CArgDescriptions::eBoolean, "T");
    arg_desc->AddDefaultKey("varword", "varword", 
        "Should variable word size be used?",
        CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("stride","stride", "Database scanning stride",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("xungap", "xungapped", 
        "X-dropoff value for ungapped extensions",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("ungapped", "ungapped", 
        "Perform only an ungapped alignment search?",
        CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("greedy", "greedy", 
        "Use greedy algorithm for gapped extensions?",
        CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("gopen", "gapopen", "Penalty for opening a gap",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("gext", "gapext", "Penalty for extending a gap",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("xgap", "xdrop", 
        "X-dropoff value for preliminary gapped extensions",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("xfinal", "xfinal", 
        "X-dropoff value for final gapped extensions with traceback",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("evalue", "evalue", 
        "E-value threshold for saving hits",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("searchsp", "searchsp", 
        "Virtual search space to be used for statistical calculations",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("perc", "percident", 
        "Percentage of identities cutoff for saving hits",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("descr", "descriptions",
        "How many matching sequence descriptions to show?",
        CArgDescriptions::eInteger, "500");
    arg_desc->AddDefaultKey("align", "alignments", 
        "How many matching sequence alignments to show?",
        CArgDescriptions::eInteger, "250");
    arg_desc->AddDefaultKey("out", "out", 
        "File name for writing output",
        CArgDescriptions::eOutputFile, "stdout");
    arg_desc->AddDefaultKey("format", "format", 
        "How to format the results?",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("html", "html", "Produce HTML output?",
                            CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("gencode", "gencode", "Query genetic code",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("dbgencode", "dbgencode", "Database genetic code",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("maxintron", "maxintron", 
                            "Longest allowed intron length for linking HSPs",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("frameshift", "frameshift",
                            "Frame shift penalty (blastx only)",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddOptionalKey("asnout", "seqalignasn", 
        "File name for writing the seqalign results in ASN.1 form",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("qstart", "query_start",
                            "Starting offset in query location",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("qend", "query_end",
                            "Ending offset in query location",
                            CArgDescriptions::eInteger, "0");

    SetupArgDescriptions(arg_desc.release());
}

static int 
CBlast_FillOptions(const CArgs& args, Uint1 program_number,
    LookupTableOptions* lookup_options,
    QuerySetUpOptions* query_options, 
    BlastInitialWordOptions* word_options,
    BlastExtensionOptions* ext_options,
    BlastHitSavingOptions* hit_options,
    BlastScoringOptions* score_options,
    BlastEffectiveLengthsOptions* eff_len_options,
    PSIBlastOptions* psi_options,
    BlastDatabaseOptions* db_options, BlastSeqSrc* bssp)
{
    bool ag_blast = TRUE, variable_wordsize = FALSE;
    int lut;
    bool mb_lookup = FALSE;
    bool greedy_extension = FALSE;
    Int8 totlen = 0;
    int numseqs = 0;
    bool db_is_na, translated_db;
    
    db_is_na = (program_number == blast_type_blastn || 
                program_number == blast_type_tblastn || 
                program_number == blast_type_tblastx);

    translated_db = (program_number == blast_type_tblastn ||
                     program_number == blast_type_tblastx);

    /* The following options are for blastn only */
    if (program_number == blast_type_blastn) {
        if (args["templen"].AsInteger() == 0) {
            ag_blast = args["ag"].AsBoolean();
            lut = args["lookup"].AsInteger();
            if (lut == 1) mb_lookup = TRUE;
            /* Variable word size can only be used for word sizes divisible 
               by 4 */
            if (args["word"].AsInteger() % COMPRESSION_RATIO == 0)
                variable_wordsize = args["varword"].AsBoolean();
        } else {
            /* Discontiguous words */
            ag_blast = FALSE;
            mb_lookup = TRUE;
            variable_wordsize = FALSE;
        }
        greedy_extension = args["greedy"].AsBoolean();
    }

    BLAST_FillLookupTableOptions(lookup_options, program_number, mb_lookup,
        args["thresh"].AsInteger(), args["word"].AsInteger(), 
        ag_blast, variable_wordsize);
    /* Fill the rest of the lookup table options */
    lookup_options->mb_template_length = args["templen"].AsInteger();
    lookup_options->mb_template_type = args["templtype"].AsInteger();

    if (args["stride"])
        lookup_options->scan_step = args["stride"].AsInteger();
    
    BLAST_FillQuerySetUpOptions(query_options, program_number, 
        args["filter"].AsString().c_str(), args["stride"].AsInteger());
    if (args["gencode"].AsInteger() &&
       (program_number == blast_type_blastx || 
        program_number == blast_type_tblastx))
      query_options->genetic_code = args["gencode"].AsInteger();
   
    BLAST_FillInitialWordOptions(word_options, program_number, 
        greedy_extension, args["window"].AsInteger(), variable_wordsize, 
        ag_blast, mb_lookup, args["xungap"].AsDouble());

    BLAST_FillExtensionOptions(ext_options, program_number, greedy_extension, 
        args["xgap"].AsDouble(), args["xfinal"].AsDouble());

    BLAST_FillScoringOptions(score_options, program_number, greedy_extension, 
        args["mismatch"].AsInteger(), args["match"].AsInteger(),
        args["matrix"].AsString().c_str(), args["gopen"].AsInteger(),
        args["gext"].AsInteger());

    score_options->matrix_path = 
       BLASTGetMatrixPath(score_options->matrix, 
                          (program_number != blast_type_blastn));

    if ((score_options->shift_pen = args["frameshift"].AsInteger())) {
       score_options->is_ooframe = TRUE;
    }

    BLAST_FillHitSavingOptions(hit_options, !args["ungapped"].AsBoolean(), 
        args["evalue"].AsDouble(), 
        MAX(args["descr"].AsInteger(), 
            args["align"].AsInteger()));
 
    hit_options->percent_identity = args["perc"].AsDouble();
    hit_options->longest_intron = args["maxintron"].AsInteger();
   
    totlen =  BLASTSeqSrcGetTotLen(bssp);
    numseqs = BLASTSeqSrcGetNumSeqs(bssp);

    BLAST_FillEffectiveLengthsOptions(eff_len_options, numseqs, totlen, 
                                      (Int8)args["searchsp"].AsDouble());
    
   if (args["dbgencode"].AsInteger() && 
       (program_number == blast_type_tblastn || 
        program_number == blast_type_tblastx))
      db_options->genetic_code = args["dbgencode"].AsInteger();

    
    if (translated_db) {
        CSeq_data gc_ncbieaa(
                CGen_code_table::GetNcbieaa(db_options->genetic_code),
                CSeq_data::e_Ncbieaa);
        CSeq_data gc_ncbistdaa;

        TSeqPos nconv = CSeqportUtil::Convert(gc_ncbieaa, &gc_ncbistdaa,
                CSeq_data::e_Ncbistdaa);

        _ASSERT(gc_ncbistdaa.IsNcbistdaa());
        _ASSERT(nconv == gc_ncbistdaa.GetNcbistdaa().Get().size());

        db_options->gen_code_string = (Uint1*) malloc(sizeof(Uint1)*nconv);
        for (unsigned int i = 0; i < nconv; i++)
            db_options->gen_code_string[i] = 
                gc_ncbistdaa.GetNcbistdaa().Get()[i];
    }

    return 0;
}

static CScope* BLASTInitScope(CRef<CObjectManager> &objmgr)
{
    CScope* scope;

    objmgr.Reset(new CObjectManager());
    objmgr->RegisterDataLoader(*new CGBDataLoader("ID", 0, 2),
                               CObjectManager::eDefault);
    
    scope = new CScope(*objmgr);
    scope->AddDefaults();
    _TRACE("Blast2seqApp: Initializing scope");
    return scope;
}

static CBlastOption::EProgram
GetEProgram(Uint1 prog)
{
    if (prog == blast_type_blastp)
        return CBlastOption::eBlastp;
    if (prog == blast_type_blastn)
        return CBlastOption::eBlastn;
    if (prog == blast_type_blastx)
        return CBlastOption::eBlastx;
    if (prog == blast_type_tblastn)
        return CBlastOption::eTblastn;
    if (prog == blast_type_tblastx)
        return CBlastOption::eTblastx;
    return CBlastOption::eBlastUndef;
}

int CBlastApplication::Run(void)
{
    BLAST_SequenceBlk* subject = NULL,* query = NULL;
    SeqLocPtr subject_slp = NULL; /* SeqLoc for the subject sequence in two
                                     sequences case */
    LookupTableOptions* lookup_options;
    QuerySetUpOptions* query_options;
    BlastInitialWordOptions* word_options;
    BlastExtensionOptions* ext_options;
    BlastHitSavingOptions* hit_options;
    BlastScoringOptions* score_options;
    BlastEffectiveLengthsOptions* eff_len_options;
    PSIBlastOptions* psi_options;
    BlastDatabaseOptions* db_options = NULL;
    ListNode* lookup_segments = NULL;
    LookupTableWrap* lookup_wrap;
    int subject_length = 0;
    Uint1 program_number;
    bool db_is_na;
    int status;
    int ctr = 0;
    BlastMask* filter_loc = NULL;	/* Masking/filtering locations */
    TSeqLocVector query_slp;
    BlastScoreBlk* sbp = NULL;
    BlastQueryInfo* query_info;
    BlastResults* results = NULL;
    Blast_Message* blast_message = NULL;
    BlastReturnStat* return_stats;
    char *dbname = NULL;
    Boolean translated_query;
    BlastSeqSrcNewInfo bssn_info;
    ReaddbNewArgs readdb_args;

    // Process command line args
    const CArgs& args = GetArgs();
    
    char *blast_program = strdup(args["program"].AsString().c_str());

    BlastProgram2Number(blast_program, &program_number);

    db_is_na = (program_number == blast_type_blastn || 
                program_number == blast_type_tblastn || 
                program_number == blast_type_tblastx);

    BLAST_InitDefaultOptions(program_number, &lookup_options,
        &query_options, &word_options, &ext_options, &hit_options,
        &score_options, &eff_len_options, &psi_options, &db_options);

    readdb_args.dbname = const_cast<char*>(args["db"].AsString().c_str());
    dbname = strdup(readdb_args.dbname);
    readdb_args.is_protein = !db_is_na;
    
    bssn_info.constructor = &ReaddbSeqSrcNew;
    bssn_info.ctor_argument = (void*) &readdb_args;
    BlastSeqSrc *bssp = BlastSeqSrcNew(&bssn_info);

    CBlast_FillOptions(args, program_number, lookup_options, 
        query_options, word_options, ext_options, hit_options, 
        score_options, eff_len_options, psi_options, db_options, 
        bssp);

    CBlastFormatOptions *format_options = 
        new CBlastFormatOptions(GetEProgram(program_number), 
                                args["out"].AsOutputFile());

    format_options->SetAlignments(args["align"].AsInteger());
    format_options->SetDescriptions(args["descr"].AsInteger());
    format_options->SetAlignView(args["format"].AsInteger());
    format_options->SetHtml(args["html"].AsBoolean());

#ifdef C_FORMATTING
    if (dbname) {
        BLAST_PrintOutputHeader(format_options, args["greedy"].AsBoolean(), 
                                dbname, db_is_na);
    }
#endif

    return_stats = (BlastReturnStat*) calloc(1, sizeof(BlastReturnStat));

    translated_query = (program_number == blast_type_blastx || 
                        program_number == blast_type_tblastx);

    ENa_strand strand = (ENa_strand) args["strand"].AsInteger();
    Int4 from = args["qstart"].AsInteger();
    Int4 to = args["qend"].AsInteger();

    CScope *scope;
    CRef<CObjectManager> objmgr;

    scope = BLASTInitScope(objmgr);

    int id_counter = 0;
    // Read the query(ies) from input file; perform the setup
    if (args["lcase"]) {
        query_slp = BLASTGetSeqLocFromStream(args["query"].AsInputFile(),
                        scope, strand, from, to, &id_counter, &query_options->lcase_mask);
    } else {
        query_slp = BLASTGetSeqLocFromStream(args["query"].AsInputFile(),
                        scope, strand, from, to, &id_counter);
    }

    if (translated_query) {
        /* Translated lower case mask must be converted to protein 
           coordinates here */
        BlastMaskDNAToProtein(&query_options->lcase_mask, query_slp);
    }

    status = BLAST_SetUpQuery(program_number, query_slp, query_options,
                              &query_info, &query);
    
    status = 
        BLAST_MainSetUp(program_number, query_options, score_options, 
            lookup_options, hit_options, query, query_info, 
            &lookup_segments, &filter_loc, &sbp, &blast_message);

    if (translated_query) {
        /* Filter locations were returned in protein coordinates; 
           convert them back to nucleotide here */
        BlastMaskProteinToDNA(&filter_loc, query_slp);
    }
        
    if (status) {
        ERR_POST(Error << "Setup returned status " << status << ".");
        return status;
    }

    BLAST_ResultsInit(query_info->num_queries, &results);
    LookupTableWrapInit(query, lookup_options, lookup_segments, sbp,
                        &lookup_wrap);
    
    BLAST_DatabaseSearchEngine(program_number, query, query_info, 
         bssp, sbp, score_options, lookup_wrap, 
         word_options, ext_options, hit_options, eff_len_options, 
         psi_options, db_options, results, return_stats);

    lookup_wrap = BlastLookupTableDestruct(lookup_wrap);
        
    /* The following works because the ListNodes' data point to simple
       double-integer structures */
    lookup_segments = ListNodeFreeData(lookup_segments);

    // Convert results to the SeqAlign form 
    vector< CConstRef<CSeq_id> > query_seqids;
    int index;

    for (index = 0; index < query_slp.size(); ++index) {
        CConstRef<CSeq_id> id(&sequence::GetId(*query_slp[index].first, 
                                               query_slp[index].second));
        query_seqids.push_back(id);
    }
    
    CRef<CSeq_align_set> seqalign;
    seqalign = BLAST_Results2CppSeqAlign(results, GetEProgram(program_number), query_seqids,
                                         bssp, 0, score_options, sbp);

    results = BLAST_ResultsFree(results);
    
#ifdef C_FORMATTING
    /* Format the results */
    status = BLAST_FormatResults(seqalign, dbname, 
                 blast_program, query_info->num_queries, query_slp,
                 filter_loc, format_options, score_options->is_ooframe);

#endif
    filter_loc = BlastMaskFree(filter_loc);
    
#ifdef C_FORMATTING
    PrintOutputFooter(program_number, format_options, score_options, 
        sbp, lookup_options, word_options, ext_options, hit_options, 
        query_info, bssp, return_stats);
#endif

    query = BlastSequenceBlkFree(query);
    query_info = BlastQueryInfoFree(query_info);
    BlastScoreBlkFree(sbp);

    subject = BlastSequenceBlkFree(subject);
    sfree(return_stats);
    LookupTableOptionsFree(lookup_options);
    BlastQuerySetUpOptionsFree(query_options);
    BlastExtensionOptionsFree(ext_options);
    BlastHitSavingOptionsFree(hit_options);
    BlastInitialWordOptionsFree(word_options);
    BlastScoringOptionsFree(score_options);
    BlastEffectiveLengthsOptionsFree(eff_len_options);
    PSIBlastOptionsFree(psi_options);
    BlastDatabaseOptionsFree(db_options);

    sfree(blast_program);
    sfree(dbname);

    return status;
}


int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
