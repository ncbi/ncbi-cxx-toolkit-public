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
 * Authors:
 *      Victor Sapojnikov
 *
 * File Description:
 *      Validate AGP data. A command line option to choose either context
 *      or GenBank validation. Context validation uses only the information
 *      in the AGP file. GenBank validation queries sequence length and taxid
 *      via ObjectManager or CEntrez2Client.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <objtools/readers/agp_validate_reader.hpp>

#include "AltValidator.hpp"
#include "AgpFastaComparator.hpp"

USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE

CRef<CAgpErrEx> pAgpErr;

class CAgpCompSpanSplitter: public IAgpRowOutput
{
public:
  CNcbiOstream* m_out;
  int part_number;
  string object_name;
  bool comp_or_gap_printed;
  CAgpCompSpanSplitter(CNcbiOstream* out=NULL)
  {
    m_out=out;
    part_number=0;
    comp_or_gap_printed=false;
  }

  virtual void SaveRow(const string& s, CRef<CAgpRow> row, TRangeColl* runs_of_Ns);

  virtual ~CAgpCompSpanSplitter() {}
};

class CAgpValidateApplication : public CNcbiApplication
{
private:
  virtual void Init(void);
  virtual int  Run(void);
  virtual void Exit(void);
  //string Run(const CArgs& args);

  string m_CurrentFileName;
  EAgpVersion m_agp_version;
  bool m_use_xml;

  enum EValidationType {
      VT_Context, VT_Acc=1, VT_Len=2, VT_Taxid=4,
      VT_AccLenTaxid = VT_Acc|VT_Len|VT_Taxid
  } m_ValidationType;

  CMapCompLen m_comp2len;
  TMapStrRangeColl m_comp2range_coll;

  //void x_LoadLen  (CNcbiIstream& istr, const string& filename);
  void x_LoadLenFa(CNcbiIstream& istr, const string& filename);

  CAltValidator* m_AltValidator;
  //CAgpContextValidator* m_ContextValidator;
  CAgpValidateReader m_reader;

  void x_ValidateUsingFiles(const CArgs& args);
  void x_ValidateFile(CNcbiIstream& istr);
  void x_ReportFastaSeqCount();

public:
  CAgpValidateApplication() : m_reader( (pAgpErr.Reset(new CAgpErrEx), *pAgpErr), m_comp2len, m_comp2range_coll)
  {
    m_agp_version=eAgpVersion_auto;
    m_use_xml=false;
  }
};


