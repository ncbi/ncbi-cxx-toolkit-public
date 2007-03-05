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
*/

#include <corelib/ncbistd.hpp>


/** @addtogroup GenClassSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XSERIAL_EXPORT CSerializable
{
public:
    virtual ~CSerializable() { }

    enum EOutputType {
        eAsFasta, 
        eAsAsnText, 
        eAsAsnBinary, 
        eAsXML, 
        eAsString
    };

    class NCBI_XSERIAL_EXPORT CProxy {
    public:
        CProxy(const CSerializable& obj, EOutputType output_type)
            : m_Obj(obj), m_OutputType(output_type) { }

    private:
        const CSerializable& m_Obj;
        EOutputType          m_OutputType;
        friend NCBI_XSERIAL_EXPORT
        CNcbiOstream& operator << (CNcbiOstream& out, const CProxy& src);
    };

    CProxy Dump(EOutputType output_type) const;

protected:
    virtual void WriteAsFasta     (CNcbiOstream& out) const;
    virtual void WriteAsAsnText   (CNcbiOstream& out) const;
    virtual void WriteAsAsnBinary (CNcbiOstream& out) const;
    virtual void WriteAsXML       (CNcbiOstream& out) const;
    virtual void WriteAsString    (CNcbiOstream& out) const;

    friend NCBI_XSERIAL_EXPORT
    CNcbiOstream& operator << (CNcbiOstream& out, const CProxy& src);
};


inline
CSerializable::CProxy CSerializable::Dump(EOutputType output_type)
    const
{
    return CProxy(*this, output_type);
}


NCBI_XSERIAL_EXPORT
CNcbiOstream& operator << (CNcbiOstream& out,
                           const CSerializable::CProxy& src);


END_NCBI_SCOPE

/* @} */

#endif  /* SERIALIZABLE__HPP */
