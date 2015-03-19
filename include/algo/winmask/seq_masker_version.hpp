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
 * Authors:  Aleksandr Morgulis
 *
 */

#ifndef C_WIN_MASK_VERSION_HPP
#define C_WIN_MASK_VERSION_HPP

#include <corelib/version.hpp>

#include <string>
#include <sstream>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
class CSeqMaskerVersion : public CComponentVersionInfo {
public:

    CSeqMaskerVersion( std::string const & component_name,
                       int ver_major, int ver_minor, int patch_level,
                       std::string const & ver_pfx = "" )
        : CComponentVersionInfo( 
                component_name, ver_major, ver_minor, patch_level ),
          ver_pfx_( ver_pfx )
    {}

    CSeqMaskerVersion( CSeqMaskerVersion const & src )
        : CComponentVersionInfo( src ), ver_pfx_( src.ver_pfx_ )
    {}

    CSeqMaskerVersion & operator=( CSeqMaskerVersion const & src ) {
        CComponentVersionInfo::operator=( src );
        ver_pfx_ = src.ver_pfx_;
        return *this;
    }

    virtual std::string Print() const {
        std::ostringstream os;
        os << GetComponentName() << ':' << ver_pfx_ << CVersionInfo::Print();
        return os.str();
    }

private:

    std::string ver_pfx_;
};

END_NCBI_SCOPE

#endif

