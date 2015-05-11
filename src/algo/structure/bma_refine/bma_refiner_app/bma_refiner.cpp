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
 * Authors:  Chris Lanczycki
 *
 * File Description:
 *       Block multiple alignment refiner application.  
 *       (formerly named AlignRefineApp2.cpp)
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/cdd/Cdd.hpp>
#include <algo/structure/bma_refine/RefinerEngine.hpp>

#include "bma_refiner.hpp"

//  hack to define namespace in using declaration in include file
BEGIN_SCOPE(cd_utils)
END_SCOPE(cd_utils)
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(align_refine)

const unsigned int CAlignmentRefiner::N_MAX_TRIALS = 500;
const unsigned int CAlignmentRefiner::N_MAX_CYCLES = 200;
const unsigned int CAlignmentRefiner::N_MAX_ROWS   = 2000;

static bool ReadCD(const string& filename, CCdd *cdd)
{
    // try to decide if it's binary or ascii
    auto_ptr<CNcbiIstream> inStream(new CNcbiIfstream(filename.c_str(), IOS_BASE::in | IOS_BASE::binary));
    if (!(*inStream)) {
        ERROR_MESSAGE_CL("Cannot open file '" << filename << "' for reading");
        return false;
    }
    string firstWord;
    *inStream >> firstWord;
    bool isBinary = !(firstWord == "Cdd");
    inStream->seekg(0);     // rewind file
    firstWord.erase();

    string err;
    EDiagSev oldLevel = SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
    bool readOK = ReadASNFromFile(filename.c_str(), cdd, isBinary, &err);
    SetDiagPostLevel(oldLevel);
    if (!readOK)
        ERROR_MESSAGE_CL("can't read input file: " << err);

    return readOK;
}

string RefinerRowSelectorCodeToStr(const RefinerRowSelectorCode& code) {
    if (code == eRandomSelectionOrder) 
        return "random (shuffled between cycles)";
    else if (code == eWorstScoreFirst)
        return "worst to best self-hit score";
    else if (code == eBestScoreFirst)
        return "best to best self-hit score";
    else 
        return "UNKNOWN!!";

}

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CAlignmentRefiner::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> argDescr(new CArgDescriptions);

    // Specify USAGE context
    argDescr->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Alignment Refinement w/ 'Leave One Out'");

    HideStdArgs(fHideLogfile | fHideConffile | fHideDryRun | fHideXmlHelp);

    //  *Mandatory*  
     // input CD/Fasta file name
    argDescr->AddKey("i", "CdFilenameIn", "full filename of input CD w/ alignment to refine (ascii or binary)", argDescr->eString);

    //  base name for output CD file
    argDescr->AddDefaultKey("o", "CdBasenameOut", "basename of output CD(s) containing refined alignment; output saved to 'basename_<number>.cn3'; ascii text by default", argDescr->eString, "refiner");

    // output binary?
    argDescr->AddFlag("ob", "output binary data");

    // File for data from the refinement process
    argDescr->AddOptionalKey
        ("details", "filename",
         "create a file to save refinenment process details",
         CArgDescriptions::eOutputFile,
         CArgDescriptions::fPreOpen);

    // quiet reporting of details (only at end of temp steps and trials)
    argDescr->AddFlag("q", "shortened report; only Error level messages");


    //  Number of cycles per trial (a cycle consists of a LOO phase followed by a 
    //  block-editing phase, either of which may be disabled for all cycles)
    //  In each LOO phase, each row is left out exactly once unless overriden by the 'nr' option.
    //  The alignment is NOT reset after a cycle; alignment IS reset after a trial.
    argDescr->AddDefaultKey("nc", "integer", "number of cycles per trial; a cycle consists of one leave-one-out (LOO) phase, followed by a block editing phase.  Either, but not both, of the phases in a cycle may be turned off.\n(Note:  alignments do NOT reset between cycles)\n", argDescr->eInteger, "1");
    argDescr->SetConstraint("nc", new CArgAllow_Integers(1, N_MAX_CYCLES));

    // Convergence criteria
    argDescr->AddDefaultKey("convSameScore", "double", "when >= this % of LOO attempts fail to change the score, stop further cycles", argDescr->eDouble, "0.95");
    argDescr->SetConstraint("convSameScore", new CArgAllow_Doubles(0, 1.0));




    //
    //  Leave-one-out parameters & options
    //

    argDescr->SetCurrentGroup("  Leave-one-out phase parameters and options  ");

    //  disable LOO phase
    argDescr->AddFlag("no_LOO", "do not perform the LOO phase of each cycle", true);
    argDescr->SetDependency("no_LOO", CArgDescriptions::eExcludes, "selection_order");

    //  *Mandatory*  
    //  Row selection order for LOO: randomly or based on the self-hit to the initial alignment.
    argDescr->AddKey("selection_order", "integer", "Method for row selection in LOO phase:\n0 == randomly (use -n option to specify the number of separate trials)\n1 == increasing self-hit row score to input alignment's PSSM\n2 == decreasing self-hit row score to input alignment's PSSM\n", argDescr->eInteger);
    argDescr->SetConstraint("selection_order", new CArgAllow_Integers(0, 2));
    argDescr->AddAlias("so", "selection_order");

    //  switch from 'leave-one-out' to 'leave-N-out':  recompute PSSM only after
    //  lno rows refined.
    argDescr->AddDefaultKey("lno", "integer", "leave-N-out mode:  speed up program by recomputing PSSM after 'lno' rows have been left out\n(values exceeding the number of rows interpreted as Nrows - 1)\nFor best results, value should be < 20% of number of rows in input alignment.\n", argDescr->eInteger, "1");
    argDescr->SetConstraint("lno", new CArgAllow_Integers(0, kMax_Int));

    //  Number of row LOO events per cycle (may be equal to, more than or less than 
    //  number of rows in the CD, and it is not guaranteed that each row will be chosen)
