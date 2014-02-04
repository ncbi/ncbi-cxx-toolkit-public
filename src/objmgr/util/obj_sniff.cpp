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
#include <objmgr/error_codes.hpp>

#include <serial/iterator.hpp>
#include <serial/serial.hpp>
#include <serial/objhook.hpp>
#include <serial/iterator.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_Sniffer


BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(5);

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

namespace {

class CCallStackGuard {
public:
    CCallStackGuard(CObjectsSniffer::TObjectStack& stack,
                    const CObjectInfo& object)
        : m_Stack(stack)
        {
            m_Stack.push_back(&object);
        }
    ~CCallStackGuard(void)
        {
            m_Stack.pop_back();
        }
private:
    CObjectsSniffer::TObjectStack& m_Stack;
};

}

void COffsetReadHook::ReadObject(CObjectIStream &in, 
                                 const CObjectInfo &object)
{
    CCallStackGuard guard(m_Sniffer->m_CallStack, object);
    
    if (m_EventMode == CObjectsSniffer::eCallAlways) {

        // Clear the discard flag before calling sniffer's event reactors
        m_Sniffer->SetDiscardCurrObject(false);

        m_Sniffer->OnObjectFoundPre(object, in.GetStreamPos());
     
        DefaultRead(in, object);

        m_Sniffer->OnObjectFoundPost(object);

        // Relay discard flag to the stream
        bool discard = m_Sniffer->GetDiscardCurrObject();
        in.SetDiscardCurrObject(discard);
    }
    else {
        if (m_EventMode == CObjectsSniffer::eSkipObject) {
            DefaultSkip(in, object);
        }
        else {
            DefaultRead(in, object);
        }
    }
}



void CObjectsSniffer::OnObjectFoundPre(const CObjectInfo& /*object*/, 
                                       CNcbiStreampos /*stream_pos*/)
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


    if (input.GetDataFormat() == eSerial_AsnText
        || input.GetDataFormat() == eSerial_Xml) {
        ProbeText(input);
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


void CObjectsSniffer::ProbeText(CObjectIStream& input)
{
    TCandidates::const_iterator it;
    TCandidates::const_iterator it_prev_cand = m_Candidates.end();
    TCandidates::const_iterator it_end = m_Candidates.end();

    string format_name;  // for LOG_POST messages
    if (input.GetDataFormat() == eSerial_AsnText) {
        format_name = "ASN.1 text";
    } else {
        format_name = "XML";
    }

    string header;

    CNcbiStreampos start_pos = input.GetStreamPos();
    try {
        while (true) {
            m_StreamPos = input.GetStreamPos();
            header = input.ReadFileHeader();

            if (it_prev_cand != it_end) {
                // Check the previously found candidate first
                // (performance optimization)
                if (header == it_prev_cand->type_info.GetTypeInfo()->GetName()) {
                    it = it_prev_cand;
                    CObjectInfo object_info(it->type_info.GetTypeInfo());

                    input.Read(object_info, CObjectIStream::eNoFileHeader);
                    m_TopLevelMap.push_back(
                        SObjectDescription(it->type_info, m_StreamPos));

                    LOG_POST_X(1, Info 
                               << format_name << " top level object found:" 
                               << it->type_info.GetTypeInfo()->GetName());
                    continue;
                }
            }

            // Scan through all candidates
            for (it = m_Candidates.begin(); it < it_end; ++it) {
                if (header == it->type_info.GetTypeInfo()->GetName()) {
                    it_prev_cand = it;

                    CObjectInfo object_info(it->type_info.GetTypeInfo());

                    input.Read(object_info, CObjectIStream::eNoFileHeader);
                    m_TopLevelMap.push_back(
                        SObjectDescription(it->type_info, m_StreamPos));

                    LOG_POST_X(2, Info 
                                << format_name << " top level object found:" 
                                << it->type_info.GetTypeInfo()->GetName());
                    break;
                }
            } // for
        } // while
    }
    catch (CEofException& /*ignored*/) {
    }
    catch (exception& e) {
        LOG_POST_X(3, Info << "Exception reading "
                   << format_name << " " << e.what());
    }
    input.SetStreamPos(start_pos);
}


void CObjectsSniffer::ProbeASN1_Bin(CObjectIStream& input)
{
    TCandidates::const_iterator it;

    //
    // Brute force interrogation of the source file.
    //

    CNcbiStreampos start_pos = input.GetStreamPos();
    it = m_Candidates.begin();
    while (it < m_Candidates.end()) {

        CObjectInfo object_info(it->type_info.GetTypeInfo());

        try {

            LOG_POST_X(4, Info 
                       << "Trying ASN.1 binary top level object:" 
                       << it->type_info.GetTypeInfo()->GetName() );

            m_StreamPos = input.GetStreamPos();

            input.Read(object_info);
            m_TopLevelMap.push_back(SObjectDescription(it->type_info, 
                                                       m_StreamPos));

            LOG_POST_X(5, Info 
                       << "ASN.1 binary top level object found:" 
                       << it->type_info.GetTypeInfo()->GetName() );
        }
        catch (CEofException& ) {
            break;
        }
        catch (exception& _DEBUG_ARG(e)) {
            _TRACE("  failed to read: " << e.what());
            Reset();
            input.SetStreamPos(m_StreamPos);
            ++it; // trying the next type.
        }
    }
    input.SetStreamPos(start_pos);
}


END_SCOPE(objects)
END_NCBI_SCOPE
