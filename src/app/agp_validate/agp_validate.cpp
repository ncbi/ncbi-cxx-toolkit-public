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

typedef map<string, int> TMapStrInt ;
class CMapCompLen : public TMapStrInt
{
public:
  typedef pair<TMapStrInt::iterator, bool> TMapStrIntResult;
  // returns 0 on success, or a previous length not equal to the new one
  int AddCompLen(const string& acc, int len);
};

class CAgpValidateApplication : public CNcbiApplication
{
private:
  virtual void Init(void);
  virtual int  Run(void);
  virtual void Exit(void);

  string m_CurrentFileName;

  enum EValidationType {
      VT_Context, VT_Acc=1, VT_Len=2, VT_Taxid=4,
      VT_AccLenTaxid = VT_Acc|VT_Len|VT_Taxid,
      VT_AccLen   = VT_Acc|VT_Len,
      VT_AccTaxid = VT_Acc|VT_Taxid
  } m_ValidationType;

  CAltValidator* m_AltValidator;
  CAgpContextValidator* m_ContextValidator;
  // data of an AGP line either
  //  from a file or the agp adb
  // typedef vector<SDataLine> TDataLines;


  // Validate either from AGP files or from AGP DB
  //  Each line (entry) of AGP data from either source is
  //  populates a SDataLine.
  //
  void x_ValidateUsingFiles(const CArgs& args);
  void x_ValidateFile(CNcbiIstream& istr);

  CMapCompLen m_comp2len;
  void x_LoadLen  (CNcbiIstream& istr, const string& filename);
  void x_LoadLenFa(CNcbiIstream& istr, const string& filename);

  int m_CommentLineCount;
  int m_EolComments;
};

// Print a nicer usage message
class CArgDesc_agp_validate : public CArgDescriptions
{
public:
  string& PrintUsage(string& str, bool detailed) const
  {
    str="Validate data in the AGP format:\n"
    "http://www.ncbi.nlm.nih.gov/genome/guide/Assembly/AGP_Specification.html\n"
    //"http://www.ncbi.nlm.nih.gov/Genbank/WGS.agpformat.html\n"
    "\n"
    "USAGE: agp_validate [-options] [FASTA files...] [AGP files...]\n"
    "\n"
    "agp_validate without any options performs all validation checks except\n"
    "for those that require the component sequences to be available in GenBank.\n"
    "It also reports component, gap, scaffold and object statistics.\n"
    "\n"
    "If component FASTA files are provided in front of AGP files, it checks that:\n"
    "- component_id from AGP is present in FASTA;\n"
    "- component_end does not exceed sequence length.\n"
    "\n"
    "OPTIONS:\n"
    "  -alt       Check component Accessions, Lengths and Taxonomy ID using GenBank data.\n"
    "             This can be very time-consuming, and is done separately from most other checks.\n"
    "  -a         Check component Accessions (not lengths or taxids); slightly faster than \"-alt\".\n"
    "  -species   Allow components from different subspecies during Taxid checks (implies -alt).\n"
    "  -out FILE  Save the AGP file, adding missing version 1 to the component accessions\n"
    "             (use with -a or -alt).\n"
    "  The above options require that the components are available in GenBank.\n"
    //"  -al, -at  Check component Accessions, Lengths (-al), Taxids (-at).\n"
    /*
    "\n"
    "  -fa  FILE (fasta)\n"
    "  -len FILE (component_id and length, tab-separated)\n"
    "    Check that each component_id from AGP can be found in the FILE(s),\n"
    "    and component_end is within the sequence length. Multiple -fa and -len are allowed.\n"
    */
    "\n"
    "  -list              List error and warning messages.\n"
    "  -limit COUNT       Print only the first COUNT messages of each type.\n"
    "                     Default=10. To print all, use: -limit 0\n"
    "  -skip, -only WHAT  Skip, or report only a particular error or warning.\n"
    "  'WHAT' could be an error code (e11 w22 etc - see -list), a part of the message text,\n"
    "  or one of these keywords: all, warn, err, alt.\n"
    /*
    "  -skip  WHAT   Do not report lines with a particular error or warning message.\n"
    "  -only  WHAT   Report only this particular error or warning.\n"
    "  Multiple -skip or -only are allowed. 'WHAT' may be:\n"
    "  - an error code (e01,.. w21,..; see '-list')\n"
    "  - a part of the actual message\n"
    "  - a keyword: all, warn[ings], err[ors], alt\n"
    */
    "\n"
    ;
    return str;
    // To do: -taxon "taxname or taxid" ?
  }
};

