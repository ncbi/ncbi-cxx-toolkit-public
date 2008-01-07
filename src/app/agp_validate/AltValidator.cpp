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
 *      Checks that use component Accession, Length and Taxid info from GenBank.
 *
 */

#include <ncbi_pch.hpp>

#include "AltValidator.hpp"

// Objects includes
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

BEGIN_NCBI_SCOPE

static CRef<CObjectManager> m_ObjectManager;
static CRef<CScope> m_Scope;
static int x_GetTaxid(CBioseq_Handle& bioseq_handle, const string& comp_id);


void CAltValidator::Init(void)
{
  m_TaxidComponentTotal = 0;
  m_SpeciesLevelTaxonCheck = false;
  m_GenBankCompLineCount=0;

  // Create an object manager
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

// Returns GI
int CAltValidator::ValidateAccession( const string& acc, int* missing_ver )
{
  CSeq_id seq_id;
  int gi=0;
  if(missing_ver) *missing_ver=0; // not missing

  try {
    seq_id.Set(acc);
  }
  //    catch (CSeqIdException::eFormat)
  catch (...) {
    agpErr.Msg(CAgpErrEx::G_InvalidCompId, string(": ")+acc);
    return 0;
  }

  try {
    CBioseq_Handle bioseq_handle=m_Scope->GetBioseqHandle(seq_id);
    if( !bioseq_handle ) {
      agpErr.Msg(CAgpErrEx::G_NotInGenbank, string(": ")+acc);
      return 0;
    }

    //// Warn if no version was supplied and GenBank version is > 1
    SIZE_TYPE pos_ver = NStr::Find( acc, "."); //, 0, NPOS, NStr::EOccurrence::eLast);
    if(pos_ver==NPOS) {
      string acc_ver = sequence::GetAccessionForId(seq_id, *m_Scope);
      pos_ver = NStr::Find( acc_ver, "."); //, 0, NPOS, NStr::EOccurrence::eLast);
      if(pos_ver==NPOS) {
        cerr << "FATAL: cannot get version for " << acc << "\n";
        exit(1);
      }
      int ver = NStr::StringToInt( acc_ver.substr(pos_ver+1) );
      if(ver>1) {
        agpErr.Msg(CAgpErrEx::G_NeedVersion,
          acc + string(" (current version ")+acc_ver+")",
          CAgpErr::fAtThisLine);
      }

      if(missing_ver) *missing_ver=ver;
    }

    try {
      CSeq_id_Handle seq_id_handle = sequence::GetId(
        bioseq_handle, sequence::eGetId_ForceGi
      );
      gi=seq_id_handle.GetGi();
    }
    catch (...) {
      agpErr.Msg(CAgpErrEx::G_DataError,
        string(" - cannot get GI for ") + acc);
      return 0;
    }

  }
  catch (...) {
    agpErr.Msg(CAgpErrEx::G_DataError,
      string(" - cannot get version for ") + acc);
    return 0;
  }


  return gi;
}

void CAltValidator::ValidateLength(
  const string& comp_id, int comp_end, int comp_len)
{
  if( comp_end > comp_len) {
    string details=": ";
    details += NStr::IntToString(comp_end);
    details += " > ";
    details += comp_id;
    details += " length = ";
    details += NStr::IntToString(comp_len);
    details += " bp";

    agpErr.Msg(CAgpErrEx::G_CompEndGtLength, details );
  }
}

void CAltValidator::ValidateLine(
  const string& comp_id,
  int line_num, int comp_end)
{
  //// Check the accession
  CSeq_id seq_id;
  try {
      seq_id.Set(comp_id);
  }
  //    catch (CSeqIdException::eFormat)
  catch (...) {
    agpErr.Msg(CAgpErrEx::G_InvalidCompId, string(": ")+comp_id);
    return;
  }

  CBioseq_Handle bioseq_handle=m_Scope->GetBioseqHandle(seq_id);
  if( !bioseq_handle ) {
    agpErr.Msg(CAgpErrEx::G_NotInGenbank, string(": ")+comp_id);
    return;
  }

  m_GenBankCompLineCount++; // component_id is a valid GenBank accession

  //// Warn if no version was supplied and GenBank version is > 1
  SIZE_TYPE pos_ver = NStr::Find( comp_id, "."); //, 0, NPOS, NStr::EOccurrence::eLast);
  if(pos_ver==NPOS) {
    string acc_ver = sequence::GetAccessionForId(seq_id, *m_Scope);
    pos_ver = NStr::Find( acc_ver, "."); //, 0, NPOS, NStr::EOccurrence::eLast);
    if(pos_ver==NPOS) {
      cerr << "FATAL: cannot get version for " << comp_id << "\n";
      exit(1);
    }
    int ver = NStr::StringToInt( acc_ver.substr(pos_ver+1) );
    if(ver>1) {
      agpErr.Msg(CAgpErrEx::G_NeedVersion,
        comp_id + string(" (current version is ")+acc_ver+")",
        CAgpErr::fAtThisLine);
    }
  }

  if(m_check_len_taxid==false) return;

  // Component out of bounds check
  ValidateLength(
    comp_id, comp_end,
    (int) bioseq_handle.GetInst_Length()
  );

  // Taxid check
  int taxid = x_GetTaxid(bioseq_handle, comp_id);
  if(m_SpeciesLevelTaxonCheck) {
    taxid = x_GetSpecies(taxid);
  }
  x_AddToTaxidMap(taxid, comp_id, line_num);
}

void CAltValidator::PrintTotals()
{
  int e_count=agpErr.CountTotals(CAgpErrEx::CODE_First, CAgpErrEx::CODE_Last);

  if(m_GenBankCompLineCount) {
    cout << m_GenBankCompLineCount << " lines with GenBank component accessions";
  }
  else{
    cout << "No GenBank components found";
  }
  if(e_count) {
    if(e_count==1) cout << ".\n1 error";
    else cout << ".\n" << e_count << " errors";
    if(agpErr.m_msg_skipped) cout << ", " << agpErr.m_msg_skipped << " not printed";
    cout << ":\n";
    agpErr.PrintMessageCounts(cout, CAgpErrEx::CODE_First, CAgpErrEx::CODE_Last);
  }
  else {
    cout << "; no non-GenBank component accessions.\n";
  }
}

int x_GetTaxid(
  CBioseq_Handle& bioseq_handle, const string& comp_id)
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
      agpErr.Msg(CAgpErrEx::G_DataError,
        string(" - cannot retrieve taxonomic id for ") +
        comp_id);
      return 0;
    }
  }

  return taxid;
}

