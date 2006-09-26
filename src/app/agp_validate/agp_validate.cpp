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
 *      or semantic validation. Syntactic validation uses only the information
 *      in the AGP file. Semantic validation queries sequence length and taxid
 *      via ObjectManager or CEntrez2Client.
 *
 */

#include <ncbi_pch.hpp>
#include "SyntaxValidator.hpp"
//#include "AgpErr.hpp"

// Objects includes
// todo: move this to a separate .cpp file with semantic validator
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
// todo: move this to a separate .cpp file with semantic validator
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

//// Legacy macros for semantic validation; to be removed later.
#define START_LINE_VALIDATE_MSG \
        m_LineErrorOccured = false;

#define AGP_POST(msg) cerr << msg << "\n"

#define END_LINE_VALIDATE_MSG(line_num, line)\
  AGP_POST( line_num << ": " << line <<\
    (string)CNcbiOstrstreamToString(*m_ValidateMsg) << "\n");\
  delete m_ValidateMsg;\
  m_ValidateMsg = new CNcbiOstrstream;

#define AGP_MSG(severity, msg) \
        m_LineErrorOccured = true; \
        *m_ValidateMsg << "\n\t" << severity << ": " <<  msg
#define AGP_ERROR(msg) agp_error_count++; AGP_MSG("ERROR", msg)
#define AGP_WARNING(msg) agp_warn_count++; AGP_MSG("WARNING", msg)
////////////////////////////////////

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
      syntax, semantic
  } m_ValidationType;
  bool m_SpeciesLevelTaxonCheck;

  CRef<CObjectManager> m_ObjectManager;
  CRef<CScope> m_Scope;

  //CRef<CDb>   m_VolDb;

  // In future, may hold other validator types
  CAgpSyntaxValidator* m_LineValidator;

  bool m_LineErrorOccured;
  CNcbiOstrstream* m_ValidateMsg;


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
  //  populates a SDataLine. The SDataline is validated
  //  syntatically or semanticaly.
  //
  void x_ValidateUsingDB(const CArgs& args);
  void x_ValidateUsingFiles(const CArgs& args);
  void x_ValidateFile(CNcbiIstream& istr);

  //
  // Semantic validate methods
  //
  void x_ValidateSemanticInit();
  void x_ValidateSemanticLine(const SDataLine& line,
    const string& text_line);

  // vsap: printableId is used for error messages only
  int x_GetTaxid(CBioseq_Handle& bioseq_handle);
  void x_AddToTaxidMap(int taxid, const SDataLine& dl);
  void x_CheckTaxid();
  int x_GetSpecies(int taxid);
  int x_GetTaxonSpecies(int taxid);
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
    "  -type semantics   Check sequence length and taxids using GenBank data\n"
    "  -type syntax      (Default) Check line formatting and data consistency\n"
    "  -taxon species    Allow sequences from different subspecies during semantic check\n"
    "  -taxon exact      (Default)\n"
    "\n"
    "  -list         List possible syntax errors and warnings.\n"
    "  -limit COUNT  Print only the first COUNT messages of each type (default=10).\n"
    "\n"
    "  -skip  WHAT   Do not report lines with a particular error or warning message.\n"
    "  -only  WHAT   Report only this particular error or warning.\n"
    "  Multiple -skip or -only are allowed. 'WHAT' may be:\n"
    "  - error code (e01,.. w21,.. - see '-list')\n"
    "  - part of the actual message\n"
    "  - keyword: all warn[ings] err[ors]\n"
    "\n"
    ;
    return str;
    // To do:
    // -taxon "taxname or taxid"
    // -s    both syntAx and semantics
  }
};

