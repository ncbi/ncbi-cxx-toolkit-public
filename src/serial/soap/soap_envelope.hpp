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
 *   Holds SOAP message contents
*/

#ifndef SOAP_ENVELOPE_HPP
#define SOAP_ENVELOPE_HPP

// standard includes
#include <serial/serialbase.hpp>

BEGIN_NCBI_SCOPE

// forward declarations
class CSoapBody;
class CSoapHeader;

// generated classes

class CSoapEnvelope : public CSerialObject
{
    typedef CSerialObject Tparent;
public:
    // constructor
    CSoapEnvelope(void);
    // destructor
    virtual ~CSoapEnvelope(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef CSoapHeader TSoapHeader;
    typedef CSoapBody TSoapBody;

    // getters
    // setters

    // optional
    // typedef CSoapHeader TSoapHeader
    bool IsSetHeader(void) const;
    bool CanGetHeader(void) const;
    void ResetHeader(void);
    const TSoapHeader& GetHeader(void) const;
    void SetHeader(TSoapHeader& value);
    TSoapHeader& SetHeader(void);

    // mandatory
    // typedef CSoapBody TSoapBody
    bool IsSetBody(void) const;
    bool CanGetBody(void) const;
    void ResetBody(void);
    const TSoapBody& GetBody(void) const;
    void SetBody(TSoapBody& value);
    TSoapBody& SetBody(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CSoapEnvelope(const CSoapEnvelope&);
    CSoapEnvelope& operator=(const CSoapEnvelope&);

    // data
    CRef< TSoapHeader > m_Header;
    CRef< TSoapBody > m_Body;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CSoapEnvelope::IsSetHeader(void) const
{
    return m_Header.NotEmpty();
}

inline
bool CSoapEnvelope::CanGetHeader(void) const
{
    return IsSetHeader();
}

inline
const CSoapHeader& CSoapEnvelope::GetHeader(void) const
{
    if (!CanGetHeader()) {
        ThrowUnassigned(0);
    }
    return (*m_Header);
}

inline
bool CSoapEnvelope::IsSetBody(void) const
{
    return m_Body.NotEmpty();
}

inline
bool CSoapEnvelope::CanGetBody(void) const
{
    return IsSetBody();
}

inline
const CSoapBody& CSoapEnvelope::GetBody(void) const
{
    if (!CanGetBody()) {
        ThrowUnassigned(1);
    }
    return (*m_Body);
}

inline
CSoapBody& CSoapEnvelope::SetBody(void)
{
    return (*m_Body);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////
END_NCBI_SCOPE


#endif // SOAP_ENVELOPE_HPP
