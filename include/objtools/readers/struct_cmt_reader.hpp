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

  std::list<CStructuredCommentsReader::TStructComment> comments;

  ILineReader reader1(ILineReader::New(filename);
  reader.LoadComments(reader1, comments);

  for (const CStructuredCommentsReader::TStructComment& cmt: comments)
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

   typedef struct {
       CRef<objects::CSeq_id> m_id;
       vector<CRef<objects::CSeqdesc> > m_descs;
   } TStructComment;

   template<typename _container>
   size_t LoadComments(ILineReader& reader, _container& cont)
   {
       vector<string> cols;
       _LoadHeaderLine(reader, cols);
       if (cols.empty())
           return 0;

       size_t loader = 0;

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
               cont.push_back(TStructComment());
               TStructComment& cmt = cont.back();
               cmt.m_id.Reset(new objects::CSeq_id(values[0], objects::CSeq_id::fParse_AnyLocal));
               _BuildStructuredComment(cmt, cols, values);
           }
       }
   }

   objects::CUser_object* FindStructuredComment(objects::CSeq_descr& descr);
#if 0
   size_t LoadCommentsVector(ILineReader& reader, vector<TStructComment>& cont)
   {
       LoadComments(reader, cont);
   }
#endif


protected:
   void _LoadHeaderLine(ILineReader& reader, vector<string>& cols);
   void _BuildStructuredComment(TStructComment& cmt, const vector<string>& cols, const vector<CTempString>& values);
   objects::CUser_object* _AddStructuredComment(objects::CUser_object* user_obj, TStructComment& cmt, const CTempString& name, const CTempString& value);
   objects::ILineErrorListener* m_logger;
};

END_NCBI_SCOPE


#endif