// Print a nicer usage message
class CArgDesc_agp_validate : public CArgDescriptions
{
public:
  string& PrintUsage(string& str, bool detailed) const
  {
      // use svn's keyword substitution to automatically keep track of
      // versioning.
      string version_str = "$Date$";
      version_str = version_str.substr(7); // chop "$Date: " from beginning
      version_str.resize( version_str.length() - 1 ); // remove final '$'

    str="Validate data in the AGP format:\n"
    "http://www.ncbi.nlm.nih.gov/projects/genome/assembly/agp/AGP_Specification.shtml\n"
    "\n"
    "Version: " + version_str + "\n"
    "\n"
    "USAGE: agp_validate [-options] [FASTA files...] [AGP files...]\n"
    "\n"
    "There are 3 validations modes:\n"
    "no mode option: (default mode) report component, gap, scaffold and object statistics, perform checks\n"
    "                that do not require component sequences to be available in GenBank (see: -list).\n"
    "-alt, -species: Check component Accessions, Lengths and Taxonomy ID using GenBank data;\n"
    "                -species allows components from different subspecies during Taxid checks.\n"
    //"-comp  Check that the supplied object sequences (in FASTA or ASN.1 file) match what can be\n"
    "-comp  Check that the supplied object sequences (in FASTA files) match what can be\n"
    "       constructed from the AGP and the component sequences (in FASTA files or in GenBank).\n"
    "       Run \"agp_validate -comp\" to see the options for this mode.\n"
    "\n"
    "OPTIONS (default and -alt modes):\n"
    "  -g         Check that component names look like Nucleotide accessions\n"
    "             (this does not require components to be in GenBank).\n"
    "  -out FILE  Save the AGP file, adding missing version 1 to the component accessions (need -alt),\n"
    "             or adding gaps where runs of Ns longer than 10 bp are found in components (need FASTA files).\n"
    "  -obj       Use FASTA files to read names and lengths of objects (the default is components).\n"
    "  -v VER     AGP version (1.1 or 2.0). The default is to choose automatically. 2.0 is chosen\n"
    "             when the linkage evidence (column 9) is not empty in the first gap line encountered.\n"
    "  -xml       Report results in XML format.\n"
    "\n"
    "  Extra checks specific to an object type:\n"
    "  -un        Unplaced/unlocalized scaffolds:\n"
    "             any single-component scaffold must use the whole component in orientation '+'\n"
    "  -scaf      Scaffold from component AGP: no scaffold-breaking gaps allowed\n"
    "  -chr       Chromosome from scaffold AGP: ONLY scaffold-breaking gaps allowed\n" //  + -cc
    "  Use both of the last 2 options in this order: -scaf Scaf_AGP_file(s) -chr Chr_AGP_file(s)\n"
    "  to check that all scaffolds in Scaf_AGP_file(s) are wholly included in Chr_AGP_file(s)\n"
    //"  -cc        Chromosome from component: check telomere/centromere/short-arm gap counts per chromosome\n"
    "\n"
    "  -list               List error and warning messages.\n"
    "  -limit COUNT        Print only the first COUNT messages of each type.\n"
    "                      Default=100. To print all, use: -limit 0\n"
    "  -skip, -only WHAT   Skip, or report only a particular error or warning.\n"
    "  -show WHAT          Show the warning hidden by default (w40, w45, w46, w52).\n"
    "  'WHAT' could be a part of the message text, an error code (e11, w22, etc; see -list),\n"
    "  or a keyword: all, warn, err, alt.\n"
    "\n"
    "If component FASTA files are given in front of AGP files, also check that:\n"
    "- component_id from AGP is present in FASTA;\n"
    "- component_end does not exceed sequence length.\n"
    "If FASTA files for objects are given (after -obj), check that:\n"
    "- object_id from AGP is present in FASTA;\n"
    "- object lengths in FASTA and in AGP match.\n"
    "\n"
    ;
    return str;
    // To do: -taxon "taxname or taxid" ?
  }
};

void CAgpValidateApplication::Init(void)
{
  auto_ptr<CArgDesc_agp_validate> arg_desc(new CArgDesc_agp_validate);

  arg_desc->SetUsageContext(
    GetArguments().GetProgramBasename(),
    "Validate AGP data", false);

  // component_id  checks that involve GenBank: Accession Length Taxid
  arg_desc->AddFlag("alt", "");

  arg_desc->AddFlag("g"   , "");
  arg_desc->AddFlag("obj" , "");
  arg_desc->AddFlag("un"  , "");
  arg_desc->AddFlag("scaf", "");
  arg_desc->AddFlag("chr" , "");
  arg_desc->AddFlag("comp", "");
  arg_desc->AddFlag("xml" , "");

  // -comp args
  arg_desc->AddOptionalKey( "loadlog", "FILE",
    "specifies where we write our loading log for -comp",
    CArgDescriptions::eOutputFile);
  arg_desc->AddFlag("ignoreagponly",     "");
  arg_desc->AddFlag("ignoreobjfileonly", "");
  arg_desc->AddDefaultKey( "diffstofind", "", "",
                           CArgDescriptions::eInteger, "0" );

  arg_desc->AddFlag("species", "allow components from different subspecies");

  arg_desc->AddOptionalKey( "out", "FILE",
    "add missing version 1 to component accessions",
    CArgDescriptions::eOutputFile);

  arg_desc->AddOptionalKey( "v", "ver",
    "AGP version",
    CArgDescriptions::eString);

  arg_desc->AddOptionalKey( "skip", "error_or_warning",
    "Message or message code to skip",
    CArgDescriptions::eString,
    CArgDescriptions::fAllowMultiple);

  arg_desc->AddOptionalKey( "only", "error_or_warning",
    "Message or message code to print (hide other)",
    CArgDescriptions::eString,
    CArgDescriptions::fAllowMultiple);

  arg_desc->AddOptionalKey( "show", "error_or_warning",
    "Message or message code to print (if not printed by default)",
    CArgDescriptions::eString,
    CArgDescriptions::fAllowMultiple);

  arg_desc->AddDefaultKey("limit", "ErrorCount",
    "Print at most ErrorCount lines with a particular error",
    CArgDescriptions::eInteger,
    "100");

  arg_desc->AddFlag("list", "all possible errors and warnings");

  // file list for file processing
  arg_desc->AddExtra(0, 10000, "files to be processed",
                      CArgDescriptions::eString
                      //CArgDescriptions::eInputFile
                      );
  // Setup arg.descriptions for this application
  SetupArgDescriptions(arg_desc.release());

}


