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
 *      AGP context-sensitive validation (uses information from several lines).
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/readers/agp_validate_reader.hpp>
#include <algorithm>
#include <objects/seqloc/Seq_id.hpp>

using namespace ncbi;
using namespace objects;
BEGIN_NCBI_SCOPE

//// class CAgpValidateReader
void CAgpValidateReader::Reset(bool for_chr_from_scaf)
{
  m_is_chr=for_chr_from_scaf;

  m_CheckObjLen=false; // if for_chr_from_scaf: scaffolds lengths are _component lengths_ in chr_from_scaf file, not objects lengths
                       // else: checking _component lengths_ is the default
  m_CommentLineCount=m_EolComments=0;
  m_componentsInLastScaffold=m_componentsInLastObject=0;
  m_gapsInLastScaffold=m_gapsInLastObject=0;
  m_prev_orientation=0; // m_prev_orientation_unknown=false;
  m_prev_component_beg = m_prev_component_end = 0;

  m_ObjCount = 0;
  m_ScaffoldCount = 0;
  m_SingleCompScaffolds = 0;
  m_SingleCompObjects = 0;
  m_SingleCompScaffolds_withGaps=0;
  m_SingleCompObjects_withGaps=0;
  m_NoCompScaffolds=0;

  m_CompCount = 0;
  m_GapCount = 0;

  m_expected_obj_len=0;
  m_comp_name_matches=0;
  m_obj_name_matches=0;

  memset(m_CompOri, 0, sizeof(m_CompOri));
  memset(m_GapTypeCnt, 0, sizeof(m_GapTypeCnt));
  m_ln_ev_flags2count.clear();
  m_Ngap_ln2count.clear();
  m_Ugap_ln2count.clear();

  m_max_comp_beg=m_max_obj_beg=0;
  m_has_partial_comp=m_has_comp_of_unknown_len=false;

  if(for_chr_from_scaf) {
    NCBI_ASSERT(m_explicit_scaf, "m_explicit_scaf is false in CAgpValidateReader::Reset(true)");

    // for W_ObjOrderNotNumerical
    m_obj_id_pattern.clear();

    m_obj_id_digits->clear();
    m_prev_id_digits->clear();

    m_prev_component_id.clear();

    m_TypeCompCnt.clear();
    m_ObjIdSet.clear();
    m_objNamePatterns.clear();
    m_CompId2Spans.clear();

    m_comp2len = &m_scaf2len;
  }
  else {
    // for W_ObjOrderNotNumerical
    m_obj_id_digits  = new CAccPatternCounter::TDoubleVec();
    m_prev_id_digits = new CAccPatternCounter::TDoubleVec();
  }
}

CAgpValidateReader::CAgpValidateReader(CAgpErrEx& agpErr, CMapCompLen& comp2len, TMapStrRangeColl& comp2range_coll) //, bool checkCompNames
  : CAgpReader(&agpErr, eAgpVersion_auto), m_AgpErr(&agpErr), m_comp2len(&comp2len), m_comp2range_coll(&comp2range_coll)
{
  m_CheckCompNames=false; // checkCompNames;
  m_unplaced=false;
  m_explicit_scaf=false;
  m_row_output=NULL;

  Reset(false);
}

CAgpValidateReader::~CAgpValidateReader()
{
  delete m_obj_id_digits;
  delete m_prev_id_digits;
}

bool CAgpValidateReader::OnError()
{
  if(m_line_skipped) {
    // Avoid printing the wrong AGP line along with "orientation_unknown" error
    m_prev_orientation=0; // m_prev_orientation_unknown=false;
    m_prev_component_beg = m_prev_component_end = 0;

    // For lines with non-syntax errors that are not skipped,
    // these are called from OnGapOrComponent()
    if(m_this_row->pcomment!=NPOS) m_EolComments++; // ??
    m_AgpErr->LineDone(m_line, m_line_num, true);
  }

  return true; // continue checking for errors
}

void CAgpValidateReader::OnComment()
{
  // Line that starts with "#".
  // No other callbacks invoked for this line.
  m_CommentLineCount++;

  if(m_row_output) m_row_output->SaveRow(m_line, CRef<CAgpRow>(), NULL);
}

