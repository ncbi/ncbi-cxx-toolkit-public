#ifndef UTIL___IEVENT_TRANSMITTER__HPP
#define UTIL___IEVENT_TRANSMITTER__HPP

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
 * Authors:  Vladimir Tereshkov
 *
 * File Description:
 *    Event transmitter interface  
 */


#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class CEvent;


class IEventTransmitter
{
protected:
    IEventTransmitter * m_pHost;

public:
    IEventTransmitter()
        : m_pHost(NULL) { }

    virtual void SetHost(IEventTransmitter * host) = 0;
    virtual void FireEvent(const CEvent * evt) = 0;
    virtual ~IEventTransmitter(void){}
};

#define EVENT_MAP_TX_BEGIN \
        virtual void SetHost(IEventTransmitter * host) { \
        if (host) m_pHost = host; \
        else      _TRACE("invalid IEventTransmitter handle");}\
        virtual void FireEvent(const CEvent * evt){

#define EVENT_FIRE_ALL() if (m_pHost) { m_pHost->FireEvent(evt); } \
        else { \
            _TRACE("attempt to route event to NULL host: FireEvent()"); \
        } \

#define EVENT_MAP_TX_END \
        }

END_NCBI_SCOPE


#endif  // UTIL___IEVENT_TRANSMITTER__HPP


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/04/06 18:25:20  dicuccio
 * Cosmetic changes.  Initialize internal pointer to NULL in ctor.
 *
 * Revision 1.1  2004/03/26 20:41:37  tereshko
 * Initial Revision
 *
 * ===========================================================================
 */

