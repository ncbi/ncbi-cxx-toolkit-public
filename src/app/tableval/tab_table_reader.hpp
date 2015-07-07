#ifndef STRUCT_CMT_READER_HPP
#define STRUCT_CMT_READER_HPP

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class ILineErrorListener;
};

class CSerialObject;
class ILineReader;

/*
  Usage examples
*/

struct CTabDelimitedValidatorMessage
{
    string m_src;
    int m_row;
    int m_col;
    bool m_warning;
    string m_msg;
    string m_colname;
};

class CTabDelimitedValidator
{
public:
   enum e_Flags
   {
       e_tab_tab_delim            =   1,
       e_tab_comma_delim          =   2,
       e_tab_noheader             =   4,
       e_tab_ignore_empty_rows    =   8,
       e_tab_xml_report           =  16,
       e_tab_tab_report           =  32,
       e_tab_text_report          =  16+32,
       e_tab_html_report          =  64,
       e_tab_ignore_unknown_types = 128
   }; 

   // If you need messages and error to be logged
   // supply an optional ILineErrorListener instance
   CTabDelimitedValidator(e_Flags flags = e_tab_tab_delim): 
   m_flags(flags)
   {
   }

   // TODO: replace with options flags instead of parameters
   void ValidateInput(ILineReader& reader, 
       const string& default_columns, const string& required,
       const string& ignored, const string& unique,
       const string& discouraged,
       const vector<string>& require_one);

   void GenerateOutput(CNcbiOstream* out_stream, bool no_headers);
   static void RegisterAliases(CNcbiIstream* in_stream);

private:
    bool _Validate(int col_number, const CTempString& value);

    void _OperateRows(ILineReader& reader);
    bool _ProcessHeader(ILineReader& reader, const CTempString& default_cols);
    bool _CheckHeader(const string& discouraged, const vector<string>& require_one);

    bool _MakeColumns(const string& message, 
        const CTempString& columns, vector<bool>& col_defs);

    void _ReportError(int col_number, const CTempString& error, const CTempString& colname, bool warning = false);
    void _ReportWarning(int col_number, const CTempString& error, const CTempString& colname);
    void _ReportTab(CNcbiOstream* out_stream);
    void _ReportXML(CNcbiOstream* out_stream, bool no_headers);


    int  m_current_row_number;
    CTempString m_delim;
    vector<string> m_col_defs;
    vector<bool>   m_required_cols;
    vector<bool>   m_ignored_cols;
    vector<bool>   m_unique_cols;
    vector< set<string> > m_require_one_cols;

    vector< map<string, int> > m_unique_values;

    list<CTabDelimitedValidatorMessage> m_errors;
    e_Flags m_flags;
};

END_NCBI_SCOPE


#endif
