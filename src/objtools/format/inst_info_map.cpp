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
 * Author:  Michael Kornbluh
 *
 * File Description:
 *   This just wraps a hash-table that converts institution abbrev to
 *   institution name and other info.  It's in its own file because 
 *   the data itself is so large.  This file should consist almost 
 *   entirely of data.
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include "inst_info_map.hpp"

#include <util/static_map.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CInstInfoMap::TVoucherInfoRef
CInstInfoMap::GetInstitutionVoucherInfo( 
        const string &inst_abbrev )
{
    // the map that maps name to lots of information

    static const string s_atcc_base("http://www.atcc.org/SearchCatalogs/linkin?id=");
    static const string s_bcrc_base("http://strain.bcrc.firdi.org.tw/BSAS/controller?event=SEARCH&bcrc_no=");
    static const string s_cbs_base("http://www.cbs.knaw.nl/collections/BioloMICS.aspx?Fields=All&ExactMatch=T&Table=CBS+strain+database&Name=CBS+");
    static const string s_ccap_base("http://www.ccap.ac.uk/strain_info.php?Strain_No=");
    static const string s_ccmp_base("https://ccmp.bigelow.org/node/1/strain/CCMP");
    static const string s_ccug_base("http://www.ccug.se/default.cfm?page=search_record.cfm&db=mc&s_tests=1&ccugno=");
    static const string s_cori_base("http://ccr.coriell.org/Sections/Search/Search.aspx?q=");
    static const string s_dsmz_base("http://www.dsmz.de/catalogues/details/culture/DSM-");
    static const string s_fsu_base("http://www.prz.uni-jena.de/data.php?fsu=");
    static const string s_kctc_base("http://www.brc.re.kr/English/_SearchView.aspx?sn=");
    static const string s_ku_base("http://collections.nhm.ku.edu/");
    static const string s_lcr_base("http://scd.landcareresearch.co.nz/Specimen/");
    static const string s_pcc_base("http://www.crbip.pasteur.fr/fiches/fichecata.jsp?crbip=PCC+");
    static const string s_pcmb_base("http://www2.bishopmuseum.org/HBS/PCMB/results3.asp?searchterm3=");
    static const string s_sag_base("http://sagdb.uni-goettingen.de/detailedList.php?str_number=");
    static const string s_tgrc_base("http://tgrc.ucdavis.edu/Data/Acc/AccDetail.aspx?AccessionNum=");
    static const string s_uam_base("http://arctos.database.museum/guid/");
    static const string s_ypm_base("http://peabody.research.yale.edu/cgi-bin/Query.Ledger?");

    static const string s_colon_pfx(":");

    static const string s_kui_pfx("KU_Fish/detail.jsp?record=");
    static const string s_kuit_pfx("KU_Tissue/detail.jsp?record=");
    static const string s_psu_pfx("PSU:Mamm:");

    static const string s_ypment_pfx("LE=ent&ID=");
    static const string s_ypmher_pfx("LE=her&ID=");
    static const string s_ypmich_pfx("LE=ich&ID=");
    static const string s_ypmiz_pfx("LE=iz&ID=");
    static const string s_ypmmam_pfx("LE=mam&ID=");
    static const string s_ypmorn_pfx("LE=orn&ID=");

    static const string s_bcrc_sfx("&type_id=6&keyword=;;");

    static const string s_uscr_pfx("_");
    
    typedef pair<const string, TVoucherInfoRef> TVoucherInfoElem;
    static const TVoucherInfoElem sc_voucher_info_map[] = {
        TVoucherInfoElem("ATCC",             TVoucherInfoRef(new SVoucherInfo(&s_atcc_base, false, NULL,          NULL,        "American Type Culture Collection") ) ),
        TVoucherInfoElem("BCRC",             TVoucherInfoRef(new SVoucherInfo(&s_bcrc_base, false, NULL,          &s_bcrc_sfx, "Bioresource Collection and Research Center") ) ),
        TVoucherInfoElem("CBS",              TVoucherInfoRef(new SVoucherInfo(&s_cbs_base,  false, NULL,          NULL,        "Centraalbureau voor Schimmelcultures, Fungal and Yeast Collection") ) ),
        TVoucherInfoElem("CCAP",             TVoucherInfoRef(new SVoucherInfo(&s_ccap_base, false, NULL,          NULL,        "Culture Collection of Algae and Protozoa") ) ),
        TVoucherInfoElem("CCMP",             TVoucherInfoRef(new SVoucherInfo(&s_ccmp_base, false, NULL,          NULL,        "Provasoli-Guillard National Center for Culture of Marine Phytoplankton") ) ),
        TVoucherInfoElem("CCUG",             TVoucherInfoRef(new SVoucherInfo(&s_ccug_base, false, NULL,          NULL,        "Culture Collection, University of Goteborg, Department of Clinical Bacteriology") ) ),
        TVoucherInfoElem("CHR",              TVoucherInfoRef(new SVoucherInfo(&s_lcr_base,  true,  &s_uscr_pfx,   NULL,        "Allan Herbarium, Landcare Research New Zealand Limited") ) ),
        TVoucherInfoElem("CRCM:Bird",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Washington State University, Charles R. Conner Museum, bird collection") ) ),
        TVoucherInfoElem("Coriell",          TVoucherInfoRef(new SVoucherInfo(&s_cori_base, false, NULL,          NULL,        "Coriell Institute for Medical Research") ) ),
        TVoucherInfoElem("DGR:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, bird tissue collection") ) ),
        TVoucherInfoElem("DGR:Ento",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, entomology tissue collection") ) ),
        TVoucherInfoElem("DGR:Fish",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, fish tissue collection") ) ),
        TVoucherInfoElem("DGR:Herp",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, herpetology tissue collection") ) ),
        TVoucherInfoElem("DGR:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, mammal tissue collection") ) ),
        TVoucherInfoElem("DMNS:Bird",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Denver Museum of Nature and Science, Ornithology Collections") ) ),
        TVoucherInfoElem("DMNS:Mamm",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Denver Museum of Nature and Science, Mammology Collection") ) ),
        TVoucherInfoElem("DSM",              TVoucherInfoRef(new SVoucherInfo(&s_dsmz_base, false, NULL,          NULL,        "Deutsche Sammlung von Mikroorganismen und Zellkulturen GmbH") ) ),
        TVoucherInfoElem("FSU<DEU>",         TVoucherInfoRef(new SVoucherInfo(&s_fsu_base,  false, NULL,          NULL,        "Jena Microbial Resource Collection") ) ),
        TVoucherInfoElem("ICMP",             TVoucherInfoRef(new SVoucherInfo(&s_lcr_base,  true,  &s_uscr_pfx,   NULL,        "International Collection of Microorganisms from Plants") ) ),
        TVoucherInfoElem("KCTC",             TVoucherInfoRef(new SVoucherInfo(&s_kctc_base, false, NULL,          NULL,        "Korean Collection for Type Cultures") ) ),
        TVoucherInfoElem("KU:I",             TVoucherInfoRef(new SVoucherInfo(&s_ku_base,   false, &s_kui_pfx,    NULL,        "University of Kansas, Museum of Natural History, Ichthyology collection") ) ),
        TVoucherInfoElem("KU:IT",            TVoucherInfoRef(new SVoucherInfo(&s_ku_base,   false, &s_kuit_pfx,   NULL,        "University of Kansas, Museum of Natural History, Ichthyology tissue collection") ) ),
        TVoucherInfoElem("KWP:Ento",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Kenelm W. Philip Collection, University of Alaska Museum of the North, Lepidoptera collection") ) ),
        TVoucherInfoElem("MLZ:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Moore Laboratory of Zoology, Occidental College, Bird Collection" ) ) ),
        TVoucherInfoElem("MLZ:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Moore Laboratory of Zoology, Occidental College, Mammal Collection" ) ) ),
        TVoucherInfoElem("MSB:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Southwestern Biology, Bird Collection") ) ),
        TVoucherInfoElem("MSB:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Southwestern Biology, Mammal Collection") ) ),
        TVoucherInfoElem("MSB:Para",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Southwestern Biology, Parasitology Collection") ) ),
        TVoucherInfoElem("MVZ:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Bird Collection") ) ),
        TVoucherInfoElem("MVZ:Egg",          TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Egg Collection") ) ),
        TVoucherInfoElem("MVZ:Herp",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Herpetology Collection") ) ),
        TVoucherInfoElem("MVZ:Hild",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Milton Hildebrand collection") ) ),
        TVoucherInfoElem("MVZ:Img",          TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Image Collection") ) ),
        TVoucherInfoElem("MVZ:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Mammal Collection") ) ),
        TVoucherInfoElem("MVZ:Page",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Notebook Page Collection") ) ),
        TVoucherInfoElem("MVZObs:Herp",      TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Herpetology Collection") ) ),
        TVoucherInfoElem("NBSB:Bird",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "National Biomonitoring Specimen Bank, U.S. Geological Survey, bird collection") ) ),
        TVoucherInfoElem("NZAC",             TVoucherInfoRef(new SVoucherInfo(&s_lcr_base,  true,  &s_uscr_pfx,   NULL,        "New Zealand Arthropod Collection") ) ),
        TVoucherInfoElem("PCC",              TVoucherInfoRef(new SVoucherInfo(&s_pcc_base,  false, NULL,          NULL,        "Pasteur Culture Collection of Cyanobacteria") ) ),
        TVoucherInfoElem("PCMB",             TVoucherInfoRef(new SVoucherInfo(&s_pcmb_base, false, NULL,          NULL,        "The Pacific Center for Molecular Biodiversity") ) ),
        TVoucherInfoElem("PDD",              TVoucherInfoRef(new SVoucherInfo(&s_lcr_base,  true,  &s_uscr_pfx,   NULL,        "New Zealand Fungal Herbarium") ) ),
        TVoucherInfoElem("PSU<USA-OR>:Mamm", TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  false, &s_psu_pfx,    NULL,        "Portland State University, Vertebrate Biology Museum, Mammal Collection") ) ),
        TVoucherInfoElem("SAG",              TVoucherInfoRef(new SVoucherInfo(&s_sag_base,  false, NULL,          NULL,        "Sammlung von Algenkulturen at Universitat Gottingen") ) ),
        TVoucherInfoElem("TGRC",             TVoucherInfoRef(new SVoucherInfo(&s_tgrc_base, false, NULL,          NULL,        "C.M. Rick Tomato Genetics Resource Center") ) ),
        TVoucherInfoElem("UAM:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Bird Collection") ) ),
        TVoucherInfoElem("UAM:Bryo",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Bryozoan Collection") ) ),
        TVoucherInfoElem("UAM:Crus",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Marine Arthropod Collection") ) ),
        TVoucherInfoElem("UAM:Ento",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Insect Collection") ) ),
        TVoucherInfoElem("UAM:Fish",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Fish Collection") ) ),
        TVoucherInfoElem("UAM:Herb",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, UAM Herbarium") ) ),
        TVoucherInfoElem("UAM:Herp",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Amphibian and Reptile Collection") ) ),
        TVoucherInfoElem("UAM:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Mammal Collection") ) ),
        TVoucherInfoElem("UAM:Moll",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Mollusc Collection") ) ),
        TVoucherInfoElem("UAM:Paleo",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, paleontology collection") ) ),
        TVoucherInfoElem("UAMObs:Mamm",      TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Mammal Collection") ) ),
        TVoucherInfoElem("WNMU:Bird",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Western New Mexico University Museum, bird collection") ) ),
        TVoucherInfoElem("WNMU:Fish",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Western New Mexico University Museum, fish collection") ) ),
        TVoucherInfoElem("WNMU:Mamm",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,  true,  &s_colon_pfx,  NULL,        "Western New Mexico University Museum, mammal collection") ) ),
        TVoucherInfoElem("YPM:ENT",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,  false, &s_ypment_pfx, NULL,        "Yale Peabody Museum of Natural History, Entomology Collection") ) ),
        TVoucherInfoElem("YPM:HER",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,  false, &s_ypmher_pfx, NULL,        "Yale Peabody Museum of Natural History, Herpetology Collection") ) ),
        TVoucherInfoElem("YPM:ICH",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,  false, &s_ypmich_pfx, NULL,        "Yale Peabody Museum of Natural History, Ichthyology Collection") ) ),
        TVoucherInfoElem("YPM:IZ",           TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,  false, &s_ypmiz_pfx,  NULL,        "Yale Peabody Museum of Natural History, Invertebrate Zoology Collection") ) ),
        TVoucherInfoElem("YPM:MAM",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,  false, &s_ypmmam_pfx, NULL,        "Yale Peabody Museum of Natural History, Mammology Collection") ) ),
        TVoucherInfoElem("YPM:ORN",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,  false, &s_ypmorn_pfx, NULL,        "Yale Peabody Museum of Natural History, Ornithology Collection") ) )
    };
    typedef CStaticArrayMap<const string, TVoucherInfoRef, PCase> TVoucherInfoMap;
    DEFINE_STATIC_ARRAY_MAP(TVoucherInfoMap, sc_VoucherInfoMap, sc_voucher_info_map);

    TVoucherInfoMap::const_iterator iter;
    SIZE_TYPE inst_first_space_pos = inst_abbrev.find_first_of(" ");
    if( NPOS == inst_first_space_pos ) {
        iter = sc_VoucherInfoMap.find( inst_abbrev );
    } else {
        // space in inst_abbrev, so lookup using part before the space
        iter = sc_VoucherInfoMap.find( inst_abbrev.substr(0, inst_first_space_pos) );
    }

    if( iter == sc_VoucherInfoMap.end() ) {
        // can't find it
        return TVoucherInfoRef();
    } else {
        return iter->second;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
