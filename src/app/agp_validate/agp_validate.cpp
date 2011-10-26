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
 *      Validate AGP data. A command line option to chose either context
 *      or GenBank validation. Context validation uses only the information
 *      in the AGP file. GenBank validation queries sequence length and taxid
 *      via ObjectManager or CEntrez2Client.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <objtools/readers/agp_util.hpp>

#include "ContextValidator.hpp"
#include "AltValidator.hpp"


USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE

CAgpErrEx agpErr;

class CAgpValidateApplication : public CNcbiApplication
{
private:
  virtual void Init(void);
  virtual int  Run(void);
  virtual void Exit(void);

  string m_CurrentFileName;

  enum EValidationType {
      VT_Context, VT_Acc=1, VT_Len=2, VT_Taxid=4,
      VT_AccLenTaxid = VT_Acc|VT_Len|VT_Taxid
  } m_ValidationType;

  CMapCompLen m_comp2len;

  //void x_LoadLen  (CNcbiIstream& istr, const string& filename);
  void x_LoadLenFa(CNcbiIstream& istr, const string& filename);

  CAltValidator* m_AltValidator;
  //CAgpContextValidator* m_ContextValidator;
  CAgpValidateReader m_reader;

  void x_ValidateUsingFiles(const CArgs& args);
  void x_ValidateFile(CNcbiIstream& istr);
  void x_ReportFastaSeqCount();

public:
  CAgpValidateApplication() : m_reader(agpErr, m_comp2len)
  {
  }
};

// Print a nicer usage message
class CArgDesc_agp_validate : public CArgDescriptions
{
public:
  string& PrintUsage(string& str, bool detailed) const
  {
    str="Validate data in the AGP format:\n"
    "http://www.ncbi.nlm.nih.gov/genome/assembly/agp/AGP_Specification.shtml\n"
    "\n"
    "USAGE: agp_validate [-options] [FASTA files...] [AGP files...]\n"
    "\n"
    "With no options: perform the checks that do not require\n"
    "the component sequences to be available in GenBank.\n"
    "Report component, gap, scaffold and object statistics.\n"
    "\n"
    "If component FASTA files are given in front of AGP files, also check that:\n"
    "- component_id from AGP is present in FASTA;\n"
    "- component_end does not exceed sequence length.\n"
    "If FASTA files for objects are given (after -obj), check that:\n"
    "- object_id from AGP is present in FASTA;\n"
    "- object lengths in FASTA and in AGP match.\n"
    "\n"
    "OPTIONS:\n"
    "  -alt       Check component Accessions, Lengths and Taxonomy ID using GenBank data.\n"
    "             This can be very time-consuming, and is done separately from most other checks.\n"
    "  -species   Allow components from different subspecies during Taxid checks (implies -alt).\n"
    "  -out FILE  Save the AGP file, adding missing version 1 to the component accessions\n"
    "             (use with -alt).\n"
    "  The above options require that the components are available in GenBank.\n"
    "  -g         Check that component names look like Nucleotide accessions\n"
    "             (this does not require components to be in GenBank).\n"
    "  -obj       Use FASTA files to read names and lengths of objects (the default is components).\n"
    "  -un        Enable the checks specific to unplaced/unlocalized single-component scaffolds\n"
    "             (use the whole component in orientation '+').\n"
    /*
    "\n"
    "  -fa  FILE (fasta)\n"
    "  -len FILE (component_id and length, tab-separated)\n"
    "    Check that each component_id from AGP can be found in the FILE(s),\n"
    "    and component_end is within the sequence length. Multiple -fa and -len are allowed.\n"
    */
    "\n"
    "  -list               List error and warning messages.\n"
    "  -limit COUNT        Print only the first COUNT messages of each type.\n"
    "                      Default=100. To print all, use: -limit 0\n"
    "  -skip, -only WHAT   Skip, or report only a particular error or warning.\n"
    "  -show WHAT          Show the warning hidden by default (w30, w31, w35, w36, w42).\n"
    "  'WHAT' could be a part of the message text, an error code (e11, w22, etc; see -list),\n"
    "  or a keyword: all, warn, err, alt.\n"
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

  arg_desc->AddFlag("g" , "");
  arg_desc->AddFlag("obj", "");
  arg_desc->AddFlag("un", "");

  arg_desc->AddFlag("species", "allow components from different subspecies");

  arg_desc->AddOptionalKey( "out", "FILE",
    "add missing version 1 to component accessions",
    CArgDescriptions::eOutputFile);

  /*
  arg_desc->AddOptionalKey( "fa", "FILE",
    "read component accessions and sequence lengths, compare to AGP",
    CArgDescriptions::eInputFile, CArgDescriptions::fAllowMultiple );

  arg_desc->AddOptionalKey( "len", "FILE",
    "read component accessions and sequence lengths, compare to AGP",
    CArgDescriptions::eString, CArgDescriptions::fAllowMultiple );
  */

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
  arg_desc->AddExtra(0, 1000, "files to be processed",
                      CArgDescriptions::eInputFile);
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

  /*
  // 2 iterations
  string arg_name="len";
  for(;;) {

    if( args[arg_name].HasValue() ) {
      // Load component accessions and lengths
      CArgValue::TStringArray args_len = args[arg_name].GetStringList();
      for(CArgValue::TStringArray::iterator it = args_len.begin();  it != args_len.end(); ++it) {
        CNcbiIfstream istr_len( it->c_str() );
        if(!istr_len.good()) {
          cerr<<"ERROR - cannot read " << *it << "\n";
          exit(1);
        }
        if(arg_name!="fa") x_LoadLen(istr_len, *it );
        else             x_LoadLenFa(istr_len, *it );
      }
    }

    if(arg_name=="fa") break;
    arg_name="fa";
  }
  */

  m_reader.m_CheckObjLen=args["obj"].HasValue();
  m_reader.m_unplaced   =args["un" ].HasValue();
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
      agpErr.SkipMsg(CAgpErr::W_CompIsWgsTypeIsNot, true);
      agpErr.SkipMsg(CAgpErr::W_CompIsNotWgsTypeIs, true);
      m_reader.m_CheckCompNames=true;
    }

  }
  if(m_ValidationType & VT_Acc) {
    //// Setup registry, error log, MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

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
    agpErr.SkipMsg("all");
    action="Allowed messages:\n";
  }
  if(err_warn) {
    // Inform agpErr what to skip; show messages that we skip.
    bool needHeading=true; // avoid printing >action when not needed
    for( CArgValue::TStringArray::const_iterator it =
      err_warn->begin();  it != err_warn->end(); ++it
    ) {
      string res  = agpErr.SkipMsg(*it, onlyNotSkip);
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
      agpErr.SkipMsg(*it, true);
    }
  }

  agpErr.m_MaxRepeat =
    args["limit"].HasValue() ? args["limit"].AsInteger() : 100;


  //// Process files, print results
  x_ValidateUsingFiles(args);
  if(m_ValidationType == VT_Context) {
    m_reader.PrintTotals();
  }
  else if(m_ValidationType & VT_Acc) {
    cout << "\n";
    if(m_ValidationType & VT_Taxid) m_AltValidator->CheckTaxids();
    m_AltValidator->PrintTotals();
  }

  return 0;
}

