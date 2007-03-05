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
 *   SOAP Subcode object
 *
 * ===========================================================================
 */

#ifndef SOAP_SUBCODE_HPP
#define SOAP_SUBCODE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


BEGIN_NCBI_SCOPE
// forward declarations
//class CSoapSubcode;


// generated classes

class CSoapSubcode : public CSerialObject
{
    typedef CSerialObject Tparent;
public:
    // constructor
    CSoapSubcode(void);
    // destructor
    virtual ~CSoapSubcode(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef std::string TSoapValue;
    typedef CSoapSubcode TSoapSubcode;

    // getters
    // setters

    // mandatory
    // typedef std::string TSoapValue
    bool IsSetSoapValue(void) const;
    bool CanGetSoapValue(void) const;
    void ResetSoapValue(void);
    const TSoapValue& GetSoapValue(void) const;
    void SetSoapValue(const TSoapValue& value);
    TSoapValue& SetSoapValue(void);

    // optional
    // typedef CSoapSubcode TSoapSubcode
    bool IsSetSoapSubcode(void) const;
    bool CanGetSoapSubcode(void) const;
    void ResetSoapSubcode(void);
    const TSoapSubcode& GetSoapSubcode(void) const;
    void SetSoapSubcode(TSoapSubcode& value);
    TSoapSubcode& SetSoapSubcode(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CSoapSubcode(const CSoapSubcode&);
    CSoapSubcode& operator=(const CSoapSubcode&);

    // data
    Uint4 m_set_State[1];
    TSoapValue m_SoapValue;
    CRef< TSoapSubcode > m_SoapSubcode;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CSoapSubcode::IsSetSoapValue(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CSoapSubcode::CanGetSoapValue(void) const
{
    return IsSetSoapValue();
}

inline
const std::string& CSoapSubcode::GetSoapValue(void) const
{
    if (!CanGetSoapValue()) {
        ThrowUnassigned(0);
    }
    return m_SoapValue;
}

inline
void CSoapSubcode::SetSoapValue(const std::string& value)
{
    m_SoapValue = value;
    m_set_State[0] |= 0x3;
}

inline
std::string& CSoapSubcode::SetSoapValue(void)
{
#ifdef _DEBUG
    if (!IsSetSoapValue()) {
        m_SoapValue = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x1;
    return m_SoapValue;
}

inline
bool CSoapSubcode::IsSetSoapSubcode(void) const
{
    return m_SoapSubcode.NotEmpty();
}

inline
bool CSoapSubcode::CanGetSoapSubcode(void) const
{
    return IsSetSoapSubcode();
}

inline
const CSoapSubcode& CSoapSubcode::GetSoapSubcode(void) const
{
    if (!CanGetSoapSubcode()) {
        ThrowUnassigned(1);
    }
    return (*m_SoapSubcode);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////
END_NCBI_SCOPE



#endif // SOAP_SUBCODE_HPP
