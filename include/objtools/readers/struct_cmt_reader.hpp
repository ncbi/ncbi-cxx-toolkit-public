#ifndef __STRUCT_CMT_READER_HPP_INCLUDED__
#define __STRUCT_CMT_READER_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <list>
#include <optional>
#include <functional>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CSeqdesc;
    class CUser_object;
    class ILineErrorListener;
}

class ILineReader;

/*
  Usage examples

  CStructuredCommentsReader reader(error_logger);

  std::list<CStructuredCommentsReader::CStructComment> comments;

  ILineReader reader1(ILineReader::New(filename);
  reader.LoadComments(reader1, comments);

  for (const CStructuredCommentsReader::CStructComment& cmt: comments)
  {
     // do something
  }
*/

class NCBI_XOBJREAD_EXPORT CStructuredCommentsReader
{
public:
   // If you need messages and error to be logged
   // supply an optional ILineErrorListener instance
   CStructuredCommentsReader(objects::ILineErrorListener* logger);
   virtual ~CStructuredCommentsReader();

   class NCBI_XOBJREAD_EXPORT CStructComment
   {
   public:
       CRef<objects::CSeq_id> m_id;
       vector<CRef<objects::CSeqdesc> > m_descs;
       static const string& GetPrefix(const objects::CSeqdesc&);
   };

   size_t LoadComments(ILineReader& reader, list<CStructComment>& comments,
            objects::CSeq_id::TParseFlags seqid_flags = objects::CSeq_id::fParse_Default);

   size_t LoadCommentsByRow(ILineReader& reader, CStructComment& cmt);

   static bool SeqIdMatchesCommentId(const objects::CSeq_id& seqID, const objects::CSeq_id& commentID);


protected:
   void _LoadHeaderLine(ILineReader& reader, vector<string>& cols);
   void _BuildStructuredComment(CStructComment& cmt, const vector<string>& cols, const vector<CTempString>& values);
   objects::CUser_object* _AddStructuredComment(objects::CUser_object* user_obj, CStructComment& cmt, const CTempString& name, const CTempString& value);
   virtual void x_LogMessage(EDiagSev, const string& msg, unsigned int lineNum);
   objects::ILineErrorListener* m_logger;
};

END_NCBI_SCOPE


#endif