void CAgpValidateReader::OnGapOrComponent()
{
  if(m_this_row->pcomment!=NPOS) m_EolComments++;
  m_TypeCompCnt.add( m_this_row->GetComponentType() );

  if(m_max_obj_beg < m_this_row->object_beg) m_max_obj_beg = m_this_row->object_beg;
  if( m_this_row->IsGap() ) {
    m_GapCount++;
    m_gapsInLastObject++;

    // count number of gaps for each gap length
    TMapIntInt* p_gap_len2count;
    if(m_this_row->component_type=='N') {
      p_gap_len2count = &m_Ngap_ln2count;
    }
    else {
      p_gap_len2count = &m_Ugap_ln2count;
    }
    TMapIntIntResult res = p_gap_len2count->insert(TPairIntInt( m_this_row->gap_length, 1));
    if(!res.second) res.first->second++;

    int i = m_this_row->gap_type;
    if(m_this_row->linkage) i+= CAgpRow::eGapCount;
    NCBI_ASSERT( i < (int)(sizeof(m_GapTypeCnt)/sizeof(m_GapTypeCnt[0])),
      "m_GapTypeCnt[] index out of bounds" );
    m_GapTypeCnt[i]++;

    if(m_this_row->gap_length < 10) {
      m_AgpErr->Msg(CAgpErrEx::W_ShortGap);
    }

    m_prev_component_id.clear();
    if( !m_this_row->GapEndsScaffold() ) {
      m_gapsInLastScaffold++;
      if(
        m_prev_orientation && m_prev_orientation != '+' && m_prev_orientation != '-' // m_prev_orientation_unknown
        && m_componentsInLastScaffold==1 // can probably ASSERT this
      ) {
        m_AgpErr->Msg(CAgpErrEx::E_UnknownOrientation, NcbiEmptyString, CAgpErr::fAtPrevLine);
        m_prev_orientation=0; // m_prev_orientation_unknown=false;
      }
      if(m_explicit_scaf && m_is_chr) {
        m_AgpErr->Msg(CAgpErrEx::E_WithinScafGap);
      }

      m_ln_ev_flags2count[m_this_row->linkage_evidence_flags]++;
    }
    else {
      if(!m_at_beg && !m_prev_row->IsGap()) {
        // check for W_BreakingGapSameCompId on the next row
        m_prev_component_id=m_prev_row->GetComponentId();
      }
      if(m_explicit_scaf && !m_is_chr) {
        m_AgpErr->Msg(CAgpErrEx::E_ScafBreakingGap);
      }

      m_last_scaf_start_file=m_AgpErr->GetFileNum();
      m_last_scaf_start_line=m_line_num;
      m_last_scaf_start_is_obj=false;
    }
    if(m_row_output) m_row_output->SaveRow(m_line, m_this_row, NULL);
  }
  else { // component line
    m_CompCount++;
    m_componentsInLastScaffold++;
    m_componentsInLastObject++;
    switch(m_this_row->orientation) {
      case '+': m_CompOri[CCompVal::ORI_plus ]++; break;
      case '-': m_CompOri[CCompVal::ORI_minus]++; break;
      case '0': m_CompOri[CCompVal::ORI_zero ]++; break;
      case 'n': m_CompOri[CCompVal::ORI_na   ]++; break;
    }
    if(m_max_comp_beg < m_this_row->component_beg) m_max_comp_beg = m_this_row->component_beg;

    //// Orientation "0" or "na" only for singletons
    // A saved potential error
    if(
      m_prev_orientation && m_prev_orientation != '+' && m_prev_orientation != '-' // m_prev_orientation_unknown
    ) {
      // Make sure that prev_orientation_unknown
      // is not a leftover from the preceding singleton.
      if( m_componentsInLastScaffold==2 ) {
        m_AgpErr->Msg(CAgpErrEx::E_UnknownOrientation,
          NcbiEmptyString, CAgpErr::fAtPrevLine);
      }
      m_prev_orientation=0; // m_prev_orientation_unknown=false;
    }

    if(m_componentsInLastScaffold==1) {
      // Report an error later if the current scaffold
      // turns out not a singleton. Only singletons can have
      // components with unknown orientation.
      //
      // Note: previous component != previous line if there was a non-breaking gap;
      // we check prev_orientation after such gaps, too.
      m_prev_orientation   = m_this_row->orientation; // if (...) m_prev_orientation_unknown=true;
      m_prev_component_beg = m_this_row->component_beg;
      m_prev_component_end = m_this_row->component_end;
    }
    else if( m_this_row->orientation != '+' && m_this_row->orientation != '-' ) {
      // This error is real, not "potential"; report it now.
      m_AgpErr->Msg(CAgpErrEx::E_UnknownOrientation, NcbiEmptyString);
      m_prev_orientation=0;
    }

    //// Check that component spans do not overlap and are in correct order

    // Try to insert to the span as a new entry
    CCompVal comp;
    comp.init(*m_this_row, m_line_num);
    TCompIdSpansPair value_pair( m_this_row->GetComponentId(), CCompSpans(comp) );
    pair<TCompId2Spans::iterator, bool> id_insert_result =
        m_CompId2Spans.insert(value_pair);

    string sameComId_otherScaf;

    if(id_insert_result.second == false) {
      // Not inserted - the key already exists.
      CCompSpans& spans = (id_insert_result.first)->second;

      // Chose the most specific warning among:
      //   W_SpansOverlap W_SpansOrder W_DuplicateComp
      // The last 2 are omitted for draft seqs.
      CCompSpans::TCheckSpan check_sp = spans.CheckSpan(
        // can replace with m_this_row->component_beg, etc
        comp.beg, comp.end, m_this_row->orientation!='-'
      );
      if( check_sp.second == CAgpErrEx::W_SpansOverlap  ) {
        m_AgpErr->Msg(CAgpErrEx::W_SpansOverlap,
          string(": ")+ check_sp.first->ToString(m_AgpErr)
        );
      }
      else if( ! m_this_row->IsDraftComponent() ) {
        m_AgpErr->Msg(check_sp.second, // W_SpansOrder or W_DuplicateComp
          string("; preceding span: ")+ check_sp.first->ToString(m_AgpErr)
        );
      }

      // W_BreakingGapSameCompId
      //
      // to do: compare
      //   *(spans.rbegin())->file_num
      //   ((CAgpErrEx*)(row.GetErrorHandler()))->GetFileNum();
      int prev_comp_file=spans.rbegin()->file_num;
      int prev_comp_line=spans.rbegin()->line_num;
      if(prev_comp_file < m_last_scaf_start_file || prev_comp_line < m_last_scaf_start_line) {
        sameComId_otherScaf="; previous occurance at ";
        if(prev_comp_file && prev_comp_file!=m_AgpErr->GetFileNum()) {
          sameComId_otherScaf += m_AgpErr->GetFile(prev_comp_file);
          sameComId_otherScaf += ":";
        }
        else {
          sameComId_otherScaf+="line ";
        }
        sameComId_otherScaf+=NStr::IntToString(prev_comp_line);

        if(m_last_scaf_start_is_obj) {
          sameComId_otherScaf+=", in another object";
        }
        else {
          sameComId_otherScaf+=", before a scaffold-breaking gap at ";

          if(m_last_scaf_start_file && m_last_scaf_start_file!=m_AgpErr->GetFileNum()) {
            // this branch is probably unneeded: it covers the case of one object spanning 2 files,
            // which is probably caught elsewhere...
            sameComId_otherScaf += m_AgpErr->GetFile(m_last_scaf_start_file);
            sameComId_otherScaf += ":";
          }
          else {
            sameComId_otherScaf+="line ";
          }
          sameComId_otherScaf+=NStr::IntToString(m_last_scaf_start_line);
        }
      }

      // Add the span to the existing entry
      spans.AddSpan(comp);
    }

    //// check the component name [and its end vs its length]
    if(m_this_row->GetComponentId()==m_this_row->GetObject()) m_AgpErr->Msg(CAgpErrEx::W_ObjEqCompId);

    CSeq_id::EAccessionInfo acc_inf = CSeq_id::IdentifyAccession( m_this_row->GetComponentId() );
    if( acc_inf==CSeq_id::eAcc_other && m_this_row->GetComponentId().find("|")!=NPOS )
      m_AgpErr->Msg(CAgpErrEx::E_InvalidBarInId, " in component_id (column 6)");
    int div = acc_inf & CSeq_id::eAcc_division_mask;
    if(m_CheckCompNames) {
      string msg;
      if(       acc_inf & CSeq_id::fAcc_prot ) msg="; looks like a protein accession";
      else if(!(acc_inf & CSeq_id::fAcc_nuc )) msg="; local or misspelled accession";

      if(msg.size()) m_AgpErr->Msg(CAgpErrEx::G_InvalidCompId, msg);
    }

    if(acc_inf & CSeq_id::fAcc_nuc) {
      if( div == CSeq_id::eAcc_wgs ||
          div == CSeq_id::eAcc_wgs_intermed
      ) {
        if(m_this_row->component_type != 'W') m_AgpErr->Msg(CAgpErr::W_CompIsWgsTypeIsNot);
      }
      else if( div == CSeq_id::eAcc_htgs ) {
        if(m_this_row->component_type == 'W') m_AgpErr->Msg(CAgpErr::W_CompIsNotWgsTypeIs);
      }
      else {
        if( string("ADF").find(m_this_row->component_type)!=NPOS ) m_AgpErr->Msg(CAgpErr::W_CompIsNotHtgTypeIs, m_this_row->GetComponentId());
      }
    }
    else {
      if( string("ADF").find(m_this_row->component_type)!=NPOS ) m_AgpErr->Msg(CAgpErr::W_CompIsNotHtgTypeIs, m_this_row->GetComponentId());
    }

    if( m_comp2len->size() ) {
      if( !m_CheckObjLen ) {
        TMapStrInt::iterator it = m_comp2len->find( m_this_row->GetComponentId() );
        if( it==m_comp2len->end() ) {
          m_has_comp_of_unknown_len=true;
          if(m_is_chr && m_explicit_scaf) {
            m_AgpErr->Msg(CAgpErrEx::E_UnknownScaf, m_this_row->GetComponentId());
          }
          else {
            m_AgpErr->Msg(CAgpErrEx::G_InvalidCompId, string(": ")+m_this_row->GetComponentId());
          }
        }
        else {
          m_comp_name_matches++;
          m_this_row->CheckComponentEnd(it->second);
          if(it->second != m_this_row->component_end) m_has_partial_comp=true;
          if(m_explicit_scaf && m_is_chr) {
            if(it->second > m_this_row->component_end || m_this_row->component_beg>1) {
              m_AgpErr->Msg(CAgpErrEx::W_ScafNotInFull,
                " (" + NStr::IntToString(m_this_row->component_end-m_this_row->component_beg+1) +
                " out of " + NStr::IntToString(it->second)+ " bp)"
              );
            }
          }
        }
      }
      else {
        m_has_comp_of_unknown_len=true;
      }
    }
    else {
      m_has_comp_of_unknown_len=true;
      if(m_explicit_scaf && m_is_chr) {
        if(m_this_row->component_beg>1) {
          m_AgpErr->Msg(CAgpErrEx::W_ScafNotInFull);
        }
      }
    }

    //// check that this span does not include gaps
    bool row_saved = false;
    if(m_comp2range_coll->size() && !m_CheckObjLen && !(m_is_chr && m_explicit_scaf) ) {
      TMapStrRangeColl::iterator it = m_comp2range_coll->find( m_this_row->GetComponentId() );
      if( it!=m_comp2range_coll->end() ) {
        TRangeColl& intersection = it->second.IntersectWith(
          TSeqRange(m_this_row->component_beg, m_this_row->component_end)
        );
        if(!intersection.empty()) {
          if(m_row_output) m_row_output->SaveRow(m_line, m_this_row, &intersection);
          row_saved=true;

          string masked_spans;
          TRangeColl::const_iterator it = intersection.begin();
          for(; it != intersection.end() && masked_spans.size() < 80; ++it) {
              if(masked_spans.size()) masked_spans += ", ";
              masked_spans +=  NStr::IntToString(it->GetFrom()) + ".." + NStr::IntToString(it->GetTo());
          }
          if(it != intersection.end()) masked_spans += ", ...";

          m_AgpErr->Msg(CAgpErrEx::G_NsWithinCompSpan, ": "+masked_spans);
        }
      }
    }

    //// W_BreakingGapSameCompId
    if( m_prev_component_id==m_this_row->GetComponentId() ) {
      m_AgpErr->Msg(CAgpErrEx::W_BreakingGapSameCompId, " (a scaffold-breaking gap in between)", CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine|CAgpErr::fAtPpLine);
    }
    else if(sameComId_otherScaf.size()) {
      // cannot show the other 2 lines involved in this error - they were encuntered too long ago
      m_AgpErr->Msg(CAgpErrEx::W_BreakingGapSameCompId, sameComId_otherScaf);
    }

    if(m_row_output && !row_saved) m_row_output->SaveRow(m_line, m_this_row, NULL);
  }

  CAgpErrEx* errEx = static_cast<CAgpErrEx*>(GetErrorHandler());
  errEx->LineDone(m_line, m_line_num);

  // m_this_row = current gap or component (check with m_this_row->IsGap())
}

