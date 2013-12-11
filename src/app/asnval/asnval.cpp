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
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectio.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Pubdesc.hpp>
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
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <common/test_assert.h>  /* This header must go last */


using namespace ncbi;
using namespace objects;
using namespace validator;

const char * ASNVAL_APP_VER = "10.0";

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CAsnvalApp : public CNcbiApplication, CReadClassMemberHook
{
public:
    CAsnvalApp(void);

    virtual void Init(void);
    virtual int  Run (void);

    void ReadClassMember(CObjectIStream& in,
        const CObjectInfo::CMemberIterator& member);

private:

    void Setup(const CArgs& args);

    CObjectIStream* OpenFile(const CArgs& args);
    CObjectIStream* OpenFile(string fname, const CArgs& args);

    CConstRef<CValidError> ProcessSeqEntry(void);
    CConstRef<CValidError> ProcessSeqSubmit(void);
    CConstRef<CValidError> ProcessSeqAnnot(void);
    CConstRef<CValidError> ProcessSeqFeat(void);
    CConstRef<CValidError> ProcessBioSource(void);
    CConstRef<CValidError> ProcessPubdesc(void);
    CConstRef<CValidError> ValidateInput (void);
    void ValidateOneDirectory(string dir_name, bool recurse);
    void ValidateOneFile(string fname);
    void ProcessReleaseFile(const CArgs& args);
    CRef<CSeq_entry> ReadSeqEntry(void);
    CRef<CSeq_feat> ReadSeqFeat(void);
    CRef<CBioSource> ReadBioSource(void);
    CRef<CPubdesc> ReadPubdesc(void);

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

    void PrintValidErrItem(const CValidErrItem& item, CNcbiOstream& os, EVerbosity verbosity);

    CRef<CObjectManager> m_ObjMgr;
    auto_ptr<CObjectIStream> m_In;
    unsigned int m_Options;
    bool m_Continue;
    bool m_OnlyAnnots;

    size_t m_Level;
    size_t m_Reported;
    size_t m_ReportLevel;

    bool m_StartXML;

    bool m_DoCleanup;
    CCleanup m_Cleanup;

    EDiagSev m_LowCutoff;
    EDiagSev m_HighCutoff;

    CNcbiOstream* m_ValidErrorStream;
    CNcbiOstream* m_LogStream;
};


CAsnvalApp::CAsnvalApp(void) :
    m_ObjMgr(0), m_In(0), m_Options(0), m_Continue(false), m_OnlyAnnots(false),
    m_Level(0), m_Reported(0), m_ValidErrorStream(0), m_LogStream(0)
{
}


