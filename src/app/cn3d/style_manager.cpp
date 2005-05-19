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
*      manager object to track drawing style of objects at various levels
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/cn3d/Cn3d_backbone_style.hpp>
#include <objects/cn3d/Cn3d_general_style.hpp>
#include <objects/cn3d/Cn3d_backbone_label_style.hpp>
#include <objects/cn3d/Cn3d_color.hpp>
#include <objects/cn3d/Cn3d_style_table_item.hpp>
#include <objects/cn3d/Cn3d_style_table_id.hpp>
#include <objects/cn3d/Cn3d_user_annotation.hpp>
#include <objects/cn3d/Cn3d_object_location.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/cn3d/Cn3d_molecule_location.hpp>
#include <objects/cn3d/Cn3d_residue_range.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/mmdb1/Molecule_id.hpp>

#include <memory>
#include <string.h> // for memcpy()

#include "style_manager.hpp"
#include "structure_set.hpp"
#include "chemical_graph.hpp"
#include "residue.hpp"
#include "periodic_table.hpp"
#include "bond.hpp"
#include "show_hide_manager.hpp"
#include "object_3d.hpp"
#include "alignment_manager.hpp"
#include "messenger.hpp"
#include "cn3d_colors.hpp"
#include "style_dialog.hpp"
#include "annotate_dialog.hpp"
#include "molecule_identifier.hpp"
#include "atom_set.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

///// StyleSettings stuff /////

static void Vector2ASNColor(const Vector &vec, CCn3d_color *asnColor)
{
    static const int SCALE = 10000;
    asnColor->SetScale_factor(SCALE);
    asnColor->SetRed((int) (vec[0] * SCALE));
    asnColor->SetGreen((int) (vec[1] * SCALE));
    asnColor->SetBlue((int) (vec[2] * SCALE));
    asnColor->SetAlpha(SCALE);  // no alpha in Cn3D's colors
}

static bool SaveBackboneStyleToASN(
    const StyleSettings::BackboneStyle bbSettings, CCn3d_backbone_style *bbASN)
{
    // these casts rely on correspondence of enumerated values!
    bbASN->SetType((ECn3d_backbone_type) bbSettings.type);
    bbASN->SetStyle((ECn3d_drawing_style) bbSettings.style);
    bbASN->SetColor_scheme((ECn3d_color_scheme) bbSettings.colorScheme);
    Vector2ASNColor(bbSettings.userColor, &(bbASN->SetUser_color()));
    return true;
}

static bool SaveGeneralStyleToASN(
    const StyleSettings::GeneralStyle gSettings, CCn3d_general_style *gASN)
{
    // these casts rely on correspondence of enumerated values!
    gASN->SetIs_on(gSettings.isOn);
    gASN->SetStyle((ECn3d_drawing_style) gSettings.style);
    gASN->SetColor_scheme((ECn3d_color_scheme) gSettings.colorScheme);
    Vector2ASNColor(gSettings.userColor, &(gASN->SetUser_color()));
    return true;
}

static bool SaveLabelStyleToASN(
    const StyleSettings::LabelStyle lSettings, CCn3d_backbone_label_style *lASN)
{
    lASN->SetSpacing(lSettings.spacing);
    // these casts rely on correspondence of enumerated values!
    lASN->SetType((CCn3d_backbone_label_style::EType) lSettings.type);
    lASN->SetNumber((CCn3d_backbone_label_style::ENumber) lSettings.numbering);
    lASN->SetTermini(lSettings.terminiOn);
    lASN->SetWhite(lSettings.white);
    return true;
}

bool StyleSettings::SaveSettingsToASN(CCn3d_style_settings *styleASN) const
{
    styleASN->SetVirtual_disulfides_on(virtualDisulfidesOn);
    Vector2ASNColor(virtualDisulfideColor, &(styleASN->SetVirtual_disulfide_color()));
    styleASN->SetHydrogens_on(hydrogensOn);
    Vector2ASNColor(backgroundColor, &(styleASN->SetBackground_color()));
    styleASN->SetIon_labels(ionLabelsOn);

    static const int SCALE = 10000;
    styleASN->SetScale_factor(SCALE);
    styleASN->SetSpace_fill_proportion((int) (spaceFillProportion * SCALE));
    styleASN->SetBall_radius((int) (ballRadius * SCALE));
    styleASN->SetStick_radius((int) (stickRadius * SCALE));
    styleASN->SetTube_radius((int) (tubeRadius * SCALE));
    styleASN->SetTube_worm_radius((int) (tubeWormRadius * SCALE));
    styleASN->SetHelix_radius((int) (helixRadius * SCALE));
    styleASN->SetStrand_width((int) (strandWidth * SCALE));
    styleASN->SetStrand_thickness((int) (strandThickness * SCALE));

    return (
        SaveBackboneStyleToASN(proteinBackbone, &(styleASN->SetProtein_backbone())) &&
        SaveBackboneStyleToASN(nucleotideBackbone, &(styleASN->SetNucleotide_backbone())) &&
        SaveGeneralStyleToASN(proteinSidechains, &(styleASN->SetProtein_sidechains())) &&
        SaveGeneralStyleToASN(nucleotideSidechains, &(styleASN->SetNucleotide_sidechains())) &&
        SaveGeneralStyleToASN(heterogens, &(styleASN->SetHeterogens())) &&
        SaveGeneralStyleToASN(solvents, &(styleASN->SetSolvents())) &&
        SaveGeneralStyleToASN(connections, &(styleASN->SetConnections())) &&
        SaveGeneralStyleToASN(helixObjects, &(styleASN->SetHelix_objects())) &&
        SaveGeneralStyleToASN(strandObjects, &(styleASN->SetStrand_objects())) &&
        SaveLabelStyleToASN(proteinLabels, &(styleASN->SetProtein_labels())) &&
        SaveLabelStyleToASN(nucleotideLabels, &(styleASN->SetNucleotide_labels()))
    );
}

static void ASNColor2Vector(const CCn3d_color& asnColor, Vector *vec)
{
    int SCALE = asnColor.GetScale_factor();
    vec->Set(
        1.0 * asnColor.GetRed() / SCALE,
        1.0 * asnColor.GetGreen() / SCALE,
        1.0 * asnColor.GetBlue() / SCALE
    );
    // no alpha in Cn3D's colors
}

static bool LoadBackboneStyleFromASN(
    const CCn3d_backbone_style& bbASN, StyleSettings::BackboneStyle *bbSettings)
{
    // these casts rely on correspondence of enumerated values!
    bbSettings->type = (StyleSettings::eBackboneType) bbASN.GetType();
    bbSettings->style = (StyleSettings::eDrawingStyle) bbASN.GetStyle();
    bbSettings->colorScheme = (StyleSettings::eColorScheme) bbASN.GetColor_scheme();
    ASNColor2Vector(bbASN.GetUser_color(), &(bbSettings->userColor));
    return true;
}

static bool LoadGeneralStyleFromASN(
    const CCn3d_general_style& gASN, StyleSettings::GeneralStyle *gSettings)
{
    // these casts rely on correspondence of enumerated values!
    gSettings->isOn = gASN.GetIs_on();
    gSettings->style = (StyleSettings::eDrawingStyle) gASN.GetStyle();
    gSettings->colorScheme = (StyleSettings::eColorScheme) gASN.GetColor_scheme();
    ASNColor2Vector(gASN.GetUser_color(), &(gSettings->userColor));
    return true;
}

static bool LoadLabelStyleFromASN(
    const CCn3d_backbone_label_style& lASN, StyleSettings::LabelStyle *lSettings)
{
    lSettings->spacing = lASN.GetSpacing();
    // these casts rely on correspondence of enumerated values!
    lSettings->type = (StyleSettings::eLabelType) lASN.GetType();
    lSettings->numbering = (StyleSettings::eNumberType) lASN.GetNumber();
    lSettings->terminiOn = lASN.GetTermini();
    lSettings->white = lASN.GetWhite();
    return true;
}

static void SetDefaultLabelStyle(StyleSettings::LabelStyle *lStyle)
{
    lStyle->spacing = 0;
    lStyle->type = StyleSettings::eThreeLetter;
    lStyle->numbering = StyleSettings::eSequentialNumbering;
    lStyle->terminiOn = false;
    lStyle->white = true;
}

