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

#include <objects/util/obj_sniff.hpp>

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
    virtual void  ReadObject (CObjectIStream &in, const CObjectInfo &object);
};

void COffsetReadHook::ReadObject(CObjectIStream &in, 
                                 const CObjectInfo &object)
{
    const CTypeInfo *type_info = object.GetTypeInfo();
    const string& class_name = type_info->GetName();
     
    DefaultRead(in, object);
}




void CObjectsSniffer::TopObjectFound(CObjectInfo& object)
{
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
        CRef<COffsetReadHook> h(new COffsetReadHook());
        
        it->SetLocalReadHook(input, &(*h));
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
        it->ResetLocalReadHook(input);
    } // for
}


void CObjectsSniffer::ProbeASN1_Text(CObjectIStream& input)
{
    TCandidates::const_iterator it;

    while (true) {
        try {
            size_t offset = input.GetStreamOffset();
            string header = input.ReadFileHeader();

            for (it = m_Candidates.begin(); it < m_Candidates.end(); ++it) {
                if (header == it->GetTypeInfo()->GetName()) {
                    CObjectInfo object_info(it->GetTypeInfo());

                    input.Read(object_info, CObjectIStream::eNoFileHeader);
                    m_TopLevelMap.push_back(SObjectDescription(*it, offset));
                }
            } // for
        }
        catch (CEofException& ) {
            break;
        }
        catch (CException& e) {
            LOG_POST(Info << "Exception reading ASN.1 text " << e.what());
            break;
        }

    } // while
}


void CObjectsSniffer::ProbeASN1_Bin(CObjectIStream& input)
{
    TCandidates::const_iterator it;

    //
    // Brute force interrogation of the source file.
    //

    it = m_Candidates.begin();
    while (it < m_Candidates.end()) {

        CObjectInfo object_info(it->GetTypeInfo());

        try {
            size_t offset = input.GetStreamOffset();

            input.Read(object_info);
            m_TopLevelMap.push_back(SObjectDescription(*it, offset));

            LOG_POST(Info 
                     << "ASN.1 Top level object found:" 
                     << it->GetTypeInfo()->GetName() );

            TopObjectFound(object_info);
        }
        catch (CEofException& ) {
            break;
        }
        catch (CException& ) {
            ++it; // trying the next type.
        }
    }

}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.2  2003/05/19 16:38:51  kuznets
* Added support for ASN text
*
* Revision 1.1  2003/05/16 19:34:55  kuznets
* Initial revision.
*
* ===========================================================================
*/
