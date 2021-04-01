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
 * Authors:  Greg Boratyn
 *
 */

/** @file blastmapper_app.cpp
 * BLASTMAPPER command line application
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/magicblast.hpp>
#include <algo/blast/api/magicblast_options.hpp>
#include <algo/blast/api/blast_usage_report.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_asn1_input.hpp>
#include <algo/blast/blast_sra_input/blast_sra_input.hpp>
#include <algo/blast/blastinput/magicblast_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include "../blast/blast_app_util.hpp"

#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include "magicblast_util.hpp"
#include "magicblast_thread.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

static const int   kMajorVersion = 1;
static const int   kMinorVersion = 6;
static const int   kPatchVersion = 0;

class CMagicBlastVersion : public CVersionInfo
{
public:
    CMagicBlastVersion() : CVersionInfo(kMajorVersion,
                                        kMinorVersion,
                                        kPatchVersion)
    {}
};


class CMagicBlastApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CMagicBlastApp()
    {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CMagicBlastVersion());
        SetFullVersion(version);
        m_StopWatch.Start();
        if (m_UsageReport.IsEnabled()) {
            m_UsageReport.AddParam(CBlastUsageReport::eVersion,
                                   GetVersion().Print());
        }
    }

    ~CMagicBlastApp()
    {
        m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    void x_LogBlastSearchInfo(CBlastUsageReport & report,
                              const CBlastOptions& options,
                              CRef<CMapperFormattingArgs> fmt_args,
                              CRef<CLocalDbAdapter> db_adapter,
                              Int8 db_size,
                              Int8 num_db_sequences);

    /// This application's command line args
    CRef<CMagicBlastAppArgs> m_CmdLineArgs;
    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;
};

void CMagicBlastApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CMagicBlastAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}


static CShortReadFastaInputSource::EInputFormat
s_QueryOptsInFmtToFastaInFmt(CMapperQueryOptionsArgs::EInputFormat infmt)
{
    CShortReadFastaInputSource::EInputFormat retval;
    switch (infmt) {
    case CMapperQueryOptionsArgs::eFasta:
        retval = CShortReadFastaInputSource::eFasta;
        break;

    case CMapperQueryOptionsArgs::eFastc:
        retval = CShortReadFastaInputSource::eFastc;
        break;

    case CMapperQueryOptionsArgs::eFastq:
        retval = CShortReadFastaInputSource::eFastq;
        break;

    default:
        NCBI_THROW(CException, eInvalid, "Invalid input format, "
                   "should be Fasta, Fastc, or Fastq");
    };

    return retval;
}

// Create input source object for reading query sequences
static CBlastInputSourceOMF*
s_CreateInputSource(CRef<CMapperQueryOptionsArgs> query_opts,
                    CRef<CMagicBlastAppArgs> cmd_line_args)
{
    CBlastInputSourceOMF* retval;

    CMapperQueryOptionsArgs::EInputFormat infmt = query_opts->GetInputFormat();

    switch (infmt) {
    case CMapperQueryOptionsArgs::eFasta:
    case CMapperQueryOptionsArgs::eFastc:
    case CMapperQueryOptionsArgs::eFastq:

        if (query_opts->HasMateInputStream()) {
            retval = new CShortReadFastaInputSource(
                    cmd_line_args->GetInputStream(),
                    *query_opts->GetMateInputStream(),
                    s_QueryOptsInFmtToFastaInFmt(query_opts->GetInputFormat()));
        }
        else {
            retval = new CShortReadFastaInputSource(
                    cmd_line_args->GetInputStream(),
                    s_QueryOptsInFmtToFastaInFmt(query_opts->GetInputFormat()),
                    query_opts->IsPaired());
        }
        break;

    case CMapperQueryOptionsArgs::eASN1text:
    case CMapperQueryOptionsArgs::eASN1bin:

        if (query_opts->HasMateInputStream()) {
            retval = new CASN1InputSourceOMF(
                                    cmd_line_args->GetInputStream(),
                                    *query_opts->GetMateInputStream(),
                                    infmt == CMapperQueryOptionsArgs::eASN1bin);
        }
        else {
            retval = new CASN1InputSourceOMF(
                                    cmd_line_args->GetInputStream(),
                                    infmt == CMapperQueryOptionsArgs::eASN1bin,
                                    query_opts->IsPaired());
        }
        break;

    case CMapperQueryOptionsArgs::eSra:
        try {
            retval = new CSraInputSource(query_opts->GetSraAccessions(),
                                         query_opts->IsPaired(),
                                         query_opts->IsSraCacheEnabled());
        } catch (CSraException& e) {
            const string& str = query_opts->GetSraAccessions().front();
            if (e.GetErrCode() == CSraException::eNotFoundDb) {
                CNcbiStrstream ss;
                ss << "The provided SRA accession '" << str
                     << "' does not exist" << ends;
                NCBI_THROW(CInputException, eEmptyUserInput, ss.str());
            }
            throw;
        } catch (...) {
            throw;
        }
        break;


    default:
        NCBI_THROW(CException, eInvalid, "Unrecognized input format");
    };

    return retval;
}


static void
s_InitializeSubject(CRef<blast::CBlastDatabaseArgs> db_args,
                    CRef<blast::CBlastOptionsHandle> opts_hndl,
                    CRef<blast::CLocalDbAdapter>& db_adapter,
                    Uint8& subject_length,
                    int& num_sequences)
{
    db_adapter.Reset();

    _ASSERT(db_args.NotEmpty());
    CRef<CSearchDatabase> search_db = db_args->GetSearchDatabase();
    CRef<CScope> scope;

    // Initialize the scope...
    if (scope.Empty()) {
        scope.Reset(new CScope(*CObjectManager::GetInstance()));
    }
    _ASSERT(scope.NotEmpty());

    // ... and then the subjects
    CRef<IQueryFactory> subjects;
    if ( (subjects = db_args->GetSubjects(scope)) ) {
        _ASSERT(search_db.Empty());
        db_adapter.Reset(new CLocalDbAdapter(subjects, opts_hndl, true));

        BlastSeqSrc* seq_src = db_adapter->MakeSeqSrc();
        _ASSERT(seq_src);
        subject_length = BlastSeqSrcGetTotLen(seq_src);
        num_sequences = BlastSeqSrcGetNumSeqs(seq_src);
    } else {
        _ASSERT(search_db.NotEmpty());
        subject_length = search_db->GetSeqDb()->GetTotalLength();
        num_sequences = search_db->GetSeqDb()->GetNumSeqs();
        db_adapter.Reset(new CLocalDbAdapter(*search_db));
    }
}


static bool
s_IsIStreamEmpty(CNcbiIstream & in)
{
	char c;
	CNcbiStreampos orig_p = in.tellg();
	// Piped input
	if(orig_p < 0)
		return false;

	IOS_BASE::iostate orig_state = in.rdstate();
	IOS_BASE::fmtflags orig_flags = in.setf(ios::skipws);

	if(! (in >> c))
		return true;

	in.seekg(orig_p);
	in.flags(orig_flags);
	in.clear();
	in.setstate(orig_state);

	return false;
}


static string
s_GetCmdlineArgs(const CNcbiArguments & a)
{
	string cmd = kEmptyStr;
	for(unsigned int i=0; i < a.Size(); i++) {
		cmd += a[i] + " ";
	}
	return cmd;
}


int CMagicBlastApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;
    const int kSamLargeNumSubjects = INT4_MAX;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);
        SetDiagPostPrefix("magicblast");

        // Prefer accessions to gis when generating reports
        CNcbiEnvironment env;
        env.Set("SEQ_ID_PREFER_ACCESSION_OVER_GI", "1");

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
        CRef<CBlastOptionsHandle> opts_hndl;
        opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));
        const CBlastOptions& opt = opts_hndl->GetOptions();

        /*** Initialize the database/subject ***/
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());

        CRef<CLocalDbAdapter> db_adapter;
        Uint8 db_size;
        int num_db_sequences;
        s_InitializeSubject(db_args, opts_hndl, db_adapter, db_size,
                            num_db_sequences);
        _ASSERT(db_adapter);


        /*** Get the query sequence(s) ***/
        CRef<CMapperQueryOptionsArgs> query_opts(
            dynamic_cast<CMapperQueryOptionsArgs*>(
                   m_CmdLineArgs->GetQueryOptionsArgs().GetNonNullPointer()));

        m_UsageReport.AddParam(CBlastUsageReport::eSRA,
                               query_opts->GetInputFormat() !=
                                                CMapperQueryOptionsArgs::eSra);

        if(query_opts->GetInputFormat() != CMapperQueryOptionsArgs::eSra &&
           s_IsIStreamEmpty(m_CmdLineArgs->GetInputStream())) {

           	NCBI_THROW(CArgException, eNoValue, "Query is Empty!");
        }

        /*** Get the formatting options ***/
        CRef<CMapperFormattingArgs> fmt_args(
             dynamic_cast<CMapperFormattingArgs*>(
                   m_CmdLineArgs->GetFormattingArgs().GetNonNullPointer()));

        // FIXME: only_specific, fr, and rf variables are set and checked
        // here and in threads to avoid multiple confusing error messages.
        // Proper error reporting for problems in threads should be implemented.

        // Is either strand-specificity flag set? (mutually exclusive)
        const bool only_specific = fmt_args->SelectOnlyStrandSpecific();
        const bool fr = fmt_args->SelectFwdRev();
        const bool rf = fmt_args->SelectRevFwd();
        // One or both MUST be false. (enforced by command-line processing)
        _ASSERT(fr == false  ||  rf == false);
        // "-fr" and "-rf" flags can only be used without
        // "-only_strand_specific" for SAM output.  Return an error if this
        // condition is not met.
        {
            if (fmt_args->GetFormattedOutputChoice() != CFormattingArgs::eSAM) {
                if (!only_specific  &&  (fr || rf)) {
                    NCBI_THROW(CArgException, eNoValue,
                               "-fr or -rf can only be used with SAM format."
                               " Use -oufmt sam option.");
                }
            }
        }
        // "-only_strand_specific" without "-fr" or "-rf" (or in the future,
        // "-f" or "-r") is not meaningful.
        // FIXME: should this be a warning?
        {
            if (only_specific  &&  !(fr || rf)) {
                NCBI_THROW(CArgException, eNoValue,
                        "-only_strand_specific without either -fr or -rf "
                        "is not valid.");
            }
        }

        if (fmt_args->GetFormattedOutputChoice() == CFormattingArgs::eSAM) {
            if (num_db_sequences < kSamLargeNumSubjects) {
                PrintSAMHeader(m_CmdLineArgs->GetOutputStream(), db_adapter,
                               s_GetCmdlineArgs(GetArguments()));
            }
        }
        else if (fmt_args->GetFormattedOutputChoice() ==
                 CFormattingArgs::eTabular) {

            PrintTabularHeader(m_CmdLineArgs->GetOutputStream(),
                               GetVersion().Print(),
                               s_GetCmdlineArgs(GetArguments()));
        }

        // print another SAM or tabular header if reporting unaligned reads
        // in another file
        if (m_CmdLineArgs->HasUnalignedOutputStream()) {
            if (fmt_args->GetUnalignedOutputFormat() == CFormattingArgs::eSAM) {
                PrintSAMHeader(*m_CmdLineArgs->GetUnalignedOutputStream(),
                               db_adapter, s_GetCmdlineArgs(GetArguments()));
            }
            else if (fmt_args->GetUnalignedOutputFormat() == CFormattingArgs::eTabular) {
                PrintTabularHeader(*m_CmdLineArgs->GetUnalignedOutputStream(),
                                   GetVersion().Print(),
                                   s_GetCmdlineArgs(GetArguments()));
            }
        }

        int batch_size = m_CmdLineArgs->GetQueryBatchSize();
        int batch_num = 500000;
        int num_threads = m_CmdLineArgs->GetNumThreads();

        string num_seqs_str = env.Get("BATCH_NUM_SEQS");
        if (!num_seqs_str.empty()) {
            batch_num = NStr::StringToInt(num_seqs_str);
        }

        // when not counting words in the database, processing smaller
        // batches is faster
        if (!opt.GetLookupDbFilter()) {
            batch_size = MAX(batch_size / num_threads, 5000000);
        }

        unique_ptr<CBlastInputSourceOMF> fasta(s_CreateInputSource(query_opts,
                                                               m_CmdLineArgs));
        CBlastInputOMF input(fasta.get(), batch_size);
        input.SetMaxBatchNumSeqs(batch_num);

        CMagicBlastThread** threads = new CMagicBlastThread*[num_threads];
        for (int i=0;i < num_threads;i++) {
            CRef<CBlastOptions> options = opt.Clone();
            CRef<CMagicBlastOptionsHandle> magic_opts(
                                   new CMagicBlastOptionsHandle(options));

            threads[i] = new CMagicBlastThread(input, magic_opts,
                                               query_opts, db_args,
                                               fmt_args,
                                               m_CmdLineArgs->GetOutputStream(),
                                               m_CmdLineArgs->GetUnalignedOutputStream());
            threads[i]->Run();
        }

        for (int i=0;i < num_threads;i++) {
            threads[i]->Join();
        }
        
        delete [] threads;


        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

        m_UsageReport.AddParam(CBlastUsageReport::eTotalQueryLength, input.GetTotalLengthProcessed());
        m_UsageReport.AddParam(CBlastUsageReport::eNumQueries, input.GetNumSeqsProcessed());
        x_LogBlastSearchInfo(m_UsageReport, opt, fmt_args, db_adapter,
                             (Int8)db_size, (Int8)num_db_sequences);
    } CATCH_ALL(status)

    m_UsageReport.AddParam(CBlastUsageReport::eNumThreads, (int) m_CmdLineArgs->GetNumThreads());
    m_UsageReport.AddParam(CBlastUsageReport::eExitStatus, status);
    return status;
}


