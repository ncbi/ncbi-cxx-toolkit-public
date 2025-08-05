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
#include <common/ncbi_source_ver.h>
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
#include <objmgr/bioseq_ci.hpp>

#include <util/line_reader.hpp>
#include <objtools/edit/remote_updater.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include "multireader.hpp"
#include "table2asn_context.hpp"
#include "struc_cmt_reader.hpp"
#include "feature_table_reader.hpp"
#include "fcs_reader.hpp"
#include "src_quals.hpp"

#include <objects/seq/Seq_descr.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/general/Date.hpp>

#include <objects/seq/Linkage_evidence.hpp>
#include <objects/seq/Seq_gap.hpp>

#include <objtools/edit/seq_entry_edit.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objtools/validator/validator.hpp>

#include <objtools/readers/message_listener.hpp>

#include "table2asn_validator.hpp"

#include <objmgr/feat_ci.hpp>
#include "visitors.hpp"

#include <objtools/readers/fasta_exception.hpp>

#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/writers/async_writers.hpp>
#include <objtools/cleanup/cleanup_pub.hpp>

#include "table2asn.hpp"
#include "suspect_feat.hpp"
#include "annot_match.hpp"

#if __has_include(<multicall/multicall.hpp>)
    #include <multicall/multicall.hpp>
#else
    #define table2asn_app_main main
#endif

#include <common/ncbi_revision.h>

#ifndef NCBI_SC_VERSION
#   define THIS_IS_TRUNK_BUILD
#elif (NCBI_SC_VERSION == 0)
#   define THIS_IS_TRUNK_BUILD
#endif

#include <common/test_assert.h> /* This header must go last */

using namespace ncbi;
using namespace objects;

namespace
{
    class CMissingInputException : exception
    {
    };
}

static void s_FailOnBadInput(const string& specifics, IObjtoolsListener& listener)
{
    listener.PutMessage(CObjtoolsMessage(specifics, eDiag_Fatal));
    throw CMissingInputException();
}

BEGIN_NCBI_SCOPE

class CObjtoolsDiagMessage : public IObjtoolsMessage
{
public:
    CObjtoolsDiagMessage(const string& txt, EDiagSev sev) :
        m_txt(txt),
        m_sev(sev)
    {
    }

    IObjtoolsMessage* Clone() const override { return new CObjtoolsDiagMessage(m_txt, m_sev); }

    void Write(CNcbiOstream& out) const override { out << m_txt; }
    void Dump(CNcbiOstream& out) const override { out << m_txt; }
    void WriteAsXML(CNcbiOstream& out) const override
    {
        out << "<message severity=\"" << NStr::XmlEncode(CNcbiDiag::SeverityName(m_sev))
            << "\" problem=\"" << NStr::XmlEncode(m_txt) << "\" />";
    }
    void DumpAsXML(CNcbiOstream& out) const override
    {
        WriteAsXML(out);
    }
    string   GetText() const override { return m_txt; }
    EDiagSev GetSeverity() const override { return m_sev; }
    int      GetCode() const override { return 0; }
    int      GetSubCode() const override { return 0; }

private:
    string m_txt;
    EDiagSev m_sev;
};


void CTable2AsnLogger::PutProgress(
        const string& sMessage,
        const Uint8   iNumDone,
        const Uint8   iNumTotal)
{
    if (m_enable_log) {
        CMessageListenerLenient::PutProgress(sMessage, iNumDone, iNumTotal);
    }
}


bool CTable2AsnLogger::PutMessage(const IObjtoolsMessage& message)
{
    auto edit = dynamic_cast<const edit::CRemoteUpdaterMessage*>(&message);
    if (edit && (edit->m_error != edit::EPubmedError::citation_not_found)) {
        return false;
    }
    return CMessageListenerLenient::PutMessage(message);
}


void CTable2AsnLogger::Post(const SDiagMessage& mess)
{
    stringstream ss;
    mess.Write(ss, SDiagMessage::fNoEndl);
    string str = ss.str();
    EDiagSev sev = (mess.m_Flags & eDPF_IsNote) ? eDiag_Info : mess.m_Severity;
    this->PutMessage(CObjtoolsDiagMessage(str, sev));
}

void g_LogDiagMessage(ILineErrorListener* logger, EDiagSev sev, const string& msg)
{
    logger->PutMessage(CObjtoolsDiagMessage(msg, sev));
}


CTbl2AsnApp::CTbl2AsnApp()
{
    SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
}


void CTbl2AsnApp::x_SetAlnArgs(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey(
        "aln-file", "InFile", "Alignment input file",
         CArgDescriptions::eInputFile);

    arg_desc.SetDependency(
        "aln-file",
        CArgDescriptions::eExcludes,
        "i");

    arg_desc.AddDefaultKey(
        "aln-gapchar", "STRING", "Alignment missing indicator",
        CArgDescriptions::eString,
        "-");

    arg_desc.AddDefaultKey(
        "aln-missing", "STRING", "Alignment missing indicator",
        CArgDescriptions::eString,
        "");

    arg_desc.AddDefaultKey(
        "aln-alphabet", "STRING", "Alignment alphabet",
        CArgDescriptions::eString,
        "prot");

    arg_desc.SetConstraint(
        "aln-alphabet",
        &(*new CArgAllow_Strings,
          "nuc",
          "prot"));
}