int CAgpValidateApplication::Run(void)
{
  //// Setup registry, error log, MT-lock for CONNECT library
  CONNECT_Init(&GetConfig());

 //// Get command line arguments
  const CArgs& args = GetArgs();

  if( args["list"].HasValue() ) {
    CAgpErrEx::PrintAllMessages(cout);
    exit(0);
  }

  if( args["xml"].HasValue() ) {
    m_use_xml=true;
    pAgpErr->m_use_xml=true;
    pAgpErr->m_out = &cout;
  }

  m_reader.m_CheckObjLen=args["obj"].HasValue();
  m_reader.m_unplaced   =args["un" ].HasValue();
  if(args["chr" ].HasValue() || args["scaf" ].HasValue()) {
    if( m_reader.m_unplaced  ) {
      cerr << "Error -- cannot specify -un with -chr/-scaf.\n";
      exit(1);
    }
    if( args["alt"].HasValue() || args["species"].HasValue() ) {
      cerr << "Error -- cannot specify -chr/-scaf with -alt/-species.\n";
      exit(1);
    }
  }
  if( args["chr"].HasValue() ) {
    if( args["scaf"].HasValue() ) {
      cerr << "Error -- -scaf and -chr must precede different files.\n";
      exit(1);
    }
    m_reader.m_is_chr=true;
    m_reader.m_explicit_scaf=true;
  }
  else if( args["scaf"].HasValue() ) {
    m_reader.m_explicit_scaf=true;
  }

  if( args["alt"].HasValue() || args["species"].HasValue() ) {
    if(m_reader.m_CheckObjLen) {
      cerr << "Error -- cannot specify -obj with -alt/-species.\n";
      exit(1);
    }
    m_ValidationType = VT_AccLenTaxid;
  }
  else {
    m_ValidationType = VT_Context;
    bool checkCompNames=args["g"].HasValue();
    // m_ContextValidator = new CAgpContextValidator(checkCompNames);
    if(checkCompNames) {
      // also print WGS component_id/component_type mismatches.
      pAgpErr->SkipMsg(CAgpErr::W_CompIsWgsTypeIsNot, true);
      pAgpErr->SkipMsg(CAgpErr::W_CompIsNotWgsTypeIs, true);
      m_reader.m_CheckCompNames=true;
    }

  }
  if(m_ValidationType & VT_Acc) {
    CONNECT_Init(&GetConfig()); // Setup registry, error log, MT-lock for CONNECT library

    m_AltValidator= new CAltValidator(m_ValidationType==VT_AccLenTaxid);
    m_AltValidator->Init();
    if( args["species"].HasValue() ) {
      m_AltValidator->m_SpeciesLevelTaxonCheck = true;
    }
    if( args["out"].HasValue() ) {
      m_AltValidator->m_out = &(args["out"].AsOutputFile());
    }
  }

  const CArgValue::TStringArray* err_warn=NULL;
  bool onlyNotSkip = args["only"].HasValue();
  string action;
  if( args["skip"].HasValue() ) {
     if( onlyNotSkip ) {
       cerr << "Error -- cannot specify both -only and -skip.\n";
       exit(1);
     }
     err_warn = &( args["skip"].GetStringList() );
     action="Skipping messages:\n";
  }
  else if(onlyNotSkip) {
    if( args["show"].HasValue() ) {
      cerr << "Error -- cannot specify both -only and -show; please use multiple -only instead.\n";
      exit(1);
    }

    err_warn = &( args["only"].GetStringList() );
    pAgpErr->SkipMsg("all");
    action="Allowed messages:\n";
  }
  if(err_warn) {
    // Inform pAgpErr what to skip; show messages that we skip.
    bool needHeading=true; // avoid printing >action when not needed
    for( CArgValue::TStringArray::const_iterator it =
      err_warn->begin();  it != err_warn->end(); ++it
    ) {
      string res  = pAgpErr->SkipMsg(*it, onlyNotSkip);
      if(res=="") {
        cerr << "WARNING: no matches for " << *it << "\n";
        needHeading=true;
      }
      else {
        if ( res[0] == ' ' && needHeading) {
          if(needHeading) cerr << action;
          cerr << res;
          needHeading=false;
        }
        else {
          cerr << res << "\n";
          needHeading=true;
        }
      }
    }
  }

  if( args["show"].HasValue() ) {
    err_warn = &( args["show"].GetStringList() );
    for( CArgValue::TStringArray::const_iterator it =
      err_warn->begin();  it != err_warn->end(); ++it
    ) {
      pAgpErr->SkipMsg(*it, true);
    }
  }

  pAgpErr->m_MaxRepeat =
    args["limit"].HasValue() ? args["limit"].AsInteger() : 100;

  if(args["v"].HasValue() ) {
    if( args["v"].AsString()[0]=='1' ) {
      m_agp_version=eAgpVersion_1_1;
    }
    else if( args["v"].AsString()[0]=='2' ) {
      m_agp_version=eAgpVersion_2_0;
    }
    else {
      cerr << "Error -- invalid AGP version after -v. Use 1.1 or 2.0.\n";
      exit(1);
    }
  }
  else {
    m_agp_version=eAgpVersion_auto; // save for CAgpRow; it is default for CAgpValidateReader
  }

  if( ! args["comp"] ) {
      // if "-comp" not specified, neither should the other
      // comp-related args
      if( args["loadlog"] || args["ignoreagponly"] ||
          args["ignoreobjfileonly"] ||
          args["diffstofind"].AsInteger() > 0 )
      {
          cerr << "Error -- -comp mode options without -comp" << endl;
          exit(1);
      }

      //// Process files, print results
      bool taxid_check_failed=false;
      if(m_use_xml) {
        cout << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n<page>\n";
      }
      x_ValidateUsingFiles(args);
      if(m_ValidationType == VT_Context) {
        m_reader.PrintTotals(cout, m_use_xml);
      }
      else if(m_ValidationType & VT_Acc) {
        if(!m_use_xml) cout << "\n";
        if(m_ValidationType & VT_Taxid) taxid_check_failed = !m_AltValidator->CheckTaxids(cout, m_use_xml);
        m_AltValidator->PrintTotals(cout, m_use_xml);
      }
      if(m_use_xml) {
        cout << "</page>\n";
      }
      return pAgpErr->CountTotals(CAgpErrEx::E_Last)>0 || taxid_check_failed ? 2 : 0;
  }
  else {
      // Note: traditional validation (now in the "if" clause above) used to be done regardless of args["comp"].
      // Doing it separately now since it does not yet work properly when both object and component FASTA files are given at the same time.

      list<string> filenames;
      for (unsigned int i = 1; i <= args.GetNExtra(); i++) {
          const string filename = args['#' + NStr::IntToString(i)].AsString();
          if( ! filename.empty() && filename[0] != '-' ) {
              filenames.push_back(filename);
          }
      }

      string comploadlog;
      if( args["loadlog"] ) {
          comploadlog = args["loadlog"].AsString();
      }

      string agp_as_fasta_file;
      if( args["out"] ) {
          agp_as_fasta_file = args["out"].AsString();
      }

      CAgpFastaComparator::TDiffsToHide diffsToHide = 0;
      if( args["ignoreagponly"] ) {
          diffsToHide |= CAgpFastaComparator::fDiffsToHide_AGPOnly;
      }
      if( args["ignoreobjfileonly"] ) {
          diffsToHide |= CAgpFastaComparator::fDiffsToHide_ObjfileOnly;
      }

      int diffsToFind = args["diffstofind"].AsInteger();

      CAgpFastaComparator agpFastaComparator;
      if( CAgpFastaComparator::eResult_Success !=
          agpFastaComparator.Run( filenames, comploadlog,
                                  agp_as_fasta_file, diffsToHide,
                                  diffsToFind) )
      {
          cerr << "AGP/FASTA comparison failed." << endl;
      }
  }

  return 0;
}

