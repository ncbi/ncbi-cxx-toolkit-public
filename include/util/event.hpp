#ifndef UTILS___EVENT__HPP
#define UTILS___EVENT__HPP

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
 *    Event object for view communications  
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class IEventTransmitter;

class CEvent : public CObject 
{
public:    
    typedef CRef<CEvent>        TEventType;
    typedef Int4                TEventID;    
    typedef Int4                TEventFlags;
    typedef CRef<CObject>       TEventObject;    
    typedef IEventTransmitter * TEventSender;

public:
    const TEventID      GetType(void)   const   { return m_Id;      }   // will be removed
    const TEventID      GetID(void)     const   { return m_Id;      }   
    const TEventFlags   GetFlags(void)  const   { return m_Flags;   }
    const TEventObject  GetObject(void) const   { return m_pObject; }  
    const TEventSender  GetSender(void) const   { return m_pSender; }

    // event creation
   const static TEventType CreateEvent(TEventID type, TEventFlags flags, TEventObject object, TEventSender sender)
    {        
        CEvent * evt = new CEvent();        
        evt->m_Id       = type;
        evt->m_Flags    = flags;
        evt->m_pObject  = object;
        evt->m_pSender  = sender;  
        return TEventType(evt);
   }    

protected:
    // should not be called directly
    CEvent(void) {}
    virtual ~CEvent(void) {}

    // unique event ID
    TEventID            m_Id;    

    // additional event modifier - affect routing, etc.
    TEventFlags         m_Flags;

    // object, accociated with event
    TEventObject        m_pObject;

    // sender's transmitter interface
    TEventSender        m_pSender;
};

END_NCBI_SCOPE

#endif  // UTILS___EVENT__HPP


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/03/26 20:41:37  tereshko
 * Initial Revision
 *
 * ===========================================================================
 */

