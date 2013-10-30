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
* Authors:  Jonathan Kans, Clifford Clausen,
*           Aaron Ucko, Sergiy Gotvyanskyy
*
* File Description:
*   Converter of various files into ASN.1 format, main application function
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

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <util/line_reader.hpp>

#include "multireader.hpp"
#include "table2asn_context.hpp"
#include "struc_cmt_reader.hpp"
#include "OpticalXML2ASN.hpp"
#include "feature_table_reader.hpp"
#include "remote_updater.hpp"
#include "fcs_reader.hpp"

#include <objects/seq/Seq_descr.hpp>
#include <objects/general/Date.hpp>

#include <objtools/readers/message_listener.hpp>

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;
using namespace objects;
//using namespace validator;

const char * TBL2ASN_APP_VER = "10.0";

/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CTbl2AsnApp : public CNcbiApplication
{
public:
    CTbl2AsnApp(void);

    virtual void Init(void);
    virtual int  Run (void);

private:

    auto_ptr<CRemoteUpdater> m_remote_updater;
    auto_ptr<CMultiReader> m_reader;
    CRef<CSeq_entry> m_replacement_proteins;
    CRef<CSeq_entry> m_possible_proteins;


    void Setup(const CArgs& args);

    void ProcessOneFile();
    void ProcessOneFile(CRef<CSerialObject>& result);
    bool ProcessOneDirectory(const CDir& directory, const CMask& mask, bool recurse);
    void ProcessSecretFiles(CSeq_entry& result);
    void ProcessTBLFile(const string& pathname, CSeq_entry& result);
    void ProcessSRCFile(const string& pathname, CSeq_entry& result);
    void ProcessQVLFile(const string& pathname, CSeq_entry& result);
    void ProcessDSCFile(const string& pathname, CSeq_entry& result);
    void ProcessCMTFile(const string& pathname, CSeq_entry& result, bool byrows);
    void ProcessPEPFile(const string& pathname, CSeq_entry& result);
    void ProcessRNAFile(const string& pathname, CSeq_entry& result);
    void ProcessPRTFile(const string& pathname, CSeq_entry& result);

#if 0
    CRef<CSeq_feat> ReadSeqFeat(void);
    CRef<CBioSource> ReadBioSource(void);
    CRef<CPubdesc> ReadPubdesc(void);
#endif

    CRef<CScope> BuildScope(void);

    CRef<CObjectManager> m_ObjMgr;
    CTable2AsnContext    m_context;
    CRef<CMessageListenerBase> m_logger;
    auto_ptr<CForeignContaminationScreenReportReader> m_fcs_reader;

    //auto_ptr<CObjectIStream> m_In;
    //unsigned int m_Options;
    //bool m_Continue;
    //bool m_OnlyAnnots;

    //size_t m_Level;
    //size_t m_Reported;
    //size_t m_ReportLevel;

    //bool m_DoCleanup;
    //CCleanup m_Cleanup;

    //EDiagSev m_LowCutoff;
    //EDiagSev m_HighCutoff;

    //CNcbiOstream* m_OutputStream;
    CNcbiOstream* m_LogStream;
};

CTbl2AsnApp::CTbl2AsnApp(void):
//m_ObjMgr(0)
    //, m_In(0), m_Options(0), m_Continue(false), m_OnlyAnnots(false)
    //m_Level(0), m_Reported(0), m_OutputStream(0), 
    m_LogStream(0)
{
}


