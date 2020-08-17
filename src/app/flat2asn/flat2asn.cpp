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

#include <objtools/flatfile/ftacpp.hpp>

#include <serial/objostrasn.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objtools/flatfile/index.h>

#include <objtools/flatfile/ftamain.h>
#include <objtools/flatfile/fta_parse_buf.h>
#include <objtools/flatfile/ftanet.h>

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "flat2asn.cpp"

std::vector<ncbi::CRef<ncbi::CArgValue> > all_args;

/* Letters in use: "abcdefghij lm o qrstuvw yz  CDEFG I  L NO Q S UV XY "
 * Letters free  : "          k  n p       x  AB     H JK M  P R T  W  Z"
 */

Int2      atag;
Int2      ctag;
Int2      Dtag;
Int2      ftag;
Int2      stag;
Int2      Stag;
Int2      Ltag;
Int2      Ntag;
Int2      utag;
Int2      Otag;
Int2      btag;
Int2      htag;
Int2      rtag;
Int2      vtag;
Int2      Etag;
Int2      ttag;
Int2      ztag;
Int2      Itag;
Int2      Vtag;
Int2      Ftag;
Int2      jtag;
Int2      mtag;
Int2      Ytag;
Int2      wtag;
Int2      Xtag;
Int2      etag;
Int2      gtag;
Int2      itag;
Int2      otag;
Int2      ltag;
Int2      Gtag;
Int2      Ctag;
Int2      dtag;
Int2      Utag;
Int2      qtag;
Int2      Qtag;
Int2      ytag;

/**********************************************************/
static bool CheckArgs(short* flag, Char ch)
{
    size_t i = 0;
    size_t sz = all_args.size();

    for (; i < sz; ++i)
    {
        if (all_args[i]->GetName()[0] == ch)
            break;
    }

    if (i < sz)
    {
        *flag = static_cast<Int2>(i);
        return true;
    }

    ErrPostEx(SEV_FATAL, ERR_ZERO, "Program error looking for arg \"%c\"", ch);
    return false;
}

/**********************************************************/
static bool SetArgTag(void)
{
    if(CheckArgs(&atag, 'a') == false || CheckArgs(&ctag, 'c') == false ||
       CheckArgs(&ftag, 'f') == false || CheckArgs(&stag, 's') == false ||
       CheckArgs(&Dtag, 'D') == false || CheckArgs(&utag, 'u') == false ||
       CheckArgs(&htag, 'h') == false || CheckArgs(&rtag, 'r') == false ||
       CheckArgs(&ttag, 't') == false || CheckArgs(&Otag, 'O') == false ||
       CheckArgs(&ztag, 'z') == false || CheckArgs(&btag, 'b') == false ||
       CheckArgs(&vtag, 'v') == false || CheckArgs(&Etag, 'E') == false ||
       CheckArgs(&Ltag, 'L') == false || CheckArgs(&Ntag, 'N') == false ||
       CheckArgs(&Stag, 'S') == false || CheckArgs(&Itag, 'I') == false ||
       CheckArgs(&Vtag, 'V') == false || CheckArgs(&Ftag, 'F') == false ||
       CheckArgs(&jtag, 'j') == false || CheckArgs(&mtag, 'm') == false ||
       CheckArgs(&Ytag, 'Y') == false || CheckArgs(&wtag, 'w') == false ||
       CheckArgs(&Xtag, 'X') == false || CheckArgs(&etag, 'e') == false ||
       CheckArgs(&gtag, 'g') == false || CheckArgs(&itag, 'i') == false ||
       CheckArgs(&otag, 'o') == false || CheckArgs(&ltag, 'l') == false ||
       CheckArgs(&Gtag, 'G') == false || CheckArgs(&Ctag, 'C') == false ||
       CheckArgs(&dtag, 'd') == false || CheckArgs(&Utag, 'U') == false ||
       CheckArgs(&qtag, 'q') == false || CheckArgs(&Qtag, 'Q') == false ||
       CheckArgs(&ytag, 'y') == false)
        return false;
    return true;
}

