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
 * Author:  Victor Sapojnikov; usage wording: Paul Kitts.
 *
 * File Description:
 *     Repair an AGP file, if possible.
 *     Includes a custom error handler,
 *     processing of the input stream in chunks.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/readers/agp_util.hpp>

USING_NCBI_SCOPE;

const char* usage=
"Clean up an AGP file:\n"
"http://www.ncbi.nlm.nih.gov/genome/assembly/agp/AGP_Specification.shtml\n"
//"http://www.ncbi.nlm.nih.gov/genome/guide/Assembly/AGP_Specification.html\n"
"\n"
"USAGE: agp_renumber <in.agp >out.agp\n"
"\n"
" - Recalculate the object begin and end coordinates from\n"
"   the length of the component span or gap length.\n"
" - Renumber the part numbers for each object.\n"
" - Lowercase gap type and linkage.\n"
" - Reorder linkage evidence terms: paired-ends;align_genus;align_xgenus;align_trnscpt;within_clone;clone_contig;map;strobe\n"
" - Reformat white space to conform to the AGP format specification:\n"
"   - add missing tabs at the ends of gap lines;\n"
"   - drop blank lines;\n"
"   - remove extra tabs and spaces at the end of lines;\n"
"   - add a missing line separator at the end of the file;\n"
"   - replace spaces with tabs (except in comments).\n";

const int MAX_BUF_LINES=100;
//int line_num_in[MAX_BUF_LINES]; // filled in ProcessStream(), read in CAgpRenumber and CCustomErrorHandler

class CCustomErrorHandler : public CAgpErr
{
public:
  static bool MustRenumber(int code)
  {
    return
      code==E_ObjRangeNeGap       ||
      code==E_ObjRangeNeComp      ||
      code==E_ObjMustBegin1       ||
      code==E_PartNumberNot1      ||
      code==E_PartNumberNotPlus1  ||
      code==E_ObjBegNePrevEndPlus1;
  }

  bool had_missing_tab;
  CCustomErrorHandler()
  {
    had_missing_tab=false;
  }

  virtual void Msg(int code, const string& details, int appliesTo=fAtThisLine)
  {
    if( MustRenumber(code) ) return;
    if( code==W_GapLineMissingCol9 ) had_missing_tab=true;
    CAgpErr::Msg(code, details, appliesTo);
  }
  // copied from CAgpErr - only to get rid of the pointless compiler warning
  virtual void Msg(int code, int appliesTo=fAtThisLine)
  {
    Msg(code, NcbiEmptyString, appliesTo);
  }
};

class CAgpRenumber : public CAgpReader
{
protected:
  int m_part_num, m_object_beg;
  ostream& m_out;
  int m_line_num_out;
  set<string> m_obj_names;

  // Callbacks
  //virtual void OnScaffoldEnd();
  virtual void OnObjectChange()
  {
    m_part_num=m_object_beg=1;
    if(!m_at_beg) {
      (renum_current_obj?renum_objs:no_renum_objs)++;
      renum_current_obj=false;
    }
    if(!m_at_end) {
      if(! m_obj_names.insert(m_this_row->GetObject()).second ) {
        GetErrorHandler()->Msg( m_error_code=CAgpErr::E_DuplicateObj, m_this_row->GetObject() );
      }
    }
  }

  virtual void OnGapOrComponent()
  {
    m_adjusted =
      m_this_row->GetObject() + "\t" +
      NStr::IntToString(m_object_beg) + "\t";
    m_object_beg += m_this_row->IsGap() ?
      m_this_row->gap_length :
      m_this_row->component_end - m_this_row->component_beg + 1;

    m_adjusted+=
      NStr::IntToString(m_object_beg-1) + "\t" +
      NStr::IntToString(m_part_num);
    m_part_num++;

    // cannot simply append the tail of m_line - it might have a missing tab...
    string s = m_this_row->ToString(true);
    string s_orig_linkage_evidence  = m_this_row->ToString(false);
    if(s!=s_orig_linkage_evidence) reordered_ln_ev++;
    m_adjusted+= s.substr(
      s.find( "\t", 1+
      s.find( "\t", 1+
      s.find( "\t", 1+
      s.find( "\t" ))))
    );

    if(m_this_row->pcomment!=NPOS)
      m_adjusted+= m_line.substr(m_this_row->pcomment);

    m_out << m_adjusted << "\n";
    m_line_num_out++;
  }

  virtual void OnComment()
  {
    m_out << m_line << "\n";
    m_line_num_out++;
  }

  virtual bool OnError()
  {
    switch(m_error_code) {
      case CAgpErr::E_EmptyLine:
        had_empty_line=true;
        m_prev_line_skipped=false; // do not skip checks on the next line
        break;

      case CAgpErr::E_ObjRangeNeGap:
      case CAgpErr::E_ObjRangeNeComp:
        // Use m_line_skipped to prevent endless recursion
        if(m_line_skipped) {
          m_line_skipped=false;
          //m_error_code=0; -- another possible way to prevent endless recursion
          if( !ProcessThisRow() ) return false;
          renum_current_obj=true;
          break;
        }
        // else: print diags

      //case CAgpErr::E_ObjEndLtBeg: -- component/gap-specific columns were not parsed...
      // die (could also: warn, set End=Beg, retry/resume parsing?)
      default:
        if( CCustomErrorHandler::MustRenumber(m_error_code) ) {
          if( m_error_code==CAgpErr::E_ObjRangeNeGap ||
              m_error_code==CAgpErr::E_ObjRangeNeComp )
            cerr <<"Corrected: " << CAgpErr::GetMsg(m_error_code) << "\n";
          cerr << "- " << m_line << "\n+ " << m_adjusted << "\n";
          cerr << "\n";

          renum_current_obj=true;
        }
        else return false; // die
    }
    return true;
  }

public:
  bool had_empty_line;
  bool renum_current_obj;
  int renum_objs, no_renum_objs, reordered_ln_ev;
  string m_adjusted;

