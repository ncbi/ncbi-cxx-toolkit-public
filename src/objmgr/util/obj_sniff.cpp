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
* Author: Anatoliy Kuznetsov
*
* File Description: Objects sniffer implementation.
*   
*/

#include <ncbi_pch.hpp>
#include <objmgr/util/obj_sniff.hpp>

#include <serial/iterator.hpp>
#include <serial/serial.hpp>
#include <serial/objhook.hpp>
#include <serial/iterator.hpp>
#include <serial/object.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



//////////////////////////////////////////////////////////////////
//
// Internal class capturing stream offsets of objects while 
// ASN parsing.
//
class COffsetReadHook : public CReadObjectHook
{
public:
    COffsetReadHook(CObjectsSniffer* sniffer, 
                    CObjectsSniffer::EEventCallBackMode event_mode)
    : m_Sniffer(sniffer),
      m_EventMode(event_mode)
    {
        _ASSERT(sniffer);
    }

    virtual void  ReadObject (CObjectIStream &in, const CObjectInfo &object);
private:
    CObjectsSniffer*                     m_Sniffer;
    CObjectsSniffer::EEventCallBackMode  m_EventMode;
};

void COffsetReadHook::ReadObject(CObjectIStream &in, 
                                 const CObjectInfo &object)
{
    m_Sniffer->m_CallStack.push_back(&object);

    try
    {
        if (m_EventMode == CObjectsSniffer::eCallAlways) {

            // Clear the discard flag before calling sniffer's event reactors
            m_Sniffer->SetDiscardCurrObject(false);

            m_Sniffer->OnObjectFoundPre(object, in.GetStreamOffset());
     
            DefaultRead(in, object);

            m_Sniffer->OnObjectFoundPost(object);

            // Relay discard flag to the stream
            bool discard = m_Sniffer->GetDiscardCurrObject();
            in.SetDiscardCurrObject(discard);
        }
        else 
        {
            if (m_EventMode == CObjectsSniffer::eSkipObject) {
                DefaultSkip(in, object);
            }
            else {
                DefaultRead(in, object);
            }
        }
    }
    catch(...)
    {
        m_Sniffer->m_CallStack.pop_back();
        throw;    
    }

    m_Sniffer->m_CallStack.pop_back();
}



void CObjectsSniffer::OnObjectFoundPre(const CObjectInfo& /*object*/, 
                                       CNcbiStreamoff /*stream_offset*/)
{
}

void CObjectsSniffer::OnObjectFoundPost(const CObjectInfo& /* object */)
{
}


void CObjectsSniffer::AddCandidate(CObjectTypeInfo ti, 
                                   EEventCallBackMode emode) 
{
    m_Candidates.push_back(SCandidateInfo(ti, emode));
}

void CObjectsSniffer::Probe(CObjectIStream& input)
{
    _ASSERT(m_Candidates.size());
    vector<CRef<COffsetReadHook> > hooks;  // list of all hooks we set

    //
    // create hooks for all candidates
    //

    TCandidates::const_iterator it;

    for (it = m_Candidates.begin(); it < m_Candidates.end(); ++it) {
        CRef<COffsetReadHook> h(new COffsetReadHook(this, it->event_mode));
        
        it->type_info.SetLocalReadHook(input, &(*h));
        hooks.push_back(h);

    } // for

    m_TopLevelMap.clear();


    if (input.GetDataFormat() == eSerial_AsnText) {
        ProbeASN1_Text(input);
    } else {
        ProbeASN1_Bin(input);
    }

    //
    // Reset(clean) the hooks
    //

    _ASSERT(hooks.size() == m_Candidates.size());
    for (it = m_Candidates.begin(); it < m_Candidates.end(); ++it) {
        it->type_info.ResetLocalReadHook(input);
    } // for
}