/**********************************************************/
static bool TransArgs(ParserPtr pp)
{
    bool retval = true;

    /* Ignore multiple tokens in DDBJ's VERSION line
     */
    pp->ign_toks = all_args[ctag]->AsBoolean();

    /* Do not reject records if protein Seq-ids are from sources
     * that differ from that of the "-s" argument
     */
    pp->ign_prot_src = all_args[jtag]->AsBoolean();

    if (all_args[mtag]->AsInteger() == 1)
        pp->mode = FTA_HTGS_MODE;
    else if (all_args[mtag]->AsInteger() == 2)
        pp->mode = FTA_HTGSCON_MODE;
    else
        pp->mode = FTA_RELEASE_MODE;

    /* replace update by currdate
     */
    pp->date = all_args[Ltag]->AsBoolean();

    pp->allow_uwsec = all_args[wtag]->AsBoolean();
    pp->convert = true;
    pp->seg_acc = all_args[Itag]->AsBoolean();
    pp->no_date = all_args[Ntag]->AsBoolean();
    pp->diff_lt = all_args[Ytag]->AsBoolean();
    pp->xml_comp = all_args[Xtag]->AsBoolean();
    pp->sp_dt_seq_ver = all_args[etag]->AsBoolean();
    pp->simple_genes = all_args[Gtag]->AsBoolean();

    /* do not sort
     */
    pp->sort = !all_args[Stag]->AsBoolean();

    pp->debug = all_args[Dtag]->AsBoolean();
    pp->segment = all_args[Etag]->AsBoolean();
    pp->accver = all_args[Vtag]->AsBoolean();
    pp->histacc = !all_args[Ftag]->AsBoolean();

    pp->transl = all_args[ttag]->AsBoolean();
    pp->entrez_fetch = all_args[ztag]->AsBoolean() ? 1 : 0;
    pp->taxserver = all_args[Otag]->AsBoolean() ? 0 : 1;
    pp->medserver = all_args[utag]->AsBoolean() ? 0 : 1;
    pp->ign_bad_qs = false;
    pp->cleanup = all_args[Ctag]->AsInteger();
    pp->allow_crossdb_featloc = all_args[dtag]->AsBoolean();
    pp->genenull = all_args[Utag]->AsBoolean();
    pp->qsfile = all_args[qtag]->AsString().c_str();
    if (*pp->qsfile == 0)
        pp->qsfile = NULL;

    pp->qamode = all_args[Qtag]->AsBoolean();

    if (all_args[ytag]->AsString() == "Bioseq-set")
        pp->output_format = FTA_OUTPUT_BIOSEQSET;
    else if (all_args[ytag]->AsString() == "Seq-submit")
        pp->output_format = FTA_OUTPUT_SEQSUBMIT;
    else
    {
        ErrPostEx(SEV_FATAL, ERR_ZERO,
                  "Sorry, the format of output file is not supported. Available are \"Bioseq-set\" and \"Seq-submit\".");
        retval = false;
    }

    fta_fill_find_pub_option(pp, all_args[htag]->AsBoolean(), all_args[rtag]->AsBoolean());

    std::string format = all_args[ftag]->AsString(),
                source = all_args[stag]->AsString();

    pp->all = all_args[atag]->AsBoolean() ? ParFlat_ALL : 0;

    if (!fta_set_format_source(*pp, format, source)) {
        retval = false;
    }

    return(retval);
}

