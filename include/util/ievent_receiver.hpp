#ifndef UTIL___IEVENT_RECIEVER__HPP
#define UTIL___IEVENT_RECIEVER__HPP

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
 *    Event reciever interface  
 */


#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class CEvent;
class IEventTransmitter;


// Interface defining reciever portion connection point
class IEventReceiver
{
public:
    virtual void OnEvent(const CEvent * evt) = 0;
    virtual ~IEventReceiver(void){}
};



// event rules
#define _EVENT_MAP_RX_BEGIN(context) \
    void context OnEvent(const CEvent * evt) { \
    bool sent_by_me = evt->GetSender()==dynamic_cast<IEventTransmitter*>(this);

// empty param gives warning on vc6
#define EVENT_MAP_RX_BEGIN_INT  _EVENT_MAP_RX_BEGIN(virtual)

#define EVENT_MAP_RX_BEGIN(className) _EVENT_MAP_RX_BEGIN(className::)

#define EVENT_FORWARD(event_type, target)  do { \
    if (evt->GetType() == (event_type)  &&  !sent_by_me ) {target->OnEvent(evt);} \
    }while(0);
        
#define EVENT_FORWARD_ALL(target)  do { \
    if (!sent_by_me ) {target->OnEvent(evt);} \
    }while(0);

#define EVENT_ACCEPT(event_type, on_function) do { \
    if (evt->GetType() == (event_type)  &&  !sent_by_me ) {(on_function)(evt->GetObject());} \
    }while(0);        

#define EVENT_ACCEPT_ALL(on_function)  do { \
    if (!sent_by_me ) { (on_function)(evt->GetObject());} \
    }while(0);

#define EVENT_MAP_RX_END }

#define EVENT_MAP_DECLARE() \
        virtual void OnEvent(const CEvent * evt);

END_NCBI_SCOPE

#endif  // UTIL___IEVENT_RECIEVER__HPP


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

