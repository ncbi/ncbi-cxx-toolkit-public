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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/mmdb2/Model_coordinate_set.hpp>
#include <objects/mmdb2/Coordinates.hpp>
#include <objects/mmdb2/Surface_coordinates.hpp>
#include <objects/mmdb2/Model_descr.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb2/Surface_coordinates.hpp>

#include "coord_set.hpp"
#include "atom_set.hpp"
#include "object_3d.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

CoordSet::CoordSet(StructureBase *parent,
                   const CBiostruc_model::TModel_coordinates& modelCoords) :
    StructureBase(parent), atomSet(NULL)
{
    // iterate SEQUENCE OF Model-coordinate-set
    CBiostruc_model::TModel_coordinates::const_iterator j, je=modelCoords.end();
    for (j=modelCoords.begin(); j!=je; ++j) {
        const CModel_coordinate_set::C_Coordinates&
            coordSet = j->GetObject().GetCoordinates();
        if (coordSet.IsLiteral()) {
            const CCoordinates& coords = coordSet.GetLiteral();

            // coordinates of atoms
            if (coords.IsAtomic()) {
                if (!atomSet)
                    atomSet = new AtomSet(this, coords.GetAtomic());
                else
                    ERRORMSG("confused by multiple atomic coords");
            }

            // coordinates of 3d-objects
            else if (coords.IsSurface() && j->GetObject().IsSetDescr() &&
                j->GetObject().GetDescr().front().GetObject().IsOther_comment() &&
                coords.GetSurface().GetContents().IsResidues()) {

                const string& descr =
                    j->GetObject().GetDescr().front().GetObject().GetOther_comment();

                // helix cylinder
                if (descr == "helix" && coords.GetSurface().GetSurface().IsCylinder()) {
                    Helix3D *helix =
                        new Helix3D(this, coords.GetSurface().GetSurface().GetCylinder(),
                            coords.GetSurface().GetContents().GetResidues());
                    if (helix->moleculeID == Object3D::VALUE_NOT_SET) {
                        delete helix;
                        WARNINGMSG("bad helix");
                    } else
                        objectMap[helix->moleculeID].push_back(helix);

                // strand brick
                } else if (descr == "strand" && coords.GetSurface().GetSurface().IsBrick()) {
                    Strand3D *strand =
                        new Strand3D(this, coords.GetSurface().GetSurface().GetBrick(),
                            coords.GetSurface().GetContents().GetResidues());
                    if (strand->moleculeID == Object3D::VALUE_NOT_SET) {
                        delete strand;
                        WARNINGMSG("bad strand");
                    } else
                        objectMap[strand->moleculeID].push_back(strand);
                }
            }
        }
    }
}

END_SCOPE(Cn3D)
