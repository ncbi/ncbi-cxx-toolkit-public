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
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbi_system.hpp>
#include <util/format_guess.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Variation_ref.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_listener.hpp>
#include <objtools/readers/idmapper.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/readers/vcf_reader.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gtf_reader.hpp>
#include <objtools/readers/gvf_reader.hpp>
#include <objtools/readers/aln_reader.hpp>
#include <objtools/readers/psl_reader.hpp>
#include <objtools/readers/agp_seq_entry.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/ucscregion_reader.hpp>
#include <objtools/readers/rm_reader.hpp>
#include <objtools/readers/read_util.hpp>
#include <objtools/readers/mod_error.hpp>
//#include <misc/hgvs/hgvs_reader.hpp>

#include <objtools/logging/listener.hpp>
#include <objtools/edit/feattable_edit.hpp>

#include <algo/phy_tree/phy_node.hpp>
#include <algo/phy_tree/dist_methods.hpp>
#include <objects/biotree/BioTreeContainer.hpp>

#include <objtools/cleanup/cleanup.hpp>

#include "multifile_source.hpp"
#include "multifile_destination.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//class CGff3LocationMerger;

string s_AlnErrorToString(const CAlnError & error)
{
    return FORMAT(
        "At ID '" << error.GetID() << "' "
        "in category '" << static_cast<int>(error.GetCategory()) << "' "
        "at line " << error.GetLineNum() << ": "
        << error.GetMsg() << "'");
}


//  ============================================================================
class TestCanceler: public ICanceled
//  ============================================================================
{
    bool IsCanceled() const { return false; };
};


//  ============================================================================
class CMultiReaderMessageListener:
    public CReaderListener
//  ============================================================================
{
public:
    bool PutMessage(
        const IObjtoolsMessage& message)
    {
        const CReaderMessage* pReaderMessage =
            dynamic_cast<const CReaderMessage*>(&message);
        if (!pReaderMessage || pReaderMessage->Severity() == eDiag_Fatal) {
            throw;
        }
        pReaderMessage->Write(cerr);
        return true;
    };
};