bool StyleSettings::LoadSettingsFromASN(const CCn3d_style_settings& styleASN)
{
    virtualDisulfidesOn = styleASN.GetVirtual_disulfides_on();
    ASNColor2Vector(styleASN.GetVirtual_disulfide_color(), &virtualDisulfideColor);
    hydrogensOn = styleASN.GetHydrogens_on();
    ASNColor2Vector(styleASN.GetBackground_color(), &backgroundColor);

    int SCALE = styleASN.GetScale_factor();
    spaceFillProportion = 1.0 * styleASN.GetSpace_fill_proportion() / SCALE;
    stickRadius = 1.0 * styleASN.GetStick_radius() / SCALE;
    tubeRadius = 1.0 * styleASN.GetTube_radius() / SCALE;
    tubeWormRadius = 1.0 * styleASN.GetTube_worm_radius() / SCALE;
    helixRadius = 1.0 * styleASN.GetHelix_radius() / SCALE;
    strandWidth = 1.0 * styleASN.GetStrand_width() / SCALE;
    strandThickness = 1.0 * styleASN.GetStrand_thickness() / SCALE;

    // label defaults (since they're optional in ASN spec)
    if (styleASN.IsSetProtein_labels()) {
        if (!LoadLabelStyleFromASN(styleASN.GetProtein_labels(), &proteinLabels)) return false;
    } else
        SetDefaultLabelStyle(&proteinLabels);
    if (styleASN.IsSetNucleotide_labels()) {
        if (!LoadLabelStyleFromASN(styleASN.GetNucleotide_labels(), &nucleotideLabels)) return false;
    } else
        SetDefaultLabelStyle(&nucleotideLabels);
    // ion labels on by default
    ionLabelsOn = (styleASN.IsSetIon_labels()) ? styleASN.GetIon_labels() : true;

    return (
        LoadBackboneStyleFromASN(styleASN.GetProtein_backbone(), &proteinBackbone) &&
        LoadBackboneStyleFromASN(styleASN.GetNucleotide_backbone(), &nucleotideBackbone) &&
        LoadGeneralStyleFromASN(styleASN.GetProtein_sidechains(), &proteinSidechains) &&
        LoadGeneralStyleFromASN(styleASN.GetNucleotide_sidechains(), &nucleotideSidechains) &&
        LoadGeneralStyleFromASN(styleASN.GetHeterogens(), &heterogens) &&
        LoadGeneralStyleFromASN(styleASN.GetSolvents(), &solvents) &&
        LoadGeneralStyleFromASN(styleASN.GetConnections(), &connections) &&
        LoadGeneralStyleFromASN(styleASN.GetHelix_objects(), &helixObjects) &&
        LoadGeneralStyleFromASN(styleASN.GetStrand_objects(), &strandObjects)
    );
}

StyleSettings& StyleSettings::operator = (const StyleSettings& orig)
{
    memcpy(this, &orig, sizeof(StyleSettings));
    return *this;
}

void StyleSettings::SetRenderingStyle(ePredefinedRenderingStyle style)
{
    // variable settings
    switch (style) {

        // set sidechains isOn only
        case eToggleSidechainsShortcut:
            proteinSidechains.isOn = !proteinSidechains.isOn;
            nucleotideSidechains.isOn = !nucleotideSidechains.isOn;
            return;

        case eWormShortcut:
            proteinBackbone.type = nucleotideBackbone.type = eTrace;
            proteinBackbone.style = nucleotideBackbone.style = eTubeWorm;
            proteinSidechains.isOn = nucleotideSidechains.isOn = false;
            proteinSidechains.style = nucleotideSidechains.style = eWire;
            heterogens.style = eBallAndStick;
            solvents.isOn = false;
            solvents.style = eBallAndStick;
            connections.style = eTubes;
            helixObjects.isOn = strandObjects.isOn = true;
            helixObjects.style = strandObjects.style = eWithArrows;
            break;

        case eTubeShortcut:
            proteinBackbone.type = nucleotideBackbone.type = eTrace;
            proteinBackbone.style = nucleotideBackbone.style = eTubes;
            proteinSidechains.isOn = nucleotideSidechains.isOn = false;
            proteinSidechains.style = nucleotideSidechains.style = eWire;
            heterogens.style = eBallAndStick;
            solvents.isOn = false;
            solvents.style = eBallAndStick;
            connections.style = eTubes;
            helixObjects.isOn = strandObjects.isOn = false;
            helixObjects.style = strandObjects.style = eWithArrows;
            break;

        case eWireframeShortcut:
            proteinBackbone.type = nucleotideBackbone.type = eComplete;
            proteinBackbone.style = nucleotideBackbone.style = eWire;
            proteinSidechains.isOn = nucleotideSidechains.isOn = true;
            proteinSidechains.style = nucleotideSidechains.style = eWire;
            heterogens.style = eWire;
            solvents.isOn = false;
            solvents.style = eBallAndStick;
            connections.style = eWire;
            helixObjects.isOn = strandObjects.isOn = false;
            helixObjects.style = strandObjects.style = eWithArrows;
            break;

        case eBallAndStickShortcut:
            proteinBackbone.type = nucleotideBackbone.type = eComplete;
            proteinBackbone.style = nucleotideBackbone.style = eBallAndStick;
            proteinSidechains.isOn = nucleotideSidechains.isOn = true;
            proteinSidechains.style = nucleotideSidechains.style = eBallAndStick;
            heterogens.style = eBallAndStick;
            solvents.isOn = false;
            solvents.style = eBallAndStick;
            connections.style = eTubes;
            helixObjects.isOn = strandObjects.isOn = false;
            helixObjects.style = strandObjects.style = eWithArrows;
            break;

        case eSpacefillShortcut:
            proteinBackbone.type = nucleotideBackbone.type = eComplete;
            proteinBackbone.style = nucleotideBackbone.style = eSpaceFill;
            proteinSidechains.isOn = nucleotideSidechains.isOn = true;
            proteinSidechains.style = nucleotideSidechains.style = eSpaceFill;
            heterogens.style = eSpaceFill;
            solvents.isOn = false;
            solvents.style = eSpaceFill;
            connections.style = eTubes;
            helixObjects.isOn = strandObjects.isOn = false;
            helixObjects.style = strandObjects.style = eWithArrows;
            break;
    }

    // common settings
    heterogens.isOn = true;
    connections.isOn = true;
    virtualDisulfidesOn = true;
    hydrogensOn = true;
    helixRadius = 1.8;
    strandWidth = 2.0;
    strandThickness = 0.5;
    spaceFillProportion = 1.0;
    ballRadius = 0.4;
    stickRadius = 0.2;
    tubeRadius = 0.3;
    tubeWormRadius = 0.3;
}

void StyleSettings::SetColorScheme(ePredefinedColorScheme scheme)
{
    // variable settings
    switch (scheme) {
        case eSecondaryStructureShortcut:
            proteinBackbone.colorScheme = eSecondaryStructure;
            nucleotideBackbone.colorScheme = eMolecule;
            proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eElement;
            heterogens.colorScheme = solvents.colorScheme = eElement;
            helixObjects.colorScheme = strandObjects.colorScheme = eSecondaryStructure;
            break;

        case eAlignedShortcut: case eIdentityShortcut: case eVarietyShortcut:
        case eWeightedVarietyShortcut: case eInformationContentShortcut:
        case eFitShortcut: case eBlockFitShortcut: case eBlockZFitShortcut: case eBlockRowFitShortcut:
            switch (scheme) {
                case eAlignedShortcut: proteinBackbone.colorScheme = eAligned; break;
                case eIdentityShortcut: proteinBackbone.colorScheme = eIdentity; break;
                case eVarietyShortcut: proteinBackbone.colorScheme = eVariety; break;
                case eWeightedVarietyShortcut: proteinBackbone.colorScheme = eWeightedVariety; break;
                case eInformationContentShortcut: proteinBackbone.colorScheme = eInformationContent; break;
                case eFitShortcut: proteinBackbone.colorScheme = eFit; break;
                case eBlockFitShortcut: proteinBackbone.colorScheme = eBlockFit; break;
                case eBlockZFitShortcut: proteinBackbone.colorScheme = eBlockZFit; break;
                case eBlockRowFitShortcut: proteinBackbone.colorScheme = eBlockRowFit; break;
            }
            nucleotideBackbone.colorScheme = eMolecule;
            proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eElement;
            heterogens.colorScheme = solvents.colorScheme = eElement;
            helixObjects.colorScheme = strandObjects.colorScheme = eObject;
            break;

        case eObjectShortcut:
            proteinBackbone.colorScheme = eObject;
            nucleotideBackbone.colorScheme = eObject;
            proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eObject;
            heterogens.colorScheme = solvents.colorScheme = eObject;
            helixObjects.colorScheme = strandObjects.colorScheme = eObject;
            break;

        case eDomainShortcut:
            proteinBackbone.colorScheme = eDomain;
            nucleotideBackbone.colorScheme = eDomain;
            proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eElement;
            heterogens.colorScheme = solvents.colorScheme = eElement;
            helixObjects.colorScheme = strandObjects.colorScheme = eDomain;
            break;

        case eMoleculeShortcut:
            proteinBackbone.colorScheme = eMolecule;
            nucleotideBackbone.colorScheme = eMolecule;
            proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eMolecule;
            heterogens.colorScheme = solvents.colorScheme = eMolecule;
            helixObjects.colorScheme = strandObjects.colorScheme = eMolecule;
            break;

        case eRainbowShortcut:
            proteinBackbone.colorScheme = eRainbow;
            nucleotideBackbone.colorScheme = eRainbow;
            proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eRainbow;
            heterogens.colorScheme = solvents.colorScheme = eElement;
            helixObjects.colorScheme = strandObjects.colorScheme = eObject;
            break;

        case eHydrophobicityShortcut:
            proteinBackbone.colorScheme = eHydrophobicity;
            nucleotideBackbone.colorScheme = eMolecule;
            proteinSidechains.colorScheme = eHydrophobicity;
            nucleotideSidechains.colorScheme = eElement;
            heterogens.colorScheme = solvents.colorScheme = eElement;
            helixObjects.colorScheme = strandObjects.colorScheme = eObject;
            break;

        case eChargeShortcut:
            proteinBackbone.colorScheme = eCharge;
            nucleotideBackbone.colorScheme = eElement;
            proteinSidechains.colorScheme = eCharge;
            nucleotideSidechains.colorScheme = eElement;
            heterogens.colorScheme = solvents.colorScheme = eElement;
            helixObjects.colorScheme = strandObjects.colorScheme = eObject;
            break;

        case eTemperatureShortcut:
            proteinBackbone.colorScheme = eTemperature;
            nucleotideBackbone.colorScheme = eTemperature;
            proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eTemperature;
            heterogens.colorScheme = solvents.colorScheme = eTemperature;
            helixObjects.colorScheme = strandObjects.colorScheme = eObject;
            break;

        case eElementShortcut:
            proteinBackbone.colorScheme = eElement;
            nucleotideBackbone.colorScheme = eElement;
            proteinSidechains.colorScheme = nucleotideSidechains.colorScheme = eElement;
            heterogens.colorScheme = eElement;
            solvents.colorScheme = eElement;
            helixObjects.colorScheme = strandObjects.colorScheme = eObject;
            break;
    }

    // common settings
    connections.colorScheme = eUserSelect;
    connections.userColor.Set(0.9,0.9,1);
    virtualDisulfideColor.Set(0.93,0.55,0.05);
    backgroundColor.Set(0,0,0);

    proteinSidechains.userColor = nucleotideSidechains.userColor =
    proteinBackbone.userColor = nucleotideBackbone.userColor =
    heterogens.userColor = solvents.userColor =
    helixObjects.userColor = strandObjects.userColor = Vector(0.5,0.5,0.5);
}