void CAgpValidateApplication::Init(void)
{
  m_CommentLineCount=0;
  m_EolComments=0;
  //auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
  auto_ptr<CArgDesc_agp_validate> arg_desc(new CArgDesc_agp_validate);

  arg_desc->SetUsageContext(
    GetArguments().GetProgramBasename(),
    "Validate AGP data", false);

  // component_id  checks that involve GenBank: Accession Length Taxid
  arg_desc->AddFlag("alt", "");
  //arg_desc->AddFlag("al" , "");
  //arg_desc->AddFlag("at" , "");
  arg_desc->AddFlag("a" , "");

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
    "Message or message code to print",
    CArgDescriptions::eString,
    CArgDescriptions::fAllowMultiple);

  arg_desc->AddDefaultKey("limit", "ErrorCount",
    "Print at most ErrorCount lines with a particular error",
    CArgDescriptions::eInteger,
    "10");

  arg_desc->AddFlag("list", "all possible errors and warnings");

  // file list for file processing
  arg_desc->AddExtra(0, 100, "files to be processed",
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

  if( args["alt"].HasValue() || args["species"].HasValue() )
    m_ValidationType = VT_AccLenTaxid;
  /*
  else if( args["al" ].HasValue() ) m_ValidationType = VT_AccLen;
  else if( args["at" ].HasValue() ) m_ValidationType = VT_AccTaxid;
  */
  else if( args["a"  ].HasValue() ) m_ValidationType = VT_Acc;
  else {
    m_ValidationType = VT_Context;
    m_ContextValidator = new CAgpContextValidator();
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
       cerr << "FATAL ERROR: cannot specify both -only and -skip.\n";
       exit(1);
     }
     err_warn = &( args["skip"].GetStringList() );
     action="Skipping messages:\n";
  }
  else if(onlyNotSkip) {
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

  agpErr.m_MaxRepeat =
    args["limit"].HasValue() ? args["limit"].AsInteger() : 10;
    //m_ValidationType == VT_Context ? 10 : 100;


  //// Process files, print results
  x_ValidateUsingFiles(args);
  if(m_ValidationType == VT_Context) {
    m_ContextValidator->PrintTotals();
    if(m_CommentLineCount || m_EolComments) cout << "\n";
    if(m_CommentLineCount) {
      cout << "#Comment line count    : " << m_CommentLineCount << "\n";
    }
    if(m_EolComments) {
      cout << "End of line #comments  : " << m_EolComments << "\n";
    }
  }
  else if(m_ValidationType & VT_Acc) {
    cout << "\n";
    if(m_ValidationType & VT_Taxid) m_AltValidator->CheckTaxids();
    m_AltValidator->PrintTotals();
  }

  return 0;
}

void CAgpValidateApplication::x_ValidateUsingFiles(
  const CArgs& args)
{
  if (args.GetNExtra() == 0) {
      x_ValidateFile(cin);
  }
  else {
    bool allowFasta=true;
    for (unsigned int i = 1; i <= args.GetNExtra(); i++) {

      m_CurrentFileName =
          args['#' + NStr::IntToString(i)].AsString();
      if( args.GetNExtra()>1 ) agpErr.StartFile(m_CurrentFileName);

      CNcbiIstream& istr =
          args['#' + NStr::IntToString(i)].AsInputFile();
      if (!istr) {
          cerr << "Unable to open file : " << m_CurrentFileName;
          exit (0);
      }

      char ch=0;
      if(allowFasta) {
        istr.get(ch); istr.putback(ch);
      }
      if(ch=='>') {
        x_LoadLenFa(istr, m_CurrentFileName);
      }
      else {
        x_ValidateFile(istr);
        allowFasta=false;
      }

      args['#' + NStr::IntToString(i)].CloseFile();
    }
  }
  // This is needed to check whether the last line was a gap, or a singleton.
  if (m_ValidationType == VT_Context) {
    m_ContextValidator->EndOfObject(true);
  }
}