void CAgpValidateReader::OnScaffoldEnd()
{
  NCBI_ASSERT(m_componentsInLastScaffold>0 || m_gapsInLastScaffold>0,
    "CAgpValidateReader::OnScaffoldEnd() invoked for a scaffold with no components or gaps");

  m_ScaffoldCount++;
  if(m_componentsInLastScaffold==1) {
    m_SingleCompScaffolds++;
    if(m_gapsInLastScaffold) m_SingleCompScaffolds_withGaps++;

    if(m_unplaced && m_prev_orientation) {
      if(m_prev_orientation!='+') m_AgpErr->Msg( CAgpErrEx::W_UnSingleOriNotPlus   , CAgpErr::fAtPrevLine );

      TMapStrInt::iterator it = m_comp2len->find( m_this_row->GetComponentId() );
      if( it!=m_comp2len->end() ) {
        int len = it->second;
        if(m_prev_component_beg!=1 || m_prev_component_end<len ) {
          m_AgpErr->Msg( CAgpErrEx::W_UnSingleCompNotInFull,
            " (" + NStr::IntToString(m_prev_component_end-m_prev_component_beg+1) + " out of " + NStr::IntToString(len)+ " bp)",
            CAgpErr::fAtPrevLine );
        }
      }
      else if(m_prev_component_beg!=1) {
        // a shorter error message since we do not know the component length
        m_AgpErr->Msg( CAgpErrEx::W_UnSingleCompNotInFull, CAgpErr::fAtPrevLine );
      }
    }

  }
  else if(m_componentsInLastScaffold==0) {
    m_NoCompScaffolds++;
  }
  m_componentsInLastScaffold=0;
  m_gapsInLastScaffold=0;
}

void CAgpValidateReader::OnObjectChange()
{
  if(!m_at_beg) {
    // m_prev_row = the last  line of the old object
    m_ObjCount++;
    if(m_componentsInLastObject==0) m_AgpErr->Msg(
      CAgpErrEx::W_ObjNoComp, string(" ") + m_prev_row->GetObject(),
      CAgpErr::fAtPrevLine
    );
    if(m_componentsInLastObject  ==1) {
      m_SingleCompObjects++;
      if(m_gapsInLastObject) m_SingleCompObjects_withGaps++;
    }
    if(m_expected_obj_len) {
      if(m_expected_obj_len!=m_prev_row->object_end) {
        string details=": ";
        details += NStr::IntToString(m_prev_row->object_end);
        details += " != ";
        details += NStr::IntToString(m_expected_obj_len);

        m_AgpErr->Msg(CAgpErr::G_BadObjLen, details, CAgpErr::fAtPrevLine);
      }
    }
    else if(m_comp2len->size() && m_CheckObjLen) {
      // if(m_obj_name_matches>0 || m_comp_name_matches==0)
      m_AgpErr->Msg(CAgpErrEx::G_InvalidObjId, m_prev_row->GetObject(), CAgpErr::fAtPrevLine);
    }
    if(m_explicit_scaf && !m_is_chr) {
      // for: -scaf Scaf_AGP_file(s) -chr Chr_AGP_file(s)
      // when reading Scaf_AGP_file(s)
      m_scaf2len.AddCompLen(m_prev_row->GetObject(), m_prev_row->object_end);
    }

    // if(m_prev_row->IsGap() && m_componentsInLastScaffold==0) m_ScaffoldCount--; (???)
    m_componentsInLastObject=0;
    m_gapsInLastObject=0;
  }

  if(!m_at_end) {
    if( CSeq_id::IdentifyAccession( m_this_row->GetObject() )==CSeq_id::eAcc_other && m_this_row->GetObject().find("|")!=NPOS ) {
      m_AgpErr->Msg(CAgpErrEx::E_InvalidBarInId, " in object_id (column 1)");
    }
    // m_this_row = the first line of the new object
    TObjSetResult obj_insert_result = m_ObjIdSet.insert(m_this_row->GetObject());
    if (obj_insert_result.second == false) {
      m_AgpErr->Msg(CAgpErrEx::E_DuplicateObj, m_this_row->GetObject(),
        CAgpErr::fAtThisLine);
    }
    else {
      // GCOL-1236: allow spaces in object names, emit a WARNING instead of an ERROR
      SIZE_TYPE p_space = m_this_row->GetObject().find(' ');
      if(NPOS != p_space) {
        m_AgpErr->Msg(CAgpErrEx::W_SpaceInObjName, m_this_row->GetObject());
      }

      // m_objNamePatterns report + W_ObjOrderNotNumerical (JIRA: GP-773)

      // swap pointers: m_prev_id_digits <-> m_obj_id_digits
      CAccPatternCounter::TDoubleVec* t=m_prev_id_digits;
      m_prev_id_digits=m_obj_id_digits;
      m_obj_id_digits=t;

      CAccPatternCounter::iterator it=m_objNamePatterns.AddName( m_this_row->GetObject(), m_obj_id_digits );
      if(m_at_beg || m_obj_id_pattern!=it->first) {
        m_obj_id_pattern=it->first;
        m_obj_id_sorted=0;
      }
      else if(m_obj_id_sorted>=0) {
        if(m_prev_row->GetObject() > m_this_row->GetObject()) {
          // not literally sorted: turn off W_ObjOrderNotNumerical for the current m_obj_id_pattern
          m_obj_id_sorted = -1;
        }
        else {
          if(m_obj_id_sorted>0) {
            if( m_prev_row->GetObject().size() > m_this_row->GetObject().size() &&
                m_prev_id_digits->size() == m_obj_id_digits->size()
            ) {
              for( SIZE_TYPE i=0; i<m_prev_id_digits->size(); i++ ) {
                if((*m_prev_id_digits)[i]<(*m_obj_id_digits)[i]) break;
                if((*m_prev_id_digits)[i]>(*m_obj_id_digits)[i]) {
                  // literally sorted, but not numerically
                  m_AgpErr->Msg(CAgpErr::W_ObjOrderNotNumerical,
                    " ("+m_prev_row->GetObject()+" before "+m_this_row->GetObject()+")",
                    CAgpErr::fAtThisLine);
                  break;
                }
              }
            }
          }
          m_obj_id_sorted++;
        }
      }
    }

    if( m_comp2len->size() && m_CheckObjLen ) {
      // save expected object length (and the fact that we do expect it) for the future checks
      TMapStrInt::iterator it_obj = m_comp2len->find( m_this_row->GetObject() );
      if( it_obj!=m_comp2len->end() ) {
        m_expected_obj_len=it_obj->second;
        m_obj_name_matches++;
      }
      else {
        m_expected_obj_len=0;
      }
    }
  }
  else {
    if(m_max_comp_beg==1 && m_max_obj_beg==1) m_AgpErr->Msg(CAgpErr::W_SingletonsOnly,
      // to do -- add a message if we can be certain that
      //   ?" and component_end=length of component sequence":
      //    " (some component_end shorter than the whole component)"
      m_has_comp_of_unknown_len?"":
      m_has_partial_comp?" (not all component_end = length of sequence)":
      " and component_end = length of sequence",
      CAgpErr::fAtNone);
  }

  m_last_scaf_start_file=m_AgpErr->GetFileNum();
  m_last_scaf_start_line=m_line_num;
  m_last_scaf_start_is_obj=true;
}

