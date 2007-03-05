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
 *   SOAP Code object
 *
 * ===========================================================================
 */

#ifndef SOAP_CODE_HPP
#define SOAP_CODE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


BEGIN_NCBI_SCOPE

// forward declarations
class CSoapSubcode;


// generated classes

class CSoapCode : public CSerialObject
{
    typedef CSerialObject Tparent;
public:
    // constructor
    CSoapCode(void);
    // destructor
    virtual ~CSoapCode(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    enum ESoap_Faultcode {
        e_not_set = 0,
        eDataEncodingUnknown = 1,
        eMustUnderstand,
        eReceiver,
        eSender,
        eVersionMismatch
    };

    // types
    typedef std::string TSoapValue;
    typedef CSoapSubcode TSoapSubcode;

    // getters
    // setters

    bool IsSetFaultcode(void) const;
    bool CanGetFaultcode(void) const;
    void ResetFaultcode(void);
    ESoap_Faultcode GetFaultcode(void) const;
    void SetFaultcode(ESoap_Faultcode value);

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
    CSoapCode(const CSoapCode&);
    CSoapCode& operator=(const CSoapCode&);

    ESoap_Faultcode m_Faultcode;
    // data
    Uint4 m_set_State[1];
    std::string m_SoapValue;
    CRef< TSoapSubcode > m_SoapSubcode;

    static string x_FaultcodeToValue(ESoap_Faultcode code);
    static ESoap_Faultcode x_ValueToFaultcode(const string& value);
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////


inline
bool CSoapCode::IsSetFaultcode(void) const
{
    return IsSetSoapValue();
}

inline
bool CSoapCode::CanGetFaultcode(void) const
{
    return CanGetSoapValue();
}

inline
bool CSoapCode::IsSetSoapValue(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CSoapCode::CanGetSoapValue(void) const
{
    return IsSetSoapValue();
}

inline
const std::string& CSoapCode::GetSoapValue(void) const
{
    if (!CanGetSoapValue()) {
        ThrowUnassigned(0);
    }
    return m_SoapValue;
}

inline
void CSoapCode::SetSoapValue(const std::string& value)
{
    m_SoapValue = value;
    m_set_State[0] |= 0x3;
}

inline
std::string& CSoapCode::SetSoapValue(void)
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
bool CSoapCode::IsSetSoapSubcode(void) const
{
    return m_SoapSubcode.NotEmpty();
}

inline
bool CSoapCode::CanGetSoapSubcode(void) const
{
    return IsSetSoapSubcode();
}

inline
const CSoapSubcode& CSoapCode::GetSoapSubcode(void) const
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

#endif // SOAP_CODE_HPP
