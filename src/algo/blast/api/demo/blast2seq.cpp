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

/** @file blast2seq.cpp
 * Main driver for blast2sequences C++ interface
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/iterator.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <corelib/ncbitime.hpp>
#include <objtools/readers/fasta.hpp>

#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include "blast_input.hpp" // From working directory

#include <objects/seqalign/Seq_align_set.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

/////////////////////////////////////////////////////////////////////////////
/// CBlast2seqApplication: command line blast2sequences application
/// @todo refactor command line options, so that only those relevant to a
/// particular program are shown (e.g: cvs -H command). This should be
/// reusable by all BLAST command line clients

class CBlast2seqApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    void InitObjMgr(void);
    CBlastOptionsHandle* ProcessCommandLineArgs() THROWS((CBlastException));

#ifndef NDEBUG
    FILE* GetOutputFilePtr(void); // needed for debugging only
#endif

    CRef<CObjectManager>    m_ObjMgr;  ///< instance of the object manager
};

/** Initialize commandline arguments. */
void CBlast2seqApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Compares 2 sequence using the BLAST algorithm");

    // Program type
    arg_desc->AddKey("program", "p", "Type of BLAST program",
            CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("program", &(*new CArgAllow_Strings, 
                "blastp", "blastn", "blastx", "tblastn", "tblastx"));

    // Query sequence
    arg_desc->AddDefaultKey("query", "q", "Query file name",
            CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    // Subject(s) sequence(s)
    arg_desc->AddKey("subject", "s", "Subject(s) file name",
            CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    // Copied from blast_app
    arg_desc->AddDefaultKey("strand", "strand", 
        "Query strands to search: 1 forward, 2 reverse, 0,3 both",
        CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("strand", new CArgAllow_Integers(0,3));
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
    arg_desc->SetConstraint("templen", 
                            &(*new CArgAllow_Strings, "0", "16", "18", "21"));

    arg_desc->AddDefaultKey("templtype", "templtype", 
        "Discontiguous word template type",
        CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("templtype", new CArgAllow_Integers(0,2));
    arg_desc->AddDefaultKey("thresh", "threshold", 
        "Score threshold for neighboring words",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("window","window", "Window size for two-hit extension",
                            CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("varword", "varword", 
        "Should variable word size be used?",
        CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("xungap", "xungapped", 
        "X-dropoff value for ungapped extensions",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("ungapped", "ungapped", 
        "Perform only an ungapped alignment search?",
        CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("greedy", "greedy", 
        "Use greedy algorithm for gapped extensions: 0 no, 1 one-step, 2 two-step, 3 two-step with ungapped",
        CArgDescriptions::eInteger, "0");
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
    arg_desc->AddDefaultKey("out", "outputfile", 
        "File name for writing the seqalign results in ASN.1 form",
        CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    // Debug parameters
    arg_desc->AddFlag("trace", "Tracing enabled?", true);

    SetupArgDescriptions(arg_desc.release());
}

/** Initialize object manager. */
void CBlast2seqApplication::InitObjMgr(void)
{
    m_ObjMgr = CObjectManager::GetInstance();
    if (!m_ObjMgr) {
         throw std::runtime_error("Could not initialize object manager");
    }
}

/** Use commandline arguments to set CBlastOptions. */
CBlastOptionsHandle*
CBlast2seqApplication::ProcessCommandLineArgs() THROWS((CBlastException))
{
    CArgs args = GetArgs();

    EProgram prog = ProgramNameToEnum(args["program"].AsString());

    if (args["lookup"].AsInteger() == 1) {
        prog = eMegablast;
        if (args["templen"].AsInteger() > 0) 
            prog = eDiscMegablast;
    }
    CBlastOptionsHandle* retval = CBlastOptionsFactory::Create(prog);
    if ( !retval ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "");
    }
    CBlastOptions& opt = retval->SetOptions();

    if (args["strand"].AsInteger()) {
        switch (args["strand"].AsInteger()) {
        case 1: opt.SetStrandOption(eNa_strand_plus); break;
        case 2: opt.SetStrandOption(eNa_strand_minus); break;
        case 3: opt.SetStrandOption(eNa_strand_both); break;
        default: abort();
        }
    }

    opt.SetFilterString(args["filter"].AsString().c_str());
    // FIXME: Handle lcase masking

    if (args["lookup"].AsInteger()) {
        opt.SetLookupTableType(args["lookup"].AsInteger());
    }
    if (args["matrix"]) {
        opt.SetMatrixName(args["matrix"].AsString().c_str());
    }
    if (args["mismatch"].AsInteger()) {
        opt.SetMismatchPenalty(args["mismatch"].AsInteger());
    }
    if (args["match"].AsInteger()) {
        opt.SetMatchReward(args["match"].AsInteger());
    }
    if (args["word"].AsInteger()) {
        opt.SetWordSize(args["word"].AsInteger());
    }
    if (args["templen"].AsInteger()) {
        opt.SetMBTemplateLength(args["templen"].AsInteger());
        opt.SetFullByteScan(false);
    }
    if (args["templtype"].AsInteger()) {
        opt.SetMBTemplateType(args["templtype"].AsInteger());
    }
    if (args["thresh"].AsInteger()) {
        opt.SetWordThreshold(args["thresh"].AsInteger());
    }
    if (args["window"].AsInteger()) {
        opt.SetWindowSize(args["window"].AsInteger());
    }

    // The next 3 apply to nucleotide searches only
    string program = args["program"].AsString();
    if (program == "blastn") {
        if (args["templen"].AsInteger()) {
            // Setting template length involves changing the scanning 
            // stride as well, which is handled in the derived 
            // CDiscNucleotideOptionsHandle class, but not in the base
            // CBlastOptionsHandle class.
            CDiscNucleotideOptionsHandle* disc_nucl_handle =
                dynamic_cast<CDiscNucleotideOptionsHandle*>(retval);

            disc_nucl_handle->SetTemplateLength(args["templen"].AsInteger());
            disc_nucl_handle->SetFullByteScan(false);
        }
        if (args["templtype"].AsInteger()) {
            opt.SetMBTemplateType(args["templtype"].AsInteger());
        }
        opt.SetVariableWordSize(args["varword"].AsBoolean());
    }

    if (args["xungap"].AsDouble()) {
        opt.SetXDropoff(args["xungap"].AsDouble());
    }

    if (args["ungapped"].AsBoolean()) {
        opt.SetGappedMode(false);
    }

    if (args["gopen"].AsInteger()) {
        opt.SetGapOpeningCost(args["gopen"].AsInteger());
    }
    if (args["gext"].AsInteger()) {
        opt.SetGapExtensionCost(args["gext"].AsInteger());
    }

    switch (args["greedy"].AsInteger()) {
    case 1: /* Immediate greedy gapped extension with traceback */
        opt.SetGapExtnAlgorithm(eGreedyWithTracebackExt);
        opt.SetUngappedExtension(false);
        break;
    case 2: /* Two-step greedy extension, no ungapped extension */
        opt.SetGapExtnAlgorithm(eGreedyExt);
        opt.SetGapTracebackAlgorithm(eGreedyTbck);
        opt.SetUngappedExtension(false);
        break;
    case 3: /* Two-step greedy extension after ungapped extension*/
        opt.SetGapExtnAlgorithm(eGreedyExt);
        opt.SetGapTracebackAlgorithm(eGreedyTbck);
        break;
    default: break;
    }


    if (args["xgap"].AsDouble()) {
        opt.SetGapXDropoff(args["xgap"].AsDouble());
    }
    if (args["xfinal"].AsDouble()) {
        opt.SetGapXDropoffFinal(args["xfinal"].AsDouble());
    }

    if (args["evalue"].AsDouble()) {
        opt.SetEvalueThreshold(args["evalue"].AsDouble());
    }

    if (args["searchsp"].AsDouble()) {
        opt.SetEffectiveSearchSpace((Int8) args["searchsp"].AsDouble());
    }
    if (args["perc"].AsDouble()) {
        opt.SetPercentIdentity(args["perc"].AsDouble());
    }

    if (args["gencode"].AsInteger()) {
        opt.SetQueryGeneticCode(args["gencode"].AsInteger());
    }
    if (args["dbgencode"].AsInteger()) {
        opt.SetDbGeneticCode(args["dbgencode"].AsInteger());
    }

    if (args["maxintron"].AsInteger()) {
        opt.SetLongestIntronLength(args["maxintron"].AsInteger());
    }
    if (args["frameshift"].AsInteger()) {
        opt.SetFrameShiftPenalty(args["frameshift"].AsInteger());
        opt.SetOutOfFrameMode();
    }

    return retval;
}

#ifndef NDEBUG

/** Optionally output to a file for debugging. */
FILE*
CBlast2seqApplication::GetOutputFilePtr(void)
{
    FILE *retval = NULL;

    if (GetArgs()["out"].AsString() == "-")
        retval = stdout;    
    else
        retval = fopen((char *)GetArgs()["out"].AsString().c_str(), "a");

    ASSERT(retval);
    return retval;
}
#endif


/** Application setup. Set up query and subject, process commandline
    arguments, run the search, and output results. */
int CBlast2seqApplication::Run(void)
{
    try {
        InitObjMgr();
        int counter = 0;
        const CArgs args = GetArgs();

        if (args["trace"])
            SetDiagTrace(eDT_Enable);

        EProgram program = ProgramNameToEnum(args["program"].AsString());
        ENa_strand query_strand = eNa_strand_unknown;
        ENa_strand subject_strand = eNa_strand_unknown;

        bool query_is_aa = (program == eBlastp || 
                            program == eTblastn || 
                            program == eRPSBlast);
        if (!query_is_aa) { 
            int cmdline_strand = args["strand"].AsInteger();

            if (cmdline_strand == 1)
                query_strand = eNa_strand_plus;
            else if (cmdline_strand == 2)
                query_strand = eNa_strand_minus;
            else
                query_strand = eNa_strand_both;
        }

        if (program == eBlastn ||
            program == eTblastn ||
            program == eTblastx)
            subject_strand = eNa_strand_plus;

        // Retrieve input sequences
        TSeqLocVector query_loc = 
            BLASTGetSeqLocFromStream(args["query"].AsInputFile(), *m_ObjMgr,
              query_strand, 0, 0, &counter, args["lcase"].AsBoolean());

        TSeqLocVector subject_loc = 
            BLASTGetSeqLocFromStream(args["subject"].AsInputFile(), *m_ObjMgr,
              subject_strand, 0, 0, &counter);

        CBlastOptionsHandle& opt_handle = *ProcessCommandLineArgs();

#ifndef NDEBUG
        CStopWatch sw;
        sw.Start();
#endif

        CBl2Seq blaster(query_loc, subject_loc, opt_handle);
        TSeqAlignVector seqalignv = blaster.Run();

#ifndef NDEBUG
        double t = sw.Elapsed();
        cerr << "CBl2seq run took " << t << " seconds" << endl;
        if (seqalignv.size() == 0) {
            cerr << "Returned NULL SeqAlign!" << endl;
            exit(1);
        }
#endif

        // Our poor man's formatting ...  
        CNcbiOstream& out = args["out"].AsOutputFile();
        for (unsigned int index = 0; index < seqalignv.size(); ++index)
            out << MSerial_AsnText << *seqalignv[index];

    } catch (const CException& e) {
        cerr << e.what() << endl;
    }

    return 0;
}

/** Application teardown. */
void CBlast2seqApplication::Exit(void)
{
    SetDiagStream(0);
}

/* @} */

/** Application entry point. */
int main(int argc, const char* argv[])
{
    return CBlast2seqApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}

