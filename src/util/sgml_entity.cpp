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
 * Authors:  Mati Shomrat
 *
 * File Description:
 *   Functions to Convert SGML to ASCII for Backbone subset SGML
 */
#include <ncbi_pch.hpp>
#include <util/sgml_entity.hpp>
#include <util/static_map.hpp>

BEGIN_NCBI_SCOPE

// mapping from SGML to ASCII

typedef pair<string, string> TSgmlAsciiPair;
static const TSgmlAsciiPair sc_sgml_entity[] = {
    TSgmlAsciiPair("Agr" , "Alpha"),
    TSgmlAsciiPair("Bgr" , "Beta"),
    TSgmlAsciiPair("Dgr" , "Delta"),
    TSgmlAsciiPair("EEgr", "Eta"),
    TSgmlAsciiPair("Egr" , "Epsilon"),
    TSgmlAsciiPair("Ggr" , "Gamma"),
    TSgmlAsciiPair("Igr" , "Iota"),
    TSgmlAsciiPair("KHgr", "Chi"),
    TSgmlAsciiPair("Kgr" , "Kappa"),
    TSgmlAsciiPair("Lgr" , "Lambda"),
    TSgmlAsciiPair("Mgr" , "Mu"),
    TSgmlAsciiPair("Ngr" , "Nu"),
    TSgmlAsciiPair("OHgr", "Omega"),
    TSgmlAsciiPair("Ogr" , "Omicron"),
    TSgmlAsciiPair("PHgr", "Phi"),
    TSgmlAsciiPair("PSgr", "Psi"),
    TSgmlAsciiPair("Pgr" , "Pi"),
    TSgmlAsciiPair("Rgr" , "Rho"),
    TSgmlAsciiPair("Sgr" , "Sigma"),
    TSgmlAsciiPair("THgr", "Theta"),
    TSgmlAsciiPair("Tgr" , "Tau"),
    TSgmlAsciiPair("Ugr" , "Upsilon"),
    TSgmlAsciiPair("Xgr" , "Xi"),
    TSgmlAsciiPair("Zgr" , "Zeta"),
    TSgmlAsciiPair("agr" , "alpha"),
    TSgmlAsciiPair("amp" , "&"),
    TSgmlAsciiPair("bgr" , "beta"),
    TSgmlAsciiPair("dgr" , "delta"),
    TSgmlAsciiPair("eegr", "eta"),
    TSgmlAsciiPair("egr" , "epsilon"),
    TSgmlAsciiPair("ggr" , "gamma"),
    TSgmlAsciiPair("gt"  , ">"),
    TSgmlAsciiPair("igr" , "iota"),
    TSgmlAsciiPair("kgr" , "kappa"),
    TSgmlAsciiPair("khgr", "chi"),
    TSgmlAsciiPair("lgr" , "lambda"),
    TSgmlAsciiPair("lt"  , "<"),
    TSgmlAsciiPair("mgr" , "mu"),
    TSgmlAsciiPair("ngr" , "nu"),
    TSgmlAsciiPair("ogr" , "omicron"),
    TSgmlAsciiPair("ohgr", "omega"),
    TSgmlAsciiPair("pgr" , "pi"),
    TSgmlAsciiPair("phgr", "phi"),
    TSgmlAsciiPair("psgr", "psi"),
    TSgmlAsciiPair("rgr" , "rho"),
    TSgmlAsciiPair("sfgr", "s"),
    TSgmlAsciiPair("sgr" , "sigma"),
    TSgmlAsciiPair("tgr" , "tau"),
    TSgmlAsciiPair("thgr", "theta"),
    TSgmlAsciiPair("ugr" , "upsilon"),
    TSgmlAsciiPair("xgr" , "xi"),
    TSgmlAsciiPair("zgr" , "zeta")
};

typedef CStaticArrayMap<string, string> TSgmlAsciiMap;
DEFINE_STATIC_ARRAY_MAP(TSgmlAsciiMap, sc_SgmlAsciiMap, sc_sgml_entity);


// in place conversion from SGML to ASCII
// we replace "&SGML entity; -> "<ASCII>"
void Sgml2Ascii(string& sgml)
{
    SIZE_TYPE amp = sgml.find('&');
    
    while (amp != NPOS) {
        SIZE_TYPE semi = sgml.find(';', amp);
        if (semi != NPOS) {
            size_t old_len = semi - amp - 1;
            TSgmlAsciiMap::const_iterator it =
                sc_SgmlAsciiMap.find(sgml.substr(amp + 1, old_len));
            if (it != sc_SgmlAsciiMap.end()) {
                size_t new_len = it->second.size();
                sgml[amp] = '<';
                sgml[semi] =  '>';
                sgml.replace(amp + 1, old_len, it->second);
                semi = amp + 1 + new_len;
            }
            else {
                semi = amp;
            }
        }
        else {
            semi = amp;
        }
        amp = sgml.find('&', semi + 1);
    }
}


// conversion of SGML to ASCII
string Sgml2Ascii(const string& sgml)
{
    string result = sgml;
    Sgml2Ascii(result);
    return result;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2004/10/18 19:17:03  shomrat
 * Initial Revision
 *
 *
 * ===========================================================================
 */

