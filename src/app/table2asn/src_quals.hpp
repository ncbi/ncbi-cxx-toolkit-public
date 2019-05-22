#ifndef SOURCE_QUAL_READER_HPP
#define SOURCE_QUAL_READER_HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objtools/readers/mod_reader.hpp>

BEGIN_NCBI_SCOPE

namespace objects {
// forward declarations
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

USING_SCOPE(objects);
void g_ApplyDeflineMods(CBioseq& bioseq);
void g_ApplyMods(
        const string& commandLineStr,
        const string& namedSrcFile,
        const string& defaultSrcFile,
        bool allowAcc,
        CSeq_entry& entry);

struct SSrcQualParsed
{
    //CTempString m_unparsed;
    string m_unparsed;
    vector<CTempString> m_parsed;
    CRef<CSeq_id> m_id;
};

struct SSrcQuals
{
    using TModList = CModHandler::TModList;

    bool AddQualifiers(CSourceModParser& mod, const CBioseq::TId& ids);
    bool AddQualifiers(CSourceModParser& mod, const string& id);
    void AddQualifiers(CSourceModParser& mod, const vector<CTempString>& values);
    bool AddQualifiers(const CBioseq::TId& ids, TModList& mods);
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

/*
    void ApplyMods(
            const string& commandLineStr,
            const string& namedSrcFile,
            const string& defaultSrcFile,
            CSeq_entry& entry);
*/

   bool LoadSourceQualifiers(const string& namedFile, const string& defaultFile);
   static void LoadSourceQualifiers(const string& fileName, bool allowAcc, SSrcQuals& quals);
   void ProcessSourceQualifiers(CSeq_entry& container);
   static bool ApplyQualifiers(CSourceModParser& mod, CBioseq& bioseq, ILineErrorListener* listener);
private:
   static bool x_ParseAndAddTracks(CBioseq& container,  const string& name, const string& value);


   CTable2AsnContext* m_context;
   SSrcQuals m_quals[2];
   SSrcQuals m_QualsFromDefaultSrcFile;
   SSrcQuals m_QualsFromNamedSrcFile;
};

END_NCBI_SCOPE


#endif