void CAgpValidateApplication::x_ReportFastaSeqCount()
{
  string s;
  if(m_comp2len.m_count!=1) s="s";
  if(!m_use_xml) cout<< m_comp2len.m_count << " "
      << (m_reader.m_CheckObjLen?"object name":"component_id")
      << s <<" and length" << s << " loaded from FASTA." << endl;
  if(m_comp2range_coll.size()) {
      int runs_of_Ns=0;
      for(TMapStrRangeColl::iterator it = m_comp2range_coll.begin();  it != m_comp2range_coll.end(); ++it) {
          runs_of_Ns += it->second.size();
      }
      if(!m_use_xml) cout <<  m_comp2range_coll.size() << " component sequences have masked spans (" << runs_of_Ns << " spans)." << endl;
  }
  else if(!m_reader.m_CheckObjLen) {
    if(!m_use_xml) cout << "No runs of Ns longer than 10 bp found in FASTA sequences." << endl;
  }

}

void CAgpValidateApplication::x_ValidateUsingFiles(const CArgs& args)
{
  if(m_reader.m_is_chr) {
    if(m_reader.m_explicit_scaf) {
      if(!m_use_xml) cout << "===== Reading Chromosome from scaffold AGP =====" << endl;
    }
    // else: cout << "===== Reading Chromosome from component AGP =====" << endl;
  }
  else if(m_reader.m_explicit_scaf) {
    if(!m_use_xml) cout << "===== Reading Scaffold from component AGP =====" << endl;
  }

  if( 0==(m_ValidationType&VT_Acc) && args["out"].HasValue()) {
    CAgpCompSpanSplitter *comp_splitter = new CAgpCompSpanSplitter(&(args["out"].AsOutputFile()));
    m_reader.SetRowOutput(comp_splitter);
  }

  if (args.GetNExtra() == 0) {
    x_ValidateFile(cin);
  }
  else {
    SIZE_TYPE num_fasta_files=0;
    bool allowFasta = !m_reader.m_explicit_scaf;
    for (unsigned int i = 1; i <= args.GetNExtra(); i++) {

      m_CurrentFileName = args['#' + NStr::IntToString(i)].AsString();
      if(m_CurrentFileName=="-chr") {
        if(m_reader.m_is_chr) {
          cerr << "Error -- second -chr is not supported.\n";
          exit(1);
        }
        if(!m_reader.m_explicit_scaf) {
          cerr << "Error -- -chr after a file, but no preceding-scaf. Expecting:\n"
               << "    -scaf Scaffold_AGP_file(s) -chr Chromosome_AGP_file(s)\n";
          exit(1);
        }

        m_reader.PrintTotals(cout, m_use_xml);
        m_reader.Reset(true);
        pAgpErr->ResetTotals();

        if(!m_use_xml) cout << "\n===== Reading Chromosome from scaffold AGP =====" << endl;
        continue;
      }

      //CNcbiIstream& istr = args['#' + NStr::IntToString(i)].AsInputFile();
      CNcbiIfstream istr(m_CurrentFileName.c_str());
      if (!istr) {
          cerr << "Error -- unable to open file : " << m_CurrentFileName << "\n";
          exit (1);
      }

      char ch=0;
      if(allowFasta) {
        istr.get(ch); istr.putback(ch);
      }
      if(ch=='>') {
        x_LoadLenFa(istr, m_CurrentFileName);
        num_fasta_files++;
      }
      else {
        if(allowFasta && num_fasta_files) x_ReportFastaSeqCount();
        if( args.GetNExtra()-num_fasta_files>1 ) pAgpErr->StartFile(m_CurrentFileName);
        x_ValidateFile(istr);
        allowFasta=false;
      }

    }
    if(num_fasta_files==args.GetNExtra()) {
      //cerr << "No AGP files."; exit (1);
      if(allowFasta && num_fasta_files) x_ReportFastaSeqCount();
      x_ValidateFile(cin);
    }
  }

}

