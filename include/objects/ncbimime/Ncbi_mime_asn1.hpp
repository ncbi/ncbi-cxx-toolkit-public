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
 */

/// @file Ncbi_mime_asn1.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'ncbimime.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: Ncbi_mime_asn1_.hpp


#ifndef OBJECTS_NCBIMIME_NCBI_MIME_ASN1_HPP
#define OBJECTS_NCBIMIME_NCBI_MIME_ASN1_HPP


// generated includes
#include <objects/ncbimime/Ncbi_mime_asn1_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_NCBIMIME_EXPORT CNcbi_mime_asn1 : public CNcbi_mime_asn1_Base
{
    typedef CNcbi_mime_asn1_Base Tparent;
public:
    // constructor
    CNcbi_mime_asn1(void);
    // destructor
    ~CNcbi_mime_asn1(void);

    // Cn3D 4.1 can't handle the sid field in the molecule graph, so call this
    // function before writing data for export to Cn3D. Returns true if any SIDs
    // were removed; false if no changes were made.
    bool RemoveSIDs(void);

private:
    // Prohibit copy constructor and assignment operator
    CNcbi_mime_asn1(const CNcbi_mime_asn1& value);
    CNcbi_mime_asn1& operator=(const CNcbi_mime_asn1& value);

};

/////////////////// CNcbi_mime_asn1 inline methods

// constructor
inline
CNcbi_mime_asn1::CNcbi_mime_asn1(void)
{
}


/////////////////// end of CNcbi_mime_asn1 inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_NCBIMIME_NCBI_MIME_ASN1_HPP
/* Original file checksum: lines: 94, chars: 2684, CRC32: cdec7be8 */