void CObjectsSniffer::ProbeASN1_Text(CObjectIStream& input)
{
    TCandidates::const_iterator it;
    TCandidates::const_iterator it_prev_cand = m_Candidates.end();
    TCandidates::const_iterator it_end = m_Candidates.end();

    string header;

    try {
        while (true) {
            m_StreamOffset = input.GetStreamOffset();
            header = input.ReadFileHeader();

            if (it_prev_cand != it_end) {
                // Check the previously found candidate first
                // (performance optimization)
                if (header == it_prev_cand->type_info.GetTypeInfo()->GetName()) {
                    it = it_prev_cand;
                    CObjectInfo object_info(it->type_info.GetTypeInfo());

                    input.Read(object_info, CObjectIStream::eNoFileHeader);
                    m_TopLevelMap.push_back(
                        SObjectDescription(it->type_info, m_StreamOffset));

                    LOG_POST(Info 
                             << "ASN.1 text top level object found:" 
                             << it->type_info.GetTypeInfo()->GetName());
                    continue;
                }
            } else {
                // Scan through all candidates
                for (it = m_Candidates.begin(); it < it_end; ++it) {
                    if (header == it->type_info.GetTypeInfo()->GetName()) {
                        it_prev_cand = it;

                        CObjectInfo object_info(it->type_info.GetTypeInfo());

                        input.Read(object_info, CObjectIStream::eNoFileHeader);
                        m_TopLevelMap.push_back(
                            SObjectDescription(it->type_info, m_StreamOffset));

                        LOG_POST(Info 
                                 << "ASN.1 text top level object found:" 
                                 << it->type_info.GetTypeInfo()->GetName());
                        break;
                    }
                } // for
            }

        } // while
    }
    catch (CEofException& ) {
    }
    catch (CException& e) {
        LOG_POST(Info << "Exception reading ASN.1 text " << e.what());
    }
}


void CObjectsSniffer::ProbeASN1_Bin(CObjectIStream& input)
{
    TCandidates::const_iterator it;

    //
    // Brute force interrogation of the source file.
    //

    it = m_Candidates.begin();
    while (it < m_Candidates.end()) {

        CObjectInfo object_info(it->type_info.GetTypeInfo());

        try {

            LOG_POST(Info 
                     << "Trying ASN.1 binary top level object:" 
                     << it->type_info.GetTypeInfo()->GetName() );

            m_StreamOffset = input.GetStreamOffset();

            input.Read(object_info);
            m_TopLevelMap.push_back(SObjectDescription(it->type_info, 
                                                       m_StreamOffset));

            LOG_POST(Info 
                     << "ASN.1 binary top level object found:" 
                     << it->type_info.GetTypeInfo()->GetName() );
        }
        catch (CEofException& ) {
            break;
        }
        catch (CException& ) {
            Reset();
            input.SetStreamOffset(m_StreamOffset);
            ++it; // trying the next type.
        }
    }

}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.20  2005/02/02 19:49:55  grichenk
* Fixed more warnings
*
* Revision 1.19  2004/08/30 18:21:05  gouriano
* Use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.18  2004/08/04 14:46:55  vasilche
* Removed unused variables.
*
* Revision 1.17  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.16  2004/02/04 13:53:03  kuznets
* Performance optimizations in CObjectsSniffer::ProbeASN1_Text
*
* Revision 1.15  2004/01/05 15:01:37  kuznets
* Sniffer::Probe() : restoring source stream offset after a failure.
*
* Revision 1.14  2003/10/07 20:43:12  kuznets
* Added Reset() call when parsing fails.
*
* Revision 1.13  2003/08/28 16:15:57  kuznets
* + SetDiscardCurrObject() method
*
* Revision 1.12  2003/08/25 14:27:38  kuznets
* Added stack reflecting current serialization hooks call hierachy.
*
* Revision 1.11  2003/08/05 21:12:14  kuznets
* +eSkipObject deserialization callback mode
*
* Revision 1.10  2003/08/05 14:43:47  kuznets
* Fixed compilation warnings
*
* Revision 1.9  2003/08/05 14:31:28  kuznets
* Implemented background "do not call" candidates for recognition.
*
* Revision 1.8  2003/06/02 18:58:25  dicuccio
* Fixed #include directives
*
* Revision 1.7  2003/06/02 18:52:17  dicuccio
* Fixed bungled commit
*
* Revision 1.5  2003/05/29 20:41:37  kuznets
* Fixed an offset bug in COffsetReadHook::ReadObject
*
* Revision 1.4  2003/05/22 16:47:22  kuznets
* ObjectFound methods renamed to OnObjectFound.
*
* Revision 1.3  2003/05/21 14:28:27  kuznets
* Implemented ObjectFoundPre, ObjectFoundPost
*
* Revision 1.2  2003/05/19 16:38:51  kuznets
* Added support for ASN text
*
* Revision 1.1  2003/05/16 19:34:55  kuznets
* Initial revision.
*
* ===========================================================================
*/
