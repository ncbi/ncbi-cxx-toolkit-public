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
 *      Lou Friedman
 *
 * File Description:
 *      Validate AGP data. The AGP data source is either an AGP file
 *      or from the AGP DB. A command line option picks the type of
 *      validation; either syntactic or semantic. Syntactic validation
 *      are tests that can be preformed solely by the information that
 *      is in the AGP file. Semantic validation are trests that require
 *      extra, external information, such as, the size of a sequence.
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/range_coll.hpp>

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
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>


#include <iostream>
#include "db.hpp"

using namespace ncbi;
using namespace objects;

#define START_LINE_VALIDATE_MSG(line_num, line) \
        m_LineErrorOccured = false; \
        if (m_ValidateMsg != NULL) delete m_ValidateMsg; \
        m_ValidateMsg = new CNcbiStrstream; \
        *m_ValidateMsg  << "\n\n" << m_CurrentFileName \
        << ", line " << line_num << ": \n" << line

#define END_LINE_VALIDATE_MSG LOG_POST(m_ValidateMsg->str())

#define COLLECT_VALIDATE_MSG(type, msg) \
        m_LineErrorOccured = true; \
        *m_ValidateMsg << "\n\t" << type << ": " <<  msg

#define VALIDATION_ERROR(msg) \
        COLLECT_VALIDATE_MSG("ERROR", msg)

#define COLLECT_VALIDATE_WARNING(msg) \
        COLLECT_VALIDATE_MSG("WARNING", msg)

#define NO_LOG false

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

    CRef<CDb>   m_VolDb;

    // count varibles
    int m_ObjCount;

    int m_CompCount;
    set<string> m_UniqComp;

    int m_CompPosCount;
    int m_CompNegCount;
    int m_CompZeroCount;
    int m_GapCount;


    // count the dfiffernt types of gaps
    map<string, int> m_TypeGapCount;

    bool m_LineErrorOccured;
    CNcbiStrstream* m_ValidateMsg;

    // data of an AGP line either
    //  from a file or the agp adb
    struct SDataLine {
        int     line_num;
        string  object;
        string  begin;
        string  end;
        string  part_num;
        string  component_type;
        string  component_id;
        string  component_start;
        string  component_end;
        string  orientation;
        string  gap_length;
        string  gap_type;
        string  linkage;
    };
    typedef vector<SDataLine> TDataLines;

    // keep track of the componet and object ids used
    //  in the AGP. Used to detect duplicates and
    //  duplicates with seq range intersections.
    typedef CRangeCollection<TSeqPos> TIdMapValue;
    typedef map<string, TIdMapValue> TIdMap;
    typedef pair<string, TIdMapValue> TIdMapKeyValuePair;
    typedef pair<TIdMap::iterator, bool> TIdMapResult;
    TIdMap m_CompIdSet;

    // keep track of the  object ids used
    //  in the AGP. Used to detect duplicates.
    typedef set<string> TObjSet;
    typedef pair<TObjSet::iterator, bool> TObjSetResult;
    TObjSet m_ObjIdSet;


    // proper values for the different fields in the AGP
    typedef set<string> TValuesSet;
    typedef pair< TValuesSet::iterator, bool> TValuesSetResult;
    TValuesSet m_ComponentTypeValues;
    TValuesSet m_GapTypeValues;
    TValuesSet m_OrientaionValues;
    TValuesSet m_LinkageValues;


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
    // Syntax validate methods
    //
    void x_ValidateSyntaxLine(const SDataLine& line,
      const string& text_line, bool last_validation = false);

    bool x_CheckValues(int line_num, const TValuesSet& values,
      const string& value, const string& field_name,
      bool log_error = true);
    int x_CheckRange(int line_num, int start, int begin, int end,
      string begin_name, string end_name);
    int x_CheckIntField(int line_num, const string& field,
      const string& field_name, bool log_error = true);
    bool x_IncremenalCheck(int line_num, int prev_num, int num,
      const string& field_name);

    //
    // Semantic validate methods
    //
    void x_ValidateSemanticInit();
    void x_ValidateSemanticLine(const SDataLine& line,
      const string& text_line);

    int x_GetTaxid(CBioseq_Handle& bioseq_handle);
    void x_AddToTaxidMap(int taxid, const SDataLine& dl);
    void x_CheckTaxid();
    void x_AgpTotals();
    int x_GetSpecies(int taxid);
    int x_GetTaxonSpecies(int taxid);
};



void CAgpValidateApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(
      GetArguments().GetProgramBasename(),
      "Validate AGP data", false);

    arg_desc->AddDefaultKey("type", "ValidationType",
      "Type of validation to be preformed",
      CArgDescriptions::eString,
      "syntax");
    CArgAllow_Strings* constraint_type = new CArgAllow_Strings;
    constraint_type->Allow("syntax");
    constraint_type->Allow("semantics");
    arg_desc->SetConstraint("type", constraint_type);

    arg_desc->AddDefaultKey("taxon", "TaxonCheckType",
      "Type of Taxonomy semanitic check to be preformed",
      CArgDescriptions::eString,
      "exact");
    CArgAllow_Strings* constraint_taxon = new CArgAllow_Strings;
    constraint_taxon->Allow("exact");
    constraint_taxon->Allow("species");
    arg_desc->SetConstraint("taxon", constraint_taxon);

    // file list for file processing
    arg_desc->AddExtra(0,100, "files to be processed",
                       CArgDescriptions::eInputFile);
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    // initialze values
    m_GapTypeValues.insert("fragment");
    m_GapTypeValues.insert("split_finished");
    m_GapTypeValues.insert("clone");
    m_GapTypeValues.insert("contig");
    m_GapTypeValues.insert("centromere");
    m_GapTypeValues.insert("short_arm");
    m_GapTypeValues.insert("heterochromatin");
    m_GapTypeValues.insert("telomere");
    m_GapTypeValues.insert("scaffold");

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

    m_ObjCount = 0;
    m_CompCount = 0;
    m_CompPosCount = 0;
    m_CompNegCount = 0;
    m_CompZeroCount = 0;
    m_GapCount = 0;

    m_TypeGapCount["fragmentyes"] = 0;
    m_TypeGapCount["fragmentno"] = 0;
    m_TypeGapCount["split_finishedyes"] = 0;
    m_TypeGapCount["split_finishedno"] = 0;
    m_TypeGapCount["cloneyes"] = 0;
    m_TypeGapCount["cloneno"] = 0;
    m_TypeGapCount["contigyes"] = 0;
    m_TypeGapCount["contigno"] = 0;
    m_TypeGapCount["centromereyes"] = 0;
    m_TypeGapCount["centromereno"] = 0;
    m_TypeGapCount["short_armyes"] = 0;
    m_TypeGapCount["short_armno"] = 0;
    m_TypeGapCount["heterochromatinyes"] = 0;
    m_TypeGapCount["heterochromatinno"] = 0;
    m_TypeGapCount["telomereyes"] = 0;
    m_TypeGapCount["telomereno"] = 0;


    m_TaxidComponentTotal = 0;

    m_ValidateMsg = NULL;
}



int CAgpValidateApplication::Run(void)
{
    // Setup registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Get arguments
    const CArgs& args = GetArgs();

    if (args["type"].AsString() == "syntax") {
        m_ValidationType = syntax;
    } else { // args_type == "semantic"
        x_ValidateSemanticInit();
        m_ValidationType = semantic;
    }

    m_SpeciesLevelTaxonCheck = false;
    if (args["taxon"].AsString() == "species") {
        m_SpeciesLevelTaxonCheck = true;
    }
        x_ValidateUsingFiles(args);
    if (m_ValidationType == syntax) {
        x_AgpTotals();
    } else {
        x_CheckTaxid();
    }
    return 0;
}

void CAgpValidateApplication::x_ValidateUsingDB(const CArgs& args)
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
    } else {
        for (unsigned int i = 1; i <= args.GetNExtra(); i++) {
            m_CurrentFileName =
                args['#' + NStr::IntToString(i)].AsString();
            CNcbiIstream& istr =
                args['#' + NStr::IntToString(i)].AsInputFile();
            if (!istr) {
                LOG_POST(Fatal << "Unable to open file : " <<
                         m_CurrentFileName);
                exit (0);
            }
            x_ValidateFile(istr);
        }
    }
}

