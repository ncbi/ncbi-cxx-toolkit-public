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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*
* ===========================================================================
*/

#ifndef GFF_UTIL__HPP
#define GFF_UTIL__HPP

#include <corelib/ncbifile.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class NCBI_XOBJIMPORT_EXPORT GffUtil 
{
public:
    static bool
    InitializeScore(
        const std::vector<std::string>&,
        bool&,
        double&);

    static bool
    InitializeFrame(
        const std::vector<std::string>&,
        std::string&);

    static CCdregion::TFrame PhaseToFrame(
        const std::string&);
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
