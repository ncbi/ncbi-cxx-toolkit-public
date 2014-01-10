#ifndef STRUCT_CMT_READER_HPP
#define STRUCT_CMT_READER_HPP

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class IMessageListener;
};

class CSerialObject;
class ILineReader;

/*
  Usage examples
*/

struct CTabDelimitedValidatorError
{
    string m_src;
    int m_row;
    int m_col;
    string m_msg;
};

class CTabDelimitedValidator
{
public:
   enum e_Flags
   {
       e_tab_tab_delim            =  1,
       e_tab_comma_delim          =  2,
       e_tab_noheader             =  4,
       e_tab_ignore_empty_rows    =  8,
       e_tab_xml_report           = 16,
       e_tab_tab_report           = 32,
       e_tab_text_report          = 16+32,
       e_tab_html_report          = 64
   };
   

   // If you need messages and error to be logged
   // supply an optional IMessageListener instance
   CTabDelimitedValidator(e_Flags flags = e_tab_tab_delim): 
   m_flags(flags)
   {
   }

   // TODO: replace with options flags instead of parameters
   void ValidateInput(ILineReader& reader, 
       const CTempString& default_columns, const CTempString& required,
       const CTempString& ignored);

   void GenerateOutput(CNcbiOstream* out_stream, bool no_headers);

private:
    bool _Validate(int col_number, const CTempString& value);

    void _OperateRows(ILineReader& reader);
    bool _ProcessHeader(ILineReader& reader, const CTempString& default_columns);

    bool _MakeColumns(const string& message, const CTempString& columns, vector<bool>& col_defs);

    void _ReportError(int col_number, const CTempString& error);
    void _ReportTab(CNcbiOstream* out_stream);
    void _ReportXML(CNcbiOstream* out_stream, bool no_headers);
    int  m_current_row_number;
    CTempString m_delim;
    vector<string> m_col_defs;
    vector<bool>   m_required_cols;
    vector<bool>   m_ignored_cols;

    list<CTabDelimitedValidatorError> m_errors;
    e_Flags m_flags;
};

END_NCBI_SCOPE


#endif
