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
 *     Usage example for CAgpValidateReader and CAgpErrEx.
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/readers/agp_validate_reader.hpp>

USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE
// Expose protected m_line, m_line_num via getter methods.
class CAgpValTestReader: public CAgpValidateReader
{
public:
  const string& GetLine() {return m_line;}
  int GetLineNum() {return m_line_num;}

  CAgpValTestReader(CAgpErrEx& agpErr, CMapCompLen& comp2len) : CAgpValidateReader(agpErr, comp2len) {}
};

class CAgpErrXml : public CAgpErrEx
{
public:
    // This pointer gives an easy way to retrieve the current line info (m_line, m_line_num)
    // Another way would be to keep error messages in memory and print them only on LineDone().
    CAgpValTestReader* m_reader;

    CAgpErrXml(CNcbiOstream* out) :  CAgpErrEx(out) {}

    void PrintAppliesTo(const string& filename, int line_num, const string& line_txt)
    {
        *m_out << "    <filename>" << NStr::XmlEncode(filename) << "</filename>\n";
        *m_out << "    <line_num>" << line_num                  << "</line_num>\n";
        *m_out << "    <line_txt>" << NStr::XmlEncode(line_txt) << "</line_txt>\n";
    }

    // override Msg() that comes from CAgpErr
    virtual void Msg(int code, const string& details, int appliesTo=fAtThisLine)
    {
        *m_out<< "  <message severity=\"" << (
            (code>=W_First && code<W_Last) ? "WARNING" : "ERROR"
            ) << "\"";
        if(code <=E_LastToSkipLine) *m_out << " line_skipped=\"1\"";
        *m_out<<">\n";

        if(appliesTo & CAgpErr::fAtPpLine) {
            PrintAppliesTo( m_filenum_pp>=0 ? m_InputFiles[m_filenum_pp] : NcbiEmptyString, m_line_num_pp, m_line_pp);
        }
        if(appliesTo & CAgpErr::fAtPrevLine) {
            PrintAppliesTo( m_filenum_prev>=0 ? m_InputFiles[m_filenum_prev] : NcbiEmptyString, m_line_num_prev, m_line_prev);
        }
        if(appliesTo & CAgpErr::fAtThisLine) {
            PrintAppliesTo( m_filename, m_reader->GetLineNum(), m_reader->GetLine());
        }

        *m_out << "    <code>" << GetPrintableCode(code) << "</code>\n";
        *m_out << "    <text>" << NStr::XmlEncode( FormatMessage( GetMsg(code), details ) ) << "</text>\n";

        *m_out << "  </message>\n";

    }
    virtual void Msg(int code, int appliesTo=fAtThisLine)
    {
        Msg(code, NcbiEmptyString, appliesTo);
    }


};
END_NCBI_SCOPE

int main(int argc, char* argv[])
{
    EAgpVersion agp_version = eAgpVersion_auto;
    int pos=1;
    if( pos<argc && "-v2" == string(argv[pos]).substr(0, 3) ) {
        agp_version = eAgpVersion_2_0;
        pos++;
    }
    else if( pos<argc && "-v1" == string(argv[pos]).substr(0, 3) ) {
        agp_version = eAgpVersion_1_1;
        pos++;
    }
    else if( pos+1<argc && "-v" == string(argv[pos]) ) {
        if     (argv[pos+1][0]=='2') agp_version = eAgpVersion_2_0;
        else if(argv[pos+1][0]=='1') agp_version = eAgpVersion_1_1;
        else {
          cerr << "Error - invalid version after -v.\n";
          exit(1);
        }
        pos+=2;
    }

    CAgpErrXml error_handler(&cout);
    CMapCompLen comp2len;
    CAgpValTestReader reader( error_handler, comp2len );
    reader.SetVersion(agp_version);
    error_handler.m_reader = &reader; // needed to call GetLine(), GetLineNum()

    cout << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
    cout << "<result>\n";

    int code=0;
    if(pos==argc) {
      code=reader.ReadStream(cin);
    }
    else {
        for( ; pos < argc; ++pos ) {
            CNcbiIfstream f(argv[pos]);
            if(!f) {
                cerr << "Error - cannot read " << argv[pos] << "\n";
                exit(1);
            }

            error_handler.StartFile(argv[pos]);
            code |= reader.ReadStream(f);
        }
    }


    cout << "</result>\n";
}


