#ifndef __FCS_READER_INCLUDED__
#define __FCS_READER_INCLUDED__

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class ILineErrorListener;
class CIdMapper;
class CBioseq;
class CSeq_inst;
class CSeq_literal;
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
    struct Tloc
    {
        int    start;  // starting at zero
        int    len;
    };

    typedef map<int, int> Tlocs;
    struct TColumns
    {
        string name;
        int    length;
        char   mode;   // M/X
        string source;
        Tlocs locs;
    };
    typedef map<string, TColumns> Tdata;

    void PostProcess(objects::CSeq_entry& entry);

protected:
    Tdata m_data;
    const CTable2AsnContext& m_context;

    bool AnnotateOrRemove(objects::CSeq_entry& entry) const;
    void xTrimData(objects::CSeq_inst& inst, const Tlocs& col) const;
    void xTrimExt(objects::CSeq_inst& inst, const Tlocs& col) const;
    void xTrimLiteral(objects::CSeq_literal& lit, int start, int end) const;
    bool xCheckLen(const objects::CBioseq& inst) const;
};

END_NCBI_SCOPE

#endif
