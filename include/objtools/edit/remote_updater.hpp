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
class CAuth_list;

BEGIN_SCOPE(edit)

class CCachedTaxon3_impl;

class NCBI_XOBJEDIT_EXPORT CRemoteUpdater
{
public:
   CRemoteUpdater(bool enable_caching = true);
   ~CRemoteUpdater();

   void UpdatePubReferences(CSerialObject& obj);
   void UpdatePubReferences(CSeq_entry_EditHandle& obj);

   void UpdateOrgFromTaxon(IMessageListener* logger, CSeq_entry& entry);
   void UpdateOrgFromTaxon(IMessageListener* logger, CSeq_entry_EditHandle& obj);
   void UpdateOrgFromTaxon(IMessageListener* logger, CSeqdesc& obj);
   void ClearCache();
   static void ConvertToStandardAuthors(CAuth_list& auth_list);
   static void PostProcessPubs(CSeq_entry_EditHandle& obj);
   static void PostProcessPubs(CSeq_entry& obj);
   static void PostProcessPubs(CPubdesc& pubdesc);

   // Use either shared singleton or individual instances
   static CRemoteUpdater& GetInstance();

private:
   void xUpdatePubReferences(CSeq_entry& entry);
   void xUpdatePubReferences(CSeq_descr& descr);
   void xUpdateOrgTaxname(IMessageListener* logger, COrg_ref& org);


   CRef<CMLAClient>  m_mlaClient;
   auto_ptr<CCachedTaxon3_impl>  m_taxClient;
   bool m_enable_caching;
   CMutex m_Mutex;
   static CMutex m_static_mutex;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
