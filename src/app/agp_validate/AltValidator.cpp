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
 *      "Alt" does NOT stand for "alternative", but "Accession, Length and Taxid".
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
#ifdef HAVE_NCBI_VDB
#include <sra/data_loaders/wgs/wgsloader.hpp>
#endif

using namespace ncbi;
using namespace objects;

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
  
#ifdef HAVE_NCBI_VDB  
  const auto wgsInfo = CWGSDataLoader::RegisterInObjectManager(
          *m_ObjectManager, CObjectManager::eDefault);

  if (!wgsInfo.IsCreated()) {
    cerr << "FATAL: cannot connect to VDB!\n";
    exit(1);
   }
#endif

  // Create a new scope ("attached" to our OM).
  m_Scope.Reset(new CScope(*m_ObjectManager));
  // Add default loaders (GB loader in this demo) to the scope.
  m_Scope->AddDefaults();
}

// Returns GI // int* missing_ver


void CAltValidator::PrintTotals(CNcbiOstream& out, bool use_xml)
{
  int e_count=pAgpErr->CountTotals(CAgpErrEx::E_Last) + pAgpErr->CountTotals(CAgpErrEx::G_Last);

  if(use_xml) {
    cout << " <LinesWithValidCompAcc>" << m_GenBankCompLineCount << "</LinesWithValidCompAcc>\n";
    cout << " <errors>" << e_count << "</errors>\n";
    // >0: too many messages
    cout << " <skipped>" << pAgpErr->m_msg_skipped << "</skipped>\n";
    pAgpErr->PrintMessageCounts(out, CAgpErrEx::CODE_First, CAgpErrEx::CODE_Last, true);
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
      if(pAgpErr->m_msg_skipped) out << ", " << pAgpErr->m_msg_skipped << " not printed";
      out << ":\n";
      pAgpErr->PrintMessageCounts(out, CAgpErrEx::CODE_First, CAgpErrEx::CODE_Last, true);
    }
    else {
      out << "; no invalid component accessions.\n";
    }
  }
}


TTaxId CAltValidator::x_GetSpecies(TTaxId taxid)
{
  TTaxidSpeciesMap::iterator map_it;
  map_it = m_TaxidSpeciesMap.find(taxid);
  TTaxId species_id;

  if (map_it == m_TaxidSpeciesMap.end()) {
    species_id = x_GetTaxonSpecies(taxid);
    m_TaxidSpeciesMap.insert(
        TTaxidSpeciesMap::value_type(taxid, species_id) );
  } else {
    species_id = map_it->second;
  }

  return species_id;
}

TTaxId CAltValidator::x_GetTaxonSpecies(TTaxId taxid)
{
  CTaxon1 taxon;
  taxon.Init();

  bool is_species = true;
  bool is_uncultured;
  string blast_name;
  string blast_name0;

  TTaxId species_id = ZERO_TAX_ID;
  TTaxId prev_id = taxid;


  for(TTaxId id = taxid;
      is_species == true && id > TAX_ID_CONST(1);
      id = taxon.GetParent(id)
  ) {
    CConstRef<COrg_ref> org_ref = taxon.GetOrgRef(
      id, is_species, is_uncultured, blast_name
    );
    if(org_ref == null) {
      pAgpErr->Msg(CAgpErrEx::G_TaxError,
        string(" for taxid ")+ NStr::NumericToString(id) );
      return ZERO_TAX_ID;
    }
    if(id==taxid) blast_name0=blast_name;

    if (is_species) {
        prev_id = id;
    } else {
        species_id = prev_id;
    }
  }

  if(species_id==ZERO_TAX_ID) {
    if(blast_name0.size()) {
      blast_name0 = NStr::NumericToString(taxid) + " (" + blast_name0 + ")";
    }
    else{
      blast_name0 = string("taxid ") + NStr::NumericToString(taxid);
    }
    pAgpErr->Msg(CAgpErrEx::G_DataError,
      string(" - ") + blast_name0 +
      " is above species level");
  }

  return species_id;
}


