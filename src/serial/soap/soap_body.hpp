/* $Id$
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
 * Author: Andrei Gourianov
 *
 * File Description:
 *   Holds SOAP message Body contents
 *
 * ===========================================================================
 */

#ifndef SOAP_BODY_HPP
#define SOAP_BODY_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <list>


BEGIN_NCBI_SCOPE
// generated classes

class CSoapBody : public CSerialObject
{
    typedef CSerialObject Tparent;
public:
    // constructor
    CSoapBody(void);
    // destructor
    virtual ~CSoapBody(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    class C_E : public CSerialObject
    {
        typedef CSerialObject Tparent;
    public:
        // constructor
        C_E(void);
        // destructor
        ~C_E(void);
    
        // type info
        DECLARE_INTERNAL_TYPE_INFO();
    
        // types
        typedef CAnyContentObject TAnyContent;
    
        // getters
        // setters
    
        // mandatory
        // typedef CAnyContentObject TAnyContent
        bool IsSetAnyContent(void) const;
        bool CanGetAnyContent(void) const;
        void ResetAnyContent(void);
        const TAnyContent& GetAnyContent(void) const;
        void SetAnyContent(TAnyContent& value);
        TAnyContent& SetAnyContent(void);
    
        // reset whole object
        void Reset(void);
    
    
    private:
        // Prohibit copy constructor and assignment operator
        C_E(const C_E&);
        C_E& operator=(const C_E&);
    
        // data
        CRef< TAnyContent > m_AnyContent;
    };
    // types
    typedef std::list< CRef< C_E > > Tdata;

    // getters
    // setters

    // mandatory
    // typedef std::list< CRef< C_E > > Tdata
    bool IsSet(void) const;
    bool CanGet(void) const;
    void Reset(void);
    const Tdata& Get(void) const;
    Tdata& Set(void);
    operator const Tdata& (void) const;
    operator Tdata& (void);


private:
    // Prohibit copy constructor and assignment operator
    CSoapBody(const CSoapBody&);
    CSoapBody& operator=(const CSoapBody&);

    // data
    Uint4 m_set_State[1];
    Tdata m_data;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CSoapBody::C_E::IsSetAnyContent(void) const
{
    return m_AnyContent.NotEmpty();
}

inline
bool CSoapBody::C_E::CanGetAnyContent(void) const
{
    return IsSetAnyContent();
}

inline
const CAnyContentObject& CSoapBody::C_E::GetAnyContent(void) const
{
    if (!CanGetAnyContent()) {
        ThrowUnassigned(0);
    }
    return (*m_AnyContent);
}

inline
CAnyContentObject& CSoapBody::C_E::SetAnyContent(void)
{
    return (*m_AnyContent);
}

inline
bool CSoapBody::IsSet(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CSoapBody::CanGet(void) const
{
    return true;
}

inline
const std::list< CRef< CSoapBody::C_E > >& CSoapBody::Get(void) const
{
    return m_data;
}

inline
std::list< CRef< CSoapBody::C_E > >& CSoapBody::Set(void)
{
    m_set_State[0] |= 0x1;
    return m_data;
}

inline
CSoapBody::operator const std::list< CRef< CSoapBody::C_E > >& (void) const
{
    return m_data;
}

inline
CSoapBody::operator std::list< CRef< CSoapBody::C_E > >& (void)
{
    m_set_State[0] |= 0x1;
    return m_data;
}


///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////
END_NCBI_SCOPE

#endif // SOAP_BODY_HPP
