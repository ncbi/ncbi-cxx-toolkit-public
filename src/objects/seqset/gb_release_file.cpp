/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author:  Mati Shomrat
* File Description:
*   Utility class for processing Genbank release files.
*
*/
#include <corelib/ncbistd.hpp>
#include <serial/objhook.hpp>
#include <serial/objistr.hpp>
#include <serial/objectiter.hpp>
#include <serial/objectio.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <vector>
#include <algorithm>
#include <objects/seqset/gb_release_file.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


////////////////////////////////////////////////////////////////////////////
//
//  CGBReleaseFileImpl


class CGBReleaseFileImpl : public CReadClassMemberHook
{
public:
    typedef CGBReleaseFile::ISeqEntryHandler*   THandler;

    CGBReleaseFileImpl(const string& file_name);
    void Read(void);
    void RegisterHandler(THandler handler);

    virtual void ReadClassMember(CObjectIStream& in, const CObjectInfoMI& member);

private:
    THandler                    m_Handler;
    auto_ptr<CObjectIStream>    m_In;
    CBioseq_set                 m_Seqset;
    bool                        m_Stopped;
};


CGBReleaseFileImpl::CGBReleaseFileImpl(const string& file_name) :
    m_In(CObjectIStream::Open(file_name, eSerial_AsnBinary)),
    m_Stopped(false)
{
    _ASSERT(m_In.get() != 0  &&  m_In->InGoodState());
}


void CGBReleaseFileImpl::RegisterHandler(THandler handler)
{
    m_Handler = handler;
}


void CGBReleaseFileImpl::Read(void)
{
    // install the read hook on the top level Bioseq-set's sequence of entries
    CObjectTypeInfo info(CBioseq_set::GetTypeInfo());
    info.FindMember("seq-set").SetLocalReadHook(*m_In, this);
    
    try {
        // read in the file, this will execute the handler's code for each
        // Seq-entry read.
        m_In->Read(&m_Seqset, CBioseq_set::GetTypeInfo());
    } catch (const CException&) {
        if ( !m_Stopped ) {
            throw;
        }
    }
}


void CGBReleaseFileImpl::ReadClassMember
(CObjectIStream& in,
 const CObjectInfoMI& member)
{
    // remove the read hook
    member.ResetLocalReadHook(in);

    // iterate over the sequence of entries
    for ( CIStreamContainerIterator it(in, member); it; ++it ) {
        CRef<CSeq_entry> se(new CSeq_entry);
        it >> *se;
        if ( se ) {
            if ( !m_Handler->HandleSeqEntry(se) ) {
                m_Stopped = true;
                break;
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CGBReleaseFile


CGBReleaseFile::CGBReleaseFile(const string& file_name) :
    m_Impl(new CGBReleaseFileImpl(file_name))
{
    _ASSERT(m_Impl);
}


CGBReleaseFile::~CGBReleaseFile(void)
{
}


void CGBReleaseFile::Read(void)
{
    x_GetImpl().Read();
}


void CGBReleaseFile::RegisterHandler(ISeqEntryHandler* handler)
{
    x_GetImpl().RegisterHandler(handler);
}


CGBReleaseFileImpl& CGBReleaseFile::x_GetImpl(void)
{
    return reinterpret_cast<CGBReleaseFileImpl&>(*m_Impl);
}


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2004/05/19 14:41:29  shomrat
* Initial Revision
*
*
* ===========================================================================
*/