#define ALIGN_W(x) setw(w) << resetiosflags(IOS_BASE::left) << (x)
#define ALIGN_M_W(x) setw(m_w) << resetiosflags(IOS_BASE::left) << (x)

/** Output the count as text or as xml.
 *  The text label could be transformed into XML tag as follows:
 *  - uppercase first letter of each word;
 *  - strip all spaces and non-alphabetic chars;
 *  - if the first character of the label was non-alphanum:
 *      prefix the tag with last_tag,
 *    else: save the tag to last_tag (for possible future use).
 */
class XPrintTotalsItem
{
public:
  CNcbiOstream& m_out;
  bool m_use_xml;
  int m_w;

  bool m_strip_attrs;
  string last_tag;
  string m_eol_text; // usually "\n", but could be gap or comp types, e.g. "(w)", "(W:35, D:8)"

  XPrintTotalsItem(CNcbiOstream& out, bool use_xml, int w) : m_out(out), m_use_xml(use_xml), m_w(w)
  {
    m_eol_text="\n";
    m_strip_attrs=true; // "  clone   , linkage yes" => attribute type="clone"
  }

  // print a value line
  void line(const string& label, const string& value, string xml_tag=NcbiEmptyString)
  {
    if(!m_use_xml) {
      m_out << label;
      if( label.find("***")==NPOS ) {
        m_out << ALIGN_M_W(value);
      }
      else {
        // no aligning for: ***single component + gap(s), ***no components, gaps only
        m_out << value;
      }

      m_out << m_eol_text;
      m_eol_text="\n"; // reset to default (AFTER printing whatever was there)
    }
    else {
      if(xml_tag.size()==0) {
        // false = add as a suffix
        bool add_label_as_attribute = ( last_tag.size() && last_tag[last_tag.size()-1]=='=' );

        if(!add_label_as_attribute || m_strip_attrs) {
          bool uc = !add_label_as_attribute;

          for(string::const_iterator it = label.begin();  it != label.end(); ++it) {
            if(isalpha(*it)) {
              xml_tag+= uc ? toupper(*it): tolower(*it);
              uc = false;
            }
            else {
              if(*it==',') break; // "***no components, gaps only: " -> "NoComponents"
              uc = !add_label_as_attribute;
            }
          }
        }
        else {
          // attribute pattern="AADB[02037555..02037659].1"
          xml_tag = NStr::XmlEncode( NStr::TruncateSpaces(label));
          if(xml_tag.size() && xml_tag[xml_tag.size()-1]==':') {
            xml_tag.resize( xml_tag.size()-1 );
            NStr::TruncateSpacesInPlace(xml_tag);
          }
        }
        m_strip_attrs=true; // reset to default

        if( add_label_as_attribute ) {
          xml_tag = last_tag + "\""+ xml_tag + "\"";
        }
        else if(isalpha(label[0])) {
          // may use as a prefix later
          last_tag = xml_tag;
        }
        else {
          // use the prior tag as a prefix, e.g. 'Objects' + 'WithSingleComponent'
          xml_tag = last_tag + xml_tag;
        }
      }

      m_out << " <" << xml_tag << ">" << NStr::XmlEncode(value);

      // strip any attributes from the closing tag
      SIZE_TYPE pos=xml_tag.find(" ");
      if(pos!=NPOS) xml_tag.resize(pos);

      m_out << "</" << xml_tag << ">\n";
    }
  }

  void line(const string& label, int value, const string& xml_tag=NcbiEmptyString)
  {
    line(label, NStr::NumericToString(value), xml_tag);
  }

  // print an empty line or static text; no-op in XML
  void line(const string& s=NcbiEmptyString)
  {
    if( !m_use_xml ) m_out << s << m_eol_text;
    m_eol_text="\n"; // reset to default (AFTER printing whatever was there)
  }
};

