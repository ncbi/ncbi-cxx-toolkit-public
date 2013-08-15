#ifndef STRUCT_CMT_READER_HPP
#define STRUCT_CMT_READER_HPP

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
	class CUser_object;
	class CSeq_descr;
	class CBioseq;
	class CObject_id;
};

class CSerialObject;
class ILineReader;

/* 
  Usage example below
  ProcessCommentsFileByRows tries to apply structure comments to all sequences in the container
  ProcessCommentsFileByCols tries to apply structure comments to those sequences which id is found in first collumn of each row

  CRef<CSeq_entry> entry;

  CStructuredCommentsReader reader;

  ILineReader reader1(ILineReader::New(filename);
  reader.ProcessCommentsFileByRows(reader1, *entry);

  ILineReader reader2(ILineReader::New(filename);
  reader.ProcessCommentsFileByRows(reader2, *entry);
*/

class CStructuredCommentsReader
{
public:
   // Read input tab delimited file and apply Structured Comments to the container
   // First row of the file is a list of Field to apply
   // First collumn of the file is an ID of the object (sequence) to apply
   void ProcessCommentsFileByCols(ILineReader& reader, CSerialObject& container);
   // Read input tab delimited file and apply Structured Comments to all objects (sequences) in the container
   // First collumn of the file is the name of the field
   // Second collumn of the file is the value of the field
   void ProcessCommentsFileByRows(ILineReader& reader, CSerialObject& container);

   void ProcessSourceQualifiers(ILineReader& reader, CSerialObject& container);
   // service functions
   static void FillVector(const string& input, vector<string>& output);
   static objects::CBioseq* FindObjectById(CSerialObject& container, const objects::CSeq_id& id);
   static void ApplySourceQualifiers(CSerialObject& container, const string& src_qualifiers);
protected:
   objects::CUser_object* AddStructuredComment(objects::CUser_object* obj, const string& name, const string& value, CSerialObject& container);
   objects::CUser_object* AddStructuredComment(objects::CUser_object* obj, const string& name, const string& value, objects::CSeq_descr& container);
   void AddSourceQualifier(const string& name, const string& value, objects::CBioseq& container);
};

END_NCBI_SCOPE


#endif