//  ============================================================================
class CMultiReaderApp
//  ============================================================================
     : public CNcbiApplication
{
public:
    CMultiReaderApp(): m_uFormat(CFormatGuess::eUnknown), m_pErrors(nullptr)
    {
        SetVersion(CVersionInfo(1, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY));
    }

    // Create quick simple messages
    static AutoPtr<ILineError> sCreateSimpleMessage(
        EDiagSev eDiagSev, const string & msg);
    void WriteMessageImmediately(
        ostream & ostr, const ILineError & line_error_p);
    bool ShowingProgress() const { return m_showingProgress; };
protected:

private:
    void Init() override;
    int Run() override;

    bool xProcessSingleFile(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessDefault(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessWiggle(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessWiggleRaw(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    bool xProcessBed(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessUCSCRegion(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessBedRaw(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGtf(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessNewick(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGff3(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGff2(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGvf(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessAlignment(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessAgp(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcess5ColFeatTable(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessFasta(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    void xProcessRmo(const CArgs&, CNcbiIstream&, CNcbiOstream&);
    //void xProcessHgvs(const CArgs&, CNcbiIstream&, CNcbiOstream&);

    void xSetFormat(const CArgs&, CNcbiIstream&);
    void xSetFlags(const CArgs&, const string&);
    void xSetFlags(const CArgs&, CNcbiIstream&);
    void xSetMapper(const CArgs&);
    void xSetMessageListener(const CArgs&);

    void xPostProcessAnnot(const CArgs&, CSeq_annot&, const CGff3LocationMerger* =nullptr);
    void xWriteObject(const CArgs&, CSerialObject&, CNcbiOstream&);
    void xDumpErrors(CNcbiOstream& );

    CFormatGuess::EFormat m_uFormat;
    bool m_bCheckOnly;
    bool m_bDumpStats;
    long  m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;
    bool m_bXmlMessages;
    bool m_showingProgress;

    unique_ptr<CIdMapper> m_pMapper;
    unique_ptr<CMessageListenerBase> m_pErrors;
    unique_ptr<CObjtoolsListener> m_pEditErrors;
};



//  ============================================================================
class CMyMessageListenerCustom:
//  ============================================================================
    public CMessageListenerBase
{
public:
    CMyMessageListenerCustom(
        int iMaxCount,
        int iMaxLevel,
        CMultiReaderApp & multi_reader_app)
        : m_iMaxCount(iMaxCount), m_iMaxLevel(iMaxLevel),
          m_multi_reader_app(multi_reader_app)
    {};

    ~CMyMessageListenerCustom() {};

    bool
    PutMessage(
        const IObjtoolsMessage& message)
    {
        StoreMessage(message);
        return (message.GetSeverity() <= m_iMaxLevel) && (Count() < m_iMaxCount);
    };

    bool
    PutError(
        const ILineError& err)
    {
        if (err.Problem() == ILineError::eProblem_ProgressInfo) {
            m_multi_reader_app.WriteMessageImmediately(cerr, err);
            return true;
        }
        StoreError(err);
        return (err.Severity() <= m_iMaxLevel) && (Count() < m_iMaxCount);
    }

    void
    PutProgress(
        const string& /*msg*/,
        const Uint8 bytesDone,
        const Uint8 /*dummy*/)
    {
        if (!m_multi_reader_app.ShowingProgress()) {
            return;
        }
        AutoPtr<ILineError> line_error_p =
            m_multi_reader_app.sCreateSimpleMessage(
                eDiag_Info,
                FORMAT("Progress: " << bytesDone << " bytes done."));
        m_multi_reader_app.WriteMessageImmediately(cerr, *line_error_p);
        //if (bytesDone > 1000000) {
        //    bIsCanceled = true;
        //}
    };

protected:
    size_t m_iMaxCount;
    int m_iMaxLevel;
    CMultiReaderApp & m_multi_reader_app;
};

//  ============================================================================
class CMyMessageListenerCustomLevel:
//  ============================================================================
    public CMessageListenerLevel
{
public:
    CMyMessageListenerCustomLevel(
        int level, CMultiReaderApp & multi_reader_app)
        : CMessageListenerLevel(level),
          m_multi_reader_app(multi_reader_app) {};

    void
    PutProgress(
        const string& msg,
        const Uint8 bytesDone,
        const Uint8 /*dummy*/)
    {
        if (!m_multi_reader_app.ShowingProgress()) {
            return;
        }
        AutoPtr<ILineError> line_error_p =
            m_multi_reader_app.sCreateSimpleMessage(
                eDiag_Info,
                FORMAT(msg << " (" << bytesDone << " bytes)"));
        m_multi_reader_app.WriteMessageImmediately(cerr, *line_error_p);
        //if (bytesDone > 1000000) {
        //    bIsCanceled = true;
        //}
    };

protected:
    size_t m_iMaxCount;
    int m_iMaxLevel;
    CMultiReaderApp & m_multi_reader_app;
};

//  ============================================================================
CMultiReaderMessageListener newStyleMessageListener;
//  ============================================================================

//  ----------------------------------------------------------------------------
void CMultiReaderApp::Init()
//  ----------------------------------------------------------------------------
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext("", "C++ multi format file reader");

    //
    //  input / output:
    //

    arg_desc->SetCurrentGroup("INPUT / OUTPUT");

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
        "indir",
        "Dir_In",
        "Input directory",
        CArgDescriptions::eDirectory,
        "");
    arg_desc->AddAlias("p", "indir");

    arg_desc->AddDefaultKey(
        "outdir",
        "Dir_Out",
        "Output directory",
        CArgDescriptions::eDirectory,
        "");
    arg_desc->AddAlias("r", "outdir");

    arg_desc->AddDefaultKey(
        "format",
        "STRING",
        "Input file format",
        CArgDescriptions::eString,
        "guess");
    arg_desc->SetConstraint(
        "format",
        &(*new CArgAllow_Strings,
            "bed",
            "microarray", "bed15",
            "wig", "wiggle", "bedgraph",
            "gtf", "gff3", "gff2", "augustus",
            "gvf",
            "agp",
            "newick", "tree", "tre",
            "vcf",
            "aln", "align",
            "fasta",
            "5colftbl",
            "ucsc",
            "hgvs",
            "psl",
            "rmo",
            "guess") );

    arg_desc->AddDefaultKey("out-format", "FORMAT",
        "This sets how the output of this program will be formatted.  "
        "Note that for some formats some or all values might have no effect.",
        CArgDescriptions::eString, "asn_text");
    arg_desc->SetConstraint(
        "out-format",
        &(*new CArgAllow_Strings,
            "asn_text",
            "asn_binary",
            "xml",
            "json" ) );


    arg_desc->AddDefaultKey(
        "flags",
        "STRING",
        "Additional flags passed to the reader, as a single flag integer or comma separated flag names",
        CArgDescriptions::eString,
        "0" );

    arg_desc->AddDefaultKey(
        "name",
        "STRING",
        "Name for annotation",
        CArgDescriptions::eString,
        "");
    arg_desc->AddDefaultKey(
        "title",
        "STRING",
        "Title for annotation",
        CArgDescriptions::eString,
        "");

    //
    //  ID mapping:
    //

    arg_desc->SetCurrentGroup("ID MAPPING");

    arg_desc->AddDefaultKey(
        "mapfile",
        "File_In",
        "IdMapper config filename",
        CArgDescriptions::eString, "" );

    arg_desc->AddDefaultKey(
        "genome",
        "STRING",
        "UCSC build number",
        CArgDescriptions::eString,
        "" );

    //
    //  Error policy:
    //

    arg_desc->SetCurrentGroup("ERROR POLICY");

    arg_desc->AddFlag(
        "dumpstats",
        "write record counts to stderr",
        true );

    arg_desc->AddFlag(
        "xmlmessages",
        "where possible, print errors, warnings, etc. as XML",
        true );

    arg_desc->AddFlag(
        "checkonly",
        "check for errors only",
        true );

    arg_desc->AddFlag(
        "noerrors",
        "suppress error display",
        true );

    arg_desc->AddFlag(
        "lenient",
        "accept all input format errors",
        true );

    arg_desc->AddFlag(
        "strict",
        "accept no input format errors",
        true );

    arg_desc->AddDefaultKey(
        "max-error-count",
        "INTEGER",
        "Maximum permissible error count",
        CArgDescriptions::eInteger,
        "-1" );

    arg_desc->AddDefaultKey(
        "max-error-level",
        "STRING",
        "Maximum permissible error level",
        CArgDescriptions::eString,
        "warning" );

    arg_desc->SetConstraint(
        "max-error-level",
        &(*new CArgAllow_Strings,
            "info", "warning", "error" ) );

    arg_desc->AddFlag("show-progress",
        "This will show progress messages on stderr, if the underlying "
        "reader supports that.");

    //
    //  bed and gff reader specific arguments:
    //

    arg_desc->SetCurrentGroup("BED AND GFF READER SPECIFIC");

    arg_desc->AddFlag(
        "all-ids-as-local",
        "turn all ids into local ids",
        true );

    arg_desc->AddFlag(
        "numeric-ids-as-local",
        "turn integer ids into local ids",
        true );

    arg_desc->AddFlag(
        "3ff",
        "use BED three feature format",
        true);

    arg_desc->AddFlag(
        "dfm",
        "use BED directed feature model",
        true);

    arg_desc->AddFlag(
        "genbank",
        "clean up output for genbank submission",
        true);

    arg_desc->AddFlag(
        "genbank-no-locus-tags",
        "clean up output for genbank submission, no locus-ag needed",
        true);

    arg_desc->AddFlag(
        "cleanup",
        "clean up output but without genbank specific extensions",
        true);

    arg_desc->AddFlag(
        "euk",
        "in -genbank mode, generate any missing mRNA features",
        true);

    arg_desc->AddFlag(
        "gene-xrefs",
        "generate parent-child xrefs involving genes",
        true);

    arg_desc->AddDefaultKey(
        "locus-tag",
        "STRING",
        "Prefix or starting tag for auto generated locus tags",
        CArgDescriptions::eString,
        "" );

    arg_desc->AddOptionalKey(
        "autosql",
        "FILENAME",
        "BED autosql definition file",
        CArgDescriptions::eString);

    //
    //  wiggle reader specific arguments:
    //

    arg_desc->SetCurrentGroup("WIGGLE READER SPECIFIC");

    arg_desc->AddFlag(
        "join-same",
        "join abutting intervals",
        true );

    arg_desc->AddFlag(
        "as-byte",
        "generate byte compressed data",
        true );

    arg_desc->AddFlag(
        "as-real",
        "generate real value data",
        true );

    arg_desc->AddFlag(
        "as-graph",
        "generate graph object",
        true );

    arg_desc->AddFlag(
        "raw",
        "iteratively return raw track data",
        true );

    //
    //  gff reader specific arguments:
    //

    arg_desc->SetCurrentGroup("GFF READER SPECIFIC");

    arg_desc->AddFlag( // no longer used, retained for backward compatibility
        "new-code",
        "use new gff3 reader implementation",
        true );
    arg_desc->AddFlag(
        "old-code",
        "use old gff3 reader implementation",
        true );

    //
    //  gff reader specific arguments:
    //

    arg_desc->SetCurrentGroup("GTF READER SPECIFIC");

    arg_desc->AddFlag( // no longer used, retained for backward compatibility
        "child-links",
        "generate gene->mrna and gene->cds xrefs",
        true );

    //
    //  alignment reader specific arguments:
    //

    arg_desc->SetCurrentGroup("ALIGNMENT READER SPECIFIC");

    arg_desc->AddDefaultKey(
        "aln-gapchar",
        "STRING",
        "Alignment gap character",
        CArgDescriptions::eString,
        "-");

    arg_desc->AddDefaultKey(
        "aln-missing",
        "STRING",
        "Alignment missing indicator",
        CArgDescriptions::eString,
        "");

    arg_desc->AddDefaultKey(
        "aln-alphabet",
        "STRING",
        "Alignment alphabet",
        CArgDescriptions::eString,
        "prot");
    arg_desc->SetConstraint(
        "aln-alphabet",
        &(*new CArgAllow_Strings,
            "nuc",
            "prot") );

    arg_desc->AddDefaultKey(
        "aln-idval",
        "STRING",
        "Alignment sequence ID validation scheme",
        CArgDescriptions::eString,
        "");

    arg_desc->AddFlag(
        "force-local-ids",
        "treat all IDs as local IDs",
        true);

    arg_desc->AddFlag(
        "ignore-nexus-info",
        "ignore char settings in NEXUS format block",
        true);
    //
    // FASTA reader specific arguments:
    //

    arg_desc->SetCurrentGroup("FASTA READER SPECIFIC");

    arg_desc->AddFlag(
        "parse-mods",
        "Parse FASTA modifiers on deflines.");

    arg_desc->AddFlag(
        "parse-gaps",
        "Make a delta sequence if gaps found.");

    arg_desc->AddDefaultKey(
        "max-id-length",
        "INTEGER",
        "Maximum permissible ID length",
        CArgDescriptions::eInteger,
        "0" );
    arg_desc->SetCurrentGroup("");

    SetupArgDescriptions(arg_desc.release());
}

//  ----------------------------------------------------------------------------
int
CMultiReaderApp::Run()
//  ----------------------------------------------------------------------------
{
    m_iFlags = 0;

    const CArgs& args = GetArgs();
    string argInFile = args["input"].AsString();
    string argOutFile = args["output"].AsString();
    string argInDir = args["indir"].AsString();
    string argOutDir = args["outdir"].AsString();

    if ((argInFile != "-") && !argInDir.empty()) {
        cerr << "multireader: command line args -input and -indir are incompatible."
             << endl;
        return 1;
    }
    if ((argOutFile != "-") && !argOutDir.empty()) {
        cerr << "multireader: command line args -output and -outdir are incompatible."
             << endl;
        return 1;
    }
    if (argInDir.empty() && !argOutDir.empty()) {
        cerr << "multireader: command line arg -outdir requires -indir."
             << endl;
        return 1;
    }
    if (argOutDir.empty() && !argInDir.empty()) {
        cerr << "multireader: command line arg -indir requires -outdir."
            << endl;
        return 1;
    }
    if (args["genbank"].AsBoolean() && args["genbank-no-locus-tags"].AsBoolean()) {
        cerr << "multireader: flags -genbank and -genbank-no-locus-tags are mutually "
            "exclusive"
            << endl;
        return 1;
    }
    if (!args["locus-tag"].AsString().empty() && args["genbank-no-locus-tags"].AsBoolean()) {
        cerr << "multireader: flags -locus-tag and -genbank-no-locus-tags are mutually "
            "exclusive"
            << endl;
        return 1;
    }
    if (argInFile == "-" && args["format"].AsString() == "guess") {
        cerr << "multireader: must specify input format (\"-format ...\") if input comes from "
                "console or pipe"
            << endl;
        return 1;
    }

    xSetMapper(args);
    xSetMessageListener(args);

    if (!argInDir.empty()) {
        // with tests above, establishes multifile operation
        string inFilePattern = CDirEntry::MakePath(argInDir, "*", "gff3");
        string inFile, outFile;
        CMultiFileSource fileSource(inFilePattern);
        CMultiFileDestination fileDestination(argOutDir);
        bool retIn = fileSource.Next(inFile);
        while (retIn) {
            if (!fileDestination.Next(inFile, outFile)) {
                cerr << "multireader: unable to create output file "
                        << outFile << "." << endl;
                return 1;
            }
            CNcbiIfstream istr(inFile, IOS_BASE::binary);
            CNcbiOfstream ostr(outFile);
            if (!xProcessSingleFile(args, istr, ostr)) {
                return 1;
            }
            retIn = fileSource.Next(inFile);
        }
    }
    else {
        // at this point, implies single file operation
        CNcbiIstream& istr = args["input"].AsInputFile(CArgValue::fBinary);
        CNcbiOstream& ostr = args["output"].AsOutputFile();
        if (!xProcessSingleFile(args, istr, ostr)) {
            return 1;
        }
    }
    return 0;
}

//  -----------------------------------------------------------------------------
bool
CMultiReaderApp::xProcessSingleFile(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  -----------------------------------------------------------------------------
{
    bool retCode = true;

    try {
        xSetFlags(args, args["input"].AsString());
        switch( m_uFormat ) {
            default:
                xProcessDefault(args, istr, ostr);
                break;
            case CFormatGuess::eWiggle:
                if (m_iFlags & CReaderBase::fAsRaw) {
                    xProcessWiggleRaw(args, istr, ostr);
                }
                else {
                    xProcessWiggle(args, istr, ostr);
                }
                break;
            case CFormatGuess::eBed:
                if (m_iFlags & CReaderBase::fAsRaw) {
                    xProcessBedRaw(args, istr, ostr);
                }
                else {
                    retCode = xProcessBed(args, istr, ostr);
                }
                break;
            case CFormatGuess::eUCSCRegion:
                xProcessUCSCRegion(args, istr, ostr);
                break;
            case CFormatGuess::eGtf:
            case CFormatGuess::eGtf_POISENED:
            case CFormatGuess::eGffAugustus:
                xProcessGtf(args, istr, ostr);
                break;
            case CFormatGuess::eNewick:
                xProcessNewick(args, istr, ostr);
                break;
            case CFormatGuess::eGff3:
                xProcessGff3(args, istr, ostr);
                break;
            case CFormatGuess::eGff2:
                xProcessGff2(args, istr, ostr);
                break;
            case CFormatGuess::eGvf:
                xProcessGvf(args, istr, ostr);
                break;
            case CFormatGuess::eAgp:
                xProcessAgp(args, istr, ostr);
                break;
            case CFormatGuess::eAlignment:
                xProcessAlignment(args, istr, ostr);
                break;
            case CFormatGuess::eFiveColFeatureTable:
                xProcess5ColFeatTable(args, istr, ostr);
                break;
            case CFormatGuess::eFasta:
                xProcessFasta(args, istr, ostr);
                break;
            case CFormatGuess::eRmo:
                xProcessRmo(args, istr, ostr);
                break;
            //case CFormatGuess::eHgvs:
            //    xProcessHgvs(args, istr, ostr);
            //    break;
        }
    }
    catch(const CReaderMessage& message) {
        message.Dump(cerr);
        retCode = false;
    }
    catch(const ILineError&) {
        AutoPtr<ILineError> line_error_p =
            sCreateSimpleMessage(
                eDiag_Fatal, "Reading aborted due to fatal error.");
        //m_pErrors->PutError(reader_ex); // duplicate!
        m_pErrors->PutError(*line_error_p);
        retCode = false;
    }
    catch(const CException& e) {
        ostringstream os;
        os << e.GetMsg();
        ostringstream osEx;
        e.ReportExtra(osEx);
        if (osEx.tellp() > 0) {
            os << " (" << osEx.str() << ')';
        }
        AutoPtr<ILineError> line_error_p =
            sCreateSimpleMessage(eDiag_Fatal,
                "Reading aborted due to fatal error: " + os.str());
        m_pErrors->PutError(*line_error_p);
        retCode = false;
    }
    catch(const std::exception & std_ex) {
        AutoPtr<ILineError> line_error_p =
            sCreateSimpleMessage(
                eDiag_Fatal,
                FORMAT(
                    "Reading aborted due to fatal error: " << std_ex.what()));
        m_pErrors->PutError(*line_error_p);
        retCode = false;
    }
    catch(int) {
        // hack on top of hackish reporting system
        retCode = false;
    }
    catch(...) {
        AutoPtr<ILineError> line_error_p =
            sCreateSimpleMessage(
                eDiag_Fatal, "Unknown Fatal Error occurred");
        m_pErrors->PutError(*line_error_p);
        retCode = false;
    }
    xDumpErrors( cerr );
    return retCode;
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessDefault(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    unique_ptr<CReaderBase> pReader(
        CReaderBase::GetReader(m_uFormat, m_iFlags, &newStyleMessageListener));
    if (!pReader) {
        CReaderMessage fatal(
            eDiag_Fatal, 1, "File format not supported");
        throw(fatal);
    }
    if (ShowingProgress()) {
        pReader->SetProgressReportInterval(10);
    }
    //TestCanceler canceler;
    //pReader->SetCanceler(&canceler);
    pReader->ReadSeqAnnots(annots, istr, m_pErrors.get());
    for (CRef<CSeq_annot> cit : annots) {
        xWriteObject(args, *cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessWiggle(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    CWiggleReader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    if (ShowingProgress()) {
        reader.SetProgressReportInterval(10);
    }
    //TestCanceler canceler;
    //reader.SetCanceler(&canceler);
    reader.ReadSeqAnnots(annots, istr, m_pErrors.get());
    for (CRef<CSeq_annot> cit : annots) {
        xWriteObject(args, *cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessWiggleRaw(
    const CArgs& /*args*/,
    CNcbiIstream& istr,
    CNcbiOstream& /*ostr*/)
//  ----------------------------------------------------------------------------
{
    CWiggleReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    CRawWiggleTrack raw;
    while (reader.ReadTrackData(lr, raw)) {
        raw.Dump(cerr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessUCSCRegion(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
{
    //  Use ReadSeqAnnot() over ReadSeqAnnots() to keep memory footprint down.
    CUCSCRegionReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    CRef<CSerialObject> pAnnot = reader.ReadObject(lr, m_pErrors.get());
    if (pAnnot) {
        xWriteObject(args, *pAnnot, ostr);
    }
}
//  ----------------------------------------------------------------------------
bool CMultiReaderApp::xProcessBed(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    //  Use ReadSeqAnnot() over ReadSeqAnnots() to keep memory footprint down.
    CBedReader reader(m_iFlags, m_AnnotName, m_AnnotTitle, &newStyleMessageListener);
    if (args["autosql"]) {
        if (!reader.SetAutoSql(args["autosql"].AsString())) {
            return false;
        }
    }
    if (ShowingProgress()) {
        reader.SetProgressReportInterval(10);
    }
    //TestCanceler canceler;
    //reader.SetCanceler(&canceler);
    CStreamLineReader lr( istr );
    CRef<CSeq_annot> pAnnot = reader.ReadSeqAnnot(lr, m_pErrors.get());
    while(pAnnot) {
        xWriteObject(args, *pAnnot, ostr);
        pAnnot.Reset();
        pAnnot = reader.ReadSeqAnnot(lr, m_pErrors.get());
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessBedRaw(
    const CArgs& /*args*/,
    CNcbiIstream& istr,
    CNcbiOstream& /*ostr*/)
//  ----------------------------------------------------------------------------
{
    CBedReader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    CStreamLineReader lr(istr);
    CRawBedTrack raw;
    while (reader.ReadTrackData(lr, raw)) {
        raw.Dump(cerr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessGtf(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    typedef CGff2Reader::TAnnotList ANNOTS;
    ANNOTS annots;

    if (args["format"].AsString() == "gff2") { // process as plain GFF2
        return xProcessGff2(args, istr, ostr);
    }
    CGtfReader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    if (ShowingProgress()) {
        reader.SetProgressReportInterval(10);
    }
    //TestCanceler canceler;
    //reader.SetCanceler(&canceler);
    reader.ReadSeqAnnots(annots, istr, m_pErrors.get());
    for (CRef<CSeq_annot> it : annots) {
        xPostProcessAnnot(args, *it);
        xWriteObject(args, *it, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessGff3(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    typedef CGff2Reader::TAnnotList ANNOTS;
    ANNOTS annots;

    if (args["format"].AsString() == "gff2") { // process as plain GFF2
        return xProcessGff2(args, istr, ostr);
    }
    CGff3Reader reader(m_iFlags, m_AnnotName, m_AnnotTitle,
                       CReadUtil::AsSeqId, &newStyleMessageListener);
    if (ShowingProgress()) {
        reader.SetProgressReportInterval(10);
    }
    //TestCanceler canceler;
    //reader.SetCanceler(&canceler);
    reader.ReadSeqAnnots(annots, istr, m_pErrors.get());
    for (CRef<CSeq_annot> it : annots) {
        const auto& data = it->GetData();
        if (data.IsFtable()) {
            const auto& features = it->GetData().GetFtable();
            if (features.empty()) {
                continue;
            }
            auto pLocationMerger = reader.GetLocationMerger();
            xPostProcessAnnot(args, *it, pLocationMerger.get());
        }
        else {
            xPostProcessAnnot(args, *it);
        }
        xWriteObject(args, *it, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessGff2(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    typedef CGff2Reader::TAnnotList ANNOTS;
    ANNOTS annots;

    CGff2Reader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    reader.ReadSeqAnnots(annots, istr, m_pErrors.get());
    for (CRef<CSeq_annot> cit : annots) {
        xWriteObject(args, *cit, ostr);
    }
}

/*//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessHgvs(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    CHgvsReader reader;
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (CRef<CSeq_annot> cit : annots) {
        xWriteObject(args, *cit, ostr);
    }
}*/

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessGvf(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    typedef CGff2Reader::TAnnotList ANNOTS;
    ANNOTS annots;

    if (args["format"].AsString() == "gff2") { // process as plain GFF2
        return xProcessGff2(args, istr, ostr);
    }
    if (args["format"].AsString() == "gff3") { // process as plain GFF3
        return xProcessGff3(args, istr, ostr);
    }
    CGvfReader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    if (ShowingProgress()) {
        reader.SetProgressReportInterval(10);
    }
    //TestCanceler canceler;
    //reader.SetCanceler(&canceler);
    reader.ReadSeqAnnots(annots, istr, m_pErrors.get());
    for (CRef<CSeq_annot> cit : annots) {
        xWriteObject(args, *cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessNewick(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    while (!istr.eof()) {
        unique_ptr<TPhyTreeNode>  pTree(ReadNewickTree(istr));
        CRef<CBioTreeContainer> btc = MakeDistanceSensitiveBioTreeContainer(
            pTree.get());
        xWriteObject(args, *btc, ostr);
    }
}


//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessAgp(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    CAgpToSeqEntry reader(m_iFlags);

    const int iErrCode = reader.ReadStream(istr);
    if( iErrCode != 0 ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "AGP reader failed with code " +
            NStr::NumericToString(iErrCode), 0 );
    }

    NON_CONST_ITERATE (CAgpToSeqEntry::TSeqEntryRefVec, it, reader.GetResult()) {
        xWriteObject(args, **it, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcess5ColFeatTable(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    if (!istr) {
        return;
    }
    CFeature_table_reader reader(m_iFlags);
    CRef<ILineReader> pLineReader = ILineReader::New(istr);
    while(!pLineReader->AtEOF()) {
        CRef<CSeq_annot> pSeqAnnot =
            reader.ReadSeqAnnot(*pLineReader, m_pErrors.get());
        if( pSeqAnnot &&
            pSeqAnnot->IsFtable() &&
            !pSeqAnnot->GetData().GetFtable().empty()) {
            xWriteObject(args, *pSeqAnnot, ostr);
        }
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessRmo(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    CRepeatMaskerReader reader;
    CRef<ILineReader> pLineReader = ILineReader::New(istr);
    while(istr) {
        CRef<CSeq_annot> pSeqAnnot =
            reader.ReadSeqAnnot(*pLineReader, m_pErrors.get());
        if( ! pSeqAnnot || ! pSeqAnnot->IsFtable() ||
            pSeqAnnot->GetData().GetFtable().empty() )
        {
            // empty annot
            break;
        }
        xWriteObject(args, *pSeqAnnot, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessFasta(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    CStreamLineReader line_reader(istr);

    CFastaReader reader(line_reader, m_iFlags);
    auto maxIdLength = args["max-id-length"].AsInteger();
    if (maxIdLength != 0) {
        reader.SetMaxIDLength(maxIdLength);
    }

    CRef<CSeq_entry> pSeqEntry = reader.ReadSeqEntry(line_reader, m_pErrors.get());
    xWriteObject(args, *pSeqEntry, ostr);
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xProcessAlignment(
    const CArgs& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    CFastaReader::TFlags fFlags = 0;
    if( args["parse-mods"] ) {
        fFlags |= CFastaReader::fAddMods;
    }
    CAlnReader reader(istr);
    reader.SetAlphabet(CAlnReader::eAlpha_Protein);
    if (args["aln-alphabet"].AsString() == "nuc") {
        reader.SetAlphabet(CAlnReader::eAlpha_Nucleotide);
    }
    try {
        CAlnReader::EReadFlags flags =
            (args["all-ids-as-local"].AsBoolean() ?
                CAlnReader::fGenerateLocalIDs :
                CAlnReader::fReadDefaults);
        reader.Read(flags, m_pErrors.get());
        CRef<CSeq_entry> pEntry = reader.GetSeqEntry(fFlags, m_pErrors.get());
        if (pEntry) {
            xWriteObject(args, *pEntry, ostr);
        }
    }
    catch (std::exception&) {
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xSetFormat(
    const CArgs& args,
    CNcbiIstream& istr )
//  ----------------------------------------------------------------------------
{
    m_uFormat = CFormatGuess::eUnknown;
    string format = args["format"].AsString();
    const string& strProgramName = GetProgramDisplayName();

    if (NStr::StartsWith(strProgramName, "wig") || format == "wig" ||
        format == "wiggle" || format == "bedgraph") {
        m_uFormat = CFormatGuess::eWiggle;
        return;
    }
    if (NStr::StartsWith(strProgramName, "bed") || format == "bed") {
        m_uFormat = CFormatGuess::eBed;
        return;
    }
    if (NStr::StartsWith(strProgramName, "b15") || format == "bed15" ||
        format == "microarray") {
        m_uFormat = CFormatGuess::eBed15;
        return;
    }
    if (NStr::StartsWith(strProgramName, "gtf") || format == "gtf") {
        m_uFormat = CFormatGuess::eGtf;
        return;
    }
    if (NStr::StartsWith(strProgramName, "gtf") || format == "augustus") {
        m_uFormat = CFormatGuess::eGffAugustus;
        return;
    }
    if (NStr::StartsWith(strProgramName, "gff3") || format == "gff3") {
        m_uFormat = CFormatGuess::eGff3;
        return;
    }
    if (NStr::StartsWith(strProgramName, "gff2") || format =="gff2") {
        m_uFormat = CFormatGuess::eGff2;
        return;
    }
    if (NStr::StartsWith(strProgramName, "agp")) {
        m_uFormat = CFormatGuess::eAgp;
        return;
    }

    if (NStr::StartsWith(strProgramName, "newick") ||
            format == "newick" || format == "tree" || format == "tre") {
        m_uFormat = CFormatGuess::eNewick;
        return;
    }
    if (NStr::StartsWith(strProgramName, "gvf") || format == "gvf") {
        m_uFormat = CFormatGuess::eGvf;
        return;
    }
    if (NStr::StartsWith(strProgramName, "aln") || format == "align" ||
            format == "aln") {
        m_uFormat = CFormatGuess::eAlignment;
        return;
    }
    if (NStr::StartsWith(strProgramName, "hgvs") ||  format == "hgvs") {
        m_uFormat = CFormatGuess::eHgvs;
        return;
    }
    if( NStr::StartsWith(strProgramName, "fasta") ||  format == "fasta" ) {
        m_uFormat = CFormatGuess::eFasta;
        return;
    }
    if( NStr::StartsWith(strProgramName, "feattbl") || format == "5colftbl" ) {
        m_uFormat = CFormatGuess::eFiveColFeatureTable;
        return;
    }
    if( NStr::StartsWith(strProgramName, "vcf") || format == "vcf" ) {
        m_uFormat = CFormatGuess::eVcf;
        return;
    }
    if( NStr::StartsWith(strProgramName, "ucsc")  ||  format == "ucsc" ) {
        m_uFormat = CFormatGuess::eUCSCRegion;
        return;
    }
    if ( NStr::StartsWith(strProgramName, "psl")  ||  format == "psl" ) {
        m_uFormat = CFormatGuess::ePsl;
        return;
    }
    if (m_uFormat == CFormatGuess::eUnknown) {
        m_uFormat = CFormatGuess::Format(istr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xSetFlags(
    const CArgs& args,
    const string& filename )
//  ----------------------------------------------------------------------------
{
    CNcbiIfstream istr(filename);
    xSetFlags(args, istr);
    istr.close();
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xSetFlags(
    const CArgs& args,
    CNcbiIstream& istr)
//  ----------------------------------------------------------------------------
{
    if (m_uFormat == CFormatGuess::eUnknown) {
        xSetFormat(args, istr);
    }

    m_AnnotName = args["name"].AsString();
    m_AnnotTitle = args["title"].AsString();
    m_bCheckOnly = args["checkonly"];
    m_bXmlMessages = args["xmlmessages"];

    switch( m_uFormat ) {

    case CFormatGuess::eWiggle:
        m_iFlags = NStr::StringToInt(
            args["flags"].AsString(), NStr::fConvErr_NoThrow, 16 );
        if ( args["join-same"] ) {
            m_iFlags |= CWiggleReader::fJoinSame;
        }
        //by default now. But still tolerate if explicitly specified.
        if (!args["as-real"]) {
            m_iFlags |= CWiggleReader::fAsByte;
        }
        if ( args["as-graph"] ) {
            m_iFlags |= CWiggleReader::fAsGraph;
        }

        if ( args["raw"] ) {
            m_iFlags |= CReaderBase::fAsRaw;
        }
        break;

    case CFormatGuess::eBed:
        m_iFlags = NStr::StringToInt(
            args["flags"].AsString(), NStr::fConvErr_NoThrow, 16 );
        if ( args["all-ids-as-local"] ) {
            m_iFlags |= CBedReader::fAllIdsAsLocal;
        }
        if ( args["numeric-ids-as-local"] ) {
            m_iFlags |= CBedReader::fNumericIdsAsLocal;
        }
        if ( args["raw"] ) {
            m_iFlags |= CReaderBase::fAsRaw;
        }
        if ( args["3ff"] ) {
            m_iFlags |= CBedReader::fThreeFeatFormat;
        }
        if ( args["dfm"] ) {
            m_iFlags |= CBedReader::fDirectedFeatureModel;
        }
        break;

    case CFormatGuess::eGtf:
        m_iFlags = NStr::StringToInt(
            args["flags"].AsString(), NStr::fConvErr_NoThrow, 16 );
        if ( args["all-ids-as-local"] ) {
            m_iFlags |= CBedReader::fAllIdsAsLocal;
        }
        if ( args["numeric-ids-as-local"] ) {
            m_iFlags |= CBedReader::fNumericIdsAsLocal;
        }
        if ( args["child-links"] ) {
            m_iFlags |= CGtfReader::fGenerateChildXrefs;
        }
        if (args["genbank-no-locus-tags"]) {
            m_iFlags |= CGtfReader::fGenbankMode;
        }
        if (args["genbank"]) {
            m_iFlags |= CGtfReader::fGenbankMode;
            if (args["locus-tag"]) {
                m_iFlags |= CGtfReader::fRetainLocusIds;
            }
        }
        break;

    case CFormatGuess::eGff3:
        m_iFlags = NStr::StringToInt(
            args["flags"].AsString(), NStr::fConvErr_NoThrow, 16 );
        if ( args["gene-xrefs"] ) {
            m_iFlags |= CGff3Reader::fGeneXrefs;
        }
        if (args["genbank-no-locus-tags"]) {
            m_iFlags |= CGff3Reader::fGeneXrefs;
            m_iFlags |= CGtfReader::fGenbankMode;
        }
        if ( args["genbank"] ) {
            m_iFlags |= CGff3Reader::fGeneXrefs;
            m_iFlags |= CGff3Reader::fGenbankMode;
            if (args["locus-tag"]) {
                m_iFlags |= CGff3Reader::fRetainLocusIds;
            }
        }
        break;

    case CFormatGuess::eFasta: {
        auto flagsStr = args["flags"].AsString();
        m_iFlags = (CFastaReader::fNoSplit | CFastaReader::fDisableParseRange);
        if( args["parse-mods"] ) {
            m_iFlags |= CFastaReader::fAddMods;
        }
        if( args["parse-gaps"] ) {
            m_iFlags |= CFastaReader::fParseGaps;
        }

        try {
            m_iFlags |= NStr::StringToInt(flagsStr, 0, 16);
        }
        catch(const CStringException&) {
            list<string> stringFlags;
            NStr::Split(flagsStr, ",", stringFlags);
            CFastaReader::AddStringFlags(stringFlags, m_iFlags);
        }
        break;
    }

    case CFormatGuess::eFiveColFeatureTable: {
        auto flagsStr = args["flags"].AsString();
        try {
            m_iFlags |= NStr::StringToInt(flagsStr, 0, 16);
        }
        catch (const CStringException&) {
            list<string> stringFlags;
            NStr::Split(flagsStr, ",", stringFlags);
            CFeature_table_reader::AddStringFlags(stringFlags, m_iFlags);
        }
        break;
    }

    default:
        m_iFlags = NStr::StringToInt(
            args["flags"].AsString(), NStr::fConvErr_NoThrow, 16 );
        break;
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xPostProcessAnnot(
    const CArgs& args,
    CSeq_annot& annot,
    const CGff3LocationMerger* pLocationMerger)
    //  ----------------------------------------------------------------------------
{
    static unsigned int startingLocusTagNumber = 1;
    static unsigned int startingFeatureId = 1;

    if (!args["genbank"].AsBoolean() && !args["genbank-no-locus-tags"].AsBoolean()) {
        if (args["cleanup"]) {
            CCleanup cleanup;
            cleanup.BasicCleanup(annot);
        }
        return;
    }

    // all other processing only applies to feature tables
    if (!annot.GetData().IsFtable()) {
        return;
    }

    string prefix, offset;
    if (NStr::SplitInTwo(args["locus-tag"].AsString(), "_", prefix, offset)) {
        int tail = NStr::StringToNonNegativeInt(offset);
        if (tail != -1) {
            startingLocusTagNumber = tail;
        }
        else {
            if (!offset.empty()) {
                //bads news
                NCBI_THROW2(CObjReaderParseException, eFormat,
                    "Invalid locus tag: Only one \"_\", and suffix must be numeric", 0);
            }
        }
    }
    else {
        prefix = args["locus-tag"].AsString();
    }

    edit::CFeatTableEdit fte(
        annot, 0, prefix, startingLocusTagNumber, startingFeatureId, m_pErrors.get());
    fte.InferPartials();
    fte.GenerateMissingParentFeatures(args["euk"].AsBoolean(), pLocationMerger);
    if (args["genbank"].AsBoolean() && !fte.AnnotHasAllLocusTags()) {
        if (!prefix.empty()) {
            fte.GenerateLocusTags();
        }
        else {
            AutoPtr<ILineError> line_error_p =
                sCreateSimpleMessage(
                    eDiag_Fatal, "Need prefix to generate missing locus tags but none was provided");
            this->WriteMessageImmediately(cerr, *line_error_p);
            throw(0);
        }
    }
    fte.GenerateProteinAndTranscriptIds();
    //fte.InstantiateProducts();
    fte.ProcessCodonRecognized();
    fte.EliminateBadQualifiers();
    fte.SubmitFixProducts();

    startingLocusTagNumber = fte.PendingLocusTagNumber();
    startingFeatureId = fte.PendingFeatureId();

    CCleanup cleanup;
    cleanup.BasicCleanup(annot);
}


//  ----------------------------------------------------------------------------
AutoPtr<ILineError> CMultiReaderApp::sCreateSimpleMessage(
    EDiagSev eDiagSev, const string & msg)
//  ----------------------------------------------------------------------------
{
    // For creating quick messages generated by CMultiReaderApp itself
    class CLineErrorForMsg : public CLineError
    {
    public:
        CLineErrorForMsg(EDiagSev eDiagSev, const string & msg)
            : CLineError(
                CLineError::eProblem_GeneralParsingError,
                eDiagSev,
                kEmptyStr, 0, kEmptyStr, kEmptyStr, kEmptyStr,
                NStr::TruncateSpaces(msg),
                CLineError::TVecOfLines()) { }
    };
    return AutoPtr<ILineError>(new CLineErrorForMsg(eDiagSev, msg));
}


//  ----------------------------------------------------------------------------
void CMultiReaderApp::WriteMessageImmediately(
    ostream & ostr, const ILineError & line_error)
//  ----------------------------------------------------------------------------
{
    // For example, progress messages and fatal errors should be written
    // immediately.
    if( m_bXmlMessages ) {
        line_error.DumpAsXML(ostr);
    } else {
        line_error.Dump(ostr);
    }
    ostr.flush();
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xWriteObject(
    const CArgs & args,
    CSerialObject& object,                  // potentially modified by mapper
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    if (m_pMapper.get()) {
        m_pMapper->MapObject(object);
    }
    if (m_bCheckOnly) {
        return;
    }
    const string out_format = args["out-format"].AsString();
    unique_ptr<MSerial_Format> pOutFormat;
    if( out_format == "asn_text" ) {
        pOutFormat.reset( new MSerial_Format_AsnText );
    } else if( out_format == "asn_binary" ) {
        pOutFormat.reset( new MSerial_Format_AsnBinary );
    } else if( out_format == "xml" ) {
        pOutFormat.reset( new MSerial_Format_Xml );
    } else if( out_format == "json" ) {
        pOutFormat.reset( new MSerial_Format_Json );
    } else {
        NCBI_USER_THROW_FMT("Unsupported out-format: " << out_format);
    }
    ostr << *pOutFormat << object;
    ostr.flush();
}

//  ----------------------------------------------------------------------------
void
CMultiReaderApp::xSetMapper(
    const CArgs& args)
//  ----------------------------------------------------------------------------
{
    string strBuild = args["genome"].AsString();
    string strMapFile = args["mapfile"].AsString();

    if (strBuild.empty() && strMapFile.empty()) {
        return;
    }


    if (!strMapFile.empty()) {
        const bool localOnly=false;
        CNcbiIfstream* pMapFile = new CNcbiIfstream(strMapFile);
        m_pMapper.reset(
            new CIdMapperConfig(*pMapFile, strBuild, false, localOnly, m_pErrors.get()));
    }
    else {
        m_pMapper.reset(new CIdMapperBuiltin(strBuild, false, m_pErrors.get()));
    }
}

//  ----------------------------------------------------------------------------
void
CMultiReaderApp::xSetMessageListener(
    const CArgs& args )
//  ----------------------------------------------------------------------------
{

    //
    //  By default, allow all errors up to the level of "warning" but nothing
    //  more serious. -strict trumps everything else, -lenient is the second
    //  strongest. In the absence of -strict and -lenient, -max-error-count and
    //  -max-error-level become additive, i.e. both are enforced.
    //
    if ( args["noerrors"] ) {   // not using error policy at all
        return;
    }
    m_showingProgress = args["show-progress"];

    if ( args["strict"] ) {
        m_pErrors.reset(new CMessageListenerStrict());
    } else if ( args["lenient"] ) {
        m_pErrors.reset(new CMessageListenerLenient());
    } else {
        int iMaxErrorCount = args["max-error-count"].AsInteger();
        int iMaxErrorLevel = eDiag_Warning;
        string strMaxErrorLevel = args["max-error-level"].AsString();
        if ( strMaxErrorLevel == "info" ) {
            iMaxErrorLevel = eDiag_Info;
        }
        else if ( strMaxErrorLevel == "error" ) {
            iMaxErrorLevel = eDiag_Error;
        }

        if ( iMaxErrorCount == -1 ) {
            m_pErrors.reset(
                new CMyMessageListenerCustomLevel(iMaxErrorLevel, *this));
        } else {
            m_pErrors.reset(
                new CMyMessageListenerCustom(
                    iMaxErrorCount, iMaxErrorLevel, *this));
        }
    }
    // if progress requested, wrap the m_pErrors so that progress is shown
    if (ShowingProgress()) {
        m_pErrors->SetProgressOstream( &cerr );
    }
}

//  ----------------------------------------------------------------------------
void CMultiReaderApp::xDumpErrors(
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    if (m_pErrors && m_pErrors->Count() > 0 ) {
        if( m_bXmlMessages ) {
            m_pErrors->DumpAsXML(ostr);
        } else {
            m_pErrors->Dump(ostr);
        }
    }
}

//  ----------------------------------------------------------------------------
int main(int argc, const char* argv[])
//  ----------------------------------------------------------------------------
{
    // this code converts single argument into multiple, just to simplify testing
    list<string>        split_args;
    vector<const char*> new_argv;

    if (argc==2 && argv && argv[1] && strchr(argv[1], ' '))
    {
        NStr::Split(argv[1], " ", split_args, NStr::fSplit_CanEscape | NStr::fSplit_CanQuote | NStr::fSplit_Truncate | NStr::fSplit_MergeDelimiters);

        argc = 1 + split_args.size();
        new_argv.reserve(argc);
        new_argv.push_back(argv[0]);
        for (auto& s : split_args) {
            new_argv.push_back(s.c_str());
            #ifdef _DEBUG
            std::cerr << s.c_str() << " ";
            #endif
        }
        std::cerr << "\n";

        argv = new_argv.data();
    }

    // Execute main application function
    return CMultiReaderApp().AppMain(argc, argv);
}