int CAltValidator::x_GetSpecies(int taxid)
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

int CAltValidator::x_GetTaxonSpecies(int taxid)
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
      agpErr.Msg(CAgpErrEx::G_TaxError,
        string(" for taxid ")+ NStr::IntToString(id) );
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
      blast_name0 = string("taxid ") + NStr::IntToString(taxid);
    }
    agpErr.Msg(CAgpErrEx::G_DataError,
      string(" - ") + blast_name0 +
      " is above species level");
  }

  return species_id;
}


void CAltValidator::x_AddToTaxidMap(
  int taxid, const string& comp_id, int line_num)
{
  SAgpLineInfo line_info;

  line_info.file_num = agpErr.GetFileNum();
  line_info.line_num = line_num;
  line_info.component_id = comp_id;

  TAgpInfoList info_list;
  TTaxidMapRes res = m_TaxidMap.insert(
    TTaxidMap::value_type(taxid, info_list)
  );
  (res.first)->second.push_back(line_info);
  m_TaxidComponentTotal++;
}

void CAltValidator::CheckTaxids()
{
  if (m_TaxidMap.size() == 0) return;

  int agp_taxid = 0;
  float agp_taxid_percent = 0;

  float max_percent = 0;
  bool taxid_found=false;

  // Find the most common the taxid
  ITERATE(TTaxidMap, it, m_TaxidMap) {
    agp_taxid_percent = float(it->second.size())/float(m_TaxidComponentTotal);
    if(agp_taxid_percent > max_percent) {
      max_percent = agp_taxid_percent;
      agp_taxid = it->first;
      if(agp_taxid_percent>=.8) {
        taxid_found=true;
        break;
      }
    }
  }

  if(!taxid_found) {
    cerr << "\nUnable to determine a Taxid for the AGP";
    if(agp_taxid) {
      cerr << ":\nless than 80% of components have one common taxid="<<agp_taxid<<"";
    }
    // else: no taxid was found
    cerr << ".\n";
    return;
  }

  cerr << "The AGP taxid is: " << agp_taxid << "\n";
  if (m_TaxidMap.size() == 1) return;

  cerr << "Components with incorrect taxids:\n";

  // report components that have an incorrect taxid
  ITERATE(TTaxidMap, map_it, m_TaxidMap) {
    if (map_it->first == agp_taxid) continue;

    int taxid = map_it->first;
    ITERATE(TAgpInfoList, list_it, map_it->second) {
      cerr << "\t";
      if(list_it->file_num) {
        cerr << agpErr.GetFile(list_it->file_num)  << ":";
      }
      cerr<< list_it->line_num << ": " << list_it->component_id
          << " - Taxid " << taxid << "\n";
    }
  }
}

