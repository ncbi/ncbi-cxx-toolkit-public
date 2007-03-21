#ifndef AGP_VALIDATE_LineValidator
#define AGP_VALIDATE_LineValidator

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
 *      Validation tests that can be preformed on one line in the AGP file.
 *      If any of these fails, the rest of the tests (which may use multiple lines
 *      or GenBank info) are skipped.
 *
 */

#include <corelib/ncbistd.hpp>
//#include <corelib/ncbimisc.hpp>
//#include <corelib/ncbienv.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>

#include <iostream>
#include <map>
#include "AgpErr.hpp"

#define NO_LOG false

BEGIN_NCBI_SCOPE

extern CAgpErr agpErr;

struct SDataLine {
  int     line_num;
  string  object;
  string  begin;
  string  end;
  string  part_num;
  string  component_type;
  string  component_id;
  string  component_beg;
  string  component_end;
  string  orientation;
  string  gap_length;
  string  gap_type;
  string  linkage;
};

typedef map<string, int> TValuesMap;

// Values for Validatable component columns (7a-9a)
// plus file number and line number.
// Use init() for simple syntax validation.
class CCompVal
{
public:
  int beg, end, ori, file_num, line_num;

  enum {
    ORI_plus,
    ORI_minus,
    ORI_zero,
    ORI_na,

    ORI_count
  };
  static TValuesMap validOri;

  CCompVal();

  bool init(const SDataLine& dl, bool log_errors = true);
  int getLen() const { return end - beg + 1; }

  string ToString() const;
};

class CGapVal
{
public:
  int len, type, linkage;

  enum {
    GAP_clone          ,
    GAP_fragment       ,
    GAP_repeat         ,

    GAP_contig         ,
    GAP_centromere     ,
    GAP_short_arm      ,
    GAP_heterochromatin,
    GAP_telomere       ,

    //GAP_scaffold       ,
    //GAP_split_finished ,

    GAP_count,
    GAP_yes_count=GAP_repeat+1
  };

  typedef const char* TStr;
  static const TStr typeIntToStr[GAP_count+GAP_yes_count];

  // Initialized on the first invokation of the constructor from typeIntToStr[]
  static TValuesMap typeStrToInt;

  enum {
    LINKAGE_no ,
    LINKAGE_yes ,
    LINKAGE_count
  };
  // Initialized on the first invokation of the constructor
  static TValuesMap validLinkage;

  CGapVal();

  bool init(const SDataLine& dl, bool log_errors = true);
  int getLen() const { return len; }
  bool endsScaffold() const;
  bool validAtObjBegin() const;
};

int x_CheckIntField(const string& field,
  const string& field_name, bool log_error = true);

// Returns an integer constant mapped to the allowed text value,
// -1 if the value is unknowm
int x_CheckValues(const TValuesMap& values,
  const string& value, const string& field_name,
  bool log_error = true);

class CAgpLine
{
public:
  CAgpLine();
  bool init(const SDataLine& dl);
  static bool IsGapType(const string& type);
  //static bool IsComponentType(const string& type);

  int obj_begin;
  int obj_end;
  int part_num;
  int comp_type;
  bool is_gap;

  CCompVal compSpan;
  CGapVal gap;

  // Initialized on the first invokation of the constructor
  static TValuesMap componentTypeValues;
};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_LineValidator */