void CAgpValidateReader::x_PrintTotals(CNcbiOstream& out, bool use_xml) // without comment counts
{
  //// Counts of errors and warnings
  int e_count=m_AgpErr->CountTotals(CAgpErrEx::E_Last);
  // In case -fa or -len was used, add counts for G_InvalidCompId and G_CompEndGtLength.
  e_count+=m_AgpErr->CountTotals(CAgpErrEx::G_Last);
  int w_count=m_AgpErr->CountTotals(CAgpErrEx::W_Last);
  if(e_count || w_count || m_ObjCount) {
    if( m_ObjCount==0 && !m_AgpErr->m_MaxRepeatTopped &&
        e_count==m_AgpErr->CountTotals(CAgpErrEx::E_NoValidLines)
    ) return; // all files are empty, no need to say it again

    // these were already included in w_count
    int note_count = m_AgpErr->CountTotals(CAgpErrEx::W_ShortGap) + m_AgpErr->CountTotals(CAgpErrEx::W_AssumingVersion);

    CAgpErrEx::TMapCcodeToString hints;
    if(use_xml) {
      // jira/browse/GP-594: [iinsignificant warning are] making it hard for the naive user to know what to fix
      // w_count -= m_AgpErr->CountTotals(CAgpErrEx::W_GapLineMissingCol9);
      m_AgpErr->PrintTotalsXml(out, e_count, w_count-note_count, note_count, m_AgpErr->m_msg_skipped);
    }
    else {
      out << "\n";
      m_AgpErr->PrintTotals(out, e_count, w_count-note_count, note_count, m_AgpErr->m_msg_skipped);
      if(m_AgpErr->m_MaxRepeatTopped) {
        out << " (to print all: -limit 0; to skip some: -skip CODE)";
      }
      out << ".";
      if(m_AgpErr->m_MaxRepeat && (e_count+w_count) ) {
        out << "\n";
        if(!m_CheckCompNames && (
          m_AgpErr->CountTotals(CAgpErrEx::W_CompIsWgsTypeIsNot) ||
          m_AgpErr->CountTotals(CAgpErrEx::W_CompIsNotWgsTypeIs)
        ) ) {
            // W_CompIsNotWgsTypeIs is the last numerically, so the hint whiil get printed
            // after one or both of the above warnings
            hints[CAgpErrEx::W_CompIsNotWgsTypeIs] =
                "(Use -g to print lines with WGS component_id/component_type mismatch.)";
        }
        if(m_AgpErr->CountTotals(CAgpErrEx::W_ShortGap)) {
            hints[CAgpErrEx::W_ShortGap] = "(Use -show "+
                CAgpErrEx::GetPrintableCode(CAgpErrEx::W_ShortGap)+
                " to print lines with short gaps.)";
        }
      }
    }
    if(use_xml || (m_AgpErr->m_MaxRepeat && (e_count+w_count)) )
      m_AgpErr->PrintMessageCounts(out, CAgpErrEx::CODE_First, CAgpErrEx::CODE_Last, true, &hints);
  }
  if(m_ObjCount==0) {
    // out << "No valid AGP lines.\n";
    return;
  }

  // w: width for right alignment
  int w = NStr::IntToString((unsigned)(m_CompId2Spans.size())).size(); // +1;
  XPrintTotalsItem xprint(out, use_xml, w);
  xprint.line();

  //// Prepare component/gap types and counts for later printing
  string s_comp, s_gap;

  CValuesCount::TValPtrVec comp_cnt;
  m_TypeCompCnt.GetSortedValues(comp_cnt);

  for(CValuesCount::TValPtrVec::iterator
    it = comp_cnt.begin();
    it != comp_cnt.end();
    ++it
  ) {
    string *s = CAgpRow::IsGap((*it)->first[0]) ? &s_gap : &s_comp;

    if( s->size() ) *s+= ", ";
    *s+= (*it)->first;
    *s+= ":";
    *s+= NStr::IntToString((*it)->second);
  }

  //// Various counts of AGP elements

  if(use_xml) out << "<stats>\n";
  xprint.line();
  xprint.line( "Objects                : ", m_ObjCount);
  xprint.line( "- with single component: ", m_SingleCompObjects);
  if(m_SingleCompObjects_withGaps) {
    xprint.line( "  *** single component + gap(s): ", m_SingleCompObjects_withGaps, "SingleCompObjects_withGaps");
  }

  xprint.line();
  xprint.line( "Scaffolds              : ", m_ScaffoldCount);
  xprint.line( "- with single component: ", m_SingleCompScaffolds);
  if(m_SingleCompScaffolds_withGaps) {
    // note: skipping ALIGN_W
    xprint.line("  ***single component + gap(s): ", m_SingleCompScaffolds_withGaps, "SingleCompScaffolds_withGaps");
  }
  if(m_NoCompScaffolds) {
    // note: skipping ALIGN_W
    xprint.line("***no components, gaps only: ", m_NoCompScaffolds);
  }
  xprint.line();



  if( s_comp.size() ) {
    if(use_xml) {
      string comp_by_type = ", " + s_comp;
      // ", W: 1234, D: 5678" => <Comp type="W">1234</Comp>\n<Comp type="D">5678</Comp>
      NStr::ReplaceInPlace(comp_by_type, ", ", "</Comp>\n <Comp type=\"");
      NStr::ReplaceInPlace(comp_by_type, ":", "\">");
      // move "</Comp>\n" from start to end of string
      comp_by_type = comp_by_type.substr(8) + comp_by_type.substr(0, 8);
      out << comp_by_type;
    }
    else {
      if( NStr::Find(s_comp, ",")!=NPOS ) {
        // (W: 1234, D: 5678)
        xprint.m_eol_text = " (" + s_comp + ")\n";
      }
      else {
        // One type of components: (W) or (invalid type)
        xprint.m_eol_text = " (" + s_comp.substr( 0, NStr::Find(s_comp, ":") ) + ")\n";
      }
    }
  }
  xprint.line("Components                   : ", m_CompCount, string("Components type_counts=\"")+s_comp+"\"");

  if(m_CompCount) {
    xprint.line("  orientation +              : ", m_CompOri[CCompVal::ORI_plus ], "CompOri val=\"+\"");
    xprint.line("  orientation -              : ", m_CompOri[CCompVal::ORI_minus], "CompOri val=\"-\"");
    xprint.line("  orientation ? (formerly 0) : ", m_CompOri[CCompVal::ORI_zero ], "CompOri val=\"? (formerly 0)\"");
    xprint.line("  orientation na             : ", m_CompOri[CCompVal::ORI_na   ], "CompOri val=\"na\"");
  }
  if( m_comp2range_coll->size() &&
      m_AgpErr->CountTotals(CAgpErrEx::G_NsWithinCompSpan)==0 &&
      m_CompCount > m_AgpErr->CountTotals(CAgpErrEx::G_InvalidCompId)
  ) {
    xprint.line("Component spans in AGP are consistent with FASTA\n(i.e. do not include or intersect runs of Ns longer than 10 bp).");
  }


  xprint.line();
  if(m_GapCount) {
    // Print (N) if all components are of one type,
    //        or (N: 1234, U: 5678)
    if( s_gap.size() ) {
      if(use_xml) {
        string gap_u_n = ", " + s_gap;
        // ", U: 1234, N: 5678" => <Gap u_n="U">1234</Gap>\n<Gap u_n="N">5678</Gap>
        NStr::ReplaceInPlace(gap_u_n, ", ", "</Gap>\n <Gap u_n=\"");
        NStr::ReplaceInPlace(gap_u_n, ":", "\">");
        // move "</Gap>\n" from start to end of string
        gap_u_n = gap_u_n.substr(7) + gap_u_n.substr(0, 7);
        out << gap_u_n;
      }
      else {
        if( NStr::Find(s_gap, ",")!=NPOS ) {
          // (N: 1234, U: 5678)
          xprint.m_eol_text =  " (" + s_gap + ")\n";
        }
        else {
          // One type of gaps: (N)
          xprint.m_eol_text =  " (" + s_gap.substr( 0, NStr::Find(s_gap, ":") ) + ")\n";
        }
      }
    }
  }
  xprint.line("Gaps                   : ", m_GapCount, string("Gaps u_n_counts=\"")+s_gap+"\"");

  if(m_GapCount) {
    int linkageYesCnt =
      m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapClone   ]+
      m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapFragment]+
      m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapRepeat  ]+
      m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapScaffold];
    int linkageNoCnt = m_GapCount - linkageYesCnt;

    int doNotBreakCnt= linkageYesCnt + m_GapTypeCnt[CAgpRow::eGapFragment];
    int breakCnt     = linkageNoCnt  - m_GapTypeCnt[CAgpRow::eGapFragment];

    xprint.line("- do not break scaffold: ", doNotBreakCnt, "GapsWithinScaf");
    if(doNotBreakCnt) {
      xprint.last_tag="GapsWithinScaf_byType linkage=\"yes\" type=";
      if(m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapClone   ])
        xprint.line("  clone   , linkage yes: ", m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapClone   ]);
      if(m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapFragment])
        xprint.line("  fragment, linkage yes: ", m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapFragment]);

      if(m_GapTypeCnt[                   CAgpRow::eGapFragment])
        xprint.line("  fragment, linkage no : ", m_GapTypeCnt[                   CAgpRow::eGapFragment],
        "GapsWithinScaf_byType linkage=\"no\" type=\"fragment\""
        );

      if(m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapRepeat  ])
        xprint.line("  repeat  , linkage yes: ", m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapRepeat  ]);
      if(m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapScaffold  ])
        xprint.line("  scaffold, linkage yes: ", m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapScaffold]);
    }

    xprint.line("- break it, linkage no : ", breakCnt, "GapsBreakScaf");
    if(breakCnt) {
      xprint.last_tag="GapsBreakScaf_byType linkage=\"no\" type=";
      for(int i=0; i<CAgpRow::eGapCount; i++) {
        if(i==CAgpRow::eGapFragment) continue;
        if(m_GapTypeCnt[i])
          /*
          out<< "\n\t"
              << setw(15) << setiosflags(IOS_BASE::left) << CAgpRow::GapTypeToString(i)
              << ": " << ALIGN_W( m_GapTypeCnt[i] );
          */
          xprint.line(
            string("\t") + CAgpRow::GapTypeToString(i) +
            string("               ").substr(0, 15-strlen(CAgpRow::GapTypeToString(i)) ) + ": ",
            m_GapTypeCnt[i]
          );
      }
    }

    if(m_Ngap_ln2count.size()) {

      int mf_len=0;
      int mf_len_cnt=0;
      int gap_cnt=0; // only N or only U
      for(TMapIntInt::iterator it = m_Ngap_ln2count.begin();  it != m_Ngap_ln2count.end(); ++it) {
        if(mf_len_cnt < it->second) {
          mf_len_cnt = it->second;
          mf_len = it->first;
        }
        gap_cnt+=it->second;
      }

      if(mf_len_cnt>1) {
        xprint.line();
        string pct;
        NStr::DoubleToString(pct, 100.0*mf_len_cnt/gap_cnt, 1, NStr::fDoubleFixed);

        string label = "Most frequent N gap_length (" + pct + "% or " +
          NStr::IntToString(mf_len_cnt) + " lines): ";

        string tag = "MostFrequentNGapLen pct=\"" + pct + "\" cnt=\"" +
          NStr::IntToString(mf_len_cnt) + "\"";

        xprint.line(label, mf_len, tag);
      }
    }

  }

  if( m_agp_version == eAgpVersion_2_0 && m_ln_ev_flags2count.size() ) {
    xprint.line();
    xprint.line("Linkage evidence:");
    xprint.last_tag="LinkageEvidence value=";

    // sort by count
    typedef multimap<int,int> TMultiMapIntInt;
    TMultiMapIntInt cnt2ln_ev;
    size_t label_width=0;
    for(TMapIntInt::iterator it = m_ln_ev_flags2count.begin();  it != m_ln_ev_flags2count.end(); ++it) {
      cnt2ln_ev.insert(TMultiMapIntInt::value_type(it->second, it->first));
      string label = CAgpRow::LinkageEvidenceFlagsToString(it->first);
      if(label.size() > label_width) label_width = label.size();
    }
    if(label_width>40) label_width=40;
    for(TMultiMapIntInt::reverse_iterator it = cnt2ln_ev.rbegin();  it != cnt2ln_ev.rend(); ++it) {
      string label = CAgpRow::LinkageEvidenceFlagsToString(it->second);
      if(label.size()<label_width) label +=
        string("                                        ").substr(0, label_width-label.size());

      xprint.m_strip_attrs=false;
      xprint.line(string("  ") + label +": ", it->first );
    }
  }


  if(m_ObjCount) {
    x_PrintPatterns(m_objNamePatterns, "Object names",
      m_CheckObjLen ? m_comp2len->m_count : 0, NULL,
      out, use_xml
    );
  }

  if(m_CompId2Spans.size()) {
    CAccPatternCounter compNamePatterns;
    for(TCompId2Spans::iterator it = m_CompId2Spans.begin();
      it != m_CompId2Spans.end(); ++it)
    {
      compNamePatterns.AddName(it->first);
    }
    bool hasSuspicious = x_PrintPatterns(compNamePatterns, "Component names",
      m_CheckObjLen ? 0 : m_comp2len->m_count,
      m_comp2len == &m_scaf2len ? " Scaffold from component AGP" : NULL,
      out, use_xml
    );
    if(!m_CheckCompNames && hasSuspicious ) {
      xprint.line("Use -g or \"-show all\" to print lines with suspicious accessions.");
    }

    const int MAX_objname_eq_comp=3;
    int cnt_objname_eq_comp=0;
    string str_objname_eq_comp;
    for(TObjSet::iterator it = m_ObjIdSet.begin();  it != m_ObjIdSet.end(); ++it) {
      if(m_CompId2Spans.find(*it)!=m_CompId2Spans.end()) {
        cnt_objname_eq_comp++;
        if(cnt_objname_eq_comp<=MAX_objname_eq_comp) {
          if(cnt_objname_eq_comp>1) str_objname_eq_comp+=", ";
          str_objname_eq_comp+=*it;
        }
      }
    }

    // to do: output to XML
    if(cnt_objname_eq_comp && !use_xml) {
      out<< "\n" << cnt_objname_eq_comp << " name"
          << (cnt_objname_eq_comp==1?" is":"s are")
          << " used both as object and as component_id:\n";
      out<< "  " << str_objname_eq_comp;
      if(cnt_objname_eq_comp>MAX_objname_eq_comp) out << ", ...";
      out << "\n";
    }
  }
  if(use_xml) out << "</stats>\n";
}