void CAgpValidateApplication::x_ValidateFile(
  CNcbiIstream& istr)
{

  if( 0==(m_ValidationType&VT_Acc) ) {
    // CAgpReader
    m_reader.SetVersion(m_agp_version);
    m_reader.ReadStream(istr); // , false
  }
  else {
    int line_num = 0;
    string  line;
    CRef<CAgpRow> agp_row( CAgpRow::New(pAgpErr.GetPointer(), m_agp_version));

    // Allow Unix, DOS, Mac EOL characters
    while( NcbiGetline(istr, line, "\r\n") ) {
      line_num++;

      int code=agp_row->FromString(line);
      if(code==-1) continue; // skip a comment line
      bool queued=false;
      bool comp2len_check_failed=false;

      if(code==0) {
        if( !agp_row->IsGap() ) {
          if( m_comp2len.size() && !agp_row->IsGap() ) {
            TMapStrInt::iterator it = m_comp2len.find( agp_row->GetComponentId() );
            if( it!=m_comp2len.end() ) {
              comp2len_check_failed=!agp_row->CheckComponentEnd(it->second);
              // Skip regular genbank-based validation for this line;
              // will print it verbatim, same as gap or error line.
              m_AltValidator->QueueLine(line);
              queued=true;
            }
            // else: will try Entrez and ObjMan
          }
          if(!queued){
            // component line - queue for batch lookup
            m_AltValidator->QueueLine(line,
              agp_row->GetComponentId(), line_num, agp_row->component_end);
            queued=true;
          }
        }
      }
      // else: the error message already reached the error handler

      if(m_AltValidator->m_out && !queued) {
        // error or gap line - queue for verbatim reprinting
        m_AltValidator->QueueLine(line);
      }

      if( code!=0 || comp2len_check_failed || // process the batch now so that error lines are printed in the correct order
          m_AltValidator->QueueSize() >= 1000
      ) {
          AutoPtr<CNcbiOstrstream> tmp_messages = pAgpErr->m_messages;
          pAgpErr->m_messages.reset(  new CNcbiOstrstream );

        // process a batch of preceding lines
        m_AltValidator->ProcessQueue();

        pAgpErr->m_messages = tmp_messages;
      }

      pAgpErr->LineDone(line, line_num, code!=0 );
    }
    m_AltValidator->ProcessQueue();
  }
}

