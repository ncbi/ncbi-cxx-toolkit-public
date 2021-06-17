#ifndef AGP_VALIDATE_AltValidator
#define AGP_VALIDATE_AltValidator

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
 *      Checks that use component Accession, Length and Taxid info from the GenBank.
 *
 */

#include <corelib/ncbistd.hpp>
#include <objtools/readers/agp_util.hpp>
#include <iostream>
#include <set>

BEGIN_NCBI_SCOPE
extern CRef<CAgpErrEx> pAgpErr;

class CAltValidator
{
public:
  CAltValidator(bool check_len_taxid)
      : m_check_len_taxid(check_len_taxid) {}

  void SetOstream(CNcbiOstream* pOstr) {
        m_pOut = pOstr;
  }

  bool IsSetOstream(void) const 
  {
      return (m_pOut != nullptr);
  }

  void SetSpeciesLevelTaxonCheck(bool check=true) {
    m_SpeciesLevelTaxonCheck = check;
  }

  void Init();
  // true - no problems, false - found bad taxids
  bool CheckTaxids(CNcbiOstream& out, bool use_xml);
  void PrintTotals(CNcbiOstream& out, bool use_xml);

  /// missing_ver: assign 0 if not missing, else the latest version
  static void ValidateLength(const string& comp_id, int comp_end, int comp_len);

  void QueueLine(const string& orig_line, 
          const string& comp_id,
          int line_num, 
          int comp_end);

  // for the lines that are not processed, such as: comment, gap, invalid line
  void QueueLine(const string& orig_line);

  size_t QueueSize() const
  {
    return m_LineQueue.size();
  }

  void ProcessQueue();

private:
  void x_QueryAccessions();
  void x_AddToTaxidMap(TTaxId taxid, const string& comp_id, int line_num);

  // searches m_TaxidSpeciesMap first, calls x_GetTaxonSpecies() if necessary
  TTaxId x_GetSpecies(TTaxId taxid);
  TTaxId x_GetTaxonSpecies(TTaxId taxid);

  struct SLineData
  {
    string orig_line;
    string comp_id;
    int line_num;
    int comp_end;
  };

  struct SComponentInfo
  {
    int currentVersion=0; 
    int len=0;
    TTaxId taxid = ZERO_TAX_ID;
    bool inDatabase=false;
  };

  using TMapAccData = map<string, SComponentInfo>; // key = accession with no version
  TMapAccData m_ComponentInfoMap;

  // taxids used in the AGP files: taxid -> vector of AGP lines
  struct SAgpLineInfo {
      int    file_num;
      int    line_num;
      string component_id;
  };

  using TAgpInfoList = vector<SAgpLineInfo>;
  using TTaxidMap = map<TTaxId, TAgpInfoList>;
  using TTaxidMapRes = pair<TTaxidMap::iterator, bool>;
  TTaxidMap m_TaxidMap;

  int m_TaxidComponentTotal;
  using TTaxidSpeciesMap = map<TTaxId, TTaxId>;
  TTaxidSpeciesMap m_TaxidSpeciesMap;

  bool m_SpeciesLevelTaxonCheck;
  set<string> m_Accessions; // with or without versions (as given in the AGP file)
  CNcbiOstream* m_pOut=nullptr;
  bool m_check_len_taxid=false;
  using TLineQueue = vector<SLineData>;
  TLineQueue m_LineQueue;
  int m_GenBankCompLineCount=0;
  set<string> m_InvalidAccessions;
};

// These really should be in agp_validate.cpp, but gcc inexplicably balks, saying:
//   `CSeq_id' undeclared! `CBioseq' has not been declared!
string ExtractAccession(const string& long_acc);
void OverrideLenIfAccession(const string & acc, int & in_out_len);

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_AltValidator */