void CAgpValidateReader::PrintTotals(CNcbiOstream& out, bool use_xml)
{
  x_PrintTotals(out, use_xml);

  if(m_comp2len->size()) {
    x_PrintIdsNotInAgp(out, use_xml);
  }

  if(use_xml) {
    if(m_CommentLineCount) out << " <CommentLineCount>" << m_CommentLineCount << "</CommentLineCount>\n";
    if(m_EolComments)      out << " <EolComments>"      << m_EolComments      << "</EolComments>\n";
  }
  else {
    if(m_CommentLineCount || m_EolComments) out << "\n";
    if(m_CommentLineCount) {
      out << "#Comment line count    : " << m_CommentLineCount << "\n";
    }
    if(m_EolComments) {
      out << "End of line #comments  : " << m_EolComments << "\n";
    }
  }
}

// TRUE:
//   &s at input &pos has one of 3 valid kinds of strings: [123..456]  [123,456]  123
//   output args, strings of digits                      :  sd1  sd2    sd1 sd2   sd1 (sd2 empty)
//   output &pos is moved past the recognized string (which can have any tail)
// FALSE: not a valid number or range format.
static bool ReadNumberOrRange(const string& s, int& pos, string& sd1, string& sd2)
{
  bool openBracket=false;
  if( s[pos]=='[' ) {
    openBracket=true;
    pos++;
  }

  //// count digits -> numDigits
  // DDD [DDD,DDD] [DDD..DDD]
  // ^p1  ^p1 ^p2   ^p1  ^p2
  int p1=pos;
  int len1=0;
  int p2=0;
  while( pos<(int)s.size() ) {
    char ch=s[pos];

    if( isdigit(ch) ) {}
    else if(openBracket) {
      // separators, closing bracket
      if(pos==p1) return false;
      if(ch=='.' || ch==',') {
        if( pos >= (int)s.size()-1 || len1 )
          return false; // nothing after separator || encountered second separator
        len1=pos-p1;
        if(ch=='.') {
          // ..
          pos++;
          if( pos >= (int)s.size() || s[pos] != '.' ) return false;
        }
        p2=pos+1;
      }
      else if(ch==']') {
        if( !p2 || p2==pos ) return false;
        openBracket=false;
        pos++;
        break;
      }
      else return false;
    }
    else break;

    pos++;
  }

  if(openBracket || pos==p1) return false;
  if(!len1) {
    // a plain number, no brackets
    sd1=s.substr(p1, pos-p1);
    sd2=NcbiEmptyString;
  }
  else {
    sd1=s.substr(p1, len1);
    sd2=s.substr(p2, pos-1-p2);
  }
  return true;
}

