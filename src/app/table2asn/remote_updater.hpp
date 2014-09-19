#ifndef __REMOTE_UPDATER_HPP_INCLUDED__
#define __REMOTE_UPDATER_HPP_INCLUDED__

BEGIN_NCBI_SCOPE

class CSerialObject;

BEGIN_SCOPE(objects)

class CSeq_entry_EditHandle;
class IMessageListener;
class CSeq_entry;
class CSeqdesc;
class CSeq_descr;
class COrg_ref;
class CMLAClient;

BEGIN_SCOPE(edit)

class CCachedTaxon3_impl;

class /*NCBI_XOBJEDIT_EXPORT*/ CRemoteUpdater
{
public:
   CRemoteUpdater(bool enable_caching = true);
   ~CRemoteUpdater();

   void UpdatePubReferences(CSerialObject& obj);
   void UpdatePubReferences(objects::CSeq_entry_EditHandle& obj);

   void UpdateOrgFromTaxon(objects::IMessageListener* logger, objects::CSeq_entry& entry);
   void UpdateOrgFromTaxon(objects::IMessageListener* logger, objects::CSeq_entry_EditHandle& obj);
   void UpdateOrgFromTaxon(objects::IMessageListener* logger, objects::CSeqdesc& obj);

   // Use either shared singleton or individual instances
   static CRemoteUpdater& GetInstance();

private:
   void xUpdatePubReferences(objects::CSeq_entry& entry);
   void xUpdatePubReferences(objects::CSeq_descr& descr);
   void xUpdateOrgTaxname(objects::IMessageListener* logger, objects::COrg_ref& org);

   CRef<objects::CMLAClient>  m_mlaClient;
   auto_ptr<CCachedTaxon3_impl>  m_taxClient;
   bool m_enable_caching;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
