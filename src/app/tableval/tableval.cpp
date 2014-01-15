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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*   Validates tab-delimited files agains ASN.1 datatypes, main application function
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_mask.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <util/line_reader.hpp>

#include <objtools/readers/message_listener.hpp>
#include "tab_table_reader.hpp"

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;
using namespace objects;

const char * TABLEVAL_APP_VER = "10.0";

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CTAbleValApp : public CNcbiApplication
{
public:
    CTAbleValApp(void);

    virtual void Init(void);
    virtual int  Run (void);

private:

    void Setup(const CArgs& args);

    void ProcessOneFile(CNcbiIstream& input, CNcbiOstream* output);
    void ProcessOneFile();
    bool ProcessOneDirectory(const CDir& directory, const CMask& mask, bool recurse);

    CRef<CMessageListenerBase> m_logger;

    //EDiagSev m_LowCutoff;
    //EDiagSev m_HighCutoff;

    //CNcbiOstream* m_OutputStream;
    CNcbiOstream* m_LogStream;

    string m_ResultsDirectory;
    string m_current_file;

    string m_columns_def;
    string m_required_cols;
    string m_ignored_cols;
    string m_format;

    CNcbiOstream* m_output;
    bool   m_comma_separated;
    bool   m_no_header;
    bool   m_skip_empty;
};

CTAbleValApp::CTAbleValApp(void):
    m_LogStream(0), m_output(0),
    m_comma_separated(false),
    m_no_header(false),
    m_skip_empty(false)
{
}