//// Batch processing using queue
void CAltValidator::QueueLine(
  const string& orig_line, const string& comp_id,
  int line_num, int comp_end)
{
  //int gi = ValidateAccession(comp_id);
  //if(gi==0) return false;

  SLineData ld;
  ld.orig_line = orig_line;
  ld.comp_id = comp_id;
  ld.line_num = line_num;
  ld.comp_end = comp_end;
  lineQueue.push_back(ld);
}

void CAltValidator::QueueLine(const string& orig_line)
{
  SLineData ld;
  ld.orig_line = orig_line;
  // ld.comp_id remains empty
  lineQueue.push_back(ld);
}

void CAltValidator::ProcessQueue()
{
  //// Collect GIs, accession-related errors and warnings
  vector<int> uids;
  for(TLineQueue::iterator it=lineQueue.begin();
      it != lineQueue.end(); ++it
  ) {

    if(it->comp_id.size()==0) {
      // a line to be printed verbatim and not to be processed
      if(m_out) {
        *m_out << it->orig_line << "\n";
      }
      continue;
    }

    int missing_ver;
    int gi = ValidateAccession(it->comp_id, &missing_ver);

    if(m_out) {
      if(missing_ver==1) {
        // add missing version 1
        SIZE_TYPE pos =
          it->orig_line.find( "\t", 1+
          it->orig_line.find( "\t", 1+
          it->orig_line.find( "\t", 1+
          it->orig_line.find( "\t", 1+
          it->orig_line.find( "\t", 1+
          it->orig_line.find( "\t") )))));
        *m_out << it->orig_line.substr(0, pos);
        if(pos!=NPOS) *m_out << ".1" << it->orig_line.substr(pos);
      }
      else {
        *m_out << it->orig_line;
        if(missing_ver!=0) *m_out << "#current version " << it->comp_id << "." << missing_ver;
      }
      *m_out << "\n";
    }

    if( agpErr.m_messages->pcount() ) {
      // Error or warning when converting comp_id to GI.
      // Save to print later.
      it->messages = agpErr.m_messages;
      agpErr.m_messages =  new CNcbiOstrstream();
    }

    it->gi = gi;
    if(gi) {
      uids.push_back(gi);
      // component_id is a valid GenBank accession
      m_GenBankCompLineCount++;
    }

    // // temporary code for testing how the queue functions
    // // (with no performance improvement, absent the batch Entrez retrieval)
    // ValidateLine(it->comp_id, it->line_num, it->comp_end);
    // agpErr.LineDone(it->orig_line, it->line_num);
  }

  //// Get docsums for the collected GIs, validate lengths and taxids,
  //// print messages to cerr.
  if( uids.size() ) {

    // Retrieve docsums
    CEntrez2Client entrez;
    CRef<CEntrez2_docsum_list> cref_docsums = entrez.GetDocsums(uids, "Nucleotide");
    const CEntrez2_docsum_list::TList& docsums = cref_docsums->GetList();

    // Walk through both doscums and lineQueue
    CEntrez2_docsum_list::TList::const_iterator
      it_docsum =   docsums.begin();
    TLineQueue::iterator
      it_line   = lineQueue.begin();

    // it_docsum != docsums.end() &&
    while(it_line != lineQueue.end() ) {

      if(it_line->messages) {
        // ASSERT(agpErr.m_messages.pcount()==0)
        delete(agpErr.m_messages);
        agpErr.m_messages = it_line->messages; // Accession without version
      }
      string orig_line = it_line->orig_line;
      int line_num = it_line->line_num;

      if( it_line->gi == 0 ) {
        // Assume it_line->messages contains the appropriate complaint
        it_line++;
      }
      // CRef<CEntrez2_docsum> docsum = *it_docsums;
      else if( it_line->gi == (*it_docsum)->GetUid() ) {
        // To do: validate length and taxid using (*it_docsum)

        // try{} ?

        int taxid_from_handle = 0;
        // Component out of bounds check
        string docsum_slen = (*it_docsum)->GetValue("Slen");
        if(docsum_slen != "") {
          int slen = NStr::StringToInt(docsum_slen);
          if(slen==0) {
            // A workaround for a possible Entrez bug/feature:
            // 0 taxid and length for replaced seqs
            CSeq_id seq_id(CSeq_id::e_Gi, it_line->gi);
            CBioseq_Handle bioseq_handle = m_Scope->GetBioseqHandle(seq_id);

            slen = (int) bioseq_handle.GetInst_Length();
            taxid_from_handle = x_GetTaxid(bioseq_handle, it_line->comp_id);
          }
          ValidateLength( it_line->comp_id, it_line->comp_end, slen );
        }
        else {
          agpErr.Msg(CAgpErrEx::G_DataError,
            string(" - cannot retrieve sequence length for ") +
            it_line->comp_id);
        }

        // Taxid check
        string docsum_taxid = (*it_docsum)->GetValue("TaxId");
        if (docsum_taxid != "") {
          int taxid = NStr::StringToInt(docsum_taxid);
          if(taxid==0) {
            // A workaround for possible Entrez bug/feature
            if(taxid_from_handle) {
              taxid = taxid_from_handle;
            }
            else {
              CSeq_id seq_id(CSeq_id::e_Gi, it_line->gi);
              CBioseq_Handle bioseq_handle = m_Scope->GetBioseqHandle(seq_id);
              taxid = x_GetTaxid(bioseq_handle, it_line->comp_id);
            }
          }
          if(m_SpeciesLevelTaxonCheck) {
            taxid = x_GetSpecies(taxid);
          }
          x_AddToTaxidMap(taxid, it_line->comp_id, it_line->line_num);
        }
        else {
          agpErr.Msg(CAgpErrEx::G_DataError,
            string(" - cannot retrieve taxonomic id for ") +
            it_line->comp_id);
        }

        it_docsum++;
        it_line++;
      }
      else {
        // Assume that it_line->gi did not have docsum (weird)
        agpErr.Msg(CAgpErrEx::G_DataError,
          string(" - cannot retrieve docsum for ") + it_line->comp_id +
          string(", GI=")+ NStr::IntToString(it_line->gi) );

        it_line++;
      }

      agpErr.LineDone(orig_line, line_num);
    }
    if( it_docsum != docsums.end() ) {
      cerr<< "Data transfer error: unexpected GI " << (*it_docsum)->GetUid()
          << " in CEntrez2_docsum_list\n";
      exit(1);
    }

  }

  lineQueue.clear();
}

END_NCBI_SCOPE

