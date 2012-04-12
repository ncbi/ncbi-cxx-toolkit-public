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

typedef SStaticPair<const char*, const char*> TSgmlAsciiPair;
static const TSgmlAsciiPair sc_sgml_entity[] = {
    { "Agr" , "Alpha" },
    { "Bgr" , "Beta" },
    { "Dgr" , "Delta" },
    { "EEgr", "Eta" },
    { "Egr" , "Epsilon" },
    { "Ggr" , "Gamma" },
    { "Igr" , "Iota" },
    { "KHgr", "Chi" },
    { "Kgr" , "Kappa" },
    { "Lgr" , "Lambda" },
    { "Mgr" , "Mu" },
    { "Ngr" , "Nu" },
    { "OHgr", "Omega" },
    { "Ogr" , "Omicron" },
    { "PHgr", "Phi" },
    { "PSgr", "Psi" },
    { "Pgr" , "Pi" },
    { "Rgr" , "Rho" },
    { "Sgr" , "Sigma" },
    { "THgr", "Theta" },
    { "Tgr" , "Tau" },
    { "Ugr" , "Upsilon" },
    { "Xgr" , "Xi" },
    { "Zgr" , "Zeta" },
    { "agr" , "alpha" },
    { "amp" , "&" },
    { "bgr" , "beta" },
    { "dgr" , "delta" },
    { "eegr", "eta" },
    { "egr" , "epsilon" },
    { "ggr" , "gamma" },
    { "gt"  , ">" },
    { "igr" , "iota" },
    { "kgr" , "kappa" },
    { "khgr", "chi" },
    { "lgr" , "lambda" },
    { "lt"  , "<" },
    { "mgr" , "mu" },
    { "ngr" , "nu" },
    { "ogr" , "omicron" },
    { "ohgr", "omega" },
    { "pgr" , "pi" },
    { "phgr", "phi" },
    { "psgr", "psi" },
    { "rgr" , "rho" },
    { "sfgr", "s" },
    { "sgr" , "sigma" },
    { "tgr" , "tau" },
    { "thgr", "theta" },
    { "ugr" , "upsilon" },
    { "xgr" , "xi" },
    { "zgr" , "zeta" }
};

typedef CStaticPairArrayMap<const char*, const char*, PCase_CStr> TSgmlAsciiMap;
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
            string ts = sgml.substr(amp + 1, old_len);
            TSgmlAsciiMap::const_iterator it = sc_SgmlAsciiMap.find(ts.c_str());
            if (it != sc_SgmlAsciiMap.end()) {
                size_t new_len = strlen(it->second);
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


//detecting SGML in string
bool ContainsSgml(const string& str)
{
	bool found = false;
	size_t pos = NStr::Find(str, "&");
	while (pos != string::npos && !found) {
  		size_t len = 0;
		const char *end = str.c_str() + pos + 1;
		while (*end != 0 && isalpha (*end)) {
			len++;
			end++;
		}
		if (*end == ';' && len > 1) {
			string match = str.substr(pos + 1, len);

			TSgmlAsciiMap::const_iterator it = sc_SgmlAsciiMap.begin();
			while (it != sc_SgmlAsciiMap.end() && !found) {
				if (NStr::StartsWith(match, it->first)) {
					found = true;
				}
				++it;
			}
		}
		if (*end == 0) {
			pos = string::npos;
		} else if (!found) {
			pos = NStr::Find(str, "&", pos + len + 1);
		}
	}
    return found;
}


END_NCBI_SCOPE
