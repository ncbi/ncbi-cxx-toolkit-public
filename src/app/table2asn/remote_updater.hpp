#ifndef __REMOTE_UPDATER_HPP_INCLUDED__
#define __REMOTE_UPDATER_HPP_INCLUDED__

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_entry;
class CSeq_submit;
class CSeq_descr;
class CSeqdesc;
class CMLAClient;
class CBioseq;
class COrg_ref;
class CSeq_entry_EditHandle;
class CCachedTaxon3_impl;
};

class CSerialObject;

class CRemoteUpdater
{
public:
   CRemoteUpdater();
   ~CRemoteUpdater();

   void UpdatePubReferences(CSerialObject& obj);
   void UpdatePubReferences(objects::CSeq_entry_EditHandle& obj);

   void UpdateOrgFromTaxon(objects::IMessageListener* logger, objects::CSeq_entry& entry);
   void UpdateOrgFromTaxon(objects::IMessageListener* logger, objects::CSeqdesc& obj);

private:
   void xUpdatePubReferences(objects::CSeq_entry& entry);
   void xUpdatePubReferences(objects::CSeq_descr& descr);
   void xUpdateOrgTaxname(objects::IMessageListener* logger, objects::COrg_ref& org);

   CRef<objects::CMLAClient>  m_mlaClient;
   auto_ptr<objects::CCachedTaxon3_impl>  m_taxClient;
   bool m_enable_caching;
};

END_NCBI_SCOPE

#endif
