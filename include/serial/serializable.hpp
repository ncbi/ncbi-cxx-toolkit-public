#ifndef SERIALIZABLE__HPP
#define SERIALIZABLE__HPP

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
* Author:  Michael Kholodov, Denis Vakatov
*
* File Description:
*   General serializable interface for different output formats
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/04/17 04:08:01  vakatov
* Redesigned from a pure interface (ISerializable) into a regular
* base class (CSerializable) to make its usage safer, more formal and
* less bulky.
*
* Revision 1.1  2001/04/12 17:01:04  kholodov
* General serializable interface for different output formats
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


class CSerializable
{
protected:
    enum EOutputType { eAsFasta, eAsAsnText, eAsAsnBinary, eAsXML };

    virtual void WriteAsFasta     (ostream& out) const;
    virtual void WriteAsAsnText   (ostream& out) const;
    virtual void WriteAsAsnBinary (ostream& out) const;
    virtual void WriteAsXML       (ostream& out) const;

    const CSerializable& Dump(EOutputType output_type) const;

private:
    mutable EOutputType m_OutputType;

    friend ostream& operator << (ostream& out, const CSerializable& src);
};



inline
const CSerializable& CSerializable::Dump(EOutputType output_type)
    const
{
    m_OutputType = output_type;
    return *this;
}


ostream& operator << (ostream& out, const CSerializable& src);


END_NCBI_SCOPE

#endif  /* SERIALIZABLE__HPP */
