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
 *   SOAP Text object
 *
 * ===========================================================================
 */

#ifndef SOAP_TEXT_HPP
#define SOAP_TEXT_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


BEGIN_NCBI_SCOPE
// generated classes

class CSoapText : public CSerialObject
{
    typedef CSerialObject Tparent;
public:
    // constructor
    CSoapText(void);
    CSoapText(const std::string& value);
    // destructor
    virtual ~CSoapText(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    class C_Attlist : public CSerialObject
    {
        typedef CSerialObject Tparent;
    public:
        // constructor
        C_Attlist(void);
        // destructor
        ~C_Attlist(void);
    
        // type info
        DECLARE_INTERNAL_TYPE_INFO();
    
        // types
        typedef std::string TXmllang;
    
        // getters
        // setters
    
        // mandatory
        // typedef std::string TXmllang
        bool IsSetXmllang(void) const;
        bool CanGetXmllang(void) const;
        void ResetXmllang(void);
        const TXmllang& GetXmllang(void) const;
        void SetXmllang(const TXmllang& value);
        TXmllang& SetXmllang(void);
    
        // reset whole object
        void Reset(void);
    
    
    private:
        // Prohibit copy constructor and assignment operator
        C_Attlist(const C_Attlist&);
        C_Attlist& operator=(const C_Attlist&);
    
        // data
        Uint4 m_set_State[1];
        TXmllang m_Xmllang;
    };
    // types
    typedef C_Attlist TAttlist;
    typedef std::string TSoapText;

    // getters
    // setters

    operator const std::string&(void) const;
    CSoapText& operator=(const std::string& value);

    // mandatory
    // typedef C_Attlist TAttlist
    bool IsSetAttlist(void) const;
    bool CanGetAttlist(void) const;
    void ResetAttlist(void);
    const TAttlist& GetAttlist(void) const;
    void SetAttlist(TAttlist& value);
    TAttlist& SetAttlist(void);

    // mandatory
    // typedef std::string TSoapText
    bool IsSetSoapText(void) const;
    bool CanGetSoapText(void) const;
    void ResetSoapText(void);
    const TSoapText& GetSoapText(void) const;
    void SetSoapText(const TSoapText& value);
    TSoapText& SetSoapText(void);

    // reset whole object
    virtual void Reset(void);

private:
    // Prohibit copy constructor and assignment operator
    CSoapText(const CSoapText&);
    CSoapText& operator=(const CSoapText&);

    // data
    Uint4 m_set_State[1];
    CRef< TAttlist > m_Attlist;
    TSoapText m_SoapText;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CSoapText::C_Attlist::IsSetXmllang(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CSoapText::C_Attlist::CanGetXmllang(void) const
{
    return IsSetXmllang();
}

inline
const std::string& CSoapText::C_Attlist::GetXmllang(void) const
{
    if (!CanGetXmllang()) {
        ThrowUnassigned(0);
    }
    return m_Xmllang;
}

inline
void CSoapText::C_Attlist::SetXmllang(const std::string& value)
{
    m_Xmllang = value;
    m_set_State[0] |= 0x3;
}

inline
std::string& CSoapText::C_Attlist::SetXmllang(void)
{
#ifdef _DEBUG
    if (!IsSetXmllang()) {
        m_Xmllang = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x1;
    return m_Xmllang;
}


inline
CSoapText::operator const std::string&(void) const
{
    return GetSoapText();
}

inline
CSoapText& CSoapText::operator=(const std::string& value)
{
    SetSoapText(value);
    return *this;
}

inline
bool CSoapText::IsSetAttlist(void) const
{
    return m_Attlist.NotEmpty();
}

inline
bool CSoapText::CanGetAttlist(void) const
{
    return IsSetAttlist();
}

inline
const CSoapText::C_Attlist& CSoapText::GetAttlist(void) const
{
    if (!CanGetAttlist()) {
        ThrowUnassigned(0);
    }
    return (*m_Attlist);
}

inline
CSoapText::C_Attlist& CSoapText::SetAttlist(void)
{
    return (*m_Attlist);
}

inline
bool CSoapText::IsSetSoapText(void) const
{
    return ((m_set_State[0] & 0xc) != 0);
}

inline
bool CSoapText::CanGetSoapText(void) const
{
    return IsSetSoapText();
}

inline
const std::string& CSoapText::GetSoapText(void) const
{
    if (!CanGetSoapText()) {
        ThrowUnassigned(1);
    }
    return m_SoapText;
}

inline
void CSoapText::SetSoapText(const std::string& value)
{
    m_SoapText = value;
    m_set_State[0] |= 0xc;
}

inline
std::string& CSoapText::SetSoapText(void)
{
#ifdef _DEBUG
    if (!IsSetSoapText()) {
        m_SoapText = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x4;
    return m_SoapText;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////

END_NCBI_SCOPE


#endif // SOAP_TEXT_HPP