void StyleSettings::SetDefaultLabeling(void)
{
    SetDefaultLabelStyle(&proteinLabels);
    SetDefaultLabelStyle(&nucleotideLabels);
    ionLabelsOn = true;
}


///// StyleManager stuff /////

StyleManager::StyleManager(const StructureSet *set) : structureSet(set)
{
}

bool StyleManager::CheckGlobalStyleSettings()
{
    return CheckStyleSettings(&globalStyle);
}

// check for inconsistencies in style settings; returns false if there's an uncorrectable problem
bool StyleManager::CheckStyleSettings(StyleSettings *settings)
{
    // can't do worm with partial or complete backbone
    if (((settings->proteinBackbone.style == StyleSettings::eWireWorm ||
          settings->proteinBackbone.style == StyleSettings::eTubeWorm) &&
         (settings->proteinBackbone.type == StyleSettings::ePartial ||
          settings->proteinBackbone.type == StyleSettings::eComplete))) {
        settings->proteinBackbone.type = StyleSettings::eTrace;
    }
    if (((settings->nucleotideBackbone.style == StyleSettings::eWireWorm ||
          settings->nucleotideBackbone.style == StyleSettings::eTubeWorm) &&
         (settings->nucleotideBackbone.type == StyleSettings::ePartial ||
          settings->nucleotideBackbone.type == StyleSettings::eComplete))) {
        settings->nucleotideBackbone.type = StyleSettings::eTrace;
    }

    // can't do non-trace backbones for ncbi-backbone models
    if (structureSet->isAlphaOnly) {
        if (settings->proteinBackbone.type == StyleSettings::ePartial ||
            settings->proteinBackbone.type == StyleSettings::eComplete) {
            settings->proteinBackbone.type = StyleSettings::eTrace;
        }
        if (settings->nucleotideBackbone.type == StyleSettings::ePartial ||
            settings->nucleotideBackbone.type == StyleSettings::eComplete) {
            settings->nucleotideBackbone.type = StyleSettings::eTrace;
        }
    }

    return true;
}

const double UNKNOWN_HYDROPHOBICITY = -1.0;

// return a hydrophobicity value from [0..1]
double GetHydrophobicity(char code)
{
    // Amino acid scale: Normalized consensus hydrophobicity scale.
    // Author(s): Eisenberg D., Schwarz E., Komarony M., Wall R.
    // Reference: J. Mol. Biol. 179:125-142(1984).
    // Amino acid scale values: (normalized to [0..1])
    switch (code) {
        case 'A': return ( 0.620 + 2.530) / (1.380 + 2.530);
        case 'R': return (-2.530 + 2.530) / (1.380 + 2.530);
        case 'N': return (-0.780 + 2.530) / (1.380 + 2.530);
        case 'D': return (-0.900 + 2.530) / (1.380 + 2.530);
        case 'C': return ( 0.290 + 2.530) / (1.380 + 2.530);
        case 'Q': return (-0.850 + 2.530) / (1.380 + 2.530);
        case 'E': return (-0.740 + 2.530) / (1.380 + 2.530);
        case 'G': return ( 0.480 + 2.530) / (1.380 + 2.530);
        case 'H': return (-0.400 + 2.530) / (1.380 + 2.530);
        case 'I': return ( 1.380 + 2.530) / (1.380 + 2.530);
        case 'L': return ( 1.060 + 2.530) / (1.380 + 2.530);
        case 'K': return (-1.500 + 2.530) / (1.380 + 2.530);
        case 'M': return ( 0.640 + 2.530) / (1.380 + 2.530);
        case 'F': return ( 1.190 + 2.530) / (1.380 + 2.530);
        case 'P': return ( 0.120 + 2.530) / (1.380 + 2.530);
        case 'S': return (-0.180 + 2.530) / (1.380 + 2.530);
        case 'T': return (-0.050 + 2.530) / (1.380 + 2.530);
        case 'W': return ( 0.810 + 2.530) / (1.380 + 2.530);
        case 'Y': return ( 0.260 + 2.530) / (1.380 + 2.530);
        case 'V': return ( 1.080 + 2.530) / (1.380 + 2.530);
    }
    return UNKNOWN_HYDROPHOBICITY;
}

int GetCharge(char code)
{
    switch (code) {
        case 'R': case 'H': case 'K': return 1;
        case 'D': case 'E': return -1;
    }
    return 0;
}

#define ATOM_NOT_DISPLAYED do { \
    atomStyle->style = eNotDisplayed; \
    return true; } while (0)

