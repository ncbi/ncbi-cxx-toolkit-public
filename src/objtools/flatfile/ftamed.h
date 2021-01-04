#ifndef FTAMED_H
#define FTAMED_H

#include <objtools/flatfile/flatfile_parse_info.hpp>
#include <corelib/ncbi_message.hpp>

BEGIN_NCBI_SCOPE

CRef<objects::CCit_art> FetchPubPmId(Int4 pmid);

using TFindPubOptions = Parser::SFindPubOptions;

class CPubFixingMessageListener : public CMessageListener_Basic 
{
public: 
    EPostResult PostMessage(const IMessage& message) override;
};

bool MedArchInit(void);
void MedArchFini(void);

END_NCBI_SCOPE

#endif // FTAMED_H