//    argDescr->AddOptionalKey("nr", "integer", "absolute number of LOO attempts per LOO cycle (defaults to all eligible rows in alignment except the master)\n", argDescr->eInteger);
//    argDescr->SetConstraint("nr", new CArgAllow_Integers(1, N_MAX_ROWS));

    //  declare whether structures are to be among the rows left out
    argDescr->AddFlag("fix_structs", "do not perform LOO refinement on structures (i.e., those sequences having a PDB identifier)", true);


    //
    //  Block aligner parameters / constraints on search space
    //

    //  leave-one-out arguments used by block aligner
    argDescr->AddDefaultKey("p", "double", "percentile parameter for loop-length cutoff in block aligner", argDescr->eDouble, "1.0");
    argDescr->SetConstraint("p", new CArgAllow_Doubles(0, 10.0));
    argDescr->AddDefaultKey("x", "integer", "extension parameter for loop-length cutoff in block aligner", argDescr->eInteger, "0");
    argDescr->SetConstraint("x", new CArgAllow_Integers(0, kMax_Int));
    argDescr->AddDefaultKey("c", "integer", "cutoff parameter for loop-length cutoff in block aligner", argDescr->eInteger, "0");
    argDescr->SetConstraint("c", new CArgAllow_Integers(0, kMax_Int));

    //  Constraints on sequence length used in search for improved alignments
    argDescr->AddFlag("fs", "allow refiner to use full sequence (by default, refinement is constrained to initial aligned footprint)", true);
    argDescr->AddDefaultKey("ex","integer", "footprint extension size (symmetric for N- and C-termini); positive values extend the footprint; negative values shrink the footprint and can be used with -fs; relevant extension overriden by -nex and/or -cex", argDescr->eInteger, "0");
    argDescr->AddOptionalKey("nex","integer", "N-terminal footprint extension size; positive values extend the footprint; negative values shrink the footprint and can be used with -fs; overrides any N-terminal extension from -ex", argDescr->eInteger);
    argDescr->AddOptionalKey("cex","integer", "C-terminal footprint extension size; positive values extend the footprint; negative values shrink the footprint and can be used with -fs; overrides any C-terminal extension from -ex\n", argDescr->eInteger);


    //  Block freezing/un-freezing
    argDescr->AddFlag("ab", "realign all blocks; overrides -f and -l options", true);
    argDescr->AddOptionalKey("f", "integer", "first block to realign (post-IBM, from 1); overridden if -ab set", argDescr->eInteger);
    argDescr->SetConstraint("f", new CArgAllow_Integers(1, kMax_Int));
    argDescr->AddOptionalKey("l", "integer", "last block to realign (post-IBM, from 1); overridden if -ab set", argDescr->eInteger);
    argDescr->SetConstraint("l", new CArgAllow_Integers(1, kMax_Int));