// get display style for atom, including show/hide status.
// May want to cache this eventually, since a
// particular atom's style may be queried several times per render (once for
// drawing atoms, and once for each bond to the atom).
bool StyleManager::GetAtomStyle(const Residue *residue,
    const AtomPntr& atom, const AtomCoord *coord,
    AtomStyle *atomStyle,
    const StyleSettings::BackboneStyle* *saveBackboneStyle,
    const StyleSettings::GeneralStyle* *saveGeneralStyle) const
{
    if (!residue || !atomStyle) {
        ERRORMSG("StyleManager::GetAtomStyle() got NULL residue or atomStyle");
        return false;
    }
    atomStyle->isHighlighted = false; // queried sometimes even if atom not displayed

    const Molecule *molecule;
    if (!residue->GetParentOfType(&molecule)) return false;

    const StructureObject *object;
    if (!molecule->GetParentOfType(&object)) return false;

    const StyleSettings& settings = GetStyleForResidue(object, atom.mID, atom.rID);
    const Residue::AtomInfo *info = residue->GetAtomInfo(atom.aID);
    if (!info)
        ATOM_NOT_DISPLAYED;

    // set up some pointers for more convenient access to style settings
    const StyleSettings::BackboneStyle *backboneStyle = NULL;
    const StyleSettings::GeneralStyle *generalStyle = NULL;
    if (info->classification == Residue::eAlphaBackboneAtom ||
        info->classification == Residue::ePartialBackboneAtom ||
        info->classification == Residue::eCompleteBackboneAtom) {
        if (residue->IsAminoAcid())
            backboneStyle = &(settings.proteinBackbone);
        else
            backboneStyle = &(settings.nucleotideBackbone);

    } else if (info->classification == Residue::eSideChainAtom) {
        if (residue->IsAminoAcid())
            generalStyle = &(settings.proteinSidechains);
        else
            generalStyle = &(settings.nucleotideSidechains);

    } else { // Residue::eUnknownAtom
        if (molecule->IsSolvent())
            generalStyle = &(settings.solvents);
        else if (molecule->IsHeterogen())
            generalStyle = &(settings.heterogens);
        else {
            ERRORMSG("StyleManager::GetAtomStyle() - confused about molecule/atom classification");
            return false;
        }
    }
    if ((!backboneStyle && !generalStyle) || (backboneStyle && generalStyle)) {
        ERRORMSG("StyleManager::GetAtomStyle() - confused about style settings");
        return false;
    }
    if (saveBackboneStyle) *saveBackboneStyle = backboneStyle;
    if (saveGeneralStyle) *saveGeneralStyle = generalStyle;

    // first check whether this atom is visible, based on show/hide and backbone and sidechain settings
    if (object->parentSet->showHideManager->IsHidden(residue))
        ATOM_NOT_DISPLAYED;

    if (info->atomicNumber == 1 && !settings.hydrogensOn)
        ATOM_NOT_DISPLAYED;

    if (info->classification == Residue::eSideChainAtom && !generalStyle->isOn)
       ATOM_NOT_DISPLAYED;

    if (info->classification == Residue::eAlphaBackboneAtom ||
        info->classification == Residue::ePartialBackboneAtom ||
        info->classification == Residue::eCompleteBackboneAtom) { // is backbone of some sort

        // control presence of non CA/C1* backbone atoms
        if ((residue->IsAminoAcid() && info->classification != Residue::eAlphaBackboneAtom) ||
            (residue->IsNucleotide() && info->code != " C1*")) {

            // skip if backbone off
            if (backboneStyle->type == StyleSettings::eOff)
                ATOM_NOT_DISPLAYED;

            // show only alpha atom if eTrace
            if (backboneStyle->type == StyleSettings::eTrace &&
                info->classification != Residue::eAlphaBackboneAtom)
                ATOM_NOT_DISPLAYED;

            // don't show complete backbone if set to partial
            if (info->classification == Residue::eCompleteBackboneAtom &&
                backboneStyle->type == StyleSettings::ePartial)
                ATOM_NOT_DISPLAYED;
        }

        // if this is alpha/C1* and backbone is off, but sidechains are on, then
        // let the atom be visible *and* take the style of the sidechain
        else if (backboneStyle->type == StyleSettings::eOff ||
                 (residue->IsNucleotide() && backboneStyle->type != StyleSettings::eComplete)) {

            const StyleSettings::GeneralStyle *sidechainStyle = NULL;
            if (residue->IsAminoAcid()) sidechainStyle = &(settings.proteinSidechains);
            else if (residue->IsNucleotide()) sidechainStyle = &(settings.nucleotideSidechains);

            if (sidechainStyle && sidechainStyle->isOn) {
                backboneStyle = NULL;
                generalStyle = sidechainStyle;
            } else
                ATOM_NOT_DISPLAYED;
        }
    }

    if (info->classification == Residue::eUnknownAtom && !generalStyle->isOn)
        ATOM_NOT_DISPLAYED;

    const Element *element = PeriodicTable.GetElement(info->atomicNumber);

    // determine radius
    switch (backboneStyle ? backboneStyle->style : generalStyle->style) {
        case StyleSettings::eWire:
        case StyleSettings::eWireWorm:
        case StyleSettings::eTubeWorm:
            // no atom, but don't do ATOM_NOT_DISPLAYED, because bonds to this atom
            // still may be displayed and need style info about this atom
            atomStyle->radius = 0.0;
            break;
        case StyleSettings::eTubes:
            atomStyle->radius = settings.tubeRadius;
            break;
        case StyleSettings::eBallAndStick:
            atomStyle->radius = settings.ballRadius;
            break;
        case StyleSettings::eSpaceFill:
            atomStyle->radius = element->vdWRadius * settings.spaceFillProportion;
            break;
        default:
            ERRORMSG("StyleManager::GetAtomStyle() - inappropriate style for atom");
            return false;
    }

    // determine color
    StyleSettings::eColorScheme
        colorStyle = backboneStyle ? backboneStyle->colorScheme : generalStyle->colorScheme;
    switch (colorStyle) {

        case StyleSettings::eElement:
            atomStyle->color = element->color;
            break;

        case StyleSettings::eAligned:
        case StyleSettings::eIdentity:
        case StyleSettings::eVariety:
        case StyleSettings::eWeightedVariety:
        case StyleSettings::eInformationContent:
        case StyleSettings::eFit:
        case StyleSettings::eBlockFit:
        case StyleSettings::eBlockZFit:
        case StyleSettings::eBlockRowFit:
            if (molecule->sequence &&
                molecule->parentSet->alignmentManager->
                    IsAligned(molecule->sequence, residue->id - 1)) { // assume seqIndex is rID - 1
                const Vector * color = molecule->parentSet->alignmentManager->
                    GetAlignmentColor(molecule->sequence, residue->id - 1, colorStyle);
                if (color)
                    atomStyle->color = *color;
                else
                    atomStyle->color = GlobalColors()->Get(Colors::eUnaligned);
                break;
            }
            if (colorStyle != StyleSettings::eAligned) {
                atomStyle->color = GlobalColors()->Get(Colors::eUnaligned);
                break;
            }
            // if eAligned and not aligned, then fall through to use eObject coloring

        case StyleSettings::eObject:
            atomStyle->color = GlobalColors()->Get(Colors::eCycle1, object->id - 1);
            break;

        case StyleSettings::eDomain:
            atomStyle->color =
                (molecule->residueDomains[residue->id - 1] == Molecule::NO_DOMAIN_SET) ?
                    GlobalColors()->Get(Colors::eNoDomain) :
                    GlobalColors()->Get(Colors::eCycle1, molecule->residueDomains[residue->id - 1] - 1);
            break;

        case StyleSettings::eMolecule:
            atomStyle->color = GlobalColors()->Get(Colors::eCycle1, molecule->id - 1);
            break;

        case StyleSettings::eRainbow: {
            double pos = 1.0;
            if ((residue->IsAminoAcid() || residue->IsNucleotide()) && molecule->NResidues() > 1) {
                pos = 1.0 * (residue->id - 1) / (molecule->NResidues() - 1);
            }
            atomStyle->color = GlobalColors()->Get(Colors::eRainbowMap, pos);
            break;
        }

        case StyleSettings::eSecondaryStructure:
            if (molecule->IsResidueInHelix(residue->id))
                atomStyle->color = GlobalColors()->Get(Colors::eHelix);
            else if (molecule->IsResidueInStrand(residue->id))
                atomStyle->color = GlobalColors()->Get(Colors::eStrand);
            else
                atomStyle->color = GlobalColors()->Get(Colors::eCoil);
            break;

        case StyleSettings::eCharge: {
            int charge = (residue->IsAminoAcid()) ? GetCharge(residue->code) : 0;
            atomStyle->color = GlobalColors()->Get(
                (charge > 0) ? Colors::ePositive : ((charge < 0) ? Colors::eNegative : Colors::eNeutral));
            break;
        }

        case StyleSettings::eTemperature:
            atomStyle->color =
                (coord && coord->averageTemperature != AtomCoord::NO_TEMPERATURE &&
				 object->maxTemperature != object->minTemperature) ?
                    GlobalColors()->Get(Colors::eTemperatureMap,
                        (coord->averageTemperature - object->minTemperature) /
                        (object->maxTemperature - object->minTemperature)) :
                    GlobalColors()->Get(Colors::eNoTemperature);
            break;

        case StyleSettings::eHydrophobicity: {
            double hydrophobicity = (residue->IsAminoAcid()) ?
                GetHydrophobicity(residue->code) : UNKNOWN_HYDROPHOBICITY;
            atomStyle->color = (hydrophobicity != UNKNOWN_HYDROPHOBICITY) ?
                GlobalColors()->Get(Colors::eHydrophobicityMap, hydrophobicity) :
                GlobalColors()->Get(Colors::eNoHydrophobicity);
            break;
        }

        case StyleSettings::eUserSelect:
            if (backboneStyle)
                atomStyle->color = backboneStyle->userColor;
            else
                atomStyle->color = generalStyle->userColor;
            break;

        default:
            ERRORMSG("StyleManager::GetAtomStyle() - inappropriate color scheme for atom");
            return false;
    }

    // determine transparency and metal ion labeling
    atomStyle->centerLabel.erase();
    if (molecule->IsSolvent())
        atomStyle->style = eTransparentAtom;
    else if (IsMetal(info->atomicNumber) ||
             (molecule->NResidues() == 1 && residue->NAtomsInGraph() == 1)) {
        atomStyle->style = eTransparentAtom;
        // always big spheres for metals or isolated atoms
        atomStyle->radius = element->vdWRadius * settings.spaceFillProportion;
        if (settings.ionLabelsOn)
            atomStyle->centerLabel = element->symbol;
    } else
        atomStyle->style = eSolidAtom;

    // add transparency; scale by occupancy if transparent
    if (atomStyle->style == eTransparentAtom) {
        atomStyle->alpha = 0.6;
        if (coord && coord->occupancy < 1 && coord->occupancy > 0)
            atomStyle->alpha *= coord->occupancy;
    } else
        atomStyle->alpha = 1.0;

    // determine whether it's highlighted, but *don't* set the color to the highlight
    // color yet, since this is used by the sequence viewer where the residue letter is
    // colored independently of the highlighted background
    atomStyle->isHighlighted = GlobalMessenger()->IsHighlighted(molecule, residue->id);

    atomStyle->name = info->glName;
    return true;
}

// this is basically a map from StyleSettings enums to StyleManager enums;
// sets bond radius, too
static bool SetBondStyleFromResidueStyle(StyleSettings::eDrawingStyle style,
    const StyleSettings& settings, BondStyle::EndStyle *end)
{
    switch (style) {
        case StyleSettings::eWire:
            end->style = StyleManager::eLineBond;
            break;
        case StyleSettings::eTubes:
            end->style = StyleManager::eCylinderBond;
            end->radius = settings.tubeRadius;
            break;
        case StyleSettings::eBallAndStick:
            end->style = StyleManager::eCylinderBond;
            end->radius = settings.stickRadius;
            break;
        case StyleSettings::eSpaceFill:
            end->style = StyleManager::eLineBond;
            break;
        case StyleSettings::eWireWorm:
            end->style = StyleManager::eLineWormBond;
            break;
        case StyleSettings::eTubeWorm:
            end->style = StyleManager::eThickWormBond;
            end->radius = settings.tubeWormRadius;
            break;
        default:
            ERRORMSG("SetBondStyleFromResidueStyle() - invalid style for bond");
            return false;
    }
    return true;
}

#define BOND_NOT_DISPLAYED do { \
    bondStyle->end1.style = bondStyle->end2.style = eNotDisplayed; \
    return true; } while (0)

