/* $Id$
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

File name: blast_prog_args.cpp

Author: Jason Papadopoulos

******************************************************************************/

/** @file blast_prog_args.cpp
 * convert blast-related command line
 * arguments into blast options (handles blast-
 * program-specific arguments)
*/

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/composition_adjustment/composition_constants.h>
#include <algo/blast/blastinput/blast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

#define ARG_PROGRAM "p"
#define ARG_DB "d"
#define ARG_MISMATCH "q"
#define ARG_MATCH "r"
#define ARG_THRESHOLD "f"
#define ARG_QGENETIC_CODE "Q"
#define ARG_DBGENETIC_CODE "D"
#define ARG_MATRIX "M"
#define ARG_DBSIZE "z"
#define ARG_STRAND "S"
#define ARG_PSI_CHKPNT "R"
#define ARG_MEGABLAST "n"
#define ARG_FRAMESHIFT "w"
#define ARG_INTRON "t"
#define ARG_COMP_BASED_STATS "C"
#define ARG_SMITH_WATERMAN "s"
#define ARG_GILIST "l"
#define ARG_GAPPED "g"

CBlastallArgs::CBlastallArgs()
    : CBlastArgs("blastall", "Basic Local Alignment Search Tool")
{
}

CArgDescriptions *
CBlastallArgs::SetCommandLine()
{
    CArgDescriptions *arg_desc = new CArgDescriptions;

    x_SetArgDescriptions(arg_desc);

    //----------------------------------------------------------------
    // blast program type
    arg_desc->AddKey(ARG_PROGRAM, "blast_program", 
                    "Type of BLAST program",
                    CArgDescriptions::eString,
                    CArgDescriptions::fOptionalSeparator);
    arg_desc->SetConstraint(ARG_PROGRAM, &(*new CArgAllow_Strings, 
                "blastp", "blastn", "blastx", "tblastn", "tblastx"));
    arg_desc->AddAlias("-program", ARG_PROGRAM);

    // database filename
    arg_desc->AddKey(ARG_DB, "database_name", "BLAST database name",
                     CArgDescriptions::eString,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-database", ARG_DB);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("general search options");

    // DB size
    arg_desc->AddOptionalKey(ARG_DBSIZE, "dbsize", 
                            "Effective length of the database "
                            "(default is the real size)",
                            CArgDescriptions::eDouble,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-dbsize", ARG_DBSIZE);

    // search strands
    arg_desc->AddOptionalKey(ARG_STRAND, "strand", 
                     "Query strands to search against database "
                     "(for blast[nx], and tblastx).  0 or 3 is both "
                     "(the default), 1 is top, 2 is bottom",
                     CArgDescriptions::eInteger,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->SetConstraint(ARG_STRAND, new CArgAllow_Integers(0,3));
    arg_desc->AddAlias("-strand", ARG_STRAND);

    // perform gapped search
    arg_desc->AddOptionalKey(ARG_GAPPED, "gapped", 
                 "Perform gapped alignment (default T, but "
                 "not available for tblastx)",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-gapped", ARG_GAPPED);

    // GI list; no OptionalSeparator in order to 
    // fix conflict with '-logfile' default
    arg_desc->AddOptionalKey(ARG_GILIST, "gilist", 
                            "Restrict search of database to list of GI's",
                            CArgDescriptions::eString);
    arg_desc->AddAlias("-gilist", ARG_GILIST);

    // largest intron length
    arg_desc->AddOptionalKey(ARG_INTRON, "maxintron", 
                    "Length of the largest intron allowed in a translated "
                    "nucleotide sequence when linking multiple distinct "
                    "alignments. (a negative value disables linking)",
                    CArgDescriptions::eInteger,
                    CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-maxintron", ARG_INTRON);

    // composition based statistics
    arg_desc->AddOptionalKey(ARG_COMP_BASED_STATS, "compo", 
                      "Use composition-based statistics for blastp / tblastn:\n"
                      "    D or d: default (equivalent to F)\n"
                      "    0 or F or f: no composition-based statistics\n"
                      "    1 or T or t: Composition-based statistics "
                                      "as in NAR 29:2994-3005, 2001\n"
                      "    2: Composition-based score adjustment as in "
                                      "Bioinformatics 21:902-911,\n"
                      "    2005, conditioned on sequence properties\n"
                      "    3: Composition-based score adjustment as in "
                                      "Bioinformatics 21:902-911,\n"
                      "    2005, unconditionally\n"
                      "For programs other than tblastn, must either be "
                      "absent or be D, F or 0",
                      CArgDescriptions::eString,
                      CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-comp_based_stats", ARG_COMP_BASED_STATS);

    // Smith-Waterman for tblastn
    arg_desc->AddOptionalKey(ARG_SMITH_WATERMAN, "swalign", 
                     "Compute locally optimal Smith-Waterman alignments "
                     "(This option is only available for gapped blastp/"
                     "tblastn, and with composition-based statistics)",
                      CArgDescriptions::eBoolean,
                      CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-swalign", ARG_SMITH_WATERMAN);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("protein search options");

    // lookup table threshold
    arg_desc->AddOptionalKey(ARG_THRESHOLD, "threshold", 
                 "Score threshold for extending hits, default is: "
                 "blastp 11, blastn 0, blastx 12, tblastn 13, "
                 "tblastx 13, megablast 0",
                 CArgDescriptions::eInteger,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-threshold", ARG_THRESHOLD);

    // score matrix
    arg_desc->AddOptionalKey(ARG_MATRIX, "matrix", 
                             "Scoring matrix name (default BLOSUM62)",
                             CArgDescriptions::eString,
                             CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-matrix", ARG_MATRIX);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("nucleotide search options");

    // blastn mismatch penalty
    arg_desc->AddOptionalKey(ARG_MISMATCH, "penalty", 
                 "Penalty a nucleotide mismatch (blastn only; default -3)",
                 CArgDescriptions::eInteger,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-penalty", ARG_MISMATCH);

    // blastn match score
    arg_desc->AddOptionalKey(ARG_MATCH, "reward", 
                 "Reward for a nucleotide match (blastn only; default 1)",
                 CArgDescriptions::eInteger, 
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-reward", ARG_MATCH);

    // use megablast
    arg_desc->AddOptionalKey(ARG_MEGABLAST, "megablast", 
                      "perform a MegaBlast search instead of a blastn "
                      "search, default F",
                      CArgDescriptions::eBoolean,
                      CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-megablast", ARG_MEGABLAST);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("translated search options");

    // query genetic code
    arg_desc->AddOptionalKey(ARG_QGENETIC_CODE, "q_gencode", 
                            "Query genetic code to use (default 1)",
                            CArgDescriptions::eInteger,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-q_gencode", ARG_QGENETIC_CODE);

    // DB genetic code
    arg_desc->AddOptionalKey(ARG_DBGENETIC_CODE, "dbgencode", 
                            "Database genetic code to use (default 1",
                            CArgDescriptions::eInteger,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-db_gencode", ARG_DBGENETIC_CODE);

    // PSI-tblastn checkpoint
    arg_desc->AddOptionalKey(ARG_PSI_CHKPNT, "psi_chkpt_file", 
                             "PSI-TBLASTN checkpoint file",
                             CArgDescriptions::eInputFile,
                             CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-psi_chkpt_file", ARG_PSI_CHKPNT);

    // frame shift penalty for out-of-frame searches
    arg_desc->AddOptionalKey(ARG_FRAMESHIFT, "frameshift",
                            "Frame shift penalty (for use with out-of-frame "
                            "gapped alignment in blastx or tblastn, default "
                            "ignored)",
                            CArgDescriptions::eInteger,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-frameshift", ARG_FRAMESHIFT);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("");

    return arg_desc;
}


string
CBlastallArgs::GetProgram(const CArgs& args)
{
    return args[ARG_PROGRAM].AsString();
}

string
CBlastallArgs::GetDatabase(const CArgs& args)
{
    return args[ARG_DB].AsString();
}

string
CBlastallArgs::GetPsiTblastnCheckpointFile(const CArgs& args)
{
    if (!args[ARG_PSI_CHKPNT])
        return string();
    return args[ARG_PSI_CHKPNT].AsString();
}

string
CBlastallArgs::GetGilist(const CArgs& args)
{
    if (!args[ARG_GILIST])
        return string();
    return args[ARG_GILIST].AsString();
}

ENa_strand
CBlastallArgs::GetQueryStrand(const CArgs& args)
{
    string program = args[ARG_PROGRAM].AsString();
    if (program == "blastp" || program == "tblastn")
        return eNa_strand_unknown;

    ENa_strand strand = eNa_strand_both;
    if (args[ARG_STRAND]) {
        int number = args[ARG_STRAND].AsInteger();
        if (number == 1)
            strand = eNa_strand_plus;
        else if (number == 2)
            strand = eNa_strand_minus;
    }
    return strand;
}

double
CBlastallArgs::GetDBSize(const CArgs& args)
{
    if (!args[ARG_DBSIZE])
        return 0;
    return args[ARG_DBSIZE].AsDouble();
}

int
CBlastallArgs::GetQueryBatchSize(const CArgs& args)
{
    string program = args[ARG_PROGRAM].AsString();
    if (program == "blastn")
        if (args[ARG_MEGABLAST] && args[ARG_MEGABLAST].AsBoolean())
            return 5000000;
        else
            return 40000;
    else if (program == "tblastn")
        return 20000;
    else
        return 10000;
}


CBlastOptionsHandle *
CBlastallArgs::SetOptions(const CArgs& args)
{
    EProgram program = ProgramNameToEnum(args[ARG_PROGRAM].AsString());
    if (program == eBlastn && args[ARG_MEGABLAST] && 
                        args[ARG_MEGABLAST].AsBoolean())
        program = eMegablast;

    CBlastOptionsHandle* opts = CBlastOptionsFactory::Create(program);

    // fill in the options that the base class is responsible for

    x_SetOptions(args, opts);

    CBlastOptions& opt = opts->SetOptions();

    if (args[ARG_MATRIX])
        opt.SetMatrixName(args[ARG_MATRIX].AsString().c_str());
    if (args[ARG_GAPPED] && program != eTblastx)
        opt.SetGappedMode(args[ARG_GAPPED].AsBoolean());
    if (args[ARG_MISMATCH])
        opt.SetMismatchPenalty(args[ARG_MISMATCH].AsInteger());
    if (args[ARG_MATCH])
        opt.SetMatchReward(args[ARG_MATCH].AsInteger());
    if (args[ARG_DBSIZE])
        opt.SetDbLength((Int8) args[ARG_DBSIZE].AsDouble());
    if (args[ARG_QGENETIC_CODE])
        opt.SetQueryGeneticCode(args[ARG_QGENETIC_CODE].AsInteger());
  
    if ((program == eTblastn || program == eTblastx) &&
                        args[ARG_DBGENETIC_CODE]) {
        opt.SetDbGeneticCode(args[ARG_DBGENETIC_CODE].AsInteger());
    }

    // use a threshold if it was specified, otherwise
    // pick a value appropriate to the score matrix and
    // blast program

    if (args[ARG_THRESHOLD]) {
        opt.SetWordThreshold(args[ARG_THRESHOLD].AsInteger());
    }
    else {
        int threshold = 0;
        BLAST_GetSuggestedThreshold(EProgramToEBlastProgramType(program),
                                    opt.GetMatrixName(), &threshold);
        opt.SetWordThreshold(threshold);
    }

    // For the 2-hit wordfinder, if no window size is specified 
    // then choose one appropriate to the score matrix. If using 
    // a 1-hit wordfinder, or the window size was specified, then
    // opt already contains the correct value

    if (GetWordfinder(args) == 0 && GetWindowSize(args) == 0) {
        // window of 0 is the correct default only for blastn; 
        // for non-blastn, a score matrix is always available
        // and the following will default to a window size of 40
        // for any score matrix that is not recognized
        int window = 0;
        BLAST_GetSuggestedWindowSize(EProgramToEBlastProgramType(program),
                                    opt.GetMatrixName(), &window);
        opt.SetWindowSize(window);
    }

    if (args[ARG_INTRON]) {
        if (args[ARG_INTRON].AsInteger() < 0)
            opt.SetSumStatisticsMode(false);
        else
            opt.SetLongestIntronLength(args[ARG_INTRON].AsInteger());
    }
    if (args[ARG_FRAMESHIFT] && args[ARG_FRAMESHIFT].AsInteger() > 0) {
        opt.SetOutOfFrameMode(true);
        opt.SetFrameShiftPenalty(args[ARG_FRAMESHIFT].AsInteger());
    }

    if ((program == eBlastp || program == eTblastn) &&
                                        args[ARG_COMP_BASED_STATS]) {
        const char *comp_stat_string = 
                       args[ARG_COMP_BASED_STATS].AsString().c_str();
        ECompoAdjustModes compo_mode = eNoCompositionBasedStats;;
    
        switch (comp_stat_string[0]) {
            case 'D': case 'd':
            case '0': case 'F': case 'f':
                compo_mode = eNoCompositionBasedStats;
                break;
            case '1': case 'T': case 't':
                compo_mode = eCompositionBasedStats;
                break;
            case '2':
                compo_mode = eCompositionMatrixAdjust;
                break;
            case '3':
                compo_mode = eCompoForceFullMatrixAdjust;
                break;
        }
        opt.SetCompositionBasedStats(compo_mode);
        if (program == eBlastp &&
            compo_mode != eNoCompositionBasedStats &&
            tolower(comp_stat_string[1]) == 'u') {
            opt.SetUnifiedP(1);
        }
        if (args[ARG_SMITH_WATERMAN])
            opt.SetSmithWatermanMode(args[ARG_SMITH_WATERMAN].AsBoolean());
    }

    return opts;
}


#define ARG_GAP_TRIGGER "N"
#define ARG_PROT_QUERY "p"

CRPSBlastArgs::CRPSBlastArgs()
    : CBlastArgs("rpsblast", "Reverse Position-Specific BLAST")
{
}

CArgDescriptions *
CRPSBlastArgs::SetCommandLine()
{
    CArgDescriptions *arg_desc = new CArgDescriptions;

    x_SetArgDescriptions(arg_desc);

    // database filename
    arg_desc->AddKey(ARG_DB, "database_name", "BLAST database name",
                     CArgDescriptions::eString,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-database", ARG_DB);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("general search options");

    // DB size
    arg_desc->AddOptionalKey(ARG_DBSIZE, "dbsize", 
                            "Effective length of the database "
                            "(default is the real size)",
                            CArgDescriptions::eDouble,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-dbsize", ARG_DBSIZE);

    // Gap trigger
    arg_desc->AddOptionalKey(ARG_GAP_TRIGGER, "gap_trigger", 
                            "Number of bits to trigger gapping (default 22.0)",
                            CArgDescriptions::eDouble,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-gap_trigger", ARG_GAP_TRIGGER);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("translated search options");

    // prot/nucleotide query
    arg_desc->AddOptionalKey(ARG_PROT_QUERY, "prot_query", 
                            "Query is protein sequence (default T)",
                            CArgDescriptions::eBoolean,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-prot_query", ARG_PROT_QUERY);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("");

    return arg_desc;
}


string
CRPSBlastArgs::GetDatabase(const CArgs& args)
{
    return args[ARG_DB].AsString();
}

double
CRPSBlastArgs::GetDBSize(const CArgs& args)
{
    if (!args[ARG_DBSIZE])
        return 0;
    return args[ARG_DBSIZE].AsDouble();
}

int
CRPSBlastArgs::GetQueryBatchSize(const CArgs& args)
{
    return 10000;
}


CBlastOptionsHandle *
CRPSBlastArgs::SetOptions(const CArgs& args)
{
    EProgram program = eRPSBlast;
    if (args[ARG_PROT_QUERY] && args[ARG_PROT_QUERY].AsBoolean() == false)
        program = eRPSTblastn;

    CBlastOptionsHandle* opts = CBlastOptionsFactory::Create(program);

    x_SetOptions(args, opts);

    CBlastOptions& opt = opts->SetOptions();

    if (args[ARG_GAP_TRIGGER])
        opt.SetGapTrigger(args[ARG_GAP_TRIGGER].AsDouble());

    if (args[ARG_DBSIZE])
        opt.SetDbLength((Int8) args[ARG_DBSIZE].AsDouble());

    if (GetWordfinder(args) == 0 && GetWindowSize(args) == 0)
        opt.SetWindowSize(40);

    return opts;
}


#define ARG_OUTTYPE "D"
#define ARG_QUERY_BATCH "M"
#define ARG_MINSCORE "s"
#define ARG_FULLID "f"
#define ARG_LOGEND "R"
#define ARG_PERCENT_ID "p"
#define ARG_TEMPL_LEN "t"
#define ARG_TEMPL_TYPE "N"
#define ARG_DISCONTIG_STRIDE1 "g"
#define ARG_NO_GREEDY "n"
#define ARG_MAX_HSP "H"
#define ARG_MASKED_Q "Q"

CMegablastArgs::CMegablastArgs()
    : CBlastArgs("megablast", "Nucleotide BLAST optimized for large sequences")
{
}

CArgDescriptions *
CMegablastArgs::SetCommandLine()
{
    CArgDescriptions *arg_desc = new CArgDescriptions;

    x_SetArgDescriptions(arg_desc);

    // database filename
    arg_desc->AddKey(ARG_DB, "database_name", "BLAST database name",
                     CArgDescriptions::eString,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-database", ARG_DB);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("general search options");

    // DB size
    arg_desc->AddOptionalKey(ARG_DBSIZE, "dbsize", 
                            "Effective length of the database "
                            "(default is the real size)",
                            CArgDescriptions::eDouble, 
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-dbsize", ARG_DBSIZE);

    // mismatch penalty
    arg_desc->AddOptionalKey(ARG_MISMATCH, "penalty", 
                 "Penalty for a nucleotide mismatch (default -3)",
                 CArgDescriptions::eInteger, 
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-penalty", ARG_MISMATCH);

    // match score
    arg_desc->AddOptionalKey(ARG_MATCH, "reward", 
                 "Reward for a nucleotide match (default 1)",
                 CArgDescriptions::eInteger, 
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-reward", ARG_MATCH);

    // query batch size
    arg_desc->AddOptionalKey(ARG_QUERY_BATCH, "batch_size", 
                 "Size of a batch of query sequences searched at "
                 "once (default 5000000)",
                 CArgDescriptions::eInteger,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-batch", ARG_QUERY_BATCH);

    // search strands
    arg_desc->AddOptionalKey(ARG_STRAND, "strand", 
                     "Query strands to search against database; "
                     "0 or 3 is both (the default), 1 is top, 2 is bottom",
                     CArgDescriptions::eInteger,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->SetConstraint(ARG_STRAND, new CArgAllow_Integers(0,3));
    arg_desc->AddAlias("-strand", ARG_STRAND);

    // GI list
    arg_desc->AddOptionalKey(ARG_GILIST, "gilist", 
                            "Restrict search of database to list of GI's",
                            CArgDescriptions::eString,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-gilist", ARG_GILIST);

    // minimum score to report
    arg_desc->AddOptionalKey(ARG_MINSCORE, "min_score", 
                 "Minimal hit score to report",
                 CArgDescriptions::eInteger,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-minscore", ARG_MINSCORE);

    // minimum percent identity
    arg_desc->AddOptionalKey(ARG_PERCENT_ID, "percent_id", 
                 "Identity percentage cut-off",
                 CArgDescriptions::eDouble,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-percent_id", ARG_PERCENT_ID);

    // discontig stride
    arg_desc->AddOptionalKey(ARG_DISCONTIG_STRIDE1, "disc_stride1", 
                 "Generate words for every base of the database or for"
                 "every 4 bases. Default (T) is every base; may only be used "
                 "with discontiguous words",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-disc_stride1", ARG_DISCONTIG_STRIDE1);

    // non-greedy alignment
    arg_desc->AddOptionalKey(ARG_NO_GREEDY, "no_greedy", 
                 "Use non-greedy (dynamic programming) extension "
                 "for affine gap scores (default F)",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-no_greedy", ARG_NO_GREEDY);

    // doscontig. template type
    arg_desc->AddOptionalKey(ARG_TEMPL_TYPE, "template", 
                    "Type of a discontiguous word template (0 - coding, "
                    "1 - optimal, 2 - two simultaneous (default 0)",
                     CArgDescriptions::eInteger,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->SetConstraint(ARG_STRAND, new CArgAllow_Integers(0,2));
    arg_desc->AddAlias("-template", ARG_TEMPL_TYPE);

    // doscontig. template length
    arg_desc->AddOptionalKey(ARG_TEMPL_LEN, "templ_len", 
                    "Length of a discontiguous word template "
                    "(default 0, i.e. contiguous word)",
                     CArgDescriptions::eInteger,
                     CArgDescriptions::fOptionalSeparator);
    arg_desc->SetConstraint(ARG_STRAND, new CArgAllow_Integers(0,2));
    arg_desc->AddAlias("-templ_len", ARG_TEMPL_TYPE);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("output options");

    // output type
    arg_desc->AddOptionalKey(ARG_OUTTYPE, "output_type", 
                        "Type of output (default 2):\n"
                        "0 - alignment endpoints and score\n"
                        "1 - all ungapped segments endpoints\n"
                        "2 - traditional BLAST output\n"
                        "3 - tab-delimited one line format\n"
                        "4 - incremental text ASN.1\n"
                        "5 - incremental binary ASN.1",
                            CArgDescriptions::eInteger,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc->SetConstraint(ARG_OUTTYPE, new CArgAllow_Integers(0,5));
    arg_desc->AddAlias("-out_type", ARG_OUTTYPE);

    // output masked query
    arg_desc->AddOptionalKey(ARG_MASKED_Q, "masked_q_out", 
                 "Output the query sequence after masking is "
                 "applied (default F)",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-masked_q_out", ARG_MASKED_Q);

    // full IDs
    arg_desc->AddOptionalKey(ARG_FULLID, "fullid",
                 "Show full IDs in the output (default is F, "
                 "only GIs or accessions)",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-fullid", ARG_FULLID);

    // log info after output
    arg_desc->AddOptionalKey(ARG_LOGEND, "log_end", 
                 "Report the log information at the end of output (default F)",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-log_end", ARG_LOGEND);

    // max number of HSPs per sequence in output
    arg_desc->AddOptionalKey(ARG_MAX_HSP, "max_hsp", 
                 "Maximal number of HSPs to save per database "
                 "sequence (default is 0, i.e. unlimited)",
                 CArgDescriptions::eInteger,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc->AddAlias("-maxhsp", ARG_MAX_HSP);

    //----------------------------------------------------------------
    arg_desc->SetCurrentGroup("");

    return arg_desc;
}


string
CMegablastArgs::GetDatabase(const CArgs& args)
{
    return args[ARG_DB].AsString();
}

string
CMegablastArgs::GetGilist(const CArgs& args)
{
    if (!args[ARG_GILIST])
        return string();
    return args[ARG_GILIST].AsString();
}

bool
CMegablastArgs::GetMaskedQueryOut(const CArgs& args)
{
    if (!args[ARG_MASKED_Q])
        return false;
    return args[ARG_MASKED_Q].AsBoolean();
}

bool
CMegablastArgs::GetFullId(const CArgs& args)
{
    if (!args[ARG_FULLID])
        return false;
    return args[ARG_FULLID].AsBoolean();
}

bool
CMegablastArgs::GetLogEnd(const CArgs& args)
{
    if (!args[ARG_LOGEND])
        return false;
    return args[ARG_LOGEND].AsBoolean();
}

double
CMegablastArgs::GetDBSize(const CArgs& args)
{
    if (!args[ARG_DBSIZE])
        return 0;
    return args[ARG_DBSIZE].AsDouble();
}

ENa_strand
CMegablastArgs::GetQueryStrand(const CArgs& args)
{
    ENa_strand strand = eNa_strand_both;
    if (args[ARG_STRAND]) {
        int number = args[ARG_STRAND].AsInteger();
        if (number == 1)
            strand = eNa_strand_plus;
        else if (number == 2)
            strand = eNa_strand_minus;
    }
    return strand;
}

int
CMegablastArgs::GetOutputType(const CArgs& args)
{
    if (!args[ARG_OUTTYPE])
        return 2;
    return args[ARG_OUTTYPE].AsInteger();
}

int
CMegablastArgs::GetQueryBatchSize(const CArgs& args)
{
    if (!args[ARG_QUERY_BATCH])
        return 5000000;
    return args[ARG_QUERY_BATCH].AsInteger();
}

CBlastOptionsHandle *
CMegablastArgs::SetOptions(const CArgs& args)
{
    CBlastOptionsHandle* opts = CBlastOptionsFactory::Create(eMegablast);

    x_SetOptions(args, opts);

    CBlastOptions& opt = opts->SetOptions();

    if (args[ARG_DBSIZE])
        opt.SetDbLength((Int8) args[ARG_DBSIZE].AsDouble());

    if (args[ARG_MISMATCH])
        opt.SetMismatchPenalty(args[ARG_MISMATCH].AsInteger());
    if (args[ARG_MATCH])
        opt.SetMatchReward(args[ARG_MATCH].AsInteger());
    if (args[ARG_MINSCORE])
        opt.SetCutoffScore(args[ARG_MINSCORE].AsInteger());
    if (args[ARG_PERCENT_ID])
        opt.SetPercentIdentity(args[ARG_PERCENT_ID].AsDouble());
    if (args[ARG_MAX_HSP])
        opt.SetMaxNumHspPerSequence(args[ARG_MAX_HSP].AsInteger());

    if (args[ARG_NO_GREEDY] && args[ARG_NO_GREEDY].AsBoolean() == true) {
        opt.SetGapExtnAlgorithm(eDynProgScoreOnly);
        opt.SetGapTracebackAlgorithm(eDynProgTbck);
    }

    // For megablast, 1-hit extensions are the default.
    // opt will have 2-hit extensions set by now if 
    // wordfinder == 0 and a window size is specified.
    // 
    // For discontiguous megablast, if the window size
    // was not specified we default to the 2-hit wordfinder
    // with window size 40. If the wordfinder is set to 1 
    // we do nothing (1-hit extensions happen) otherwise
    // ungapped extension is turned off

    opt.SetUngappedExtension(true);
    if (args[ARG_TEMPL_LEN] && args[ARG_TEMPL_LEN].AsInteger() > 0) {
        opt.SetMBTemplateLength(args[ARG_TEMPL_LEN].AsInteger());

        if (args[ARG_TEMPL_TYPE])
            opt.SetMBTemplateType(args[ARG_TEMPL_TYPE].AsInteger());

        if (GetWordfinder(args) == 0) {
            opt.SetUngappedExtension(false);
            if (GetWindowSize(args) == 0)
                opt.SetWindowSize(40);
        }
    }

    return opts;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*---------------------------------------------------------------------
 * $Log$
 * Revision 1.4  2006/12/04 20:50:31  papadopo
 * work around runtime error with '-l'
 *
 * Revision 1.3  2006/09/26 21:44:12  papadopo
 * add to blast scope; add CVS log
 *
 *-------------------------------------------------------------------*/
