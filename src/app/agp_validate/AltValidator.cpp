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
 *      Check component Accession, Length and Taxid using info from GenBank.
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

using namespace ncbi;
using namespace objects;

#ifndef s2i
#define s2i(x) NStr::StringToNonNegativeInt(x)
#endif

BEGIN_NCBI_SCOPE

static CRef<CObjectManager> m_ObjectManager;
static CRef<CScope> m_Scope;


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

// Returns GI // int* missing_ver

int CAltValidator::GetAccDataFromObjMan( const string& acc, SGiVerLenTaxid& acc_data )
{

  CSeq_id seq_id;
  try {
    seq_id.Set(acc);
  }
  //    catch (CSeqIdException::eFormat)
  catch (...) {
    agpErr.Msg(CAgpErrEx::G_InvalidCompId, string(": ")+acc);
    return 0;
  }


  CBioseq_Handle bioseq_handle;
  try {
    bioseq_handle=m_Scope->GetBioseqHandle(seq_id);
    if( !bioseq_handle ) {
      agpErr.Msg(CAgpErrEx::G_NotInGenbank, string(": ")+acc);
      return 0;
    }

    if(acc_data.gi<=0) {
      try {
        CSeq_id_Handle seq_id_handle = sequence::GetId(
          bioseq_handle, sequence::eGetId_ForceGi
        );
        acc_data.gi=seq_id_handle.GetGi();

        // seq_id_handle.GetSeqId()->GetSeqIdString(sequence::eWithAccessionVersion);
        string acc_ver = sequence::GetAccessionForGi( acc_data.gi, *m_Scope );
        SIZE_TYPE pos_dot = acc_ver.find('.');
        if( pos_dot!=NPOS && pos_dot<acc_ver.size()-1 ) {
          acc_data.ver = s2i( acc_ver.substr(pos_dot+1) );
        }
      }
      catch (...) {
        agpErr.Msg(CAgpErrEx::G_DataError,
          string(" - cannot get GI for ") + acc);
        return 0;
      }
    }

    if(m_check_len_taxid) {
      acc_data.len   = (int) bioseq_handle.GetInst_Length();
      acc_data.taxid =  sequence::GetTaxId(bioseq_handle);;\
      if(acc_data.taxid<=0) {
        agpErr.Msg(CAgpErrEx::G_DataError,
          " - cannot retrieve taxonomic id for "+acc);
      }
    }

  }
  catch (...) {
    agpErr.Msg(CAgpErrEx::G_DataError,
      string(" - cannot get version for ") + acc);
    return 0;
  }

  return acc_data.gi;
}

/*
// to do: use CAgpRow::CheckComponentEnd()
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
*/

