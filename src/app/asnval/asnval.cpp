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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko
 *
 * File Description:
 *   validator
 *
 */

#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/error_codes.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectio.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Objects includes
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objtools/validator/validator.hpp>
#include <objtools/validator/valid_cmdargs.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include <objects/seqset/Bioseq_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <serial/objostrxml.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <util/compress/stream_util.hpp>
#include <objtools/readers/format_guess_ex.hpp>

#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace objects;
using namespace validator;

#define USE_XMLWRAPP_LIBS

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//

class CValXMLStream;

class CAsnvalApp : public CNcbiApplication, CReadClassMemberHook
{
public:
    CAsnvalApp(void);
    ~CAsnvalApp(void);

    virtual void Init(void);
    virtual int  Run (void);

    // CReadClassMemberHook override
    void ReadClassMember(CObjectIStream& in,
        const CObjectInfo::CMemberIterator& member);

private:

    void Setup(const CArgs& args);
    void x_AliasLogFile();

    unique_ptr<CObjectIStream> OpenFile(const string& fname, string& asn_type);

    CConstRef<CValidError> ProcessCatenated(void);
    CConstRef<CValidError> ProcessSeqEntry(CSeq_entry& se);
    CConstRef<CValidError> ProcessSeqEntry(void);
    CConstRef<CValidError> ProcessSeqSubmit(void);
    CConstRef<CValidError> ProcessSeqAnnot(void);
    CConstRef<CValidError> ProcessSeqFeat(void);
    CConstRef<CValidError> ProcessBioSource(void);
    CConstRef<CValidError> ProcessPubdesc(void);
    CConstRef<CValidError> ProcessBioseqset(void);
    CConstRef<CValidError> ProcessBioseq(void);
    CConstRef<CValidError> ProcessSeqDesc(void);

    CConstRef<CValidError> ValidateInput(string asn_type);
    void ValidateOneDirectory(string dir_name, bool recurse);
    void ValidateOneFile(const string& fname);
    void ProcessBSSReleaseFile(const CArgs& args);
    void ProcessSSMReleaseFile(const CArgs& args);

    void ConstructOutputStreams();
    void DestroyOutputStreams();

    CRef<CSeq_feat> ReadSeqFeat(void);
    CRef<CBioSource> ReadBioSource(void);
    CRef<CPubdesc> ReadPubdesc(void);

    /// @param p_exception
    ///   Pointer to the exception with more info on the read failure or nullptr if not available
    /// @return
    ///   The error(s) to add to describe the read failure
    CRef<CValidError> ReportReadFailure(const CException *p_exception);

    CRef<CScope> BuildScope(void);

    void PrintValidError(CConstRef<CValidError> errors, 
        const CArgs& args);

    enum EVerbosity {
        eVerbosity_Normal = 1,
        eVerbosity_Spaced = 2,
        eVerbosity_Tabbed = 3,
        eVerbosity_XML = 4,
        eVerbosity_min = 1, eVerbosity_max = 4
    };

    void PrintValidErrItem(const CValidErrItem& item);

    CRef<CObjectManager> m_ObjMgr;
    unique_ptr<CObjectIStream> m_In;
    unsigned int m_Options;
    bool m_Continue;
    bool m_OnlyAnnots;
    bool m_Quiet;
    double m_Longest;
    string m_CurrentId;
    string m_LongestId;
    size_t m_NumFiles;
    size_t m_NumRecords;

    size_t m_Level;
    size_t m_Reported;
    EDiagSev m_ReportLevel;

    bool m_DoCleanup;
    CCleanup m_Cleanup;

    EDiagSev m_LowCutoff;
    EDiagSev m_HighCutoff;

    EVerbosity m_verbosity;
    string     m_obj_type;

    CNcbiOstream* m_ValidErrorStream;
#ifdef USE_XMLWRAPP_LIBS
    unique_ptr<CValXMLStream> m_ostr_xml;
#endif
};

class CValXMLStream: public CObjectOStreamXml
{
public:
    CValXMLStream(CNcbiOstream& out, EOwnership deleteOut) : CObjectOStreamXml(out, deleteOut){};
    void Print(const CValidErrItem& item);
};


// constructor
CAsnvalApp::CAsnvalApp(void) :
    m_ObjMgr(0), m_Options(0), m_Continue(false), m_OnlyAnnots(false),
    m_Quiet(false), m_Longest(0), m_CurrentId(""), m_LongestId(""), m_NumFiles(0),
    m_NumRecords(0), m_Level(0), m_Reported(0), m_verbosity(eVerbosity_min),
    m_ValidErrorStream(0)
{
    const CVersionInfo vers (3, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY);
    SetVersion (vers);
}


// destructor
CAsnvalApp::~CAsnvalApp (void)

{
}