// Bond style is set by the residue style of the atoms at each end; the color
// is taken from the atom style (GetAtomStyle()), as well as some convenience
// style pointers (backboneStyle, generalStyle). Show/hide status is taken
// from the atoms - if either is hidden, the bond isn't shown either.
bool StyleManager::GetBondStyle(const Bond *bond,
        const AtomPntr& atom1, const AtomCoord *coord1,
        const AtomPntr& atom2, const AtomCoord *coord2,
        double bondLength, BondStyle *bondStyle) const
{
    const StructureObject *object;
    if (!bond->GetParentOfType(&object)) return false;

    const Residue::AtomInfo
        *info1 = object->graph->GetAtomInfo(atom1),
        *info2 = object->graph->GetAtomInfo(atom2);
    if (!info1 || !info2) BOND_NOT_DISPLAYED;

    AtomStyle atomStyle1, atomStyle2;
    const StyleSettings::BackboneStyle *backboneStyle1, *backboneStyle2;
    const StyleSettings::GeneralStyle *generalStyle1, *generalStyle2;
    if (!GetAtomStyle(info1->residue, atom1, coord1, &atomStyle1, &backboneStyle1, &generalStyle1) ||
        !GetAtomStyle(info2->residue, atom2, coord2, &atomStyle2, &backboneStyle2, &generalStyle2))
        return false;

    // if both atoms are hidden, or either one doesn't have coordinates, don't show the bond
    if ((atomStyle1.style == eNotDisplayed && atomStyle2.style == eNotDisplayed) || (!coord1 || !coord2))
        BOND_NOT_DISPLAYED;

     // defaults
    bondStyle->end1.atomCap = bondStyle->end2.atomCap = false;
    bondStyle->end1.name = info1->glName;
    bondStyle->end2.name = info2->glName;
    bondStyle->midCap = false;

    // if one atom is hidden, check for special cases to see if bond is visible at all
    if (atomStyle1.style == eNotDisplayed || atomStyle2.style == eNotDisplayed) {
        bool isSpecial = false;

        // is residue PRO, and bond is between CD and N?
        if (info1->residue->IsAminoAcid() && info1->residue->nameGraph == "PRO" &&
            atom1.mID == atom2.mID && atom1.rID == atom2.rID)
        {
            const Molecule *molecule;
            if (!info1->residue->GetParentOfType(&molecule))
                return false;
            // atom1 is CD and is visible, switch N (atom2) to side chain style
            if (info1->code == " CD " && atomStyle1.style != eNotDisplayed && info2->code == " N  ")
            {
                generalStyle2 = generalStyle1;
                backboneStyle2 = NULL;
                atomStyle2.isHighlighted = GlobalMessenger()->IsHighlighted(molecule, info2->residue->id);
                bondStyle->end2.atomCap = true;
                isSpecial = true;
            }
            // atom2 is CD and is visible
            else if (info2->code == " CD " && atomStyle2.style != eNotDisplayed && info1->code == " N  ")
            {
                generalStyle1 = generalStyle2;
                backboneStyle1 = NULL;
                atomStyle1.isHighlighted = GlobalMessenger()->IsHighlighted(molecule, info1->residue->id);
                bondStyle->end1.atomCap = true;
                isSpecial = true;
            }
        }

        // will show half-bonds in trace backbones
        if (bond->order == Bond::eVirtual)
            isSpecial = true;   // will set up style stuff later

        // otherwise, don't show the bond at all when one atom is hidden
        if (!isSpecial)
            BOND_NOT_DISPLAYED;
    }

    // use connection style if bond is between molecules
    if (atom1.mID != atom2.mID && bond->order != Bond::eRealDisulfide && bond->order != Bond::eVirtualDisulfide) {
        if (globalStyle.connections.isOn == false)
            BOND_NOT_DISPLAYED;
        bondStyle->end1.color = bondStyle->end2.color = globalStyle.connections.userColor;
        if (globalStyle.connections.style == StyleSettings::eWire)
            bondStyle->end1.style = bondStyle->end2.style = eLineBond;
        else if (globalStyle.connections.style == StyleSettings::eTubes) {
            bondStyle->end1.style = bondStyle->end2.style = eCylinderBond;
            bondStyle->end1.radius = bondStyle->end2.radius = globalStyle.tubeRadius;
            bondStyle->end1.atomCap = bondStyle->end2.atomCap = true;
        } else {
            ERRORMSG("StyleManager::GetBondStyle() - invalid connection style");
            return false;
        }
    }

    // otherwise, need to query atom style to figure bond style parameters
    else {

        const StyleSettings&
            settings1 = GetStyleForResidue(object, atom1.mID, atom1.rID),
            settings2 = GetStyleForResidue(object, atom2.mID, atom2.rID);

        StyleSettings::eDrawingStyle style1;
        if (backboneStyle1)
            style1 = backboneStyle1->style;
        else
            style1 = generalStyle1->style;
        if (!SetBondStyleFromResidueStyle(style1, settings1, &(bondStyle->end1)))
            return false;

        StyleSettings::eDrawingStyle style2;
        if (backboneStyle2)
            style2 = backboneStyle2->style;
        else
            style2 = generalStyle2->style;
        if (!SetBondStyleFromResidueStyle(style2, settings2, &(bondStyle->end2)))
            return false;

        // special handling of alpha virtual bonds
        if (bond->order == Bond::eVirtual) {
            if (backboneStyle1->type != StyleSettings::eTrace || atomStyle1.style == eNotDisplayed)
                bondStyle->end1.style = eNotDisplayed;
            if (backboneStyle2->type != StyleSettings::eTrace || atomStyle2.style == eNotDisplayed)
                bondStyle->end2.style = eNotDisplayed;
            if (atomStyle1.style == eNotDisplayed || atomStyle2.style == eNotDisplayed)
                bondStyle->midCap = true;
            // set worm tension, tighter for smaller protein alpha-helix
            if (info1->residue->IsAminoAcid())
                bondStyle->tension = -0.8;
            else
                bondStyle->tension = -0.4;
        }

        // special case coloring and rendering for disulfides
        if (bond->order == Bond::eVirtualDisulfide) {
            if (backboneStyle1->type != StyleSettings::eTrace || backboneStyle2->type != StyleSettings::eTrace ||
                !settings1.virtualDisulfidesOn || !settings2.virtualDisulfidesOn)
                    BOND_NOT_DISPLAYED;
            // don't use worms for disulfides
            if (bondStyle->end1.style == eLineWormBond) bondStyle->end1.style = eLineBond;
            else if (bondStyle->end1.style == eThickWormBond) bondStyle->end1.style = eCylinderBond;
            if (bondStyle->end2.style == eLineWormBond) bondStyle->end2.style = eLineBond;
            else if (bondStyle->end2.style == eThickWormBond) bondStyle->end2.style = eCylinderBond;
            bondStyle->end1.color = settings1.virtualDisulfideColor;
            bondStyle->end2.color = settings2.virtualDisulfideColor;
        }

        // use atom color for all else
        else {
            bondStyle->end1.color = atomStyle1.color;
            bondStyle->end2.color = atomStyle2.color;
        }


        // special case for bonds between side chain and residue - make whole bond
        // same style/color as side chain side, and add endCap if atom is of lesser radius
        if (info2->classification == Residue::eSideChainAtom &&
            (info1->classification == Residue::eAlphaBackboneAtom ||
             info1->classification == Residue::ePartialBackboneAtom ||
             info1->classification == Residue::eCompleteBackboneAtom)
           ) {
            bondStyle->end1.style = bondStyle->end2.style;
            bondStyle->end1.color = bondStyle->end2.color;
            bondStyle->end1.radius = bondStyle->end2.radius;
            if (atomStyle1.radius < bondStyle->end1.radius)
                bondStyle->end1.atomCap = true;
        }
        else if (info1->classification == Residue::eSideChainAtom &&
                 (info2->classification == Residue::eAlphaBackboneAtom ||
                  info2->classification == Residue::ePartialBackboneAtom ||
                  info2->classification == Residue::eCompleteBackboneAtom)
                ) {
            bondStyle->end2.style = bondStyle->end1.style;
            bondStyle->end2.color = bondStyle->end1.color;
            bondStyle->end2.radius = bondStyle->end1.radius;
            if (atomStyle2.radius < bondStyle->end2.radius)
                bondStyle->end2.atomCap = true;
        }

        // add midCap if style or radius for two sides of bond is different;
        if (bondStyle->end1.style != bondStyle->end2.style || bondStyle->end1.radius != bondStyle->end2.radius)
            bondStyle->midCap = true;

        // atomCaps needed at ends of thick worms when at end of chain, or if internal residues
        // are hidden or of a different style, or when missing coords of prev/next bond
        if (bondStyle->end1.style == eThickWormBond || bondStyle->end2.style == eThickWormBond) {

            const Residue::AtomInfo *infoV;
            AtomStyle atomStyleV;
            const StyleSettings::BackboneStyle *backboneStyleV;
            const StyleSettings::GeneralStyle *generalStyleV;
            const AtomSet *atomSet;
            if (!coord1->GetParentOfType(&atomSet))
                return false;
            bool overlayConfs = atomSet->parentSet->showHideManager->OverlayConfEnsembles();

            if (bondStyle->end1.style == eThickWormBond &&
                    (!bond->previousVirtual ||
                    !(infoV = object->graph->GetAtomInfo(bond->previousVirtual->atom1)) ||
                    !GetAtomStyle(infoV->residue, bond->previousVirtual->atom1, NULL,
                        &atomStyleV, &backboneStyleV, &generalStyleV) ||
                    atomStyleV.style == eNotDisplayed ||
                    backboneStyleV->style != style1 ||
                    !atomSet->GetAtom(bond->previousVirtual->atom1, overlayConfs, true)))
                bondStyle->end1.atomCap = true;
//            if (bondStyle->end1.atomCap)
//                TRACEMSG("bondStyle->end1.atomCap true at rID " << atom1.rID);

            if (bondStyle->end2.style == eThickWormBond &&
                    (!bond->nextVirtual ||
                    !(infoV = object->graph->GetAtomInfo(bond->nextVirtual->atom2)) ||
                    !GetAtomStyle(infoV->residue, bond->nextVirtual->atom2, NULL,
                        &atomStyleV, &backboneStyleV, &generalStyleV) ||
                    atomStyleV.style == eNotDisplayed ||
                    backboneStyleV->style != style2 ||
                    !atomSet->GetAtom(bond->nextVirtual->atom2, overlayConfs, true)))
                bondStyle->end2.atomCap = true;
//            if (bondStyle->end2.atomCap)
//                TRACEMSG("bondStyle->end2.atomCap true at rID " << atom2.rID);
        }
    }

    // if atom is larger than half bond length, don't show that half of the bond
    bondLength /= 2;
    if (atomStyle1.radius > bondLength) {
        bondStyle->end1.style = eNotDisplayed;
        bondStyle->midCap = true;
    }
    if (atomStyle2.radius > bondLength) {
        bondStyle->end2.style = eNotDisplayed;
        bondStyle->midCap = true;
    }

    // set highlighting color if necessary
    if (atomStyle1.isHighlighted) bondStyle->end1.color = GlobalColors()->Get(Colors::eHighlight);
    if (atomStyle2.isHighlighted) bondStyle->end2.color = GlobalColors()->Get(Colors::eHighlight);

    return true;
}

