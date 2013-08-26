#ifndef TABLE2ASN_MULTIREADER_HPP
#define TABLE2ASN_MULTIREADER_HPP

#include <util/format_guess.hpp>

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

class CTable2AsnContext;
class CSerialObject;

//  ============================================================================
class CMultiReader
//  ============================================================================
{
public:
   CMultiReader(objects::IMessageListener* logger);
   ~CMultiReader();

   CRef<CSerialObject> ReadFile(const CTable2AsnContext& args, const string& ifname);
   CRef<objects::CSeq_entry> LoadFile(const CTable2AsnContext& args, const string& ifname);
   void Cleanup(const CTable2AsnContext& args, CRef<objects::CSeq_entry>);
   void WriteObject(CSerialObject&, ostream& );
   void ApplyAdditionalProperties(const CTable2AsnContext& args, objects::CSeq_entry& entry);
   void LoadTemplate(CTable2AsnContext& context, const string& ifname);
   void LoadDescriptors(const CTable2AsnContext& args, const string& ifname, CRef<objects::CSeq_descr> & out_desc);
   CRef<objects::CSeq_descr> GetSeqDescr(CSerialObject* obj);
   void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeq_descr & source);
   void MergeDescriptors(objects::CSeq_descr & dest, const objects::CSeqdesc & source);
   void ApplyDescriptors(objects::CSeq_entry & obj, const objects::CSeq_descr & source);
   CRef<CSerialObject> HandleSubmitTemplate(const CTable2AsnContext& args, CRef<objects::CSeq_entry> object) const;

protected:

private:
    CRef<CSerialObject> xProcessDefault(const CTable2AsnContext&, CNcbiIstream&);
    CRef<objects::CSeq_entry> CreateNewSeqFromTemplate(const CTable2AsnContext& context, objects::CBioseq& bioseq) const;
#if 0
    void xProcessWiggle(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessWiggleRaw(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessBed(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessBedRaw(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGtf(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessVcf(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessNewick(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGff3(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGff2(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessGvf(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessAlignment(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
    void xProcessAgp(const CTable2AsnContext&, CNcbiIstream&, CNcbiOstream&);
#endif

    void xSetFormat(const CTable2AsnContext&, CNcbiIstream&);
    void xSetFlags(const CTable2AsnContext&, CNcbiIstream&);
    void xSetMapper(const CTable2AsnContext&);
    void xSetErrorContainer(const CTable2AsnContext&);

//    void xWriteObject(CSerialObject&, CNcbiOstream& );
//    void xDumpErrors(CNcbiOstream& );

    CFormatGuess::EFormat m_uFormat;
    bool m_bDumpStats;
    int  m_iFlags;
    string m_AnnotName;
    string m_AnnotTitle;

    auto_ptr<objects::CIdMapper> m_pMapper;
    objects::IMessageListener* m_logger;
};

END_NCBI_SCOPE

#endif
