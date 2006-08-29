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
 *      Syntactic validation tests that can be preformed solely by the
 *      information in the AGP file.
 *
 *
 */

#include <ncbi_pch.hpp>
#include "SyntaxValidator.hpp"

USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE

CAgpSyntaxValidator::CAgpSyntaxValidator()
{
  prev_end = 0;
  prev_part_num = 0;
  prev_line_num = 0;
  prev_line_error_occured = false;
  componentsInLastScaffold = 0;

  m_ObjCount = 0;
  m_ScaffoldCount = 0;
  m_SingletonCount = 0;
  m_CompCount = 0;
  m_CompPosCount = 0;
  m_CompNegCount = 0;
  m_CompZeroCount = 0;
  m_GapCount = 0;

  m_TypeGapCnt["fragmentyes"] = 0;
  m_TypeGapCnt["fragmentno"] = 0;
  m_TypeGapCnt["split_finishedyes"] = 0;
  m_TypeGapCnt["split_finishedno"] = 0;
  m_TypeGapCnt["cloneyes"] = 0;
  m_TypeGapCnt["cloneno"] = 0;
  m_TypeGapCnt["contigyes"] = 0;
  m_TypeGapCnt["contigno"] = 0;
  m_TypeGapCnt["centromereyes"] = 0;
  m_TypeGapCnt["centromereno"] = 0;
  m_TypeGapCnt["short_armyes"] = 0;
  m_TypeGapCnt["short_armno"] = 0;
  m_TypeGapCnt["heterochromatinyes"] = 0;
  m_TypeGapCnt["heterochromatinno"] = 0;
  m_TypeGapCnt["telomereyes"] = 0;
  m_TypeGapCnt["telomereno"] = 0;

  // initialize values
  m_GapTypes.insert("fragment");
  m_GapTypes.insert("split_finished");
  m_GapTypes.insert("clone");
  m_GapTypes.insert("contig");
  m_GapTypes.insert("centromere");
  m_GapTypes.insert("short_arm");
  m_GapTypes.insert("heterochromatin");
  m_GapTypes.insert("telomere");
  m_GapTypes.insert("scaffold");

  m_ComponentTypeValues.insert("A");
  m_ComponentTypeValues.insert("D");
  m_ComponentTypeValues.insert("F");
  m_ComponentTypeValues.insert("G");
  m_ComponentTypeValues.insert("P");
  m_ComponentTypeValues.insert("N");
  m_ComponentTypeValues.insert("O");
  m_ComponentTypeValues.insert("W");

  m_OrientaionValues.insert("+");
  m_OrientaionValues.insert("-");
  m_OrientaionValues.insert("0");

  m_LinkageValues.insert("yes");
  m_LinkageValues.insert("no");
}

