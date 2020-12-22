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
 * Author: Irena Zaretskaya
 *
 */

/** @file blast_report.cpp
 * Stand-alone command line HTML report for BLAST. Uses tempalates for descriptions and alignments. Outputs metadata as json
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/iterator.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include "blast_app_util.hpp"


#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif


#define EXIT_CODE__FORMAT_SUCCESS 0
#define EXIT_CODE__NO_RESULTS_FOUND 1
#define EXIT_CODE__INVALID_INPUT_FORMAT 2
#define EXIT_CODE__BLAST_ARCHIVE_ERROR 3
#define EXIT_CODE__CANNOT_ACCESS_FILE 4
#define EXIT_CODE__QUERY_INDEX_INVALID 5
#define EXIT_CODE__NETWORK_CONNECTION_ERROR 5

class CBlastReportApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastReportApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
	m_LoadFromArchive = false;
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
    
    /// Prints the BLAST formatted output
    int PrintFormattedOutput(void);

    /// Extracts the queries to be formatted
    /// @param query_is_protein Are the queries protein sequences? [in]
    CRef<CBlastQueryVector> x_ExtractQueries(bool query_is_protein);

    /// Build the query from a PSSM
    /// @param pssm PSSM to inspect [in]
    CRef<CBlastSearchQuery> 
    x_BuildQueryFromPssm(const CPssmWithParameters& pssm);

    /// Package a scope and Seq-loc into a SSeqLoc from a Bioseq
    /// @param bioseq Bioseq to inspect [in]
    /// @param scope Scope object to add the sequence data to [in|out]
    SSeqLoc x_QueryBioseqToSSeqLoc(const CBioseq& bioseq, CRef<CScope> scope);

    /// Our link to the NCBI BLAST service
    CRef<CRemoteBlast> m_RmtBlast;

    /// The source of CScope objects for queries
    CRef<CBlastScopeSource> m_QueryScopeSource;

    /// Tracks whether results come from an archive file.
    bool m_LoadFromArchive;
};

void CBlastReportApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                  "Stand-alone BLAST formatter client, version " 
                  + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("Input options");
    

    // add input file for seq-align here?
    arg_desc->AddKey(kArgArchive, "ArchiveFile", "File containing BLAST Archive format in ASN.1 (i.e.: output format 11)", 
                     CArgDescriptions::eInputFile);
            
    arg_desc->AddDefaultKey(kArgAlignSeqList, "alignseqlist", "List of comma separated seqids to display", CArgDescriptions::eString, "");
    arg_desc->AddDefaultKey(kArgMetadata, "searchmetadata", "Search Metadata indicator", CArgDescriptions::eBoolean,"f");
    arg_desc->AddDefaultKey(kArgQueryIndex, "queryindex", "Query Index", CArgDescriptions::eInteger, "0");
    

    CFormattingArgs fmt_args;
    fmt_args.SetArgumentDescriptions(*arg_desc);

    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey(kArgOutput, "output_file", "Output file name", 
                            CArgDescriptions::eOutputFile, "-");

    arg_desc->SetCurrentGroup("Miscellaneous options");
    arg_desc->AddFlag(kArgParseDeflines,
                 "Should the query and subject defline(s) be parsed?", true);
    arg_desc->SetCurrentGroup("");

    CDebugArgs debug_args;
    debug_args.SetArgumentDescriptions(*arg_desc);

    SetupArgDescriptions(arg_desc.release());
}

SSeqLoc
CBlastReportApp::x_QueryBioseqToSSeqLoc(const CBioseq& bioseq, 
                                           CRef<CScope> scope)
{
    static bool first_time = true;
    _ASSERT(scope);

    if ( !HasRawSequenceData(bioseq) && first_time ) {
        _ASSERT(m_QueryScopeSource);
        m_QueryScopeSource->AddDataLoaders(scope);
        first_time = false;
    }
    else {
        scope->AddBioseq(bioseq);
    }
    CRef<CSeq_loc> seqloc(new CSeq_loc);
    seqloc->SetWhole().Assign(*bioseq.GetFirstId());
    return SSeqLoc(seqloc, scope);
}

CRef<CBlastSearchQuery>
CBlastReportApp::x_BuildQueryFromPssm(const CPssmWithParameters& pssm)
{
    if ( !pssm.HasQuery() ) {
        throw runtime_error("PSSM has no query");
    }
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    const CSeq_entry& seq_entry = pssm.GetQuery();
    if ( !seq_entry.IsSeq() ) {
        throw runtime_error("Cannot have multiple queries in a PSSM");
    }
    SSeqLoc ssl = x_QueryBioseqToSSeqLoc(seq_entry.GetSeq(), scope);
    CRef<CBlastSearchQuery> retval;
    retval.Reset(new CBlastSearchQuery(*ssl.seqloc, *ssl.scope));
    _ASSERT(ssl.scope.GetPointer() == scope.GetPointer());
    return retval;
}

CRef<CBlastQueryVector>
CBlastReportApp::x_ExtractQueries(bool query_is_protein)
{
    CRef<CBlast4_queries> b4_queries = m_RmtBlast->GetQueries();
    _ASSERT(b4_queries);
    const size_t kNumQueries = b4_queries->GetNumQueries();

    CRef<CBlastQueryVector> retval(new CBlastQueryVector);

    SDataLoaderConfig dlconfig(query_is_protein, SDataLoaderConfig::eUseNoDataLoaders);
    dlconfig.OptimizeForWholeLargeSequenceRetrieval(false);
    m_QueryScopeSource.Reset(new CBlastScopeSource(dlconfig));

    if (b4_queries->IsPssm()) {
        retval->AddQuery(x_BuildQueryFromPssm(b4_queries->GetPssm()));
    } else if (b4_queries->IsSeq_loc_list()) {
        CRef<CScope> scope = m_QueryScopeSource->NewScope();
        ITERATE(CBlast4_queries::TSeq_loc_list, seqloc,
                b4_queries->GetSeq_loc_list()) {
            _ASSERT( !(*seqloc)->GetId()->IsLocal() );
            CRef<CBlastSearchQuery> query(new CBlastSearchQuery(**seqloc,
                                                                *scope));
            retval->AddQuery(query);
        }
    } else if (b4_queries->IsBioseq_set()) {
        CTypeConstIterator<CBioseq> itr(ConstBegin(b4_queries->GetBioseq_set(),
                                                   eDetectLoops));
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        for (; itr; ++itr) {
            SSeqLoc ssl = x_QueryBioseqToSSeqLoc(*itr, scope);
            CRef<CBlastSearchQuery> query(new CBlastSearchQuery(*ssl.seqloc,
                                                                *ssl.scope));
            retval->AddQuery(query);
        }
    }

    (void)kNumQueries;  // eliminate compiler warning;
    _ASSERT(kNumQueries == retval->size());
    return retval;
}

static int s_GetError(string errorName, string defaultMessage, int defaultErrCode, string &errorMsg,string blastArchName = "")
{
    CNcbiApplication* app = CNcbiApplication::Instance();
    string message = defaultMessage;
    int status = 0;
    if (app)  {
        const CNcbiRegistry& registry = app->GetConfig();
        string errorCode;
        string errorInfo = registry.Get("Errors", errorName);
        if(!errorInfo.empty()) {
            NStr::SplitInTwo(errorInfo, ":", errorCode, message);
            status = NStr::StringToInt(errorCode,NStr::fConvErr_NoThrow);
            message = NStr::Replace(message,"#filename",blastArchName);            
        }        
    }
    if(!status || message.empty()) {
        errorMsg = defaultMessage;
        status = defaultErrCode;
    }                    
    else {
        errorMsg = message;
    }
    return status;
}



int CBlastReportApp::PrintFormattedOutput(void)
{
    int retval = EXIT_CODE__FORMAT_SUCCESS;
    const CArgs& args = GetArgs();

    
    CNcbiOstream& out = args[kArgOutput].AsOutputFile();
    CFormattingArgs fmt_args;

    string alignSeqList = args[kArgAlignSeqList].HasValue() ? args[kArgAlignSeqList].AsString() : kEmptyStr;
    bool searchMetadata = args[kArgMetadata].HasValue() ? args[kArgMetadata].AsBoolean() : false;
    unsigned int queryIndex = args[kArgQueryIndex].HasValue() ? args[kArgQueryIndex].AsInteger() : 0;
    
    

    CRef<CBlastOptionsHandle> opts_handle = m_RmtBlast->GetSearchOptions();
    CBlastOptions& opts = opts_handle->SetOptions();
    fmt_args.ExtractAlgorithmOptions(args, opts);
    {{
        CDebugArgs debug_args;
        debug_args.ExtractAlgorithmOptions(args, opts);
        if (debug_args.ProduceDebugOutput()) {
            opts.DebugDumpText(NcbiCerr, "BLAST options", 1);
        }
    }}


    const EBlastProgramType p = opts.GetProgramType();
    
    CRef<CBlastQueryVector> queries = x_ExtractQueries(Blast_QueryIsProtein(p)?true:false);
    CRef<CScope> scope = queries->GetScope(0);
    _ASSERT(queries);

    CRef<CBlastDatabaseArgs> db_args(new CBlastDatabaseArgs());  // FIXME, what about rpsblast?
    int filtering_algorithm = -1;
    if (m_RmtBlast->IsDbSearch())
    {
        CRef<CBlast4_database> db = m_RmtBlast->GetDatabases();
        _ASSERT(db);
        _TRACE("Fetching results for " + Blast_ProgramNameFromType(p) + " on "
               + db->GetName());
        filtering_algorithm = m_RmtBlast->GetDbFilteringAlgorithmId();
        CRef<CSearchDatabase> search_db(new CSearchDatabase(db->GetName(), db->IsProtein()
                              ? CSearchDatabase::eBlastDbIsProtein
                              : CSearchDatabase::eBlastDbIsNucleotide));
        db_args->SetSearchDatabase(search_db);
    }
    
    CRef<CLocalDbAdapter> db_adapter;
    InitializeSubject(db_args, opts_handle, true, db_adapter, scope);


    CBlastFormat formatter(opts, *db_adapter,
                           fmt_args.GetFormattedOutputChoice(),
                           static_cast<bool>(args[kArgParseDeflines]),
                           out,
                           fmt_args.GetNumDescriptions(),
                           fmt_args.GetNumAlignments(),
                           *scope,
                           opts.GetMatrixName(),
                           fmt_args.ShowGis(),
                           fmt_args.DisplayHtmlOutput(),
                           opts.GetQueryGeneticCode(),
                           opts.GetDbGeneticCode(),
                           opts.GetSumStatisticsMode(),
                           false,
                           filtering_algorithm);
    
    formatter.SetLineLength(fmt_args.GetLineLength());
    formatter.SetAlignSeqList(alignSeqList);
        
    CRef<CSearchResultSet> results = m_RmtBlast->GetResultSet();
    

    try {
        if(queryIndex > results->GetNumQueries() - 1) {
            string msg;
            retval  = s_GetError("InvalidQueryIndex", "Invalid query index.", EXIT_CODE__QUERY_INDEX_INVALID, msg);                                                            
            NCBI_THROW(CInputException, eInvalidInput,msg);                                 
        }
            
        bool hasAlignments = (*results)[queryIndex].HasAlignments();
        //BlastFormatter_PreFetchSequenceData(*results, scope, fmt_args.GetFormattedOutputChoice());//*****Do we need to do this here???    
        CBlastFormat::DisplayOption displayOption;
        if(searchMetadata) {
            displayOption = CBlastFormat::eMetadata;   
        }
        else if(!alignSeqList.empty()){
            displayOption = CBlastFormat::eAlignments;    
        }
        else {
            displayOption = CBlastFormat::eDescriptions;
        }
        if(hasAlignments || displayOption == CBlastFormat::eMetadata) {
            formatter.PrintReport((*results)[queryIndex], displayOption);        
        }
        if(!hasAlignments) {
            retval = EXIT_CODE__NO_RESULTS_FOUND;
        }           
    }catch (const CException & e) {
       cerr << e.GetMsg() << endl;              
    }    
	
    return retval;
}



int CBlastReportApp::Run(void)
{
    int status = EXIT_CODE__FORMAT_SUCCESS;
    const CArgs& args = GetArgs();
    string msg;
    try {
        SetDiagPostLevel(eDiag_Warning);        
        if (args[kArgArchive].HasValue()) {    
            CNcbiIstream& istr = args[kArgArchive].AsInputFile();
            m_RmtBlast.Reset(new CRemoteBlast(istr)); 
            if (m_RmtBlast->LoadFromArchive()) {
                if(!m_RmtBlast->IsErrMsgArchive()) {
                    status = PrintFormattedOutput();                    
                    return status;
                }    
                else {
                    status = s_GetError("NetConError", "Network connection error", EXIT_CODE__NETWORK_CONNECTION_ERROR, msg);                    
                }
            }
        }    	    
    }        
    catch (const CSerialException&) {
        status = s_GetError("InvailInputFormat", "Invalid input format for BLAST Archive.", EXIT_CODE__INVALID_INPUT_FORMAT, msg);                            
    }        
    catch (const CException& e) {        
        if (e.GetErrCode() == CBlastException::eInvalidArgument) {
            status = s_GetError("ErrorBlastArchive", "Error processing BLAST Archive.", EXIT_CODE__BLAST_ARCHIVE_ERROR, msg);                                    
        }
        else {
            status = s_GetError("ErrorAccessingFile", e.GetMsg(), EXIT_CODE__CANNOT_ACCESS_FILE, msg,args[kArgArchive].AsString());            
        }                
    } 
    
    //cerr << "****retval:" << status << endl;         
    cerr << msg << endl;         
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastReportApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