void CAsnvalApp::x_AliasLogFile(void)
{
    const CArgs& args = GetArgs();
    Setup(args);

    if (args["L"]) {
        if (args["logfile"]) {
            if (NStr::Equal(args["L"].AsString(), args["logfile"].AsString())) {
                // no-op
            } else {
                NCBI_THROW(CException, eUnknown, "Cannot specify both -L and -logfile");
            }
        } else {
            SetLogFile(args["L"].AsString());
        }
    }
}


void CAsnvalApp::Init(void)
{
    // Prepare command line descriptions

    // Create
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddOptionalKey
        ("p", "Directory", "Path to ASN.1 Files",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey
        ("i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey(
        "o", "OutFile", "Single Output File",
        CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey(
        "f", "Filter", "Substring Filter",
        CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey
        ("x", "String", "File Selection Substring", CArgDescriptions::eString, ".ent");
    arg_desc->AddFlag("u", "Recurse");
    arg_desc->AddDefaultKey(
        "R", "SevCount", "Severity for Error in Return Code\n\
\tinfo(0)\n\
\twarning(1)\n\
\terror(2)\n\
\tcritical(3)\n\
\tfatal(4)\n\
\ttrace(5)",
        CArgDescriptions::eInteger, "4");
    arg_desc->AddDefaultKey(
        "Q", "SevLevel", "Lowest Severity for Error to Show\n\
\tinfo(0)\n\
\twarning(1)\n\
\terror(2)\n\
\tcritical(3)\n\
\tfatal(4)\n\
\ttrace(5)",
        CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey(
        "P", "SevLevel", "Highest Severity for Error to Show\n\
\tinfo(0)\n\
\twarning(1)\n\
\terror(2)\n\
\tcritical(3)\n\
\tfatal(4)\n\
\ttrace(5)",
        CArgDescriptions::eInteger, "5");
    CArgAllow* constraint = new CArgAllow_Integers(eDiagSevMin, eDiagSevMax);
    arg_desc->SetConstraint("Q", constraint);
    arg_desc->SetConstraint("P", constraint);
    arg_desc->SetConstraint("R", constraint);
    arg_desc->AddOptionalKey(
        "E", "String", "Only Error Code to Show",
        CArgDescriptions::eString);

    arg_desc->AddDefaultKey("a", "a", 
                            "ASN.1 Type\n\
\ta Automatic\n\
\tc Catenated\n\
\tz Any\n\
\te Seq-entry\n\
\tb Bioseq\n\
\ts Bioseq-set\n\
\tm Seq-submit\n\
\tt Batch Bioseq-set\n\
\tu Batch Seq-submit\n\
\td Seq-desc",
                            CArgDescriptions::eString,
                            "a");

    arg_desc->AddFlag("b", "Input is in binary format");
    arg_desc->AddFlag("c", "Batch File is Compressed");

    arg_desc->AddFlag("quiet", "Do not log progress");

    CValidatorArgUtil::SetupArgDescriptions(arg_desc.get());
    arg_desc->AddFlag("annot", "Verify Seq-annots only");

    arg_desc->AddOptionalKey(
        "L", "OutFile", "Log File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("v", "Verbosity", 
                            "Verbosity\n"
                            "\t1 Standard Report\n"
                            "\t2 Accession / Severity / Code(space delimited)\n"
                            "\t3 Accession / Severity / Code(tab delimited)\n"
                            "\t4 XML Report",
                            CArgDescriptions::eInteger, "1");
    CArgAllow* v_constraint = new CArgAllow_Integers(eVerbosity_min, eVerbosity_max);
    arg_desc->SetConstraint("v", v_constraint);

    arg_desc->AddFlag("cleanup", "Perform BasicCleanup before validating (to match C Toolkit)");

    arg_desc->AddOptionalKey(
        "D", "String", "Path to lat_lon country data files",
        CArgDescriptions::eString);

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc,
                                              CDataLoadersUtil::fDefault |
                                              CDataLoadersUtil::fGenbankOffByDefault);

    // Program description
    string prog_description = "ASN Validator\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

    x_AliasLogFile();
}


CConstRef<CValidError> CAsnvalApp::ValidateInput(string asn_type)
{
    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    CConstRef<CValidError> eval;
    // ASN.1 Type (a Automatic, z Any, e Seq-entry, b Bioseq, s Bioseq-set, m Seq-submit, t Batch Bioseq-set, u Batch Seq-submit",
    m_In->ReadFileHeader();
    string header = asn_type;
    if (header.empty() && !m_obj_type.empty())
    {
        switch (m_obj_type[0])
        {
        case 'e':
            header = "Seq-entry";
            break;
        case 'm':
            header = "Seq-submit";
            break;
        case 's':
            header = "Bioseq-set";
            break;
        case 'b':
            header = "Bioseq";
            break;
        case 'd':
            header = "Seqdesc";
            break;
        }
    }

    if (!m_obj_type.empty() && m_obj_type[0] == 'c') {
        eval = ProcessCatenated();
    } else if (header == "Seq-submit" ) {           // Seq-submit
        eval = ProcessSeqSubmit();
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        eval = ProcessSeqEntry();
    } else if ( header == "Seq-annot" ) {           // Seq-annot
        eval = ProcessSeqAnnot();
    } else if (header == "Seq-feat" ) {             // Seq-feat
        eval = ProcessSeqFeat();
    } else if (header == "BioSource" ) {            // BioSource
        eval = ProcessBioSource();
    } else if (header == "Pubdesc" ) {              // Pubdesc
        eval = ProcessPubdesc();
    } else if (header == "Bioseq-set" ) {           // Bioseq-set
        eval = ProcessBioseqset();
    } else if (header == "Bioseq" ) {               // Bioseq
        eval = ProcessBioseq();
    } else if (header == "Seqdesc" ) {             // Seq-desc
        eval = ProcessSeqDesc();
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }

    return eval;
}


void CAsnvalApp::ValidateOneFile(const string& fname)
{
    const CArgs& args = GetArgs();

    if (! args["quiet"] ) {
        LOG_POST_XX(Corelib_App, 1, fname);
    }
    unique_ptr<CNcbiOfstream> local_stream;

    bool close_error_stream = false;

    try {
        if (!m_ValidErrorStream) {
            string path;
            if (fname.empty())  {
                path = "stdin.val";
            } else {
                size_t pos = NStr::Find(fname, ".", NStr::eNocase, NStr::eReverseSearch);
                if (pos != string::npos)
                    path = fname.substr(0, pos);
                else
                    path = fname;

                path.append(".val");
            }

            local_stream.reset(new CNcbiOfstream(path.c_str()));
            m_ValidErrorStream = local_stream.get();

            ConstructOutputStreams();
            close_error_stream = true;
        }
    }
    catch (CException) {
    }

    string asn_type;
    m_In = OpenFile(fname, asn_type);
    if (!m_In) {
        PrintValidError(ReportReadFailure(nullptr), args);
        if (close_error_stream) {
            DestroyOutputStreams();
        }
        // NCBI_THROW(CException, eUnknown, "Unable to open " + fname);
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Unable to open invalid ASN.1 file " + fname);
    } else {
        try {
            if (/*asn_type == "Bioseq-set" ||*/ m_obj_type == "t") {    // Bioseq-set release file
                ProcessBSSReleaseFile(args);
            } else if (/*asn_type == "Seq-submit" ||*/ m_obj_type == "u") { // Seq-submit release file
                ProcessSSMReleaseFile(args);
            } else {
                size_t num_validated = 0;
                while (true) {
                   CStopWatch sw(CStopWatch::eStart);
                   try {
                        CConstRef<CValidError> eval = ValidateInput(asn_type);

                        if (eval) {
                            PrintValidError(eval, args);
                        }
                        num_validated++;
                    }
                    catch (const CException &e) {
                        if (num_validated == 0) {
                            throw(e);
                        }
                        else {
                            break;
                        }
                    }                    
                    double elapsed = sw.Elapsed();
                    if (elapsed > m_Longest) {
                        m_Longest = elapsed;
                        m_LongestId = m_CurrentId;
                    }
                }
            }
        } catch (const CException &e) {
            string errstr = e.GetMsg();
            errstr = NStr::Replace(errstr, "\n", " * ");
            errstr = NStr::Replace(errstr, " *   ", " * ");
            CRef<CValidError> eval(new CValidError());
            if (NStr::StartsWith (errstr, "duplicate Bioseq id", NStr::eNocase)) {
                eval->AddValidErrItem(eDiag_Critical, eErr_GENERIC_DuplicateIDs, errstr);
                PrintValidError(eval, args);
                // ERR_POST(e);
            } else {
                eval->AddValidErrItem(eDiag_Fatal, eErr_INTERNAL_Exception, errstr);
                PrintValidError(eval, args);
                ERR_POST(e);
            }
            ++m_Reported;
        }
    }
    m_NumFiles++;
    if (close_error_stream) {
        DestroyOutputStreams();
    }
    m_In.reset();
}


//LCOV_EXCL_START
//unable to exercise with our test framework
void CAsnvalApp::ValidateOneDirectory(string dir_name, bool recurse)
{
    const CArgs& args = GetArgs();

    CDir dir(dir_name);

    string suffix = ".ent";
    if (args["x"]) {
        suffix = args["x"].AsString();
    }
    string mask = "*" + suffix;

    CDir::TEntries files (dir.GetEntries(mask, CDir::eFile));
    ITERATE(CDir::TEntries, ii, files) {
        string fname = (*ii)->GetName();
        if ((*ii)->IsFile() &&
            (!args["f"] || NStr::Find (fname, args["f"].AsString()) != string::npos)) {
            string fname = CDirEntry::MakePath(dir_name, (*ii)->GetName());
            ValidateOneFile (fname);
        }
    }
    if (recurse) {
        CDir::TEntries subdirs (dir.GetEntries("", CDir::eDir));
        ITERATE(CDir::TEntries, ii, subdirs) {
            string subdir = (*ii)->GetName();
            if ((*ii)->IsDir() && !NStr::Equal(subdir, ".") && !NStr::Equal(subdir, "..")) {
                string subname = CDirEntry::MakePath(dir_name, (*ii)->GetName());
                ValidateOneDirectory (subname, recurse);
            }
        }
    }
}
//LCOV_EXCL_STOP


int CAsnvalApp::Run(void)
{
    const CArgs& args = GetArgs();
    Setup(args);

    time_t start_time = time(NULL);

    if (args["o"]) {
        m_ValidErrorStream = &(args["o"].AsOutputFile());
    }

    if (args["D"]) {
        string lat_lon_path = args["D"].AsString();
        if (! lat_lon_path.empty()) {
            SetEnvironment ( "NCBI_LAT_LON_DATA_PATH", lat_lon_path );
        }
    }

    // note - the C Toolkit uses 0 for SEV_NONE, but the C++ Toolkit uses 0 for SEV_INFO
    // adjust here to make the inputs to asnvalidate match asnval expectations
    m_ReportLevel = static_cast<EDiagSev>(args["R"].AsInteger() - 1);
    m_LowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    m_HighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);

    m_DoCleanup = args["cleanup"] && args["cleanup"].AsBoolean();
    m_verbosity = static_cast<EVerbosity>(args["v"].AsInteger());


    m_Quiet = args["quiet"] && args["quiet"].AsBoolean();

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    m_Reported = 0;

    m_obj_type = args["a"].AsString();

    if (args["b"] && m_obj_type == "a")
    {
        NCBI_THROW(CException, eUnknown, "Specific argument -a must be used along with -b flags" );
    }

    bool execption_caught = false;
    try {
        ConstructOutputStreams();

        if ( args["p"] ) {
            ValidateOneDirectory (args["p"].AsString(), args["u"]);
        } else if (args["i"]) {
            ValidateOneFile (args["i"].AsString());
        } else {
            ValidateOneFile("");
        }
    } catch (CException& e) {
        ERR_POST(Error << e);
        execption_caught = true;
    }
    if (m_NumFiles == 0) {
       ERR_POST("No matching files found");
    }

    time_t stop_time = time(NULL);
    if (! m_Quiet ) {
        LOG_POST_XX(Corelib_App, 1, "Finished in " << stop_time - start_time << " seconds");
        LOG_POST_XX(Corelib_App, 1, "Longest processing time " << m_Longest << " seconds on " << m_LongestId);
        LOG_POST_XX(Corelib_App, 1, "Total number of records " << m_NumRecords);
    }

    DestroyOutputStreams();

    if (m_Reported > 0  ||  execption_caught) {
        return 1;
    } else {
        return 0;
    }
}


CRef<CScope> CAsnvalApp::BuildScope (void)
{
    CRef<CScope> scope(new CScope (*m_ObjMgr));
    scope->AddDefaults();

    return scope;
}


void CAsnvalApp::ReadClassMember
(CObjectIStream& in,
 const CObjectInfo::CMemberIterator& member)
{
    m_Level++;

    if ( m_Level == 1 ) {
        size_t n = 0;
        // Read each element separately to a local TSeqEntry,
        // process it somehow, and... not store it in the container.
        for ( CIStreamContainerIterator i(in, member); i; ++i ) {
            try {
                // Get seq-entry to validate
                CRef<CSeq_entry> se(new CSeq_entry);
                i >> *se;

                // Validate Seq-entry
                CValidator validator(*m_ObjMgr);
                CRef<CScope> scope = BuildScope();
                CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*se);

                CBioseq_CI bi(seh);
                if (bi) {
                    m_CurrentId = "";
                    bi->GetId().front().GetSeqId()->GetLabel(&m_CurrentId);
                    if (! m_Quiet ) {
                        LOG_POST_XX(Corelib_App, 1, m_CurrentId);
                    }
                }

                if (m_DoCleanup) {
                    m_Cleanup.SetScope (scope);
                    m_Cleanup.BasicCleanup (*se);
                }

                if ( m_OnlyAnnots ) {
                    for (CSeq_annot_CI ni(seh); ni; ++ni) {
                        const CSeq_annot_Handle& sah = *ni;
                        CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
                        m_NumRecords++;
                        if ( eval ) {
                            PrintValidError(eval, GetArgs());
                        }
                    }
                } else {
                    // CConstRef<CValidError> eval = validator.Validate(*se, &scope, m_Options);
                    CStopWatch sw(CStopWatch::eStart);
                    CConstRef<CValidError> eval = validator.Validate(seh, m_Options);
                    m_NumRecords++;
                    double elapsed = sw.Elapsed();
                    if (elapsed > m_Longest) {
                        m_Longest = elapsed;
                        m_LongestId = m_CurrentId;
                   }
                    //if (m_ValidErrorStream) {
                    //    *m_ValidErrorStream << "Elapsed = " << sw.Elapsed() << endl;
                    //}
                    if ( eval ) {
                        PrintValidError(eval, GetArgs());
                    }
                }
                scope->RemoveTopLevelSeqEntry(seh);
                scope->ResetHistory();
                n++;
            } catch (exception&) {
                if ( !m_Continue ) {
                    throw;
                }
                // should we issue some sort of warning?
            }
        }
    } else {
        in.ReadClassMember(member);
    }

    m_Level--;
}


void CAsnvalApp::ProcessBSSReleaseFile
(const CArgs& args)
{
    CRef<CBioseq_set> seqset(new CBioseq_set);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CBioseq_set>();
    set_type.FindMember("seq-set").SetLocalReadHook(*m_In, this);

    // Read the CBioseq_set, it will call the hook object each time we 
    // encounter a Seq-entry
    try {
        *m_In >> *seqset;
    }
    catch (const CException&) {
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is not a batch Bioseq-set, do not use -a t to process.");
        ++m_Reported;
    }
}

void CAsnvalApp::ProcessSSMReleaseFile
(const CArgs& args)
{
    CRef<CSeq_submit> seqset(new CSeq_submit);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CSeq_submit>();
    (*(*(*set_type.FindMember("data")).GetPointedType().FindVariant("entrys")).GetElementType().GetPointedType().FindVariant("set")).FindMember("seq-set").SetLocalReadHook(*m_In, this);

    // Read the CSeq_submit, it will call the hook object each time we 
    // encounter a Seq-entry
    try {
        *m_In >> *seqset;
    }
    catch (const CException&) {
        LOG_POST_XX(Corelib_App, 1, "FAILURE: Record is not a batch Seq-submit, do not use -a u to process.");
        ++m_Reported;
    }
}

CRef<CValidError> CAsnvalApp::ReportReadFailure(const CException *p_exception)
{
    CRef<CValidError> errors(new CValidError());
    
    CNcbiOstrstream os;
    os << "Unable to read invalid ASN.1";
    
    const CSerialException *p_serial_exception = dynamic_cast<const CSerialException*>(p_exception);
    if( p_serial_exception ) {
        if( m_In.get() ) {
            os << ": " << m_In->GetPosition();
        }
        if( p_serial_exception->GetErrCode() == CSerialException::eEOF ) {
            os << ": unexpected end of file";
        } else {
            // manually call ReportAll(0) because what() includes a lot of info
            // that's not of interest to the submitter such as stacktraces and
            // GetMsg() doesn't include enough info.
            os << ": " + p_exception->ReportAll(0);
        }
    }
    
    string errstr = CNcbiOstrstreamToString(os);
    // newlines don't play well with XML
    errstr = NStr::Replace(errstr, "\n", " * ");
    errstr = NStr::Replace(errstr, " *   ", " * ");

    errors->AddValidErrItem(eDiag_Critical, eErr_GENERIC_InvalidAsn, errstr);
    return errors;
}


CConstRef<CValidError> CAsnvalApp::ProcessCatenated(void)
{
    try {
        while (true) {
            // Get seq-entry to validate
            CRef<CSeq_entry> se(new CSeq_entry);

            try {
                m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);
            }
            catch (const CSerialException& e) {
                if (e.GetErrCode() == CSerialException::eEOF) {
                    break;
                } else {
                    throw;
                }
            }
            catch (const CException& e) {
                ERR_POST(Error << e);
                return ReportReadFailure(&e);
            }
            try {
                CConstRef<CValidError> eval = ProcessSeqEntry(*se);
                if ( eval ) {
                    PrintValidError(eval, GetArgs());
                }
            }
            catch (const CObjMgrException& om_ex) {
                if (om_ex.GetErrCode() == CObjMgrException::eAddDataError)
                  se->ReassignConflictingIds();
                CConstRef<CValidError> eval = ProcessSeqEntry(*se);
                if ( eval ) {
                    PrintValidError(eval, GetArgs());
                }
            }
            try {
                m_In->SkipFileHeader(CSeq_entry::GetTypeInfo());
            }
            catch (const CEofException&) {
                break;
            }
        }
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    return CConstRef<CValidError>();
}

CConstRef<CValidError> CAsnvalApp::ProcessBioseq(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(new CSeq_entry);
    CBioseq& bioseq = se->SetSeq();

    try {
        m_In->Read(ObjectInfo(bioseq), CObjectIStream::eNoFileHeader);
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}

CConstRef<CValidError> CAsnvalApp::ProcessBioseqset(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(new CSeq_entry);
    CBioseq_set& bioseqset = se->SetSet();

    try {
        m_In->Read(ObjectInfo(bioseqset), CObjectIStream::eNoFileHeader);
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    // Validate Seq-entry
    return ProcessSeqEntry(*se);
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqEntry(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(new CSeq_entry);

    try {
        m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);    
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    try
    {
        return ProcessSeqEntry(*se);
    }
    catch (const CObjMgrException& om_ex)
    {        
        if (om_ex.GetErrCode() == CObjMgrException::eAddDataError)
          se->ReassignConflictingIds();
    }
    // try again
    return ProcessSeqEntry(*se);
}

CConstRef<CValidError> CAsnvalApp::ProcessSeqEntry(CSeq_entry& se)
{
    // Validate Seq-entry
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {        
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (se);
    }
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(se);
    CBioseq_CI bi(seh);
    if (bi) {
        m_CurrentId = "";
        bi->GetId().front().GetSeqId()->GetLabel(&m_CurrentId);
        if (! m_Quiet ) {
            LOG_POST_XX(Corelib_App, 1, m_CurrentId);
        }
    }

    if ( m_OnlyAnnots ) {
        for (CSeq_annot_CI ni(seh); ni; ++ni) {
            const CSeq_annot_Handle& sah = *ni;
            CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
            m_NumRecords++;
            if ( eval ) {
                PrintValidError(eval, GetArgs());
            }
        }
        return CConstRef<CValidError>();
    }
    CConstRef<CValidError> eval = validator.Validate(se, scope, m_Options);
    m_NumRecords++;
    return eval;
    }


CRef<CSeq_feat> CAsnvalApp::ReadSeqFeat(void)
{
    CRef<CSeq_feat> feat(new CSeq_feat);

    try {
        m_In->Read(ObjectInfo(*feat), CObjectIStream::eNoFileHeader);
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
    }

    return feat;
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqFeat(void)
{
    CRef<CSeq_feat> feat(ReadSeqFeat());

    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*feat);
    }

    CValidator validator(*m_ObjMgr);
    CConstRef<CValidError> eval = validator.Validate(*feat, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CRef<CBioSource> CAsnvalApp::ReadBioSource(void)
{
    CRef<CBioSource> src(new CBioSource);

    try {
        m_In->Read(ObjectInfo(*src), CObjectIStream::eNoFileHeader);
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
    }

    return src;
}


CConstRef<CValidError> CAsnvalApp::ProcessBioSource(void)
{
    CRef<CBioSource> src(ReadBioSource());

    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    CConstRef<CValidError> eval = validator.Validate(*src, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CRef<CPubdesc> CAsnvalApp::ReadPubdesc(void)
{
    CRef<CPubdesc> pd(new CPubdesc());

    try {
        m_In->Read(ObjectInfo(*pd), CObjectIStream::eNoFileHeader);
    }
    catch (const CException& e) {
        ERR_POST(Error << e);
    }

    return pd;
}


CConstRef<CValidError> CAsnvalApp::ProcessPubdesc(void)
{
    CRef<CPubdesc> pd(ReadPubdesc());

    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    CConstRef<CValidError> eval = validator.Validate(*pd, scope, m_Options);
    m_NumRecords++;
    return eval;
}



CConstRef<CValidError> CAsnvalApp::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to validate
    try {
        m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    // Validate Seq-submit
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, se, ss->SetData().SetEntrys()) {
            scope->AddTopLevelSeqEntry(**se);
        }
    }
    if (m_DoCleanup) {
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*ss);
    }

    CConstRef<CValidError> eval = validator.Validate(*ss, scope, m_Options);
    m_NumRecords++;
    return eval;
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqAnnot(void)
{
    CRef<CSeq_annot> sa(new CSeq_annot);

    // Get seq-annot to validate
    try {
        m_In->Read(ObjectInfo(*sa), CObjectIStream::eNoFileHeader);
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    // Validate Seq-annot
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*sa);
    }
    CSeq_annot_Handle sah = scope->AddSeq_annot(*sa);
    CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
    m_NumRecords++;
    return eval;
}

static CRef<objects::CSeq_entry> s_BuildGoodSeq(void)
{
    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry());
    entry->SetSeq().SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    entry->SetSeq().SetInst().SetLength(60);

    CRef<objects::CSeq_id> id(new objects::CSeq_id());
    id->SetLocal().SetStr ("good");
    entry->SetSeq().SetId().push_back(id);

    CRef<objects::CSeqdesc> mdesc(new objects::CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(objects::CMolInfo::eBiomol_genomic);    
    entry->SetSeq().SetDescr().Set().push_back(mdesc);

    /*
    AddGoodSource (entry);
    AddGoodPub(entry);
    */

    return entry;
}

CConstRef<CValidError> CAsnvalApp::ProcessSeqDesc(void)
{
    CRef<CSeqdesc> sd(new CSeqdesc);

    try {
        m_In->Read(ObjectInfo(*sd), CObjectIStream::eNoFileHeader);
    }
    catch (CException& e) {
        ERR_POST(Error << e);
        return ReportReadFailure(&e);
    }

    CRef<CSeq_entry> ctx = s_BuildGoodSeq();

    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*ctx);

    CConstRef<CValidError> eval = validator.Validate(*sd, *ctx);
    m_NumRecords++;
    return eval;
}


void CAsnvalApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    // CORE_SetLOG(LOG_cxx2c());
    // CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    m_ObjMgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *m_ObjMgr,
                                         CDataLoadersUtil::fDefault |
                                         CDataLoadersUtil::fGenbankOffByDefault);

    m_OnlyAnnots = args["annot"];

    // Set validator options
    m_Options = CValidatorArgUtil::ArgsToValidatorOptions(args);


}


unique_ptr<CObjectIStream> CAsnvalApp::OpenFile(const string& fname, string& asn_type)
{
    ENcbiOwnership own = eNoOwnership;
    unique_ptr<CNcbiIstream> hold_stream;
    CNcbiIstream* InputStream = &NcbiCin;

    if (!fname.empty()) {
        own = eTakeOwnership;
        hold_stream = make_unique<CNcbiIfstream>(fname, ios::binary);
        InputStream = hold_stream.get();
    }

    CCompressStream::EMethod method;

    CFormatGuessEx FG(*InputStream);
    CFileContentInfo contentInfo;
    CFormatGuess::EFormat format = FG.GuessFormatAndContent(contentInfo);
    switch (format)
    {
        case CFormatGuess::eGZip:  method = CCompressStream::eGZipFile;  break;
        case CFormatGuess::eBZip2: method = CCompressStream::eBZip2;     break;
        case CFormatGuess::eLzo:   method = CCompressStream::eLZO;       break;
        default:                   method = CCompressStream::eNone;      break;
    }
    if (method != CCompressStream::eNone)
    {
        CDecompressIStream* decompress(new CDecompressIStream(*InputStream, method, CCompressStream::fDefault, own));
        hold_stream.release();
        hold_stream.reset(decompress);
        InputStream = hold_stream.get();
        own = eTakeOwnership;
        CFormatGuessEx fg(*InputStream);
        format = fg.GuessFormatAndContent(contentInfo);
    }

    unique_ptr<CObjectIStream> objectStream;
    switch (format)
    {
        case CFormatGuess::eBinaryASN:
        case CFormatGuess::eTextASN:
            asn_type = contentInfo.mInfoGenbank.mObjectType;
            objectStream.reset(CObjectIStream::Open(format==CFormatGuess::eBinaryASN ? eSerial_AsnBinary : eSerial_AsnText, *InputStream, own));
            hold_stream.release();
            break;
        default:
            break;
    }
    return objectStream;
}


void CAsnvalApp::PrintValidError
(CConstRef<CValidError> errors, 
 const CArgs& args)
{
    if ( errors->TotalSize() == 0 ) {
        return;
    }

    for ( CValidError_CI vit(*errors); vit; ++vit) {
        if (vit->GetSeverity() >= m_ReportLevel) {
            ++m_Reported;
        }
        if ( vit->GetSeverity() < m_LowCutoff || vit->GetSeverity() > m_HighCutoff) {
            continue;
        }
        if (args["E"] && !(NStr::EqualNocase(args["E"].AsString(), vit->GetErrCode()))) {
            continue;
        }
        PrintValidErrItem(*vit);
    }
    m_ValidErrorStream->flush();
}


static string s_GetSeverityLabel(EDiagSev sev, bool is_xml = false)
{
    static const string str_sev[] = {
        "NOTE", "WARNING", "ERROR", "REJECT", "FATAL", "MAX"
    };
    if (sev < 0 || sev > eDiagSevMax) {
        return "NONE";
    }
    if (sev == 0 && is_xml) {
        return "INFO";
    }

    return str_sev[sev];
}


void CAsnvalApp::PrintValidErrItem(const CValidErrItem& item)
{
    CNcbiOstream& os = *m_ValidErrorStream;
    switch (m_verbosity) {
    case eVerbosity_Normal:
        os << s_GetSeverityLabel(item.GetSeverity())
            << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() << "] "
            << item.GetMsg();
        if (item.IsSetObjDesc()) {
            os << " " << item.GetObjDesc();
        }
        os << endl;
        break;
    case eVerbosity_Spaced:
    {
        string spacer = "                    ";
        string msg = item.GetAccnver() + spacer;
        msg = msg.substr(0, 15);
        msg += s_GetSeverityLabel(item.GetSeverity());
        msg += spacer;
        msg = msg.substr(0, 30);
        msg += item.GetErrGroup() + "_" + item.GetErrCode();
        os << msg << endl;
    }
    break;
    case eVerbosity_Tabbed:
        os << item.GetAccnver() << "\t"
            << s_GetSeverityLabel(item.GetSeverity()) << "\t"
            << item.GetErrGroup() << "_" << item.GetErrCode() << endl;
        break;
#ifdef USE_XMLWRAPP_LIBS
    case eVerbosity_XML:
    {
        m_ostr_xml->Print(item);
    }
#else
    case eVerbosity_XML:
    {
        string msg = NStr::XmlEncode(item.GetMsg());
        if (item.IsSetFeatureId()) {
            os << "  <message severity=\"" << s_GetSeverityLabel(item.GetSeverity())
                << "\" seq-id=\"" << item.GetAccnver() 
                << "\" feat-id=\"" << item.GetFeatureId()
                << "\" code=\"" << item.GetErrGroup() << "_" << item.GetErrCode()
                << "\">" << msg << "</message>" << endl;
        } else {
            os << "  <message severity=\"" << s_GetSeverityLabel(item.GetSeverity())
                << "\" seq-id=\"" << item.GetAccnver() 
                << "\" code=\"" << item.GetErrGroup() << "_" << item.GetErrCode()
                << "\">" << msg << "</message>" << endl;
        }
    }
#endif
    break;
    }
}

void CValXMLStream::Print(const CValidErrItem& item)
{
#if 0
    TTypeInfo info = item.GetThisTypeInfo();    
    WriteObject(&item, info);
#else
    m_Output.PutString("  <message severity=\"");
    m_Output.PutString(s_GetSeverityLabel(item.GetSeverity(), true));
    if (item.IsSetAccnver()) {
        m_Output.PutString("\" seq-id=\"");
        WriteString(item.GetAccnver(), eStringTypeVisible);
    }

    if (item.IsSetFeatureId()) {
        m_Output.PutString("\" feat-id=\"");
        WriteString(item.GetFeatureId(), eStringTypeVisible);
    }

    if (item.IsSetLocation()) {
        m_Output.PutString("\" interval=\"");
        string loc = item.GetLocation();
        if (NStr::StartsWith(loc, "(") && NStr::EndsWith(loc, ")")) {
            loc = "[" + loc.substr(1, loc.length() - 2) + "]";
        }
        WriteString(loc, eStringTypeVisible);
    }

    m_Output.PutString("\" code=\"");
    WriteString(item.GetErrGroup(), eStringTypeVisible);
    m_Output.PutString("_");
    WriteString(item.GetErrCode(), eStringTypeVisible);
    m_Output.PutString("\">");

    WriteString(item.GetMsg(), eStringTypeVisible);

    m_Output.PutString("</message>");
    m_Output.PutEol();
#endif
}

void CAsnvalApp::ConstructOutputStreams()
{
    if (m_ValidErrorStream && m_verbosity == eVerbosity_XML)
    {
#ifdef USE_XMLWRAPP_LIBS
        m_ostr_xml.reset(new CValXMLStream(*m_ValidErrorStream, eNoOwnership));
        m_ostr_xml->SetEncoding(eEncoding_UTF8);
        m_ostr_xml->SetReferenceDTD(false);
        m_ostr_xml->SetEnforcedStdXml(true);
        m_ostr_xml->WriteFileHeader(CValidErrItem::GetTypeInfo());
        m_ostr_xml->SetUseIndentation(true);
        m_ostr_xml->Flush();

        *m_ValidErrorStream << endl << "<asnvalidate version=\"" << "3." << NCBI_SC_VERSION_PROXY << "."
        << NCBI_TEAMCITY_BUILD_NUMBER_PROXY << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << endl;
        m_ValidErrorStream->flush();
#else
        *m_ValidErrorStream << "<asnvalidate version=\"" << "3." << NCBI_SC_VERSION_PROXY << "."
        << NCBI_TEAMCITY_BUILD_NUMBER_PROXY << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << endl;
#endif
    }
}

void CAsnvalApp::DestroyOutputStreams()
{
#ifdef USE_XMLWRAPP_LIBS
    if (m_ostr_xml.get())
    {
        m_ostr_xml.reset();
        *m_ValidErrorStream << "</asnvalidate>" << endl;
    }
#endif
    m_ValidErrorStream = 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CAsnvalApp().AppMain(argc, argv);
}