void CAgpValidateApplication::x_ValidateFile(CNcbiIstream& istr)
{

    int line_num = 1;
    string  line;
    while (NcbiGetlineEOL(istr, line)) {
        if (line == "") continue;

        SDataLine data_line;
        vector<string> cols;
        NStr::Tokenize(line, "\t", cols);

        data_line.line_num        = line_num++;
        if (cols.size() > 0)
            data_line.object          = cols[0];
        if (cols.size() > 1)
            data_line.begin           = cols[1];
        if (cols.size() > 2)
            data_line.end             = cols[2];
        if (cols.size() > 3)
            data_line.part_num        = cols[3];
        if (cols.size() > 4)
            data_line.component_type  = cols[4];

        if (cols.size() > 5) {
            data_line.component_id = cols[5];
            data_line.gap_length = cols[5];
        }
        if (cols.size() > 6) {
            data_line.component_start = cols[6];
            data_line.gap_type = cols[6];
        }
        if (cols.size() > 7) {
            data_line.component_end = cols[7];
            data_line.linkage = cols[7];
        }

            data_line.orientation = "";
            if (data_line.component_type != "N") {  // Component

                if (cols.size() > 8)
                     data_line.orientation = cols[8];
            }

        if (m_ValidationType == syntax) {
            x_ValidateSyntaxLine(data_line, line);
        } else {
            x_ValidateSemanticLine(data_line, line);
        }
    }
}

void CAgpValidateApplication::x_AgpTotals()
{
  LOG_POST(
    "\nObjects: " << m_ObjCount << "\n" <<
    "Unique Component Accessions: " << m_UniqComp.size() <<"\n"<<
    "Lines with Components: " << m_CompCount        << "\n" <<
    "\tWith orientation of +: " << m_CompPosCount   << "\n" <<
    "\tWith orientation of -: " << m_CompNegCount   << "\n" <<
    "\tWith orientation of 0: " << m_CompZeroCount  << "\n\n" <<

    "Gaps with linkage, with no linkage: " << m_GapCount
 << "\n\tFragment       : "<<m_TypeGapCount["fragmentyes"       ]
 << "\t"                   <<m_TypeGapCount["fragmentno"        ]
 << "\n\tClone          : "<<m_TypeGapCount["cloneyes"          ]
 << "\t"                   <<m_TypeGapCount["cloneno"           ]
 << "\n\tContig         : "<<m_TypeGapCount["contigyes"         ]
 << "\t"                   <<m_TypeGapCount["contigno"          ]
 << "\n\tCentromere     : "<<m_TypeGapCount["centromereyes"     ]
 << "\t"                   <<m_TypeGapCount["centromereno"      ]
 << "\n\tShort_arm      : "<<m_TypeGapCount["short_armyes"      ]
 << "\t"                   <<m_TypeGapCount["short_armno"       ]
 << "\n\tHeterochromatin: "<<m_TypeGapCount["heterochromatinyes"]
 << "\t"                   <<m_TypeGapCount["heterochromatinno" ]
 << "\n\tTelomere       : "<<m_TypeGapCount["telomereyes"       ]
 << "\t"                   <<m_TypeGapCount["telomereno"        ]
 << "\n\tSplit_finished : "<<m_TypeGapCount["split_finishedyes" ]
 << "\t"                   <<m_TypeGapCount["split_finishedno"  ]
  );
}



bool CAgpValidateApplication::x_IncremenalCheck(
  int line_num, int prev_num, int num, const string& field_name)
{
    if (num != ++prev_num) {
        VALIDATION_ERROR("The " << field_name
          << " filed must increment sequentially");
        return false;
    }

    return true;
}

int CAgpValidateApplication::x_CheckIntField(
  int line_num, const string& field,
  const string& field_name, bool log_error)
{
    int field_value = 0;
    try {
        field_value = NStr::StringToInt(field);
    } catch (...) {
    }

    if (field_value <= 0  &&  log_error) {
        VALIDATION_ERROR("The " << field_name <<
          " field must be a positive integer");
    }
    return field_value;
}