void CAgpValidateApplication::x_ValidateFile(
  CNcbiIstream& istr)
{
  int line_num = 0;
  string  line;
  CAgpRow agp_row(&agpErr);

  bool valid=false;

  // Allow Unix, DOS, Mac EOL characters
  while( NcbiGetline(istr, line, "\r\n") ) {
    line_num++;

    bool queued=false;

    int code=agp_row.FromString(line);
    if(code==-1) {
      m_CommentLineCount++;
      continue;
    }
    if(agp_row.pcomment!=NPOS) m_EolComments++;

    if(code==0) {
      valid=true;

      //// begin: -len -fa
      if( m_comp2len.size() && !agp_row.IsGap() ) {
        TMapStrInt::iterator it = m_comp2len.find( agp_row.GetComponentId() );
        if( it==m_comp2len.end() ) {
          if(m_ValidationType == VT_Context) {
            agpErr.Msg(CAgpErrEx::G_InvalidCompId, string(": ")+agp_row.GetComponentId());
          }
          // else: will try GenBank
        }
        else {
          CAltValidator::ValidateLength(
            agp_row.GetComponentId(),
            agp_row.component_end,
            it->second );
          if(m_ValidationType != VT_Context) {
            // Skip regular genbank-based validation for this line.
            // We could print it verbatim, same as comment or a gap line.
            m_AltValidator->QueueLine(line);
            queued=true;
          }
        }
      }
      //// end: -len -fa

      if(m_ValidationType == VT_Context) {
        m_ContextValidator->ValidateRow(agp_row, line_num);
      }
      //else if( !agp_line.is_gap )
      else if( !agp_row.IsGap() && !queued )
      {
        m_AltValidator->QueueLine(line,
          agp_row.GetComponentId(), line_num, agp_row.component_end);
        queued=true;
      }

      if(istr.eof()) {
        agpErr.Msg(CAgpErrEx::W_NoEolAtEof, NcbiEmptyString);
      }
    }
    else valid=false; // the error message must be passed already via an error handler adaptor

    if(m_ValidationType & VT_Acc) {
      if(m_AltValidator->m_out && !queued) {
        m_AltValidator->QueueLine(line); // line_orig
      }
      if(
        !valid ||
        m_AltValidator->QueueSize() >= 150
      ) {
        // process the batch now so that error lines are printed in the correct order
        CNcbiOstrstream* tmp_messages = agpErr.m_messages;
        agpErr.m_messages =  new CNcbiOstrstream();

        // process a batch of preceding lines
        m_AltValidator->ProcessQueue();

        delete agpErr.m_messages;
        agpErr.m_messages = tmp_messages;
      }
    }

    agpErr.LineDone(line, line_num, !valid );
    if(!valid && m_ValidationType == VT_Context)
    {
      // Adjust the context after an invalid line
      // (to suppress some spurious errors)
      m_ContextValidator->InvalidLine();
    }
  }

  if( m_ValidationType & VT_Acc) m_AltValidator->ProcessQueue();
}

void CAgpValidateApplication::Exit(void)
{
  SetDiagStream(0);
}

/*
void CAgpValidateApplication::x_LoadLen(CNcbiIstream& istr, const string& filename)
{
  string line;
  int line_num=0;
  int acc_count=0;
  char buf[65];
  int len;

  while( NcbiGetline(istr, line, "\r\n") ) {
    line_num++;
    if(line.size()==0) continue;

    if( 2 != sscanf(line.c_str(), "%64s %i", buf, &len) || len<=0 ) {
      cerr<< "ERROR - invalid line at " << filename << ":" << line_num << ":\n"
          << line.substr(0, 100) << "\n"
          << "\tExpecting: component_id length.\n";
      exit(1);
    }

    int prev_len =  m_comp2len.AddCompLen(buf, len);
    if(prev_len) {
      cerr<< "ERROR - component length redefined from " << prev_len << " to " << len << "\n"
          << "  component_id: " << buf << "\n"
          << "  File: " << filename << "\n"
          << "  Line: " <<  line_num << "\n\n";
      exit(1);
    }

    acc_count++;
  }

  if(acc_count==0) {
    cerr<< "WARNING - empty file " << filename << "\n";
  }
}
*/

void CAgpValidateApplication::x_LoadLenFa(CNcbiIstream& istr, const string& filename)
{
  string line;
  string acc, acc_long;
  int line_num=0;
  int acc_count=0;
  int len;
  int header_line_num, prev_len;

  while( NcbiGetline(istr, line, "\r\n") ) {
    line_num++;
    //if(line.size()==0) continue;

    if(line[0]=='>') {
      if( acc.size() ) {
        prev_len =  m_comp2len.AddCompLen(acc, len);
        if(acc_long!=acc) prev_len =  m_comp2len.AddCompLen(acc_long, len);
        if(prev_len) goto LengthRedefinedFa;
      }

      // Get first word, trim final '|' (if any).
      SIZE_TYPE pos1=line.find(' ' , 1);
      SIZE_TYPE pos2=line.find('\t', 1);
      if(pos2<pos1) pos1 = pos2;
      if(pos1!=NPOS) {
        pos1--;
        while(pos1>0 && line[pos1-1]=='|') pos1--;
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
      len+=line.size();
    }
  }

  if( acc.size() ) {
    prev_len =  m_comp2len.AddCompLen(acc, len);
    if(acc_long!=acc) prev_len =  m_comp2len.AddCompLen(acc_long, len);
    if(prev_len) goto LengthRedefinedFa;
  }
  if(acc_count==0) {
    cerr<< "WARNING - empty file " << filename << "\n";
  }
  return;

LengthRedefinedFa:
  cerr<< "ERROR - component length redefined from " << prev_len << " to " << len << "\n"
      << "  component_id: " << acc_long << "\n"
      << "  File: " << filename << "\n"
      << "  Lines: "<< header_line_num << ".." << line_num << "\n\n";
  exit(1);
}

int CMapCompLen::AddCompLen(const string& acc, int len)
{
  TMapStrInt::value_type acc_len(acc, len);
  TMapStrIntResult insert_result = insert(acc_len);
  if(insert_result.second == false) {
    if(insert_result.first->second != len)
      return insert_result.first->second; // error: already have a different length
  }
  return 0; // success
}

END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
  return CAgpValidateApplication().AppMain(
    argc, argv, 0, eDS_Default, 0
  );
}