void CAgpValidateApplication::Exit(void)
{
  SetDiagStream(0);
}

// To be moved to MapCompLen.cpp
void CAgpValidateApplication::x_LoadLenFa(CNcbiIstream& istr, const string& filename)
{
  string line;
  string acc, acc_long;
  int line_num=0;
  int acc_count=0;

  // these are initialized only to suppress the warnings
  int header_line_num=0;
  int len=0;
  int prev_len=0;

  TRangeColl range_coll; // runs of Ns in the fasta of the current component
  TSeqPos mfa_firstMasked=0;
  TSeqPos mfa_pos=0;
  bool mfa_bMasked=false;
  bool mfa_prevMasked=false;

  while( NcbiGetline(istr, line, "\r\n") ) {
    line_num++;
    //if(line.size()==0) continue;

    if(line[0]=='>') {
      if( acc.size() ) {
        // close off the previous acc

        // warn if acc could also be an accession
        OverrideLenIfAccession(acc, len);

        prev_len =  m_comp2len.AddCompLen(acc, len);
        if(acc_long!=acc) prev_len =  m_comp2len.AddCompLen(acc_long, len, false);
        if(prev_len) goto LengthRedefinedFa;

        if(mfa_bMasked) {
          if(mfa_pos-mfa_firstMasked > 10)
            range_coll += TSeqRange(mfa_firstMasked, mfa_pos-1);
        }
        if(!range_coll.empty()) {
          m_comp2range_coll[acc] = range_coll;
        }

        range_coll.clear();
        mfa_firstMasked=mfa_pos=0;
        mfa_bMasked=false;
        mfa_prevMasked=false;
      }

      // Get first word, trim final '|' (if any).
      SIZE_TYPE pos1=line.find(' ' , 1);
      SIZE_TYPE pos2=line.find('\t', 1);
      if(pos2<pos1) pos1 = pos2;
      if(pos1!=NPOS) {
        pos1--;
        if(pos1>0 && line[pos1]=='|') pos1--;
      }

      acc_long=line.substr(1, pos1);
      acc=ExtractAccession( acc_long );
      len=0;
      header_line_num=line_num;
      acc_count++;
    }
    else {
      if(acc.size()==0) {
        cerr<< "ERROR - expecting >fasta_header at start of file " << filename << ", got:\n"
            << line.substr(0, 100) << "\n\n";
        exit(1);
      }

      for(SIZE_TYPE i=0; i<line.size(); i++ ) {
        if(!isalpha(line[i])) {
          cerr<< "ERROR - non-alphabetic character in the FASTA:\n"
                 "  file " << filename << "\n  line " << line_num << "\n  column " << i+1 << "\n\n";
          exit(1);
        }

        mfa_pos++;
        mfa_bMasked = toupper(line[i]) == 'N';
        if(mfa_bMasked!=mfa_prevMasked) {
          if(mfa_bMasked) {
            mfa_firstMasked=mfa_pos;
          }
          else{
            if(mfa_pos-mfa_firstMasked > 10)
              range_coll += TSeqRange(mfa_firstMasked, mfa_pos-1);
          }
        }
        mfa_prevMasked=mfa_bMasked;

      }

      len+=line.size();

      /* to do: save runs of Ns as CRangeCollection<TSeqPos>
         later, will test component spans with:

         // returns iterator pointing to the TRange that has ToOpen > pos
          const_iterator  find(position_type pos)   const
          {
              PRangeLessPos<TRange, position_type> p;
              return lower_bound(begin(), end(), pos, p);
          }
      */
    }
  }

  if( acc.size() ) {
    // close off the last acc
    prev_len =  m_comp2len.AddCompLen(acc, len);
    if(acc_long!=acc) prev_len =  m_comp2len.AddCompLen(acc_long, len, false);
    if(prev_len) goto LengthRedefinedFa;

    if(mfa_bMasked) {
      if(mfa_pos-mfa_firstMasked > 10)
        range_coll += TSeqRange(mfa_firstMasked, mfa_pos-1);
    }
    if(!range_coll.empty()) {
      m_comp2range_coll[acc] = range_coll;
    }
  }
  if(acc_count==0) {
    cerr<< "WARNING - empty file " << filename << "\n";
  }
  return;

LengthRedefinedFa:
  cerr<< "ERROR - sequence length redefined from " << prev_len << " to " << len << "\n"
      << "  sequence id: " << acc_long << "\n"
      << "  File: " << filename << "\n"
      << "  Lines: "<< header_line_num << ".." << line_num << "\n\n";
  exit(1);
}

