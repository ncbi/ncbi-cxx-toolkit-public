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
*      Classes to information about atomic elements
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "periodic_table.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

PeriodicTableClass PeriodicTable; // one global copy for now

const Element* PeriodicTableClass::GetElement(int Z) const
{
    ZMapType::const_iterator i = ZMap.find(Z);
    if (i != ZMap.end())
        return (*i).second;
    else
        return NULL;
}

void PeriodicTableClass::AddElement(int Z, const char * name,
                                    const char * symbol,
                                    double r, double g, double b,
                                    double v)
{
    ZMapType::const_iterator i = ZMap.find(Z);
    if (i != ZMap.end()) delete const_cast<Element*>((*i).second);
    ZMap[Z] = new Element(name,symbol,r,g,b,v);
}

// defaults; can be overridden later on
PeriodicTableClass::PeriodicTableClass(void)
{
    //      atomic number      name    symbol       color (rgb)     vdW radius
    AddElement(    1,      "Hydrogen",   "H",     0.8,  0.8,  0.8,     1.2  );
    AddElement(    2,        "Helium",  "He",     0.8, 0.37, 0.08,    1.22  );
    AddElement(    3,       "Lithium",  "Li",     0.7,  0.7,  0.7,    1.52  );
    AddElement(    4,     "Beryllium",  "Be",     0.8, 0.37, 0.08,     1.7  );
    AddElement(    5,         "Boron",   "B",     0.9,  0.4,    0,    2.08  );
    AddElement(    6,        "Carbon",   "C",     0.3,  0.3,  0.3,    1.85  );
    AddElement(    7,      "Nitrogen",   "N",     0.2,  0.2,  0.8,    1.54  );
    AddElement(    8,        "Oxygen",   "O",     0.8,  0.2,  0.2,     1.4  );
    AddElement(    9,      "Fluorine",   "F",     0.7, 0.85, 0.45,    1.35  );
    AddElement(   10,          "Neon",  "Ne",     0.8, 0.37, 0.08,     1.6  );
    AddElement(   11,        "Sodium",  "Na",     0.6,  0.6,  0.6,    2.31  );
    AddElement(   12,     "Magnesium",  "Mg",     0.4,  0.4,  0.4,     1.7  );
    AddElement(   13,      "Aluminum",  "Al",     0.4,  0.4,  0.4,    2.05  );
    AddElement(   14,       "Silicon",  "Si",     0.7,    0,  0.1,       2  );
    AddElement(   15,    "Phosphorus",   "P",     0.1,  0.7,  0.3,     1.4  );
    AddElement(   16,        "Sulfur",   "S",    0.95,  0.9,  0.2,    1.85  );
    AddElement(   17,      "Chlorine",  "Cl",    0.15,  0.5,  0.1,    1.81  );
    AddElement(   18,         "Argon",  "Ar",     0.8, 0.37, 0.08,    1.91  );
    AddElement(   19,     "Potassium",   "K",     0.8,  0.5,  0.7,    2.31  );
    AddElement(   20,       "Calcium",  "Ca",     0.8,  0.8,  0.7,   1.973  );
    AddElement(   21,      "Scandium",  "Sc",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   22,      "Titanium",  "Ti",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   23,      "Vanadium",   "V",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   24,      "Chromium",  "Cr",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   25,     "Manganese",  "Mn",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   26,          "Iron",  "Fe",     0.7,    0,  0.1,     1.7  );
    AddElement(   27,        "Cobalt",  "Co",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   28,        "Nickel",  "Ni",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   29,        "Copper",  "Cu",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   30,          "Zinc",  "Zn",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   31,       "Gallium",  "Ga",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   32,     "Germanium",  "Ge",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   33,       "Arsenic",  "As",     0.4,  0.8,  0.1,       2  );
    AddElement(   34,      "Selenium",  "Se",     0.8,  0.8,  0.1,       2  );
    AddElement(   35,       "Bromine",  "Br",     0.5, 0.08, 0.12,     2.1  );
    AddElement(   36,       "Krypton",  "Kr",     0.8, 0.37, 0.08,     1.7  );
    AddElement(   37,      "Rubidium",  "Rb",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   38,     "Strontium",  "Sr",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   39,       "Yttrium",   "Y",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   40,     "Zirconium",  "Zr",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   41,       "Niobium",  "Nb",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   42,    "Molybdenum",  "Mo",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   43,    "Technetium",  "Tc",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   44,     "Ruthenium",  "Ru",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   45,       "Rhodium",  "Rh",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   46,     "Palladium",  "Pd",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   47,        "Silver",  "Ag",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   48,       "Cadmium",  "Cd",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   49,        "Indium",  "In",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   50,           "Tin",  "Sn",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   51,      "Antimony",  "Sb",     0.5,  0.5,  0.5,     2.2  );
    AddElement(   52,     "Tellurium",  "Te",     0.5,  0.5,  0.5,     2.2  );
    AddElement(   53,        "Iodine",   "I",     0.5,  0.1,  0.5,    2.15  );
    AddElement(   54,         "Xenon",  "Xe",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   55,        "Cesium",  "Cs",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   56,        "Barium",  "Ba",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   57,     "Lanthanum",  "La",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   58,        "Cerium",  "Ce",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   59,  "Praseodymium",  "Pr",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   60,     "Neodymium",  "Nd",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   61,    "Promethium",  "Pm",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   62,      "Samarium",  "Sm",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   63,      "Europium",  "Eu",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   64,    "Gadolinium",  "Gd",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   65,       "Terbium",  "Tb",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   66,    "Dysprosium",  "Dy",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   67,       "Holmium",  "Ho",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   68,        "Erbium",  "Er",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   69,       "Thulium",  "Tm",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   70,     "Ytterbium",  "Yb",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   71,      "Lutetium",  "Lu",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   72,       "Hafnium",  "Hf",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   73,      "Tantalum",  "Ta",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   74,      "Tungsten",   "W",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   75,       "Rhenium",  "Re",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   76,        "Osmium",  "Os",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   77,       "Iridium",  "Ir",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   78,      "Platinum",  "Pt",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   79,          "Gold",  "Au",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   80,       "Mercury",  "Hg",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   81,      "Thallium",  "Tl",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   82,          "Lead",  "Pb",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   83,       "Bismuth",  "Bi",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   84,      "Polonium",  "Po",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   85,      "Astatine",  "At",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   86,         "Radon",  "Rn",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   87,      "Francium",  "Fr",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   88,        "Radium",  "Ra",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   89,      "Actinium",  "Ac",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   90,       "Thorium",  "Th",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   91,  "Protactinium",  "Pa",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   92,       "Uranium",   "U",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   93,     "Neptunium",  "Np",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   94,     "Plutonium",  "Pu",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   95,     "Americium",  "Am",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   96,        "Curium",  "Cm",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   97,     "Berkelium",  "Bk",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   98,   "Californium",  "Cf",     0.5,  0.5,  0.5,     1.7  );
    AddElement(   99,   "Einsteinium",  "Es",     0.5,  0.5,  0.5,     1.7  );
    AddElement(  100,       "Fermium",  "Fm",     0.5,  0.5,  0.5,     1.7  );
    AddElement(  101,   "Mendelevium",  "Md",     0.5,  0.5,  0.5,     1.7  );
    AddElement(  102,      "Nobelium",  "No",     0.5,  0.5,  0.5,     1.7  );
    AddElement(  103,    "Lawrencium",  "Lr",     0.5,  0.5,  0.5,     1.7  );
    AddElement(  254,         "other",   "?",     0.4,  0.4,  0.4,     1.6  );
    AddElement(  255,       "unknown",   "?",     0.4,  0.4,  0.4,     1.6  );
}

PeriodicTableClass::~PeriodicTableClass(void)
{
    ZMapType::const_iterator i, ie = ZMap.end();
    for (i=ZMap.begin(); i!=ie; ++i)
        if ((*i).second) delete const_cast<Element*>((*i).second);
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/03/15 18:27:12  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.4  2004/02/19 17:05:01  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.3  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.2  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.1  2000/07/12 23:27:50  thiessen
* now draws basic CPK model
*
*/
