/*
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
* Author:
*
* File Description:
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_system.hpp>
#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <misc/xmlwrapp/event_parser.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <cmath>

#ifndef __process_eutils__hpp__
#define __process_eutils__hpp__



class CEUtilsGeneric : public xml::event_parser
{
public:
    CEUtilsGeneric()
    {
    }

    bool TryQuery (string query)
    {
        if (m_Host.empty() || m_Url.empty()) return false;
        for (int attempt = 0;  attempt < 5;  ++attempt) {
            try {
                CConn_HttpStream istr(m_Host, m_Url, query);

                /*
                NcbiStreamCopy(cout, istr);
                return true;
                */

                xml::error_messages msgs;
                parse_stream(istr, &msgs);

                if (msgs.has_errors()  ||  msgs.has_fatal_errors()) {
                    ERR_POST(Warning << "error parsing xml: " << msgs.print());
                    return false;
                }

                if (Succeeded ()) return true;
                return false;
            }
            catch (CException& e) {
                ERR_POST(Warning << "failed on attempt " << attempt + 1
                         << ": " << e);
            }

            int sleep_secs = ::sqrt((double)attempt);
            if (sleep_secs) {
                SleepSec(sleep_secs);
            }
        }
        return false;
    }

protected:
    virtual bool StartElement (const std::string &name)
    {
        return true;
    }
    virtual bool EndElement (const std::string &name)
    {
        return true;
    }
    virtual bool Text (const std::string &contents)
    {
        return true;
    }
    virtual bool Succeeded ()
    {
        return true;
    }

    bool PathSuffixIs (const char* suffix)
    {
        string::size_type pos = m_Path.rfind(suffix);
        return (pos != string::npos  &&  pos == m_Path.size() - strlen(suffix));
    }

protected:
    bool error(const string& message)
    {
        LOG_POST(Error << "parse error: " << message);
        return false;
    }

    bool warning(const string& message)
    {
        LOG_POST(Warning << "parse warning: " << message);
        return false;
    }

    bool start_element(const string& name, const attrs_type& attrs)
    {
        if ( !m_Path.empty() ) {
            m_Path += "/";
        }
        m_Path += name;
        if (! StartElement(name)) {
            return false;
        }
        return true;
    }

    bool end_element(const string& name)
    {
        bool bail = false;
        if (! EndElement(name)) {
            bail = true;
        }
        string::size_type pos = m_Path.find_last_of("/");
        if (pos != string::npos) {
            m_Path.erase(pos);
        }
        if (bail) {
          return false;
        }
        return true;
    }

    bool text(const string& contents)
    {
        if (contents.empty()) {
            return true;
        }

        bool empty_text = true;
        int i;
        for (i = 0; i < contents.length(); i++) {
          if (contents [i] != ' ' &&
              contents [i] != '\n' &&
              contents [i] != '\r' &&
              contents [i] != '\t') {
              empty_text = false;
          }
        }
        if (empty_text) {
            return true;
        }

        if (! Text(contents)) {
            return false;
        }

        return true;
    }
protected:
    string m_Host;
    string m_Url;
    string m_Path;
};


class CESearchGeneric : public CEUtilsGeneric
{
public:
    CESearchGeneric(vector<int>& uids)
        : m_Uids(uids)
    {
        m_Host = "eutils.ncbi.nlm.nih.gov";
        m_Url = "/entrez/eutils/esearch.fcgi";
    }

protected:
    bool Text (const string& contents)
    {
        if (PathSuffixIs("/IdList/Id")) {
            m_Uids.push_back(NStr::StringToInt(contents));
        }
        return true;
    }

    bool Succeeded ()
    {
        if (m_Uids.size() > 0) return true;
        return false;
    }

private:
    vector<int>& m_Uids;
};

class CESummaryGeneric : public CEUtilsGeneric
{
public:
    CESummaryGeneric(vector<string>& strs)
        : m_Strs(strs)
     {
        m_Host = "eutils.ncbi.nlm.nih.gov";
        m_Url = "/entrez/eutils/esummary.fcgi";
    }

protected:
    bool StartElement (const string &name)
    {
        if (PathSuffixIs("/DocumentSummary")) {
            m_Iso.clear();
            m_Title.clear();
            m_Issn.clear();
        }
        return true;
    }

    bool EndElement (const string &name)
    {
        if (! PathSuffixIs("/DocumentSummary")) return true;

        if (m_Iso.empty()) return true;

        if (! m_Title.empty() && ! m_Issn.empty()) {
            m_Strs.push_back(m_Iso + "||(" + m_Title + ":" + m_Issn + ")");
        } else if (! m_Title.empty()) {
            m_Strs.push_back(m_Iso + "||(" + m_Title + ")");
        } else if (! m_Issn.empty()) {
            m_Strs.push_back(m_Iso + "||(" + m_Issn + ")");
        } else {
            m_Strs.push_back(m_Iso);
        }

        return true;
    }

    bool Text (const string& contents)
    {
        if (PathSuffixIs("/DocumentSummary/ISOAbbreviation")) {
            if (m_Iso.empty()) {
                m_Iso = contents;
            }
        }
        if (PathSuffixIs("/TitleMain/Title")) {
            if (m_Title.empty()) {
                m_Title = contents;
            }
        }
        if (PathSuffixIs("/ISSNInfo/issn")) {
            if (m_Issn.empty()) {
                m_Issn = contents;
            }
        }
        return true;
    }