void CAsnvalApp::Init(void)
{
    // Prepare command line descriptions

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

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
        "R", "SevCount", "Severity for Error in Return Code",
        CArgDescriptions::eInteger, "4");
    arg_desc->AddDefaultKey(
        "Q", "SevLevel", "Lowest Severity for Error to Show",
        CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey(
        "P", "SevLevel", "Highest Severity for Error to Show",
        CArgDescriptions::eInteger, "4");
    CArgAllow* constraint = new CArgAllow_Integers(eDiagSevMin, eDiagSevMax);
    arg_desc->SetConstraint("Q", constraint);
    arg_desc->SetConstraint("P", constraint);
    arg_desc->SetConstraint("R", constraint);
    arg_desc->AddOptionalKey(
        "E", "String", "Only Error Code to Show",
        CArgDescriptions::eString);

    arg_desc->AddDefaultKey("a", "a", 
                            "ASN.1 Type (a Automatic, z Any, e Seq-entry, b Bioseq, s Bioseq-set, m Seq-submit, t Batch Bioseq-set, u Batch Seq-submit",
                            CArgDescriptions::eString,
                            "a");

    arg_desc->AddFlag("b", "Input is in binary format");
    arg_desc->AddFlag("c", "Batch File is Compressed");

    CValidatorArgUtil::SetupArgDescriptions(arg_desc.get());
    arg_desc->AddFlag("annot", "Verify Seq-annots only");

    arg_desc->AddOptionalKey(
        "L", "OutFile", "Log File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("v", "Verbosity", "Verbosity", CArgDescriptions::eInteger, "1");
    CArgAllow* v_constraint = new CArgAllow_Integers(eVerbosity_min, eVerbosity_max);
    arg_desc->SetConstraint("v", v_constraint);

    arg_desc->AddFlag("cleanup", "Perform BasicCleanup before validating (to match C Toolkit)");

    // Program description
    string prog_description = "ASN Validator\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());

}


CConstRef<CValidError> CAsnvalApp::ValidateInput (void)
{
    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    CConstRef<CValidError> eval;
    string header = m_In->ReadFileHeader();

    if (header == "Seq-submit" ) {  // Seq-submit
        eval = ProcessSeqSubmit();
    } else if ( header == "Seq-entry" ) {           // Seq-entry
        eval = ProcessSeqEntry();
    } else if ( header == "Seq-annot" ) {           // Seq-annot
        eval = ProcessSeqAnnot();
    } else if (header == "Seq-feat" ) {             // Seq-feat
        eval = ProcessSeqFeat();
    } else if (header == "BioSource" ) {             // BioSource
        eval = ProcessBioSource();
    } else if (header == "Pubdesc" ) {             // Pubdesc
        eval = ProcessPubdesc();
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled type " + header);
    }
    return eval;
}


void CAsnvalApp::ValidateOneFile(string fname)
{
    const CArgs& args = GetArgs();

    bool need_to_close = false;

    if (!m_ValidErrorStream) {
        string path = fname;
        size_t pos = NStr::Find(path, ".", 0, string::npos, NStr::eLast);
        if (pos != string::npos) {
            path = path.substr(0, pos);
        }
        path = path + ".val";

        m_ValidErrorStream = new CNcbiOfstream(path.c_str());
        need_to_close = true;
    }
    m_In.reset(OpenFile(fname, args));
    if ( NStr::Equal(args["a"].AsString(), "t")) {          // Release file
        // Open File 
        ProcessReleaseFile(args);
    } else {

        CConstRef<CValidError> eval = ValidateInput ();

        if ( eval ) {
            PrintValidError(eval, args);
        }

    }
    if (need_to_close) {
        if (m_StartXML) {
            // close XML
            *m_ValidErrorStream << "</asnvalidate>" << endl;
            m_StartXML = false;
        }
        m_ValidErrorStream = 0;
    }
}


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


int CAsnvalApp::Run(void)
{
    const CArgs& args = GetArgs();
    Setup(args);

    if (args["o"]) {
        m_ValidErrorStream = &(args["o"].AsOutputFile());
    }
            
    m_LogStream = args["L"] ? &(args["L"].AsOutputFile()) : &NcbiCout;

    // note - the C Toolkit uses 0 for SEV_NONE, but the C++ Toolkit uses 0 for SEV_INFO
    // adjust here to make the inputs to asnvalidate match asnval expectations
    m_ReportLevel = args["R"].AsInteger() - 1;
    m_LowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    m_HighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);

    m_DoCleanup = args["cleanup"] && args["cleanup"].AsBoolean();

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    m_Reported = 0;
    m_StartXML = false;

    if ( args["p"] ) {
        ValidateOneDirectory (args["p"].AsString(), args["u"]);
    } else {
        if (args["i"]) {
            ValidateOneFile (args["i"].AsString());
        }
    }

    if (m_StartXML) {
        // close XML
        *m_ValidErrorStream << "</asnvalidate>" << endl;
        m_StartXML = false;
    }

    if (m_Reported > 0) {
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

                if (m_DoCleanup) {
                    m_Cleanup.SetScope (scope);
                    m_Cleanup.BasicCleanup (*se);
                }

                if ( m_OnlyAnnots ) {
                    for (CSeq_annot_CI ni(seh); ni; ++ni) {
                        const CSeq_annot_Handle& sah = *ni;
                        CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
                        if ( eval ) {
                            PrintValidError(eval, GetArgs());
                        }
                    }
                } else {
                    // CConstRef<CValidError> eval = validator.Validate(*se, &scope, m_Options);
                    CStopWatch sw(CStopWatch::eStart);
                    CConstRef<CValidError> eval = validator.Validate(seh, m_Options);
                    if (m_ValidErrorStream) {
                        *m_ValidErrorStream << "Elapsed = " << sw.Elapsed() << endl;
                    }
                    if ( eval ) {
                        PrintValidError(eval, GetArgs());
                    }
                }
                n++;
            } catch (exception e) {
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


void CAsnvalApp::ProcessReleaseFile
(const CArgs& args)
{
    CRef<CBioseq_set> seqset(new CBioseq_set);

    // Register the Seq-entry hook
    CObjectTypeInfo set_type = CType<CBioseq_set>();
    set_type.FindMember("seq-set").SetLocalReadHook(*m_In, this);

    // Read the CBioseq_set, it will call the hook object each time we 
    // encounter a Seq-entry
    *m_In >> *seqset;
}


CRef<CSeq_entry> CAsnvalApp::ReadSeqEntry(void)
{
    CRef<CSeq_entry> se(new CSeq_entry);
    m_In->Read(ObjectInfo(*se), CObjectIStream::eNoFileHeader);

    return se;
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqEntry(void)
{
    // Get seq-entry to validate
    CRef<CSeq_entry> se(ReadSeqEntry());

    // Validate Seq-entry
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {        
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*se);
    }
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*se);

    if ( m_OnlyAnnots ) {
        for (CSeq_annot_CI ni(seh); ni; ++ni) {
            const CSeq_annot_Handle& sah = *ni;
            CConstRef<CValidError> eval = validator.Validate(sah, m_Options);
            if ( eval ) {
                PrintValidError(eval, GetArgs());
            }
        }
        return CConstRef<CValidError>();
    }
    return validator.Validate(*se, scope, m_Options);
}


CRef<CSeq_feat> CAsnvalApp::ReadSeqFeat(void)
{
    CRef<CSeq_feat> feat(new CSeq_feat);
    m_In->Read(ObjectInfo(*feat), CObjectIStream::eNoFileHeader);

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
    return validator.Validate(*feat, scope, m_Options);
}


CRef<CBioSource> CAsnvalApp::ReadBioSource(void)
{
    CRef<CBioSource> src(new CBioSource);
    m_In->Read(ObjectInfo(*src), CObjectIStream::eNoFileHeader);

    return src;
}


CConstRef<CValidError> CAsnvalApp::ProcessBioSource(void)
{
    CRef<CBioSource> src(ReadBioSource());

    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    return validator.Validate(*src, scope, m_Options);
}


CRef<CPubdesc> CAsnvalApp::ReadPubdesc(void)
{
    CRef<CPubdesc> pd(new CPubdesc());
    m_In->Read(ObjectInfo(*pd), CObjectIStream::eNoFileHeader);

    return pd;
}


CConstRef<CValidError> CAsnvalApp::ProcessPubdesc(void)
{
    CRef<CPubdesc> pd(ReadPubdesc());

    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    return validator.Validate(*pd, scope, m_Options);
}



CConstRef<CValidError> CAsnvalApp::ProcessSeqSubmit(void)
{
    CRef<CSeq_submit> ss(new CSeq_submit);

    // Get seq-submit to validate
    m_In->Read(ObjectInfo(*ss), CObjectIStream::eNoFileHeader);

    // Validae Seq-submit
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (ss->GetData().IsEntrys()) {
        ITERATE(CSeq_submit::TData::TEntrys, se, ss->GetData().GetEntrys()) {
            scope->AddTopLevelSeqEntry(**se);
        }
    }
    if (m_DoCleanup) {
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*ss);
    }

    return validator.Validate(*ss, scope, m_Options);
}