bool CAgpSyntaxValidator::ValidateLine(
  CNcbiOstrstream* msgStream, const SDataLine& dl,
  const string& text_line, bool last_validation)
{
  bool new_obj = false;
  bool error;

  int obj_begin = 0;
  int obj_end = 0;
  int part_num = 0;
  int comp_start = 0;
  int comp_end = 0;
  int gap_len = 0;

  int obj_range_len = 0;
  int comp_len = 0;

  // for compatibility with AGP_WARNING() AGP_ERROR()
  m_LineErrorOccured = false;
  m_ValidateMsg=msgStream;


  pair<TCompId2Spans::iterator, bool> id_insert_result;
  TObjSetResult obj_insert_result;

  if (dl.object != prev_object || last_validation) {
    prev_end = 0;
    prev_part_num = 0;
    prev_object = dl.object;
    new_obj = true;
    m_ObjCount++;

    // 2006/08/28
    m_ScaffoldCount++;
    if(componentsInLastScaffold==1) m_SingletonCount++;
    componentsInLastScaffold=0;
  }

  if( new_obj && (prev_component_type == "N") &&
      (prev_gap_type == "fragment")
  ) {
    // A new scafold. Previous line is a scaffold
    // ending with a fragment gap

    // special error reporting mechanism since the
    //  error is on the previous line. Directly Log
    //  the error messages.
    if(!prev_line_error_occured) {
      LOG_POST("\n\n" << prev_line_filename << ", line "
        << prev_line_num << ":\n" << prev_line);
    }
    LOG_POST("\tWARNING: Fragment gap at the end of a scaffold.");
  }
  if(last_validation) {
    // Finished validating all lines.
    // Just did a final gap ending check.
    return m_LineErrorOccured;
  }

  if(new_obj) {
    obj_insert_result = m_ObjIdSet.insert(dl.object);
    if (obj_insert_result.second == false) {
      AGP_ERROR("Duplicate object: " << dl.object);
    }
  }

  obj_begin = x_CheckIntField(
    dl.line_num, dl.begin, "object_begin"
  );
  if( obj_begin && ( obj_end = x_CheckIntField(
        dl.line_num, dl.end, "object_end"
  ))){
    if(new_obj && obj_begin != 1) {
      AGP_ERROR("First line of an object must have "
        "object_begin=1");
    }

    obj_range_len = x_CheckRange(
      dl.line_num, prev_end, obj_begin, obj_end,
      "object_begin", "object_end");
    prev_end = obj_end;
  }

  if (part_num = x_CheckIntField(
    dl.line_num, dl.part_num, "part_num"
  )) {
    if(part_num != prev_part_num+1) {
      AGP_ERROR("Part number (column 4) != previous "
        "part number +1");
    }
    prev_part_num = part_num;
  }

  x_CheckValues( dl.line_num, m_ComponentTypeValues,
    dl.component_type,"component_type");


  if (dl.component_type == "N") { // gap
    m_GapCount++;
    error = false;
    if(gap_len = x_CheckIntField(
      dl.line_num, dl.gap_length, "gap_length"
    )) {
      if (obj_range_len && obj_range_len != gap_len) {
        AGP_ERROR("Object range length not equal to gap length"
          ": " << obj_range_len << " != " << gap_len);
        error = true;
      }
    }
    else {
      error = true;
    }

    if( x_CheckValues(
          dl.line_num, m_GapTypes, dl.gap_type, "gap_type"
        ) && x_CheckValues(
          dl.line_num, m_LinkageValues, dl.linkage, "linkage"
        )
    ) {
      string key = dl.gap_type + dl.linkage;
      m_TypeGapCnt[key]++;
    }
    else {
      error = true;
    }

    if (new_obj) {
      if (dl.gap_type == "fragment") {
        AGP_WARNING("Scaffold begins with a fragment gap.");
      }
      else {
        AGP_WARNING("Object begins with a scaffold-ending gap."
        );
      }
    }

    if (prev_component_type == "N") {
      // Previous line a gap. Check the gap_type.
      if(prev_gap_type == "fragment") {
        // two gaps in a row

        if (!new_obj) {
          if (dl.gap_type == "fragment") {
            AGP_WARNING("Two fragment gaps in a row.");
          }
          else {
            // Current gap type is a scaffold boundary

            // special error reporting mechanism since
            // the error is on the previous line.
            // Directly Log the error messages.
            if(!prev_line_error_occured) {
              LOG_POST("\n\n" << prev_line_filename
                << ", line " << prev_line_num
                << ":\n" << prev_line);
            }
            LOG_POST("\tWARNING: "
              "Next line is a scaffold-ending gap. "
              "Current line is a fragment gap. "
              "A Scaffold should not end with a gap."
            );
          }
        }
      }
      else {
        if (!new_obj) {
          // Previous line is a a scafold gap
          // This line is the start of a new scaffold
          if (dl.gap_type == "fragment") {
            // Scaffold starts with a fragment gap.
            AGP_WARNING("Scaffold should not begin with a gap."
              "(Previous line was a scaffold-ending gap.)" );
          }
          else {
            // Current gap type is a scaffold boundary.
            //  Two scaffold gaps in a row.
            AGP_WARNING( "Two scaffold-ending gaps in a row.");
          }
        }
      }
    }
    else if( dl.gap_type != "fragment" ) {
      // 2006/08/28: Previous line is the last component of a scaffold
      m_ScaffoldCount++;
      if(componentsInLastScaffold==1) m_SingletonCount++;
    }
    if( dl.gap_type != "fragment" ) {
      componentsInLastScaffold=0;
    }

    if (error) {
      // Check if the line should be a component type
      // i.e., dl.component_type != "N"

      // A component line has integers in column 7
      // (component start) and column 8 (component end);
      // +, - or 0 in column 9 (orientation).
      if( x_CheckIntField(
            dl.line_num, dl.component_start,
            "component_start", NO_LOG
          ) && x_CheckIntField(
            dl.line_num, dl.component_end,
            "component_end", NO_LOG
          ) && x_CheckValues(
            dl.line_num, m_OrientaionValues,
            dl.orientation, "orientation", NO_LOG
      ) ) {
        AGP_WARNING( "Line with component_type=N appears to be"
          " a component line and not a gap line.");
      }
    }
  }
  else { // component
    error = false;
    m_CompCount++;
    componentsInLastScaffold++;

    if( (comp_start = x_CheckIntField(
          dl.line_num,dl.component_start,"component_start"
        )) &&
        (comp_end   = x_CheckIntField(
          dl.line_num, dl.component_end ,"component_end"
        ))
    ) {
      comp_len = x_CheckRange(
        dl.line_num, 0, comp_start, comp_end,
        "component_start", "component_end"
      );
      if( comp_len && obj_range_len &&
          comp_len != obj_range_len
      ) {
        AGP_ERROR( "Object range length not equal to component"
          " range length");
        error = true;
      }
    } else {
      error = true;
    }

    if (x_CheckValues(
      dl.line_num, m_OrientaionValues, dl.orientation,
      "orientation"
    )) {
      if( dl.orientation == "0") {
        AGP_ERROR( "Component cannot have an unknown"
          " orientation.");
        m_CompZeroCount++;
        error = true;
      } else if (dl.orientation == "+") {
          m_CompPosCount++;
      } else {
          m_CompNegCount++;
      }
    } else {
      error = true;
    }


    CRange<TSeqPos>  component_range(comp_start, comp_end);
    TCompIdSpansPair value_pair(
      dl.component_id, TCompSpans(component_range)
    );
    id_insert_result = m_CompId2Spans.insert(value_pair);
    if (id_insert_result.second == false) {
      TCompSpans& collection =
        (id_insert_result.first)->second;

      string str_details="";
      if(collection.IntersectingWith(component_range)) {
        str_details = " The span overlaps "
          "a previous span for this component.";
      }
      else if (
        comp_start < (int)collection.GetToOpen() &&
        dl.orientation!="-"
      ) {
        str_details=" Component span is out of order.";
      }
      AGP_WARNING("Duplicate component id found." << str_details);
      collection.CombineWith(component_range);
    }

    if(error) {
      // Check if the line should be a gap type
      //  i.e., dl.component_type == "N"

      // Gap line has integer (gap len) in column 6,
      // gap type value in column 7,
      // a yes/no in column 8.
      // (vsap) was: gap_len = x_CheckIntField(
      if(x_CheckIntField(
          dl.line_num, dl.gap_length, "gap_length",
        NO_LOG) && x_CheckValues(
          dl.line_num, m_GapTypes, dl.gap_type, "gap_type",
        NO_LOG) && x_CheckValues(
          dl.line_num, m_LinkageValues, dl.linkage, "linkage",
        NO_LOG)
      ) {
        AGP_WARNING( "Line with component_type="
          << dl.component_type <<" appears to be a gap line "
          "and not a component line");
      }
    }
  }

  prev_component_type = dl.component_type;
  prev_gap_type = dl.gap_type;
  prev_line = text_line;
  prev_line_num = dl.line_num;
  //prev_line_filename = m_app->m_CurrentFileName;
  prev_line_error_occured = m_LineErrorOccured;

  return m_LineErrorOccured;
}