bool StyleManager::GetObjectStyle(const StructureObject *object, const Object3D& object3D,
    const StyleSettings::GeneralStyle& generalStyle, ObjectStyle *objectStyle) const
{
    // check to see if any residue covered by the object is visible
    bool anyResidueVisible = false;
    const Molecule *molecule = object->graph->molecules.find(object3D.moleculeID)->second;
    for (int r=object3D.fromResidueID; r<=object3D.toResidueID; ++r) {
        if (object->parentSet->showHideManager->IsVisible(molecule->residues.find(r)->second)) {
            anyResidueVisible = true;
            break;
        }
    }
    if (!anyResidueVisible) {
        objectStyle->style = eNotDisplayed;
        return true;
    }

    // set drawing style
    if (generalStyle.style == StyleSettings::eWithArrows) {
        objectStyle->style = eObjectWithArrow;
    } else if (generalStyle.style == StyleSettings::eWithoutArrows) {
        objectStyle->style = eObjectWithoutArrow;
    } else {
        WARNINGMSG("StyleManager::GetObjectStyle() - invalid 3d-object style");
        return false;
    }

    // set color
    switch (generalStyle.colorScheme) {
        case StyleSettings::eMolecule:
            objectStyle->color = GlobalColors()->Get(Colors::eCycle1, object3D.moleculeID - 1);
            break;
        case StyleSettings::eObject:
            objectStyle->color = GlobalColors()->Get(Colors::eCycle1, object->id - 1);
            break;
        case StyleSettings::eDomain:
            {
                int domainID = molecule->residueDomains[object3D.fromResidueID - 1];
                if (domainID == Molecule::NO_DOMAIN_SET)
                    objectStyle->color = GlobalColors()->Get(Colors::eNoDomain);
                else
                    objectStyle->color = GlobalColors()->Get(Colors::eCycle1, domainID - 1);
            }
            break;
        case StyleSettings::eUserSelect:
            objectStyle->color = generalStyle.userColor;
            break;
        case StyleSettings::eSecondaryStructure:
            // set by caller
            break;
        default:
            ERRORMSG("StyleManager::GetObjectStyle() - inappropriate color scheme for 3d-object");
            return false;
    }

    return true;
}

bool StyleManager::GetHelixStyle(const StructureObject *object,
    const Helix3D& helix, HelixStyle *helixStyle) const
{
    // use style of first associated residue
    const StyleSettings&
        settings = GetStyleForResidue(object, helix.moleculeID, helix.fromResidueID);

    if (!settings.helixObjects.isOn) {
        helixStyle->style = eNotDisplayed;
        return true;
    }

    if (!GetObjectStyle(object, helix, settings.helixObjects, helixStyle))
        return false;

    // helix-specific settings
    helixStyle->radius = settings.helixRadius;
    if (settings.helixObjects.style == StyleSettings::eWithArrows) {
        helixStyle->arrowLength = 4.0;
        helixStyle->arrowBaseWidthProportion = 1.2;
        helixStyle->arrowTipWidthProportion = 0.4;
    }
    if (settings.helixObjects.colorScheme == StyleSettings::eSecondaryStructure)
        helixStyle->color = GlobalColors()->Get(Colors::eHelix);

    return true;
}

bool StyleManager::GetStrandStyle(const StructureObject *object,
    const Strand3D& strand, StrandStyle *strandStyle) const
{
    // use style of first associated residue
    const StyleSettings&
        settings = GetStyleForResidue(object, strand.moleculeID, strand.fromResidueID);

    if (!settings.strandObjects.isOn) {
        strandStyle->style = eNotDisplayed;
        return true;
    }

    if (!GetObjectStyle(object, strand, settings.strandObjects, strandStyle))
        return false;

    // strand-specific settings
    strandStyle->width = settings.strandWidth;
    strandStyle->thickness = settings.strandThickness;
    if (settings.strandObjects.style == StyleSettings::eWithArrows) {
        strandStyle->arrowLength = 2.8;
        strandStyle->arrowBaseWidthProportion = 1.6;
    }
    if (settings.strandObjects.colorScheme == StyleSettings::eSecondaryStructure)
        strandStyle->color = GlobalColors()->Get(Colors::eStrand);

    return true;
}

const StyleSettings& StyleManager::GetStyleForResidue(const StructureObject *object,
    int moleculeID, int residueID) const
{
    const Molecule *molecule = object->graph->molecules.find(moleculeID)->second;

    // find the highest priority (lowest index) annotation in the list of displayed annotations,
    // that also covers this residue
    const StyleSettings *style = &globalStyle;

    UserAnnotationList::const_iterator d, de = userAnnotations.end();
    for (d=userAnnotations.begin(); d!=de; ++d) {
        if (!(*d)->isDisplayed)
            continue;
        // check to see if the annotation covers this residue
        ResidueMap::const_iterator residues = (*d)->residues.find(molecule->identifier);
        if (residues != (*d)->residues.end() &&
            residues->second[residueID - 1] == true) {
            StyleMap::const_iterator userStyle = userStyles.find((*d)->styleID);
            if (userStyle == userStyles.end())
                ERRORMSG("User style-id " << (*d)->styleID << " not found in style dictionary!");
            else
                style = &(userStyle->second);
            break;
        }
    }

    return *style;
}

const Vector& StyleManager::GetObjectColor(const Molecule *molecule) const
{
    static const Vector black(0,0,0);
    const StructureObject *object;
    if (!molecule || !molecule->GetParentOfType(&object)) return black;
    return GlobalColors()->Get(Colors::eCycle1, object->id - 1);
}

bool StyleManager::EditGlobalStyle(wxWindow *parent)
{
    StyleDialog dialog(parent, &globalStyle, structureSet);
    return (dialog.ShowModal() == wxOK);
}

CCn3d_style_dictionary * StyleManager::CreateASNStyleDictionary(void) const
{
    auto_ptr<CCn3d_style_dictionary> dictionary(new CCn3d_style_dictionary());
    if (!globalStyle.SaveSettingsToASN(&(dictionary->SetGlobal_style()))) return NULL;

    if (userStyles.size() > 0) {

        // create an ordered list of style id's
        typedef list < int > IntList;
        IntList keys;
        StyleMap::const_iterator s, se = userStyles.end();
        for (s=userStyles.begin(); s!=se; ++s) keys.push_back(s->first);
        keys.sort();

        // create a new style table entry for each user style
        IntList::const_iterator i, ie = keys.end();
        for (i=keys.begin(); i!=ie; ++i) {
            CRef < CCn3d_style_table_item > entry(new CCn3d_style_table_item());
            entry->SetId().Set(*i);
            if (!userStyles.find(*i)->second.SaveSettingsToASN(&(entry->SetStyle()))) return NULL;
            dictionary->SetStyle_table().push_back(entry);
        }
    }

    return dictionary.release();
}

bool StyleManager::LoadFromASNStyleDictionary(const CCn3d_style_dictionary& styleDictionary)
{
    if (!globalStyle.LoadSettingsFromASN(styleDictionary.GetGlobal_style())) return false;

    userStyles.clear();
    if (styleDictionary.IsSetStyle_table()) {
        CCn3d_style_dictionary::TStyle_table::const_iterator t, te = styleDictionary.GetStyle_table().end();
        for (t=styleDictionary.GetStyle_table().begin(); t!=te; ++t) {
            int id = (*t)->GetId().Get();
            if (userStyles.find(id) != userStyles.end()) {
                ERRORMSG("repeated style table id in style dictionary");
                return false;
            } else
                if (!userStyles[id].LoadSettingsFromASN((*t)->GetStyle())) return false;
        }
    }
    return true;
}

bool StyleManager::EditUserAnnotations(wxWindow *parent)
{
    AnnotateDialog dialog(parent, this, structureSet);
    dialog.ShowModal();
    return false;
}

bool StyleManager::AddUserStyle(int *id, StyleSettings **newStyle)
{
    // create a style with the lowest integer id (above zero) available
    static const int max = 10000;
    for (int i=1; i<max; ++i) {
        if (userStyles.find(i) == userStyles.end()) {
            *newStyle = &(userStyles[i]);
            *id = i;
            structureSet->SetDataChanged(StructureSet::eStyleData);
            return true;
        }
    }
    return false;
}