enum EComponentNameCategory
{
  fNucleotideAccession=0,
  fUnknownFormat      =1,
  fProtein            =2,
  fOneAccManyVer      =4,
  f_category_mask     =7,

  fSome               = 8 // different results for [start..stop]
};
static int GetNameCategory(const string& s)
{
  //// count letters_ -> numLetters
  int numLetters=0;
  for(;;) {
    if( numLetters>=(int)s.size() ) return fUnknownFormat;
    if( !isalpha(s[numLetters]) && s[numLetters]!='_' ) break;
    numLetters++;
  }
  if(numLetters<1 || numLetters>4) return fUnknownFormat;

  int pos=numLetters;
  string sd1, sd2; // strings of digits
  if( !ReadNumberOrRange(s, pos, sd1, sd2) ) return fUnknownFormat;
  if(sd2.size()==0 && s[pos]=='[') {
    // 111[222...333] => [111222...111333]
    string ssd1, ssd2;
    if( !ReadNumberOrRange(s, pos, ssd1, ssd2) || ssd2.size()==0 ) return fUnknownFormat;
    sd2 =sd1+ssd2;
    sd1+=ssd1;
  }

  //// optional .version or .[range of versions]
  string ver1, ver2; // string of digits
  if(pos<(int)s.size()) {
    if(s[pos]!='.') return fUnknownFormat;
    pos++;

    if( !ReadNumberOrRange(s, pos, ver1, ver2) ) return fUnknownFormat;
    if(pos<(int)s.size()) return fUnknownFormat;

    if(ver1.size()) ver1=string(".")+ver1;
    if(ver2.size()) ver2=string(".")+ver2;

    if(ver1.size()>4 && ver2.size()>4) return fUnknownFormat;
  }

  if(sd2.size()==0) {
    // one accession
    if(ver2.size()!=0) return fOneAccManyVer;
    CSeq_id::EAccessionInfo acc_inf = CSeq_id::IdentifyAccession(s);
    if(  acc_inf & CSeq_id::fAcc_prot ) return fProtein;
    if(  acc_inf & CSeq_id::fAcc_nuc  ) return fNucleotideAccession; // the best possible result
    return fUnknownFormat;
  }

  // check both ends of the range in case one has a different number of digits
  string ltr=s.substr(0, numLetters);
  // Note: we do not care if ver1 did not actually came from ltr+sd1, ver2 from ltr+sd2, etc.
  int c1 = GetNameCategory( ltr + sd1 + ver1 );
  int c2 = GetNameCategory( ltr + sd2 + (ver2.size()?ver2:ver1));
  if(c1==c2) {
    if(c1==fNucleotideAccession && (ver1.size()>4 || ver2.size()>4) )
      return fUnknownFormat|fSome;
    return c1;
  }
  return fSome|(c1>c2?c1:c2); // some accessions are suspicious
}

// Sort by accession count, print not more than MaxPatterns or 2*MaxPatterns
bool CAgpValidateReader::x_PrintPatterns(
  CAccPatternCounter& namePatterns, const string& strHeader, int fasta_count, const char* count_label,
  CNcbiOstream& out, bool use_xml)
{
  const int MaxPatterns=10;
  //const int MaxPatterns=2;
  const string SPACES="                                        ";

  // Sorted by count to print most frequent first
  CAccPatternCounter::TMapCountToString cnt_pat; // multimap<int,string>
  namePatterns.GetSortedPatterns(cnt_pat);
  SIZE_TYPE cnt_pat_size=cnt_pat.size();

  //out << "\n";

  // Calculate widths of columns 1 (wPattern) and 2 (w)
  int w = NStr::IntToString(
      cnt_pat.rbegin()->first // the biggest count
    ).size()+1;
  XPrintTotalsItem xprint(out, use_xml, w);
  xprint.line();

  int wPattern=strHeader.size()-2;
  int totalCount=0;
  int nucCount=0;
  int otherCount=0;
  int patternsPrinted=0;
  bool mixedPattern=false;
  for(
    CAccPatternCounter::TMapCountToString::reverse_iterator
    it = cnt_pat.rbegin(); it != cnt_pat.rend(); it++
  ) {
    if( ++patternsPrinted<=MaxPatterns ||
        cnt_pat_size<=2*MaxPatterns
    ) {
      int i = it->second.size();
      if( i > wPattern ) wPattern = i;
    }
    else {
      if(w+15>wPattern) wPattern = w+15;
    }
    totalCount+=it->first;
    int code=GetNameCategory(it->second);
    ( code==fNucleotideAccession ? nucCount : otherCount ) += it->first;
    if(code & fSome) mixedPattern=true;
  }

  bool mixedCategories=(nucCount && otherCount);
  if(mixedCategories && wPattern<20) wPattern=20;
  // Print the total
  //string xml_tag_for_pattern_and_count = "";
  string xml_outer_tag;
  if(strHeader.size()) {
    if(use_xml) {
      xml_outer_tag = strHeader.substr(0, strHeader.find(' ')) + "Names";
      out << "<" << xml_outer_tag << ">\n";
    }
    if(fasta_count && fasta_count!=totalCount) {
      // this is not needed in XML since there is no FASTA to compare to yet...
      xprint.m_eol_text = string(" != ") +
        NStr::NumericToString(fasta_count) + " in the " +
        (count_label ? count_label : "FASTA")+"\n";
    }
    xprint.line(
        strHeader+SPACES.substr(
          0, wPattern+2>(int)strHeader.size() ? wPattern+2-strHeader.size() : 0
        ) + ": ",
        totalCount,
        "count"
      );
    //xml_tag_for_pattern_and_count = xprint.last_tag;
  }

  bool printNuc=(nucCount>0);
  // 1 or 2 (if mixedCategories) iterations
  for(;;) {
    if(mixedCategories) {
      // Could be an error - print extra sub-headings to get attention
      xprint.m_eol_text=NcbiEmptyString; // no "\n" for the next line()
      xprint.line(string("------------------------").substr(0, wPattern-20)+" ");

      if     (printNuc) xprint.line("Nucleotide accessions: ", nucCount);
      else              xprint.line("OTHER identifiers    : ", otherCount);
    }

    // Print the patterns
    patternsPrinted=0;
    int accessionsSkipped=0;
    int patternsSkipped=0;
    for(
      CAccPatternCounter::TMapCountToString::reverse_iterator
      it = cnt_pat.rbegin(); it != cnt_pat.rend(); it++
    ) {
      int code=GetNameCategory(it->second);
      if(mixedCategories && (code==fNucleotideAccession)!=printNuc) continue;

      // Limit the number of lines to MaxPatterns or 2*MaxPatterns
      if( ++patternsPrinted<=MaxPatterns ||
          cnt_pat_size<=2*MaxPatterns
      ) {

        string acc_warning;
        if(!printNuc) {
          switch(code)
          {
            case fUnknownFormat|fSome:
            case fOneAccManyVer|fSome:
              acc_warning ="some local or misspelled"; break;
            case fProtein|fSome:
              acc_warning ="some look like protein accessions"; break;

            case fUnknownFormat: if(!(mixedCategories || mixedPattern)) break;
            case fOneAccManyVer: acc_warning ="local or misspelled"; break;
            case fProtein      : acc_warning ="looks like protein accession"; break;
          }
        }
        string xml_tag = "";
        if( acc_warning.size() ) {
          xprint.m_eol_text = string(" (") + acc_warning + ")\n";
        }

        // output pattern as an attribute
        xprint.last_tag =
          //xml_tag_for_pattern_and_count +
          "names"+
          ( acc_warning.size() ? string(" warn=\"")+acc_warning+"\"" : "" )+
          " pattern=";
        xprint.m_strip_attrs=false;

        xprint.line(
          // pattern
          string("  ") + it->second +
          SPACES.substr(0, wPattern - it->second.size()) + ": ",
          // count
          it->first
        );
        xprint.last_tag = NcbiEmptyString;
      }
      else {
        // accessionsSkipped += CAccPatternCounter::GetCount(*it);
        accessionsSkipped += it->first;
        patternsSkipped++;
      }
    }

    if(accessionsSkipped) {
      string s = "other ";
      s+=NStr::IntToString(patternsSkipped);
      s+=" patterns";
      xprint.line( "  " + s + SPACES.substr(0, wPattern - s.size()) + ": ",
          accessionsSkipped,
          //xml_tag_for_pattern_and_count +
          "names"
          " patterns=\"other\""
      );
    }

    if(!mixedCategories || !printNuc) break;
    printNuc=false;
  }

  if(use_xml && xml_outer_tag.size()) {
    out << "</" << xml_outer_tag << ">\n";
  }

  return mixedCategories||mixedPattern;
}