void CAltValidator::PrintTotals(CNcbiOstream& out, bool use_xml)
{
  int e_count=agpErr.CountTotals(CAgpErrEx::E_Last) + agpErr.CountTotals(CAgpErrEx::G_Last);

  if(use_xml) {
    cout << " <LinesWithValidCompAcc>" << m_GenBankCompLineCount << "</LinesWithValidCompAcc>\n";
    cout << " <errors>" << e_count << "</errors>\n";
    // >0: too many messages
    cout << " <skipped>" << agpErr.m_msg_skipped << "</skipped>\n";
    agpErr.PrintMessageCounts(out, CAgpErrEx::CODE_First, CAgpErrEx::CODE_Last, true);
  }
  else {
    if(m_GenBankCompLineCount) {
      out << m_GenBankCompLineCount << " lines with valid component accessions";
    }
    else{
      out << "No valid component accessions found";
    }
    if(e_count) {
      if(e_count==1) out << ".\n1 error";
      else out << ".\n" << e_count << " errors";
      if(agpErr.m_msg_skipped) out << ", " << agpErr.m_msg_skipped << " not printed";
      out << ":\n";
      agpErr.PrintMessageCounts(out, CAgpErrEx::CODE_First, CAgpErrEx::CODE_Last, true);
    }
    else {
      out << "; no invalid component accessions.\n";
    }
  }
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

bool CAltValidator::CheckTaxids(CNcbiOstream& out, bool use_xml)
{
  if (m_TaxidMap.size() == 0) return true;

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

  if(use_xml) out << " <taxid>" << agp_taxid << "</taxid>\n";
  if(!taxid_found) {
    if(use_xml) {
      out << " <cannot_determine_taxid/>\n";
    }
    else {
      cerr << "\nUnable to determine a Taxid for the AGP";
      if(agp_taxid) {
        cerr << ":\nless than 80% of components have one common taxid="<<agp_taxid<<"";
      }
      // else: no taxid was found
      cerr << ".\n";
    }
    return false;
  }

  if(!use_xml) out << "The AGP taxid is: " << agp_taxid << endl;
  if (m_TaxidMap.size() == 1) return true;

  if(!use_xml) cerr << "Components with incorrect taxids:\n";

  // report components that have an incorrect taxid
  ITERATE(TTaxidMap, map_it, m_TaxidMap) {
    if (map_it->first == agp_taxid) continue;

    int taxid = map_it->first;
    ITERATE(TAgpInfoList, list_it, map_it->second) {
      if(use_xml) {
         out << " <CompBadTaxid taxid=\"" << taxid
             << "\" line_num=\"" << list_it->line_num
             << (list_it->file_num ? string(
                  "\" filename=\"" + NStr::XmlEncode(agpErr.GetFile(list_it->file_num))
                ) : NcbiEmptyString)
             << "\">" <<  NStr::XmlEncode(list_it->component_id)<< "</CompBadTaxid>\n";
      }
      else {
        cerr << "\t";
        if(list_it->file_num) {
          cerr << agpErr.GetFile(list_it->file_num)  << ":";
        }
        cerr<< list_it->line_num << ": " << list_it->component_id
            << " - Taxid " << taxid << "\n";
      }
    }
  }
  if(!use_xml) cerr << "\n";
  return false;
}

//// Batch processing using queue
void CAltValidator::QueueLine(
  const string& orig_line, const string& comp_id,
  int line_num, int comp_end)
{
  SLineData ld;
  ld.orig_line = orig_line;
  ld.comp_id = comp_id;
  ld.line_num = line_num;
  ld.comp_end = comp_end;
  lineQueue.push_back(ld);

  accessions.insert(comp_id);
}

void CAltValidator::QueueLine(const string& orig_line)
{
  SLineData ld;
  ld.orig_line = orig_line;
  // ld.comp_id remains empty
  lineQueue.push_back(ld);
}

// In: accessions; Out: mapAccData.
void CAltValidator::QueryAccessions()
{
  string query;
  for(set<string>::iterator it = accessions.begin();  it != accessions.end(); ++it) {
    query+=*it;
    query+="[PACC] OR ";
  }
  if(query.size()==0) return;
  query.resize( query.size()-4 );

  try{
    CEntrez2Client entrez;
    vector<int> gis;
    entrez.Query(query, "Nucleotide", gis);

    CRef<CEntrez2_docsum_list> cref_docsums = entrez.GetDocsums(gis, "Nucleotide");
    const CEntrez2_docsum_list::TList& docsums = cref_docsums->GetList();

    for(CEntrez2_docsum_list::TList::const_iterator it_docsum = docsums.begin();
        it_docsum!=docsums.end(); it_docsum++
    ) {
      string acc  = (*it_docsum)->GetValue("Caption"); // accession, no version
      SGiVerLenTaxid& gvt = mapAccData[acc];

      gvt.gi=(*it_docsum)->GetUid();

      string s = (*it_docsum)->GetValue("Extra");   // gi|...|accession.ver...
      SIZE_TYPE pos=s.find("|"+acc+".");
      if(pos!=NPOS) gvt.ver=atoi(s.c_str()+pos+2+acc.size());

      gvt.len   = s2i( (*it_docsum)->GetValue("Slen" ) );
      gvt.taxid = s2i( (*it_docsum)->GetValue("TaxId") );
    }
  }
  catch(CException e){
    cerr << "Note: temporary Entrez2 problems; falling back to Object Manager for this batch:\n  ";

    // first 3 ... last 3 (total count)
    int count_accessions = accessions.size();
    int i=0;
    for(set<string>::iterator it = accessions.begin();  it != accessions.end(); ++it) {
      ++i;
      if(i<=3 || i>=count_accessions-3 || (i==4 && count_accessions==7) ) cerr << *it << " ";
      else if(i==4 && count_accessions>7) cerr << "... ";
    }
    if(count_accessions>7) cerr << "(total " << count_accessions << ")";
    cerr << "\nPLEASE IGNORE any (Timeout) warnings above!\n";
  }
  accessions.clear();
}

void CAltValidator::ProcessQueue()
{
  int entrez_count=0;
  int objman_count=0;
  CAgpRow row;
  //cerr << "before QueryAccessions" << endl;
  QueryAccessions(); // In: accessions; Out: mapAccData.
  //cerr << "after QueryAccessions" << endl;

  for(TLineQueue::iterator it=lineQueue.begin();
      it != lineQueue.end(); ++it
  ) {
    string& acc=it->comp_id;
    if(acc.size()==0) {
      // a line to be printed verbatim and not to be processed
      if(m_out) {
        *m_out << it->orig_line << "\n";
      }
    }
    else {
      SIZE_TYPE pos_ver = acc.find('.');
      int ver=0;
      if(pos_ver!=NPOS) ver=s2i( acc.substr(pos_ver+1) );
      string acc_nover= acc.substr(0, pos_ver );

      SGiVerLenTaxid acc_data;
      TMapAccData::const_iterator itAccData = mapAccData.find( acc_nover );
      if( itAccData!=mapAccData.end() && itAccData->second.MatchesVersion_HasAllData(ver, m_check_len_taxid) ) {
        acc_data = itAccData->second;
        entrez_count++;
      }
      else {
        // a fallback
        GetAccDataFromObjMan(acc, acc_data);
        objman_count++;
      }

      if(m_out) {
        if(ver==0 && acc_data.ver==1) {
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
          if(ver<=0) {
            if(acc_data.ver) {
              *m_out << "#current version " << acc << "." << acc_data.ver;
            }
            else if(acc_data.gi==0){
              *m_out << "#component_id not in GenBank";
            }
          }
        }
        *m_out << "\n";
      }

      if(acc_data.gi) {
        // component_id is a valid GenBank accession
        m_GenBankCompLineCount++;

        // Warn if no version was supplied and GenBank version is > 1
        if(ver==0 && acc_data.ver>1) agpErr.Msg(CAgpErrEx::G_NeedVersion,
          acc + " (current version " + NStr::IntToString(acc_data.ver) + ")",
          CAgpErr::fAtThisLine);

        if(m_check_len_taxid) {
          // Component out of bounds check
          CAgpRow::CheckComponentEnd( acc, it->comp_end, acc_data.len, agpErr );

          // Taxid check
          int taxid = acc_data.taxid;
          if(taxid) {
            if(m_SpeciesLevelTaxonCheck) {
              taxid = x_GetSpecies(taxid);
            }
            x_AddToTaxidMap(taxid, acc, it->line_num);
          }
        }
      }
    }

    agpErr.LineDone(it->orig_line, it->line_num);
  }

  lineQueue.clear();
  //cerr << "entrez_count=" << entrez_count << " objman_count=" << objman_count << endl;
}

string ExtractAccession(const string& long_acc)
{
  SIZE_TYPE pos1=long_acc.find('|');
  if(pos1==NPOS) return long_acc;

  SIZE_TYPE pos2=long_acc.find('|', pos1+1);
  if(pos2==NPOS) return long_acc.substr(pos1+1);

  if( long_acc.substr(0,pos1) == "gi" ) {
    // trim "gi|xxx|" prefix off
    pos2++;
  }
  else {
    // Do not trim off the first and the second fields
    pos2=0;
  }

  CBioseq::TId ids;
  try{
    CSeq_id::ParseFastaIds(ids, long_acc.substr(pos2));
    string s = ids.front()->GetSeqIdString(true);
    // remove undesirable "XXXX:" from "XXXX:Scaffold1_1".
    //pos1 = s.find(':');
    pos1 = NStr::Find(s, ":", 0, NPOS, NStr::eLast);
    if( pos1 != NPOS ) return s.substr(pos1+1);
    return s;
  }
  catch(CException e){
    return long_acc;
  }
}



END_NCBI_SCOPE