bool StyleManager::RemoveUserStyle(int id)
{
    StyleMap::iterator u = userStyles.find(id);
    if (u == userStyles.end()) return false;
    userStyles.erase(u);
    structureSet->SetDataChanged(StructureSet::eStyleData);
    return true;
}

StyleManager::UserAnnotation * StyleManager::AddUserAnnotation(void)
{
    userAnnotations.resize(userAnnotations.size() + 1);
    userAnnotations.back().Reset(new UserAnnotation());
    userAnnotations.back()->styleID = -1;
    userAnnotations.back()->isDisplayed = false;
    structureSet->SetDataChanged(StructureSet::eStyleData);
    return userAnnotations.back().GetPointer();
}

bool StyleManager::RemoveUserAnnotation(UserAnnotation *annotation)
{
    // remove annotation from available list
    UserAnnotationList::iterator u, ue = userAnnotations.end();
    int removedStyleID = -1;
    for (u=userAnnotations.begin(); u!=ue; ++u) {
        if (annotation == u->GetPointer()) {
            if (annotation->isDisplayed) {
                GlobalMessenger()->PostRedrawAllStructures();
                GlobalMessenger()->PostRedrawAllSequenceViewers();
            }
            removedStyleID = (*u)->styleID;
            userAnnotations.erase(u);
            break;
        }
    }
    if (u == ue) return false;

    // also remove the style if it's not used by any other annotation
    for (u=userAnnotations.begin(); u!=ue; ++u)
        if ((*u)->styleID == removedStyleID)
            break;
    if (u == ue)
        RemoveUserStyle(removedStyleID);

    structureSet->SetDataChanged(StructureSet::eStyleData);
    return true;
}

bool StyleManager::DisplayUserAnnotation(UserAnnotation *annotation, bool display)
{
    // first check to make sure this annotation is known
    UserAnnotationList::const_iterator a, ae = userAnnotations.end();
    for (a=userAnnotations.begin(); a!=ae; ++a)
        if (annotation == a->GetPointer())
            break;
    if (a == ae)
        return false;

    // if display flag is changed
    if (annotation->isDisplayed != display) {
        // set flag
        annotation->isDisplayed = display;

        // need to redraw with new flags
        GlobalMessenger()->PostRedrawAllStructures();
        GlobalMessenger()->PostRedrawAllSequenceViewers();
        structureSet->SetDataChanged(StructureSet::eStyleData);
    }

    return true;
}

bool StyleManager::MoveUserAnnotation(UserAnnotation *annotation, bool moveUp)
{
    // look for the annotation in the list of annotations
    UserAnnotationList::iterator d, de = userAnnotations.end();
    for (d=userAnnotations.begin(); d!=de; ++d)
        if (annotation == d->GetPointer())
            break;
    if (d == userAnnotations.end())
        return false;

    UserAnnotationList::iterator swap;
    bool doSwap = false;
    if (moveUp && d != userAnnotations.begin()) {
        swap = d;
        --swap;     // swap with previous
        doSwap = true;
    } else if (!moveUp) {
        swap = d;
        ++swap;     // swap with next
        if (swap != userAnnotations.end())
            doSwap = true;
    }
    if (doSwap) {
        CRef < UserAnnotation > tmp(*d);
        *d = *swap;
        *swap = tmp;
        structureSet->SetDataChanged(StructureSet::eStyleData);
        // need to redraw if displayed annotation order list has changed
        if (annotation->isDisplayed) {
            GlobalMessenger()->PostRedrawAllStructures();
            GlobalMessenger()->PostRedrawAllSequenceViewers();
        }
    }

    return true;
}

static bool CreateObjectLocation(
    CCn3d_user_annotation::TResidues *residuesASN,
    const StyleManager::ResidueMap& residueMap)
{
    residuesASN->clear();

    StyleManager::ResidueMap::const_iterator r, re = residueMap.end();
    for (r=residueMap.begin(); r!=re; ++r) {

        // find an existing object location that matches this MMDB ID
        CCn3d_user_annotation::TResidues::iterator l, le = residuesASN->end();
        for (l=residuesASN->begin(); l!=le; ++l)
            if ((*l)->GetStructure_id().GetMmdb_id().Get() == r->first->mmdbID) break;

        // if necessary, create new object location, with Biostruc-id from MMDB ID
        if (l == le) {
            CRef < CCn3d_object_location > loc(new CCn3d_object_location());
            if (r->first->mmdbID != MoleculeIdentifier::VALUE_NOT_SET) {
                CMmdb_id *mmdbID = new CMmdb_id();
                mmdbID->Set(r->first->mmdbID);
                loc->SetStructure_id().SetMmdb_id(*mmdbID);
            } else {
                ERRORMSG("CreateObjectLocation() - MoleculeIdentifier must (currently) have MMDB ID");
                return false;
            }
            residuesASN->push_back(loc);
            l = residuesASN->end();
            --l;    // set iterator to the new object location
        }

        // set molecule location
        CRef < CCn3d_molecule_location > molecule(new CCn3d_molecule_location());
        molecule->SetMolecule_id().Set(r->first->moleculeID);
        (*l)->SetResidues().push_back(molecule);

        // check if covers whole molecule; if so, leave 'residues' field of Cn3d_molecule_location blank
        int i;
        for (i=0; i<r->second.size(); ++i)
            if (!r->second[i]) break;

        // else break list down into individual contiguous residue ranges
        if (i != r->second.size()) {
            int first = 0, last = 0;
            while (first < r->second.size()) {
                // find first included residue
                while (first < r->second.size() && !r->second[first]) ++first;
                if (first >= r->second.size()) break;
                // find last in contiguous stretch of included residues
                last = first;
                while (last + 1 < r->second.size() && r->second[last + 1]) ++last;
                CRef < CCn3d_residue_range > range(new CCn3d_residue_range());
                range->SetFrom().Set(first + 1);    // assume moleculeID = index + 1
                range->SetTo().Set(last + 1);
                molecule->SetResidues().push_back(range);
                first = last + 2;
            }
            if (molecule->GetResidues().size() == 0) {
                ERRORMSG("CreateObjectLocation() - no residue ranges found");
                return false;
            }
        }
    }

    return true;
}

bool StyleManager::SaveToASNUserAnnotations(ncbi::objects::CCn3d_user_annotations *annotations) const
{
    if (!annotations) return false;
    annotations->ResetAnnotations();
    if (userAnnotations.size() == 0) return true;

    UserAnnotationList::const_iterator a, ae = userAnnotations.end();
    for (a=userAnnotations.begin(); a!=ae; ++a) {

        // fill out individual annotations
        CRef < CCn3d_user_annotation > annotation(new CCn3d_user_annotation());
        annotation->SetName((*a)->name);
        annotation->SetDescription((*a)->description);
        annotation->SetStyle_id().Set((*a)->styleID);
        annotation->SetIs_on((*a)->isDisplayed);

        // fill out residues list
        if (!CreateObjectLocation(&(annotation->SetResidues()), (*a)->residues)) {
            ERRORMSG("StyleManager::CreateASNUserAnnotations() - error creating object location");
            return false;
        }

        annotations->SetAnnotations().push_back(annotation);
    }

    return true;
}

static bool ExtractObjectLocation(
    StyleManager::ResidueMap *residueMap,
    const CCn3d_user_annotation::TResidues& residuesASN)
{
    CCn3d_user_annotation::TResidues::const_iterator o, oe = residuesASN.end();
    for (o=residuesASN.begin(); o!=oe; ++o) {
        int mmdbID;
        if ((*o)->GetStructure_id().IsMmdb_id()) {
            mmdbID = (*o)->GetStructure_id().GetMmdb_id().Get();
        } else {
            ERRORMSG("ExtractObjectLocation() - can't handle non-MMDB identifiers (yet)");
            return false;
        }

        // extract molecules
        CCn3d_object_location::TResidues::const_iterator m, me = (*o)->GetResidues().end();
        for (m=(*o)->GetResidues().begin(); m!=me; ++m) {
            int moleculeID = (*m)->GetMolecule_id().Get();

            // get identifier for this molecule
            const MoleculeIdentifier *identifier = MoleculeIdentifier::FindIdentifier(mmdbID, moleculeID);
            if (!identifier) {
                WARNINGMSG("ExtractObjectLocation() - can't find identifier for molecule location");
                WARNINGMSG("structure MMDB ID " << mmdbID << " not loaded?");
                continue;
            }

            // set residue ranges
            int i;
            vector < bool >& residueFlags = (*residueMap)[identifier];
            if (residueFlags.size() == 0)
                residueFlags.resize(identifier->nResidues, false);

            if ((*m)->IsSetResidues()) {
                // parse individual ranges
                CCn3d_molecule_location::TResidues::const_iterator r, re = (*m)->GetResidues().end();
                for (r=(*m)->GetResidues().begin(); r!=re; ++r) {
                    for (i=(*r)->GetFrom().Get(); i<=(*r)->GetTo().Get(); ++i) {
                        if (i > 0 && i <= residueFlags.size()) {
                            residueFlags[i - 1] = true;     // assume index = residue id - 1
                        } else {
                            ERRORMSG("ExtractObjectLocation() - residue from/to out of range");
                            return false;
                        }
                    }
                }
            } else {
                // assume all residues are included if none specified
                for (i=0; i<residueFlags.size(); ++i) residueFlags[i] = true;
            }
        }
    }
	return true;
}

