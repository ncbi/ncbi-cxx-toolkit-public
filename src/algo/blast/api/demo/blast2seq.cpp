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

static char const rcsid[] = 
    "$Id$";

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

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
/// CBlast2seqApplication: command line blast2sequences application
/// @todo Implement formatting
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

    CRef<CObjectManager>    m_ObjMgr;
    CRef<CScope>            m_Scope;
};

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
    arg_desc->AddDefaultKey("scantype", "scantype", 
        "Method for scanning the database: 0 default; "
        "1 AG - extension in both directions, \n"
        "2 traditional - extension to the right,\n"
        "3 Update of word length on diagonal entries.",
        CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("scantype", new CArgAllow_Integers(0,3));

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
    arg_desc->AddDefaultKey("descr", "descriptions",
        "How many matching sequence descriptions to show?",
        CArgDescriptions::eInteger, "500");
    arg_desc->AddDefaultKey("align", "alignments", 
        "How many matching sequence alignments to show?",
        CArgDescriptions::eInteger, "250");
    arg_desc->AddDefaultKey("out", "out", "File name for writing output",
        CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);
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

    // Debug parameters
    arg_desc->AddFlag("trace", "Tracing enabled?", true);

    SetupArgDescriptions(arg_desc.release());
}

void 
CBlast2seqApplication::InitObjMgr(void)
{
    m_ObjMgr = CObjectManager::GetInstance();
    if (!m_ObjMgr) {
         throw std::runtime_error("Could not initialize object manager");
    }
}

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
        }
        if (args["templtype"].AsInteger()) {
            opt.SetMBTemplateType(args["templtype"].AsInteger());
        }
        // Setting seed extension method involves changing the scanning 
        // stride as well, which is handled in the derived 
        // CBlastNucleotideOptionsHandle class, but not in the base
        // CBlastOptionsHandle class.
        CBlastNucleotideOptionsHandle* nucl_handle =
            dynamic_cast<CBlastNucleotideOptionsHandle*>(retval);
        switch(args["scantype"].AsInteger()) {
        case 1:
            nucl_handle->SetSeedExtensionMethod(eRightAndLeft);
            break;
        case 2:
            nucl_handle->SetSeedExtensionMethod(eRight);
            break;
        case 3:
            nucl_handle->SetSeedExtensionMethod(eUpdateDiag);
            break;
        default:
            break;
        }
        nucl_handle->SetSeedContainerType(eDiagArray);

        opt.SetVariableWordSize(args["varword"].AsBoolean());

        // Override the scan step value if it is set by user
        if (args["stride"].AsInteger()) {
            opt.SetScanStep(args["stride"].AsInteger());
        }
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
        opt.SetGapTracebackAlgorithm(eSkipTbck);
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

/*****************************************************************************/

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

        if (program == eBlastn || program == eBlastx) { 
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
        if (args["asnout"]) {
            auto_ptr<CObjectOStream> asnout(
                CObjectOStream::Open(args["asnout"].AsString(), eSerial_AsnText));
            for (unsigned int index = 0; index < seqalignv.size(); ++index)
                *asnout << *seqalignv[index];
        }

    } catch (const CException& e) {
        cerr << e.what() << endl;
    }

    return 0;
}

void CBlast2seqApplication::Exit(void)
{
    SetDiagStream(0);
}