//    argDescr->AddExtra(0, 25, "Block number (post-IBM, from 1) to freeze:  can override -ab or -f, -l for a specific block(s) in a range of unfrozen blocks\n", argDescr->eInteger);
    argDescr->AddExtra(0, 25, "Row OR block numbers to exclude from LOO (from 1 to # rows/blocks); master == row 1 is always excluded\n(See 'extras_are_blocks' flag.)\n", argDescr->eInteger);

    //  Flag to tell whether the extra args are row numbers (if not present) or block numbers (if present).
    argDescr->AddFlag("extras_are_blocks", "treat extra arguments as block numbers (default is to treat them as row numbers)", true);

    argDescr->SetCurrentGroup("");


    //
    //  Random selection order related options
    //

    argDescr->SetCurrentGroup("  Random selection order specific options  ");

    //  Number of trials
    argDescr->AddDefaultKey("n", "integer", "number of independent trials (restarts from original alignment)\nNOTE:  only relevant if use random selection order of rows; ignored if deterministic selection order used (see 'selection_order')", argDescr->eInteger, "3");
    argDescr->SetConstraint("n", new CArgAllow_Integers(1, N_MAX_TRIALS));

    argDescr->AddDefaultKey("nout", "integer", "number of output CDs; save CDs from the top 'nout' trials", argDescr->eInteger, "1");
    argDescr->SetConstraint("nout", new CArgAllow_Integers(1, N_MAX_TRIALS));

    argDescr->AddDefaultKey("convScoreChange", "double", "when avg. deviation <= this % of mean score for M trials, stop further trials", argDescr->eDouble, "0.001");
    argDescr->SetConstraint("convScoreChange", new CArgAllow_Doubles(0, 1.0));

    // Specify seed to RNG
    argDescr->AddOptionalKey("seed", "positive_integer", "specify the seed for random number generation\n", argDescr->eInteger);
    argDescr->SetConstraint("seed", new CArgAllow_Integers(1000, kMax_Int-1));


    argDescr->SetCurrentGroup("");


    //
    //  Block editing options
    //

    argDescr->SetCurrentGroup("  Block-editing phase parameters and options  ");

    argDescr->AddFlag("be_fix", "do not modify block boundaries", true);
    argDescr->AddFlag("be_noShrink", "do not attempt to shrink block boundaries", true);
    argDescr->AddFlag("be_shrinkFirst", "if set, try to shrink before extending a block; requires be_alg = both\n\n", true);

    argDescr->AddDefaultKey("be_minSize", "integer", "smallest allowed block width (set to 0 to allow block deletion events);\nblocks that start smaller than min size are not truncated.", argDescr->eInteger, "1");
    argDescr->SetConstraint("be_minSize", new CArgAllow_Integers(0, 1000));

    //  Scoring methods and algorithms for block edits.
    argDescr->AddDefaultKey("be_alg", "string", "block editing method", argDescr->eString, "both");
    argDescr->SetConstraint("be_alg", &(*new CArgAllow_Strings, "extend", "greedyExt", "shrink", "both"));
    argDescr->AddDefaultKey("be_score", "string", "column scoring method", argDescr->eString, "3.3.3");
    argDescr->SetConstraint("be_score", &(*new CArgAllow_Strings, "vote", "sumScores", "median", "scoreWeight", "compound", "3.3.3"));


    /////////////////////////
    //  The following three options define the parameters for the 'compound' scoring method.
    //  '3.3.3' is a special shortcut compound method that uses 3, .3, .3 for these values,
    //  respectively.  
    //  For future:  Use 'compound' to use same three scorers w/ non-default values.
    /////////////////////////
    argDescr->AddDefaultKey("be_median", "integer", "if be_score = 'median', '3.3.3', value column median must equal or exceed", argDescr->eInteger, "3");
    argDescr->SetConstraint("be_median", new CArgAllow_Integers(-10, 20));
    argDescr->AddDefaultKey("be_negScore", "double", "negative score fraction:  fraction of total column score less than zero\nto be added to a block, column's fraction must have at least this value\n** used for 'scoreWeight', 'compound', '3.3.3' scoring only", argDescr->eDouble, ".3");
    argDescr->SetConstraint("be_negScore", new CArgAllow_Doubles(0, 1));
    argDescr->AddDefaultKey("be_negRows", "double", "negative rows fraction:  largest fraction of rows in a column with a negative value\nfor the column to be added to a block\n** used for 'compound', '3.3.3' scoring only", argDescr->eDouble, ".3");
    argDescr->SetConstraint("be_negRows", new CArgAllow_Doubles(0, 1));



    argDescr->AddDefaultKey("be_minScore", "integer", "if be_score = 'vote', score at or above which a row votes 'yes'", argDescr->eInteger, "0");
    argDescr->SetConstraint("be_minScore", new CArgAllow_Integers(-100, 100));
    argDescr->AddOptionalKey("be_eThresh", "double", "extension threshold:\n column score at or above which a column is added to a block\nif not given, assumes same values as 'be_sThresh', or if neither is present an error is reported\n** with 'vote' scoring, must be in [0, 1]", argDescr->eDouble);
//    argDescr->SetConstraint("be_eThresh", new CArgAllow_Doubles(-1000, 1000));
    argDescr->AddOptionalKey("be_sThresh", "double", "shrinkage threshold (ignored if -be_noShrink not specified):\n column score below which a column is removed from a block\nif not given, assumes same values as 'be_eThresh', or if neither is present an error is reported\n** with 'vote' scoring, must be in [0, 1]\n", argDescr->eDouble);
//    argDescr->SetConstraint("be_sThresh", new CArgAllow_Doubles(-1000, 1000));

    argDescr->SetCurrentGroup("");



    // argDescr->AddFlag("qd", "shortened details report; only report details at end of cycles and trials.  Has an affect only if '-details' is provided");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(argDescr.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Main Run method; invoke requested tests (printout arguments obtained from command-line)

