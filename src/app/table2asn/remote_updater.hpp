#ifndef __REMOTE_UPDATER_HPP_INCLUDED__
#define __REMOTE_UPDATER_HPP_INCLUDED__

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class IMessageListener;
class CMLAClient;
class CBioseq;
class CTaxon1;
class COrg_ref;
};

class CTable2AsnContext;
class CSerialObject;

class CRemoteUpdater
{
public:
   CRemoteUpdater(const CTable2AsnContext& ctx);
   ~CRemoteUpdater();

   void UpdatePubReferences(CSerialObject& obj);

   void UpdateOrgReferences(objects::CSeq_entry& entry);
private:
   void xUpdatePubReferences(objects::CSeq_entry& entry);
   void xUpdateOrgTaxname(objects::COrg_ref& org);

   CRef<objects::CMLAClient>  m_mlaClient;
   auto_ptr<objects::CTaxon1> m_taxClient;
   const CTable2AsnContext&   m_context;
};

END_NCBI_SCOPE

#endif

