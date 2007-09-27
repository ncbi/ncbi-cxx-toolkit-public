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
 *      Lou Friedman, Victor Sapojnikov
 *
 * File Description:
 *      Validate AGP data. A command line option to chose either context
 *      or GenBank validation. Context validation uses only the information
 *      in the AGP file. GenBank validation queries sequence length and taxid
 *      via ObjectManager or CEntrez2Client.
 *
 */

#include <ncbi_pch.hpp>
#include "ContextValidator.hpp"
#include "AltValidator.hpp"

#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>

USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE

CAgpErr agpErr;

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
    "USAGE: agp_validate [-options] [input files...]\n"
    "\n"
    "agp_validate without any options performs all validation checks except\n"
    "for those that require the component sequences to be available in GenBank.\n"
    "It also reports component, gap, scaffold and object statistics.\n"
    "\n"
    "OPTIONS:\n"
    "  -alt       Check component Accessions, Lengths and Taxonomy ID using GenBank data.\n"
    "             This can be very time-consuming, and is done separately from most other checks.\n"
    "  -a         Check component Accessions (not lengths or taxids); slightly faster than \"-alt\".\n"
    "  -species   Allow components from different subspecies during Taxid checks (implies -alt).\n"
    "  -out FILE  Save the AGP file, adding missing version 1 to the component accessions\n"
    "             (use with -a or -alt).\n"
    "  The above options require that the components are available in GenBank.\n"
    /*
    "  -al, -at  Check component Accessions, Lengths (-al), Taxids (-at).\n"
    */
    "\n"
    "  -list         List error and warning messages.\n"
    "  -limit COUNT  Print only the first COUNT messages of each type.\n"
    "                Default=10. To print all, use: -limit 0\n"
    "\n"
    "  -skip  WHAT   Do not report lines with a particular error or warning message.\n"
    "  -only  WHAT   Report only this particular error or warning.\n"
    "  Multiple -skip or -only are allowed. 'WHAT' may be:\n"
    "  - an error code (e01,.. w21,..; see '-list')\n"
    "  - a part of the actual message\n"
    "  - a keyword: all, warn[ings], err[ors], alt\n"
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
     CAgpErr::PrintAllMessages(cout);
     exit(0);
   }

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
      x_ValidateFile(istr);
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
  SDataLine data_line;  // strings
  CAgpLine  agp_line ;  // successfuly parsed columns with non-contratictory data
  vector<string> cols;

  // Allow Unix, DOS, Mac EOL characters
  while( NcbiGetline(istr, line, "\r\n") )
  {
    line_num++;

    string line_orig=line; // With EOL #comments
    bool tabsStripped=false;
    bool valid=false;
    bool badCount=false;
    bool queued=false;
    // Strip #comments
    {
      SIZE_TYPE pos = NStr::Find(line, "#");
      if(pos==0) {
        m_CommentLineCount++;
        if( (m_ValidationType & VT_Acc) && m_AltValidator->m_out ) {
          m_AltValidator->QueueLine(line_orig);
          // do not need to set queued=true since we are not jumping to "NextLine" label
        }
        continue;
      }
      else if(pos != NPOS) {
        m_EolComments++;
        while( pos>0 && (line[pos-1]==' ' || line[pos-1]=='\t') ) {
          if( line[pos-1]=='\t' ) tabsStripped=true;
          pos--;
        }
        line.resize(pos);
      }
    }

    if(line == "") {
      agpErr.Msg(CAgpErr::E_EmptyLine);

      // avoid suppressing the errors related to the previous line
      // valid=true;
      // ^^ can't do it that easily -- this variable affects
      //    2 other things as well: 1) skipped line count
      //    2) correct order of -alt messages

      goto NextLine;
    }


    cols.clear();
    NStr::Tokenize(line, "\t", cols);
    if( cols.size()==10 && cols[9]=="") {
      agpErr.Msg(CAgpErr::W_ExtraTab);
    }
    else if( cols.size() < 8 || cols.size() > 9 ) {
      // skip this entire line, report an error
      agpErr.Msg(CAgpErr::E_ColumnCount,
        string(", found ") + NStr::IntToString(cols.size()) );
      goto NextLine;
    }

    for(int i=0; i<8; i++) {
      if(cols[i].size()==0) {
        // skip this entire line, report an error
        agpErr.Msg(CAgpErr::E_EmptyColumn,
            NcbiEmptyString, AT_ThisLine, // defaults
            NStr::IntToString(i+1)
          );
        goto NextLine; // cannor use "continue"
      }
    }

    data_line.line_num = line_num;

    // 5 common columns for components and gaps.
    data_line.object         = cols[0];
    data_line.begin          = cols[1];
    data_line.end            = cols[2];
    data_line.part_num       = cols[3];
    data_line.component_type = cols[4];

    // columns with different meaning for components and gaps.
    data_line.component_id   = cols[5];
    data_line.gap_length     = cols[5];

    data_line.component_beg  = cols[6];
    data_line.gap_type       = cols[6];

    data_line.component_end   = cols[7];
    data_line.linkage         = cols[7];

    data_line.orientation = "";
    if( cols.size() > 8 ) {
      data_line.orientation = cols[8];
    }
    else {
      if( ! CAgpLine::IsGapType(data_line.component_type) ) {
        if(tabsStripped) {
          agpErr.Msg(CAgpErr::E_EmptyColumn,
            NcbiEmptyString, AT_ThisLine, // defaults
            "9");
          badCount=true;
        }
        else {
          agpErr.Msg(CAgpErr::E_ColumnCount,
            string(", found ") + NStr::IntToString(cols.size()) );
          badCount=true;
        }
      }
      else if(tabsStripped==false){
        agpErr.Msg(CAgpErr::W_GapLineMissingCol9);
      }
    }

    valid=agp_line.init(data_line);
    if(badCount) valid=false; // make sure the line is skipped, as we said it is
    if(valid) {
      if(m_ValidationType == VT_Context) {
        m_ContextValidator->ValidateLine(data_line, agp_line);
      }
      else if( !agp_line.is_gap ) {
        m_AltValidator->QueueLine(line_orig,
          data_line.component_id, line_num, agp_line.compSpan.end);
        queued=true;
      }

      if(istr.eof()) {
        agpErr.Msg(CAgpErr::W_NoEolAtEof);
      }
    }

  NextLine:
    if(m_ValidationType & VT_Acc) {
      if(m_AltValidator->m_out && !queued) {
        m_AltValidator->QueueLine(line_orig);
      }
      if( !valid || m_AltValidator->QueueSize() >= 150 ) {
        // process the batch now so that error lines are printed in the correct order
        CNcbiOstrstream* tmp_messages = agpErr.m_messages;
        agpErr.m_messages =  new CNcbiOstrstream();

        // process a batch of preceding lines
        m_AltValidator->ProcessQueue();

        delete agpErr.m_messages;
        agpErr.m_messages = tmp_messages;
      }
    }

    agpErr.LineDone(line_orig, line_num, !valid );
    if(!valid && m_ValidationType == VT_Context) {
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
END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
  return CAgpValidateApplication().AppMain(
    argc, argv, 0, eDS_Default, 0
  );
}