int CAlignmentRefiner::Run(void)
{
    CCdd cdd;
    unsigned int alWidth;
    unsigned int nBlocksFromAU;
    string message;

    // Get arguments
    const CArgs& args = GetArgs();
    string fname, err;
    string basename = args["o"].AsString() + "_", suffix = ".cn3"; 

    // Stream to results file, if provided, or cout
    // (NOTE: "x_lg" is just a workaround for bug in SUN WorkShop 5.1 compiler)
    ostream* x_lg = args["details"] ? &args["details"].AsOutputFile() : &cout;
    ostream& detailsStream = *x_lg;

    SetDiagStream(x_lg); // send all diagnostic messages to the same stream

    //  Set to info level here, not in main, to avoid info messages about missing log files.
    SetDiagPostLevel(eDiag_Info);

    // Set up details stream first...
    if (args["details"]) {
        string str;
        detailsStream << args.Print(str) << endl << endl;
        detailsStream << string(72, '=') << endl;
        detailsStream.precision(2);
//        if (args["qd"]) m_quietDetails = true;
    }

    // Get initial alignment (in a Cdd blob) from the input file;
    // convert to an AlignmentUtility object.

    if (!ReadCD(args["i"].AsString(), &cdd)) {
        ERROR_MESSAGE_CL("error reading CD from input file " << args["i"].AsString());
        return eRefinerResultCantReadCD;
    }

    AlignmentUtility* au = new AlignmentUtility(cdd.GetSequences(), cdd.GetSeqannot());
    const BlockMultipleAlignment* bma = (au && au->Okay()) ? au->GetBlockMultipleAlignment() : NULL;
    if (!bma) {
        delete au;
        ERROR_MESSAGE_CL("Found invalid alignment in CD " << args["i"].AsString());
        return eRefinerResultAlignmentUtilityError;
    }

    nBlocksFromAU = bma->NAlignedBlocks();
    alWidth       = bma->AlignmentWidth();

    TERSE_INFO_MESSAGE_CL("\nRows in alignment:  " << bma->NRows());
    TERSE_INFO_MESSAGE_CL("Alignment width  :  " << alWidth);
    TERSE_INFO_MESSAGE_CL("Number of Aligned Blocks after IBM:  " << nBlocksFromAU << "\n");

    
    //  Some general parameters...

    m_nTrials = (unsigned) args["n"].AsInteger();
    m_nCycles = args["nc"].AsInteger();
    m_scoreDeviationThreshold = args["convScoreChange"].AsDouble();

    m_quietMode = (args["q"]);
    //  if (m_quietMode) SetDiagPostLevel(eDiag_Error);   


    //  Fill out data structure for leave-one-out parameters
    //  LOO is performed unless -no_LOO option used

    RefinerResultCode looParamResult = ExtractLOOArgs(nBlocksFromAU, message);
    if (looParamResult != eRefinerResultOK) {
        ERROR_MESSAGE_CL(message);
        return looParamResult;
    }

    //  If using a fixed selection order based on row-scores of the
    //  initial alignment, no need to do multiple trials.
    if (m_nTrials > 1 && m_loo.selectorCode != eRandomSelectionOrder) {
        m_nTrials = 1;
        WARNING_MESSAGE_CL("For deterministic row-selection order, multiple trials are redundant.\nSetting number of trials to one and continuing.\n");
    }

    //EchoSettings(detailsStream, true, false);

    //  Fill out data structure for block editing parameters.  
    //  By default, edit blocks -- must explicitly skip with the -be_fix option.
    RefinerResultCode beParamResult = ExtractBEArgs(nBlocksFromAU, message);
    if (beParamResult != eRefinerResultOK) {
        ERROR_MESSAGE_CL(message);
        return beParamResult;
    }

    //EchoSettings(detailsStream, false, true);
    EchoSettings(detailsStream, true, true);

    if (!m_blockEdit.editBlocks && !m_loo.doLOO) {
        ERROR_MESSAGE_CL("Nothing will happen as both LOO and block editing have been disabled.  Stopping");
        return eRefinerResultInconsistentArgumentCombination;
    }


    //  Perform the refinement...

    TRACE_MESSAGE_CL("Entering refiner engine...\n");

    CBMARefinerEngine refinerEngine(m_loo, m_blockEdit, m_nCycles, m_nTrials, true, !m_quietMode, m_scoreDeviationThreshold);
    RefinerResultCode result = refinerEngine.Refine(au, &detailsStream);


    //  Output final statistics and refined alignments
    //  Get results from all trials; use reverse iterator to get them
    //  out of the map in order of highest to lowest score.


    unsigned int n = 0;
    unsigned int trial;
    unsigned int nToWrite = (m_nTrials > 1) ? args["nout"].AsInteger() : 1;

    const RefinedAlignments& optimizedAlignments = refinerEngine.GetAllResults();
    RefinedAlignmentsRevCIt rcit = optimizedAlignments.rbegin(), rend = optimizedAlignments.rend();

    if (rcit != rend) {
        detailsStream << endl << endl << "Original Alignment Score = " << refinerEngine.GetInitialScore() << endl;
        detailsStream << endl << "Best Refined Alignments (in descending score order)\n\n";
    } else {
        detailsStream << endl << "No Refined Alignments found (?)\n\n";
    }

    for (; rcit != rend; ++rcit, ++n) {
        trial = rcit->second.iteration;
        if (rcit->second.au == NULL) {
            detailsStream << "Problem in trial " << trial << " -> no refined alignment available." << endl << endl;
            continue;
        }

        detailsStream << "Alignment " << n << ":  Score = " << rcit->first << " (trial " << trial << ")" << endl;
        if (n < nToWrite) {
            err.erase();
            cdd.SetSeqannot() = rcit->second.au->GetSeqAnnots();

            // write output as a CD in a new file
            fname = basename + NStr::UIntToString(n) + "_trial" + NStr::UIntToString(trial) + suffix;
            detailsStream << "    (written to file '" << fname << "')";
            if (!WriteASNToFile(fname.c_str(), cdd, args["ob"].HasValue(), &err)) {
                ERROR_MESSAGE_CL("error writing output file " << fname);
            }
        }
        detailsStream << endl;
    }

    //  Destructor of RefinerEngine cleans up map of optimized alignments
    //  once it goes out of scope.
//    delete au;
//    delete auOriginal;

    if (args["details"]) args["details"].CloseFile();
    return result;

}

