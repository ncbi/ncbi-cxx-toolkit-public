#ifndef __STRUCT_CMT_READER_HPP_INCLUDED__
#define __STRUCT_CMT_READER_HPP_INCLUDED__

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CSeq_descr;
    class CSeqdesc;
    class CSeq_id;
    class CUser_object;
    class ILineErrorListener;
};

class CSerialObject;
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
   ~CStructuredCommentsReader();

   class NCBI_XOBJREAD_EXPORT CStructComment
   {
   public:
       CRef<objects::CSeq_id> m_id;
       vector<CRef<objects::CSeqdesc> > m_descs;
       static const string& GetPrefix(const objects::CSeqdesc&);
   };

   template<typename _container>
   size_t LoadComments(ILineReader& reader, _container& cont,
            objects::CSeq_id::TParseFlags seqid_flags = objects::CSeq_id::fParse_Default)
   {
       vector<string> cols;
       _LoadHeaderLine(reader, cols);
       if (cols.empty())
           return 0;

       while (!reader.AtEOF())
       {
           reader.ReadLine();
           // First line is a collumn definitions
           CTempString current = reader.GetCurrentLine();
           if (current.empty())
               continue;

           // Each line except first is a set of values, first collumn is a sequence id
           vector<CTempString> values;
           NStr::Split(current, "\t", values);
           if (!values[0].empty())
           {
               // try to find destination sequence
               cont.push_back(CStructComment());
               CStructComment& cmt = cont.back();
               cmt.m_id.Reset(new objects::CSeq_id(values[0], seqid_flags));
               _BuildStructuredComment(cmt, cols, values);
           }
       }
       return cont.size();
   }

   size_t LoadCommentsByRow(ILineReader& reader, CStructComment& cmt)
   {
       objects::CUser_object* user = 0;

       while (!reader.AtEOF())
       {
           reader.ReadLine();
           // First line is a collumn definitions
           CTempString current = reader.GetCurrentLine();
           if (current.empty())
               continue;

           CTempString commentname, comment;
           NStr::SplitInTwo(current, "\t", commentname, comment);

           // create new user object
           user = _AddStructuredComment(user, cmt, commentname, comment);
       }
       return cmt.m_descs.size();
   }


protected:
   void _LoadHeaderLine(ILineReader& reader, vector<string>& cols);
   void _BuildStructuredComment(CStructComment& cmt, const vector<string>& cols, const vector<CTempString>& values);
   objects::CUser_object* _AddStructuredComment(objects::CUser_object* user_obj, CStructComment& cmt, const CTempString& name, const CTempString& value);
   objects::ILineErrorListener* m_logger;
};

END_NCBI_SCOPE


#endif
