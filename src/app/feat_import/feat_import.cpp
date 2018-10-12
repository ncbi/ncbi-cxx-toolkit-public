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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Test application for the CFormatGuess component
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <util/format_guess.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrxml.hpp>
#include <serial/objistrjson.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objtools/import/feat_import_error.hpp>
#include <objtools/import/feat_message_handler.hpp>
#include <objtools/import/feat_importer.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
class CFeatImportApp
//  ============================================================================
     : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    string
    xGetInputFormat(
        const CArgs&,
        CNcbiIstream&);

    unsigned int
    xGetImporterFlags(
        const CArgs&);
};

//  ============================================================================
void 
CFeatImportApp::Init(void)
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "CGffImport front end: Import GFF data");

    //
    //  shared flags and parameters:
    //        
    arg_desc->AddDefaultKey(
        "input", 
        "File_In",
        "Input filename",
        CArgDescriptions::eInputFile,
        "-");
    arg_desc->AddAlias("i", "input");

    arg_desc->AddDefaultKey(
        "output", 
        "File_Out",
        "Output filename",
        CArgDescriptions::eOutputFile, "-"); 
    arg_desc->AddAlias("o", "output");

    arg_desc->AddDefaultKey(
        "format", 
        "STRING",
        "Input file format",
        CArgDescriptions::eString, 
        "");

    arg_desc->AddFlag(
        "all-ids-as-local",
        "turn all ids into local ids",
        true);

    arg_desc->AddFlag(
        "numeric-ids-as-local",
        "turn integer ids into local ids",
        true);

    arg_desc->AddFlag(
        "show-progress",
        "periodically report line and record counts",
        true);

    SetupArgDescriptions(arg_desc.release());
}

//  ============================================================================
int 
CFeatImportApp::Run(void)
//  ============================================================================
{
    CFeatImportError errorFormatNotRecognized(
        CFeatImportError::CRITICAL, "Input file format not recognized");

    const CArgs& args = GetArgs();

    CNcbiIstream& istr = args["input"].AsInputFile(CArgValue::fBinary);
    CNcbiOstream& ostr = args["output"].AsOutputFile();

    CSeq_annot annot;
    CFeatMessageHandler errorHandler;
    unique_ptr<CFeatImporter> pImporter(nullptr);
 
    try {
        auto inFormat = xGetInputFormat(args, istr);
        auto flags = xGetImporterFlags(args);
        pImporter.reset(CFeatImporter::Get(inFormat, flags, errorHandler));
        if (!pImporter) {
            throw errorFormatNotRecognized;
        }
    }
    catch (const CFeatImportError& error) {
        cerr << "Line " << error.LineNumber() << ": " << error.SeverityStr() 
             << ": " << error.Message() << "\n";
        return 1;
    } 

    while (true) {
        annot.Reset();
        try {
            pImporter->ReadSeqAnnot(istr, annot);
        }
        catch (const CFeatImportError& error) {
            cerr << "Line " << error.LineNumber() << ": " << error.SeverityStr() 
                << ": " << error.Message() << "\n";
            return 1;
        } 
        if (annot.IsFtable()  &&  !annot.GetData().GetFtable().empty()) {
            ostr << MSerial_Format_AsnText() << annot;
        }
        else {
            break;
        }
    }
    errorHandler.Dump(cerr);

    return 0;
}

//  ============================================================================
void 
CFeatImportApp::Exit(void)
//  ============================================================================
{
    SetDiagStream(0);
}

//  ============================================================================
string
CFeatImportApp::xGetInputFormat(
    const CArgs& args,
    CNcbiIstream& istr)
//  ============================================================================
{
    auto format = args["format"].AsString();
    if (!format.empty()) {
        if (format == "tbl") {
            format = "5col";
        }
        return format;
    }

    CFormatGuess fg(istr);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eGtf);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eGffAugustus);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eBed);
    fg.GetFormatHints().AddPreferredFormat(CFormatGuess::eFiveColFeatureTable);
    fg.GetFormatHints().DisableAllNonpreferred();

    switch(fg.GuessFormat()) {
    default:
        return "";
    case CFormatGuess::eGtf:
    case CFormatGuess::eGffAugustus:
        return "gtf";
    case CFormatGuess::eBed:
        return "bed";
    case CFormatGuess::eFiveColFeatureTable:
        return "5col";
    }
}
  
//  ============================================================================
unsigned int
CFeatImportApp::xGetImporterFlags(
    const CArgs& args)
//  ============================================================================
{
    unsigned int flags(CFeatImporter::fNormal);
    if (args["all-ids-as-local"]) {
        flags |= CFeatImporter::fAllIdsAsLocal;
    }
    if (args["numeric-ids-as-local"]) {
        flags |= CFeatImporter::fNumericIdsAsLocal;
    }
    if (args["show-progress"]) {
        flags |= CFeatImporter::fReportProgress;
    }
    return flags;
}

//  ============================================================================
int main(int argc, const char* argv[])
//  ============================================================================
{
    return CFeatImportApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

