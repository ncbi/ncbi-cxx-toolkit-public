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

/// @file GC_Replicon.hpp
/// User-defined methods of the data storage class.
///
/// This file was originally generated by application DATATOOL
/// using the following specifications:
/// 'genome_collection.asn'.
///
/// New methods or data members can be added to it if needed.
/// See also: GC_Replicon_.hpp


#ifndef INTERNAL_GPIPE_OBJECTS_GENOMECOLL_GC_REPLICON_HPP
#define INTERNAL_GPIPE_OBJECTS_GENOMECOLL_GC_REPLICON_HPP


// generated includes
#include <objects/genomecoll/GC_Replicon_.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/////////////////////////////////////////////////////////////////////////////
class NCBI_GENOME_COLLECTION_EXPORT CGC_Replicon : public CGC_Replicon_Base
{
    typedef CGC_Replicon_Base Tparent;
    friend class CGC_Assembly;
    friend class CGC_AssemblyUnit;
public:
    // constructor
    CGC_Replicon(void);
    // destructor
    ~CGC_Replicon(void);

    /// Access the assembly unit the sequence belongs to
    CConstRef<CGC_AssemblyUnit> GetAssemblyUnit() const;

    /// Access the most specific full assembly the replicon belongs to
    /// This is needed because assemblies are packaged as sets of assemblies;
    /// knowing the unit is not always enough
    CConstRef<CGC_Assembly>     GetFullAssembly() const;

    string GetMoleculeLocation() const;
    string GetMoleculeType() const;
    string GetMoleculeLabel() const;

protected:
    CGC_Assembly*     m_Assembly;
    CGC_AssemblyUnit* m_AssemblyUnit;

private:
    // Prohibit copy constructor and assignment operator
    CGC_Replicon(const CGC_Replicon& value);
    CGC_Replicon& operator=(const CGC_Replicon& value);

};

/////////////////// CGC_Replicon inline methods


/////////////////// end of CGC_Replicon inline methods


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // INTERNAL_GPIPE_OBJECTS_GENOMECOLL_GC_REPLICON_HPP
/* Original file checksum: lines: 86, chars: 2546, CRC32: 14b0f924 */
