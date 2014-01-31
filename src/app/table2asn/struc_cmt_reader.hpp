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
    class CSeq_entry;
    class IMessageListener;
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
   // If you need messages and error to be logged
   // supply an optional IMessageListener instance
   CStructuredCommentsReader(objects::IMessageListener* logger): m_logger(logger)
   {
   }
   // Read input tab delimited file and apply Structured Comments to the container
   // First row of the file is a list of Field to apply
   // First collumn of the file is an ID of the object (sequence) to apply
   void ProcessCommentsFileByCols(ILineReader& reader, objects::CSeq_entry& container);
   // Read input tab delimited file and apply Structured Comments to all objects
   // (sequences) in the container
   // First collumn of the file is the name of the field
   // Second collumn of the file is the value of the field
   void ProcessCommentsFileByRows(ILineReader& reader, objects::CSeq_entry& container);

   void ProcessSourceQualifiers(ILineReader& reader, objects::CSeq_entry& container);
private:
   void AddStructuredCommentToAllObjects(objects::CSeq_entry& container,
       const string& name, const string& value);

   objects::CUser_object* AddStructuredComment(objects::CUser_object* obj,
       objects::CSeq_descr& container,
       const string& name, const string& value);

   void AddSourceQualifier(objects::CBioseq& container,
       const string& name, const string& value);

    objects::IMessageListener* m_logger;
};

class CStructuredComments
{
public:
    CStructuredComments(objects::CSeq_entry& container);
protected:
    CRef<objects::CSeq_entry> m_entry;
};

END_NCBI_SCOPE


#endif