void CTAbleValApp::Init(void)
{

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Prepare command line descriptions, inherit them from tbl2asn legacy application

    arg_desc->AddOptionalKey
        ("p", "Directory", "Path to input files",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey
        ("r", "Directory", "Path to results",
        CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey
        ("i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey(
        "o", "OutFile", "Single Output File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey
        ("x", "String", "Suffix", CArgDescriptions::eString, ".tab");

    arg_desc->AddFlag("E", "Recurse");

    arg_desc->AddFlag("no-header", "Start from the first row");
    arg_desc->AddFlag("skip-empty", "Ignore all empty rows");
    arg_desc->AddDefaultKey("format", "String", "Output type: tab, xml, text, html", CArgDescriptions::eString, "tab");

    arg_desc->AddFlag("comma", "Use comma separator instead of tabs");

    arg_desc->AddOptionalKey("columns", "String", "Comma separated columns definitions", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("required", "String", "Comma separated required columns, use indices or names", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("ignore", "String", "Comma separated columns to be ignored, use indices or names", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("logfile", "LogFile", "Error Log File", CArgDescriptions::eOutputFile);    // done

    // Program description
    string prog_description = "Validates tab delimited files against ASN.1 data types\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}


int CTAbleValApp::Run(void)
{
    const CArgs& args = GetArgs();

    Setup(args);

    m_LogStream = args["logfile"] ? &(args["logfile"].AsOutputFile()) : &NcbiCout;
    m_logger.Reset(new CMessageListenerLenient());
    m_logger->SetProgressOstream(m_LogStream);

    // note - the C Toolkit uses 0 for SEV_NONE, but the C++ Toolkit uses 0 for SEV_INFO
    // adjust here to make the inputs to table2asn match tbl2asn expectations
    //m_ReportLevel = args["R"].AsInteger() - 1;
    //m_LowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    //m_HighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);

    m_comma_separated = args["comma"];
    if (args["columns"])
    {
        m_columns_def = args["columns"].AsString();
        NStr::ToLower(m_columns_def);
    }
    if (args["required"])
    {
        m_required_cols = args["required"].AsString();
        NStr::ToLower(m_required_cols);
    }

    if (args["ignore"])
    {
        m_ignored_cols = args["ignore"].AsString();
        NStr::ToLower(m_ignored_cols);
    }
       

    m_no_header = args["no-header"];
    m_skip_empty = args["skip-empty"];
    m_format = args["format"].AsString();

    // Designate where do we output files: local folder, specified folder or a specific single output file
    if (args["o"])
    {
        m_output = &args["o"].AsOutputFile();
    }
    else
    {
        if (args["r"])
        {
            m_ResultsDirectory = args["r"].AsString();
        }
        else
        {
            m_ResultsDirectory = ".";
        }
        m_ResultsDirectory = CDir::AddTrailingPathSeparator(m_ResultsDirectory);

        CDir outputdir(m_ResultsDirectory);
        if (!IsDryRun())
        if (!outputdir.Exists())
            outputdir.Create();
    }

    try
    {
    // Designate where do we get input: single file or a folder or folder structure
        if ( args["p"] )
        {
            CDir directory(args["p"].AsString());
            if (directory.Exists())
            {
                CMaskFileName masks;
                masks.Add("*" +args["x"].AsString());

                ProcessOneDirectory (directory, masks, args["E"].AsBoolean());
            }
        } else {
            if (args["i"])
            {
                m_current_file = args["i"].AsString();
                ProcessOneFile ();
            }
        }
    }
    catch (CException& e)
    {
        m_logger->PutError(*CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error, 
            "", 0, "", "", "",
            e.GetMsg()));
    }

    m_logger->Dump(*m_LogStream);
    if (m_logger->Count() == 0)
        return 0;
    else
    {
        int errors = m_logger->LevelCount(eDiag_Critical) + 
                     m_logger->LevelCount(eDiag_Error) + 
                     m_logger->LevelCount(eDiag_Fatal);
        // all errors reported as failure
        if (errors > 0)
            return 1;
        
        // only warnings reported as 2 
        if (m_logger->LevelCount(eDiag_Warning)>0)
            return 2;

        // otherwise it's ok
        return 0;
    }
}
void CTAbleValApp::ProcessOneFile(CNcbiIstream& input, CNcbiOstream* output)
{
   int flags = 
       (m_comma_separated ? CTabDelimitedValidator::e_tab_comma_delim : CTabDelimitedValidator::e_tab_tab_delim) |
       (m_no_header       ? CTabDelimitedValidator::e_tab_noheader : 0) |
       (m_skip_empty      ? CTabDelimitedValidator::e_tab_ignore_empty_rows : 0);

   if (m_format == "xml")
       flags |= CTabDelimitedValidator::e_tab_xml_report;
   else
   if (m_format == "tab")
       flags |= CTabDelimitedValidator::e_tab_tab_report;
   else
   if (m_format == "html")
       flags |= CTabDelimitedValidator::e_tab_html_report;
   else
   if (m_format == "text")
       flags |= CTabDelimitedValidator::e_tab_text_report;

   CTabDelimitedValidator validator((CTabDelimitedValidator::e_Flags)flags);

   CRef<ILineReader> reader(ILineReader::New(input));

   validator.ValidateInput(*reader, m_columns_def, m_required_cols, m_ignored_cols);
   validator.GenerateOutput(output, false);
}

void CTAbleValApp::ProcessOneFile()
{
    CFile file(m_current_file);
    if (!file.Exists())
    {
        m_logger->PutError(
            *CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
            "File " + m_current_file + " does not exists"));
        return;
    }

    CNcbiOstream* output = 0;
    auto_ptr<CNcbiOfstream> local_output(0);
    CFile local_file;
    try
    {
        if (!DryRun())
        {
            if (m_output == 0)
            {
                string temp_file = CTempString(m_current_file, 0, m_current_file.rfind('.')); // npos will signal to use the whole string
                temp_file += ".val";
                local_file.Reset(temp_file);
                local_output.reset(new CNcbiOfstream(local_file.GetPath().c_str()));
                output = local_output.get();
            }
            else
            {
                output = m_output;
            }
        }

        CNcbiIfstream input(m_current_file.c_str());
        ProcessOneFile(input, output);
        //if (!IsDryRun())
            //m_reader->WriteObject(*obj, *output);
    }
    catch(...)
    {
        // if something goes wrong - remove the partial output to avoid confuse
        if (local_output.get())
        {
            local_file.Remove();
        }
        throw;
    }
}

bool CTAbleValApp::ProcessOneDirectory(const CDir& directory, const CMask& mask, bool recurse)
{
    CDir::TEntries* e = directory.GetEntriesPtr("*", CDir::fCreateObjects | CDir::fIgnoreRecursive);
    auto_ptr<CDir::TEntries> entries(e);

    for (CDir::TEntries::const_iterator it = e->begin(); it != e->end(); it++)
    {
        // first process files and then recursivelly access other folders
        if (!(*it)->IsDir())
        {
            if (mask.Match((*it)->GetPath()))
            {
                m_current_file = (*it)->GetPath();
                ProcessOneFile();
            }
        }
        else
            if (recurse)
            {
                ProcessOneDirectory(**it, mask, recurse);
            }
    }

    return true;
}

void CTAbleValApp::Setup(const CArgs& args)
{
    // Setup application registry and logs for CONNECT library
    CORE_SetLOG(LOG_cxx2c());
    CORE_SetREG(REG_cxx2c(&GetConfig(), false));
    // Setup MT-safety for CONNECT library
    // CORE_SetLOCK(MT_LOCK_cxx2c());

    // Create object manager
    //m_ObjMgr = CObjectManager::GetInstance();
    if ( args["r"] ) {
        // Create GenBank data loader and register it with the OM.
        // The last argument "eDefault" informs the OM that the loader must
        // be included in scopes during the CScope::AddDefaults() call.
        //CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
    }
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CTAbleValApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