bool StyleManager::LoadFromASNUserAnnotations(const ncbi::objects::CCn3d_user_annotations& annotations)
{
    if (!annotations.IsSetAnnotations()) return true;

    CCn3d_user_annotations::TAnnotations::const_iterator a, ae = annotations.GetAnnotations().end();
    for (a=annotations.GetAnnotations().begin(); a!=ae; ++a) {
        UserAnnotation *userAnnot = AddUserAnnotation();

        // fill out annotation parameters
        userAnnot->name = (*a)->GetName();
        if ((*a)->IsSetDescription())
            userAnnot->description = (*a)->GetDescription();
        userAnnot->styleID = (*a)->GetStyle_id().Get();
        userAnnot->isDisplayed = (*a)->GetIs_on();

        // extract object locations
        if (!ExtractObjectLocation(&(userAnnot->residues), (*a)->GetResidues()))
            return false;
    }

    return true;
}

void StyleManager::SetGlobalColorScheme(StyleSettings::ePredefinedColorScheme scheme)
{
    globalStyle.SetColorScheme(scheme);
//    structureSet->SetDataChanged(StructureSet::eStyleData);
}

void StyleManager::SetGlobalRenderingStyle(StyleSettings::ePredefinedRenderingStyle style)
{
    globalStyle.SetRenderingStyle(style);
//    structureSet->SetDataChanged(StructureSet::eStyleData);
}

bool StyleManager::SetGlobalStyle(const ncbi::objects::CCn3d_style_settings& styleASN)
{
    bool okay = globalStyle.LoadSettingsFromASN(styleASN);
    if (okay) {
        CheckGlobalStyleSettings();
//        structureSet->SetDataChanged(StructureSet::eStyleData);
        GlobalMessenger()->PostRedrawAllStructures();
        GlobalMessenger()->PostRedrawAllSequenceViewers();
    }
    return okay;
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.86  2005/05/19 20:38:16  thiessen
* check that user style exists before returning reference to it
*
* Revision 1.85  2004/06/02 21:33:13  thiessen
* reorganize user annotation storage so that reordering is saved
*
* Revision 1.84  2004/05/28 19:10:28  thiessen
* fix typos
*
* Revision 1.83  2004/05/27 13:40:50  thiessen
* more cleanup ; use residue rather than global style for disulfides
*
* Revision 1.82  2004/05/26 22:18:42  thiessen
* fix display of single residues with trace backbone
*
* Revision 1.81  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.80  2004/03/15 18:11:01  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.79  2004/02/19 17:05:19  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.78  2004/01/19 17:27:14  thiessen
* add Proline CD-N bond for sidechain display
*
* Revision 1.77  2004/01/19 16:17:37  thiessen
* fix worm display problem when coords of prev/next alpha are missing
*
* Revision 1.76  2003/07/22 18:54:42  thiessen
* fix object access bug
*
* Revision 1.75  2003/06/21 08:18:58  thiessen
* show all atoms with coordinates, even if not in all coord sets
*
* Revision 1.74  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
* Revision 1.73  2003/02/06 16:39:53  thiessen
* add block row fit coloring
*
* Revision 1.72  2003/02/03 19:20:08  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.71  2003/01/30 14:00:23  thiessen
* add Block Z Fit coloring
*
* Revision 1.70  2003/01/28 21:07:56  thiessen
* add block fit coloring algorithm; tweak row dragging; fix style bug
*
* Revision 1.69  2002/11/18 20:49:11  thiessen
* move unaligned/no-coord colors into Colors class
*
* Revision 1.68  2002/11/10 01:26:16  thiessen
* tweak loading of style annotation residue ranges
*
* Revision 1.67  2002/08/15 22:13:18  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.66  2002/04/27 16:32:15  thiessen
* fix small leaks/bugs found by BoundsChecker
*
* Revision 1.65  2002/04/12 01:54:43  thiessen
* tweaks to style stuff
*
* Revision 1.64  2002/04/11 17:50:28  thiessen
* fix domain coloring bug
*
* Revision 1.63  2002/04/11 16:39:56  thiessen
* fix style manager bug
*
* Revision 1.62  2002/02/22 20:46:57  thiessen
* more rigorous fix for same
*
* Revision 1.61  2002/02/22 20:17:24  thiessen
* fix feature range problems
*
* Revision 1.60  2002/01/19 02:34:50  thiessen
* fixes for changes in asn serialization API
*
* Revision 1.59  2001/12/12 14:04:15  thiessen
* add missing object headers after object loader change
*
* Revision 1.58  2001/12/06 23:13:46  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.57  2001/11/27 16:26:10  thiessen
* major update to data management system
*
* Revision 1.56  2001/10/24 17:13:46  thiessen
* skip style annotation if structure not loaded
*
* Revision 1.55  2001/10/17 17:46:22  thiessen
* save camera setup and rotation center in files
*
* Revision 1.54  2001/09/18 03:10:46  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.53  2001/09/04 14:40:19  thiessen
* add rainbow and charge coloring
*
* Revision 1.52  2001/08/24 00:41:36  thiessen
* tweak conservation colors and opengl font handling
*
* Revision 1.51  2001/08/21 01:10:46  thiessen
* add labeling
*
* Revision 1.50  2001/08/13 22:30:59  thiessen
* add structure window mouse drag/zoom; add highlight option to render settings
*
* Revision 1.49  2001/08/10 15:01:58  thiessen
* fill out shortcuts; add update show/hide menu
*
* Revision 1.48  2001/08/09 19:07:14  thiessen
* add temperature and hydrophobicity coloring
*
* Revision 1.47  2001/08/03 13:41:33  thiessen
* add registry and style favorites
*
* Revision 1.46  2001/07/12 17:35:15  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* Revision 1.45  2001/07/04 19:39:17  thiessen
* finish user annotation system
*
* Revision 1.44  2001/06/29 18:54:46  thiessen
* fix mysterious CRef error in MSVC MT compile mode
*
* Revision 1.43  2001/06/29 18:13:58  thiessen
* initial (incomplete) user annotation system
*
* Revision 1.42  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.41  2001/06/15 14:06:40  thiessen
* save/load asn styles now complete
*
* Revision 1.40  2001/06/15 13:00:05  thiessen
* fix enumerated type casts fo unix compilers
*
* Revision 1.39  2001/06/15 02:23:00  thiessen
* fill code for settings<->asn
*
* Revision 1.38  2001/06/14 17:45:10  thiessen
* progress in styles<->asn ; add structure limits
*
* Revision 1.37  2001/06/14 00:34:02  thiessen
* asn additions
*
* Revision 1.36  2001/06/08 14:47:06  thiessen
* fully functional (modal) render settings panel
*
* Revision 1.35  2001/06/07 19:05:38  thiessen
* functional (although incomplete) render settings panel ; highlight title - not sequence - upon mouse click
*
* Revision 1.34  2001/06/01 13:01:00  thiessen
* fix wx-string problem again...
*
* Revision 1.33  2001/05/31 18:47:10  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.32  2001/05/15 23:48:38  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.31  2001/03/29 15:49:21  thiessen
* use disulfide color only for virtual disulfides
*
* Revision 1.30  2001/03/29 15:32:42  thiessen
* change disulfide, connection colors to not-yellow
*
* Revision 1.29  2001/03/23 23:31:56  thiessen
* keep atom info around even if coords not all present; mainly for disulfide parsing in virtual models
*
* Revision 1.28  2001/03/23 15:14:07  thiessen
* load sidechains in CDD's
*
* Revision 1.27  2001/03/23 04:18:53  thiessen
* parse and display disulfides
*
* Revision 1.26  2001/03/22 00:33:17  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.25  2001/03/09 15:49:05  thiessen
* major changes to add initial update viewer
*
* Revision 1.24  2001/02/13 20:33:50  thiessen
* add information content coloring
*
* Revision 1.23  2001/02/08 23:01:51  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.22  2001/01/30 20:51:20  thiessen
* minor fixes
*
* Revision 1.21  2000/12/15 15:51:48  thiessen
* show/hide system installed
*
* Revision 1.20  2000/12/01 19:35:57  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.19  2000/11/30 15:49:40  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.18  2000/10/16 14:25:48  thiessen
* working alignment fit coloring
*
* Revision 1.17  2000/10/05 18:34:43  thiessen
* first working editing operation
*
* Revision 1.16  2000/09/20 22:22:30  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.15  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.14  2000/09/11 22:57:34  thiessen
* working highlighting
*
* Revision 1.13  2000/09/11 14:06:30  thiessen
* working alignment coloring
*
* Revision 1.12  2000/09/08 20:16:55  thiessen
* working dynamic alignment views
*
* Revision 1.11  2000/08/24 23:40:19  thiessen
* add 'atomic ion' labels
*
* Revision 1.10  2000/08/24 18:43:52  thiessen
* tweaks for transparent sphere display
*
* Revision 1.9  2000/08/21 19:31:48  thiessen
* add style consistency checking
*
* Revision 1.8  2000/08/21 17:22:38  thiessen
* add primitive highlighting for testing
*
* Revision 1.7  2000/08/18 18:57:40  thiessen
* added transparent spheres
*
* Revision 1.6  2000/08/17 18:33:12  thiessen
* minor fixes to StyleManager
*
* Revision 1.5  2000/08/17 14:24:07  thiessen
* added working StyleManager
*
* Revision 1.4  2000/08/13 02:43:02  thiessen
* added helix and strand objects
*
* Revision 1.3  2000/08/11 12:58:31  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.2  2000/08/04 22:49:04  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.1  2000/08/03 15:13:59  thiessen
* add skeleton of style and show/hide managers
*
*/
