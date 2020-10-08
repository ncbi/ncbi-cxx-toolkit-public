/* flat2asn.cpp
 *
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
 * File Name:  flat2asn.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Main routines for parsing flat files to ASN.1 file format.
 * Available flat file format are GENBANK (LANL), EMBL, SWISS-PROT.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

#include <serial/objostrasn.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/fta_parse_buf.h>
#include <objtools/logging/listener.hpp>

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "flat2asn.cpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

using TConfig = Parser;

/**********************************************************/
static bool InitConfig(const CArgs& args, Parser& config)
{
    bool retval = true;

    /* Ignore multiple tokens in DDBJ's VERSION line
     */
    config.ign_toks = args["c"].AsBoolean();

    /* Do not reject records if protein Seq-ids are from sources
     * that differ from that of the "-s" argument
     */
    config.ign_prot_src = args["j"].AsBoolean();

    if (args["m"].AsInteger() == 1)
        config.mode = Parser::EMode::HTGS;
    else if (args["m"].AsInteger() == 2)
        config.mode = Parser::EMode::HTGSCON;
    else
        config.mode = Parser::EMode::Release;

    /* replace update by currdate
     */
    config.date = args["L"].AsBoolean();

    config.allow_uwsec = args["w"].AsBoolean();
    config.convert = true;
    config.seg_acc = args["I"].AsBoolean();
    config.no_date = args["N"].AsBoolean();
    config.diff_lt = args["Y"].AsBoolean();
    config.xml_comp = args["X"].AsBoolean();
    config.sp_dt_seq_ver = args["e"].AsBoolean();
    config.simple_genes = args["G"].AsBoolean();

    /* do not sort
     */
    config.sort = !args["S"].AsBoolean();

    config.debug = args["D"].AsBoolean();
    config.segment = args["E"].AsBoolean();
    config.accver = args["V"].AsBoolean();
    config.histacc = !args["F"].AsBoolean();

    config.transl = args["t"].AsBoolean();
    config.entrez_fetch = args["z"].AsBoolean() ? 1 : 0;
    config.taxserver = args["O"].AsBoolean() ? 0 : 1;
    config.medserver = args["u"].AsBoolean() ? 0 : 1;
    config.ign_bad_qs = false;
    config.cleanup = args["C"].AsInteger();
    config.allow_crossdb_featloc = args["d"].AsBoolean();
    config.genenull = args["U"].AsBoolean();
    config.qsfile = args["q"].AsString().c_str();
    if (*config.qsfile == 0)
        config.qsfile = NULL;

    config.qamode = args["Q"].AsBoolean();

    if (args["y"].AsString() == "Bioseq-set")
        config.output_format = Parser::EOutput::BioseqSet;
    else if (args["y"].AsString() == "Seq-submit")
        config.output_format = Parser::EOutput::Seqsubmit;

    config.fpo.always_look = config.fpo.replace_cit = !args["h"].AsBoolean();

    std::string format = args["f"].AsString(),
                source = args["s"].AsString();

    config.all = args["a"].AsBoolean();

   if (!fta_set_format_source(config, format, source)) {
        retval = false;
    }

    return(retval);
}

/**********************************************************/
static bool OpenFiles(const CArgs& args, TConfig& config, IObjtoolsListener& listener)
{
    Char      str[1000];
    bool delin=false;

    std::string infile = args["i"].AsString();
    if (infile == "stdin")
    {
        infile = CDirEntry::GetTmpName(CFile::eTmpFileCreate);
        delin = true;
        auto fd = fopen(infile.c_str(), "w");

        while(fgets(str, 999, stdin) != NULL)
            fprintf(fd, "%s", str);
        fclose(fd);
    }

#ifdef WIN32
    config.ifp = fopen(infile.c_str(), "rb");
#else
    config.ifp = fopen(infile.c_str(), "r");
#endif

    if (delin) {
        CDirEntry de(infile);
        de.Remove();
    }

    if(!config.ifp)
    {
        listener.PutMessage(
                CObjtoolsMessage("Failed to open input flatfile " + infile, eDiag_Fatal));
        return false;
    }


    if(config.qsfile != NULL)
    {
        config.qsfd = fopen(config.qsfile, "rb");
        if(config.qsfd == NULL)
        {
            listener.PutMessage(
                   CObjtoolsMessage("Failed to open Quality Scores file " + string(config.qsfile), eDiag_Fatal)); 

            fclose(config.ifp);
            config.ifp = NULL;
            return false;
        }
    }

    return true;
}

