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
  Usage examples
  ProcessCommentsFileByRows tries to apply structure comments to all sequences
  in the container.
  ProcessCommentsFileByCols tries to apply structure comments to those 
  sequences which id is found in first collumn of each row in the file.

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
   // Read input tab delimited file and apply Structured Comments to all objects 
   // (sequences) in the container
   // First collumn of the file is the name of the field
   // Second collumn of the file is the value of the field
   void ProcessCommentsFileByRows(ILineReader& reader, CSerialObject& container);

   void ProcessSourceQualifiers(ILineReader& reader, CSerialObject& container);

   static void ApplySourceQualifiers(CSerialObject& container, 
	   const string& src_qualifiers);
private:
   // service functions
   static objects::CBioseq* FindObjectById(CSerialObject& container, 
	   const objects::CSeq_id& id);

   void AddStructuredCommentToAllObjects(CSerialObject& container, 
	   const string& name, const string& value);

   objects::CUser_object* AddStructuredComment(objects::CUser_object* obj,
	   objects::CSeq_descr& container, 
	   const string& name, const string& value);

   void AddSourceQualifier(objects::CBioseq& container, 
	   const string& name, const string& value);
};

END_NCBI_SCOPE


#endif