int CAgpValidateApplication::x_CheckRange(
  int line_num, int start, int begin, int end,
  string begin_name, string end_name)
{
    int range = 0;
    if (begin <= start){
        VALIDATION_ERROR(begin_name <<
          " field overlaps previous line in the file");
    }
    else if (end < begin) {
        VALIDATION_ERROR(end_name <<
                               " field is less than " <<
                               end_name << " field");
    } else {
        range = end - begin + 1;
    }

    return range;
}

bool CAgpValidateApplication::x_CheckValues(
  int line_num,
  const TValuesSet& values,
  const string& value,
  const string& field_name,
  bool log_error)
{
    if (values.count(value) == 0) {
        if (log_error)
            VALIDATION_ERROR("Invalid value for " << field_name);
        return false;
    }
    return true;
}

void CAgpValidateApplication::x_ValidateSyntaxLine(
  const SDataLine& dl,
  const string& text_line,
  bool last_validation)
{
    static string   prev_object = "";
    static string   prev_component_type = "";
    static string   prev_gap_type = "";
    static string   prev_line;
    static string   prev_line_filename = "";
    static int      prev_end = 0;
    static int      prev_part_num = 0;
    static int      prev_line_num = 0;
    static bool     prev_line_error_occured = false;

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


    TIdMapResult id_insert_result;
    TObjSetResult obj_insert_result;

    START_LINE_VALIDATE_MSG(dl.line_num, text_line);

    if (dl.object != prev_object) {
        prev_end = 0;
        prev_part_num = 0;
        prev_object = dl.object;
        new_obj = true;
        m_ObjCount++;
    }

    if( new_obj && (prev_component_type == "N") &&
        (prev_gap_type == "fragment")
    ) {
        // new scafold. Previous line is a scaffold ending
        //   with a fragment gap

        // special error reporting mechanism since the
        //  error is on the previous line. Directly Log
        //  the error messages.
        if (!prev_line_error_occured) {
            LOG_POST("\n\n" << prev_line_filename << ", line " <<
                      prev_line_num << ":\n" << prev_line);
        }
        LOG_POST("\tWARNING: Next line is a new scaffold "
                 "(and new object). "
                 "Current line is a fragment gap. "
                 "A Scaffold should not end with a gap.");

        if (last_validation) {
            // Finished validating all lines.
            //  Just did a final gap ending check
            //  No more validating to perform
            return;
        }
    }


    if (new_obj) {
        obj_insert_result = m_ObjIdSet.insert(dl.object);
        if (obj_insert_result.second == false) {
          VALIDATION_ERROR("Duplicate object " << dl.object <<
                                       " found");
        }
    }

    if( (obj_begin = x_CheckIntField(
          dl.line_num, dl.begin, "object_begin"
        )) &&
        (obj_end = x_CheckIntField(
          dl.line_num, dl.end, "object_end"
        ))
    ) {

        if (new_obj && obj_begin != 1) {
          VALIDATION_ERROR(
            "A new object must have an object_begin "
            "field equal to 1");
        }

        obj_range_len = x_CheckRange(
          dl.line_num, prev_end, obj_begin, obj_end,
          "object_begin", "object_end");
        prev_end = obj_end;

    }

    if (part_num = x_CheckIntField(
      dl.line_num, dl.part_num, "part_num"
    )) {
        x_IncremenalCheck(dl.line_num, prev_part_num,
          part_num, "part_num");
        prev_part_num = part_num;
    }

    if (x_CheckValues(
      dl.line_num, m_ComponentTypeValues,
      dl.component_type,"component_type"
    )) {}


    if (dl.component_type == "N") { // gap
        m_GapCount++;
        error = false;
        if (gap_len = x_CheckIntField(dl.line_num, dl.gap_length,
                                      "gap_length")) {
            if (obj_range_len && obj_range_len != gap_len) {
                VALIDATION_ERROR("The object range length (" <<
                  obj_range_len <<
                  ") is not equal to the gap length (" <<
                  gap_len << ")");
                error = true;
            }
        } else {
            error = true;
        }

        if (x_CheckValues(dl.line_num, m_GapTypeValues,
                          dl.gap_type, "gap_type") &&
            x_CheckValues(dl.line_num, m_LinkageValues,
                          dl.linkage, "linakge")) {
            string key = dl.gap_type + dl.linkage;
            m_TypeGapCount[key]++;
        } else {
            error = true;
        }

        if (new_obj) {
            // New scafold. A Scaffold should not begin with a gap
            if (dl.gap_type == "fragment") {
                COLLECT_VALIDATE_WARNING(
                  "Current line is a new scaffold. A scaffold"
                  " should not begin with a fragment gap.");
            } else {
                COLLECT_VALIDATE_WARNING(
                  "Current line is a new object. "
                  "Object begins with a scaffold-ending gap.");
            }
        }

        if (prev_component_type == "N") {
            // Previous line a gap. Check the gap_type.

            if (prev_gap_type == "fragment") {
                // two gaps in a row

                if (!new_obj) {

                    if (dl.gap_type == "fragment") {
                        COLLECT_VALIDATE_WARNING(
                          "Previous line was a fragment gap. "
                          "Two fragment gap lines in a row.");
                    } else {
                        // Current gap type is a scaffold boundary

                        // special error reporting mechanism since
                        // the error is on the previous line.
                        // Directly Log the error messages.
                        if (!prev_line_error_occured) {
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
                    COLLECT_VALIDATE_WARNING(
                      "Previous line is a scaffold ending gap. "
                      "A Scaffold should not begin with a gap.");
                  }
                  else {
                    // Current gap type is a scaffold boundary.
                    //  Two scaffold gaps in a row.
                    COLLECT_VALIDATE_WARNING(
                      "Previous line is a scaffold ending gap. "
                      "Current line is another scaffold "
                      "ending gap.");
                  }
              }
            }
        }

        if (error) {
            // Check if the line should be a component type
            //  i.e., dl.component_type != "N"

            // A component line has an integer in
            //  column 7 (component start) and column 8 (c end)
            //  It also has a +, - or 0 in column 9 (orientation)
            if (
                x_CheckIntField(dl.line_num, dl.component_start,
                                "component_start", NO_LOG) &&
                x_CheckIntField(dl.line_num, dl.component_end,
                                  "component_end", NO_LOG) &&
                x_CheckValues(dl.line_num, m_OrientaionValues,
                          dl.orientation, "orientation", NO_LOG)
               ) {
                COLLECT_VALIDATE_WARNING(
                  "Line appears to be a component line and "
                  "not a gap line");
            }
        }
    } else { // component
        error = false;
        m_CompCount++;
        m_UniqComp.insert(dl.component_id);

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
            VALIDATION_ERROR(
              "The object range length is not "
              "equal to the component length");
            error = true;
          }
        } else {
          error = true;
        }

        if (x_CheckValues(
          dl.line_num, m_OrientaionValues, dl.orientation,
          "orientation"
        )) {
            if ( dl.orientation == "0") {
                VALIDATION_ERROR(
                  "A component can not have an "
                  "unknown orientation.");
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
        TIdMapKeyValuePair value_pair(
          dl.component_id, TIdMapValue(component_range)
        );
        id_insert_result = m_CompIdSet.insert(value_pair);
        if (id_insert_result.second == false) {
          TIdMapValue& collection =
            (id_insert_result.first)->second;

          string post_message = "Duplicate component id found.";

          if (collection.IntersectingWith(component_range)) {
            post_message += " The span overlaps a previous span "
                            "for this component.";
          } else if (
            comp_start < collection.GetToOpen() &&
            dl.orientation!="-"
          ) {
            post_message+= " The component span is out of order.";
          }
          COLLECT_VALIDATE_WARNING(post_message);

          collection.CombineWith(component_range);
        }

        if (error) {
            // Check if the line sholud be a gap type
            //  i.e., dl.component_type == "N"

            // A gap line has integer in column 6 (gap length),
            //  a gap type value in column 7,
            //  and a yes or no in column 8.
            // vsap todo: FIX THIS ERROR
            if( gap_len = x_CheckIntField(
                  dl.line_num, dl.gap_length, "gap_length", NO_LOG
                ) && x_CheckValues(
                  dl.line_num, m_GapTypeValues,
                  dl.gap_type, "gap_type", NO_LOG
                ) && x_CheckValues(
                  dl.line_num, m_LinkageValues,
                  dl.linkage, "linakge", NO_LOG
                )
            ) {
              COLLECT_VALIDATE_WARNING(
                "Line appears to be a gap line and "
                "not a component line");
            }
        }
    }
    if (m_LineErrorOccured) {
        END_LINE_VALIDATE_MSG;
    }

    prev_component_type = dl.component_type;
    prev_gap_type = dl.gap_type;
    prev_line = text_line;
    prev_line_num = dl.line_num;
    prev_line_filename = m_CurrentFileName;
    prev_line_error_occured = m_LineErrorOccured;
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
  if (dl.component_type == "N") { // gap
      return;
  }

  START_LINE_VALIDATE_MSG(dl.line_num, text_line);

  // Create Seq-id, set it to the GI specified on the cmdline
  CSeq_id seq_id;
  try {
      seq_id.Set(dl.component_id);
  }
  //    catch (CSeqIdException::eFormat)
  catch (...) {
        VALIDATION_ERROR(
                            "Component " << dl.component_id <<
                            " has an invalid component id");
  }

  // Get Bioseq handle for the Seq-id.
  // * Most of requests will use this handle.
  CBioseq_Handle bioseq_handle=m_Scope->GetBioseqHandle(seq_id);

  // Accession not found check
  if ( !bioseq_handle ) {
        VALIDATION_ERROR(
                            "Component " << dl.component_id <<
                            " not found in Genbank");
      return;
  }

  // Component out of bounds check
  CBioseq_Handle::TInst_Length seq_len =
    bioseq_handle.GetInst_Length();
  if (NStr::StringToUInt(dl.component_end) > seq_len) {
      VALIDATION_ERROR(
        "Component end " << dl.component_end <<
        " is out of bounds for " << dl.component_id <<
        " that has a length of " << seq_len);
  }

  int taxid = x_GetTaxid(bioseq_handle);
  x_AddToTaxidMap(taxid, dl);
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
                    docsums->GetList()) {
                docsum_taxid = (*it)->GetValue("TaxId");

                if (docsum_taxid != "") {
                    taxid = NStr::StringToInt(docsum_taxid);
                    break;
                }
            }


        }
        catch(...) {
          VALIDATION_ERROR("Unable to get Entrez Docsum Info");
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

    for (int id = taxon.GetParent(taxid);
         is_species == true && id > 1;
         id = taxon.GetParent(id)) {

        CConstRef<COrg_ref> org_ref = taxon.GetOrgRef(
          id, is_species, is_uncultured, blast_name
        );
        if (org_ref == null) {
            VALIDATION_ERROR(
              "Taxon returned a null Org_ref for taxid " << id);
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
      LOG_POST("Unable to determine a Taxid for the AGP");
      return;
  }

  LOG_POST(Error<< "The AGP's taxid is: " << agp_taxid);
  LOG_POST(Error<<
    "The following components have incorrect Taxids");

  // report components that have an incorrect taxid
  ITERATE(TTaxidMap, map_it, m_TaxidMap) {
      if (map_it->first == agp_taxid) continue;

      int taxid = map_it->first;
      ITERATE(TAgpInfoList, list_it, map_it->second) {
          LOG_POST(Error << "\t" <<
            list_it->filename << ", " <<
            list_it->line_num << ": " <<
            list_it->component_id << " - Taxid " <<
            taxid);
      }
  }
}

void CAgpValidateApplication::Exit(void)
{
    SetDiagStream(0);
}


int main(int argc, const char* argv[])
{
    return CAgpValidateApplication().AppMain(
      argc, argv, 0, eDS_Default, 0
    );
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/08/24 22:25:24  sapojnik
 * long lines reformatted for easier printing
 *
 * Revision 1.3  2006/08/11 16:46:05  sapojnik
 * Print Unique Component Accessions, do not complain about the order of "-" spans
 *
 * Revision 1.2  2006/04/06 20:35:51  ucko
 * Replace "and" with "&&" for compatibility with GCC 2.95.
 *
 * Revision 1.1  2006/03/29 19:51:12  friedman
 * Initial version
 *
 * ===========================================================================
 */