RefinerResultCode CAlignmentRefiner::ExtractLOOArgs(unsigned int nAlignedBlocks, string& msg) {

    int selectionOrder;
    unsigned int nBlocksMade, nExtra, extra;

    // Get arguments
    const CArgs& args = GetArgs();
    RefinerResultCode result = eRefinerResultOK;

    msg.erase();

    m_loo.doLOO      = (!args["no_LOO"]);
    m_loo.fixStructures = (args["fix_structs"]);
    m_loo.extrasAreRows = (!args["extras_are_blocks"]);


    //  "selection_order" is mandatory (unless -no_LOO is present) and constrained to {0, 1, 2}.
    //  number of trials is only relevant for a random selection order.
    selectionOrder = (m_loo.doLOO) ? args["selection_order"].AsInteger() : 0;
    switch (selectionOrder) {
    case 0:
        m_loo.selectorCode = eRandomSelectionOrder;
        break;
    case 1:
        m_nTrials = 1;
        m_loo.selectorCode = eWorstScoreFirst;
        break;
    case 2:
        m_nTrials = 1;
        m_loo.selectorCode = eBestScoreFirst;
        break;
    };


    if (m_loo.doLOO) {
        m_loo.fullSequence = (args["fs"]);
        m_loo.nExt = (args["nex"]) ? args["nex"].AsInteger() : args["ex"].AsInteger();
        m_loo.cExt = (args["cex"]) ? args["cex"].AsInteger() : args["ex"].AsInteger();

        m_loo.seed = (args["seed"]) ? args["seed"].AsInteger() : 0; 
        m_loo.lno  = (unsigned int) args["lno"].AsInteger();
        m_loo.sameScoreThreshold      = args["convSameScore"].AsDouble();

        m_loo.percentile = args["p"].AsDouble();
        m_loo.extension  = (unsigned) args["x"].AsInteger();
        m_loo.cutoff     = (unsigned) args["c"].AsInteger();
    }

   if (m_loo.doLOO) {
       if (m_loo.extrasAreRows) {
           nExtra = (unsigned int) args.GetNExtra();
           for (unsigned int i = 1; i <= nExtra; ++i) {
               extra = (unsigned int) args[i].AsInteger() - 1;
               m_loo.rowsToExclude.push_back(extra);
           }
       }
       //  'false' == don't exclude any blocks using extra cmd line arguments;
       nBlocksMade   = GetBlocksToAlign(nAlignedBlocks, m_loo.blocks, msg, !m_loo.extrasAreRows);
       msg = "Freeze " + NStr::UIntToString(nAlignedBlocks - nBlocksMade) + " blocks in ExtractLOOArgs.\n";
    }
    return result;
}


