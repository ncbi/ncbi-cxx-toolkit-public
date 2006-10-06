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
 *      Validate AGP data. A command line option to chose either syntactic
 *      or GenBank validation. Syntactic validation uses only the information
 *      in the AGP file. GenBank validation queries sequence length and taxid
 *      via ObjectManager or CEntrez2Client.
 *
 */

#include <ncbi_pch.hpp>
#include "SyntaxValidator.hpp"
//#include "AgpErr.hpp"

// Objects includes
// todo: move this to a separate .cpp file with GenBank validator
#include <objects/seq/Bioseq.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/entrez2/entrez2_client.hpp>
#include <objects/entrez2/Entrez2_docsum.hpp>
#include <objects/entrez2/Entrez2_docsum_data.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objects/seqfeat/Org_ref.hpp>

// Object Manager includes
// todo: move this to a separate .cpp file with GenBank validator
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

// #include "db.hpp"

using namespace ncbi;
using namespace objects;

#define AGP_POST(msg) cerr << msg << "\n"

BEGIN_NCBI_SCOPE

// USING_NCBI_SCOPE;
CAgpErr agpErr;

class CAgpValidateApplication : public CNcbiApplication
{
private:

  virtual void Init(void);
  virtual int  Run(void);
  virtual void Exit(void);

  string m_CurrentFileName;

  enum EValidationType {
      VT_Syntax, VT_Acc=1, VT_Len=2, VT_Taxid=4,
      VT_AccLenTaxid = VT_Acc|VT_Len|VT_Taxid,
      VT_AccLen   = VT_Acc|VT_Len,
      VT_AccTaxid = VT_Acc|VT_Taxid
  } m_ValidationType;
  bool m_SpeciesLevelTaxonCheck;

  CRef<CObjectManager> m_ObjectManager;
  CRef<CScope> m_Scope;

  //CRef<CDb>   m_VolDb;

  // In future, may hold other validator types
  CAgpSyntaxValidator* m_LineValidator;


  // data of an AGP line either
  //  from a file or the agp adb
  // typedef vector<SDataLine> TDataLines;


  // The next few structures are used to keep track
  //  of the taxids used in the AGP files. A map of
  //  taxid to a vector of AGP file lines is maintained.
  struct SAgpLineInfo {
      string  filename;
      int     line_num;
      string  component_id;
  };

  typedef vector<SAgpLineInfo> TAgpInfoList;
  typedef map<int, TAgpInfoList> TTaxidMap;
  typedef pair<TTaxidMap::iterator, bool> TTaxidMapRes;
  TTaxidMap m_TaxidMap;
  int m_TaxidComponentTotal;

  typedef map<int, int> TTaxidSpeciesMap;
  TTaxidSpeciesMap m_TaxidSpeciesMap;

  //
  // Validate either from AGP files or from AGP DB
  //  Each line (entry) of AGP data from either source is
  //  populates a SDataLine.
  //
  void x_ValidateUsingDB(const CArgs& args);
  void x_ValidateUsingFiles(const CArgs& args);
  void x_ValidateFile(CNcbiIstream& istr);

  //
  // GenBank validate methods
  //
  void x_ValidateGenBankInit();
  void x_ValidateGenBankLine(const SDataLine& line);
  void x_GenBankPrintTotals();

  int x_GetTaxid(CBioseq_Handle& bioseq_handle, const SDataLine& dl);
  void x_AddToTaxidMap(int taxid, const SDataLine& dl);
  void x_CheckTaxid();
  int x_GetSpecies(int taxid);
  int x_GetTaxonSpecies(int taxid);

  int m_GenBankCompLineCount;
};

// Povide a nicer usage message
class CArgDesc_agp_validate : public CArgDescriptions
{
public:
  string& PrintUsage(string& str, bool detailed) const
  {
    str="Validate data in the AGP format:\n"
    "http://www.ncbi.nlm.nih.gov/Genbank/WGS.agpformat.html\n"
    "\n"
    "USAGE: agp_validate [-options] [input files...]\n"
    "\n"
    "OPTIONS:\n"
    "  -alt      Check component Accessions, Lengths and Taxids using GenBank data.\n"
    "            This can be very time-consuming, and is done separately from most other checks.\n"
    /*
    "  -a        Check component Accessions (but not lengths or taxids); faster than \"-alt\".\n"
    "  -al, -at  Check component Accessions, Lengths (-al), Taxids (-at).\n"
    */
    "  -species  Allow components from different subspecies during Taxid checks.\n"
    "\n"
    "  -list         List possible syntax errors and warnings.\n"
    "  -limit COUNT  Print only the first COUNT messages of each type.\n"
    "                Default=10. To print all, use: -limit 0\n"
    "\n"
    "  -skip  WHAT   Do not report lines with a particular error or warning message.\n"
    "  -only  WHAT   Report only this particular error or warning.\n"
    "  Multiple -skip or -only are allowed. 'WHAT' may be:\n"
    "  - an error code (e01,.. w21,..; see '-list')\n"
    "  - a part of the actual message\n"
    "  - a keyword: all, warn[ings], err[ors]\n"
    "\n"
    ;
    return str;
    // To do: -taxon "taxname or taxid" ?
  }
};

