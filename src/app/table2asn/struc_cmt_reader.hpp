#ifndef STRUCT_CMT_READER_HPP
#define STRUCT_CMT_READER_HPP

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
	class CUser_object;
	class CSeq_descr;
};
class CSerialObject;
class ILineReader;


class CStructuredCommentsReader
{
public:
   CStructuredCommentsReader()
   {
   };
   virtual ~CStructuredCommentsReader()
   {
   }

public:
   // Read input tab delimited file and apply Structured Comments to the container
   // First row of the file is a list of Field to apply
   // First collumn of the file is an ID of the object (sequence) to apply
   void ProcessCommentsFileByCols(ILineReader& reader, CSerialObject& container);
   // Read input tab delimited file and apply Structured Comments to all objects (sequences) in the container
   // First collumn of the file is the name of the field
   // Second collumn of the file is the value of the field
   void ProcessCommentsFileByRows(ILineReader& reader, CSerialObject& container);
protected:
   objects::CUser_object* AddStructuredComment(objects::CUser_object* obj, const string& name, const string& value, CSerialObject& container);
   objects::CUser_object* AddStructuredComment(objects::CUser_object* obj, const string& name, const string& value, objects::CSeq_descr& container);
};

END_NCBI_SCOPE


#endif
