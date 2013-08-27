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
};

class CTable2AsnContext;
class CSerialObject;

class CRemoteUpdater
{
public:
   CRemoteUpdater();
   ~CRemoteUpdater();
   void UpdatePubReferences(objects::CSeq_entry& entry);
private:
   CRef<objects::CMLAClient> mlaclient;
};

END_NCBI_SCOPE

#endif