void CMagicBlastApp::x_LogBlastSearchInfo(CBlastUsageReport & report,
                                          const CBlastOptions& options,
                                          CRef<CMapperFormattingArgs> fmt_args,
                                          CRef<CLocalDbAdapter> db_adapter,
                                          Int8 db_size,
                                          Int8 num_db_sequences)
{
	if (report.IsEnabled()) {
		report.AddParam(CBlastUsageReport::eProgram, (string)"magicblast");
		EProgram task = options.GetProgram();
		string task_str =  EProgramToTaskName(task);
		report.AddParam(CBlastUsageReport::eTask, task_str);
		report.AddParam(CBlastUsageReport::eOutputFmt,
                        fmt_args->GetFormattedOutputChoice());

        report.AddParam(CBlastUsageReport::eNumSubjects, num_db_sequences);
        report.AddParam(CBlastUsageReport::eSubjectsLength, db_size);

        if (db_adapter->IsBlastDb()) {
            CRef<CSearchDatabase> db = db_adapter->GetSearchDatabase();
			if(db.NotEmpty()){
                string dir = kEmptyStr;
                string db_name = db->GetDatabaseName();
                CFile::SplitPath(db->GetDatabaseName(), &dir);
                if (dir != kEmptyStr) {
                    db_name = db->GetDatabaseName().substr(dir.length());
                }
                report.AddParam(CBlastUsageReport::eDBName, db_name);
				if(db->GetGiList().NotEmpty()) {
                    CRef<CSeqDBGiList>  l = db->GetGiList();
                    if (l->GetNumGis()) {
                        report.AddParam(CBlastUsageReport::eGIList, true);
                    }
                    if (l->GetNumSis()){
                        report.AddParam(CBlastUsageReport::eSeqIdList, true);
                    }
                    if (l->GetNumTaxIds()){
                        report.AddParam(CBlastUsageReport::eTaxIdList, true);
                    }
                    if (l->GetNumPigs()) {
                        report.AddParam(CBlastUsageReport::eIPGList, true);
                    }
				}
				if(db->GetNegativeGiList().NotEmpty()) {
                    CRef<CSeqDBGiList>  l = db->GetNegativeGiList();
                    if (l->GetNumGis()) {
                        report.AddParam(CBlastUsageReport::eNegGIList, true);
                    }
                    if (l->GetNumSis()){
                        report.AddParam(CBlastUsageReport::eNegSeqIdList, true);
                    }
                    if (l->GetNumTaxIds()){
                        report.AddParam(CBlastUsageReport::eNegTaxIdList, true);
                    }
                    if (l->GetNumPigs()) {
                        report.AddParam(CBlastUsageReport::eNegIPGList, true);
                    }
				}
			}
            
        }
    }
}


#ifndef SKIP_DOXYGEN_PROCESSING
int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CMagicBlastApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