void CAgpValidateApplication::Init(void)
{
  //auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
  auto_ptr<CArgDesc_agp_validate> arg_desc(new CArgDesc_agp_validate);

  arg_desc->SetUsageContext(
    GetArguments().GetProgramBasename(),
    "Validate AGP data", false);

  arg_desc->AddDefaultKey("type", "ValidationType",
    "Type of validation",
    CArgDescriptions::eString,
    "syntax");
  CArgAllow_Strings* constraint_type = new CArgAllow_Strings;
  constraint_type->Allow("syntax");
  constraint_type->Allow("semantics");
  arg_desc->SetConstraint("type", constraint_type);

  arg_desc->AddDefaultKey("taxon", "TaxonCheckType",
    "Type of Taxonomy semanitic check to be performed",
    CArgDescriptions::eString,
    "exact");
  CArgAllow_Strings* constraint_taxon= new CArgAllow_Strings;
  constraint_taxon->Allow("exact");
  constraint_taxon->Allow("species");
  arg_desc->SetConstraint("taxon", constraint_taxon);

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
  arg_desc->AddExtra(0,100, "files to be processed",
                      CArgDescriptions::eInputFile);
  // Setup arg.descriptions for this application
  SetupArgDescriptions(arg_desc.release());

  m_TaxidComponentTotal = 0;

  m_ValidateMsg = new CNcbiOstrstream;
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

  if (args["type"].AsString() == "syntax") {
    m_ValidationType = syntax;
    m_LineValidator = new CAgpSyntaxValidator();
  }
  else { // args_type == "semantic"
    x_ValidateSemanticInit();
    m_ValidationType = semantic;
  }

  m_SpeciesLevelTaxonCheck = false;
  if(args["taxon"].AsString() == "species") {
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


  agpErr.m_MaxRepeat = args["limit"].AsInteger();

  //// Process files, print results
  x_ValidateUsingFiles(args);
  if(m_ValidationType == syntax) {
    m_LineValidator->PrintTotals();
  }
  else {
    x_CheckTaxid();
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
  if (m_ValidationType == syntax) {
    m_LineValidator->EndOfObject();
  }
}

void CAgpValidateApplication::x_ValidateFile(
  CNcbiIstream& istr)
{
  int line_num = 0;
  string  line;
  SDataLine data_line;
  vector<string> cols;
  while (NcbiGetlineEOL(istr, line)) {
    line_num++;

    // Strip #comments
    {
      SIZE_TYPE pos = NStr::Find(line, "#");
      if(pos != NPOS) {
        while( pos>0 && line[pos-1]==' ') pos--;
        line.resize(pos);
      }
    }
    if (line == "") continue;

    bool invalid_line=false;
    cols.clear();
    NStr::Tokenize(line, "\t", cols);
    if( cols.size() < 8 || cols.size() > 9 ) {
      // skip this entire line, report an error
      agpErr.Msg(CAgpErr::E_ColumnCount,
        string(", found ") + NStr::IntToString(cols.size()) );
      invalid_line=true;
    }
    else {
      data_line.line_num = line_num;

      // 5 common columns for components and gaps.
      data_line.object         = cols[0];
      data_line.begin          = cols[1];
      data_line.end            = cols[2];
      data_line.part_num       = cols[3];
      data_line.component_type = cols[4];

      // columns with different meaning for components an gaps.
      data_line.component_id   = cols[5];
      data_line.gap_length     = cols[5];

      data_line.component_start = cols[6];
      data_line.gap_type        = cols[6];

      data_line.component_end   = cols[7];
      data_line.linkage         = cols[7];

      data_line.orientation = "";
      if (data_line.component_type != "N") {
        // Component
        if (cols.size() > 8) data_line.orientation = cols[8];
      }

      if (m_ValidationType == syntax) {
        // x_ValidateSyntaxLine(data_line, line);
        m_LineValidator->ValidateLine(data_line, line);
      } else {
        x_ValidateSemanticLine(data_line, line);
      }
    }

    agpErr.LineDone(line, line_num, invalid_line);
  }
}

void CAgpValidateApplication::x_ValidateSemanticInit()
{
  // Create object manager
  // * CRef<> here will automatically delete the OM on exit.
  // * While the CRef<> exists GetInstance() returns the same
  //   object.
  m_ObjectManager.Reset(CObjectManager::GetInstance());

  // Create GenBank data loader and register it with the OM.
  // * The GenBank loader is automatically marked as a default
  // * to be included in scopes during the CScope::AddDefaults()
  CGBDataLoader::RegisterInObjectManager(*m_ObjectManager);

  // Create a new scope ("attached" to our OM).
  m_Scope.Reset(new CScope(*m_ObjectManager));
  // Add default loaders (GB loader in this demo) to the scope.
  m_Scope->AddDefaults();
}

void CAgpValidateApplication::x_ValidateSemanticLine(
  const SDataLine& dl, const string& text_line)
{
  if(dl.component_type == "N") return; // gap

  START_LINE_VALIDATE_MSG;

  CSeq_id seq_id;
  try {
      seq_id.Set(dl.component_id);
  }
  //    catch (CSeqIdException::eFormat)
  catch (...) {
    AGP_ERROR( "Invalid component id: " << dl.component_id);
  }
  CBioseq_Handle bioseq_handle=m_Scope->GetBioseqHandle(seq_id);
  if( !bioseq_handle ) {
    AGP_ERROR( "Component not in Genbank: "<< dl.component_id);
    return;
  }

  // Component out of bounds check
  CBioseq_Handle::TInst_Length seq_len =
    bioseq_handle.GetInst_Length();
  if (NStr::StringToUInt(dl.component_end) > seq_len) {
      AGP_ERROR( "Component end greater than sequence length: "
        << dl.component_end << " > "
        << dl.component_id << " length = "
        << seq_len << " bp");
  }

  int taxid = x_GetTaxid(bioseq_handle);
  x_AddToTaxidMap(taxid, dl);

  if (m_LineErrorOccured) {
    END_LINE_VALIDATE_MSG(dl.line_num, text_line);
  }
}

int CAgpValidateApplication::x_GetTaxid(
  CBioseq_Handle& bioseq_handle)
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
      AGP_ERROR("Unable to get Entrez Docsum.");
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

  int species_id = 0;
  int prev_id = taxid;

  for(int id = taxon.GetParent(taxid);
      is_species == true && id > 1;
      id = taxon.GetParent(id)
  ) {
    CConstRef<COrg_ref> org_ref = taxon.GetOrgRef(
      id, is_species, is_uncultured, blast_name
    );
    if(org_ref == null) {
      AGP_ERROR( "GetOrgRef() returned NULL for taxid "
        << id);
      break;
    }

    if (is_species) {
        prev_id = id;
    } else {
        species_id = prev_id;
    }
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
  if (m_TaxidMap.size() == 1) return;

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

  //LOG_POST(Error<< "The AGP's taxid is: " << agp_taxid);
  //LOG_POST_ONCE(Error<< "Components with incorrect taxids:");
  AGP_POST("The AGP's taxid is: " << agp_taxid);
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


/*
 * ===========================================================================
 * Revision 1.10  2006/08/29 21:31:56  sapojnik
 * "Syntax" validations moved to a separate class CAgpSyntaxValidator
 *
 * Revision 1.9  2006/08/29 18:35:51  sapojnik
 * printing of semantic errors fixed; do not create a new CNcbiOstrstream for each line unless there were errors
 *
 * Revision 1.8  2006/08/29 16:21:04  ucko
 * Allow END_LINE_VALIDATE_MSG to take advantage of CNcbiOstrstreamToString
 * rather than having to reimplement it.
 *
 * Revision 1.7  2006/08/29 16:07:34  sapojnik
 * a workaround for strstream.str() bug; TIdMap* types renamed to TComp(Id|Span)*; counting scaffolds and singletons (does not work right yet); catching unwarranted GAPs at EOF
 *
 * Revision 1.6  2006/08/28 19:52:02  sapojnik
 * A nicer usage message via CArgDesc_agp_validate; more reformatting
 *
 * Revision 1.5  2006/08/28 16:43:15  sapojnik
 * Reformatting, fixing typos, improving error messages and a few names
 *
 * Revision 1.3  2006/08/11 16:46:05  sapojnik
 * Print Unique Component Accessions, do not complain about the order of "-" spans
 *
 * Revision 1.1  2006/03/29 19:51:12  friedman
 * Initial version
 *
 * ===========================================================================
 */
