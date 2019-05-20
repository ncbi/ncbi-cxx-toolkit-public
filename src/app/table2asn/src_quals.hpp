#ifndef SOURCE_QUAL_READER_HPP
#define SOURCE_QUAL_READER_HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seq/Bioseq.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
namespace objects
{
    class CUser_object;
    class CSeq_descr;
    class CBioseq;
    class CObject_id;
    class CSeq_entry;
    class CSeq_id;
    class ILineErrorListener;
    class CSourceModParser;
};

class CSerialObject;
class ILineReader;

/*
  Usage examples
*/

struct SSrcQualParsed
{
    //CTempString m_unparsed;
    string m_unparsed;
    vector<CTempString> m_parsed;
    CRef<objects::CSeq_id> m_id;
};

struct SSrcQuals
{
    bool AddQualifiers(objects::CSourceModParser& mod, const objects::CBioseq::TId& ids);
    bool AddQualifiers(objects::CSourceModParser& mod, const string& id);
    void AddQualifiers(objects::CSourceModParser& mod, const vector<CTempString>& values);

    using TLineMap = map<string, SSrcQualParsed>;
    TLineMap m_lines_map;
    //vector<CTempString> columnNames;
    vector<string> columnNames;
};


class CTable2AsnContext;
class CSourceQualifiersReader
{
public:
   CSourceQualifiersReader(CTable2AsnContext* context);
   ~CSourceQualifiersReader();

   bool LoadSourceQualifiers(const string& namedFile, const string& defaultFile);
   void ProcessSourceQualifiers(objects::CSeq_entry& container);
   static bool ApplyQualifiers(objects::CSourceModParser& mod, objects::CBioseq& bioseq, objects::ILineErrorListener* listener);
private:
   static bool x_ParseAndAddTracks(objects::CBioseq& container,  const string& name, const string& value);

   void x_LoadSourceQualifiers(SSrcQuals& quals, const string& fileName);

   CTable2AsnContext* m_context;
   SSrcQuals m_quals[2];
   SSrcQuals m_QualsFromDefaultSrcFile;
   SSrcQuals m_QualsFromNamedSrcFile;
};

END_NCBI_SCOPE


#endif
