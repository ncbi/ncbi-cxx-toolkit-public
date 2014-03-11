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
 * Author: Amelia Fong
 *
 */

/** @file makeprofiledb.cpp
 * Command line tool to create RPS,COBALT & DELTA BLAST databases.
 * This is the successor to formatrpsdb from the C toolkit
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbitime.hpp>
#include <util/math/matrix.hpp>
#include <serial/objistrasn.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include <algo/blast/api/psi_pssm_input.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/scoremat/PssmParameters.hpp>
#include <objects/scoremat/FormatRpsDbParameters.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/CoreBlock.hpp>
#include <objects/scoremat/CoreDef.hpp>
#include <objects/scoremat/LoopConstraint.hpp>
#include <algo/blast/core/blast_aalookup.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/ncbi_math.h>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objtools/blast/seqdb_writer/taxid_set.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif /* SKIP_DOXYGEN_PROCESSING */


//Input args specify to makeprofiledb
static const string kInPssmList("in");
static const string kOutDbName("out");
static const string kMaxFileSize("max_file_sz");
static const string kOutDbType("dbtype");
static const string kPssmScaleFactor("scale");
static const string kOutIndexFile("index");
static const string kObsrThreshold("obsr_threshold");
static const string kExcludeInvalid("exclude_invalid");
static const string kBinaryScoremat("binary");

static const string kLogFile("logfile");

//Supported Output Database Types
static const string kOutDbRps = "rps";
static const string kOutDbCobalt = "cobalt";
static const string kOutDbDelta = "delta";

//Supported Matrices
static const string kMatrixBLOSUM62 = "BLOSUM62";
static const string kMatrixBLOSUM80 = "BLOSUM80";
static const string kMatrixBLOSUM50 = "BLOSUM50";
static const string kMatrixBLOSUM45 = "BLOSUM45";
static const string kMatrixBLOSUM90 = "BLOSUM90";
static const string kMatrixPAM250 = "PAM250";
static const string kMatrixPAM30 = "PAM30";
static const string kMatrixPAM70 = "PAM70";

//Default Input Values
static const string kDefaultMatrix(kMatrixBLOSUM62);
static const string kDefaultMaxFileSize("1GB");
static const string kDefaultOutDbType(kOutDbRps);
static const string kDefaultOutIndexFile("true");
static const string kDefaultExcludeInvalid("true");
#define kDefaultWordScoreThreshold (9.82)
#define kDefaultPssmScaleFactor (100.00)
#define kDefaultObsrThreshold (6.0)

//Fix point scale factor for delta blast
static const Uint4 kFixedPointScaleFactor = 1000;
#define kEpsylon (0.0001)

#define DEFAULT_POS_MATRIX_SIZE	2000
#define RPS_NUM_LOOKUP_CELLS 32768
#if BLASTAA_SIZE == 28
#define RPS_DATABASE_VERSION RPS_MAGIC_NUM_28
#else
#define RPS_DATABASE_VERSION RPS_MAGIC_NUM
#endif

class CMakeDbPosMatrix
{
public:
	CMakeDbPosMatrix():  m_posMatrix(NULL), m_size(0) { };
	~CMakeDbPosMatrix(){Delete();};

	void Create(int seq_size);
	void Delete(void);

	Int4 ** Get(void) { return m_posMatrix;};
	unsigned int GetSize(void){return m_size;};

private:

	Int4 ** m_posMatrix;
	int m_size;
};

void CMakeDbPosMatrix::Create(int size)
{
	Delete();

	m_posMatrix = new Int4* [size];

	for(int i = 0; i < size; ++ i)
	{
		m_posMatrix[i] = new Int4[BLASTAA_SIZE];
	}
	m_size = size;

	return;
}

void CMakeDbPosMatrix::Delete(void)
{
	if( NULL == m_posMatrix)
		return;

	for(int i = 0; i < m_size; ++ i)
	{
		if (m_posMatrix[i] != NULL)
			delete [] m_posMatrix[i];
	}

	delete [] m_posMatrix;
	m_posMatrix = NULL;
	return;
}

class CMakeProfileDBApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CMakeProfileDBApp(void);
    ~CMakeProfileDBApp();
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    enum op_mode
    {
    	op_rps ,
    	op_cobalt,
    	op_delta ,
    	op_invalid
    };

    struct RPS_DbInfo
    {
        Int4 num_seqs;
        CNcbiOfstream lookup_file;
        CNcbiOfstream pssm_file;
        CNcbiOfstream aux_file;
        CNcbiOfstream blocks_file;
        CNcbiOfstream freq_file;

        CMakeDbPosMatrix pos_matrix;
        Int4 gap_open;
        Int4 gap_extend;
        Int4 scale_factor;
        Int4 curr_seq_offset;
        QuerySetUpOptions *query_options;
        LookupTableOptions *lookup_options;
        BlastAaLookupTable *lookup;
        string matrix;

        RPS_DbInfo():
        		gap_open(0), gap_extend(0),scale_factor(0), curr_seq_offset(0),
        		query_options(NULL), lookup_options(NULL), lookup(NULL)
        { };
    }s;

    enum CheckInputScoremat_RV
    {
    	sm_valid_has_pssm,
    	sm_valid_freq_only,
     	sm_invalid
    };

    CheckInputScoremat_RV x_CheckInputScoremat(const CPssmWithParameters & pssm_w_parameters,
    										   const string & filename);
    void x_SetupArgDescriptions(void);
    void x_InitProgramParameters(void);
    vector<string> x_GetSMPFilenames(void);
    void x_InitOutputDb(void);
    void x_InitRPSDbInfo(Int4 num_files);
    void x_UpdateRPSDbInfo(const CPssmWithParameters & pssm_p);
    void x_RPSAddFirstSequence(CPssmWithParameters  & pssm_w_parameters, bool freq_only);
    void x_RPSUpdateLookup(Int4 seq_size);
    void x_RPSUpdateStatistics(CPssmWithParameters & seq, Int4 seq_size);
    void x_FillInRPSDbParameters(CPssmWithParameters & pssm_p);
    void x_RPSUpdatePSSM(const CPssm & pssm, Int4 seq_index, Int4 seq_size);
    void x_RPS_DbClose(void);
    void x_UpdateCobalt( const CPssmWithParameters  & pssm_p, Int4 seq_size);
    bool x_UpdateDelta( const CPssm  & pssm, Int4 seq_size, const string & filename,
    					CNcbiOfstream & obsr_buff_file, CNcbiOfstream & freq_buff_file);
    bool x_ValidateCd(const list<double>& freqs, const list<double>& observ, unsigned int alphabet_size);
    void x_WrapUpDelta(CTmpFile & tmp_obsr_file, CTmpFile & tmp_freq_file);
    vector<string> x_CreateDeltaList(void);
    void x_UpdateFreqRatios(const CPssmWithParameters & pssm_p, Int4 seq_index, Int4 seq_size);
    bool x_IsUpdateFreqRatios(const CPssm & p);

    // Data
    CNcbiOstream * m_LogFile;
    CNcbiIstream * m_InPssmList;
    string m_Title;
    double m_WordScoreThreshold;
    string m_OutDbName;
    Uint8	m_MaxFileSize;
    string m_OutDbType;
    bool m_CreateIndexFile;
    int m_GapOpenPenalty;
    int m_GapExtPenalty;
    double m_PssmScaleFactor;
    string m_Matrix;
    op_mode m_op_mode;
    bool m_binary_scoremat;

    struct RPS_DbInfo	m_RpsDbInfo;
    CRef<CWriteDB>	m_OutputDb;
    CRef<CTaxIdSet> m_Taxids;
    bool m_Done;

    //For Delta Blast
	double m_ObsrvThreshold;
	bool m_ExcludeInvalid;
	list<Int4> m_FreqOffsets;
	list<Int4> m_ObsrOffsets;
	Int4 m_CurrFreqOffset;
	Int4 m_CurrObsrOffset;

	bool m_UpdateFreqRatios;
};