void CTbl2AsnApp::Init(void)
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
        ("x", "String", "Suffix", CArgDescriptions::eString, ".fsa");

    arg_desc->AddFlag("E", "Recurse");
    arg_desc->AddOptionalKey("t", "InFile", "Template File",
        CArgDescriptions::eInputFile);                              

    arg_desc->AddDefaultKey
        ("a", "String", "File Type\n\
                        a Any\n\
                        r20u Runs of 20+ Ns are gaps, 100 Ns are unknown length\n\
                        r20k Runs of 20+ Ns are gaps, 100 Ns are known length\n\
                        r10u Runs of 10+ Ns are gaps, 100 Ns are unknown length\n\
                        r10k Runs of 10+ Ns are gaps, 100 Ns are known length\n\
                        s FASTA Set (s Batch, s1 Pop, s2 Phy, s3 Mut, s4 Eco, s9 Small-genome)\n\
                        d FASTA Delta, di FASTA Delta with Implicit Gaps\n\
                        l FASTA+Gap Alignment (l Batch, l1 Pop, l2 Phy, l3 Mut, l4 Eco, l9 Small-genome)\n\
                        z FASTA with Gap Lines\n\
                        e PHRAP/ACE\n\
                        b ASN.1 for -M flag", CArgDescriptions::eString, "a");

    arg_desc->AddFlag("s", "Read FASTAs as Set");              // done
    arg_desc->AddFlag("g", "Genomic Product Set");              
    arg_desc->AddFlag("J", "Delayed Genomic Product Set ");
    arg_desc->AddDefaultKey
        ("F", "String", "Feature ID Links\n\
                        o By Overlap\n\
                        p By Product", CArgDescriptions::eString, "o");

    arg_desc->AddOptionalKey
        ("A", "String", "Accession", CArgDescriptions::eString);  // done
    arg_desc->AddOptionalKey
        ("C", "String", "Genome Center Tag", CArgDescriptions::eString);
    arg_desc->AddOptionalKey
        ("n", "String", "Organism Name", CArgDescriptions::eString); // done
    arg_desc->AddOptionalKey
        ("j", "String", "Source Qualifiers", CArgDescriptions::eString); // done
    arg_desc->AddOptionalKey
        ("y", "String", "Comment", CArgDescriptions::eString); // done
    arg_desc->AddOptionalKey
        ("Y", "InFile", "Comment File", CArgDescriptions::eInputFile); // done
    arg_desc->AddOptionalKey
        ("D", "InFile", "Descriptors File", CArgDescriptions::eInputFile); // done
    arg_desc->AddOptionalKey
        ("f", "InFile", "Single Table File", CArgDescriptions::eInputFile); // done

    arg_desc->AddOptionalKey
        ("k", "String", "CDS Flags (combine any of the following letters)\n\
                        c Annotate Longest ORF\n\
                        r Allow Runon ORFs\n\
                        m Allow Alternative Starts\n\
                        k Set Conflict on Mismatch", CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("V", "String", "Verification (combine any of the following letters)\n\
                        v Validate with Normal Stringency\n\
                        r Validate without Country Check\n\
                        c BarCode Validation\n\
                        b Generate GenBank Flatfile\n\
                        g Generate Gene Report\n\
                        t Validate with TSA Check", CArgDescriptions::eString);

    arg_desc->AddFlag("q", "Seq ID from File Name");      // almost done
    arg_desc->AddFlag("u", "GenProdSet to NucProtSet");   // done
    arg_desc->AddFlag("I", "General ID to Note");

    arg_desc->AddOptionalKey("G", "String", "Alignment Gap Flags (comma separated fields, e.g., p,-,-,-,?,. )\n\
                                            n Nucleotide or p Protein,\n\
                                            Begin, Middle, End Gap Characters,\n\
                                            Missing Characters, Match Characters", CArgDescriptions::eString);

    arg_desc->AddFlag("R", "Remote Sequence Record Fetching from ID");
    arg_desc->AddFlag("S", "Smart Feature Annotation");

    arg_desc->AddOptionalKey("Q", "String", "mRNA Title Policy\n\
                                            s Special mRNA Titles\n\
                                            r RefSeq mRNA Titles", CArgDescriptions::eString);

    arg_desc->AddFlag("U", "Remove Unnecessary Gene Xref");
    arg_desc->AddFlag("L", "Force Local protein_id/transcript_id");
    arg_desc->AddFlag("T", "Remote Taxonomy Lookup");               // almost done
    arg_desc->AddFlag("P", "Remote Publication Lookup");            // done
    arg_desc->AddFlag("W", "Log Progress");
    arg_desc->AddFlag("K", "Save Bioseq-set");

    arg_desc->AddOptionalKey("H", "String", "Hold Until Publish\n\
                                            y Hold for One Year\n\
                                            mm/dd/yyyy", CArgDescriptions::eString); // done

    arg_desc->AddOptionalKey(
        "Z", "OutFile", "Discrepancy Report Output File", CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("c", "String", "Cleanup (combine any of the following letters)\n\
                                            d Correct Collection Dates (assume month first)\n\
                                            D Correct Collection Dates (assume day first)\n\
                                            b Append note to coding regions that overlap other coding regions with similar product names and do not contain 'ABC'\n\
                                            x Extend partial ends of features by one or two nucleotides to abut gaps or sequence ends\n\
                                            p Add exception to non-extendable partials\n\
                                            s Add exception to short introns\n\
                                            f Fix product names", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("z", "OutFile", "Cleanup Log File", CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("X",  "String", "Extra Flags (combine any of the following letters)\n\
                                             A Automatic definition line generator\n\
                                             C Apply comments in .cmt files to all sequences\n\
                                             E Treat like eukaryota in the Discrepancy Report", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("N", "Integer", "Project Version Number", CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("w", "InFile", "Single Structured Comment File (overrides the use of -X C)", CArgDescriptions::eInputFile); //done
    arg_desc->AddOptionalKey("M", "String", "Master Genome Flags\n\
                                            n Normal\n\
                                            b Big Sequence\n\
                                            p Power Option\n\
                                            t TSA", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("l", "String", "Add type of evidence used to assert linkage across assembly gaps (only for TSA records). Must be one of the following:\n\
                                            paired-ends\n\
                                            align-genus\n\
                                            align-xgenus\n\
                                            align-trnscpt\n\
                                            within-clone\n\
                                            clone-contig\n\
                                            map\n\
                                            strobe", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("m", "String", "Lineage to use for Discrepancy Report tests", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("taxid", "Integer", "Organism taxonomy ID", CArgDescriptions::eInteger); //done
    arg_desc->AddOptionalKey("taxname", "String", "Taxonomy name", CArgDescriptions::eString);          //done
    arg_desc->AddOptionalKey("strain-name", "String", "Strain name", CArgDescriptions::eString);            //done
    arg_desc->AddOptionalKey("ft-url", "String", "FileTrack URL for the XML file retrieval", CArgDescriptions::eString); //done

    arg_desc->AddOptionalKey("gaps-min", "Integer", "minunim run of Ns recognised as a gap", CArgDescriptions::eInteger); //done
    arg_desc->AddOptionalKey("gaps-unknown", "Integer", "exact number of Ns recognised as a gap with unknown length", CArgDescriptions::eInteger); //done
    arg_desc->AddOptionalKey("min-threshold", "Integer", "minimun length of sequence", CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("fcs-file", "FileName", "FCS report file", CArgDescriptions::eInputFile);
    arg_desc->AddFlag("fcs-trim", "Trim FCS regions instead of annotate");
    arg_desc->AddFlag("avoid-submit", "Avoid submit block for optical map");

    arg_desc->AddOptionalKey("logfile", "LogFile", "Error Log File", CArgDescriptions::eOutputFile);    // done

    // Program description
    string prog_description = "Converts files of various formats to ASN.1\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        prog_description, false);

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}


int CTbl2AsnApp::Run(void)
{
    const CArgs& args = GetArgs();

    Setup(args);

    m_LogStream = args["logfile"] ? &(args["logfile"].AsOutputFile()) : &NcbiCout;
    m_logger.Reset(new CMessageListenerLenient());
    m_logger->SetProgressOstream(m_LogStream);
    m_context.m_logger = m_logger;

    /*

    // note - the C Toolkit uses 0 for SEV_NONE, but the C++ Toolkit uses 0 for SEV_INFO
    // adjust here to make the inputs to table2asn match tbl2asn expectations
    //m_ReportLevel = args["R"].AsInteger() - 1;
    //m_LowCutoff = static_cast<EDiagSev>(args["Q"].AsInteger() - 1);
    //m_HighCutoff = static_cast<EDiagSev>(args["P"].AsInteger() - 1);

    //m_DoCleanup = args["cleanup"] && args["cleanup"].AsBoolean();

    // Process file based on its content
    // Unless otherwise specifien we assume the file in hand is
    // a Seq-entry ASN.1 file, other option are a Seq-submit or NCBI
    // Release file (batch processing) where we process each Seq-entry
    // at a time.
    m_Reported = 0;
    */

    m_reader.reset(new CMultiReader(m_context));
    m_remote_updater.reset(new CRemoteUpdater(m_context));

    if (args["fcs-file"])
    {
        m_fcs_reader.reset(new CForeignContaminationScreenReportReader(m_context));
        CRef<ILineReader> reader(ILineReader::New(args["fcs-file"].AsInputFile()));

        m_fcs_reader->LoadFile(*reader);
        m_context.m_fcs_trim = args["fcs-trim"];

        if (args["min-threshold"])
            m_context.m_minimal_sequence_length = args["min-threshold"].AsInteger();
    }

    if (args["n"])
        m_context.m_OrganismName = args["n"].AsString();

    if (args["y"])
        m_context.m_Comment = args["y"].AsString();
    else
    if (args["Y"])
    {
        CNcbiIfstream comments(args["Y"].AsString().c_str());
        comments >> m_context.m_Comment;
    }

    m_context.m_GenomicProductSet = args["g"].AsBoolean();
    m_context.m_HandleAsSet = args["s"].AsBoolean();
    m_context.m_NucProtSet = args["u"].AsBoolean();

    if (m_context.m_NucProtSet || m_context.m_GenomicProductSet)
    {
        m_context.m_HandleAsSet = true;
    }

    if (args["taxname"])
        m_context.m_OrganismName = args["taxname"].AsString();
    if (args["taxid"])
        m_context.m_taxid   = args["taxid"].AsInteger();
    if (args["strain-name"])
        m_context.m_strain  = args["strain-name"].AsString();
    if (args["ft-url"])
        m_context.m_url     = args["ft-url"].AsString();
    if (args["A"])
        m_context.m_accession = args["A"].AsString();
    if (args["j"])
        m_context.m_source_qualifiers = args["j"].AsString();

    if (args["f"])
        m_context.m_single_table5_file = args["f"].AsString();

    if (args["w"])
        m_context.m_single_structure_cmt = args["w"].AsString();

    m_context.m_RemotePubLookup = args["P"].AsBoolean();

    m_context.m_RemoteTaxonomyLookup = args["T"].AsBoolean();

    m_context.m_avoid_submit_block = args["avoid-submit"].AsBoolean();

    if (args["a"])
    {
        const string& gaps = args["a"].AsString();
        if (gaps.length() > 2 && gaps[0] == 'r')
        {            
            switch (*(gaps.end()-1))
            {
            case 'u':
                m_context.m_gap_Unknown_length = 100; // yes, it's hardcoded value
                // do not put break staterment here
            case 'k':
                m_context.m_gapNmin = NStr::StringToUInt(gaps.substr(1, gaps.length()-2));
                break;
            default:
                // error
                break;
            }
        }
    }
    if (args["gaps-min"])
        m_context.m_gapNmin = args["gaps-min"].AsInteger();
    if (args["gaps-unknown"])
        m_context.m_gap_Unknown_length = args["gaps-unknown"].AsInteger();

    if (m_context.m_gapNmin < 0)
        m_context.m_gapNmin = 0;

    if (m_context.m_gapNmin == 0 || m_context.m_gap_Unknown_length < 0 )
        m_context.m_gap_Unknown_length = 0;

    if (args["k"])
        m_context.m_find_open_read_frame = args["k"].AsString();

    if (args["t"])
    {
        m_reader->LoadTemplate(m_context, args["t"].AsString());
    }
    if (args["D"])
    {
        m_reader->LoadDescriptors(args["D"].AsString(), m_context.m_descriptors);
    }

    m_context.ApplySourceQualifiers(m_context.m_entry_template, m_context.m_source_qualifiers);

    if (args["H"])
    {
        try
        {   
            CTime time(args["H"].AsString(), "M/D/Y" );
            m_context.m_HoldUntilPublish.Reset(new CDate(time, CDate::ePrecision_day));
            //m_context.m_HoldUntilPublish->SetToTime(
        }
        catch( const CException & )
        {
            int years = NStr::StringToInt(args["H"].AsString());
            CTime time;
            time.SetCurrent();
            time.SetYear(time.Year()+years);
            m_context.m_HoldUntilPublish.Reset(new CDate(time, CDate::ePrecision_day));
            //                    // couldn't parse date
            //                    x_HandleBadModValue(*mod);
        }
    }

    if (args["N"])
        m_context.m_ProjectVersionNumber = args["N"].AsInteger();

    // Designate where do we output files: local folder, specified folder or a specific single output file
    if (args["o"])
    {
        m_context.m_output = &args["o"].AsOutputFile();
    }
    else
    {
        if (args["r"])
        {
            m_context.m_ResultsDirectory = args["r"].AsString();
        }
        else
        {
            m_context.m_ResultsDirectory = ".";
        }
        m_context.m_ResultsDirectory = CDir::AddTrailingPathSeparator(m_context.m_ResultsDirectory);

        CDir outputdir(m_context.m_ResultsDirectory);
        if (!IsDryRun())
        if (!outputdir.Exists())
            outputdir.Create();
    }

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
            m_context.m_current_file = args["i"].AsString();
            ProcessOneFile ();
        }
    }

    m_logger->Dump(*m_LogStream);

    /*
    if (m_Reported > 0) {
    return 1;
    } else {
    return 0;
    }
    */

    return 0;
}


CRef<CScope> CTbl2AsnApp::BuildScope (void)
{
    CRef<CScope> scope(new CScope (*m_ObjMgr));
    scope->AddDefaults();

    return scope;
}

void CTbl2AsnApp::ProcessOneFile(CRef<CSerialObject>& result)
{
    CRef<CSeq_entry> entry;
    m_context.m_avoid_orf_lookup = false;
    bool avoid_submit_block = false;

    if (m_context.m_current_file.substr(m_context.m_current_file.length()-4).compare(".xml") == 0)
    {
        avoid_submit_block = m_context.m_avoid_submit_block;
        COpticalxml2asnOperator op;
        entry = op.LoadXML(m_context.m_current_file, m_context);
        entry->Parentize();
    }
    else
    {
        entry = m_reader->LoadFile(m_context.m_current_file);
        if (m_fcs_reader.get())
        {
            m_fcs_reader->PostProcess(*entry);
        }
        entry->Parentize();

        m_context.HandleGaps(*entry);
    }

    if (m_context.m_descriptors.NotNull())
        m_reader->ApplyDescriptors(*entry, *m_context.m_descriptors);

    ProcessSecretFiles(*entry);

    m_reader->ApplyAdditionalProperties(*entry);

    CFeatureTableReader fr(m_logger);
    // this may convert seq into seq-set
    if (!m_context.m_avoid_orf_lookup && !m_context.m_find_open_read_frame.empty())
        fr.FindOpenReadingFrame(*entry);

    //fr.m_replacement_protein = m_replacement_proteins;
    fr.MergeCDSFeatures(*entry);
    entry->Parentize();
    //if (m_possible_proteins.NotEmpty())
    //    fr.AddProteins(*m_possible_proteins, *entry);

    bool need_update_date = m_context.ApplyCreateDate(*entry);
    if (need_update_date)
        m_context.ApplyUpdateDate(*entry);


    if (m_context.m_RemoteTaxonomyLookup)
    {
        m_remote_updater->UpdateOrgReferences(*entry);
    }

    m_context.ApplyAccession(*entry);
    m_context.ApplySourceQualifiers(entry, m_context.m_source_qualifiers);

    if (avoid_submit_block)
        result = m_context.CreateSeqEntryFromTemplate(entry);
    else
        result = m_context.CreateSubmitFromTemplate(entry);

    if (m_context.m_RemotePubLookup)
    {
        m_remote_updater->UpdatePubReferences(*result);
    }

    if (avoid_submit_block)
    {
        // we need to fix cit-sub date
        COpticalxml2asnOperator::UpdatePubDate(*result);
    }


}

string GenerateOutputStream(const CTable2AsnContext& m_context, const string& pathname)
{
    string dir;
    string outputfile;
    string base;
    CDirEntry::SplitPath(pathname, &dir, &base, 0);

    outputfile = m_context.m_ResultsDirectory.empty() ? dir : m_context.m_ResultsDirectory;
    outputfile += base;
    outputfile += ".asn";

    return outputfile.c_str();
}

void CTbl2AsnApp::ProcessOneFile()
{
    CFile file(m_context.m_current_file);
    if (!file.Exists())
    {
        m_logger->PutError(
            *CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
            "File " + m_context.m_current_file + " does not exists"));
        return;
    }

    CNcbiOstream* output = 0;
    auto_ptr<CNcbiOfstream> local_output(0);
    CFile local_file;
    try
    {
        if (!DryRun())
        {
            if (m_context.m_output == 0)
            {
                local_file = GenerateOutputStream(m_context, m_context.m_current_file);
                local_output.reset(new CNcbiOfstream(local_file.GetPath().c_str()));
                output = local_output.get();
            }
            else
            {
                output = m_context.m_output;
            }
        }

        CRef<CSerialObject> obj;
        ProcessOneFile(obj);
        if (!IsDryRun())
            m_reader->WriteObject(*obj, *output);
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

bool CTbl2AsnApp::ProcessOneDirectory(const CDir& directory, const CMask& mask, bool recurse)
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
                m_context.m_current_file = (*it)->GetPath();
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

void CTbl2AsnApp::Setup(const CArgs& args)
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
        //CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
    }
}

/*
tbl    5-column Feature Table
.src   tab-delimited table with source qualifiers
.qvl   PHRAP/PHRED/consed quality scores
.dsc  One or more descriptors in ASN.1 format
.cmt  Tab-delimited file for structured comment
.pep  Replacement proteins for coding regions on this sequence, use to mark conflicts
.rna   Replacement mRNA sequences for RNA editing
.prt    Proteins for suggest intervals
*/
void CTbl2AsnApp::ProcessSecretFiles(CSeq_entry& result)
{
    string dir;
    string base;
    string ext;
    CDirEntry::SplitPath(m_context.m_current_file, &dir, &base, &ext);

    string name = dir + base;

    ProcessTBLFile(name + ".tbl", result);
    ProcessTBLFile(m_context.m_single_table5_file, result);
    ProcessSRCFile(name + ".src", result);
    ProcessQVLFile(name + ".qvl", result);
    ProcessDSCFile(name + ".dsc", result);
    ProcessCMTFile(name + ".cmt", result, m_context.m_flipped_struc_cmt);
    ProcessCMTFile(m_context.m_single_structure_cmt, result, m_context.m_flipped_struc_cmt);
    ProcessPEPFile(name + ".pep", result);
    ProcessRNAFile(name + ".rna", result);
    ProcessPRTFile(name + ".prt", result);
}

void CTbl2AsnApp::ProcessTBLFile(const string& pathname, CSeq_entry& result)
{
    CFile file(pathname);
    if (!file.Exists()) return;

    m_context.m_avoid_orf_lookup = true;
    CRef<ILineReader> reader(ILineReader::New(pathname));

    CFeatureTableReader feature_reader(m_logger);
    feature_reader.ReadFeatureTable(result, *reader);
}

void CTbl2AsnApp::ProcessSRCFile(const string& pathname, CSeq_entry& result)
{
    CFile file(pathname);
    if (!file.Exists()) return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    CStructuredCommentsReader cmt_reader(m_logger);
    cmt_reader.ProcessSourceQualifiers(*reader, result);
}

void CTbl2AsnApp::ProcessQVLFile(const string& pathname, CSeq_entry& result)
{
    CFile file(pathname);
    if (!file.Exists()) return;
}

void CTbl2AsnApp::ProcessDSCFile(const string& pathname, CSeq_entry& result)
{
    CFile file(pathname);
    if (!file.Exists()) return;

    CRef<CSeq_descr> descr;
    m_reader->LoadDescriptors(pathname, descr);
    m_reader->ApplyDescriptors(result, *descr);
}

void CTbl2AsnApp::ProcessCMTFile(const string& pathname, CSeq_entry& result, bool byrows)
{
    CFile file(pathname);
    if (!file.Exists()) return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    CStructuredCommentsReader cmt_reader(m_logger);

    if (byrows)
        cmt_reader.ProcessCommentsFileByRows(*reader, result);
    else
        cmt_reader.ProcessCommentsFileByCols(*reader, result);
}

void CTbl2AsnApp::ProcessPEPFile(const string& pathname, CSeq_entry& entry)
{
    CFile file(pathname);
    if (!file.Exists()) return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    CFeatureTableReader peps(m_context.m_logger);

    m_replacement_proteins = peps.ReadReplacementProtein(*reader);
}

void CTbl2AsnApp::ProcessRNAFile(const string& pathname, CSeq_entry& entry)
{
    CFile file(pathname);
    if (!file.Exists()) return;
}

void CTbl2AsnApp::ProcessPRTFile(const string& pathname, CSeq_entry& entry)
{
    CFile file(pathname);
    if (!file.Exists()) return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    CFeatureTableReader prts(m_context.m_logger);

    m_possible_proteins = prts.ReadProtein(*reader);
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    return CTbl2AsnApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