void CAgpCompSpanSplitter::SaveRow(const string& s, CRef<CAgpRow> row, TRangeColl* runs_of_Ns)
{
  if( row ) {
    comp_or_gap_printed=true;
    if(object_name != row->GetObject() ) {
      object_name = row->GetObject();
      part_number = 1; // row->GetPartNumber();
    }
    CRef<CAgpRow> tmp_row( row->Clone() );

    if(runs_of_Ns && runs_of_Ns->size()) {

      if( row->GetVersion() == eAgpVersion_auto ) {
        cerr << "FATAL: need AGP version (for adding gap lines). Please use -v 1.1 or -v 2.0\n";
        exit(1);
      }
      /*
      CAgpRow tmp_gap_row = *row; // to retain the object name
      tmp_gap_row.GetComponentType() = "N";
      tmp_gap_row.is_gap   = true;
      tmp_gap_row.linkage  = true;
      tmp_gap_row.gap_type = row->GetVersion() == eAgpVersion_1_1 ? CAgpRow::eGapFragment : CAgpRow::eGapScaffold;
      tmp_gap_row.linkage_evidence_flags = CAgpRow::fLinkageEvidence_unspecified;'
      */
      CRef<CAgpRow> tmp_gap_row( CAgpRow::New(NULL, row->GetVersion(), NULL) );
      tmp_gap_row->FromString(
        row->GetObject()+
        "\t1\t100\t1\tN\t100\t"+
        string(row->GetVersion() == eAgpVersion_1_1 ? "fragment\tyes\t" : "scaffold\tyes\tunspecified")
      );

      int comp2obj_ofs = row->object_beg - row->component_beg;

      for(TRangeColl::const_iterator it = runs_of_Ns->begin(); it != runs_of_Ns->end(); ++it) {
        if( (TSeqPos) tmp_row->component_beg < it->GetFrom() ) {
          // component line
          tmp_row->component_end = it->GetFrom()-1;
          tmp_row->object_end    = comp2obj_ofs + tmp_row->component_end;

          tmp_row->part_number = part_number;
          (*m_out) << tmp_row->ToString() << endl;
          part_number++;
        }

        // gap line
        tmp_gap_row->object_beg = comp2obj_ofs + it->GetFrom();
        tmp_gap_row->object_end = comp2obj_ofs + it->GetTo();
        tmp_gap_row->gap_length = it->GetTo() - it->GetFrom() + 1;

        tmp_gap_row->part_number = part_number;
        (*m_out) << tmp_gap_row->ToString(true) << endl; // true: use linkage_evidence_flags
        part_number++;

        tmp_row->component_beg = it->GetTo() + 1;
        tmp_row->object_beg    = comp2obj_ofs + tmp_row->component_beg;
      }

      if(tmp_row->component_beg <= row->component_end) {
        // this component does not end with Ns => need to print the final component span
        tmp_row->component_end = row->component_end;
        tmp_row->object_end    = row->object_end;
      }
      else return; // ends with Ns => skip printing the component row below
    }

    tmp_row->part_number = part_number;
    (*m_out) << tmp_row->ToString() << endl;
    part_number++;
  }
  else if(!comp_or_gap_printed){
    // comment line (only at the head of file, to comply with AGP 2.0)
    (*m_out) << s << endl;
  }
}

