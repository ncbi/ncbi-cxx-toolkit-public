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
 * Author:  Charlie Liu
 *
 * File Description:
 *
 *       
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuCdUpdateParameters.hpp>
#include <stdio.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

string BlastTypeNames[] = {
    "blast",
    "psi-blast"
};

string BlastDatabaseNames[] = {
    "nr", 
    "swissprot", 
    "pdb", 
    "pat", 
    "Yeast", 
    "ecoli", 
    "Drosophila genome", 
    "month",
    "SMARTBLAST/landmark",
    "CDSEARCH_TEST/nr_v4",
    "CDSEARCH_TEST/pdb_v5",
    "CDSEARCH/CDDupdate"
};

string OrganismNames[] = {
    "All organisms",
    "Cellular organisms",
    "Viruses", 
    "Archaea", 
    "Bacteria", 
    "Eukaryota", 
    "Viridiplantae", 
    "Fungi", 
    "Metazoa", 
    "Arthropoda", 
    "Vertebrata", 
    "Mammalia", 
    "Rodentia", 
    "Primates"
};

string EnvironmentalTaxNames[] = {
    "unclassified", 
    "environmental samples", 
    "environmental sequence"
};


CdUpdateParameters::CdUpdateParameters()
    : blastType(ePSI_BLAST),
    database(eNR),
    organism(eAll_organisms),
    entrezQuery(),
    numHits(0),
    evalue(0.01),
    timeToCheck(100000),
    missingResidueThreshold(1),
    excludingTaxNodes(true),
    nonRedundify(false),
    refresh(false),
    useNRPrefs(false),
	noFilter(false),
	replaceOldAcc(true),
	identityThreshold(-1),
	allowedOverlapWithCDRow(0)
{
}


CdUpdateParameters::~CdUpdateParameters()
{
}

string CdUpdateParameters::toString()
{
    string result("CD-Updating parameters:");
    result += getBlastTypeName(blastType);
    result += ',';
    result += getBlastDatabaseName(database);
    result += ',';
    result += getOrganismName(organism);
    result += ',';
    result += entrezQuery;
    result += ',';

    char evalueStr[100];
    sprintf(evalueStr,"e-value:%.2e", evalue);
    result += evalueStr;
    return result;
}

string CdUpdateParameters::getBlastTypeName(enum BlastType bt)
{
    if (bt >= eBlastTypeEnd)
        return "";
    else
        return BlastTypeNames[bt];
}

string CdUpdateParameters::getBlastDatabaseName(BlastDatabase db)
{
    if (db >= eBlastDatabaseEnd)
        return "";
    else
        return BlastDatabaseNames[db];
}

string CdUpdateParameters::getOrganismName(Organism org)
{
    if (org >= eOrganismEnd)
        return "";
    else
        return OrganismNames[org];
}

string CdUpdateParameters::getEnvironmentalTaxName(EnvironmentalTax et)
{
    if (et >= eEnvironmentalTaxEnd)
        return "";
    else
        return EnvironmentalTaxNames[et];
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
