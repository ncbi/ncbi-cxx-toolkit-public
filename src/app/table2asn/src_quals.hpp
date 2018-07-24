#ifndef SOURCE_QUAL_READER_HPP
#define SOURCE_QUAL_READER_HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbifile.hpp>

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

typedef struct 
{
    CTempString m_unparsed;
    vector<CTempString> m_parsed;
    CRef<objects::CSeq_id> m_id;
} TSrcQualParsed;

class TSrcQuals
{
public:
    typedef map<string, TSrcQualParsed> TLineMap;
    TLineMap m_lines_map;
    size_t m_id_col;
    auto_ptr<CMemoryFileMap> m_filemap;
    vector<CTempString> m_cols;

    bool AddQualifiers(objects::CSourceModParser& mod, const objects::CBioseq& bioseq);
    bool AddQualifiers(objects::CSourceModParser& mod, const string& id);
    void AddQualifiers(objects::CSourceModParser& mod, const vector<CTempString>& values);
};

class CTable2AsnContext;
class CSourceQualifiersReader
{
public:
   CSourceQualifiersReader(CTable2AsnContext* context);
   ~CSourceQualifiersReader();

   bool LoadSourceQualifiers(const string& filename, const string& opt_map_filename);
   void ProcessSourceQualifiers(objects::CSeq_entry& container, const string& opt_map_filename);
   static bool ApplyQualifiers(objects::CSourceModParser& mod, objects::CBioseq& bioseq, objects::ILineErrorListener* listener);
private:
   static bool x_ParseAndAddTracks(objects::CBioseq& container,  const string& name, const string& value);

   bool x_ApplyAllQualifiers(objects::CSourceModParser& mod, objects::CBioseq& bioseq);
   void x_AddQualifiers(objects::CSourceModParser& mod, const string& filename);
   void x_LoadSourceQualifiers(TSrcQuals& quals, const string& filename, const string& opt_map_filename);

   CTable2AsnContext* m_context;
   TSrcQuals m_quals[2];
};

END_NCBI_SCOPE


#endif