CConstRef<CValidError> CAsnvalApp::ProcessSeqAnnot(void)
{
    CRef<CSeq_annot> sa(new CSeq_annot);

    // Get seq-annot to validate
    m_In->Read(ObjectInfo(*sa), CObjectIStream::eNoFileHeader);

    // Validae Seq-annot
    CValidator validator(*m_ObjMgr);
    CRef<CScope> scope = BuildScope();
    if (m_DoCleanup) {
        m_Cleanup.SetScope (scope);
        m_Cleanup.BasicCleanup (*sa);
    }
    CSeq_annot_Handle sah = scope->AddSeq_annot(*sa);
    return validator.Validate(sah, m_Options);
}


void CAsnvalApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    m_ObjMgr = CObjectManager::GetInstance();
    if ( args["r"] ) {
        // Create GenBank data loader and register it with the OM.
        // The last argument "eDefault" informs the OM that the loader must
        // be included in scopes during the CScope::AddDefaults() call.
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
    }

    m_OnlyAnnots = args["annot"];

    // Set validator options
    m_Options = CValidatorArgUtil::ArgsToValidatorOptions(args);
}


CObjectIStream* CAsnvalApp::OpenFile
(const CArgs& args)
{
    // file name
    string fname = args["i"].AsString();

    // file format 
    ESerialDataFormat format = eSerial_AsnText;
    if ( args["b"] ) {
        format = eSerial_AsnBinary;
    }

    return CObjectIStream::Open(fname, format);
}


CObjectIStream* CAsnvalApp::OpenFile
(string fname, const CArgs& args)
{
    // file format 
    ESerialDataFormat format = eSerial_AsnText;
    if ( args["b"] ) {
        format = eSerial_AsnBinary;
    }

    return CObjectIStream::Open(fname, format);
}

void CAsnvalApp::PrintValidError
(CConstRef<CValidError> errors, 
 const CArgs& args)
{
    EVerbosity verbosity = static_cast<EVerbosity>(args["v"].AsInteger());

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
        PrintValidErrItem(*vit, *m_ValidErrorStream, verbosity);
    }
    m_ValidErrorStream->flush();
}


static string s_GetSeverityLabel (EDiagSev sev)
{
    static const string str_sev[] = {
        "INFO", "WARNING", "ERROR", "REJECT", "FATAL", "MAX"
    };
    if (sev < 0 || sev > eDiagSevMax) {
        return "NONE";
    }

    return str_sev[sev];
}


void CAsnvalApp::PrintValidErrItem
(const CValidErrItem& item,
 CNcbiOstream& os,
 EVerbosity verbosity)
{
    switch (verbosity) {
        case eVerbosity_Normal:
            os << s_GetSeverityLabel(item.GetSeverity()) 
               << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() <<"] "
               << item.GetMsg() << " " << item.GetObjDesc() << endl;
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
        case eVerbosity_XML:
            {
                string msg = NStr::XmlEncode(item.GetMsg());
                if (!m_StartXML) {
                    os << "<asnvalidate version=\"" << ASNVAL_APP_VER << "\" severity_cutoff=\""
                    << s_GetSeverityLabel(m_LowCutoff) << "\">" << endl;
                    m_StartXML = true;
                }
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
            break;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CAsnvalApp().AppMain(argc, argv);
}