  CCustomErrorHandler custom_err;
  CAgpRenumber(ostream& out) : CAgpReader(&custom_err, false, eAgpVersion_auto), m_out(out)
  {
    had_empty_line = false;
    renum_objs=no_renum_objs=0;
    m_line_num_out = 0;
    renum_current_obj=false;
    reordered_ln_ev = 0;
  }

};

int ProcessStream(istream &in, ostream& out)
{
  CAgpRenumber renum(out);

  string s;
  CNcbiOstrstream* buf=new CNcbiOstrstream();
  int buf_lines=0;
  int code=0;

  // for reporting
  bool had_space    =false;
  bool had_extra_tab=false;
  bool no_eol_at_eof=false;
  bool bad_case_gap =false;

  while( NcbiGetline(in, s, "\r\n") ) {
    // get rid of spaces except in or in front of EOL #comments
    char prev_ch=0;
    int tab_count=0;
    bool at_beg=true;

    char component_type=0;
    for(SIZE_TYPE i=0; i<s.size(); i++) {
      char ch=s[i];
      switch(ch) {
        case ' ':
          if(at_beg) continue;
          had_space=true;
          ch='\t';
        case '\t':
          if(prev_ch!='\t') {
            tab_count++;
            *buf<<'\t';
            if(tab_count>8) {
              if( tab_count==9 && i<s.size()-1 && s[i+1]=='#' ) {
                // don't bark at the tab we keep (for aesthetic reasons)
                // in front of EOL comment in component lines
              }
              else if(!had_space){
                had_extra_tab=true;
              }
            }
          }
          else if(!had_space){
             // not necessarily a complete diags, but at least true
            had_extra_tab=true;
          }

          break;
        case '#':
          *buf << s.substr(i);
          goto EndFor;
        default:
          // 2010/09/14 lowercase gap type and linkage
          if(prev_ch=='\t' && tab_count==4) {
            component_type=ch;
          }
          if( (component_type=='N' || component_type=='U') &&
              (tab_count==6 || tab_count==7) && tolower(ch)!=ch
          ) {
            ch=tolower(ch); bad_case_gap=true;
          }

          if(tab_count>8) {
            // A fatal error - let CAgpRow catch it and complain
            *buf << '\t' << s.substr(i);
            goto EndFor;
          }
          at_beg=false;
          *buf << ch;
      }
      prev_ch=ch;
    }
    EndFor:

    *buf << '\n';
    if(++buf_lines>=MAX_BUF_LINES) {
      buf_lines=0;

      CNcbiIstrstream is(static_cast<const char*>(buf->str()), buf->pcount());
      code=renum.ReadStream(is, false);
      if(code) break;

      delete buf;
      buf=new CNcbiOstrstream();
    }

    if(in.eof()) no_eol_at_eof=true;
  }

  if(buf_lines) {
    CNcbiIstrstream is(static_cast<const char*>(buf->str()), buf->pcount());
    code=renum.ReadStream(is, false);
  }

  if(!code) code=renum.Finalize();
  if( code) {
    cerr << renum.GetErrorMessage()<<"\nRenumbering not completed because of errors.\n";
    return 1;
  }

  if(had_space           ) cerr << "Spaces converted to tabs.\n";
  if(had_extra_tab       ) cerr << "Extra tabs removed.\n";
  if(renum.had_empty_line) cerr << "Empty line(s) removed.\n";
  if(renum.custom_err.had_missing_tab) cerr << "Missing tabs added at the ends of gap lines.\n";
  if(no_eol_at_eof       ) cerr << "Line break added at the end of file.\n";
  if(bad_case_gap        ) cerr << "Gap type/linkage converted to lower case.\n";
  if(renum.reordered_ln_ev) cerr << "Linkage evidence terms reordered.\n";
  if(renum.renum_objs    ) cerr << renum.renum_objs << " object(s) renumbered.\n";
  if(renum.no_renum_objs ) {
    if(renum.renum_objs)
      cerr << renum.no_renum_objs << " object(s) did not need renumbering.\n";
    else
      cerr << "All lines have proper object_beg, object_end, part_number.\n";
  }

  delete buf;
  return 0;
}

// to do:
// - prints these warnings in handler: W_GapObjBegin, W_GapObjEnd, W_ConseqGaps

int main(int argc, char* argv[])
{
  if(argc==1) {
    return ProcessStream(cin, cout);
  }
  else if(argv[1][0]=='-' || argc > 1+1) {
    cout << usage;
    return 1;
  }
  else {
    CNcbiIfstream in(argv[1]);
    if( !in.good() ) {
      cerr << "Error - cannot open for reading: " << argv[1] << "\n";
      return 1;
    }
    return ProcessStream(in, cout);
  }
}
