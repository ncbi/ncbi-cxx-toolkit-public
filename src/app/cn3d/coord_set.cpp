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
* Revision 1.6  2000/08/07 00:21:17  thiessen
* add display list mechanism
*
* Revision 1.5  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.4  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.3  2000/07/16 23:19:10  thiessen
* redo of drawing system
*
* Revision 1.2  2000/07/12 23:27:49  thiessen
* now draws basic CPK model
*
* Revision 1.1  2000/07/11 13:45:29  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#include <objects/mmdb2/Model_coordinate_set.hpp>
#include <objects/mmdb2/Coordinates.hpp>

#include "cn3d/coord_set.hpp"
#include "cn3d/atom_set.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE(Cn3D)

CoordSet::CoordSet(StructureBase *parent, 
                   const CBiostruc_model::TModel_coordinates& modelCoords) :
    StructureBase(parent), atomSet(NULL)
{
    // iterate SEQUENCE OF Model-coordinate-set
    CBiostruc_model::TModel_coordinates::const_iterator j, je=modelCoords.end();
    for (j=modelCoords.begin(); j!=je; j++) {
        const CModel_coordinate_set::C_Coordinates& 
            coordSet = j->GetObject().GetCoordinates();
        if (coordSet.IsLiteral()) {
            const CCoordinates& coords = coordSet.GetLiteral();
            if (coords.IsAtomic()) {
                if (!atomSet)
                    atomSet = new AtomSet(this, coords.GetAtomic());
                else
                    ERR_POST(Fatal << "confused by multiple atomic coords");
            }
            // will eventually unpack feature coordinates here
        }
    }
}

bool CoordSet::Draw(const AtomSet *atomSet) const
{
    // don't draw CoordSets directly
    return false;
}

END_SCOPE(Cn3D)

