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
 * Author:  Victor Sapojnikov
 *
 * File Description:
 *   Usage example for CAgpReader (CAgpErr, CAgpRow).
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/readers/agp_util.hpp>

USING_NCBI_SCOPE;

// Count objects, scaffolds, components, gaps
class CTestAgpReader : public CAgpReader
{
public:
  int objects1, objects2, scaffolds, components, gaps, comments;
  CTestAgpReader()
  {
    objects1=objects2=scaffolds=components=gaps=comments=0;
  }

#define P(x) cout << #x << "=" << x << "\n"
#define PPP(x, y, z) P(x); P(y); P(z)
  void PrintResults()
  {
    PPP(objects1, objects2, scaffolds);
    cout << "\n";
    PPP(components, gaps, comments);

  }

  // Callbacks
  virtual void OnGapOrComponent()
  {
    if(this_row->is_gap) gaps++;
    else components++;
  }

  virtual void OnScaffoldEnd()
  {
    scaffolds++;
  }

  virtual void OnObjectChange()
  {
    // If CAgpReader works properly, both counts are the same.
    if(!at_end) objects1++;
    if(!at_beg) objects2++;
  }

  virtual void OnComment()
  {
    comments++;
  }


};

int main(int argc, char* argv[])
{
  CTestAgpReader reader;
  int code=reader.ReadStream(cin);
  if(code) {
    // Print line(s) on which the error occured
    if(CAgpRow::agpErr->messages_apply_to & CAgpErr::AT_PrevLine) {
      cerr <<  reader.prev_line_num << ": " <<reader.prev_row->ToString() << "\n";
    }
    if(CAgpRow::agpErr->messages_apply_to & CAgpErr::AT_ThisLine) {
      cerr <<  reader.line_num << ": " <<reader.line << "\n";
    }
    cerr << CAgpRow::GetErrors();
  }
  else {
    reader.PrintResults();
  }
  return 0;
}
