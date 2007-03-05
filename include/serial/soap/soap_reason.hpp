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
 *   SOAP Reason object
 *
 * ===========================================================================
 */

#ifndef SOAP_REASON_HPP
#define SOAP_REASON_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <list>


BEGIN_NCBI_SCOPE
// forward declarations
class CSoapText;


// generated classes

class CSoapReason : public CSerialObject
{
    typedef CSerialObject Tparent;
public:
    // constructor
    CSoapReason(void);
    // destructor
    virtual ~CSoapReason(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef std::list< CRef< CSoapText > > TSoapText;

    // getters
    // setters

    // mandatory
    // typedef std::list< CRef< CSoapText > > TSoapText
    bool IsSetSoapText(void) const;
    bool CanGetSoapText(void) const;
    void ResetSoapText(void);
    const TSoapText& GetSoapText(void) const;
    TSoapText& SetSoapText(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CSoapReason(const CSoapReason&);
    CSoapReason& operator=(const CSoapReason&);

    // data
    Uint4 m_set_State[1];
    TSoapText m_SoapText;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CSoapReason::IsSetSoapText(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CSoapReason::CanGetSoapText(void) const
{
    return true;
}

inline
const std::list< CRef< CSoapText > >& CSoapReason::GetSoapText(void) const
{
    return m_SoapText;
}

inline
std::list< CRef< CSoapText > >& CSoapReason::SetSoapText(void)
{
    m_set_State[0] |= 0x1;
    return m_SoapText;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////
END_NCBI_SCOPE


#endif // SOAP_REASON_HPP
