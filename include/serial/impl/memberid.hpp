#ifndef MEMBERID__HPP
#define MEMBERID__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>


/** @addtogroup FieldsComplex
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// CMemberId class holds information about logical object member access:
//     name and/or tag (ASN.1)
// default value of name is empty string
// default value of tag is eNoExplicitTag
class NCBI_XSERIAL_EXPORT CMemberId {
public:
    typedef int TTag;
    enum {
        eNoExplicitTag = -1,
        eParentTag = 30,
        eFirstTag = 0
    };

    CMemberId(void);
    CMemberId(TTag tag, bool explicitTag = true);
    CMemberId(const string& name);
    CMemberId(const string& name, TTag tag, bool explicitTag = true);
    CMemberId(const char* name);
    CMemberId(const char* name, TTag tag, bool explicitTag = true);
    ~CMemberId(void);

    const string& GetName(void) const;     // ASN.1 tag name
    TTag GetTag(void) const;               // ASN.1 effective binary tag value
    bool HaveExplicitTag(void) const;      // ASN.1 explicit binary tag value

    void SetName(const string& name);
    void SetTag(TTag tag, bool explicitTag = true);

    bool HaveParentTag(void) const;
    void SetParentTag(void);

    // return visible representation of CMemberId (as in ASN.1)
    string ToString(void) const;

    void SetNoPrefix(void);
    bool HaveNoPrefix(void) const;

    void SetAttlist(void);
    bool IsAttlist(void) const;

    void SetNotag(void);
    bool HasNotag(void) const;

    void SetAnyContent(void);
    bool HasAnyContent(void) const;

    void SetCompressed(void);
    bool IsCompressed(void) const;

    void SetNsQualified(bool qualified);
    ENsQualifiedMode IsNsQualified(void) const;

private:
    // identification
    string m_Name;
    TTag m_Tag;
    bool m_ExplicitTag;
    bool m_NoPrefix;
    bool m_Attlist;
    bool m_Notag;
    bool m_AnyContent;
    bool m_Compressed;
    ENsQualifiedMode m_NsqMode;
};


/* @} */


#include <serial/impl/memberid.inl>

END_NCBI_SCOPE

#endif  /* MEMBERID__HPP */
