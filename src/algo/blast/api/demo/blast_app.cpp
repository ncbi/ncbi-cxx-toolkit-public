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
#include <objmgr/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <algo/blast/api/blast_option.hpp>
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
    int BlastSearch(void);
    void RegisterBlastDbLoader(char* dbname, bool is_na);

    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>         m_Scope;
    CBlastOption*        m_Options;
    BlastSeqSrc *        m_bssp;
    CBlastFormatOptions* m_format_options;
    TSeqLocVector        m_query;
    BlastQueryInfo*      m_query_info;
    BlastScoreBlk*       m_sbp;
    BlastMask*           m_filter_loc;/* Masking/filtering locations */
    BlastReturnStat*     m_return_stats;
    TSeqAlignVector     m_seqalign;
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

void CBlastApplication::SetOptions(const CArgs& args)
{
    EProgram program = m_Options->GetProgram();

    bool ag_blast = TRUE, variable_wordsize = FALSE;
    int lut;
    bool mb_lookup = FALSE;
    bool greedy_extension = FALSE;
    Int8 totlen = 0;
    int numseqs = 0;
    bool db_is_na, translated_db;
    
    db_is_na = (program == eBlastn || 
                program == eTblastn || 
                program == eTblastx);

    translated_db = (program == eTblastn ||
                     program == eTblastx);

    /* The following options are for blastn only */
    if (program == eBlastn) {
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

    BLAST_FillLookupTableOptions(m_Options->GetLookupTableOpts(), program, mb_lookup,
        args["thresh"].AsInteger(), args["word"].AsInteger(), 
        ag_blast, variable_wordsize);
    /* Fill the rest of the lookup table options */
    m_Options->SetMBTemplateLength(args["templen"].AsInteger());
    m_Options->SetMBTemplateType(args["templtype"].AsInteger());

    if (args["stride"])
        m_Options->SetScanStep(args["stride"].AsInteger());
    
    BLAST_FillQuerySetUpOptions(m_Options->GetQueryOpts(), program, 
        args["filter"].AsString().c_str(), args["stride"].AsInteger());
    if (args["gencode"].AsInteger() &&
       (program == eBlastx || 
        program == eTblastx))
      m_Options->SetQueryGeneticCode(args["gencode"].AsInteger());
   
    BLAST_FillInitialWordOptions(m_Options->GetInitWordOpts(), program, 
        greedy_extension, args["window"].AsInteger(), variable_wordsize, 
        ag_blast, mb_lookup, args["xungap"].AsDouble());

    BLAST_FillExtensionOptions(m_Options->GetExtensionOpts(), program, greedy_extension, 
        args["xgap"].AsDouble(), args["xfinal"].AsDouble());

    BLAST_FillScoringOptions(m_Options->GetScoringOpts(), program, greedy_extension, 
        args["mismatch"].AsInteger(), args["match"].AsInteger(),
        args["matrix"].AsString().c_str(), args["gopen"].AsInteger(),
        args["gext"].AsInteger());

    m_Options->SetMatrixPath(BLASTGetMatrixPath(m_Options->GetMatrixName(), 
                                 (program != eBlastn)));

    m_Options->SetFrameShiftPenalty(args["frameshift"].AsInteger());

    BLAST_FillHitSavingOptions(m_Options->GetHitSavingOpts(), !args["ungapped"].AsBoolean(), 
        args["evalue"].AsDouble(), 
        MAX(args["descr"].AsInteger(), 
            args["align"].AsInteger()));
 
    m_Options->SetPercentIdentity(args["perc"].AsDouble());
    m_Options->SetLongestIntronLength(args["maxintron"].AsInteger());
   
    totlen =  BLASTSeqSrcGetTotLen(m_bssp);
    numseqs = BLASTSeqSrcGetNumSeqs(m_bssp);

    BLAST_FillEffectiveLengthsOptions(m_Options->GetEffLenOpts(), numseqs, totlen, 
                                      (Int8)args["searchsp"].AsDouble());
    
    
    if (translated_db) {
        if (args["dbgencode"].AsInteger())
            m_Options->SetDbGeneticCode(args["dbgencode"].AsInteger());

        CSeq_data gc_ncbieaa(
                CGen_code_table::GetNcbieaa(args["dbgencode"].AsInteger()),
                CSeq_data::e_Ncbieaa);
        CSeq_data gc_ncbistdaa;

        TSeqPos nconv = CSeqportUtil::Convert(gc_ncbieaa, &gc_ncbistdaa,
                CSeq_data::e_Ncbistdaa);

        _ASSERT(gc_ncbistdaa.IsNcbistdaa());
        _ASSERT(nconv == gc_ncbistdaa.GetNcbistdaa().Get().size());

        Uint1* gen_code_string = (Uint1*) malloc(sizeof(Uint1)*nconv);
        for (unsigned int i = 0; i < nconv; i++)
            gen_code_string[i] = 
                gc_ncbistdaa.GetNcbistdaa().Get()[i];

        m_Options->SetDbGeneticCodeStr(gen_code_string);
    }
}

int CBlastApplication::BlastSearch()
{
    EProgram program = m_Options->GetProgram();
    QuerySetUpOptions* query_options = m_Options->GetQueryOpts();
    BLAST_SequenceBlk* query_blk = NULL;
    ListNode* lookup_segments = NULL;
    BlastResults* results = NULL;
    Blast_Message* blast_message = NULL;
    LookupTableWrap* lookup_wrap;
    int status;

    bool translated_query = (program == eBlastx || 
                             program == eTblastx);

    if (translated_query) {
        /* Translated lower case mask must be converted to protein 
           coordinates here */
        BlastMaskDNAToProtein(&query_options->lcase_mask, m_query);
    }

    status = BLAST_SetUpQuery(program, m_query, query_options,
                              &m_query_info, &query_blk);
    m_sbp = 0;
    status = 
        BLAST_MainSetUp(program, query_options,
            m_Options->GetScoringOpts(), m_Options->GetLookupTableOpts(), 
            m_Options->GetHitSavingOpts(), query_blk, m_query_info, 
            &lookup_segments, &m_filter_loc, &m_sbp, &blast_message);

    if (translated_query) {
        /* Filter locations were returned in protein coordinates; 
           convert them back to nucleotide here */
        BlastMaskProteinToDNA(&m_filter_loc, m_query);
    }
        
    if (status) {
        ERR_POST(Error << "Setup returned status " << status << ".");
        return status;
    }

    BLAST_ResultsInit(m_query_info->num_queries, &results);
    LookupTableWrapInit(query_blk, m_Options->GetLookupTableOpts(), 
                        lookup_segments, m_sbp, &lookup_wrap);
    
    m_return_stats = (BlastReturnStat*) calloc(1, sizeof(BlastReturnStat));

    BLAST_DatabaseSearchEngine(program, query_blk, m_query_info, 
         m_bssp, m_sbp, m_Options->GetScoringOpts(), lookup_wrap, 
         m_Options->GetInitWordOpts(), m_Options->GetExtensionOpts(), 
         m_Options->GetHitSavingOpts(), 
         m_Options->GetEffLenOpts(), NULL, m_Options->GetDbOpts(), results, 
         m_return_stats);

    lookup_wrap = BlastLookupTableDestruct(lookup_wrap);
    query_blk = BlastSequenceBlkFree(query_blk);

    /* The following works because the ListNodes' data point to simple
       double-integer structures */
    lookup_segments = ListNodeFreeData(lookup_segments);

    // Convert results to the SeqAlign form 
    m_seqalign = BLAST_Results2CSeqAlign(results, program, m_query,
                     m_bssp, 0, m_Options->GetScoringOpts(), m_sbp,
                     m_Options->GetGappedMode());

    results = BLAST_ResultsFree(results);

    return status;
}

int CBlastApplication::Run(void)
{
    Uint1 program_number;
    bool db_is_na;
    int status;
    BlastSeqSrcNewInfo bssn_info;
    ReaddbNewArgs readdb_args;

    // Process command line args
    const CArgs& args = GetArgs();
    
    BlastProgram2Number(args["program"].AsString().c_str(), &program_number);
    EProgram e_program = BLASTGetEProgram(program_number);

    db_is_na = (e_program == eBlastn || 
                e_program == eTblastn || 
                e_program == eTblastx);

    readdb_args.dbname = const_cast<char*>(args["db"].AsString().c_str());
    readdb_args.is_protein = !db_is_na;
    
    bssn_info.constructor = &ReaddbSeqSrcNew;
    bssn_info.ctor_argument = (void*) &readdb_args;
    m_bssp = BlastSeqSrcNew(&bssn_info);

    m_Options = new CBlastOption(e_program);
    SetOptions(args);
    
    m_format_options = 
        new CBlastFormatOptions(e_program, args["out"].AsOutputFile());

    m_format_options->SetAlignments(args["align"].AsInteger());
    m_format_options->SetDescriptions(args["descr"].AsInteger());
    m_format_options->SetAlignView(args["format"].AsInteger());
    m_format_options->SetHtml(args["html"].AsBoolean());

#ifdef C_FORMATTING
    if (readdb_args.dbname) {
        BLAST_PrintOutputHeader(format_options, args["greedy"].AsBoolean(), 
                                readdb_args.dbname, db_is_na);
    }
#endif

    ENa_strand strand = (ENa_strand) args["strand"].AsInteger();
    Int4 from = args["qstart"].AsInteger();
    Int4 to = args["qend"].AsInteger();

    InitScope();

    int id_counter = 0;
    // Read the query(ies) from input file; perform the setup
    if (args["lcase"]) {
        m_query = BLASTGetSeqLocFromStream(args["query"].AsInputFile(),
                      m_Scope, strand, from, to, &id_counter, 
                      &m_Options->GetQueryOpts()->lcase_mask);
    } else {
        m_query = BLASTGetSeqLocFromStream(args["query"].AsInputFile(),
                        m_Scope, strand, from, to, &id_counter);
    }

    status = BlastSearch();

    if (args["asnout"]) {
        int query_index;
        // Convert CSeq_align_set to linked list of SeqAlign structs
        DECLARE_ASN_CONVERTER(CSeq_align, SeqAlign, converter);
        SeqAlignPtr salp = NULL, tmp = NULL, tail = NULL;
        
        for (query_index = 0; query_index < m_seqalign.size();
             ++query_index)
        {
            ITERATE(list< CRef<CSeq_align> >, itr, 
                    m_seqalign[query_index]->Get())
            {
                tmp = converter.ToC(**itr);
                
                if (!salp)
                    salp = tail = tmp;
                else {
                    tail->next = tmp;
                    while (tail->next)
                        tail = tail->next;
                }
            }
        }

        AsnIoPtr aip = 
            AsnIoOpen((char*)args["asnout"].AsString().c_str(), (char*)"w");
        GenericSeqAlignSetAsnWrite(salp, aip);
        AsnIoReset(aip);
        AsnIoClose(aip);
    }

    RegisterBlastDbLoader(readdb_args.dbname, !readdb_args.is_protein);
    /* Format the results */
    status = BLAST_FormatResults(m_seqalign, 
                 e_program, m_query, m_filter_loc, 
                 m_format_options, 
                 m_Options->GetOutOfFrameMode());
    
#ifdef C_FORMATTING
    PrintOutputFooter(program_number, format_options, score_options, 
        m_sbp, lookup_options, word_options, ext_options, hit_options, 
        m_query_info, bssp, return_stats);
#endif

    return status;
}

void CBlastApplication::Exit(void)
{
    m_filter_loc = BlastMaskFree(m_filter_loc);
    m_query_info = BlastQueryInfoFree(m_query_info);
    m_sbp = BlastScoreBlkFree(m_sbp);

    sfree(m_return_stats);

    SetDiagStream(0);
}


int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
