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
* Authors:  Paul Thiessen
*
* File Description:
*      Classes to hold sets of coordinates for atoms and features
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/07/11 13:49:28  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#ifndef CN3D_COORDSET__HPP
#define CN3D_COORDSET__HPP

#include <objects/mmdb2/Biostruc_model.hpp>

#include "cn3d/structure_base.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

class AtomSet;
class FeatureCoord;

// a CoordSet contains one set of atomic coordinates, plus any accompanying
// feature (helix, strand, ...) coordinates - basically the contents of
// an ASN1 Biostruc-model

class CoordSet : public StructureBase
{
public:
    CoordSet(StructureBase *parent, 
                const CBiostruc_model::TModel_coordinates& modelCoords);
    //~CoordSet(void);

    // public data
    AtomSet *atomSet;
    typedef LIST_TYPE < const FeatureCoord * > FeatureCoordList;
    FeatureCoordList featureCoords;

    // public methods
    bool Draw(void) const;

private:
};

END_SCOPE(Cn3D)

#endif // CN3D_COORDSET__HPP
