#ifndef FTAMED_H
#define FTAMED_H

#include <corelib/ncbi_message.hpp>

//#include <objtools/edit/pub_fix.hpp>
namespace ncbi::objects::edit
{
    class IPubmedUpdater;
}

BEGIN_NCBI_SCOPE

objects::edit::IPubmedUpdater* GetPubmedClient();
CRef<objects::CCit_art>        FetchPubPmId(TEntrezId pmid);

class CPubFixMessageListener : public CMessageListener_Basic
{
public:
    EPostResult PostMessage(const IMessage& message) override;
};

END_NCBI_SCOPE

#endif // FTAMED_H
