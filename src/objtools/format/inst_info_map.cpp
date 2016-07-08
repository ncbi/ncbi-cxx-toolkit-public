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

    static const string  s_acbr_base("http://www.acbr-database.at/BioloMICS.aspx?Link=T&DB=0&Table=0&Descr=");
    static const string  s_atcc_base("http://www.atcc.org/Products/All/");
    static const string  s_bcrc_base("https://catalog.bcrc.firdi.org.tw/BSAS_cart/controller?event=SEARCH&bcrc_no=");
    static const string  s_cas_base("http://collections.calacademy.org/herp/specimen/");
    static const string  s_cbs_base("http://www.cbs.knaw.nl/collections/BioloMICS.aspx?Fields=All&ExactMatch=T&Table=CBS+strain+database&Name=CBS+");
    static const string  s_ccap_base("http://www.ccap.ac.uk/strain_info.php?Strain_No=");
    static const string  s_ccmp_base("https://ccmp.bigelow.org/node/1/strain/CCMP");
    static const string  s_ccug_base("http://www.ccug.se/default.cfm?page=search_record.cfm&db=mc&s_tests=1&ccugno=");
    static const string  s_cfmr_base("http://www.fpl.fs.fed.us/search/mycologysearch_action.php?sorting_rule=1u&phrasesAndKeywords02=");
    static const string  s_cori_base("http://ccr.coriell.org/Sections/Search/Search.aspx?q=");
    static const string  s_dsmz_base("http://www.dsmz.de/catalogues/details/culture/DSM-");
    static const string  s_frr_base("http://www.foodscience.csiro.au/cgi-bin/rilax/search.pl?stpos=0&stype=AND&query=");
    static const string  s_fsu_base("http://www.prz.uni-jena.de/data.php?fsu=");
    static const string  s_jcm_base("http://www.jcm.riken.jp/cgi-bin/jcm/jcm_number?JCM=");
    static const string  s_kctc_base("http://www.brc.re.kr/English/_SearchView.aspx?sn=");
    static const string  s_ku_base("https://ichthyology.specify.ku.edu/specify/bycatalog/");
    static const string  s_lcr_base("http://scd.landcareresearch.co.nz/Specimen/");
    static const string  s_maff_base("http://www.gene.affrc.go.jp/databases-micro_search_detail_en.php?maff=");
    static const string  s_mcz_base("http://mczbase.mcz.harvard.edu/guid/");
    static const string  s_mtcc_base("http://mtcc.imtech.res.in/catalogue_hyper.php?a=");
    static const string  s_mucl_base("http://bccm.belspo.be/db/mucl_search_results.php?FIRSTITEM=1&LIST1=STRAIN_NUMBER&TEXT1=");
    static const string  s_nbrc_base("http://www.nbrc.nite.go.jp/NBRC2/NBRCCatalogueDetailServlet?ID=NBRC&CAT=");
    static const string  s_ncimb_base("http://www.ncimb.com/BioloMICS.aspx?Table=NCIMBstrains&ExactMatch=T&Fields=All&Name=NCIMB%20");
    static const string  s_nctc_base("https://www.phe-culturecollections.org.uk/products/bacteria/detail.jsp?collection=nctc&refId=NCTC+");
    static const string  s_nrrl_base("http://nrrl.ncaur.usda.gov/cgi-bin/usda/prokaryote/report.html?nrrlcodes=");
    static const string  s_pcc_base("http://www.crbip.pasteur.fr/fiches/fichecata.jsp?crbip=PCC+");
    static const string  s_pcmb_base("http://www2.bishopmuseum.org/HBS/PCMB/results3.asp?searchterm3=");
    static const string  s_pycc_base("http://pycc.bio-aware.com/BioloMICS.aspx?Table=PYCC%20strains&Name=PYCC%20");
    static const string  s_sag_base("http://sagdb.uni-goettingen.de/detailedList.php?str_number=");
    static const string  s_tgrc_base("http://tgrc.ucdavis.edu/Data/Acc/AccDetail.aspx?AccessionNum=");
    static const string  s_uam_base("http://arctos.database.museum/guid/");
    static const string  s_uamh_base("https://secure.devonian.ualberta.ca/uamh/details.php?id=");
    static const string  s_usnm_base("http://collections.mnh.si.edu/services/resolver/resolver.php?");
    static const string  s_ypm_base("http://collections.peabody.yale.edu/search/Record/");

    static const string yp0("0");

    static const string s_colon_pfx(":");
    static const string s_uscr_pfx("_");
    
    static const string s_kui_pfx("KU_Fish/detail.jsp?record=");
    static const string s_kuit_pfx("KU_Tissue/detail.jsp?record=");
    static const string s_psu_pfx("PSU:Mamm:");
    static const string s_usnm_pfx("voucher=Birds:");

    static const string s_ypment_pfx("YPM-ENT-");
    static const string s_ypmher_pfx("YPM-HER-");
    static const string s_ypmich_pfx("YPM-ICH-");
    static const string s_ypmiz_pfx ("YPM-IZ-");
    static const string s_ypmmam_pfx("YPM-MAM-");
    static const string s_ypmorn_pfx("YPM-ORN-");

    static const string s_acbr_sfx("&Fields=All&ExactMatch=T");
    static const string s_atcc_sfx(".aspx");
    static const string s_bcrc_sfx("&type_id=9&keyword=");
    static const string s_ku_sfx("/");
    static const string s_mucl_sfx("&LIST2=ALL+FIELDS&CONJ=OR&RANGE=20&B3=Run+Query");
    static const string s_pycc_sfx("&Fields=All&ExactMatch=T");

    typedef SStaticPair<const char*, TVoucherInfoRef> TVoucherInfoElem;
    static const TVoucherInfoElem sc_voucher_info_map[] = {
        { "ACBR",             TVoucherInfoRef(new SVoucherInfo(&s_acbr_base,  false, 0, NULL,   NULL,          &s_acbr_sfx, "Austrian Center of Biological Resources and Applied Mycology") ) },
        { "ATCC",             TVoucherInfoRef(new SVoucherInfo(&s_atcc_base,  false, 0, NULL,   NULL,          &s_atcc_sfx, "American Type Culture Collection") ) },
        { "BCRC",             TVoucherInfoRef(new SVoucherInfo(&s_bcrc_base,  false, 0, NULL,   NULL,          &s_bcrc_sfx, "Bioresource Collection and Research Center") ) },
        { "CAS:HERP",         TVoucherInfoRef(new SVoucherInfo(&s_cas_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "California Academy of Sciences, Herpetology collection") ) },
        { "CBS",              TVoucherInfoRef(new SVoucherInfo(&s_cbs_base,   false, 0, NULL,   NULL,          NULL,        "Centraalbureau voor Schimmelcultures, Fungal and Yeast Collection") ) },
        { "CCAP",             TVoucherInfoRef(new SVoucherInfo(&s_ccap_base,  false, 0, NULL,   NULL,          NULL,        "Culture Collection of Algae and Protozoa") ) },
        { "CCMP",             TVoucherInfoRef(new SVoucherInfo(&s_ccmp_base,  false, 0, NULL,   NULL,          NULL,        "Provasoli-Guillard National Center for Culture of Marine Phytoplankton") ) },
        { "CCUG",             TVoucherInfoRef(new SVoucherInfo(&s_ccug_base,  false, 0, NULL,   NULL,          NULL,        "Culture Collection, University of Goteborg, Department of Clinical Bacteriology") ) },
        { "CFMR",             TVoucherInfoRef(new SVoucherInfo(&s_cfmr_base,  false, 0, NULL,   NULL,          NULL,        "Center for Forest Mycology Research") ) },
        { "CHR",              TVoucherInfoRef(new SVoucherInfo(&s_lcr_base,   true,  0, NULL,   &s_uscr_pfx,   NULL,        "Allan Herbarium, Landcare Research New Zealand Limited") ) },
        { "CRCM:Bird",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Washington State University, Charles R. Conner Museum, bird collection") ) },
        { "Coriell",          TVoucherInfoRef(new SVoucherInfo(&s_cori_base,  false, 0, NULL,   NULL,          NULL,        "Coriell Institute for Medical Research") ) },
        { "DGR:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, bird tissue collection") ) },
        { "DGR:Ento",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, entomology tissue collection") ) },
        { "DGR:Fish",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, fish tissue collection") ) },
        { "DGR:Herp",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, herpetology tissue collection") ) },
        { "DGR:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Division of Genomic Resources, University of New Mexico, mammal tissue collection") ) },
        { "DMNS:Bird",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Denver Museum of Nature and Science, Ornithology Collections") ) },
        { "DMNS:Mamm",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Denver Museum of Nature and Science, Mammology Collection") ) },
        { "DSM",              TVoucherInfoRef(new SVoucherInfo(&s_dsmz_base,  false, 0, NULL,   NULL,          NULL,        "Deutsche Sammlung von Mikroorganismen und Zellkulturen GmbH") ) },
        { "FRR",              TVoucherInfoRef(new SVoucherInfo(&s_frr_base,   false, 0, NULL,   NULL,          NULL,        "Food Science Australia, Ryde") ) },
        { "FSU<DEU>",         TVoucherInfoRef(new SVoucherInfo(&s_fsu_base,   false, 0, NULL,   NULL,          NULL,        "Jena Microbial Resource Collection") ) },
        { "ICMP",             TVoucherInfoRef(new SVoucherInfo(&s_lcr_base,   true,  0, NULL,   &s_uscr_pfx,   NULL,        "International Collection of Microorganisms from Plants") ) },
        { "JCM",              TVoucherInfoRef(new SVoucherInfo(&s_jcm_base,   false, 0, NULL,   NULL,          NULL,        "Japan Collection of Microorganisms") ) },
        { "KCTC",             TVoucherInfoRef(new SVoucherInfo(&s_kctc_base,  false, 0, NULL,   NULL,          NULL,        "Korean Collection for Type Cultures") ) },
        { "KNWR:Ento",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Kenai National Wildlife Refuge, Entomology Collection") ) },
        { "KU:I",             TVoucherInfoRef(new SVoucherInfo(&s_ku_base,    false, 0, NULL,   &s_kui_pfx,    NULL,        "University of Kansas, Museum of Natural History, Ichthyology collection") ) },
        { "KU:IT",            TVoucherInfoRef(new SVoucherInfo(&s_ku_base,    false, 0, NULL,   &s_kuit_pfx,   NULL,        "University of Kansas, Museum of Natural History, Ichthyology tissue collection") ) },
        { "KWP:Ento",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Kenelm W. Philip Collection, University of Alaska Museum of the North, Lepidoptera collection") ) },
        { "MAFF",             TVoucherInfoRef(new SVoucherInfo(&s_maff_base,  false, 0, NULL,   NULL,          NULL,        "Genebank, Ministry of Agriculture Forestry and Fisheries") ) },
        { "MCZ:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Ornithology Collection") ) },
        { "MCZ:Cryo",         TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Cryogenic Collection") ) },
        { "MCZ:Ent",          TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Entomology Collection") ) },
        { "MCZ:Fish",         TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Icthyology Collection") ) },
        { "MCZ:Herp",         TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Herpetology Collection") ) },
        { "MCZ:IP",           TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Invertebrate Paleontology Collection") ) },
        { "MCZ:IZ",           TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Invertebrate Zoology Collection") ) },
        { "MCZ:Ich",          TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Icthyology Collection") ) },
        { "MCZ:Mala",         TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Mollusk Collection") ) },
        { "MCZ:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Mammal Collection") ) },
        { "MCZ:Orn",          TVoucherInfoRef(new SVoucherInfo(&s_mcz_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Harvard Museum of Comparative Zoology, Ornithology Collection") ) },
        { "MLZ:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Moore Laboratory of Zoology, Occidental College, Bird Collection" ) ) },
        { "MLZ:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Moore Laboratory of Zoology, Occidental College, Mammal Collection" ) ) },
        { "MSB:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Southwestern Biology, Bird Collection") ) },
        { "MSB:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Southwestern Biology, Mammal Collection") ) },
        { "MSB:Para",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Southwestern Biology, Parasitology Collection") ) },
        { "MTCC",             TVoucherInfoRef(new SVoucherInfo(&s_mtcc_base,  false, 0, NULL,   NULL,          NULL,        "Microbial Type Culture Collection & Gene Bank") ) },
        { "MUCL",             TVoucherInfoRef(new SVoucherInfo(&s_mucl_base,  false, 0, NULL,   NULL,          &s_mucl_sfx, "Mycotheque de l'Universite Catholique de Louvain") ) },
        { "MVZ:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Bird Collection") ) },
        { "MVZ:Egg",          TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Egg Collection") ) },
        { "MVZ:Herp",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Herpetology Collection") ) },
        { "MVZ:Hild",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Milton Hildebrand collection") ) },
        { "MVZ:Img",          TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Image Collection") ) },
        { "MVZ:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Mammal Collection") ) },
        { "MVZ:Page",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Notebook Page Collection") ) },
        { "MVZObs:Herp",      TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Museum of Vertebrate Zoology, University of California at Berkeley, Herpetology Collection") ) },
        { "NBRC",             TVoucherInfoRef(new SVoucherInfo(&s_nbrc_base,  false, 0, NULL,   NULL,          NULL,        "NITE Biological Resource Center") ) },
        { "NBSB:Bird",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "National Biomonitoring Specimen Bank, U.S. Geological Survey, bird collection") ) },
        { "NCIMB",            TVoucherInfoRef(new SVoucherInfo(&s_ncimb_base, false, 0, NULL,   NULL,          NULL,        "National Collections of Industrial Food and Marine Bacteria (incorporating the NCFB)") ) },
        { "NCTC",             TVoucherInfoRef(new SVoucherInfo(&s_nctc_base,  false, 0, NULL,   NULL,          NULL,        "National Collection of Type Cultures") ) },
        { "NRRL",             TVoucherInfoRef(new SVoucherInfo(&s_nrrl_base,  false, 0, NULL,   NULL,          NULL,        "Agricultural Research Service Culture Collection") ) },
        { "NZAC",             TVoucherInfoRef(new SVoucherInfo(&s_lcr_base,   true,  0, NULL,   &s_uscr_pfx,   NULL,        "New Zealand Arthropod Collection") ) },
        { "PCC",              TVoucherInfoRef(new SVoucherInfo(&s_pcc_base,   false, 0, NULL,   NULL,          NULL,        "Pasteur Culture Collection of Cyanobacteria") ) },
        { "PCMB",             TVoucherInfoRef(new SVoucherInfo(&s_pcmb_base,  false, 0, NULL,   NULL,          NULL,        "The Pacific Center for Molecular Biodiversity") ) },
        { "PDD",              TVoucherInfoRef(new SVoucherInfo(&s_lcr_base,   true,  0, NULL,   &s_uscr_pfx,   NULL,        "New Zealand Fungal Herbarium") ) },
        { "PSU<USA-OR>:Mamm", TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   false, 0, NULL,   &s_psu_pfx,    NULL,        "Portland State University, Vertebrate Biology Museum, Mammal Collection") ) },
        { "PYCC",             TVoucherInfoRef(new SVoucherInfo(&s_pycc_base,  false, 0, NULL,   NULL,          &s_pycc_sfx, "Portuguese Yeast Culture Collection") ) },
        { "SAG",              TVoucherInfoRef(new SVoucherInfo(&s_sag_base,   false, 0, NULL,   NULL,          NULL,        "Sammlung von Algenkulturen at Universitat Gottingen") ) },
        { "TGRC",             TVoucherInfoRef(new SVoucherInfo(&s_tgrc_base,  false, 0, NULL,   NULL,          NULL,        "C.M. Rick Tomato Genetics Resource Center") ) },
        { "UAM:Bird",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Bird Collection") ) },
        { "UAM:Bryo",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Bryozoan Collection") ) },
        { "UAM:Crus",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Marine Arthropod Collection") ) },
        { "UAM:Ento",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Insect Collection") ) },
        { "UAM:Fish",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Fish Collection") ) },
        { "UAM:Herb",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, UAM Herbarium") ) },
        { "UAM:Herp",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Amphibian and Reptile Collection") ) },
        { "UAM:Mamm",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Mammal Collection") ) },
        { "UAM:Moll",         TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Mollusc Collection") ) },
        { "UAM:Paleo",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, paleontology collection") ) },
        { "UAMH",             TVoucherInfoRef(new SVoucherInfo(&s_uamh_base,  false, 0, NULL,   NULL,          NULL,        "University of Alberta Microfungus Collection and Herbarium") ) },
        { "UAMObs:Mamm",      TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "University of Alaska, Museum of the North, Mammal Collection") ) },
        { "USNM:Birds",       TVoucherInfoRef(new SVoucherInfo(&s_usnm_base,  false, 0, NULL,   &s_usnm_pfx,   NULL,        "National Museum of Natural History, Smithsonian Institution, Division of Birds") ) },
        { "WNMU:Bird",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Western New Mexico University Museum, bird collection") ) },
        { "WNMU:Fish",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Western New Mexico University Museum, fish collection") ) },
        { "WNMU:Mamm",        TVoucherInfoRef(new SVoucherInfo(&s_uam_base,   true,  0, NULL,   &s_colon_pfx,  NULL,        "Western New Mexico University Museum, mammal collection") ) },
        { "YPM:ENT",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,   false, 6, &yp0,  &s_ypment_pfx, NULL,        "Yale Peabody Museum of Natural History, Entomology Collection") ) },
        { "YPM:HER",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,   false, 6, &yp0,  &s_ypmher_pfx, NULL,        "Yale Peabody Museum of Natural History, Herpetology Collection") ) },
        { "YPM:ICH",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,   false, 6, &yp0,  &s_ypmich_pfx, NULL,        "Yale Peabody Museum of Natural History, Ichthyology Collection") ) },
        { "YPM:IZ",           TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,   false, 6, &yp0,  &s_ypmiz_pfx,  NULL,        "Yale Peabody Museum of Natural History, Invertebrate Zoology Collection") ) },
        { "YPM:MAM",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,   false, 6, &yp0,  &s_ypmmam_pfx, NULL,        "Yale Peabody Museum of Natural History, Mammology Collection") ) },
        { "YPM:ORN",          TVoucherInfoRef(new SVoucherInfo(&s_ypm_base,   false, 6, &yp0,  &s_ypmorn_pfx, NULL,        "Yale Peabody Museum of Natural History, Ornithology Collection") ) }
    };
    typedef CStaticArrayMap<const char*, TVoucherInfoRef, PCase_CStr> TVoucherInfoMap;
    DEFINE_STATIC_ARRAY_MAP(TVoucherInfoMap, sc_VoucherInfoMap, sc_voucher_info_map);

    TVoucherInfoMap::const_iterator iter;
    SIZE_TYPE inst_first_space_pos = inst_abbrev.find_first_of(" ");
    if( NPOS == inst_first_space_pos ) {
        iter = sc_VoucherInfoMap.find( inst_abbrev.c_str() );
    } else {
        // space in inst_abbrev, so lookup using part before the space
        iter = sc_VoucherInfoMap.find( inst_abbrev.substr(0, inst_first_space_pos).c_str() );
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