    bool Succeeded ()
    {
        if (m_Strs.size() > 0) return true;
        return false;
    }

private:
    string m_Iso;
    string m_Title;
    string m_Issn;

private:
    vector<string>& m_Strs;
};

static unsigned char _ToKey[256] = {
    0x00, 0x01, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,

    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x1F,
  /*  sp     !     "     #     $     %     &     ' */
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x27,
  /*   (     )     *     +     ,     -     .     / */
    0x20, 0x20, 0x20, 0x20, 0x2C, 0x20, 0x20, 0x2F,
  /*   0     1     2     3     4     5     6     7 */
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  /*   8     9     :     ;     <     =     >     ? */
    0x38, 0x39, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  /*   @     A     B     C     D     E     F     G */
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  /*   H     I     J     K     L     M     N     O */
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  /*   P     Q     R     S     T     U     V     W */
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  /*   X     Y     Z     [     \     ]     ^     _ */
    0x78, 0x79, 0x7A, 0x20, 0x20, 0x20, 0x20, 0x20,
  /*   `     a     b     c     d     e     f     g */
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
  /*   h     i     j     k     l     m     n     o */
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
  /*   p     q     r     s     t     u     v     w */
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  /*   x     y     z     {     |     }     ~   DEL */
    0x78, 0x79, 0x7A, 0x20, 0x20, 0x20, 0x20, 0x20,

    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,

    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,

    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,

    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,

    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,

    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,

    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,

    0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,

    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,

    0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,

    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,

    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,

    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,

    0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,

    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,

    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

class CEJournalSearch
{
public:
    CEJournalSearch()
    {
    }

protected:
    bool DoOneESearch (string db, string term, string field, vector<int>& uids)

    {
        string query;

        query += "db=" + NStr::URLEncode(db);
        query += "&term=" + NStr::URLEncode(term);
        query += NStr::URLEncode(field);
        query += "&retmode=xml&retmax=200";

        uids.clear();

        CESearchGeneric parser(uids);
        return parser.TryQuery (query);
    }

    bool DoOneESummary (string db, vector<int>& uids, vector<string>& strs)

    {
        string query;

        query += "db=" + NStr::URLEncode(db);
        query += "&retmax=200&version=2.0&id=";
        for (int i = 0; i < uids.size(); i++) {
            if (i > 0) {
                query += ",";
            }
            query += NStr::IntToString(uids [i]);
        }

        strs.clear();

        CESummaryGeneric parser(strs);
        return parser.TryQuery (query);
    }

    bool LooksLikeISSN (string str)

    {
        if (str.length() != 9) return false;

        char ch = str [4];
        if (ch != '-' && ch != ' ' && ch != '+') return false;

        for (int i = 0; i < 9; i++) {
            ch = str [i];
            if (ch >= '0' && ch <= '9') continue;
            if (i == 4) {
                if (ch == '-' || ch == '+' || ch == ' ') continue;
            }
            if (i == 8) {
                if (ch == 'X' || ch == 'x') continue;
            }
            return false;
        }

        return true;
    }

public:
    bool DoJournalSearch (string journal, vector<int>& uids)

    {
        for (int i = 0; i < journal.size(); i++) {
            journal [i] = _ToKey [(int) (unsigned char) journal [i]];
        }

        if (LooksLikeISSN (journal)) {
            if (journal [4] == '+' || journal [4] == ' ') {
                journal [4] = '-';
            }
            if (journal [8] == 'x') {
                journal [8] = 'X';
            }
            if (DoOneESearch ("nlmcatalog", journal, "[issn]", uids)) {
                return true;
            }
        }

        if (DoOneESearch ("nlmcatalog", journal, "[multi] AND ncbijournals[sb]", uids)) {
            return true;
        }

        if (DoOneESearch ("nlmcatalog", journal, "[jour]", uids)) {
            return true;
        }

        return false;
    }

    bool DoJournalSummary (vector<int>& uids, vector<string>& strs)

    {
        if (DoOneESummary ("nlmcatalog", uids, strs)) {
            return true;
        }

        return false;
    }
};

//  ============================================================================
class CEUtilsProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CEUtilsProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out (0)
    {};

    //  ------------------------------------------------------------------------
    ~CEUtilsProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::ProcessInitialize( args );

        m_out = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
        m_journal = args["journal"].AsString();
    };

    //  ------------------------------------------------------------------------
    void ProcessFinalize()
    //  ------------------------------------------------------------------------
    {
    }

    //  ------------------------------------------------------------------------
    virtual void SeqEntryInitialize(
        CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::SeqEntryInitialize( se );
    };

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        vector<int> uids;
        vector<string> strs;
        CEJournalSearch searcher;
        if (searcher.DoJournalSearch (m_journal, uids)) {
            if (searcher.DoJournalSummary (uids, strs)) {
                // cout << "Success" << NcbiEndl;
                // cout << "Vector of " + NStr::IntToString(strs.size()) + " strings" << NcbiEndl;
                for (int i = 0; i < strs.size(); i++) {
                    cout << strs [i] << NcbiEndl;
                }
            }
        }
        // cout << "Vector of " + NStr::IntToString(uids.size()) + " elements" << NcbiEndl;

    };

protected:
    CNcbiOstream* m_out;
    string m_journal;
};

#endif