void CAgpValidateApplication::x_ReportFastaSeqCount()
{
  string s;
  if(m_comp2len.m_count!=1) s="s";
  cout<< m_comp2len.m_count << " "
      << (m_reader.m_CheckObjLen?"object name":"component_id")
      << s <<" and length" << s << " loaded from FASTA." << endl;
}

void CAgpValidateApplication::x_ValidateUsingFiles(
  const CArgs& args)
{
  if (args.GetNExtra() == 0) {
      x_ValidateFile(cin);
  }
  else {
    SIZE_TYPE num_fasta_files=0;
    bool allowFasta=true;
    for (unsigned int i = 1; i <= args.GetNExtra(); i++) {

      m_CurrentFileName =
          args['#' + NStr::IntToString(i)].AsString();

      CNcbiIstream& istr =
          args['#' + NStr::IntToString(i)].AsInputFile();
      if (!istr) {
          cerr << "Unable to open file : " << m_CurrentFileName;
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
        if( args.GetNExtra()-num_fasta_files>1 ) agpErr.StartFile(m_CurrentFileName);
        x_ValidateFile(istr);
        allowFasta=false;
      }

      args['#' + NStr::IntToString(i)].CloseFile();
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

  if( 0==m_ValidationType & VT_Acc) {
    // CAgpReader
    m_reader.ReadStream(istr); // , false
  }
  else {
    int line_num = 0;
    string  line;
    CAgpRow agp_row(&agpErr, eAgpVersion_auto);

    // Allow Unix, DOS, Mac EOL characters
    while( NcbiGetline(istr, line, "\r\n") ) {
      line_num++;

      int code=agp_row.FromString(line);
      if(code==-1) continue; // skip a comment line
      bool queued=false;
      bool comp2len_check_failed=false;

      if(code==0) {
        if( !agp_row.IsGap() ) {
          if( m_comp2len.size() && !agp_row.IsGap() ) {
            TMapStrInt::iterator it = m_comp2len.find( agp_row.GetComponentId() );
            if( it!=m_comp2len.end() ) {
              comp2len_check_failed=!agp_row.CheckComponentEnd(it->second);
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
              agp_row.GetComponentId(), line_num, agp_row.component_end);
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
        CNcbiOstrstream* tmp_messages = agpErr.m_messages;
        agpErr.m_messages =  new CNcbiOstrstream();

        // process a batch of preceding lines
        m_AltValidator->ProcessQueue();

        delete agpErr.m_messages;
        agpErr.m_messages = tmp_messages;
      }

      agpErr.LineDone(line, line_num, code!=0 );
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

  while( NcbiGetline(istr, line, "\r\n") ) {
    line_num++;
    //if(line.size()==0) continue;

    if(line[0]=='>') {
      if( acc.size() ) {
        prev_len =  m_comp2len.AddCompLen(acc, len);
        if(acc_long!=acc) prev_len =  m_comp2len.AddCompLen(acc_long, len, false);
        if(prev_len) goto LengthRedefinedFa;
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
      }

      len+=line.size();
    }
  }

  if( acc.size() ) {
    prev_len =  m_comp2len.AddCompLen(acc, len);
    if(acc_long!=acc) prev_len =  m_comp2len.AddCompLen(acc_long, len, false);
    if(prev_len) goto LengthRedefinedFa;
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

END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
  return CAgpValidateApplication().AppMain(
    argc, argv, 0, eDS_Default, 0
  );
}