void CAltValidator::x_AddToTaxidMap(
  TTaxId taxid, const string& comp_id, int line_num)
{
  SAgpLineInfo line_info;

  line_info.file_num = pAgpErr->GetFileNum();
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

  TTaxId agp_taxid = ZERO_TAX_ID;
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
      if(agp_taxid != ZERO_TAX_ID) {
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

    TTaxId taxid = map_it->first;
    ITERATE(TAgpInfoList, list_it, map_it->second) {
      if(use_xml) {
         out << " <CompBadTaxid taxid=\"" << taxid
             << "\" line_num=\"" << list_it->line_num
             << (list_it->file_num ? string(
                  "\" filename=\"" + NStr::XmlEncode(pAgpErr->GetFile(list_it->file_num))
                ) : NcbiEmptyString)
             << "\">" <<  NStr::XmlEncode(list_it->component_id)<< "</CompBadTaxid>\n";
      }
      else {
        cerr << "\t";
        if(list_it->file_num) {
          cerr << pAgpErr->GetFile(list_it->file_num)  << ":";
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
  m_LineQueue.push_back(ld);
  m_Accessions.insert(comp_id);
}

void CAltValidator::QueueLine(const string& orig_line)
{
  SLineData ld;
  ld.orig_line = orig_line;
  // ld.comp_id remains empty
  m_LineQueue.push_back(ld);
}


void CAltValidator::x_QueryAccessions()
{
    m_InvalidAccessions.clear();
    vector<CSeq_id_Handle> idHandles;
    CSeq_id seqId;
    for (const auto& accession : m_Accessions) {
        try {
            seqId.Set(accession);
        }
        catch (...)
        {
            m_InvalidAccessions.insert(accession);
            continue;
        }
        idHandles.push_back(CSeq_id_Handle::GetHandle(seqId));
    }

    auto bioseqHandles = m_Scope->GetBioseqHandles(idHandles);

    size_t index = 0; 
    for (auto it=begin(bioseqHandles);
              it!=end(bioseqHandles); 
              ++it, ++index)   {
        if (!*it) {
          continue; 
        }
        const auto& bioseqHandle = *it;
        auto idHandle = sequence::GetId(bioseqHandle, sequence::eGetId_ForceAcc);
        const auto& accVer = idHandle.GetSeqId()->IsGenbank() ?
                             idHandle.GetSeqId()->GetGenbank() :
                             idHandle.GetSeqId()->GetOther();

        // Add information to map  
        auto& compInfo = m_ComponentInfoMap[accVer.GetAccession()];
        compInfo.currentVersion = accVer.GetVersion();
        compInfo.len = bioseqHandle.GetInst_Length();
        compInfo.taxid = sequence::GetTaxId(bioseqHandle);
        compInfo.inDatabase=true;
    }
    m_Accessions.clear();
}

static void s_WriteLine(
        const string& accession,
        const CTempString& line,
        bool inDatabase,
        int currentVersion,
        bool versionSpecifiedInFile,
        CNcbiOstream& ostr)
{

    if (versionSpecifiedInFile) {
        ostr << line << '\n';
        return;
    }

    if (currentVersion==1) {  
        size_t pos=NPOS;
        size_t tabCount=0;
        for (size_t i=0; i<line.size(); ++i) {
            if (line[i] == '\t') {
                ++tabCount;
            }
            if (tabCount==6) {
                pos=i;
                break;
            }
        }
        ostr << line.substr(0, pos);
        if (pos != NPOS) ostr << ".1" << line.substr(pos);
        ostr << '\n';
        return;
    }

    if (currentVersion) {
        ostr << line << "#current version " << accession << "." << currentVersion << '\n';
    }
    else 
    if (!inDatabase) 
    {   
        ostr << line << "#component_id not in GenBank" << '\n';
    }
}
    

void CAltValidator::ProcessQueue()
{
    x_QueryAccessions(); // In: accessions; Out: m_ComponentInfoMap.

    for (const auto& lineInfo : m_LineQueue) 
    {
        const CTempString& acc=lineInfo.comp_id;
        const string& orig_line = lineInfo.orig_line;
        if(acc.empty()) {
            if (m_pOut) {
                *m_pOut << orig_line << '\n';
            }
            pAgpErr->LineDone(orig_line, lineInfo.line_num);
            continue;
        }


        SIZE_TYPE pos_ver = acc.find('.');
        const bool versionSpecified = 
            (pos_ver != NPOS) ?
            (NStr::StringToNonNegativeInt(acc.substr(pos_ver+1)) != -1) :
            false;

        const CTempString& acc_nover = versionSpecified ?
            acc.substr(0, pos_ver) :
            acc;

        SComponentInfo acc_data;
        auto itAccData = m_ComponentInfoMap.find(acc_nover);

        if (itAccData!=m_ComponentInfoMap.end()) {
            acc_data = itAccData->second;
            if (!acc_data.currentVersion) {
                pAgpErr->Msg(CAgpErrEx::G_DataError, 
                    " - cannot get version for "+acc);
            }
            else 
            if (m_check_len_taxid && acc_data.taxid == ZERO_TAX_ID) {
                pAgpErr->Msg(CAgpErrEx::G_DataError,
                    " - cannot retrieve taxonomic id for "+acc);
            }

            m_GenBankCompLineCount++;
            // Warn if no version was supplied and GenBank version is > 1
            if(!versionSpecified && acc_data.currentVersion>1) pAgpErr->Msg(CAgpErrEx::G_NeedVersion,
                acc + " (current version " + NStr::IntToString(acc_data.currentVersion) + ")",
                CAgpErr::fAtThisLine);

            if(m_check_len_taxid) {
                // Component out of bounds check
                CAgpRow::CheckComponentEnd( acc, lineInfo.comp_end, acc_data.len, *pAgpErr );

                // Taxid check
                TTaxId taxid = acc_data.taxid;
                if(taxid != ZERO_TAX_ID) {
                    if(m_SpeciesLevelTaxonCheck) {
                        taxid = x_GetSpecies(taxid);
                    }
                    x_AddToTaxidMap(taxid, acc, lineInfo.line_num);
                }
            }
        }
        else {
            auto it = m_InvalidAccessions.find(acc);
            if (it != end(m_InvalidAccessions)) {
                pAgpErr->Msg(CAgpErrEx::G_InvalidCompId, ": "+acc);
            }
            else {
                pAgpErr->Msg(CAgpErrEx::G_NotInGenbank, ": "+acc);
            }
        }

        if (m_pOut) {
            s_WriteLine(acc, 
                    orig_line, 
                    acc_data.inDatabase, 
                    acc_data.currentVersion,
                    versionSpecified,
                    *m_pOut);
        }
        pAgpErr->LineDone(orig_line, lineInfo.line_num);
    }
    m_LineQueue.clear();
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
    pos1 = s.rfind(':');
    if( pos1 != NPOS ) return s.substr(pos1+1);
    return s;
  }
  catch(CException e){
    return long_acc;
  }
}

void OverrideLenIfAccession(const string & acc, int & in_out_len)
{
    // if the acc is in Genbank, we override it as necessary

    try {
        CRef<CSeq_id> temp_seq_id( new CSeq_id(acc) );
        if( temp_seq_id->IsLocal() ) {
            return;
        }

        CSeq_id::EAccessionInfo fAccnInfo = temp_seq_id->IdentifyAccession();
        const bool bAccnIsProtOnly = (
            (fAccnInfo & CSeq_id::fAcc_prot) &&
            ! (fAccnInfo & CSeq_id::fAcc_nuc));
        if ( bAccnIsProtOnly ) {
            return;
        }

        if( ! m_Scope ) {
            return;
        }

        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*temp_seq_id);
        if( bsh ) {
            in_out_len = bsh.GetInst_Length();
            cerr << "WARNING: '" << acc << "' was found in component files but Genbank version overrides it." << endl;
        }

    } catch(CSeqIdException & ) {
        // it's not an accession, so just continue
    }
}

END_NCBI_SCOPE

