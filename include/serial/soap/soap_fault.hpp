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
 *   SOAP Fault object
 *
 * ===========================================================================
 */

#ifndef SOAP_FAULT_HPP
#define SOAP_FAULT_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>

#include <serial/soap/soap_code.hpp>
#include <serial/soap/soap_reason.hpp>
#include <serial/soap/soap_detail.hpp>
#include <serial/soap/soap_subcode.hpp>
#include <serial/soap/soap_text.hpp>


BEGIN_NCBI_SCOPE
// forward declarations
//class CSoapCode;
//class CSoapDetail;
//class CSoapReason;


// generated classes

class CSoapFault : public CSerialObject
{
    typedef CSerialObject Tparent;
public:
    // constructor
    CSoapFault(void);
    // destructor
    virtual ~CSoapFault(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef CSoapCode TSoapCode;
    typedef CSoapReason TSoapReason;
    typedef std::string TSoapNode;
    typedef std::string TSoapRole;
    typedef CSoapDetail TSoapDetail;

    // getters
    // setters

    // mandatory
    // typedef CSoapCode TSoapCode
    bool IsSetSoapCode(void) const;
    bool CanGetSoapCode(void) const;
    void ResetSoapCode(void);
    const TSoapCode& GetSoapCode(void) const;
    void SetSoapCode(TSoapCode& value);
    TSoapCode& SetSoapCode(void);

    // mandatory
    // typedef CSoapReason TSoapReason
    bool IsSetSoapReason(void) const;
    bool CanGetSoapReason(void) const;
    void ResetSoapReason(void);
    const TSoapReason& GetSoapReason(void) const;
    void SetSoapReason(TSoapReason& value);
    TSoapReason& SetSoapReason(void);

    // optional
    // typedef std::string TSoapNode
    bool IsSetSoapNode(void) const;
    bool CanGetSoapNode(void) const;
    void ResetSoapNode(void);
    const TSoapNode& GetSoapNode(void) const;
    void SetSoapNode(const TSoapNode& value);
    TSoapNode& SetSoapNode(void);

    // optional
    // typedef std::string TSoapRole
    bool IsSetSoapRole(void) const;
    bool CanGetSoapRole(void) const;
    void ResetSoapRole(void);
    const TSoapRole& GetSoapRole(void) const;
    void SetSoapRole(const TSoapRole& value);
    TSoapRole& SetSoapRole(void);

    // optional
    // typedef CSoapDetail TSoapDetail
    bool IsSetSoapDetail(void) const;
    bool CanGetSoapDetail(void) const;
    void ResetSoapDetail(void);
    const TSoapDetail& GetSoapDetail(void) const;
    void SetSoapDetail(TSoapDetail& value);
    TSoapDetail& SetSoapDetail(void);

    // reset whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CSoapFault(const CSoapFault&);
    CSoapFault& operator=(const CSoapFault&);

    // data
    Uint4 m_set_State[1];
    CRef< TSoapCode > m_SoapCode;
    CRef< TSoapReason > m_SoapReason;
    TSoapNode m_SoapNode;
    TSoapRole m_SoapRole;
    CRef< TSoapDetail > m_SoapDetail;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CSoapFault::IsSetSoapCode(void) const
{
    return m_SoapCode.NotEmpty();
}

inline
bool CSoapFault::CanGetSoapCode(void) const
{
    return IsSetSoapCode();
}

inline
const CSoapCode& CSoapFault::GetSoapCode(void) const
{
    if (!CanGetSoapCode()) {
        ThrowUnassigned(0);
    }
    return (*m_SoapCode);
}

inline
CSoapCode& CSoapFault::SetSoapCode(void)
{
    return (*m_SoapCode);
}

inline
bool CSoapFault::IsSetSoapReason(void) const
{
    return m_SoapReason.NotEmpty();
}

inline
bool CSoapFault::CanGetSoapReason(void) const
{
    return IsSetSoapReason();
}

inline
const CSoapReason& CSoapFault::GetSoapReason(void) const
{
    if (!CanGetSoapReason()) {
        ThrowUnassigned(1);
    }
    return (*m_SoapReason);
}

inline
CSoapReason& CSoapFault::SetSoapReason(void)
{
    return (*m_SoapReason);
}

inline
bool CSoapFault::IsSetSoapNode(void) const
{
    return ((m_set_State[0] & 0x30) != 0);
}

inline
bool CSoapFault::CanGetSoapNode(void) const
{
    return IsSetSoapNode();
}

inline
const std::string& CSoapFault::GetSoapNode(void) const
{
    if (!CanGetSoapNode()) {
        ThrowUnassigned(2);
    }
    return m_SoapNode;
}

inline
void CSoapFault::SetSoapNode(const std::string& value)
{
    m_SoapNode = value;
    m_set_State[0] |= 0x30;
}

inline
std::string& CSoapFault::SetSoapNode(void)
{
#ifdef _DEBUG
    if (!IsSetSoapNode()) {
        m_SoapNode = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x10;
    return m_SoapNode;
}

inline
bool CSoapFault::IsSetSoapRole(void) const
{
    return ((m_set_State[0] & 0xc0) != 0);
}

inline
bool CSoapFault::CanGetSoapRole(void) const
{
    return IsSetSoapRole();
}

inline
const std::string& CSoapFault::GetSoapRole(void) const
{
    if (!CanGetSoapRole()) {
        ThrowUnassigned(3);
    }
    return m_SoapRole;
}

inline
void CSoapFault::SetSoapRole(const std::string& value)
{
    m_SoapRole = value;
    m_set_State[0] |= 0xc0;
}

inline
std::string& CSoapFault::SetSoapRole(void)
{
#ifdef _DEBUG
    if (!IsSetSoapRole()) {
        m_SoapRole = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x40;
    return m_SoapRole;
}

inline
bool CSoapFault::IsSetSoapDetail(void) const
{
    return m_SoapDetail.NotEmpty();
}

inline
bool CSoapFault::CanGetSoapDetail(void) const
{
    return IsSetSoapDetail();
}

inline
const CSoapDetail& CSoapFault::GetSoapDetail(void) const
{
    if (!CanGetSoapDetail()) {
        ThrowUnassigned(4);
    }
    return (*m_SoapDetail);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////
END_NCBI_SCOPE


#endif // SOAP_FAULT_HPP