void CTbl2AsnApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    HideStdArgs(fHideDryRun);

    // Print -h document if no arguments supplied
    arg_desc->SetMiscFlags(CArgDescriptions::fUsageIfNoArgs);

    // Prepare command line descriptions, inherit them from tbl2asn legacy application

    arg_desc->AddOptionalKey(
        "indir", "Directory", "Path to input files",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey(
        "outdir", "Directory", "Path to results",
        CArgDescriptions::eOutputFile);

    arg_desc->AddFlag("E", "Recurse");

    arg_desc->AddDefaultKey(
        "x", "String", "Suffix", CArgDescriptions::eString, ".fsa");

    arg_desc->AddOptionalKey(
        "i", "InFile", "Single Input File",
        CArgDescriptions::eInputFile);

    x_SetAlnArgs(*arg_desc);

    arg_desc->AddOptionalKey(
        "o", "OutFile", "Single Output File",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey(
        "out-suffix", "String", "ASN.1 files suffix", CArgDescriptions::eString, ".sqn");

    arg_desc->AddFlag("binary", "Output binary ASN.1");

    arg_desc->AddOptionalKey("t", "InFile", "Template File",
        CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey(
        "a", "String", "File Type\n"
"      a Any\n"
"      s FASTA Set\n"
"      d FASTA Delta, di FASTA Delta with Implicit Gaps\n"
#if 0
may be implemented in the future; RW-1253
"      l FASTA+Gap Alignment (l Batch, l1 Pop, l2 Phy, l3 Mut, l4 Eco,\n"
"        l9 Small-genome)\n"
#endif
"      z FASTA with Gap Lines"
#if 0
may be implemented in the future; RW-1253
"\n"
"      e PHRAP/ACE"
#endif
      , CArgDescriptions::eString, "a");

    arg_desc->AddFlag("J", "Delayed Genomic Product Set ");                // done

    arg_desc->AddOptionalKey(
        "A", "String", "Accession", CArgDescriptions::eString);           // done
    arg_desc->AddOptionalKey(
        "C", "String", "Genome Center Tag", CArgDescriptions::eString);   // done
//    arg_desc->AddOptionalKey(
//        "n", "String", "Organism Name", CArgDescriptions::eString);       // done
    arg_desc->AddOptionalKey(
        "j", "String", "Source Qualifiers.\nThese qualifier values override any conflicting values read from a file (See -src-file)",
         CArgDescriptions::eString);   // done
    arg_desc->AddOptionalKey("src-file", "InFile", "Single source qualifiers file. The qualifiers in this file override any conflicting qualifiers automically read from a .src file, which, in turn, take precedence over source qualifiers specified in a fasta defline", CArgDescriptions::eInputFile); //done
    arg_desc->AddFlag("accum-mods", "Accumulate non-conflicting modifier values from different sources. For example, with this option, a 'note' modifier specified on the command line no longer overwrites a 'note' modifier read from a .src file. Both notes will appear in the output ASN.1. If modifier values conflict, the rules of precedence specified above apply");
    arg_desc->AddOptionalKey(
        "y", "String", "Comment", CArgDescriptions::eString);             // done
    arg_desc->AddOptionalKey(
        "Y", "InFile", "Comment File", CArgDescriptions::eInputFile);     // done
    arg_desc->AddOptionalKey(
        "D", "InFile", "Descriptors File", CArgDescriptions::eInputFile); // done
    arg_desc->AddOptionalKey(
        "f", "InFile", "Single 5 column table file or other annotations", CArgDescriptions::eInputFile); // done

    arg_desc->AddOptionalKey(
        "V", "String", "Verification (combine any of the following letters)\n\
      v Validate with Normal Stringency\n\
      b Generate GenBank Flatfile\n\
      t Validate with TSA Check", CArgDescriptions::eString);

    arg_desc->AddFlag("q", "Seq ID from File Name");      // done

    arg_desc->AddFlag("U", "Remove Unnecessary Gene Xref");
    arg_desc->AddFlag("T", "Remote Taxonomy Lookup");               // done
    arg_desc->AddFlag("P", "Remote Publication Lookup");            // done
    arg_desc->AddFlag("W", "Log Progress");                         // done
    arg_desc->AddFlag("K", "Save Bioseq-set");                      // done

    arg_desc->AddOptionalKey("H", "String", "Hold Until Publish\n\
      y Hold for One Year\n\
      mm/dd/yyyy", CArgDescriptions::eString); // done

    arg_desc->AddFlag("Z", "Output discrepancy report");
    arg_desc->AddFlag("split-dr", "Create unique discrepancy report for each output file");

    arg_desc->AddOptionalKey("c", "String", "Cleanup (combine any of the following letters)\n\
      b Basic cleanup (default)\n\
      e Extended cleanup\n\
      f Fix product names\n\
      s Add exception to short introns\n\
      w WGS cleanup (only needed when using a GFF3 file)\n\
      d Correct Collection Dates (assume month first)\n\
      D Correct Collection Dates(assume day first)\n\
      x Extend ends of features by one or two nucleotides to abut gaps or sequence ends\n\
      - avoid cleanup", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("z", "OutFile", "Cleanup Log File", CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("N", "String", "Project Version Number", CArgDescriptions::eString); //done

    arg_desc->AddOptionalKey("w", "InFile", "Single Structured Comment File", CArgDescriptions::eInputFile); //done
    arg_desc->AddOptionalKey("M", "String", "Master Genome Flags\n\
      n Normal\n\
      t TSA", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("l", "String", "Add type of evidence used to assert linkage across assembly gaps. May be used multiple times. Must be one of the following:\n\
      paired-ends\n\
      align-genus\n\
      align-xgenus\n\
      align-trnscpt\n\
      within-clone\n\
      clone-contig\n\
      map\n\
      strobe\n\
      unspecified\n\
      pcr\n\
      proximity-ligation", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);

    arg_desc->AddOptionalKey("linkage-evidence-file", "InFile", "File listing linkage evidence for gaps of different lengths", CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("gap-type", "String", "Set gap type for runs of Ns. Must be one of the following:\n\
      scaffold\n\
      short-arm\n\
      heterochromatin\n\
      centromere\n\
      telomere\n\
      repeat\n\
      contamination\n\
      contig\n\
      unknown (obsolete)\n\
      fragment\n\
      clone\n\
      other (for future use)", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("m", "String", "Lineage to use for Discrepancy Report tests", CArgDescriptions::eString);

    // all new options are done
    // arg_desc->AddOptionalKey("taxid", "Integer", "Organism taxonomy ID", CArgDescriptions::eInteger);
    // arg_desc->AddOptionalKey("taxname", "String", "Taxonomy name", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("ft-url", "String", "FileTrack URL for the XML file retrieval", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("ft-url-mod", "String", "FileTrack URL for the XML file base modifications", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("gaps-min", "Integer", "minimum run of Ns recognised as a gap", CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("gaps-unknown", "Integer", "exact number of Ns recognised as a gap with unknown length", CArgDescriptions::eInteger);

    // disabled per RW-589
    //arg_desc->AddOptionalKey("min-threshold", "Integer", "minimum length of sequence", CArgDescriptions::eInteger);
    //arg_desc->AddOptionalKey("fcs-file", "FileName", "FCS report file", CArgDescriptions::eInputFile);
    //arg_desc->AddFlag("fcs-trim", "Trim FCS regions instead of annotate");
    arg_desc->AddFlag("postprocess-pubs", "Postprocess pubs: convert authors to standard");
    arg_desc->AddOptionalKey("locus-tag-prefix", "String", "Add prefix to locus tags in annotation files", CArgDescriptions::eString);
    arg_desc->AddFlag("no-locus-tags-needed", "Submission data does not require locus tags");
    arg_desc->AddFlag("euk", "Assume eukaryote, and create missing mRNA features");
    arg_desc->AddOptionalKey("suspect-rules", "String", "Path to a file containing suspect rules set. Overrides environment variable PRODUCT_RULES_LIST", CArgDescriptions::eString);
    arg_desc->AddFlag("allow-acc", "Allow accession recognition in sequence IDs. Default is local");
    arg_desc->AddFlag("augustus-fix", "(Deprecated) Special handling of unusual problems in Augustus annotations",
                      CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
    arg_desc->AddFlag("intronless", "Intronless alignments");
    arg_desc->AddFlag("refine-prt-alignments", "Refine ProSplign aligments when processing .prt input");
    arg_desc->AddOptionalKey("prt-alignment-filter-query", "String",
            "Filter query string for .prt alignments", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("logfile", "LogFile", "Error Log File", CArgDescriptions::eOutputFile);
    arg_desc->AddOptionalKey("logxml", "LogFile", "XML Error Log File", CArgDescriptions::eOutputFile);
    arg_desc->SetDependency("logxml",
                            CArgDescriptions::eExcludes,
                            "logfile");
    arg_desc->AddFlag("split-logs", "Create unique log file for each output file");
    arg_desc->AddFlag("verbose", "Be verbose on reporting");
    arg_desc->AddFlag("huge", "Execute in huge-file mode");
    arg_desc->AddFlag("disable-huge", "Explicitly disable huge-files mode");
    arg_desc->SetDependency("disable-huge",
                            CArgDescriptions::eExcludes,
                            "huge");

    arg_desc->AddOptionalKey("usemt", "String", "Try to use as many threads as:\n\
      one\n\
      two\n\
      many", CArgDescriptions::eString);

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc, default_loaders);
    arg_desc->AddFlag("fetchall", "Search data in all available databases");

    // Program description
    arg_desc->SetUsageContext("", "Converts files of various formats to ASN.1");

    // Pass argument descriptions to the application
    SetupArgDescriptions(arg_desc.release());
}

static void s_PubCleanup(CRef<CPub>& pub)
{
    if (pub->IsArticle()) {
        CCitArtCleaner::CleanArticle(pub->SetArticle(), true, true);
    }
}


size_t CTbl2AsnApp::xGetNumThreads() const
{
    const auto& args = GetArgs();
    string numThreadsConfig;
    if (args["usemt"]) {
        numThreadsConfig = args["usemt"].AsString();
    } else {
        numThreadsConfig = GetConfig().GetString("table2asn", "UseThreads", "one");
    }
    static constexpr array<string_view, 3> numThreadsValues{ "one", "two", "many" };
    auto numThreads = distance(begin(numThreadsValues),
            find(begin(numThreadsValues), end(numThreadsValues), numThreadsConfig));
    numThreads += 1;
    if (1 <= numThreads && numThreads <= 3) {
        cerr << "Will be using " << numThreadsConfig << " threads" << endl;
        return numThreads;
    }
    // default to using a single thread
    return 1;
}

int CTbl2AsnApp::Run()
{
    const CArgs& args = GetArgs();

    Setup(args);

    CTime expires = GetFullVersion().GetBuildInfo().GetBuildTime();
    if (! expires.IsEmpty()) {
        expires.AddYear();
        if (CTime(CTime::eCurrent) > expires) {
            NcbiCerr << "This copy of " << GetProgramDisplayName()
                     << " is more than 1 year old. Please download the current version if it is newer." << endl;
        }
    }

    m_context.m_disable_huge_files = args["disable-huge"];
    if (! m_context.m_disable_huge_files) {
        m_context.m_can_use_huge_files = args["huge"] || GetConfig().GetBool("table2asn", "UseHugeFiles", false);
        if (m_context.m_can_use_huge_files) {
            std::cerr << "Will be using huge files scenario" << std::endl;
        }
    }

    m_context.m_split_log_files = args["split-logs"].AsBoolean();
    if (m_context.m_split_log_files && args["logfile"]) {
        NCBI_THROW(CArgException, eConstraint,
            "-logfile cannot be used with -split-logs");
    }
    m_context.m_verbose = args["verbose"].AsBoolean();

    CTable2AsnLogger* app_logger = new CTable2AsnLogger;
    CNcbiOstream* error_log = args["logfile"] ? &(args["logfile"].AsOutputFile()) : &NcbiCerr;
    app_logger->SetProgressOstream(error_log);
    SetDiagHandler(app_logger, false);
    m_logger.Reset(app_logger);
    m_context.m_logger = m_logger;
    m_logger->m_enable_log = args["W"].AsBoolean();
    m_context.m_remote_updater.reset(new edit::CRemoteUpdater(m_logger));
    m_context.m_remote_updater->SetPubmedInterceptor(s_PubCleanup);
    m_validator.Reset(new CTable2AsnValidator(m_context));

    m_context.m_SetIDFromFile = args["q"].AsBoolean();
    m_context.m_allow_accession = args["allow-acc"].AsBoolean();
    m_context.m_delay_genprodset = args["J"].AsBoolean();
    m_context.m_accumulate_mods = args["accum-mods"].AsBoolean();
    m_context.m_binary_asn1_output = args["binary"].AsBoolean();

    if (args["c"]) {
        if (args["c"].AsString().find_first_not_of("-befwsdDx") != string::npos) {
            NCBI_THROW(CArgException, eConvert,
                "Unrecognized cleanup type " + args["c"].AsString());
        }

        m_context.m_cleanup = args["c"].AsString();
    } else
        m_context.m_cleanup = "b"; // always cleanup

    if (args["M"]) {
        m_context.m_master_genome_flag = args["M"].AsString();
        m_context.m_delay_genprodset = true;
        m_context.m_HandleAsSet = true;
        m_context.m_cleanup += "fU";
        m_context.m_validate = "v"; // do we still need that?
        if (m_context.m_master_genome_flag.find('n') != string::npos) {
            m_context.m_discrepancy_group = NDiscrepancy::eSubmitter;
        } else if (m_context.m_master_genome_flag.find('t') != string::npos) {
            m_context.m_discrepancy_group = NDiscrepancy::eTSA;
        }
    }

    m_reader.reset(new CMultiReader(m_context));

    // excluded per RW-589
#if 0
    if (args["fcs-file"]) {
        m_fcs_reader.reset(new CForeignContaminationScreenReportReader(m_context));
        CRef<ILineReader> reader(ILineReader::New(args["fcs-file"].AsInputFile()));

        m_fcs_reader->LoadFile(*reader);
        m_context.m_fcs_trim = args["fcs-trim"];

        if (args["min-threshold"])
            m_context.m_minimal_sequence_length = args["min-threshold"].AsInteger();
    }
#endif

    // if (args["n"])
    //     m_context.m_OrganismName = args["n"].AsString();

    if (args["y"])
        m_context.m_Comment = args["y"].AsString();
    else if (args["Y"]) {
        CRef<ILineReader> reader(ILineReader::New(args["Y"].AsInputFile()));
        while (! reader->AtEOF()) {
            reader->ReadLine();
            m_context.m_Comment += reader->GetCurrentLine();
            m_context.m_Comment += " ";
        }
    }
    NStr::TruncateSpacesInPlace(m_context.m_Comment);

    if (args["U"] && args["U"].AsBoolean())
        m_context.m_cleanup += 'U';

    if (args["m"]) {
        m_context.m_disc_lineage = args["m"].AsString();
    }

    m_context.m_asn1_suffix = args["out-suffix"].AsString();

    m_context.m_save_bioseq_set = args["K"].AsBoolean();
    m_context.prtAlnOptions.intronless = args["intronless"].AsBoolean();
    m_context.prtAlnOptions.refineAlignment = args["refine-prt-alignments"].AsBoolean();
    if (args["prt-alignment-filter-query"]) {
        m_context.prtAlnOptions.filterQueryString = args["prt-alignment-filter-query"].AsString();
    }
    //   if (args["taxname"])
    //       m_context.m_OrganismName = args["taxname"].AsString();
    //   if (args["taxid"])
    //       m_context.m_taxid = args["taxid"].AsInteger();
    if (args["ft-url"])
        m_context.m_ft_url = args["ft-url"].AsString();
    if (args["ft-url-mod"])
        m_context.m_ft_url_mod = args["ft-url-mod"].AsString();
    if (args["A"])
        m_context.m_accession.Reset(new CSeq_id(args["A"].AsString()));
    if (args["j"]) {
        m_context.mCommandLineMods = args["j"].AsString();
    }
    if (args["w"])
        m_context.m_single_structure_cmt = args["w"].AsString();

    m_context.m_RemotePubLookup = args["P"].AsBoolean();
    if (! m_context.m_RemotePubLookup) // those are always postprocessed
        m_context.m_postprocess_pubs = args["postprocess-pubs"].AsBoolean();

    m_context.m_RemoteTaxonomyLookup = args["T"].AsBoolean();
    if (m_context.m_RemoteTaxonomyLookup) {
        m_context.m_cleanup += 'T';
    }

    if (args["a"]) {
        const string& a_arg = args["a"].AsString();
        if (a_arg == "s" || a_arg == "z") {
            m_context.m_HandleAsSet = true;
        } else if (a_arg == "s1") {
            m_context.m_HandleAsSet = true;
            m_context.m_ClassValue = CBioseq_set::eClass_pop_set;
        } else if (a_arg == "s2") {
            m_context.m_HandleAsSet = true;
            m_context.m_ClassValue = CBioseq_set::eClass_phy_set;
        } else if (a_arg == "s3") {
            m_context.m_HandleAsSet = true;
            m_context.m_ClassValue = CBioseq_set::eClass_mut_set;
        } else if (a_arg == "s4") {
            m_context.m_HandleAsSet = true;
            m_context.m_ClassValue = CBioseq_set::eClass_eco_set;
        } else if (a_arg == "s9") {
            m_context.m_HandleAsSet = true;
            m_context.m_ClassValue = CBioseq_set::eClass_small_genome_set;
        } else if (a_arg == "di") {
            m_context.m_di_fasta = true;
        } else if (a_arg == "d") {
            m_context.m_d_fasta = true;
        }
    }
    if (args["gaps-min"]) {
        int gaps_min = args["gaps-min"].AsInteger();
        if (gaps_min < 0) {
            NCBI_THROW(CArgException, eConvert,
                "Invalid value: gaps-min cannot be negative.");
        }
        m_context.m_gapNmin = static_cast<TSeqPos>(gaps_min);
    }
    if (args["gaps-unknown"]) {
        int gaps_unknown = args["gaps-unknown"].AsInteger();
        if (gaps_unknown < 0) {
            NCBI_THROW(CArgException, eConvert,
                "Invalid value: gaps-unknown cannot be negative.");
        }
        m_context.m_gap_Unknown_length = static_cast<TSeqPos>(gaps_unknown);
    }
    if (m_context.m_gap_Unknown_length > 0 && m_context.m_gapNmin == 0) {
        m_context.m_gapNmin = m_context.m_gap_Unknown_length;
    }

    if (args["linkage-evidence-file"]) {
        // auto lefile_cstr = args["linkage-evidence-file"].AsString().c_str();
        // auto pLEStream = make_unique<CNcbiIfstream>(lefile_cstr,ios::binary);

        g_LoadLinkageEvidence(
            args["linkage-evidence-file"].AsString(),
            m_context.m_GapsizeToEvidence,
            m_context.m_logger);
        m_context.m_gap_type = CSeq_gap::eType_scaffold; // for compatibility with tbl2asn
    }

    if (args["l"]) {
        auto linkage_evidence_to_value = CLinkage_evidence::ENUM_METHOD_NAME(EType)();
        for (auto& arg_it : args["l"].GetStringList()) {
            try {
                auto value = linkage_evidence_to_value->FindValue(arg_it);
                m_context.m_DefaultEvidence.insert(value);
                m_context.m_gap_type = CSeq_gap::eType_scaffold; // for compatibility with tbl2asn
            } catch (...) {
                NCBI_THROW(CArgException, eConvert,
                    "Unrecognized linkage evidence " + arg_it);
            }
        }
    }

    if (args["gap-type"]) {
        auto gaptype_to_value = CSeq_gap::ENUM_METHOD_NAME(EType)();
        try {
            auto value = gaptype_to_value->FindValue(args["gap-type"].AsString());
            m_context.m_gap_type = value;
        } catch (...) {
            NCBI_THROW(CArgException, eConvert,
                "Unrecognized gap type " + args["gap-type"].AsString());
        }
    }

    if (args["H"]) {
        string sdate = args["H"].AsString();
        if (sdate == "Y" || sdate == "y") {
            m_context.m_HoldUntilPublish.SetCurrent();
            m_context.m_HoldUntilPublish.SetYear(m_context.m_HoldUntilPublish.Year() + 1);
        } else {
            try {
                if (sdate[0] == '\'' && sdate.length() > 0 && sdate[sdate.length() - 1] == '\'') {
                    sdate.erase(0, 1);
                    sdate.erase(sdate.length() - 1, 1);
                }
                m_context.m_HoldUntilPublish = CTime(sdate, "M/D/Y");
            } catch (const CException&) {
                int years = NStr::StringToInt(args["H"].AsString());
                m_context.m_HoldUntilPublish.SetCurrent();
                m_context.m_HoldUntilPublish.SetYear(m_context.m_HoldUntilPublish.Year() + years);
            }
        }
    }

    if (args["N"])
        m_context.m_ProjectVersionNumber = args["N"].AsString();

    if (args["C"]) {
        m_context.m_genome_center_id = args["C"].AsString();
        if (! m_context.m_ProjectVersionNumber.empty())
            m_context.m_genome_center_id += m_context.m_ProjectVersionNumber;
    }

    if (args["V"]) {
        m_context.m_validate += args["V"].AsString();
        size_t p;
        while ((p = m_context.m_validate.find("b")) != string::npos) {
            m_context.m_validate.erase(p, 1);
            m_context.m_make_flatfile = true;
        }
        while ((p = m_context.m_validate.find("t")) != string::npos) {
            //m_context.m_discrepancy = eTriState_False;
            m_context.m_validate.erase(p, 1);
        }
    }

    if (args["Z"]) {
        m_context.m_run_discrepancy = true;
        if (args["split-dr"])
            m_context.m_split_discrepancy = true;
    }

    if (args["locus-tag-prefix"] || args["no-locus-tags-needed"]) {
        if (args["locus-tag-prefix"] && args["no-locus-tags-needed"]) {
            // mutually exclusive
            NCBI_THROW(CArgException, eConstraint,
                "-no-locus-tags-needed and -locus-tag-prefix are mutually exclusive");
        }
        if (args["no-locus-tags-needed"]) {
            m_context.m_locus_tag_prefix  = "";
            m_context.m_locus_tags_needed = false;
        } else {
            m_context.m_locus_tag_prefix  = args["locus-tag-prefix"].AsString();
            m_context.m_locus_tags_needed = true;
        }
    }

    if (m_context.m_HandleAsSet) {
        if (false)
            NCBI_THROW(CArgException, eConstraint, "-s flag cannot be used with -d, -e, -l or -z");
    }

    // Designate where do we output files: local folder, specified folder or a specific single output file
    if (args["outdir"])
        m_context.m_ResultsDirectory = CDir::AddTrailingPathSeparator(args["outdir"].AsString());

    if (args["o"]) {
        m_context.m_output_filename = args["o"].AsString();
        m_context.m_output = &args["o"].AsOutputFile();
    } else {
        if (args["outdir"]) {
            CDir outputdir(m_context.m_ResultsDirectory);
            if (! IsDryRun())
                if (! outputdir.Exists())
                    outputdir.Create();
        }
    }

    m_context.m_eukaryote = args["euk"].AsBoolean();

    if (m_context.m_cleanup.find('f') != string::npos)
        m_context.m_use_hypothetic_protein = true;

    if (args["suspect-rules"])
        m_context.m_suspect_rules->SetRulesFilename(args["suspect-rules"].AsString());

    try {
        if (args["t"]) {
            m_context.m_t = true;
            m_reader->LoadTemplate(args["t"].AsString());
        }
    } catch (const CException&) {
        g_LogGeneralParsingError("Error loading template file", *m_logger);
    }

    try {
        if (args["D"]) {
            m_reader->LoadDescriptors(args["D"].AsString(), m_global_files.m_descriptors);
        }
    } catch (const CException&) {
        g_LogGeneralParsingError("Error loading descriptors file", *m_logger);
    }

    if (m_logger->Count() == 0)
    try {
        if (args["f"]) {
            string annot_file = args["f"].AsString();
            if (! CFile(annot_file).Exists()) {
                s_FailOnBadInput(
                    "The specified annotation file \"" + annot_file + "\" does not exist.",
                    *m_logger);
            }
            m_context.m_single_annot_file = args["f"].AsString();
        }
        if (args["src-file"]) {
            string src_file = args["src-file"].AsString();
            if (! CFile(src_file).Exists()) {
                s_FailOnBadInput(
                    "The specified source qualifier file \"" + src_file + "\" does not exist.",
                    *m_logger);
            }
            m_context.m_single_source_qual_file = args["src-file"].AsString();
        }

        // Designate where do we get input: single file or a folder or folder structure
        if (args["i"]) {
            m_context.m_current_file = args["i"].AsString();
            CFile argAsFile(m_context.m_current_file);
            if (! argAsFile.Exists()) {
                s_FailOnBadInput(
                    "The specified input file \"" + m_context.m_current_file + "\" does not exist.",
                    *m_logger);
            }
            if (argAsFile.GetLength() > TBL2ASN_MAX_ALLOWED_FASTA_SIZE && m_context.m_disable_huge_files) {
                if (CFormatGuess::Format(m_context.m_current_file) == CFormatGuess::eFasta) {
                    s_FailOnBadInput(
                        "The specified input file \"" +
                            m_context.m_current_file +
                            "\" is too long. The maximum permissible file size for a FASTA sequence is " +
                            NStr::NumericToString(TBL2ASN_MAX_ALLOWED_FASTA_SIZE, NStr::fWithCommas) +
                            " bytes. Consider allowing huge mode (remove \"-disable-huge\" from the command line).",
                        *m_logger);
                }
            }
            ProcessOneFile(false);
        } else if (args["indir"]) {
            // initiate validator output
            string indir = args["indir"].AsString();
            CDir directory(indir);
            if (! directory.Exists()) {
                s_FailOnBadInput(
                    "The specified input directory \"" + indir + "\" does not exist.",
                    *m_logger);
            }
            string basename = m_context.m_output_filename.empty() ?
                CDir(CDir::CreateAbsolutePath(indir, CDir::eRelativeToCwd)).GetBase() :
                m_context.m_output_filename;

            m_context.m_base_name = basename;

            CMaskFileName masks;
            masks.Add("*" + args["x"].AsString());

            ProcessOneDirectory(directory, masks, args["E"].AsBoolean());
        } else if (args["aln-file"]) {
            m_context.m_current_file = args["aln-file"].AsString();
            if (! CFile(m_context.m_current_file).Exists()) {
                s_FailOnBadInput(
                    "The specified alignment file \"" + m_context.m_current_file + "\" does not exist.",
                    *m_logger);
            }
            const bool isAlignment = true;
            ProcessOneFile(isAlignment);
        }

        // RW-927
        if (m_context.m_verbose &&
            m_global_files.mp_src_qual_map &&
            ! m_global_files.mp_src_qual_map->Empty()) {
            m_global_files.mp_src_qual_map->ReportUnusedIds();
        }

        if (m_validator->ValTotalErrors() > 0) {
            std::ofstream ostr;
            ostr.exceptions(ios::failbit | ios::badbit);
            ostr.open(m_context.GenerateOutputFilename(eFiles::stats, m_context.m_base_name));
            m_validator->ValReportErrorStats(ostr);
        }
        m_validator->ReportDiscrepancies(m_context.GenerateOutputFilename(eFiles::dr, m_context.m_base_name));
    } catch (const CMissingInputException&) {
        // Error message has already been logged
    } catch (const CException& ex) {
        const CException* original = &ex;
        // ASN writer populates exception with all nested exceptions stack which is not neccessary
        // we need the original exception
        while (original->GetPredecessor()) original = original->GetPredecessor();

        auto msg = original->GetMsg();
        auto bad_res_exc = dynamic_cast<const CBadResiduesException*>(original);
        if (bad_res_exc) {
            int line = 0;
            ILineError::TVecOfLines lines;
            if (bad_res_exc->GetBadResiduePositions().m_BadIndexMap.size() == 1) {
                line = bad_res_exc->GetBadResiduePositions().m_BadIndexMap.begin()->first;
            } else {
                lines.reserve(bad_res_exc->GetBadResiduePositions().m_BadIndexMap.size());
                for(auto rec: bad_res_exc->GetBadResiduePositions().m_BadIndexMap) {
                    lines.push_back(rec.first);
                }
            }

            unique_ptr<CLineError> le(
                CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error,
                bad_res_exc->GetBadResiduePositions().m_SeqId->AsFastaString(),
                line, "", "", "", msg, lines));
            m_logger->PutError(*le);
        } else {
            auto seq_map_exc = dynamic_cast<const CSeqMapException*>(original);
            if (seq_map_exc) {
                if (!args["r"] && !(args.Exist("vdb") && args["vdb"]) && msg.find("Cannot resolve") != string::npos) {
                    msg += " - try running with -r to enable remote retrieval of sequences";
                }
            }
            g_LogGeneralParsingError(msg, *m_logger);
        }
    }

    int ret = 0;
    if (m_logger->Count() == 0) {
        #ifdef THIS_IS_TRUNK_BUILD
            m_context.m_remote_updater->ReportStats(std::cerr);
        #endif
    } else {
        m_logger->Dump();
        if (args["logxml"]) {
            CNcbiOstream& log_xml = args["logxml"].AsOutputFile();
            log_xml << "<logmessages>" << endl;
            m_logger->DumpAsXML(log_xml);
            log_xml << "</logmessages>" << endl;
        }

        size_t errors = m_logger->LevelCount(eDiag_Critical) +
                        m_logger->LevelCount(eDiag_Error) +
                        m_logger->LevelCount(eDiag_Fatal);
        // all errors reported as failure
        if (errors > 0) {
            ret = 1;
        } else {
            // only warnings reported as 2
            if (m_logger->LevelCount(eDiag_Warning) > 0)
                ret = 2;
            else // otherwise it's ok
                ret = 0;
        }
    }

    // prevent further logging after m_logger is autodestroyed; RW-2219, RW-2233
    SetDiagHandler(nullptr, false);
    return ret;
}

void CTbl2AsnApp::ProcessOneEntry(
    CFormatGuess::EFormat inputFormat,
    CRef<CSerialObject>   obj,
    CRef<CSerialObject>&  result)
{
    auto scope = Ref(new CScope(*m_context.m_ObjMgr));
    scope->AddDefaults();

    CRef<CSeq_entry> entry;
    CRef<CSeq_submit> submit;

    m_reader->GetSeqEntry(entry, submit, obj);

    bool avoid_submit_block = false;

    entry->Parentize();

    if (m_context.m_SetIDFromFile) {
        m_context.SetSeqId(*entry);
    }

    m_context.ApplyAccession(*entry);

    if (! IsDryRun()) {
        std::function<std::ostream&()> f = [this]() -> std::ostream& { return m_context.GetOstream(eFiles::fixedproducts); };
        m_context.m_suspect_rules->SetupOutput(f);
    }
    m_context.ApplyFileTracks(*entry);

    const bool readModsFromTitle =
        inputFormat == CFormatGuess::eFasta ||
        inputFormat == CFormatGuess::eAlignment;
    xProcessSecretFiles1Phase(readModsFromTitle, *entry);

    if (m_context.m_RemoteTaxonomyLookup) {
        m_context.m_remote_updater->UpdateOrgFromTaxon(*entry);
    } else {
        VisitAllBioseqs(*entry, CTable2AsnContext::UpdateTaxonFromTable);
    }

    m_secret_files->m_feature_table_reader->m_replacement_protein = m_secret_files->m_replacement_proteins;
    TAsyncToken token;
    m_secret_files->m_feature_table_reader->MergeCDSFeatures(*entry, token);

    entry->Parentize();
    m_secret_files->m_feature_table_reader->MoveProteinSpecificFeats(*entry);

    m_context.CorrectCollectionDates(*entry);

    if (m_secret_files->m_possible_proteins.NotEmpty())
        m_secret_files->m_feature_table_reader->AddProteins(*m_secret_files->m_possible_proteins, *entry);

    if (m_context.m_HandleAsSet) {
        m_secret_files->m_feature_table_reader->ConvertNucSetToSet(entry);
    }

    if ((inputFormat == CFormatGuess::eTextASN) ||
        (inputFormat == CFormatGuess::eBinaryASN)) {
        // if create-date exists apply update date
        m_context.ApplyCreateUpdateDates(*entry);
    }

    m_context.ApplyComments(*entry);
    ProcessSecretFiles2Phase(*entry);

    // this methods do not remove entry nor change it. But create 'result' object which either
    // equal to 'entry' or contain reference to 'entry'.
    if (avoid_submit_block)
        result = m_context.CreateSeqEntryFromTemplate(entry);
    else
        result = m_context.CreateSubmitFromTemplate(entry, submit);

    m_secret_files->m_feature_table_reader->MakeGapsFromFeatures(*entry);

    if (m_context.m_delay_genprodset) {
        VisitAllFeatures(*entry, [this](CSeq_feat& feature) { m_context.RenameProteinIdsQuals(feature); });
    } else {
        VisitAllFeatures(*entry, [this](CSeq_feat& feature) { m_context.RemoveProteinIdsQuals(feature); });
    }

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CCleanup::ConvertPubFeatsToPubDescs(seh);

    if (m_context.m_RemotePubLookup) {
        m_context.m_remote_updater->UpdatePubReferences(*obj);
    }
    if (m_context.m_postprocess_pubs) {
        edit::CRemoteUpdater::PostProcessPubs(*entry);
    }

    if (m_context.m_cleanup.find('-') == string::npos) {
        m_validator->Cleanup(submit, seh, m_context.m_cleanup);
    }

    // make asn.1 look nicier
    edit::SortSeqDescr(*entry);

    m_secret_files->m_feature_table_reader->ChangeDeltaProteinToRawProtein(*entry);

    if (! IsDryRun()) {
        m_validator->UpdateECNumbers(*entry);

        if (! m_context.m_validate.empty()) {
            m_validator->ValCollect(submit, entry, m_context.m_validate);
        }

        m_validator->CollectDiscrepancies(submit, seh);

        if (m_context.m_make_flatfile) {
            MakeFlatFile(seh, submit, m_context.GetOstream(eFiles::gbf));
        }
    }
}

void CTbl2AsnApp::ProcessTopEntry(CFormatGuess::EFormat inputFormat, bool need_update_date, CRef<CSeq_submit>& submit, CRef<CSeq_entry>& entry)
{
    m_context.ApplyComments(*entry);

    if (m_global_files.m_descriptors)
        m_reader->ApplyDescriptors(*entry, *m_global_files.m_descriptors);

    if (m_secret_files->m_descriptors)
        m_reader->ApplyDescriptors(*entry, *m_secret_files->m_descriptors);

    if (need_update_date) {
        m_context.ApplyUpdateDate(*entry);
    }

    if (submit) {
        if (m_context.m_RemotePubLookup) {
            m_context.m_remote_updater->UpdatePubReferences(*submit);
        }

        CCleanup cleanup(nullptr, CCleanup::eScope_UseInPlace); // RW-1070 - CCleanup::eScope_UseInPlace is essential
        cleanup.ExtendedCleanup(*submit, CCleanup::eClean_NoNcbiUserObjects);
    }

    bool need_report = (inputFormat == CFormatGuess::eFasta) && ! m_context.m_HandleAsSet;
    if (need_report) {
        g_LogGeneralParsingError(
            eDiag_Warning,
            "File " + m_context.m_current_file + " contains multiple sequences",
            *(m_context.m_logger));
    }
}


void CTbl2AsnApp::MakeFlatFile(CSeq_entry_Handle seh, CRef<CSeq_submit> submit, std::ostream& ostream)
{
    CFlatFileGenerator ffgenerator(
        CFlatFileConfig::eFormat_GenBank,
        CFlatFileConfig::eMode_Entrez);

    if (submit.Empty())
        ffgenerator.Generate(seh, ostream);
    else
        ffgenerator.Generate(*submit, seh.GetScope(), ostream);
}

void CTbl2AsnApp::SetupAndOpenDiagnosticStreams()
{
    for (eFiles e : { eFiles::ecn, eFiles::gbf, eFiles::val, eFiles::fixedproducts }) {
        m_context.SetOutputFilename(e, m_context.GenerateOutputFilename(e));
    }

    if (m_context.m_split_discrepancy) {// otherwise leave it unopened
        m_context.SetOutputFilename(eFiles::dr, m_context.GenerateOutputFilename(eFiles::dr));
    }
    m_context.OpenDiagnosticOutputs();
}

void CTbl2AsnApp::SetupAndOpenDataStreams()
{
    if (m_context.m_output) {
        m_context.SetOutputFile(eFiles::asn, *m_context.m_output);
    } else {
        m_context.SetOutputFilename(eFiles::asn, m_context.GenerateOutputFilename(eFiles::asn));
    }
    m_context.OpenDataOutputs();
}

void CTbl2AsnApp::CloseDiagnosticStreams()
{
    m_context.CloseDiagnosticOutputs();
}

void CTbl2AsnApp::CloseDataStreams()
{
    m_context.CloseDataOutputs();
}

static bool s_UseHugeFileMode(const CTable2AsnContext& context, CFormatGuess::EFormat format)
{
    if (context.m_disable_huge_files) {
        return false;
    }
    return (context.m_can_use_huge_files ||
            format == CFormatGuess::eFasta ||
            format == CFormatGuess::eGff3);
}


void CTbl2AsnApp::ProcessOneFile(bool isAlignment, bool manageDiagnosticStreams, bool manageDataStreams)
{
    if (m_context.m_split_log_files)
        m_context.m_logger->ClearAll();

    CFile log_name;
    if (! IsDryRun() && m_context.m_split_log_files) {
        log_name = m_context.GenerateOutputFilename(eFiles::log);
        CNcbiOstream* error_log = new CNcbiOfstream(log_name.GetPath());
        m_logger->SetProgressOstream(error_log);
        SetDiagStream(error_log);
    }

    try {
        if (manageDiagnosticStreams) {
            SetupAndOpenDiagnosticStreams();
        }
        if (manageDataStreams) {
            SetupAndOpenDataStreams();
        }
        CNcbiOstream* output = &m_context.GetOstream(eFiles::asn);

        std::function<std::ostream&()> f = [this]() -> std::ostream& { return m_context.GetOstream(eFiles::fixedproducts); };
        m_context.m_suspect_rules->SetupOutput(f);

        m_context.m_huge_files_mode = false;

        LoadAdditionalFiles();

        if (isAlignment) {
            ProcessAlignmentFile(output);
        } else {
            m_validator->Clear();

            edit::CHugeFile hugeFile;
            try {
                hugeFile.Open(m_context.m_current_file, &CMultiReader::kSupportedTypes);
            } catch (CObjReaderParseException& e) {
                auto message = e.GetMsg();
                if (message == "File format not supported") {
                    hugeFile.m_format = CFormatGuess::eFasta;
                } else {
                    throw;
                }
            }

            if (s_UseHugeFileMode(m_context, hugeFile.m_format)) {

                if (! m_context.m_use_threads) {
                    m_context.m_use_threads = xGetNumThreads();
                }

                ProcessHugeFile(hugeFile, output);
            } else {
                const string objectType =
                    hugeFile.m_content ?
                    hugeFile.m_content->GetName() :
                    "";
                ProcessOneFile(
                    hugeFile.m_format,
                    objectType,
                    // *(hugeFile.m_stream),
                    hugeFile.m_stream,
                    output);
            }

            if (! m_context.m_validate.empty())
                m_validator->ValReportErrors();

            if (m_context.m_split_discrepancy)
                m_validator->ReportDiscrepancies();

            ReportUnusedSourceQuals();

        } // !isAlignment

        if (! log_name.GetPath().empty()) {
            m_logger->SetProgressOstream(&NcbiCout);
        }
        if (manageDiagnosticStreams) {
            CloseDiagnosticStreams();
        }
        if (manageDataStreams) {
            CloseDataStreams();
        }
    } catch (...) {
        if (! log_name.GetPath().empty()) {
            m_logger->SetProgressOstream(&NcbiCout);
        }

        m_context.DeleteOutputs();
        if (m_context.m_output) {
            GetArgs()["o"].CloseFile();
            _ASSERT(! m_context.m_output_filename.empty());
            CFile(m_context.m_output_filename).Remove(CDirEntry::fIgnoreMissing);
        }

        throw;
    }
}

void CTbl2AsnApp::ProcessOneFile(
    CFormatGuess::EFormat format,
    const string& contentType,
    unique_ptr<CNcbiIstream>& pIstr,
    CNcbiOstream* output)
{
    CMultiReader::TAnnots annots;
    CRef<CSerialObject> pInputObject =
        m_reader->FetchEntry(format,
                             contentType,
                             pIstr,
                             annots);

    xProcessOneFile(format, pInputObject, annots, output);
}


void CTbl2AsnApp::ProcessOneFile(CNcbiOstream* output)
{
    CRef<CSerialObject> input_obj;
    TAnnots annots;
    CFormatGuess::EFormat format = m_reader->OpenFile(m_context.m_current_file, input_obj, annots);
    xProcessOneFile(format, input_obj, annots, output);
}

void CTbl2AsnApp::xProcessOneFile(
        CFormatGuess::EFormat format,
        CRef<CSerialObject> input_obj,
        TAnnots& /*annots*/,
        CNcbiOstream* output)
{

#if 0
    if (! annotMap.empty()) {
        for (auto entry : annotMap) {
            auto it = m_secret_files->m_AnnotMap.find(entry.first);
            if (it == m_secret_files->m_AnnotMap.end()) {
                m_secret_files->m_AnnotMap.emplace(entry.first, entry.second);
            } else {
                it->second.splice(it->second.end(), entry.second);
            }
        }
        annotMap.clear();
    }
#endif
    do {
        CRef<CSerialObject> result;
        ProcessOneEntry(format, input_obj, result);

        if (! IsDryRun() && result.NotEmpty()) {
            const CSerialObject* to_write = result;
            if (m_context.m_save_bioseq_set) {
                if (result->GetThisTypeInfo()->IsType(CSeq_entry::GetTypeInfo())) {
                    const CSeq_entry* se = static_cast<const CSeq_entry*>(result.GetPointer());
                    if (se->IsSet())
                        to_write = &se->GetSet();
                }
            }

            m_reader->WriteObject(*to_write, *output);
        }
        input_obj = m_reader->ReadNextEntry();
    } while (input_obj.NotEmpty());
}

void CTbl2AsnApp::ProcessAlignmentFile(CNcbiOstream* output)
{
    const string& filename = m_context.m_current_file;
    unique_ptr<CNcbiIstream> pIstream(new CNcbiIfstream(filename));

    CRef<CSeq_entry> pEntry = m_reader->ReadAlignment(*pIstream, GetArgs());
    pEntry->Parentize();
    m_context.MergeWithTemplate(*pEntry);

    CRef<CSerialObject> pResult;

    CFormatGuess::EFormat inputFormat = CFormatGuess::eAlignment;
    ProcessOneEntry(inputFormat, pEntry, pResult);

    if (IsDryRun() || ! pResult) {
        return;
    }

    if (m_context.m_save_bioseq_set &&
        pResult->GetThisTypeInfo()->IsType(CSeq_entry::GetTypeInfo())) {

        const CSeq_entry* pTempEntry
            = static_cast<const CSeq_entry*>(pResult.GetPointer());
        if (pTempEntry->IsSet()) {
            m_reader->WriteObject(pTempEntry->GetSet(), *output);
            return;
        }
    }
    m_reader->WriteObject(*pResult, *output);
}


bool CTbl2AsnApp::ProcessOneDirectory(const CDir& directory, const CMask& mask, bool recurse)
{
    unique_ptr<CDir::TEntries> entries(directory.GetEntriesPtr("*", CDir::fCreateObjects | CDir::fIgnoreRecursive));
    vector<unique_ptr<CDir::CDirEntry>> vec(entries->size());
    auto vec_it = vec.begin();
    for (CDir::TEntry& it : *entries) {
        vec_it->reset(it.release());
        ++vec_it;
    }

    auto compareNames = [](const auto& l, const auto& r) { return l->GetPath() < r->GetPath(); };
    sort(vec.begin(), vec.end(), compareNames);

    bool commonOutputStream = GetArgs()["o"];
    if (commonOutputStream) {
        SetupAndOpenDataStreams();
    }
    SetupAndOpenDiagnosticStreams();

    for (const auto& it : vec) {
        // first process files and then recursivelly access other folders
        if (! it->IsDir()) {
            auto pathName = it->GetPath();
            if (mask.Match(pathName)) {
                m_context.m_current_file = pathName;
                ProcessOneFile(false, ! commonOutputStream, ! commonOutputStream);
            }
        } else if (recurse) {
            ProcessOneDirectory(*it, mask, recurse);
        }
    }
    if (commonOutputStream) {
        CloseDataStreams();
    }
    CloseDiagnosticStreams();
    return true;
}


void CTbl2AsnApp::Setup(const CArgs& args)
{
    // initialize conn library
    CONNECT_Init(&GetConfig());

    // Create object manager and scope

    m_context.m_ObjMgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *m_context.m_ObjMgr, default_loaders);
}

/*
.tbl   5-column Feature Table
.src   tab-delimited table with source qualifiers
.qvl   PHRAP/PHRED/consed quality scores
.dsc   One or more descriptors in ASN.1 format
.cmt   Tab-delimited file for structured comment
.pep   Replacement proteins for coding regions on this sequence, use to mark conflicts
.rna   Replacement mRNA sequences for RNA editing
.prt   Proteins for suggest intervals
*/
void CTbl2AsnApp::xProcessSecretFiles1Phase(bool readModsFromTitle, CSeq_entry& result)
{
    auto modMergePolicy =
        m_context.m_accumulate_mods ?
        CModHandler::eAppendPreserve :
        CModHandler::ePreserve;

    g_ApplyMods(
        m_global_files.mp_src_qual_map.get(),
        m_secret_files->mp_src_qual_map.get(),
        m_context.mCommandLineMods,
        readModsFromTitle,
        m_context.m_verbose,
        modMergePolicy,
        m_logger,
        result);

    if (! m_context.m_huge_files_mode) {
        if (m_global_files.m_descriptors)
            m_reader->ApplyDescriptors(result, *m_global_files.m_descriptors);
        if (m_secret_files->m_descriptors)
            m_reader->ApplyDescriptors(result, *m_secret_files->m_descriptors);
    }

    if (m_global_files.m_indexed_annots || m_secret_files->m_indexed_annots)
    {
        AddAnnots(result);
    }
}

void CTbl2AsnApp::ProcessSecretFiles2Phase(CSeq_entry& result) const
{
    ProcessCMTFiles(result);
}

void CTbl2AsnApp::LoadDSCFile(const string& pathname)
{
    CFile file(pathname);
    if (! file.Exists() || file.GetLength() == 0)
        return;
    m_reader->LoadDescriptors(pathname, m_secret_files->m_descriptors);
}

void CTbl2AsnApp::ProcessCMTFiles(CSeq_entry& result) const
{
    if (m_global_files.m_struct_comments)
        m_global_files.m_struct_comments->ProcessComments(result);
    if (m_secret_files && m_secret_files->m_struct_comments)
        m_secret_files->m_struct_comments->ProcessComments(result);
}

void CTbl2AsnApp::LoadPEPFile(const string& pathname)
{
    CFile file(pathname);
    if (! file.Exists() || file.GetLength() == 0)
        return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    m_secret_files->m_replacement_proteins = m_secret_files->m_feature_table_reader->ReadProtein(*reader);
}

void CTbl2AsnApp::LoadRNAFile(const string& pathname)
{
    CFile file(pathname);
    if (! file.Exists() || file.GetLength() == 0)
        return;
}

void CTbl2AsnApp::LoadPRTFile(const string& pathname)
{
    CFile file(pathname);
    if (! file.Exists() || file.GetLength() == 0)
        return;

    CRef<ILineReader> reader(ILineReader::New(pathname));

    m_secret_files->m_possible_proteins = m_secret_files->m_feature_table_reader->ReadProtein(*reader);
}


bool CTbl2AsnApp::LoadAnnotMap(const string& pathname, TAdditionalFiles& files)
{
    CFile file(pathname);

    if (! file.Exists())
        return false;

    if (file.IsIdentical(m_context.m_current_file)) {
        g_LogGeneralParsingError(
            eDiag_Warning,
            "Ignorning annotation " + pathname + " because it was already used as input source",
            *m_logger);
        return false;
    }

    if (file.GetLength() == 0) {
        g_LogGeneralParsingError(eDiag_Warning, "Empty file: " + pathname, *m_logger);
        return false;
    }

    files.m_indexed_annots = m_reader->LoadIndexedAnnot(pathname);

    return true;
}


void CTbl2AsnApp::AddAnnots(CSeq_entry& entry)
{
    if (entry.IsSeq()) {
        m_reader->AddAnnots(m_global_files.m_indexed_annots.get(), entry.SetSeq());
        if (m_secret_files) {
            m_reader->AddAnnots(m_secret_files->m_indexed_annots.get(), entry.SetSeq());
        }
        return;
    }

    if (! entry.GetSet().IsSetSeq_set()) {
        return;
    }

    // If this is a nuc-prot set, only add annotations to the nucleotide sequence
    if (entry.GetSet().IsSetClass() &&
        entry.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot)
    { // We expect the nucleotide sequence to appear first, but this will work even if it doesn't.
        for (auto pSubEntry : entry.SetSet().SetSeq_set()) {
            if (pSubEntry && pSubEntry->IsSeq() && pSubEntry->GetSeq().IsNa()) {
                AddAnnots(*pSubEntry);
                return;
            }
        }
    }

    for (auto pSubEntry : entry.SetSet().SetSeq_set()) {
        if (pSubEntry) {
            AddAnnots(*pSubEntry);
        }
    }
}

void CTbl2AsnApp::LoadCMTFile(const string& pathname, unique_ptr<CTable2AsnStructuredCommentsReader>& comments)
{
    if (! comments) {
        CFile file(pathname);
        if (file.Exists() && file.GetLength()) {
            comments.reset(new CTable2AsnStructuredCommentsReader(pathname, m_logger, m_context.m_verbose));
        }
    }
}

void CTbl2AsnApp::LoadAdditionalFiles()
{
    string dir;
    string base;
    string ext;
    CDirEntry::SplitPath(m_context.m_current_file, &dir, &base, &ext);

    string name = dir + base;

    // always reset secret file
    m_secret_files.reset(new TAdditionalFiles);
    m_secret_files->m_feature_table_reader.reset(new CFeatureTableReader(m_context));

    const auto& namedSrcFile = m_context.m_single_source_qual_file;
    if (! NStr::IsBlank(namedSrcFile) && ! m_global_files.mp_src_qual_map) {
        m_global_files.mp_src_qual_map.reset(new CMemorySrcFileMap(m_logger));
        m_global_files.mp_src_qual_map->MapFile(namedSrcFile, m_context.m_allow_accession);
    }

    const string defaultSrcFile = name + ".src";
    if (CFile(defaultSrcFile).Exists()) {
        m_secret_files->mp_src_qual_map.reset(new CMemorySrcFileMap(m_logger));
        m_secret_files->mp_src_qual_map->MapFile(defaultSrcFile, m_context.m_allow_accession);
    }

    LoadPEPFile(name + ".pep");
    LoadRNAFile(name + ".rna");
    LoadPRTFile(name + ".prt");
    LoadDSCFile(name + ".dsc");

    LoadCMTFile(m_context.m_single_structure_cmt, m_global_files.m_struct_comments);
    LoadCMTFile(name + ".cmt", m_secret_files->m_struct_comments);

    // if (m_context.m_can_use_huge_files && ! m_context.m_disable_huge_files) {
    {
        if (! m_context.m_single_annot_file.empty() && ! m_global_files.m_indexed_annots)
        { // load only once
            LoadAnnotMap(m_context.m_single_annot_file, m_global_files);
        } else {
            for (auto suffix : { ".tbl", ".gff", ".gff3", ".gff2", ".gtf" }) {
                if (LoadAnnotMap(name + suffix, *m_secret_files))
                    break;
            }
#ifdef THIS_IS_TRUNK_BUILD
            for (auto suffix : { ".gbf" }) {
                LoadAnnotMap(name + suffix, *m_secret_files);
            }
#endif
        }
    }
}

void CTbl2AsnApp::ReportUnusedSourceQuals()
{
    if (m_context.m_verbose && m_secret_files && m_secret_files->mp_src_qual_map)
        m_secret_files->mp_src_qual_map->ReportUnusedIds();
}

END_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int table2asn_app_main(int argc, const char* argv[])
{
    #ifdef _DEBUG
    // this code converts single argument into multiple, just to simplify testing
    list<string> split_args;
    vector<const char*> new_argv;

    if (argc==2 && argv && argv[1] && strchr(argv[1], ' ')) {
        NStr::Split(argv[1], " ", split_args);

        auto it = split_args.begin();
        while (it != split_args.end()) {
            auto next = it; ++next;
            if (next != split_args.end() &&
                ((it->front() == '"' && it->back() != '"') ||
                 (it->front() == '\'' && it->back() != '\'')))
            {
                it->append(" "); it->append(*next);
                next = split_args.erase(next);
            } else it = next;
        }
        for (auto& rec : split_args) {
            if (rec.front() == '\'' && rec.back()=='\'')
                rec = rec.substr(1, rec.length()-2);
        }
        argc = 1 + split_args.size();
        new_argv.reserve(argc);
        new_argv.push_back(argv[0]);
        for (const string& s : split_args) {
            new_argv.push_back(s.c_str());
            std::cerr << s.c_str() << " ";
        }
        std::cerr << "\n";


        argv = new_argv.data();
    }
    #endif
    return CTbl2AsnApp().AppMain(argc, argv, 0, eDS_Default, "table2asn.conf");
}
