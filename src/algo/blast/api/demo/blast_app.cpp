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
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/db_blast.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include "blast_input.hpp"
#include <algo/blast/api/seqsrc_readdb.h>
#include <objtools/alnmgr/util/blast_format.hpp>

#ifndef NCBI_C_TOOLKIT
#define NCBI_C_TOOLKIT
#endif

// C include files
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_engine.h>

// For writing out seqalign only
#include <ctools/asn_converter.hpp>
#include <objalign.h>

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

class CBlastApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int Run(void);
    virtual void Exit(void);
    void InitScope(void);
    void InitOptions(void);
    void SetOptions(const CArgs& args);
    void ProcessCommandLineArgs(CBlastOptions& opt, BlastSeqSrc* bssp);
    int BlastSearch(void);
    void RegisterBlastDbLoader(char* dbname, bool is_na);
    void FormatResults(CDbBlast& blaster, TSeqAlignVector& seqalignv);
    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>         m_Scope;
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
        "Method for scanning the database: 0 traditional, 1 AG",
        CArgDescriptions::eInteger, "1");
    arg_desc->SetConstraint("scantype", new CArgAllow_Integers(0,1));

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
    arg_desc->AddDefaultKey("hitlist", "hitlist_size",
        "How many best matching sequences to find?",
        CArgDescriptions::eInteger, "500");
    arg_desc->AddDefaultKey("descr", "descriptions",
        "How many matching sequence descriptions to show?",
        CArgDescriptions::eInteger, "500");
    arg_desc->AddDefaultKey("align", "alignments", 
        "How many matching sequence alignments to show?",
        CArgDescriptions::eInteger, "250");
    arg_desc->AddOptionalKey("out", "outfile", 
        "File name for writing output",
        CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey("format", "format", 
        "How to format the results?",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("html", "html", "Produce HTML output?",
                            CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("gencode", "gencode", "Query genetic code",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("dbgencode", "dbgencode", "Database genetic code",
                            CArgDescriptions::eInteger, "1");
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

    arg_desc->AddOptionalKey("pattern", "phipattern",
                            "Pattern for PHI-BLAST",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("pssm", "pssm", 
        "File name for uploading a PSSM for PSI BLAST seach",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("dbrange", "databaserange",
                            "Range of ordinal ids in the BLAST database.\n"
                             "Format: \"oid1 oid2\"",
                             CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}

void 
CBlastApplication::InitScope(void)
{
    if (m_Scope.Empty()) {
        m_ObjMgr.Reset(new CObjectManager());
        m_ObjMgr->RegisterDataLoader(*new CGBDataLoader("ID", 0, 2),
                CObjectManager::eDefault);

        m_Scope.Reset(new CScope(*m_ObjMgr));
        m_Scope->AddDefaults();
        _TRACE("Blast2seqApp: Initializing scope");
    }
}

void 
CBlastApplication::RegisterBlastDbLoader(char *dbname, bool db_is_na)
{
    m_ObjMgr->RegisterDataLoader(*new CBlastDbDataLoader("BLASTDB", dbname, 
                  db_is_na? (CBlastDbDataLoader::eNucleotide) : 
                            (CBlastDbDataLoader::eProtein)),  
                  CObjectManager::eDefault);
}

void
CBlastApplication::ProcessCommandLineArgs(CBlastOptions& opt, 
                                              BlastSeqSrc* bssp)
{
    CArgs args = GetArgs();

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
        opt.SetVariableWordSize(args["varword"].AsBoolean());
        switch(args["scantype"].AsInteger()) {
        case 1:
            opt.SetSeedExtensionMethod(eRightAndLeft);
            opt.SetScanStep(CalculateBestStride(opt.GetWordSize(),
                                                opt.GetVariableWordSize(), 
                                                opt.GetLookupTableType()));
            break;
        default:
            opt.SetSeedExtensionMethod(eRight);
            break;
        }
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
        opt.SetGapExtnAlgorithm(EXTEND_GREEDY);
        opt.SetUngappedExtension(false);
        break;
    case 2: /* Two-step greedy extension, no ungapped extension */
        opt.SetGapExtnAlgorithm(EXTEND_GREEDY_NO_TRACEBACK);
        opt.SetUngappedExtension(false);
        break;
    case 3: /* Two-step greedy extension after ungapped extension*/
        opt.SetGapExtnAlgorithm(EXTEND_GREEDY_NO_TRACEBACK);
        opt.SetUngappedExtension(true);
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
    } else if (bssp) {
        opt.SetDbLength(BLASTSeqSrcGetTotLen(bssp));
        opt.SetDbSeqNum(BLASTSeqSrcGetNumSeqs(bssp));
    }

    if (args["hitlist"].AsInteger()) {
        opt.SetHitlistSize(args["hitlist"].AsInteger());
        /* Hitlist size for preliminary alignments is increased, unless 
           no traceback is performed. */
        if (args["ungapped"].AsBoolean() || args["greedy"].AsInteger() == 1) {
            opt.SetPrelimHitlistSize(args["hitlist"].AsInteger());
        } else {
            opt.SetPrelimHitlistSize(MIN(2*args["hitlist"].AsInteger(),
                                         args["hitlist"].AsInteger() + 50));
        }
    }

    if (args["perc"].AsDouble()) {
        opt.SetPercentIdentity(args["perc"].AsDouble());
    }

    if (args["gencode"].AsInteger()) {
        opt.SetQueryGeneticCode(args["gencode"].AsInteger());
    }
  
    if ((program == "tblastn" || program == "tblastx") &&
        args["dbgencode"].AsInteger() != BLAST_GENETIC_CODE) {
        opt.SetDbGeneticCode(args["dbgencode"].AsInteger());
    }

    if (args["maxintron"].AsInteger()) {
        opt.SetLongestIntronLength(args["maxintron"].AsInteger());
    }
    if (args["frameshift"].AsInteger()) {
        opt.SetFrameShiftPenalty(args["frameshift"].AsInteger());
        opt.SetOutOfFrameMode();
    }

    if (args["pattern"]) {
        opt.SetPHIPattern(args["pattern"].AsString().c_str(),
                                 (program == "blastn"));
    }

    if (args["pssm"]) {
#if 0
        FILE* pssmfp = fopen(args["pssm"].AsInputFile(), "r");
        // Read the pssm string and fill the posMatrix
        bool use_pssm = TRUE;
        fclose(pssmfp);
#endif
    }

    return;
}

static CDisplaySeqalign::TranslatedFrames 
Context2TranslatedFrame(int context)
{
    switch (context) {
    case 1: return CDisplaySeqalign::ePlusStrand1;
    case 2: return CDisplaySeqalign::ePlusStrand2;
    case 3: return CDisplaySeqalign::ePlusStrand3;
    case 4: return CDisplaySeqalign::eMinusStrand1;
    case 5: return CDisplaySeqalign::eMinusStrand2;
    case 6: return CDisplaySeqalign::eMinusStrand3;
    default: return CDisplaySeqalign::eFrameNotSet;
    }
}

#define NUM_FRAMES 6

static TSeqLocInfoVector
BlastMaskLoc2CSeqLoc(const BlastMaskLoc* mask, const TSeqLocVector& slp,
    EProgram program)
{
    TSeqLocInfoVector retval;
    int frame, num_frames;
    bool translated_query;
    int index;

    translated_query = (program == eBlastx ||
                        program == eTblastx);

    num_frames = (translated_query ? NUM_FRAMES : 1);

    TSeqLocInfo mask_info_list;

    for (index = 0; index < (int)slp.size(); ++index) {
        mask_info_list.clear();

        if (!mask) {
            retval.push_back(mask_info_list);
            continue;
        }
        for ( ; mask && mask->index < index*num_frames;
              mask = mask->next);
        BlastSeqLoc* loc;
        CDisplaySeqalign::SeqlocInfo* seqloc_info =
            new CDisplaySeqalign::SeqlocInfo;

        for ( ; mask && mask->index < (index+1)*num_frames;
              mask = mask->next) {
            frame = (translated_query ? (mask->index % num_frames) + 1 : 0);


            for (loc = mask->loc_list; loc; loc = loc->next) {
                seqloc_info->frame = Context2TranslatedFrame(frame);
                CRef<CSeq_loc> seqloc(new CSeq_loc());
                seqloc->SetInt().SetFrom(((DoubleInt*) loc->ptr)->i1);
                seqloc->SetInt().SetTo(((DoubleInt*) loc->ptr)->i2);
                seqloc->SetInt().SetId(*(const_cast<CSeq_id*>(&sequence::GetId(*
slp[index].seqloc, slp[index].scope))));

                seqloc_info->seqloc = seqloc;
                mask_info_list.push_back(seqloc_info);
            }
        }
        retval.push_back(mask_info_list);
    }

    return retval;
}

void CBlastApplication::FormatResults(CDbBlast& blaster, 
                                      TSeqAlignVector& seqalignv)
{
    CArgs args = GetArgs();

    if (args["asnout"]) {
        auto_ptr<CObjectOStream> asnout(
            CObjectOStream::Open(args["asnout"].AsString(), eSerial_AsnText));
        unsigned int query_index;
        for (query_index = 0; query_index < seqalignv.size(); ++query_index)
        {
            if (!seqalignv[query_index]->IsSet())
                continue;
            *asnout << *seqalignv[query_index];
        }
    }

    if (args["out"]) {
        EProgram program = blaster.SetOptions().GetProgram();
        char* dbname = const_cast<char*>(args["db"].AsString().c_str());
        bool db_is_na = (program == eBlastn || program == eTblastn || 
                         program == eTblastx);
        CBlastFormatOptions* format_options = 
            new CBlastFormatOptions(program, args["out"].AsOutputFile());
        
        format_options->SetAlignments(args["align"].AsInteger());
        format_options->SetDescriptions(args["descr"].AsInteger());
        format_options->SetAlignView(args["format"].AsInteger());
        format_options->SetHtml(args["html"].AsBoolean());
        
#ifdef C_FORMATTING
        if (dbname) {
            BLAST_PrintOutputHeader(format_options, 
                args["greedy"].AsBoolean(), dbname, db_is_na);
        }
#endif
        
        RegisterBlastDbLoader(dbname, db_is_na);
        /* Format the results */
        TSeqLocInfoVector maskv =
            BlastMaskLoc2CSeqLoc(blaster.GetFilteredQueryRegions(), 
                              blaster.GetQueries(), program);
        
        if (BLAST_FormatResults(seqalignv, program, blaster.GetQueries(), 
                maskv, format_options, blaster.GetOptions().GetOutOfFrameMode())) {
            NCBI_THROW(CBlastException, eInternal, 
                       "Error in formatting results");
        }
        
#ifdef C_FORMATTING
        PrintOutputFooter(program, format_options, score_options, 
            m_sbp, lookup_options, word_options, ext_options, hit_options, 
            blaster.GetQueryInfo(), dbname, blaster.GetReturnStats());
#endif
        
    }
}

int CBlastApplication::Run(void)
{
    Uint1 program_number;
    int status = 0;

    // Process command line args
    const CArgs& args = GetArgs();
    
    BlastProgram2Number(args["program"].AsString().c_str(), &program_number);
    EProgram program = static_cast<EProgram>(program_number);

    ENa_strand strand = (ENa_strand) args["strand"].AsInteger();
    Int4 from = args["qstart"].AsInteger();
    Int4 to = args["qend"].AsInteger();
    Int4 first_oid = 0;
    Int4 last_oid = 0;

    InitScope();

    int id_counter = 0;
    // Read the query(ies) from input file; perform the setup
    TSeqLocVector query_loc = 
        BLASTGetSeqLocFromStream(args["query"].AsInputFile(),
                  *m_ObjMgr, strand, from, to, &id_counter, 
                  args["lcase"].AsBoolean());

    if (args["dbrange"]) {
        char* delimiters = " ,:;";
        char* range_str = strdup(args["dbrange"].AsString().c_str());
        char* ptr = NULL;
        first_oid =
            atoi(strtok_r(range_str, delimiters, &ptr));
        last_oid = atoi(ptr);
        sfree(range_str);
    }

    BlastSeqSrc* seq_src = 
        ReaddbBlastSeqSrcInit(args["db"].AsString().c_str(), 
                              (program == eBlastp || program == eBlastx),
                              first_oid, last_oid, NULL);
                                                 

    CDbBlast blaster(query_loc, seq_src, program);
    ProcessCommandLineArgs(blaster.SetOptions(), seq_src);

    TSeqAlignVector seqalignv = blaster.Run();

    FormatResults(blaster, seqalignv);

    return status;
}

void CBlastApplication::Exit(void)
{
    SetDiagStream(0);
}


int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