/*
void CAgpSyntaxValidator::x_OnGapLine(
  const SDataLine& dl, const string& text_line)
{

}

void CAgpSyntaxValidator::x_OnComponentLine(
  const SDataLine& dl, const string& text_line)
{

}

bool CAgpSyntaxValidator::GapBreaksScaffold(int type, int linkage)
{
  // Not implemented
  return false;
}
*/

int CAgpSyntaxValidator::x_CheckIntField(
  int line_num, const string& field,
  const string& field_name, bool log_error)
{
  int field_value = 0;
  try {
      field_value = NStr::StringToInt(field);
  } catch (...) {
  }

  if (field_value <= 0  &&  log_error) {
    AGP_ERROR( field_name << " field must be a positive "
    "integer");
  }
  return field_value;
}

int CAgpSyntaxValidator::x_CheckRange(
  int line_num, int start, int begin, int end,
  string begin_name, string end_name)
{
  int length = 0;
  if(begin <= start){
    AGP_ERROR( begin_name << " field overlaps the previous "
      "line");
  }
  else if (end < begin) {
    AGP_ERROR(end_name << " is less than " << begin_name );
  }
  else {
    length = end - begin + 1;
  }

  return length;
}

bool CAgpSyntaxValidator::x_CheckValues(
  int line_num,
  const TValuesSet& values,
  const string& value,
  const string& field_name,
  bool log_error)
{
  if (values.count(value) == 0) {
    if (log_error)
      AGP_ERROR("Invalid value for " << field_name);
    return false;
  }
  return true;
}

