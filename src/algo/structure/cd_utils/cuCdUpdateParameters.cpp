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

char* BlastTypeNames[] = {
    "blast",
    "psi-blast"
};

char* BlastDatabaseNames[] = {
    "nr", 
    "swissprot", 
    "pdb", 
    "pat", 
    "Yeast", 
    "ecoli", 
    "Drosophila genome", 
    "month"
};

char* OrganismNames[] = {
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

char* EnvironmentalTaxNames[] = {
    "unclassified", 
    "environmental samples", 
    "environmental sequence"
};

CdUpdateParameters::CdUpdateParameters()
    : entrezQuery(),
    blastType(ePSI_BLAST),
    database(eNR),
    organism(eAll_organisms),
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
	identityThreshold(-1)
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

char* CdUpdateParameters::getBlastTypeName(enum BlastType bt)
{
    if (bt >= eBlastTypeEnd)
        return 0;
    else
        return BlastTypeNames[bt];
}

string CdUpdateParameters::getBlastTypeDefline()
{
    string defline("Type of Blast:");
    for (int i = 0; i < eBlastTypeEnd; i++)
    {
        char* part = getBlastTypeName((BlastType)i);
        if (part)
        {
            defline += '|';
            defline += part;
        }
    }
    return defline;
}

char* CdUpdateParameters::getBlastDatabaseName(BlastDatabase db)
{
    if (db >= eBlastDatabaseEnd)
        return 0;
    else
        return BlastDatabaseNames[db];
}

string CdUpdateParameters::getBlastDatabaseDefline()
{
    string defline("Choose a database:");
    for (int i = 0; i < eBlastDatabaseEnd; i++)
    {
        char* part = getBlastDatabaseName((BlastDatabase)i);
        if (part)
        {
            defline += '|';
            defline += part;
        }
    }
    return defline;
}


char* CdUpdateParameters::getOrganismName(Organism org)
{
    if (org >= eOrganismEnd)
        return 0;
    else
        return OrganismNames[org];
}

string CdUpdateParameters::getOrganismDefline()
{
    string defline("Limit search by organism:");
    for (int i = 0; i < eOrganismEnd; i++)
    {
        char* part = getOrganismName((Organism)i);
        if (part)
        {
            defline += '|';
            defline += part;
        }
    }
    return defline;
}


char* CdUpdateParameters::getEnvironmentalTaxName(EnvironmentalTax et)
{
    if (et >= eEnvironmentalTaxEnd)
        return 0;
    else
        return EnvironmentalTaxNames[et];
}

string CdUpdateParameters::getEnvironmentalTaxDefline()
{
    string defline("Exclude sequences with this taxonomic classification:");
    for (int i = 0; i < eEnvironmentalTaxEnd; i++)
    {
        char* part = getEnvironmentalTaxName((EnvironmentalTax)i);
        if (part)
        {
            defline += '|';
            defline += part;
        }
    }
    return defline; 

}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
