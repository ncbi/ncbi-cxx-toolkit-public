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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1999/07/13 20:18:05  vasilche
* Changed types naming.
*
* Revision 1.2  1999/07/02 21:31:43  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.1  1999/06/30 16:04:23  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CMemberId {
public:
    typedef int TTag;

    CMemberId(void);
    CMemberId(const string& name);
    CMemberId(const string& name, TTag tag);
    CMemberId(TTag tag);

    string ToString(void) const;

    const string& GetName(void) const;
    void SetName(const string& name);

    TTag GetTag(void) const;
    CMemberId* SetTag(TTag tag);

    void SetNext(const CMemberId& id);
    bool operator==(const CMemberId& id) const;

private:
    // identification
    string m_Name;
    TTag m_Tag;
};

#include <serial/memberid.inl>

END_NCBI_SCOPE

#endif  /* MEMBERID__HPP */