CMakeProfileDBApp::CMakeProfileDBApp(void)
                : m_LogFile(NULL), m_InPssmList(NULL), m_Title(kEmptyStr),
                  m_WordScoreThreshold(0), m_OutDbName(kEmptyStr), m_MaxFileSize(0),
                  m_OutDbType(kEmptyStr), m_CreateIndexFile(false),m_GapOpenPenalty(0),
                  m_GapExtPenalty(0), m_PssmScaleFactor(0),m_Matrix(kEmptyStr),  m_op_mode(op_invalid),
                  m_binary_scoremat(false), m_Taxids(new CTaxIdSet()), m_Done(false),
                  m_ObsrvThreshold(0), m_ExcludeInvalid(false), m_CurrFreqOffset(0), m_CurrObsrOffset(0),
                  m_UpdateFreqRatios(true)
{
	CRef<CVersion> version(new CVersion());
	version->SetVersionInfo(new CBlastVersion());
	SetFullVersion(version);
}

CMakeProfileDBApp::~CMakeProfileDBApp()
{

	 if( NULL != m_RpsDbInfo.query_options)
		 BlastQuerySetUpOptionsFree(m_RpsDbInfo.query_options);

	 if(NULL != m_RpsDbInfo.lookup)
		 BlastAaLookupTableDestruct(m_RpsDbInfo.lookup);

	 if(NULL != m_RpsDbInfo.lookup_options)
		 LookupTableOptionsFree(m_RpsDbInfo.lookup_options);

	 if(m_Done == false)
	 {
		 if(kEmptyStr != m_OutDbName)
	 	 {
			string rps_str = m_OutDbName + ".rps";
		 	string lookup_str = m_OutDbName + ".loo";
		 	string aux_str = m_OutDbName + ".aux";
		 	string freq_str = m_OutDbName + ".freq";
		 	CFile(rps_str).Remove();
		 	CFile(lookup_str).Remove();
		 	CFile(aux_str).Remove();
		 	CFile(freq_str).Remove();

		 	if(op_cobalt == m_op_mode)
		 	{
				string blocks_str = m_OutDbName + ".blocks";
		 		CFile(blocks_str).Remove();
		 	}

		 	if(op_delta == m_op_mode)
		 	{
		 		string wcounts_str = m_OutDbName + ".wcounts";
		 		string obsr_str = m_OutDbName + ".obsr";
		 		CFile(wcounts_str).Remove();
		 		CFile(obsr_str).Remove();
		 	}

		 	if(m_OutputDb.NotEmpty())
		 	{
		 		m_OutputDb->Close();
		 		vector<string> tmp_files;
		 		m_OutputDb->ListFiles(tmp_files);
		 		for(vector<string>::iterator itr = tmp_files.begin();
		 			itr != tmp_files.end(); ++itr)
		 			CFile(*itr).Remove();
		 	}
	 	 }
	 }
	 else
	 {
		 string pog_str = m_OutDbName + ".pog";
		 CFile(pog_str).Remove();
	 }
}

