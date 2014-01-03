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

class CTabDelimitedValidator
{
public:
   // If you need messages and error to be logged
   // supply an optional IMessageListener instance
   CTabDelimitedValidator(objects::IMessageListener* logger): m_logger(logger)
   {
   }

   // TODO: replace with options flags instead of parameters
   void ValidateInput(ILineReader& reader, const CTempString& default_collumns, 
           const CTempString& required, bool comma_separated, bool no_header, bool ignore_empty_rows);
private:
    void _Validate(int col_number, const CTempString& datatype, const CTempString& value);
    void _ReportError(int col_number, const CTempString& error);
    int  m_current_row_number;

    objects::IMessageListener* m_logger;
};

END_NCBI_SCOPE


#endif