/**********************************************************/
static bool OpenFiles(ParserPtr pp)
{
    Char      str[1000];

    FILE* fd;

    Char      tfile[PATH_MAX];
    bool      delin;
    bool      delout;

    delin = false;
    delout = true;

    std::string infile = all_args[itag]->AsString();
    if (infile == "stdin")
    {
        delin = true;
        tmpnam(tfile);
        fd = fopen(tfile, "w");
        while(fgets(str, 999, stdin) != NULL)
            fprintf(fd, "%s", str);

        fclose(fd);

        infile = tfile;
    }

    pp->outfile = all_args[otag]->AsString();

    if (pp->outfile == "stdout")
        delout = false;

    std::string logfile = all_args[ltag]->AsString();

#ifdef WIN32
    pp->ifp = fopen(infile.c_str(), "rb");
#else
    pp->ifp = fopen(infile.c_str(), "r");
#endif

    if(delin)
        unlink(infile.c_str());

    if(pp->ifp == NULL)
    {
        ErrPostEx(SEV_FATAL, ERR_ZERO,
                  "Open input flatfile %s failed.", infile.c_str());
        return false;
    }

    if(all_args[btag]->AsBoolean())
        pp->output_binary = true;

    if(pp->qsfile != NULL)
    {
        pp->qsfd = fopen(pp->qsfile, "rb");
        if(pp->qsfd == NULL)
        {
            ErrPostEx(SEV_FATAL, ERR_ZERO,
                      "Open Quality Scores file %s failed.", pp->qsfile);
            fclose(pp->ifp);
            pp->ifp = NULL;

            if(delout)
                unlink(pp->outfile.c_str());
            return false;
        }
    }

    if (logfile.empty())
        return true;

    if (!ErrSetLog(logfile.c_str()))
    {
        ErrPostEx(SEV_FATAL, ERR_ZERO, "Open logfile %s failed.",
                  logfile.c_str());
        fclose(pp->ifp);

        pp->ifp = NULL;
        if(pp->qsfd != NULL)
        {
            fclose(pp->qsfd);
            pp->qsfd = NULL;
        }
        if(delout)
            unlink(pp->outfile.c_str());
        return false;
    }

    if (logfile == "stderr")
        ErrSetMessageLevel(SEV_MAX);
    else
        ErrSetMessageLevel(SEV_WARNING);

    ErrSetOptFlags(EO_SHOW_CODES);
    ErrSetOptFlags(EO_LOG_USRFILE);
    ErrSetOptFlags(all_args[vtag]->AsBoolean() ? EO_MSG_MSGTEXT : 0);
    ErrSetOptFlags(EO_LOG_FILELINE);

    return true;
}

/**********************************************************/
static ParserPtr ParseArgs(char* pgmname)
{
    ParserPtr pp = new Parser;

    /* As of June, 2004 sequence length limitation removed
     */
    pp->limit = 0;

    if (!SetArgTag() || !TransArgs(pp))
    {
        delete pp;
        return(NULL);
    }

    if(!OpenFiles(pp))
    {
        delete pp;
        return(NULL);
    }

    if(pp->output_format == FTA_OUTPUT_BIOSEQSET)
        ;
    else if(pp->output_format == FTA_OUTPUT_SEQSUBMIT)
        ;
    else
    {
        delete pp;
        return(NULL);
    }

    return(pp);
}

class CFlat2AsnApp : public ncbi::CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};

void CFlat2AsnApp::Init()
{
    std::auto_ptr<ncbi::CArgDescriptions> arg_descrs(new ncbi::CArgDescriptions);
    arg_descrs->Delete("h");

    arg_descrs->AddDefaultKey("i", "InputFlatfile", "Input flatfile to parse", ncbi::CArgDescriptions::eString, "stdin");
    arg_descrs->AddDefaultKey("o", "OutputAsnFile", "Output ASN.1 file", ncbi::CArgDescriptions::eString, "stdout");

    arg_descrs->AddDefaultKey("l", "LogFile", "Log file", ncbi::CArgDescriptions::eString, "");

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

    SetupArgDescriptions(arg_descrs.release());
}

int CFlat2AsnApp::Run()
{
    char*   pgmname = (char *) "flat2asn Revision: 1.3 ";
    ParserPtr pp;

    ErrSetFatalLevel((ErrSev)(SEV_FATAL + 1)); /* don't die on me */
    ErrSetOptFlags(EO_MSG_CODES);
    ErrSetMessageLevel(SEV_WARNING);

    all_args = GetArgs().GetAll();
    pp = ParseArgs(pgmname);
    if (pp == NULL)
    {
        FtaErrFini();
        return(1);
    }

    Int2 ret = fta_main(pp, false);
    FtaErrFini();
    return ret;
}

int main(int argc, const char* argv[])
{
    return CFlat2AsnApp().AppMain(argc, argv);
}