void CMakeProfileDBApp::x_SetupArgDescriptions(void)
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                  "Application to create databases for rpsblast, cobalt and deltablast, version "
                  + CBlastVersion().Print());

    string dflt("Default = input file name provided to -");
    dflt += kInPssmList + " argument";

    arg_desc->SetCurrentGroup("Input options");
    arg_desc->AddKey(kInPssmList, "in_pssm_list",
                     "Input file that contains a list of smp files (delimited by space, tab or newline)",
                     CArgDescriptions::eInputFile);

    arg_desc->AddFlag(kBinaryScoremat,
       				  "Scoremats are in binary format",
       				  true);

    arg_desc->SetCurrentGroup("Configuration options");
    arg_desc->AddOptionalKey(kArgDbTitle, "database_title",
                             "Title for database\n" + dflt,
                             CArgDescriptions::eString);

    arg_desc->AddDefaultKey(kArgWordScoreThreshold, "word_score_threshold",
    						"Minimum word score to add a word to the lookup table",
    						CArgDescriptions::eDouble,
    						NStr::DoubleToString(kDefaultWordScoreThreshold));

    arg_desc->SetCurrentGroup("Output options");
    arg_desc->AddOptionalKey(kOutDbName, "database_name",
                             "Name of database to be created\n" +
                              dflt , CArgDescriptions::eString);

    arg_desc->AddDefaultKey(kMaxFileSize, "max_file_size_in_bytes",
                            "Maximum file size for database files",
                            CArgDescriptions::eString, kDefaultMaxFileSize);

    arg_desc->AddDefaultKey(kOutDbType, "output_db_type",
                            "Output database type: cobalt, delta, rps",
                            CArgDescriptions::eString, kDefaultOutDbType);
    arg_desc->SetConstraint(kOutDbType, &(*new CArgAllow_Strings, kOutDbRps, kOutDbCobalt , kOutDbDelta ));

    arg_desc->AddDefaultKey(kOutIndexFile, "create_index_files",
                            "Create Index Files",
                            CArgDescriptions::eBoolean, kDefaultOutIndexFile);

    arg_desc->SetCurrentGroup("Used only if scoremat files do not contain PSSM scores, ignored otherwise.");
    arg_desc->AddOptionalKey(kArgGapOpen, "gap_open_penalty",
                            "Cost to open a gap",
                            CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey(kArgGapExtend, "gap_extend_penalty",
                            "Cost to extend a gap, ",
                            CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey(kPssmScaleFactor, "pssm_scale_factor",
                            "Pssm Scale factor ",
                            CArgDescriptions::eDouble,
                            NStr::DoubleToString(kDefaultPssmScaleFactor));

    arg_desc->AddDefaultKey(kArgMatrixName, "matrix_name",
                            "Scoring matrix name",
                            CArgDescriptions::eString,
                            kDefaultMatrix);
    arg_desc->SetConstraint(kArgMatrixName, &(*new CArgAllow_Strings,kMatrixBLOSUM62, kMatrixBLOSUM80,
    						kMatrixBLOSUM50, kMatrixBLOSUM45, kMatrixBLOSUM90, kMatrixPAM250, kMatrixPAM30, kMatrixPAM70));

    //Delta Blast Options
    arg_desc->SetCurrentGroup("Delta Blast Options");
    arg_desc->AddDefaultKey(kObsrThreshold, "observations_threshold", "Exclude domains with "
                            "with maximum number of independent observations "
                            "below this threshold", CArgDescriptions::eDouble,
                            NStr::DoubleToString(kDefaultObsrThreshold));

    arg_desc->AddDefaultKey(kExcludeInvalid, "exclude_invalid", "Exclude domains that do "
                            "not pass validation test",
                            CArgDescriptions::eBoolean, kDefaultExcludeInvalid);

    SetupArgDescriptions(arg_desc.release());
}

void CMakeProfileDBApp::x_InitProgramParameters(void)
{
	const CArgs& args = GetArgs();

	//log_file
	if (args[kLogFile].HasValue())
		m_LogFile = &args[kLogFile].AsOutputFile();
	else
		m_LogFile = &cout;


	//in_list
	if (args[kInPssmList].HasValue())
		m_InPssmList = &args[kInPssmList].AsInputFile();
	else
		NCBI_THROW(CInputException, eInvalidInput,  "Please provide an input file with list of smp files");

	// Binary Scoremat
	m_binary_scoremat = args[kBinaryScoremat];

	//title
	if (args[kArgDbTitle].HasValue())
		m_Title = args[kArgDbTitle].AsString();
	else
		m_Title = args[kInPssmList].AsString();

	//threshold
	m_WordScoreThreshold = args[kArgWordScoreThreshold].AsDouble();

	//Out
	if(args[kOutDbName].HasValue())
		m_OutDbName = args[kOutDbName].AsString();
	else
		m_OutDbName = args[kInPssmList].AsString();

	//max_file_sz
	m_MaxFileSize = NStr::StringToUInt8_DataSize(args[kMaxFileSize].AsString());

	//out_db_type
	m_OutDbType = args[kOutDbType].AsString();
	if(kOutDbRps == m_OutDbType)
		m_op_mode = op_rps;
	else if (kOutDbCobalt == m_OutDbType)
		m_op_mode = op_cobalt;
	else if(kOutDbDelta == m_OutDbType)
		m_op_mode = op_delta;
	else
		NCBI_THROW(CInputException, eInvalidInput,  "Invalid Output database type");

	m_CreateIndexFile = args[kOutIndexFile].AsBoolean();

	int default_gap_open = 0;
	int default_gap_extend = 0;
	//matrix
	m_Matrix = args[kArgMatrixName].AsString();
	 BLAST_GetProteinGapExistenceExtendParams(m_Matrix.c_str(), &default_gap_open, &default_gap_extend);

	//gapopen
	if(args[kArgGapOpen].HasValue())
		m_GapOpenPenalty = args[kArgGapOpen].AsInteger();
	else
		m_GapOpenPenalty = default_gap_open;

	//gapextend
	if(args[kArgGapExtend].HasValue())
		m_GapExtPenalty = args[kArgGapExtend].AsInteger();
	else
		m_GapExtPenalty = default_gap_extend;

	//pssm scale factor
	m_PssmScaleFactor = args[kPssmScaleFactor].AsDouble();

	//matrix
	m_Matrix = args[kArgMatrixName].AsString();

	//Delta Blast Parameters
	m_ObsrvThreshold = args[kObsrThreshold].AsDouble();
	m_ExcludeInvalid = args[kExcludeInvalid].AsBoolean();

}

vector<string> CMakeProfileDBApp::x_GetSMPFilenames(void)
{
	vector<string> filenames;

	while(!m_InPssmList->eof())
	{
		string line;
		vector<string> tmp;
		NcbiGetlineEOL(*m_InPssmList, line);
		NStr::Tokenize(line, " \t\r", tmp, NStr::eMergeDelims);

		if(tmp.size()  > 0)
			filenames.insert(filenames.end(), tmp.begin(), tmp.end() );
	}

	if( 0 == filenames.size())
		NCBI_THROW(CInputException, eInvalidInput,  "Input file contains no smp filnames");

	return filenames;
}

CMakeProfileDBApp::CheckInputScoremat_RV
CMakeProfileDBApp::x_CheckInputScoremat(const CPssmWithParameters & pssm_w_parameters,
										const string & filename)
{
	CheckInputScoremat_RV sm = sm_invalid;

	if(pssm_w_parameters.IsSetPssm())
	{
		const CPssm & pssm = pssm_w_parameters.GetPssm();

		if(!pssm.IsSetQuery() || (0 == pssm.GetQueryLength()))
		{
			string err = filename + " contians no bioseq data";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		if(!pssm.IsSetNumRows() || !pssm.IsSetNumColumns())
		{
			string err = filename + " contians no info on num of columns or num of rows";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		if((int) (pssm.GetQueryLength()) != pssm.GetNumColumns())
		{
			string err = filename + " 's num of columns does not match size of sequence";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		int num_rows = pssm.GetNumRows();
		if( num_rows <= 0 || num_rows > BLASTAA_SIZE )
		{
			string err = filename + " has invalid alphabet size";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		// First time around
		if(NULL == m_RpsDbInfo.lookup)
		{
			m_UpdateFreqRatios = x_IsUpdateFreqRatios(pssm);
		}

		if(m_UpdateFreqRatios && (!pssm.IsSetIntermediateData()|| !pssm.GetIntermediateData().IsSetFreqRatios()))
		{
			string err = filename + " contains no frequence ratios for building database";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		if(op_cobalt == m_op_mode)
		{
			if(!pssm_w_parameters.IsSetParams() || !pssm_w_parameters.GetParams().IsSetConstraints() ||
			! pssm_w_parameters.GetParams().GetConstraints().IsSetBlocks())
			{
				string err = filename + " contains no core block to build cobalt database";
				NCBI_THROW(CInputException, eInvalidInput,  err);
			}
		}

		if(pssm.IsSetFinalData())
		{
			sm = sm_valid_has_pssm;
		}
		else if(pssm.IsSetIntermediateData())
		{
			if(pssm.GetIntermediateData().IsSetFreqRatios())
			{
				sm = sm_valid_freq_only;
			}
		}

		if(sm_invalid == sm)
		{
			string err = filename + " contians no pssm or residue frequencies";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}
	}
	else
	{
		string err = filename + " contians no scoremat";
		NCBI_THROW(CInputException, eInvalidInput,  err);
	}

	return sm;
}

bool CMakeProfileDBApp::x_IsUpdateFreqRatios(const CPssm & p)
{
	if(op_cobalt == m_op_mode)
		return true;

	if(!p.IsSetIntermediateData()|| !p.GetIntermediateData().IsSetFreqRatios())
		return false;

	return true;
}

void CMakeProfileDBApp::x_InitOutputDb(void)
{
	CWriteDB::EIndexType index_type = (m_CreateIndexFile == true ? CWriteDB::eDefault : CWriteDB::eNoIndex);
	m_OutputDb.Reset(new CWriteDB(m_OutDbName, CWriteDB::eProtein, m_Title, index_type, m_CreateIndexFile));
	m_OutputDb->SetMaxFileSize(m_MaxFileSize);
	return;
}


static void s_CreateDirectories(const string& dbname)
{
    CDirEntry dir_entry(dbname);


    string dir_name = dir_entry.GetDir(CDirEntry::eIfEmptyPath_Current);
    CDir tmp(dir_name);
    if (!tmp.CheckAccess(CDirEntry::fWrite)) {
        string msg("You do not have write permissions in the destination directory");
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }

    dir_name = dir_entry.GetDir(CDirEntry::eIfEmptyPath_Empty);
    if(dir_name.empty()) {
    	return;
    }

    CDir d(dir_name);
    if ( !d.Exists() ) {
        if ( !d.CreatePath() ) {
            string msg("Failed to create directory '" + d.GetName() + "'");
            NCBI_THROW(CSeqDBException, eFileErr, msg);
        }
    }

}

static bool s_DeleteMakeprofileDb(const string & name )
{
	bool isRemoved = false;
	static const char * ext[]={".rps", ".loo", ".aux", ".freq", ".blocks", ".wcounts", ".obsr", NULL};
	for(const char ** p=ext; *p != NULL; p++) {
		CFile f(name + (*p));
		if(f.Remove())
			isRemoved = true;
	}
	if(DeleteBlastDb(name, CSeqDB::eProtein))
		isRemoved = true;

	return isRemoved;
}


void CMakeProfileDBApp::x_InitRPSDbInfo(Int4 num_files)
{

	 s_CreateDirectories(m_OutDbName);
	 if ( s_DeleteMakeprofileDb(m_OutDbName)) {
		 *m_LogFile << "Deleted existing BLAST database with identical name." << endl;
	 }
     m_RpsDbInfo.num_seqs = num_files;

     string rps_str = m_OutDbName + ".rps";
     m_RpsDbInfo.pssm_file.open(rps_str.c_str(), IOS_BASE::out|IOS_BASE::binary);
     if (!m_RpsDbInfo.pssm_file.is_open())
    	 NCBI_THROW(CSeqDBException, eFileErr,"Failed to open output .rps file ");

     string lookup_str = m_OutDbName + ".loo";
     m_RpsDbInfo.lookup_file.open(lookup_str.c_str(), IOS_BASE::out|IOS_BASE::binary);
     if (!m_RpsDbInfo.lookup_file.is_open())
    	 NCBI_THROW(CSeqDBException, eFileErr,"Failed to open output .loo file");

     string aux_str = m_OutDbName + ".aux";
     m_RpsDbInfo.aux_file.open(aux_str.c_str());
     if (!m_RpsDbInfo.aux_file.is_open())
    	 NCBI_THROW(CSeqDBException, eFileErr,"Failed to open output .aux file");

	 string freq_str = m_OutDbName + ".freq";
	 m_RpsDbInfo.freq_file.open(freq_str.c_str(), IOS_BASE::out|IOS_BASE::binary);
	 if (!m_RpsDbInfo.freq_file.is_open())
		 NCBI_THROW(CSeqDBException, eFileErr,"Failed to open output .freq file");

     /* Write the magic numbers to the PSSM file */

     Int4 version = RPS_DATABASE_VERSION;
      m_RpsDbInfo.pssm_file.write ((char *)&version , sizeof(Int4));
      m_RpsDbInfo.freq_file.write ((char *)&version , sizeof(Int4));

     /* Fill in space for the sequence offsets. The PSSM
        data gets written after this list of integers. Also
        write the number of sequences to the PSSM file */

      m_RpsDbInfo.pssm_file.write((char *) &num_files, sizeof(Int4));
      m_RpsDbInfo.freq_file.write((char *) &num_files, sizeof(Int4));
     for (Int4 i = 0; i <= num_files; i++)
     {
    	 m_RpsDbInfo.pssm_file.write((char *)&i, sizeof(Int4));
    	 m_RpsDbInfo.freq_file.write((char *)&i, sizeof(Int4));
     }

     if(op_cobalt == m_op_mode)
     {
    	 string blocks_str = m_OutDbName + ".blocks";
    	 m_RpsDbInfo.blocks_file.open(blocks_str.c_str());
    	 if (!m_RpsDbInfo.blocks_file.is_open())
    		 NCBI_THROW(CSeqDBException, eFileErr,"Failed to open output .blocks file");
     }


     m_RpsDbInfo.curr_seq_offset = 0;
     //Init them to input arg values first , may change after reading in the first sequence
     m_RpsDbInfo.gap_extend = m_GapExtPenalty;
     m_RpsDbInfo.gap_open = m_GapOpenPenalty;
     m_RpsDbInfo.matrix = m_Matrix;
     m_RpsDbInfo.scale_factor = (Int4) ceil(m_PssmScaleFactor);

     return;
 }

//For first sequence only
void CMakeProfileDBApp::x_UpdateRPSDbInfo(const CPssmWithParameters & pssm_p)
{
	if(pssm_p.IsSetParams())
	{
		if(pssm_p.GetParams().IsSetRpsdbparams())
		{
			const CFormatRpsDbParameters & rps_db_params = pssm_p.GetParams().GetRpsdbparams();
			if(rps_db_params.IsSetGapExtend())
				m_RpsDbInfo.gap_extend = rps_db_params.GetGapExtend();

			if(rps_db_params.IsSetGapOpen())
			     m_RpsDbInfo.gap_open = rps_db_params.GetGapOpen();

			if(rps_db_params.IsSetMatrixName())
			     m_RpsDbInfo.matrix = rps_db_params.GetMatrixName();
		}
	}
	return;
}

void CMakeProfileDBApp::x_FillInRPSDbParameters(CPssmWithParameters &pssm_p )
{
	if(!pssm_p.IsSetParams())
		pssm_p.SetParams();

	if(!pssm_p.GetParams().IsSetRpsdbparams())
		pssm_p.SetParams().SetRpsdbparams();

	CFormatRpsDbParameters & rps_params= pssm_p.SetParams().SetRpsdbparams();
	if(!rps_params.IsSetGapExtend())
		rps_params.SetGapExtend(m_RpsDbInfo.gap_extend);
	else if(rps_params.GetGapExtend() != m_RpsDbInfo.gap_extend)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Gap extend penalties do not match");

	if(!rps_params.IsSetGapOpen())
		rps_params.SetGapOpen(m_RpsDbInfo.gap_open);
	else if(rps_params.GetGapOpen() != m_RpsDbInfo.gap_open)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Gap open penalties do not match");

	if(!rps_params.IsSetMatrixName())
		rps_params.SetMatrixName (m_RpsDbInfo.matrix);
	else if(rps_params.GetMatrixName()!= m_RpsDbInfo.matrix)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Score matrix does not match");

	return;
}

/* Update the input scoremat with a new PSSM and modified
   statistics. Scoremat must contain only residue frequencies.
   Note that upon completion the new PSSM will always have
   columns of length BLASTAA_SIZE
        seq is the sequence and set of score frequencies read in
                from the next data file
        seq_size is the number of letters in this sequence
        alphabet_size refers to the number of PSSM rows
        ScalingFactor is the multiplier for all PSSM scores
*/
void CMakeProfileDBApp::x_RPSUpdateStatistics(CPssmWithParameters & seq, Int4 seq_size )
{

    CPssm & pssm = seq.SetPssm();
    const CPssmParameters & params = seq.GetParams();
    string matrix_name = params.GetRpsdbparams().GetMatrixName();

    /* Read in the sequence residues from the scoremat structure. */
    CNCBIstdaa query_stdaa;
    pssm.GetQuerySequenceData(query_stdaa);

    vector <char>   query_v = query_stdaa.Get();

    if((Int4) (query_v.size()) != seq_size)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Query sequence lengths mismatch");

    /* allocate query array and PSSM row array */
    AutoArray<Uint1>  query(seq_size);

    for(unsigned int i = 0; i < query_v.size(); i++)
    	query[i] = query_v[i];

    auto_ptr<CNcbiMatrix <double> > freq_list (CScorematPssmConverter::GetFreqRatios(seq));

    CPsiBlastInputFreqRatios pssm_freq_ratio(query.get(), seq_size, *freq_list,
    										 matrix_name.c_str(), m_RpsDbInfo.gap_open,
    										 m_RpsDbInfo.gap_extend, m_RpsDbInfo.scale_factor);
    CPssmEngine pssm_engine(&pssm_freq_ratio);
    CRef<CPssmWithParameters> out_par(pssm_engine.Run());

    CPssmFinalData & i = pssm.SetFinalData();
    const CPssmFinalData & o = out_par->GetPssm().GetFinalData();
    i.SetScores() = o.GetScores();
    i.SetLambda() = o.GetLambda();
    i.SetKappa() = o.GetKappa();
    i.SetH() = o.GetH();
    i.SetScalingFactor(m_RpsDbInfo.scale_factor);

    return;
}

 /* The first sequence in the list determines several
    parameters that all other sequences in the list must
    have. In this case, extra initialization is required

         info contains all the information on data files
                 and parameters from previously added sequences
         seq is the sequence and PSSM read in from the next data file
         seq_index refers to the (0-based) position of this sequence
                 in the complete list of seqences
         seq_size is the number of letters in this sequence
         alphabet_size refers to the number of PSSM rows
 */
 void CMakeProfileDBApp::x_RPSAddFirstSequence(CPssmWithParameters & pssm_w_parameters, bool freq_only )
 {
     x_UpdateRPSDbInfo( pssm_w_parameters);

     x_FillInRPSDbParameters(pssm_w_parameters);

     if(!freq_only)
     {
    	 if(pssm_w_parameters.GetPssm().GetFinalData().IsSetScalingFactor())
    	 {
    		 m_RpsDbInfo.scale_factor = pssm_w_parameters.GetPssm().GetFinalData().GetScalingFactor();
    	 }
	 else
	 {
                // asn1 default value is 1
                m_RpsDbInfo.scale_factor = 1.0;
	 }
     }
     else
    	 x_RPSUpdateStatistics(pssm_w_parameters, pssm_w_parameters.GetPssm().GetQueryLength());

     /* scale up the threshold value and convert to integer */
     double threshold = m_RpsDbInfo.scale_factor * m_WordScoreThreshold;

     /* create BLAST lookup table */
     if (LookupTableOptionsNew(eBlastTypeBlastp, &(m_RpsDbInfo.lookup_options)) != 0)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Cannot create lookup options");

     if (BLAST_FillLookupTableOptions(m_RpsDbInfo.lookup_options, eBlastTypePsiBlast,
                                      FALSE, /* no megablast */
                                      threshold, /* neighboring threshold */
                                      BLAST_WORDSIZE_PROT ) != 0)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Cannot set lookup table options");

     if (BlastAaLookupTableNew(m_RpsDbInfo.lookup_options, &(m_RpsDbInfo.lookup)) != 0)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Cannot allocate lookup table");

     m_RpsDbInfo.lookup->use_pssm = TRUE;  /* manually turn on use of PSSMs */

     /* Perform generic query setup */

     if (BlastQuerySetUpOptionsNew(&(m_RpsDbInfo.query_options)) != 0)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Generic query setup failed");

     if (BLAST_FillQuerySetUpOptions(m_RpsDbInfo.query_options, eBlastTypeBlastp,
                                     NULL,        /* no filtering */
                                     0            /* strand not applicable */ ) != 0)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Cannot fill query options");

     /* Write the header of the RPS .aux file */
     m_RpsDbInfo.aux_file << m_RpsDbInfo.matrix << "\n";
     m_RpsDbInfo.aux_file << m_RpsDbInfo.gap_open << "\n";
     m_RpsDbInfo.aux_file << m_RpsDbInfo.gap_extend << "\n";
     m_RpsDbInfo.aux_file << scientific << 0.0 << "\n";
     m_RpsDbInfo.aux_file << scientific << 0.0 << "\n";
     m_RpsDbInfo.aux_file << (int) 0 << "\n";
     m_RpsDbInfo.aux_file << (int) 0 << "\n";
     m_RpsDbInfo.aux_file << fixed << (double) m_RpsDbInfo.scale_factor << "\n";

     return;
 }

 void CMakeProfileDBApp::x_UpdateCobalt(const CPssmWithParameters & pssm_p, Int4 seq_size)
 {
	 const CPssm & pssm = pssm_p.GetPssm();
	 // Update .blocks file
	 const list<CRef<CCoreBlock> > & block_list = pssm_p.GetParams().GetConstraints().GetBlocks();

	 list<CRef<CCoreBlock> >::const_iterator  itr = block_list.begin();

	 int count =0;

	 while(itr != block_list.end())
	 {
		 const CCoreBlock & block = (**itr);
		 if(!block.IsSetStart() || !block.IsSetStop())
			NCBI_THROW(CInputException, eInvalidInput,  "No start Or stop found in conserved block");

		 string seq_id_str = "id" + NStr::IntToString(count);
		 if(pssm.IsSetQuery())
		 {
			 if(pssm.GetQuery().IsSeq())
			 {
				 if(pssm.GetQuery().GetSeq().IsSetDescr())
				 {
					 const list<CRef<CSeqdesc> > descr_list= pssm.GetQuery().GetSeq().GetDescr();
					 if(descr_list.size() > 0)
					 {
						const CRef<CSeqdesc> descr = descr_list.front();
						if(descr->IsTitle())
						{
							string title = descr->GetTitle();
							string accession;
					 		string tmp;
					 		if(NStr::SplitInTwo(title, ",", accession, tmp))
						 		seq_id_str = accession;
						 }
					 }
				 }
			 }
		 }

		 m_RpsDbInfo.blocks_file << seq_id_str << "\t";
		 m_RpsDbInfo.blocks_file << count << "\t";
		 m_RpsDbInfo.blocks_file << block.GetStart() << "\t";
		 m_RpsDbInfo.blocks_file << block.GetStop() << "\n";
		 count++;
		 ++itr;
	 }
	 return;
 }
void CMakeProfileDBApp::x_UpdateFreqRatios(const CPssmWithParameters & pssm_p, Int4 seq_index, Int4 seq_size)
 {
	if (!m_UpdateFreqRatios)
		return;

	 const CPssm & pssm = pssm_p.GetPssm();
	 // Update .freq file
	 Int4 i = 0;
	 Int4 j = 0;
	 Int4 row[BLASTAA_SIZE];
	 Int4 alphabet_size = pssm.GetNumRows();

	 const list<double> & freq_ratios = pssm.GetIntermediateData().GetFreqRatios();
	 list<double>::const_iterator itr_fr = freq_ratios.begin();
    m_RpsDbInfo.freq_file.seekp(0, ios_base::end);

	 if (pssm.GetByRow() == FALSE) {
	    for (i = 0; i < seq_size; i++) {
	        for (j = 0; j < alphabet_size; j++) {
	            if (itr_fr == freq_ratios.end())
	                break;
	            row[j] = (Int4) BLAST_Nint(*itr_fr * FREQ_RATIO_SCALE);
	            ++itr_fr;
	        }
	        for ( ;j < BLASTAA_SIZE; j++) {
	        	row[j] = 0;
	        }
	        m_RpsDbInfo.freq_file.write((const char *)row, sizeof(Int4)*BLASTAA_SIZE);
	    }
    }
    else {
    	auto_ptr<CNcbiMatrix<double> > matrix (CScorematPssmConverter::GetFreqRatios(pssm_p));

	    for (i = 0; i < seq_size; i++) {
	        for (j = 0; j < BLASTAA_SIZE; j++) {
	            row[j] = (Int4) BLAST_Nint((*matrix)(i,j ) * FREQ_RATIO_SCALE);
	        }
	        m_RpsDbInfo.freq_file.write((const char *)row, sizeof(Int4)*BLASTAA_SIZE);
	    }
    }

	memset(row, 0, sizeof(row));
	m_RpsDbInfo.freq_file.write((const char *)row, sizeof(Int4)*BLASTAA_SIZE);

    m_RpsDbInfo.freq_file.seekp( 8 + (seq_index) * sizeof(Int4), ios_base::beg);
    m_RpsDbInfo.freq_file.write((const char *) &m_RpsDbInfo.curr_seq_offset, sizeof(Int4));
	return;
 }

 /* Incrementally update the BLAST lookup table with
    words derived from the present sequence
         info contains all the information on data files
                 and parameters from previously added sequences
         seq is the sequence and PSSM read in from the next data file
         seq_size is the number of letters in this sequence
 */
 void CMakeProfileDBApp::x_RPSUpdateLookup(Int4 seq_size)
 {
     BlastSeqLoc *lookup_segment = NULL;

     /* Tell the blast engine to index the entire input
        sequence. Since only the PSSM matters for lookup
        table creation, the process does not require
        actually extracting the sequence data from 'seq'*/

     BlastSeqLocNew(&lookup_segment, 0, seq_size - 1);

     /* add this sequence to the lookup table. NULL
        is passed in place of the query */

     Int4 ** posMatrix = m_RpsDbInfo.pos_matrix.Get();
     if (NULL == posMatrix)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "Empty pos matrix");

     BlastAaLookupIndexQuery(m_RpsDbInfo.lookup, posMatrix,
                             NULL, lookup_segment, m_RpsDbInfo.curr_seq_offset);

     BlastSeqLocFree(lookup_segment);
     return;
 }

 /* Incrementally update the RPS PSSM file with the
    PSSM for the next input sequence
         info contains all the information on data files
                 and parameters from previously added sequences
         seq is the sequence and PSSM read in from the next data file
         seq_index refers to the (0-based) position of this sequence
                 in the complete list of seqences
         seq_size is the number of letters in this sequence
         alphabet_size refers to the number of PSSM rows
 */
void CMakeProfileDBApp::x_RPSUpdatePSSM(const CPssm & pssm, Int4 seq_index, Int4 seq_size)
{
     Int4 i = 0;
     Int4 j = 0;

     /* Note that RPS blast requires an extra column at
      * the end of the PSSM */

     list<int>::const_iterator  score_list_itr = pssm.GetFinalData().GetScores().begin();
     list<int>::const_iterator  score_list_end = pssm.GetFinalData().GetScores().end();
     Int4 alphabet_size = pssm.GetNumRows();

     m_RpsDbInfo.pos_matrix.Create(seq_size + 1);
     Int4 ** posMatrix = m_RpsDbInfo.pos_matrix.Get();
     if (pssm.GetByRow() == FALSE) {
         for (i = 0; i < seq_size; i++) {
             for (j = 0; j < alphabet_size; j++) {
                 if (score_list_itr == score_list_end)
                     break;
                 posMatrix[i][j] = *score_list_itr;
                 score_list_itr++;
             }
             if (j < alphabet_size)
                 break;
             for (; j < BLASTAA_SIZE; j++) {
                 posMatrix[i][j] = INT2_MIN;
             }
         }
     }
     else {
         for (j = 0; j < alphabet_size; j++) {
             for (i = 0; i < seq_size; i++) {
                 if (score_list_itr == score_list_end)
                     break;
                 posMatrix[i][j] = *score_list_itr;
                 score_list_itr++;
             }
             if (i < seq_size)
                 break;
         }
         if (j == alphabet_size) {
             for (; j < BLASTAA_SIZE; j++) {
                 for (i = 0; i < seq_size; i++) {
                     posMatrix[i][j] = INT2_MIN;
                 }
             }
         }
     }

     if (i < seq_size || j < alphabet_size)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "PSSM was truncated early");

     if(score_list_itr != score_list_end)
    	 NCBI_THROW(CBlastException, eCoreBlastError, "PSSM too large for this sequence");

     /* manually fill in the extra (last) column of the PSSM.
        Note that the value to use should more appropriately
        be BLAST_SCORE_MIN, but we instead follow the convention
        used in copymat */

     for (i = 0; i < BLASTAA_SIZE; i++)
        posMatrix[seq_size][i] = -BLAST_SCORE_MAX;

     /* Dump the score matrix, column by column */
     m_RpsDbInfo.pssm_file.seekp(0, ios_base::end);
     for (i = 0; i < seq_size + 1; i++) {
    	m_RpsDbInfo.pssm_file.write((const char *) posMatrix[i], sizeof(Int4)*BLASTAA_SIZE);
     }
     /* Write the next context offset. Note that the
        RPSProfileHeader structure is one int too large for
        our purposes, so that the index of this sequence
        must be decremented to get the right byte offset
        into the file */

     m_RpsDbInfo.pssm_file.seekp( 8 + (seq_index) * sizeof(Int4), ios_base::beg);
     m_RpsDbInfo.pssm_file.write((const char *) &m_RpsDbInfo.curr_seq_offset, sizeof(Int4));

     return;
 }

/* Once all sequences have been processed, perform
   final setup on the BLAST lookup table and finish
   up the RPS files */

void CMakeProfileDBApp::x_RPS_DbClose(void)
{
    /* Write the last context offset to the PSSM file.
       This is the total number of letters for all RPS
       DB sequences combined */

    m_RpsDbInfo.pssm_file.seekp(8 + (m_RpsDbInfo.num_seqs) * sizeof(Int4), ios::beg);
    m_RpsDbInfo.pssm_file.write((const char *) &m_RpsDbInfo.curr_seq_offset, sizeof(Int4));
    m_RpsDbInfo.freq_file.seekp(8 + (m_RpsDbInfo.num_seqs) * sizeof(Int4), ios::beg);
    m_RpsDbInfo.freq_file.write((const char *) &m_RpsDbInfo.curr_seq_offset, sizeof(Int4));

    /* Pack the lookup table into its compressed form */
    if(NULL == m_RpsDbInfo.lookup)
        NCBI_THROW(CBlastException, eCoreBlastError, "Empty database");

    if (BlastAaLookupFinalize(m_RpsDbInfo.lookup, eBackbone) != 0) {
        NCBI_THROW(CBlastException, eCoreBlastError, "Failed to compress lookup table");
    }
    else {
        /* Change the lookup table format to match that
           of the legacy BLAST lookup table */

        BlastRPSLookupFileHeader header;
        BlastAaLookupTable *lut = m_RpsDbInfo.lookup;
        Int4 i, index;
        Int4 cursor, old_cursor;
        AaLookupBackboneCell *cell;
        RPSBackboneCell empty_cell;

        memset(&header, 0, sizeof(header));
        header.magic_number = RPS_DATABASE_VERSION;

        /* for each lookup table cell */

        for (index = cursor = 0; index < lut->backbone_size; index++) {
            cell = (AaLookupBackboneCell*)lut->thick_backbone + index;


            if (cell->num_used == 0)
                continue;

            /* The cell contains hits */

            if (cell->num_used <= RPS_HITS_PER_CELL) {
                /* if 3 hits or less, just update each hit offset
                   to point to the end of the word rather than
                   the beginning */

                for (i = 0; i < cell->num_used; i++)
                    cell->payload.entries[i] += BLAST_WORDSIZE_PROT - 1;
            }
            else {
                /* if more than 3 hits, pack the first hit into the
                   lookup table cell, pack the overflow array byte
                   offset into the cell, and compress the resulting
                   'hole' in the overflow array. Update the hit
                   offsets as well */

                old_cursor = cell->payload.overflow_cursor;
                cell->payload.entries[0] = ((Int4*)lut->overflow)[old_cursor] +
                                        BLAST_WORDSIZE_PROT - 1;
                cell->payload.entries[1] = cursor * sizeof(Int4);
                for (i = 1; i < cell->num_used; i++, cursor++) {
                    ((Int4*)lut->overflow)[cursor]
                            = ((Int4*)lut->overflow)[old_cursor + i] +
                                        BLAST_WORDSIZE_PROT - 1;
                }
            }
        }

        header.start_of_backbone = sizeof(header);
        header.end_of_overflow = header.start_of_backbone +
                   (RPS_NUM_LOOKUP_CELLS + 1) * sizeof(RPSBackboneCell) +
                   cursor * sizeof(Int4);

        /* write the lookup file header */

        m_RpsDbInfo.lookup_file.write((const char *)&header, sizeof(header));

        /* write the thick backbone */

        m_RpsDbInfo.lookup_file.write((const char *)lut->thick_backbone,
        							  sizeof(RPSBackboneCell)* lut->backbone_size);

        /* write extra backbone cells */
        memset(&empty_cell, 0, sizeof(empty_cell));
        for (i = lut->backbone_size; i < RPS_NUM_LOOKUP_CELLS + 1; i++) {
            m_RpsDbInfo.lookup_file.write((const char *)&empty_cell, sizeof(empty_cell));
        }

        /* write the new overflow array */
        m_RpsDbInfo.lookup_file.write((const char *)lut->overflow, sizeof(Int4)*cursor);
    }

    /* Free data, close files */

    m_RpsDbInfo.lookup = BlastAaLookupTableDestruct(m_RpsDbInfo.lookup);
    m_RpsDbInfo.query_options = BlastQuerySetUpOptionsFree(m_RpsDbInfo.query_options);
    m_RpsDbInfo.lookup_file.flush();
    m_RpsDbInfo.lookup_file.close();
    m_RpsDbInfo.pssm_file.flush();
    m_RpsDbInfo.pssm_file.close();
    m_RpsDbInfo.aux_file.flush();
    m_RpsDbInfo.aux_file.close();
	m_RpsDbInfo.freq_file.flush();
	m_RpsDbInfo.freq_file.close();

    if(op_cobalt == m_op_mode)
    {
    	m_RpsDbInfo.blocks_file.flush();
    	m_RpsDbInfo.blocks_file.close();
    }
    else if(!m_UpdateFreqRatios)
    {
    	string freq_str = m_OutDbName + ".freq";
	 	CFile(freq_str).Remove();
    }

}

void CMakeProfileDBApp::Init(void)
{
	x_SetupArgDescriptions();
	x_InitProgramParameters();
}

static bool s_HasDefline(const CBioseq & bio)
{
    if (bio.CanGetDescr()) {
        return true;
    }

    return false;
}

static CRef<CBlast_def_line_set> s_GenerateBlastDefline(const CBioseq & bio)
{
	 CRef<CBlast_def_line_set> defline_set(new CBlast_def_line_set());
	 CRef<CBlast_def_line> defline(new CBlast_def_line());
	 defline->SetSeqid() = bio.GetId();
	 defline_set->Set().push_back(defline);
	 return defline_set;
}

int CMakeProfileDBApp::Run(void)
{
	vector<string> smpFilenames = (op_delta == m_op_mode )? x_CreateDeltaList():x_GetSMPFilenames();

	x_InitRPSDbInfo((Int4) smpFilenames.size());
	x_InitOutputDb();

	for(int seq_index=0; seq_index < m_RpsDbInfo.num_seqs; seq_index++)
	{
		string filename = smpFilenames[seq_index];
		CFile f(filename);
		if(!f.Exists())
		{
			string err = filename + " does not exists";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		//Read PssmWithParameters from file
		CPssmWithParameters pssm_w_parameters;
		if(m_binary_scoremat)
		{
			CNcbiIfstream	in_stream(filename.c_str(), ios::binary);
			in_stream >> MSerial_AsnBinary >> pssm_w_parameters;
		}
		else
		{
			CNcbiIfstream	in_stream(filename.c_str());
			in_stream >> MSerial_AsnText >> pssm_w_parameters;
		}

		CheckInputScoremat_RV sm = x_CheckInputScoremat(pssm_w_parameters, filename);
		// Should have error out already....
		if(sm_invalid == sm)
		{
			string err = filename + " contains invalid scoremat";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		const CPssm & pssm = pssm_w_parameters.GetPssm();
		int seq_size = pssm.GetQueryLength();

		const CBioseq & bioseq = pssm.GetQuery().GetSeq();
		CRef<CBlast_def_line_set> deflines;
		if(s_HasDefline(bioseq)) {
			deflines = CWriteDB::ExtractBioseqDeflines(bioseq);
		}
		else {
			deflines = s_GenerateBlastDefline(bioseq);
		}

		m_Taxids->FixTaxId(deflines);
		m_OutputDb->AddSequence(bioseq);
		m_OutputDb->SetDeflines(*deflines);

		//Complete RpsDnInfo init with data from first file
		if(NULL == m_RpsDbInfo.lookup)
		{
			x_RPSAddFirstSequence(pssm_w_parameters, sm == sm_valid_freq_only);
		}
		else
		{
			 x_FillInRPSDbParameters(pssm_w_parameters);
			if(sm_valid_freq_only == sm)
				x_RPSUpdateStatistics(pssm_w_parameters, seq_size);

			if( pssm.GetFinalData().IsSetScalingFactor())
			{
				if( pssm.GetFinalData().GetScalingFactor() != m_RpsDbInfo.scale_factor)
					NCBI_THROW(CBlastException, eCoreBlastError, "Scaling factors do not match");
			}
                        else
                        {
                               // If scaling factor not specified, the default is 1
                               if( 1 != m_RpsDbInfo.scale_factor)
					NCBI_THROW(CBlastException, eCoreBlastError, "Scaling factors do not match");
                        }

		}

		x_RPSUpdatePSSM(pssm, seq_index, seq_size);
		x_RPSUpdateLookup(seq_size);
		x_UpdateFreqRatios(pssm_w_parameters, seq_index, seq_size);

		m_RpsDbInfo.aux_file << seq_size << "\n";
		m_RpsDbInfo.aux_file << scientific << pssm.GetFinalData().GetKappa() << "\n";
		m_RpsDbInfo.curr_seq_offset +=(seq_size +1);
		m_RpsDbInfo.pos_matrix.Delete();

		if(op_cobalt == m_op_mode)
			x_UpdateCobalt(pssm_w_parameters, seq_size);
	}

	m_OutputDb->Close();
	x_RPS_DbClose();
	m_Done = true;
	return 0;
}

static void s_WriteInt4List(CNcbiOfstream & ostr, const list<Int4> & l)
{
	ITERATE(list<Int4>, it, l)
	{
		ostr.write((char*)&(*it), sizeof(Int4));
	}
}

static void s_WriteUint4List(CNcbiOfstream & ostr, const list<Uint4> & l)
{
	ITERATE(list<Uint4>, it, l)
	{
		ostr.write((char*)&(*it), sizeof(Uint4));
	}
}

vector<string> CMakeProfileDBApp::x_CreateDeltaList(void)
{
	vector<string> smpFilenames = x_GetSMPFilenames();
	vector<string> deltaList;

	CTmpFile tmp_obsr_file(CTmpFile::eRemove);
	CTmpFile tmp_freq_file(CTmpFile::eRemove);
	CNcbiOfstream  tmp_obsr_buff(tmp_obsr_file.GetFileName().c_str(), IOS_BASE::out | IOS_BASE::binary);
	CNcbiOfstream  tmp_freq_buff(tmp_freq_file.GetFileName().c_str(), IOS_BASE::out | IOS_BASE::binary);

	for(unsigned int seq_index=0; seq_index < smpFilenames.size(); seq_index++)
	{
		string filename = smpFilenames[seq_index];
		CFile f(filename);
		if(!f.Exists())
		{
			string err = filename + " does not exists";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		//Read PssmWithParameters from file
		CPssmWithParameters pssm_w_parameters;
		if(m_binary_scoremat)
		{
			CNcbiIfstream	in_stream(filename.c_str(), ios::binary);
			in_stream >> MSerial_AsnBinary >> pssm_w_parameters;
		}
		else
		{
			CNcbiIfstream	in_stream(filename.c_str());
			in_stream >> MSerial_AsnText >> pssm_w_parameters;
		}

		CheckInputScoremat_RV sm = x_CheckInputScoremat(pssm_w_parameters, filename);
		// Should have error out already....
		if(sm_invalid == sm)
		{
			string err = filename + " contains invalid scoremat";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		const CPssm & pssm = pssm_w_parameters.GetPssm();
		int seq_size = pssm.GetQueryLength();
		if(!pssm.IsSetIntermediateData()|| !pssm.GetIntermediateData().IsSetWeightedResFreqsPerPos())
		{
			string err = filename + " contains no weighted residue frequencies for building delta database";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		if(!pssm.GetIntermediateData().IsSetNumIndeptObsr())
		{
			string err = filename + " contains no observations information for building delta database";
			NCBI_THROW(CInputException, eInvalidInput,  err);
		}

		if (true == x_UpdateDelta(pssm, seq_size, filename, tmp_obsr_buff, tmp_freq_buff))
		{
			deltaList.push_back(filename);
		}

	}

	tmp_obsr_buff.flush();
	tmp_freq_buff.flush();
	x_WrapUpDelta(tmp_obsr_file, tmp_freq_file);

	return deltaList;
}

bool CMakeProfileDBApp::x_ValidateCd(const list<double>& freqs,
                                     const list<double>& observ,
                                     unsigned int alphabet_size)
{

    if (freqs.size() / alphabet_size != observ.size())
    {
    	string err = "Number of frequency and observations columns do not match";
        NCBI_THROW(CException, eInvalid, err);
    }

    ITERATE (list<double>, it, freqs)
    {
        unsigned int residue = 0;
        double sum = 0.0;
        while (residue < alphabet_size - 1)
        {
            sum += *it;
            it++;
            residue++;
        }
        sum += *it;

        if (fabs(sum - 1.0) > kEpsylon)
            return false;
    }

    ITERATE (list<double>, it, observ)
    {
           if (*it < 1.0)
               return false;
   }

    return true;
}


bool CMakeProfileDBApp::x_UpdateDelta( const CPssm  & pssm, Int4 seq_size, const string & filename,
									   CNcbiOfstream & obsr_buff_file, CNcbiOfstream & freq_buff_file)
{

    // get weightd residue frequencies
   const list<double>& orig_freqs = pssm.GetIntermediateData().GetWeightedResFreqsPerPos();

    // get number of independent observations
    const list<double>& obsr = pssm.GetIntermediateData().GetNumIndeptObsr();

    int alphabet_size = pssm.GetNumRows();
    list<double> modify_freqs;

    if(pssm.GetByRow())
    {
    	// need to flip the freq matrix
    	vector<double> tmp(orig_freqs.size());
    	list<double>::const_iterator f_itr = orig_freqs.begin();

    	for(int i = 0; i < alphabet_size; i++)
    	{
    		for(int j = 0; j < seq_size; j++)
    		{
    			tmp[i + j*alphabet_size] = *f_itr;
    			++f_itr;
    		}
    	}
    	copy(tmp.begin(), tmp.end(), modify_freqs.begin());
    }

    // Pad matrix if necessary
  	if(alphabet_size < BLASTAA_SIZE)
  	{
  		if(0 == modify_freqs.size())
  			copy(orig_freqs.begin(), orig_freqs.end(), modify_freqs.begin());

  		list<double>::iterator p_itr = modify_freqs.begin();

  		for (int j=0; j < seq_size; j++)
  		{
  			for(int i=0; i < alphabet_size; i++)
  			{
  				if(modify_freqs.end() == p_itr)
  					break;

  				++p_itr;
  			}

  			modify_freqs.insert(p_itr, (BLASTAA_SIZE-alphabet_size), 0);
  		}
    }

  	const list<double> & freqs = (modify_freqs.size()? modify_freqs:orig_freqs );
    double max_obsr = *max_element(obsr.begin(), obsr.end()) + 1.0;
    if(max_obsr < m_ObsrvThreshold)
    {
    	*m_LogFile << filename +
    			" was excluded: due to too few independent observations\n";
    	return false;
    }

    if( !x_ValidateCd(freqs, obsr, BLASTAA_SIZE) && m_ExcludeInvalid)
    {
    	*m_LogFile << filename +
    			" was excluded: it conatins an invalid CD \n";
    	return false;
    }

    //save offset for this record
    m_ObsrOffsets.push_back(m_CurrObsrOffset);

    list<Uint4> ObsrBuff;
    // write effective observations in compressed form
    // as a list of pairs: value, number of occurences
    unsigned int num_obsr_columns = 0;
    list<double>::const_iterator obsr_it = obsr.begin();
    do
    {
    	double current = *obsr_it;
        Uint4 num = 1;
        num_obsr_columns++;
        obsr_it++;
        while (obsr_it != obsr.end() && fabs(*obsr_it - current) < 1e-4)
        {
        	obsr_it++;
            num++;
            num_obsr_columns++;
        }

        // +1 because pssm engine returns alpha (in psi-blast papers)
        // which is number of independent observations - 1
        ObsrBuff.push_back((Uint4)((current + 1.0) * kFixedPointScaleFactor));
        ObsrBuff.push_back(num);
    }
  	while (obsr_it != obsr.end());

    Uint4 num_weighted_counts = 0;

    // save offset for this frequencies record
    m_FreqOffsets.push_back(m_CurrFreqOffset / BLASTAA_SIZE);

    list<Uint4> FreqBuff;
    // save weighted residue frequencies
    ITERATE (list<double>, it, freqs)
    {
      	FreqBuff.push_back((Uint4)(*it * kFixedPointScaleFactor));
      	num_weighted_counts++;
    }

    if (num_obsr_columns != num_weighted_counts / BLASTAA_SIZE)
    {
    	string err = "Number of frequencies and observations columns do not match in " + filename;
    	NCBI_THROW(CException, eInvalid, err);
    }

    // additional column of zeros is added for compatibility with rps database
    unsigned int padded_size = FreqBuff.size() + BLASTAA_SIZE;
    FreqBuff.resize(padded_size, 0);

    m_CurrFreqOffset += FreqBuff.size();
    m_CurrObsrOffset += ObsrBuff.size();
    s_WriteUint4List(freq_buff_file, FreqBuff);
    s_WriteUint4List(obsr_buff_file, ObsrBuff);

    return true;
}



void CMakeProfileDBApp::x_WrapUpDelta(CTmpFile & tmp_obsr_file, CTmpFile & tmp_freq_file)
{
    m_FreqOffsets.push_back(m_CurrFreqOffset / BLASTAA_SIZE);
    m_ObsrOffsets.push_back(m_CurrObsrOffset);

    string wcounts_str = m_OutDbName + ".wcounts";
    CNcbiOfstream wcounts_file(wcounts_str.c_str(), ios::out | ios::binary);
    if (!wcounts_file.is_open())
    	 NCBI_THROW(CSeqDBException, eFileErr,"Failed to open output .wcounts file");

    string obsr_str = m_OutDbName + ".obsr";
    CNcbiOfstream 	 obsr_file(obsr_str.c_str(), IOS_BASE::out|IOS_BASE::binary);
    if (!obsr_file.is_open())
    	 NCBI_THROW(CSeqDBException, eFileErr,"Failed to open output .obsr file");

 	CNcbiIfstream tmp_obsr_buff (tmp_obsr_file.GetFileName().c_str(), IOS_BASE::in | IOS_BASE::binary);
 	CNcbiIfstream tmp_freq_buff (tmp_freq_file.GetFileName().c_str(), IOS_BASE::in | IOS_BASE::binary);

    // write RPS BLAST database magic number
    Int4 magic_number = RPS_MAGIC_NUM_28;
    wcounts_file.write((char*)&magic_number, sizeof(Int4));
    obsr_file.write((char*)&magic_number, sizeof(Int4));

    // write number of recrods
    Int4 num_wcounts_records = m_FreqOffsets.size() -1;
    Int4 num_obsr_records = m_ObsrOffsets.size() -1;
    wcounts_file.write((char*)&num_wcounts_records, sizeof(Int4));
    obsr_file.write((char*)&num_obsr_records, sizeof(Int4));

    s_WriteInt4List(wcounts_file, m_FreqOffsets);
    wcounts_file.flush();
    wcounts_file << tmp_freq_buff.rdbuf();
    wcounts_file.flush();
    wcounts_file.close();

    s_WriteInt4List(obsr_file, m_ObsrOffsets);
    obsr_file.flush();
    obsr_file << tmp_obsr_buff.rdbuf();
    obsr_file.flush();
    obsr_file.close();
}



#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
		return CMakeProfileDBApp().AppMain(argc, argv, 0, eDS_Default, 0);
}




#endif /* SKIP_DOXYGEN_PROCESSING */