int main(int argc, const char* argv[])
{
    return CBlast2seqApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.52  2004/08/18 18:14:13  camacho
 * Remove GetProgramFromBlastProgramType, add ProgramNameToEnum
 *
 * Revision 1.51  2004/08/17 17:22:01  dondosha
 * Removed call to register GenBank loader in object manager
 *
 * Revision 1.50  2004/08/11 15:25:14  dondosha
 * Use appropriate derived class for options handle in case of megablast and discontiguous megablast; use scantype argument 0 for default setting and 1-3 for specific types
 *
 * Revision 1.49  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.48  2004/07/06 15:52:07  dondosha
 * Distinguish between 2 different enumerations for program
 *
 * Revision 1.47  2004/06/08 15:21:44  dondosha
 * Set traceback algorithm properly
 *
 * Revision 1.46  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.45  2004/05/19 14:52:02  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.44  2004/05/17 15:33:57  madden
 * Int algorithm_type replaced with enum EBlastPrelimGapExt
 *
 * Revision 1.43  2004/04/30 15:56:31  papadopo
 * Plus/minus/both strands are acceptable for any blast program
 * that takes a nucleotide query
 *
 * Revision 1.42  2004/04/23 13:51:56  papadopo
 * handle strands for blastx correctly
 *
 * Revision 1.41  2004/04/19 21:35:23  papadopo
 * explicitly calculate strands for input sequences
 *
 * Revision 1.40  2004/03/26 18:50:32  camacho
 * Use CException::what() in catch block
 *
 * Revision 1.39  2004/03/17 20:09:08  dondosha
 * Use CBlastNucleotideOptionsHandle method to set both extension method and scan step, instead of directly calling CalculateBestStride
 *
 * Revision 1.38  2004/03/11 17:27:41  camacho
 * Minor change to avoid confusing doxygen
 *
 * Revision 1.37  2004/03/09 18:55:34  dondosha
 * Fix: set out-of-frame mode boolean option in addition to the frame shift penalty
 *
 * Revision 1.36  2004/02/13 03:31:51  camacho
 * 1. Use CBlastOptionsHandle class (still needs some work)
 * 2. Remove dead code, clean up, add @todo doxygen tags
 *
 * Revision 1.35  2004/01/05 18:50:27  vasilche
 * Fixed path to include files.
 *
 * Revision 1.34  2003/12/31 20:05:58  dondosha
 * For discontiguous megablast, set extension method and scanning stride correctly
 *
 * Revision 1.33  2003/12/09 15:13:58  camacho
 * Use difference scopes for queries and subjects
 *
 * Revision 1.32  2003/12/04 17:07:51  camacho
 * Remove yet another unused variable
 *
 * Revision 1.31  2003/11/26 18:36:45  camacho
 * Renaming blast_option*pp -> blast_options*pp
 *
 * Revision 1.30  2003/11/26 18:24:32  camacho
 * +Blast Option Handle classes
 *
 * Revision 1.29  2003/11/03 15:20:39  camacho
 * Make multiple query processing the default for Run().
 *
 * Revision 1.28  2003/10/27 20:52:29  dondosha
 * Made greedy option an integer, to specify number of extension stages
 *
 * Revision 1.27  2003/10/24 20:55:30  camacho
 * Rename GetDefaultStride
 *
 * Revision 1.26  2003/10/22 16:48:09  dondosha
 * Changed "ag" option to "scantype";
 * Use function from core library to calculate default value of stride if AG
 * scanning method is used.
 *
 * Revision 1.25  2003/10/21 22:15:33  camacho
 * Rearranging of C options structures, fix seed extension method
 *
 * Revision 1.24  2003/10/21 17:34:34  camacho
 * Renaming of gap open/extension accessors/mutators
 *
 * Revision 1.23  2003/10/17 18:22:28  dondosha
 * Use separate variables for different initial word extension options
 *
 * Revision 1.22  2003/10/08 15:27:02  camacho
 * Remove unnecessary conditional
 *
 * Revision 1.21  2003/10/07 17:37:10  dondosha
 * Lower case mask is now a boolean argument in call to BLASTGetSeqLocFromStream
 *
 * Revision 1.20  2003/09/26 21:36:29  dondosha
 * Show results for all queries in multi-query case
 *
 * Revision 1.19  2003/09/26 15:42:23  dondosha
 * Added second argument to SetExtendWordMethod, so bit can be set or unset
 *
 * Revision 1.18  2003/09/11 17:46:16  camacho
 * Changed CBlastOption -> CBlastOptions
 *
 * Revision 1.17  2003/09/09 15:43:43  ucko
 * Fix #include directive for blast_input.hpp.
 *
 * Revision 1.16  2003/09/05 18:24:28  camacho
 * Restoring printing of SeqAlign, fix setting of default word extension method
 *
 * Revision 1.15  2003/08/28 23:17:20  camacho
 * Add processing of command-line options
 *
 * Revision 1.14  2003/08/19 20:36:44  dondosha
 * EProgram enum type is no longer part of CBlastOptions class
 *
 * Revision 1.13  2003/08/18 20:58:57  camacho
 * Added blast namespace, removed *__.hpp includes
 *
 * Revision 1.12  2003/08/18 17:07:42  camacho
 * Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
 * Change in function to read seqlocs from files.
 *
 * Revision 1.11  2003/08/15 16:03:00  dondosha
 * TSeqLoc and TSeqLocVector types no longer belong to class CBl2Seq, but are common to all BLAST applications
 *
 * Revision 1.10  2003/08/11 20:16:43  camacho
 * Change return type of BLASTGetSeqLocFromStream and fix namespaces
 *
 * Revision 1.9  2003/08/11 15:26:30  dondosha
 * BLASTGetSeqLocFromStream function moved to blast_input.cpp
 *
 * Revision 1.8  2003/08/08 20:46:08  camacho
 * Fix to use new ReadFasta arguments
 *
 * Revision 1.7  2003/08/08 20:24:31  dicuccio
 * Adjustments for Unix build: rename 'ncmimath' -> 'ncbi_math'; fix #include in demo app
 *
 * Revision 1.6  2003/08/01 22:38:31  camacho
 * Added conditional compilation to write seqaligns
 *
 * Revision 1.5  2003/07/30 16:33:31  madden
 * Remove C toolkit dependencies
 *
 * Revision 1.4  2003/07/16 20:25:34  camacho
 * Added dummy features argument to C formatter
 *
 * Revision 1.3  2003/07/14 21:53:32  camacho
 * Minor
 *
 * Revision 1.2  2003/07/11 21:22:57  camacho
 * Use same command line option as blast to display seqalign
 *
 * Revision 1.1  2003/07/10 18:35:58  camacho
 * Initial revision
 *
 * ===========================================================================
 */
