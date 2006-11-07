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

static CRef<CObjectManager> m_ObjectManager;
static CRef<CScope> m_Scope;
static int x_GetTaxid(CBioseq_Handle& bioseq_handle, const SDataLine& dl);


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

void CAltValidator::ValidateLine(
  const SDataLine& dl, int comp_end)
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

  //if(m_ValidationType & VT_Len) {
    // Component out of bounds check
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
    }
  //}

  //if(m_ValidationType & VT_Taxid) {
    int taxid = x_GetTaxid(bioseq_handle, dl);
    if(m_SpeciesLevelTaxonCheck) {
      taxid = x_GetSpecies(taxid);
    }

    x_AddToTaxidMap(taxid, dl);
  //}
}

void CAltValidator::PrintTotals()
{
  int e_count=agpErr.CountTotals(CAgpErr::CODE_First, CAgpErr::CODE_Last);

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
    agpErr.PrintMessageCounts(cout, CAgpErr::CODE_First, CAgpErr::CODE_Last);
  }
  else {
    cout << "; no non-GenBank component accessions.\n";
  }
}

int x_GetTaxid(
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
      agpErr.Msg(CAgpErr::G_TaxError,
        string(" - cannot retrieve the taxonomic id for ") +
        dl.component_id);
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
      agpErr.Msg(CAgpErr::G_TaxError,
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
    agpErr.Msg(CAgpErr::G_TaxError,
      string(" - ") + blast_name0 +
      " is above species level");
  }

  return species_id;
}


void CAltValidator::x_AddToTaxidMap(
  int taxid, const SDataLine& dl)
{
    SAgpLineInfo line_info;

    line_info.file_num = agpErr.GetFileNum();
    line_info.line_num = dl.line_num;
    line_info.component_id = dl.component_id;

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
      cerr << "\t";
      if(list_it->file_num) {
        cerr << agpErr.GetFile(list_it->file_num)  << ":";
      }
      cerr<< list_it->line_num << ": " << list_it->component_id
          << " - Taxid " << taxid << "\n";
    }
  }
}
END_NCBI_SCOPE

