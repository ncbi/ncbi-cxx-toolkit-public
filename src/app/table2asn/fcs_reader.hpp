#ifndef __FCS_READER_INCLUDED__
#define __FCS_READER_INCLUDED__

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class IMessageListener;
class CIdMapper;
class CBioseq;
};

class ILineReader;
class CTable2AsnContext;
class CSerialObject;


class CForeignContaminationScreenReportReader
{
public:
    CForeignContaminationScreenReportReader(const CTable2AsnContext& context);
    ~CForeignContaminationScreenReportReader();

    void LoadFile(ILineReader& linereader);

//#sequence name, length, span(s), M/X, apparent source
    struct TColumns
    {
        string name;
        int    length;
        int    start;
        int    stop;
        char   mode;   // M/X
        string source;
    };
    typedef multimap<string, TColumns> Tdata;

    void PostProcess(objects::CSeq_entry& entry);

protected:
    Tdata m_data;
    const CTable2AsnContext& m_context;

    bool AnnotateOrRemove(objects::CSeq_entry& entry) const;
};

END_NCBI_SCOPE

#endif