void CAgpSyntaxValidator::PrintTotals()
{
  LOG_POST("\n"
    "Objects     : " << m_ObjCount << "\n" <<
    "Scaffolds   : " << m_ScaffoldCount   << "\n"
    "  singletons: " << m_SingletonCount << "\n\n"
    "Unique Component Accessions: "<< m_CompId2Spans.size() <<"\n"<<
    "Lines with Components      : " << m_CompCount        << "\n" <<
    "\torientation +       : " << m_CompPosCount   << "\n" <<
    "\torientation -       : " << m_CompNegCount   << "\n" <<
    "\torientation 0       : " << m_CompZeroCount  << "\n\n"

 << "Gaps: " << m_GapCount
 << "\n\t   with linkage: yes\tno"
 << "\n\tFragment       : "<<m_TypeGapCnt["fragmentyes"       ]
 << "\t"                   <<m_TypeGapCnt["fragmentno"        ]
 << "\n\tClone          : "<<m_TypeGapCnt["cloneyes"          ]
 << "\t"                   <<m_TypeGapCnt["cloneno"           ]
 << "\n\tContig         : "<<m_TypeGapCnt["contigyes"         ]
 << "\t"                   <<m_TypeGapCnt["contigno"          ]
 << "\n\tCentromere     : "<<m_TypeGapCnt["centromereyes"     ]
 << "\t"                   <<m_TypeGapCnt["centromereno"      ]
 << "\n\tShort_arm      : "<<m_TypeGapCnt["short_armyes"      ]
 << "\t"                   <<m_TypeGapCnt["short_armno"       ]
 << "\n\tHeterochromatin: "<<m_TypeGapCnt["heterochromatinyes"]
 << "\t"                   <<m_TypeGapCnt["heterochromatinno" ]
 << "\n\tTelomere       : "<<m_TypeGapCnt["telomereyes"       ]
 << "\t"                   <<m_TypeGapCnt["telomereno"        ]
 << "\n\tSplit_finished : "<<m_TypeGapCnt["split_finishedyes" ]
 << "\t"                   <<m_TypeGapCnt["split_finishedno"  ]
  );
}


END_NCBI_SCOPE