unsigned int  CAlignmentRefiner::GetBlocksToAlign(unsigned int nBlocks, vector<unsigned int>& blocks, string& msg, bool useExtras) {

    bool skip  = false;
    unsigned int first = 0, last = nBlocks - 1;
    blocks.clear();
    if (nBlocks == 0) return 0;

    const CArgs& args = GetArgs();
    unsigned int nExtra = (useExtras) ? (unsigned int) args.GetNExtra() : 0;

    //  If specify realignment of all blocks, the default settings are OK.
    //  Otherwise, use the range specified in the -f and -l flags.
    //  Recall that the command line takes in a one-based integer; blocks is zero-based.
    if (!args["ab"]) {

        if (args["f"]) {
            first = (unsigned) args["f"].AsInteger() - 1;
        }
        if (args["l"]) {
            last  = (unsigned) args["l"].AsInteger() - 1;
        }
        if (first >= nBlocks) {
            first = 0;
        }
        if (last < first || last >= nBlocks) {
            last = nBlocks - 1;
        }
    }

    //  If there are any extra arguments provided, they refer to block numbers to freeze.
    msg = "\nAligning blocks: ";
    for (unsigned int i = first; i <= last; ++i) {
        if (nExtra > 0) {
            skip = false;
            for (size_t extra = 1; extra <= nExtra; ++extra) {
                if (args[extra].AsInteger() - 1 == (int) i) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;
        }
        blocks.push_back(i);
        msg.append(NStr::UIntToString(i+1) + " ");
        if ((last-first+1)%15 == 0 && first != 0) msg.append("\n");
    }
    //TERSE_INFO_MESSAGE_CL("message in GetBlocksToAlign:\n" << msg);
    return blocks.size();
}


RefinerResultCode CAlignmentRefiner::ExtractBEArgs(unsigned int nAlignedBlocks, string& msg) {

    // Get arguments
    const CArgs& args = GetArgs();
    RefinerResultCode result = eRefinerResultOK;

    m_blockEdit.editBlocks = (!args["be_fix"]);
    m_blockEdit.canShrink  = (!args["be_noShrink"]);
    m_blockEdit.extendFirst = (!args["be_shrinkFirst"]);

    msg.erase();
    if (m_blockEdit.editBlocks) {

        //  Get algorithm to use.
        string algMethod = args["be_alg"].AsString();
        if (algMethod == "both") {
            m_blockEdit.algMethod = eSimpleExtendAndShrink;
        } else if (algMethod == "extend") {
            m_blockEdit.algMethod = eSimpleExtend;
        } else if (algMethod == "greedyExt") { 
            m_blockEdit.algMethod = eGreedyExtend;
        } else if (algMethod == "shrink") {
            m_blockEdit.algMethod = eSimpleShrink;
        } else {
            m_blockEdit.algMethod = eInvalidBBAMethod;
            msg += "Unrecognized algorithm specified:  'be_alg = " + algMethod + "'.  Stopping.";
            result = eRefinerResultInconsistentArgumentCombination;
        }


        //  Define the column scoring method to be used.
        //  Greedy extend implies median scoring in initial implementation.
        string columnMethod = (m_blockEdit.algMethod == eGreedyExtend) ? "median" : args["be_score"].AsString();

        m_blockEdit.columnScorerThreshold = (double) args["be_minScore"].AsInteger();
        if (columnMethod == "vote") {
            m_blockEdit.columnMethod = ePercentAtOrOverThreshold;
        } else if (columnMethod == "sumScores") {
            m_blockEdit.columnMethod = eSumOfScores;
        } else if (columnMethod == "median") {
            m_blockEdit.columnMethod = eMedianScore;
        } else if (columnMethod == "scoreWeight") {
            m_blockEdit.columnMethod = ePercentOfWeightOverThreshold;
        } else {
            //  anything else is some flavor of compound scorer
            m_blockEdit.columnMethod = eCompoundScorer;
            m_blockEdit.median = args["be_median"].AsInteger();
            m_blockEdit.negScoreFraction = args["be_negScore"].AsDouble();
            m_blockEdit.negRowsFraction = args["be_negRows"].AsDouble();

            //  for the special case, ensure will be looking at -ve vs. non-neg in scorers
            if (columnMethod == "3.3.3") m_blockEdit.columnScorerThreshold = 0;
        }


        //  Validate options re: block shrinking.
        if (!m_blockEdit.canShrink) {
            if (algMethod == "both") {
                WARNING_MESSAGE_CL("With '-be_noShrink' on, method 'both' is changed to 'extend'.");
                m_blockEdit.algMethod = eSimpleExtend;
                m_blockEdit.extendFirst = true;
                algMethod = "extend";
            } else if (algMethod == "shrink") {
                msg += "\nInconsistent parameters:  Do not specify 'shrink' method with '-be_noShrink'!  Aborting";
                result = eRefinerResultInconsistentShrinkageSettings;
            }
        }

        //  Deal with shrinking & extending parameters.
        if (!args["be_eThresh"] && !args["be_sThresh"] && 
            !((columnMethod == "scoreWeight" && args["be_negScore"]) || columnMethod == "3.3.3")) {
            msg += "\nWhen block boundaries are edited, you must provide either or both\nof the 'be_eThresh' or 'be_sThresh' options when not using '3.3.3', or 'scoreWeight' w/ be_negScore, scoring.\nAborting";
            result = eRefinerResultInvalidThresholdValue;
        }

        if (args["be_eThresh"] || columnMethod == "3.3.3") {
            m_blockEdit.extensionThreshold = (args["be_eThresh"]) ? args["be_eThresh"].AsDouble() : 0;
            m_blockEdit.shrinkageThreshold = (args["be_sThresh"]) ?
                args["be_sThresh"].AsDouble() : m_blockEdit.extensionThreshold;
        } else if (args["be_sThresh"]) {
            m_blockEdit.shrinkageThreshold = args["be_sThresh"].AsDouble();
            m_blockEdit.extensionThreshold = m_blockEdit.shrinkageThreshold;
        }

        if (columnMethod == "scoreWeight" && args["be_negScore"]) {
            m_blockEdit.negScoreFraction = args["be_negScore"].AsDouble();
            m_blockEdit.shrinkageThreshold = 1.0 - m_blockEdit.negScoreFraction;
            m_blockEdit.extensionThreshold = m_blockEdit.shrinkageThreshold;
        }

        if (!m_blockEdit.canShrink) m_blockEdit.shrinkageThreshold = 0;


        //  Validate range of extension thresholds in voting (in other column
        //  scoring methods, they may be outside this interval).
        if (columnMethod == "vote") {
            double et = m_blockEdit.extensionThreshold, st = m_blockEdit.shrinkageThreshold;
            if (et < 0 || et > 1) {
                msg += "\nInvalid extension threshold (" + NStr::DoubleToString(et) + "); for vote scoring must be in [0, 1].\n";
                result = eRefinerResultInvalidThresholdValue;
            }
            if (st < 0 || st > 1) {
                msg += "\nInvalid shrinkage threshold (" + NStr::DoubleToString(st) + "); for vote scoring must be in [0, 1].\n";
                result = eRefinerResultInvalidThresholdValue;
            }
        }

        m_blockEdit.minBlockSize = (unsigned) args["be_minSize"].AsInteger();
        m_blockEdit.columnMethod2 = eInvalidColumnScorerMethod;

        //  Exclude blocks in the same way done for LOO...
        vector<unsigned int> blocksToAlign;
        unsigned int nBlocksMade   = GetBlocksToAlign(nAlignedBlocks, blocksToAlign, msg, true);
        m_blockEdit.editableBlocks.clear();
        m_blockEdit.editableBlocks.insert(blocksToAlign.begin(), blocksToAlign.end());
        msg = "Freeze " + NStr::UIntToString(nAlignedBlocks - nBlocksMade) + " blocks in ExtractBEArgs.\n";
    }

    return result;
}


void CAlignmentRefiner::EchoSettings(ostream& echoStream, bool echoLOO, bool echoBE) {

    static string yes = "Yes", no = "No";

    const CArgs& args = GetArgs();
    unsigned int nExtra = (unsigned int) args.GetNExtra();

    if ((!echoLOO && !echoBE) || (echoLOO && echoBE)) {
        echoStream << "Global Refinement Parameters:" << endl;
        echoStream << "=================================" << endl;
        echoStream << "Number of trials = " << m_nTrials << endl;
        echoStream << "Number of cycles per trial = " << m_nCycles << endl;
        echoStream << "Alignment score deviation threshold = " << m_scoreDeviationThreshold << endl;

        if (nExtra > 0) {
            echoStream << "Extra argument(s) freeze " << ((m_loo.extrasAreRows) ? "Row:\n    " : "Block:\n    ");
            for (size_t extra = 1; extra <= nExtra; ++extra) {
                echoStream << args[extra].AsInteger() << "  ";
            }
            echoStream << endl;
        } else {
            echoStream << "No extra arguments that exclude specific rows/blocks from refinement." << endl;
        }
//    echoStream << "Quiet details mode? " << ((m_quietDetails) ? "ON" : "OFF") << endl;
//    echoStream << "Forced threshold (for MC only) = " << m_forcedThreshold << endl;
        echoStream << "Quiet mode? " << ((m_quietMode) ? "ON" : "OFF") << endl;
        echoStream << endl;
    }

    if (echoLOO) {
        echoStream << "Leave-One_Out parameters:" << endl;
        echoStream << "=================================" << endl;
        echoStream << "LOO on?  " << ((m_loo.doLOO) ? yes : no) << endl;
        if (m_loo.doLOO) {
            echoStream << "Row selection order:  " << RefinerRowSelectorCodeToStr(m_loo.selectorCode) << endl;
            echoStream << "Number left out between PSSM recomputation = " << m_loo.lno << endl;

            echoStream << "Freeze alignment of rows with structure?  " << ((m_loo.fixStructures) ? yes : no) << endl;
            echoStream << "Use full sequence or aligned footprint?  " << ((m_loo.fullSequence) ? "Full" : "Aligned") << endl;
            echoStream << "N-terminal extension allowed = " << m_loo.nExt << endl;
            echoStream << "C-terminal extension allowed = " << m_loo.cExt << endl;

            echoStream << "Converged after fraction of rows left out do not change score = " << m_loo.sameScoreThreshold << endl;
            echoStream << "Random number generator seed = " << m_loo.seed << endl;

            echoStream << "LOO loop percentile:  longest loop allowed = max initial loop * " << m_loo.percentile << endl;
            echoStream << "LOO extension to longest loop allowed = " << m_loo.extension << endl;
            echoStream << "LOO absolute maximum longest loop (zero == no max) = " << m_loo.cutoff << endl;
        }
        echoStream << endl;
}

    if (echoBE) {
        string algMethod = "Invalid Method";
        string columnMethod = algMethod;

        switch (m_blockEdit.algMethod) {
        case eSimpleExtendAndShrink:
            algMethod = "Extend and Shrink";
            break;
        case eSimpleExtend:
            algMethod = "Extend Only";
            break;
        case eSimpleShrink:
            algMethod = "Shrink Only";
            break;
        case eGreedyExtend:
            algMethod = "Greedy Extend Only";
            break;
        default:
            break;
        };

        switch (m_blockEdit.columnMethod) {
        case ePercentAtOrOverThreshold:
            columnMethod = "% Rows at or Over Threshold";
            break;
        case eSumOfScores:
            columnMethod = "Sum of Scores";
            break;
        case eMedianScore:
            columnMethod = "Median Score";
            break;
        case ePercentOfWeightOverThreshold:
            columnMethod = "% Score Weight at or Over Threshold";
            break;
        case eCompoundScorer:
            columnMethod = "3.3.3";
            if (GetArgs()["be_score"].AsString() != "3.3.3") {
                columnMethod = "Compound Scoring";
            }
            break;
        default:
            break;
        };


        echoStream << "Block editing parameters:" << endl;
        echoStream << "=================================" << endl;
        echoStream << "block editing on?         " << ((m_blockEdit.editBlocks) ? yes : no) << endl;
        if (m_blockEdit.editBlocks) {
            echoStream << "block shrinking on?       " << ((m_blockEdit.canShrink) ? yes : no) << endl;
            echoStream << "extend first?             " << ((m_blockEdit.extendFirst) ? yes : no) << endl;
            echoStream << endl;
            echoStream << "block editing method    = " << algMethod << endl;
            echoStream << "column scoring method   = " << columnMethod << endl;
            echoStream << endl;
//        echoStream << "not used:  column meth2 = " << m_blockEdit.columnMethod2 << endl << endl; 
            if (GetArgs()["be_score"].AsString() == "3.3.3") {
                echoStream << "(used for 3.3.3 scoring only):" << endl;
                echoStream << "    median threshold        = " << m_blockEdit.median << endl;
                echoStream << "    negative score fraction = " << m_blockEdit.negScoreFraction << endl;
                echoStream << "    negative row   fraction = " << m_blockEdit.negRowsFraction << endl;
            } else {
                echoStream << "minimum block size      = " << m_blockEdit.minBlockSize << endl;
                echoStream << "column-scorer threshold = " << m_blockEdit.columnScorerThreshold << endl;
                echoStream << "extension threshold     = " << m_blockEdit.extensionThreshold << endl;
                echoStream << "shrinkage threshold     = " << m_blockEdit.shrinkageThreshold << endl;
            }

        }
        echoStream << endl;
    }
}

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAlignmentRefiner::Exit(void)
{
    SetDiagStream(0);
}

END_SCOPE(align_refine)

/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_SCOPE(align_refine);

int main(int argc, const char* argv[])
{
    int result;

    SetDiagStream(&NcbiCerr); // send all diagnostic messages to cerr
    SetDiagPostLevel(eDiag_Warning);   
    //    SetupCToolkitErrPost(); // reroute C-toolkit err messages to C++ err streams

    SetDiagTrace(eDT_Default);      // trace messages only when DIAG_TRACE env. var. is set
    //#ifdef _DEBUG
    //    SetDiagPostFlag(eDPF_File);
    //    SetDiagPostFlag(eDPF_Line);
    //#else
    UnsetDiagTraceFlag(eDPF_File);
    UnsetDiagTraceFlag(eDPF_Line);
    //#endif

    // C++ object verification
    CSerialObject::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectIStream::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectOStream::SetVerifyDataGlobal(eSerialVerifyData_Always);

    // Execute main application function
    const CTime start(CTime::eCurrent);
    CTime stop;
    CAlignmentRefiner refiner;
    result = refiner.AppMain(argc, argv, 0, eDS_Default, 0);

    //  Timing info
    stop.SetCurrent();
    cout << "\n\n****  Elapsed Time = " << stop.DiffSecond(start) << " sec  ****" << endl << endl;

    return result;
}