/**********************************************************/
static unique_ptr<TConfig> ParseArgs(const CArgs& args, char* pgmname, IObjtoolsListener& listener)
{
    
    unique_ptr<Parser> pConfig(new TConfig());

    /* As of June, 2004 sequence length limitation removed
     */
    pConfig->limit = 0;

    if (!InitConfig(args, *pConfig))
    {
        return nullptr;
    }

    if(!OpenFiles(args, *pConfig, listener))
    {
        return nullptr;
    }

    return pConfig;
}

class CFlat2AsnApp : public ncbi::CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    
};

void CFlat2AsnApp::Init()
{
    std::auto_ptr<ncbi::CArgDescriptions> arg_descrs(new ncbi::CArgDescriptions);
    arg_descrs->Delete("h");

    arg_descrs->AddDefaultKey("i", "InputFlatfile", "Input flatfile to parse", ncbi::CArgDescriptions::eString, "stdin");

    arg_descrs->AddOptionalKey("o", "OutputAsnFile", "Output ASN.1 file", ncbi::CArgDescriptions::eOutputFile);
    arg_descrs->AddOptionalKey("l", "LogFile", "Log file", ncbi::CArgDescriptions::eOutputFile);

    arg_descrs->AddDefaultKey("a", "ParseRegardlessAccessionPrefix", "Parse all flatfile entries, regardless of accession prefix letter", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("D", "DebugMode", "Debug mode, output everything possible", ncbi::CArgDescriptions::eBoolean, "F");

    arg_descrs->AddKey("f", "FlatfileFormat", "Flatfile format(embl, genbank, pir, prf, sprot, xml)", CArgDescriptions::eString);
    arg_descrs->AddKey("s", "SourceData", "Source of the data file(embl, ddbj, lanl, ncbi, pir, prf, sprot, flybase, refseq)", CArgDescriptions::eString);

    arg_descrs->AddDefaultKey("u", "AvoidMuidLookup", "Avoid MUID lookup", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("h", "AvoidReferencesLookup", "Avoid lookup of references which already have muids", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("r", "AvoidReferencesReplacement", "Avoid replacement of references with MedArch server versions", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("t", "TranslationReplacement", "Replace original translation with parser generated version", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("O", "AvoidOrganismLookup", "Avoid ORGANISM lookup", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("z", "UsePubseq", "Connect to PUBSEQ OS to fetch missing bioseqs", ncbi::CArgDescriptions::eBoolean, "T");
    arg_descrs->AddDefaultKey("b", "BinaryOutput", "ASN.1 output in binary format", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("v", "Verbose", "Display verbose error message on console", ncbi::CArgDescriptions::eBoolean, "F");

    arg_descrs->AddDefaultKey("E", "EmblSegmentedSet", "Treat input EMBL flatfile as a segmented set", ncbi::CArgDescriptions::eBoolean, "F");

    arg_descrs->AddDefaultKey("N", "NoDates", "No update date or current date", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("S", "NoSort", "Don't sort the entries in flatfile", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("L", "ReplaceUpdateDate", "Replace update date from LOCUS line with current date", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("I", "UseAccession", "Use accession for segmented set id", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("V", "ParseAccessionVersion", "Parse with ACCESSION.VERSION identifiers", ncbi::CArgDescriptions::eBoolean, "T");
    arg_descrs->AddDefaultKey("F", "NoPopulate", "Do not populate Seq-inst.hist.replaces with secondary accessions", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("c", "IgnoreMultipleTokens", "Ignore multiple tokens in DDBJ's VERSION line", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("j", "NoReject", "Do not reject records if protein Seq-ids are from sources that differ from that of the \"-s\" argument",
                              ncbi::CArgDescriptions::eBoolean, "F");

    // constraint: 0, 1
    arg_descrs->AddDefaultKey("m", "ParsingMode", "Parsing mode. Values: 0 - RELEASE, 1 - HTGS", ncbi::CArgDescriptions::eInteger, "0");

    arg_descrs->AddDefaultKey("Y", "AllowsInconsistentPair", "Allows inconsistent pairs of /gene+/locus_tag quals, when same genes go along with different locus_tags", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("w", "AllowsUnusualWgsAccessions", "Allows unusual secondary WGS accessions with prefixes not matching the primary one", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("X", "CompatibleMode", "INSDSeq/GenBank/EMBL compatible mode. Please don't use it", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("e", "SwissprotVersion", "For SwissProt \"Reviewed\" records only: parse the sequence version number from \"sequence version\" DT line into Seq-id.version slot", ncbi::CArgDescriptions::eBoolean, "T");
    arg_descrs->AddDefaultKey("g", "AllowsSingleBaseGap", "For ALL sources: allows single-base \"gap\" features (obsolete)", ncbi::CArgDescriptions::eBoolean, "T");
    arg_descrs->AddDefaultKey("G", "SimpleGeneLocations", "Always generate simple gene locations, no joined ones", ncbi::CArgDescriptions::eBoolean, "F");

    // constraint: 0, 1, 2
    arg_descrs->AddDefaultKey("C", "CleanupMode", "Cleanup function to use:\n       0,1 - Toolkit's SeriousSeqEntryCleanup;\n        2 - none.\n   ", ncbi::CArgDescriptions::eInteger, "1");
    
    arg_descrs->AddDefaultKey("d", "AllowsXDb", "Allow cross-database join locations", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("U", "UseNull", "Use NULLs for Gene Features that are not single-interval", ncbi::CArgDescriptions::eBoolean, "T");

    arg_descrs->AddDefaultKey("q", "QualityScoresFilename", "Quality Scores filename", ncbi::CArgDescriptions::eString, "");
    arg_descrs->AddDefaultKey("Q", "QAMode", "QA mode: does not populate top level create-date and\n      NcbiCleanup user-object", ncbi::CArgDescriptions::eBoolean, "F");
    arg_descrs->AddDefaultKey("y", "OutputFormat", "Output format. Possible values: \"Bioseq-set\" and \"Seq-submit\"", ncbi::CArgDescriptions::eString, "Bioseq-set");
    arg_descrs->SetConstraint("y", 
            &(*new  CArgAllow_Strings,
                "Bioseq-set", "Seq-submit"));

    SetupArgDescriptions(arg_descrs.release());
}


class CFlat2AsnListener : public CObjtoolsListener
{
public:
    CFlat2AsnListener(const string& prefix="") : m_Prefix(prefix) {}
    virtual ~CFlat2AsnListener() {}

    bool PutMessage(const IObjtoolsMessage& msg) override {
        if (msg.GetSeverity() >= eDiag_Warning) {
            return CObjtoolsListener::PutMessage(msg);
        }
        return false;
    }

    void Dump(CNcbiOstream& ostr) const override {
        if (m_Prefix.empty()) {
            CObjtoolsListener::Dump(ostr);
        }
        else {
            for (const auto& pMessage : m_Messages) {
                ostr << m_Prefix << " ";
                pMessage->Dump(ostr);
            }
        }
    }
private:
    string m_Prefix;
};


int CFlat2AsnApp::Run()
{
    char*   pgmname = (char *) "flat2asn Revision: 1.3 ";
    const auto& args = GetArgs();

    CNcbiOstream* pLogStream = args["l"] ? 
        &args["l"].AsOutputFile() :
        &NcbiCerr;

    CFlat2AsnListener messageListener(args["l"] ? "" : "[" + CNcbiApplication::GetAppName() + "]");

    auto pConfig = ParseArgs(args, pgmname, messageListener);
    if (!pConfig)
    {
        return 1;
    }

    CFlatFileParser ffparser(&messageListener);
    auto pSerialObject = ffparser.Parse(*pConfig);


    if (messageListener.Count() > 0) {
        messageListener.Dump(*pLogStream);
    }

    if (pSerialObject) {
        CNcbiOstream* pOstream = args["o"] ?
            &args["o"].AsOutputFile() :
            &NcbiCout;
        if (args["b"].AsBoolean()) {
            *pOstream << MSerial_AsnBinary << *pSerialObject;
        } 
        else {
            *pOstream << MSerial_AsnText << *pSerialObject;
        }
        return 0;
    }

    return 1;
}

int main(int argc, const char* argv[])
{
    return CFlat2AsnApp().AppMain(argc, argv);
}
