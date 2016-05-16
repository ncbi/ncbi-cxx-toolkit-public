#ifndef READ_HOOKS
#define READ_HOOKS

#include <serial/objhook.hpp>
#include <objects/seqset/gb_release_file.hpp>

BEGIN_NCBI_SCOPE

class CReadSubmitBlockHook : public CReadClassMemberHook
{
public:
    CReadSubmitBlockHook(CObjectOStream& out);

    virtual void ReadClassMember(CObjectIStream &in, const CObjectInfoMI &member);

private:
    CObjectOStream& m_out;
};

class CReadEntryHook : public CReadObjectHook
{
public:
    CReadEntryHook(CGBReleaseFile::ISeqEntryHandler& handler, CObjectOStream& out);

    virtual void ReadObject(CObjectIStream &in, const CObjectInfo& obj);

private:
    CGBReleaseFile::ISeqEntryHandler& m_handler;
    CObjectOStream& m_out;
};

END_NCBI_SCOPE

#endif // READ_HOOKS