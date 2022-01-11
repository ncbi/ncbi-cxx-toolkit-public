#ifndef FTAMED_H
#define FTAMED_H

#include <objtools/flatfile/flatfile_parse_info.hpp>
#include <corelib/ncbi_message.hpp>

BEGIN_NCBI_SCOPE

CRef<objects::CCit_art> FetchPubPmId(TEntrezId pmid);

using TFindPubOptions = Parser::SFindPubOptions;

class CPubFixMessageListener : public CMessageListener_Basic
{
public:
    EPostResult PostMessage(const IMessage& message) override;
};

bool MedArchInit();
void MedArchFini();

END_NCBI_SCOPE

#endif // FTAMED_H