// label = "component(s) from FASTA not found in AGP"
// label = "scaffold(s) not found in Chromosome from scaffold AGP"
// 2012/02/21: xml mode not tested since the current CGI has no info to provide this kind of validation
void CAgpValidateReader::x_PrintIdsNotInAgp(CNcbiOstream& out, bool use_xml)
{
  CAccPatternCounter patterns;
  set<string> ids;
  int cnt=0;

    // ids in m_comp2len but not in m_CompId2Spans
  for(CMapCompLen::iterator it = m_comp2len->begin();  it != m_comp2len->end(); ++it) {
    string id;
    if(m_CheckObjLen) {
      // ids in m_comp2len but not in m_ObjIdSet
      TObjSet::iterator obj = m_ObjIdSet.find(it->first);
      if(obj==m_ObjIdSet.end()) {
        id=it->first;
      }
    }
    else {
      TCompId2Spans::iterator spans = m_CompId2Spans.find(it->first);
      if(spans==m_CompId2Spans.end()) {
        id=it->first;
      }
    }
    if( id.size() &&
      id.find("|") == NPOS // works only if AGP contains plain accessions...
    ) {
      patterns.AddName(it->first);
      ids.insert(it->first);
      cnt++;
    }
  }

  if(cnt>0) {
    string label =
      m_CheckObjLen ? "object name(s) in FASTA not found in AGP" :
      m_comp2len == &m_scaf2len ? "scaffold(s) not found in Chromosome from scaffold AGP":
      "component name(s) in FASTA not found in AGP";

    if(use_xml) {
      // print both patterns and ALL missing names
      label = label.substr(0, label.find(' '));
      out << "<MissingSeqNames level=\""+label+"\">\n";
      for(set<string>::iterator it = ids.begin();  it != ids.end(); ++it) {
        out << " <name>" << NStr::XmlEncode(*it) << "</name>\n";
      }
    }
    else {
      string tmp;
      NStr::Replace(label, "(s)", cnt==1 ? "" : "s", tmp);
      out << "\nWARNING -- " << cnt << " " << tmp << ": ";
    }

    if(!use_xml && cnt==1) {
      out << *(ids.begin()) << "\n";
    }
    else if(!use_xml && (cnt<m_AgpErr->m_MaxRepeat||m_AgpErr->m_MaxRepeat==0)) {
      out << "\n";
      for(set<string>::iterator it = ids.begin();  it != ids.end(); ++it) {
        out << "  " << *it << "\n";
      }
    }
    else {
      x_PrintPatterns(patterns, NcbiEmptyString, 0, NULL, out, use_xml);
    }
    if(use_xml) {
      out << "</MissingSeqNames>\n";
    }
  }
}

void CAgpValidateReader::SetRowOutput(IAgpRowOutput* row_output)
{
  m_row_output = row_output;
}

//// class CValuesCount

void CValuesCount::GetSortedValues(TValPtrVec& out)
{
  out.clear(); out.reserve( size() );
  for(iterator it = begin();  it != end(); ++it) {
    out.push_back(&*it);
  }
  std::sort( out.begin(), out.end(), x_byCount );
}

void CValuesCount::add(const string& c)
{
  iterator it = find(c);
  if(it==end()) {
    (*this)[c]=1;
  }
  else{
    it->second++;
  }
}

int CValuesCount::x_byCount( value_type* a, value_type* b )
{
  if( a->second != b->second ){
    return a->second > b->second; // by count, largest first
  }
  return a->first < b->first; // by name
}

//// class CCompSpans - stores data for all preceding components
CCompSpans::TCheckSpan CCompSpans::CheckSpan(int span_beg, int span_end, bool isPlus)
{
  // The lowest priority warning (to be ignored for draft seqs)
  TCheckSpan res( begin(), CAgpErrEx::W_DuplicateComp );

  for(iterator it = begin();  it != end(); ++it) {
    // A high priority warning
    if( (it->beg <= span_beg && span_beg <= it->end) ||
        (it->beg <= span_end && span_end <= it->end) )
      return TCheckSpan(it, CAgpErrEx::W_SpansOverlap);

    // A lower priority warning (to be ignored for draft seqs)
    if( ( isPlus && span_beg < it->beg) ||
        (!isPlus && span_end > it->end)
    ) {
      res.first  = it;
      res.second = CAgpErrEx::W_SpansOrder;
    }
  }

  return res;
}

void CCompSpans::AddSpan(const CCompVal& span)
{
  push_back(span);
}

//// class CMapCompLen
int CMapCompLen::AddCompLen(const string& acc, int len, bool increment_count)
{
  TMapStrInt::value_type acc_len(acc, len);
  TMapStrIntResult insert_result = insert(acc_len);
  if(insert_result.second == false) {
    if(insert_result.first->second != len)
      return insert_result.first->second; // error: already have a different length
  }
  if(increment_count) m_count++;
  return 0; // success
}

END_NCBI_SCOPE