void CAgpValidateApplication::Init(void)
{
  m_GenBankCompLineCount=0;
  //auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
  auto_ptr<CArgDesc_agp_validate> arg_desc(new CArgDesc_agp_validate);

  arg_desc->SetUsageContext(
    GetArguments().GetProgramBasename(),
    "Validate AGP data", false);

  // component_id  checks that involve GenBank: Accession Length Taxid
  arg_desc->AddFlag("alt", "");
  arg_desc->AddFlag("al" , "");
  arg_desc->AddFlag("at" , "");
  arg_desc->AddFlag("a" , "");

  arg_desc->AddFlag("species", "allow components from different subspecies");


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

  m_TaxidComponentTotal = 0;

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

  if     ( args["alt"].HasValue() ) m_ValidationType = VT_AccLenTaxid;
  /*
  else if( args["al" ].HasValue() ) m_ValidationType = VT_AccLen;
  else if( args["at" ].HasValue() ) m_ValidationType = VT_AccTaxid;
  else if( args["a"  ].HasValue() ) m_ValidationType = VT_Acc;
  */
  else {
    m_ValidationType = VT_Syntax;
    m_LineValidator = new CAgpSyntaxValidator();
  }
  if(m_ValidationType & VT_Acc) {
    x_ValidateGenBankInit();
  }

  m_SpeciesLevelTaxonCheck = false;
  //if(args["taxon"].AsString() == "species")
  if( args["species"].HasValue() )
  {
    m_SpeciesLevelTaxonCheck = true;
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
    //m_ValidationType == VT_Syntax ? 10 : 100;


  //// Process files, print results
  x_ValidateUsingFiles(args);
  if(m_ValidationType == VT_Syntax) {
    m_LineValidator->PrintTotals();
  }
  else if(m_ValidationType & VT_Taxid) {
    cout << "\n";
    x_CheckTaxid();
    x_GenBankPrintTotals();
  }

  return 0;
}

void CAgpValidateApplication::x_ValidateUsingDB(
  const CArgs& args)
{
    // Validate the data from the AGP DB based on submit id.
    // The args contain the submit id and db info.
    // 2006/08/24 see agp_validate_extra.cpp
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

      //AGP_POST(m_CurrentFileName<<"\n");
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
  // Needed to check if the last line was a gap, or a singleton.
  if (m_ValidationType == VT_Syntax) {
    m_LineValidator->EndOfObject(true);
  }
}

void CAgpValidateApplication::x_ValidateFile(
  CNcbiIstream& istr)
{
  int line_num = 0;
  string  line;
  SDataLine data_line;
  vector<string> cols;
  //while (NcbiGetlineEOL(istr, line))
  while (NcbiGetline(istr, line, "\r\n")) // Allow Unix, DOS, Mac EOL characters
  {
    line_num++;

    string line_orig=line; // With EOL #comments
    // Strip #comments
    {
      SIZE_TYPE pos = NStr::Find(line, "#");
      if(pos != NPOS) {
        while( pos>0 && (line[pos-1]==' ' || line[pos-1]=='\t') ) pos--;
        line.resize(pos);
      }
    }
    if (line == "") continue;

    cols.clear();
    NStr::Tokenize(line, "\t", cols);
    if( cols.size()==10 && cols[9]=="") {
      agpErr.Msg(CAgpErr::W_ExtraTab);
    }
    else if( cols.size() < 8 || cols.size() > 9 ) {
      // skip this entire line, report an error
      agpErr.Msg(CAgpErr::E_ColumnCount,
        string(", found ") + NStr::IntToString(cols.size()) );
      agpErr.LineDone(line_orig, line_num, true);  // true: invalid_line
      continue;
    }

    for(int i=0; i<8; i++) {
      if(cols[i].size()==0) {
        // skip this entire line, report an error
        agpErr.Msg(CAgpErr::E_EmptyColumn,
            NcbiEmptyString, AT_ThisLine, // defaults
            NStr::IntToString(i+1)
          );
        agpErr.LineDone(line_orig, line_num, true);  // true: invalid_line
        continue;
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
    if (data_line.component_type != "N") {
      // Component
      if (cols.size() > 8) data_line.orientation = cols[8];
    }

    bool invalid_line=false;
    if (m_ValidationType == VT_Syntax) {
      invalid_line = m_LineValidator->ValidateLine(data_line, line);
    } else if( !CAgpSyntaxValidator::IsGapType(data_line.component_type) ) {
      x_ValidateGenBankLine(data_line);
    }

    agpErr.LineDone(line_orig, line_num, invalid_line);
  }
}

void CAgpValidateApplication::x_ValidateGenBankInit()
{
  // Create object manager
  // * CRef<> here will automatically delete the OM on exit.
  // * While the CRef<> exists GetInstance() returns the same
  //   object.
  m_ObjectManager.Reset(CObjectManager::GetInstance());

  // Create GenBank data loader and register it with the OM.
  // * The GenBank loader is automatically marked as a default
  // * to be included in scopes during the CScope::AddDefaults()
  CGBDataLoader::TRegisterLoaderInfo inf =
    CGBDataLoader::RegisterInObjectManager(*m_ObjectManager);
  if( ! inf.IsCreated() ) {
    cerr << "FATAL: cannot connect to GenBank!\n";
    exit(1);
  }

  // Create a new scope ("attached" to our OM).
  m_Scope.Reset(new CScope(*m_ObjectManager));
  // Add default loaders (GB loader in this demo) to the scope.
  m_Scope->AddDefaults();
}

void CAgpValidateApplication::x_ValidateGenBankLine(
  const SDataLine& dl)
{
  //// Check the accession
  CSeq_id seq_id;
  try {
      seq_id.Set(dl.component_id);
  }
  //    catch (CSeqIdException::eFormat)
  catch (...) {
    agpErr.Msg(CAgpErr::G_InvalidCompId, string(": ")+dl.component_id);
    return;
  }

  CBioseq_Handle bioseq_handle=m_Scope->GetBioseqHandle(seq_id);
  if( !bioseq_handle ) {
    agpErr.Msg(CAgpErr::G_NotInGenbank, string(": ")+dl.component_id);
    return;
  }

  m_GenBankCompLineCount++; // component_id is a valid GenBank accession

  //// Warn if no version was supplied and GenBank version is > 1
  SIZE_TYPE pos_ver = NStr::Find( dl.component_id, "."); //, 0, NPOS, NStr::EOccurrence::eLast);
  if(pos_ver==NPOS) {
    string acc_ver = sequence::GetAccessionForId(seq_id, *m_Scope);
    pos_ver = NStr::Find( acc_ver, "."); //, 0, NPOS, NStr::EOccurrence::eLast);
    if(pos_ver==NPOS) {
      cerr << "FATAL: cannot get version for " << dl.component_id << "\n";
      exit(1);
    }
    int ver = NStr::StringToInt( acc_ver.substr(pos_ver+1) );
    if(ver>1) {
      agpErr.Msg(CAgpErr::G_NeedVersion,
        string(" (current version is ")+acc_ver+")",
        AT_ThisLine,
        dl.component_id);
    }
  }

  if(m_ValidationType & VT_Len) {
    // Component out of bounds check
    int comp_end = CAgpSyntaxValidator::x_CheckIntField(
      dl.component_end, "component_end (column 8)" );
    if(comp_end<=0) return;

    CBioseq_Handle::TInst_Length seq_len =
      bioseq_handle.GetInst_Length();

    if ( comp_end > (int) seq_len) {
      string details=": ";
      details += dl.component_end;
      details += " > ";
      details += dl.component_id;
      details += " length = ";
      details += NStr::IntToString(seq_len);
      details += " bp";

      agpErr.Msg(CAgpErr::G_CompEndGtLength, details );

      //AGP_ERROR( "Component end greater than sequence length: "
      //  << dl.component_end << " > "
      //  << dl.component_id << " length = "
      //  << seq_len << " bp");
    }
  }

  if(m_ValidationType & VT_Taxid) {
    int taxid = x_GetTaxid(bioseq_handle, dl);
    x_AddToTaxidMap(taxid, dl);
  }
}

void CAgpValidateApplication::x_GenBankPrintTotals()
{
  int e_count=agpErr.CountTotals(CAgpErr::CODE_First, CAgpErr::CODE_Last);

  if(m_GenBankCompLineCount) {
    cout << m_GenBankCompLineCount << " lines with GenBank component accessions";
  }
  else{
    cout << "No GenBank components found";
  }
  if(e_count) {
    if(e_count==1) cout << "; 1 error";
    else cout << "; " << e_count << " errors";
    if(agpErr.m_skipped_count) cout << ", " << agpErr.m_skipped_count << " not printed";
    cout << ":\n";
    agpErr.PrintMessageCounts(cout, CAgpErr::CODE_First, CAgpErr::CODE_Last);
  }
  else {
    cout << "; no non-GenBank component accessions.\n";
  }
}

int CAgpValidateApplication::x_GetTaxid(
  CBioseq_Handle& bioseq_handle, const SDataLine& dl)
{
  int taxid = 0;
  string docsum_taxid = "";

  taxid = sequence::GetTaxId(bioseq_handle);

  if (taxid == 0) {
    try {
      CSeq_id_Handle seq_id_handle = sequence::GetId(
        bioseq_handle, sequence::eGetId_ForceGi
      );
      int gi = seq_id_handle.GetGi();
      CEntrez2Client entrez;
      CRef<CEntrez2_docsum_list> docsums =
        entrez.GetDocsums(gi, "Nucleotide");

      ITERATE(CEntrez2_docsum_list::TList, it,
        docsums->GetList()
      ) {
        docsum_taxid = (*it)->GetValue("TaxId");

        if (docsum_taxid != "") {
          taxid = NStr::StringToInt(docsum_taxid);
          break;
        }
      }
    }
    catch(...) {
      agpErr.Msg(CAgpErr::G_NoTaxid, string(" for ") + dl.component_id);
      //AGP_ERROR("Unable to get Entrez Docsum.");
      return 0;
    }
  }

  if (m_SpeciesLevelTaxonCheck) {
    taxid = x_GetSpecies(taxid);
  }
  return taxid;
}

int CAgpValidateApplication::x_GetSpecies(int taxid)
{
  TTaxidSpeciesMap::iterator map_it;
  map_it = m_TaxidSpeciesMap.find(taxid);
  int species_id;

  if (map_it == m_TaxidSpeciesMap.end()) {
    species_id = x_GetTaxonSpecies(taxid);
    m_TaxidSpeciesMap.insert(
        TTaxidSpeciesMap::value_type(taxid, species_id) );
  } else {
    species_id = map_it->second;
  }

  return species_id;
}

int CAgpValidateApplication::x_GetTaxonSpecies(int taxid)
{
  CTaxon1 taxon;
  taxon.Init();

  bool is_species = true;
  bool is_uncultured;
  string blast_name;
  string blast_name0;

  int species_id = 0;
  int prev_id = taxid;


  for(int id = taxid;
      is_species == true && id > 1;
      id = taxon.GetParent(id)
  ) {
    CConstRef<COrg_ref> org_ref = taxon.GetOrgRef(
      id, is_species, is_uncultured, blast_name
    );
    if(org_ref == null) {
      agpErr.Msg(CAgpErr::G_NoOrgRef,
        string(" ")+ NStr::IntToString(id) );
      // AGP_ERROR( "GetOrgRef() returned NULL for taxid "
      //  << id);
      return 0;
    }
    if(id==taxid) blast_name0=blast_name;

    if (is_species) {
        prev_id = id;
    } else {
        species_id = prev_id;
    }
  }

  if(species_id==0) {
    if(blast_name0.size()) {
      blast_name0 = NStr::IntToString(taxid) + " (" + blast_name0 + ")";
    }
    else{
      blast_name0 = NStr::IntToString(taxid);
    }
    agpErr.Msg(CAgpErr::G_AboveSpeciesLevel,
      NcbiEmptyString, AT_ThisLine, // defaults
      blast_name0 );
  }

  return species_id;
}


void CAgpValidateApplication::x_AddToTaxidMap(
  int taxid, const SDataLine& dl)
{
    SAgpLineInfo line_info;

    line_info.filename = m_CurrentFileName;
    line_info.line_num = dl.line_num;
    line_info.component_id = dl.component_id;

    TAgpInfoList info_list;
    TTaxidMapRes res = m_TaxidMap.insert(
      TTaxidMap::value_type(taxid, info_list)
    );
    (res.first)->second.push_back(line_info);
    m_TaxidComponentTotal++;
}


void CAgpValidateApplication::x_CheckTaxid()
{
  if (m_TaxidMap.size() == 0) return;

  int agp_taxid = 0;
  float agp_taxid_percent = 0;

  // determine the taxid for the agp
  ITERATE(TTaxidMap, it, m_TaxidMap) {
      agp_taxid_percent =
        float(it->second.size())/float(m_TaxidComponentTotal);
      if (agp_taxid_percent >= .8) {
          agp_taxid = it->first;
          break;
      }
  }

  if (!agp_taxid) {
      AGP_POST("Unable to determine a Taxid for the AGP");
      // todo: print most popular taxids
      return;
  }

  AGP_POST("The AGP taxid is: " << agp_taxid);
  if (m_TaxidMap.size() == 1) return;

  AGP_POST( "Components with incorrect taxids:");

  // report components that have an incorrect taxid
  ITERATE(TTaxidMap, map_it, m_TaxidMap) {
    if (map_it->first == agp_taxid) continue;

    int taxid = map_it->first;
    ITERATE(TAgpInfoList, list_it, map_it->second) {
      AGP_POST( "\t"
        << list_it->filename     << ", "
        << list_it->line_num     << ": "
        << list_it->component_id
        << " - Taxid " << taxid
      );
    }
  }
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

