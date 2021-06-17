#ifndef _TABLE2ASN_DESCR_APPLY_HPP_
#define _TABLE2ASN_DESCR_APPLY_HPP_

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE
using namespace objects;

void g_ApplyDescriptors(const list<CRef<CSeqdesc>>& descriptors,
        CSeq_entry& seqEntry,
        ILineErrorListener* pErrorListener=nullptr);

END_NCBI_SCOPE

#endif // _TABLE2ASN_DESCR_APPLY_HPP_