END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
  if(argc==1+1 && string("-comp")==argv[1]) {
    cout << "agp_validate -comp (formerly agp_fasta_compare):\n"
      // "check that the object sequences (in FASTA or ASN.1 file) match the AGP.\n" //
      "check that the object sequences FASTA matches the AGP.\n" //
      "\n"
      //"USAGE: agp_validate -comp [-options] ASN.1/FASTA file(s)... AGP file(s)...\n"
      "USAGE: agp_validate -comp [-options] FASTA file(s)... AGP file(s)...\n"
      "OPTIONS:\n"
      "    -loadlog OUTPUT_FILE   Save the list of all loaded sequences.\n"
      "    -ignoreagponly         Do not report objects present in AGP file(s) only.\n"
      "    -ignoreobjfileonly     Do not report objects present in FASTA file(s) only.\n"
      "    -diffstofind NUM       (EXPERIMENTAL) If specified, list the first NUM lines of each difference.\n"
      "    -out OUTPUT_FILE       Save the assembled AGP sequences as FASTA.\n"
      "\n"
      "FASTA files for components can be provided (along with object FASTA files) if components are not yet in GenBank.\n"
      ;
    return 0;
  }

  return CAgpValidateApplication().AppMain(
    argc, argv, 0, eDS_Default, 0
  );
}

