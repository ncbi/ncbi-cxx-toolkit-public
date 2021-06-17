#ifndef BIGFILE_PROCESSING
#define BIGFILE_PROCESSING

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CSubmit_block;
class CBioseq;
class CBioseq_set;
class CSeq_descr;
class CSeq_annot;

class IProcessorCallback
{
public:

    virtual void Process(CRef<CSerialObject>& obj) = 0;
};

enum EBigFileContentType
{
    eContentUndefined,
    eContentSeqEntry,
    eContentBioseqSet,
    eContentSeqSubmit
};

bool ProcessBigFile(CObjectIStream& in, CObjectOStream& out, IProcessorCallback& callback, EBigFileContentType content_type);

END_objects_SCOPE
END_NCBI_SCOPE

#endif // BIGFILE_PROCESSING
