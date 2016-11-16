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
 *
 * ===========================================================================
 */

#ifndef CU_CD_UPDATE_PARAMETERS_HPP
#define CU_CD_UPDATE_PARAMETERS_HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

enum BlastType
{
	eBLAST,
	ePSI_BLAST,
	eBlastTypeEnd
};

extern string BlastTypeNames[];

enum BlastDatabase
{
	eNR,
	eSwissprot,
	ePdb,
	ePat,
	eYeast,
	eEcoli,  
	eDrosophila_genome,
	eMonth,
    eSmartblast_landmark,
	eBlastDatabaseEnd
};

extern string BlastDatabaseNames[];

enum Organism
{
	eAll_organisms,
	eCellular,
	eViruses,
	eArchaea,
	eBacteria,
	eEukaryota,
	eViridiplantae,
	eFungi,
	eMetazoa,
	eArthropoda,
	eVertebrata,
	eMammalia,
	eRodentia,
	ePrimates,
	eOrganismEnd
};

extern string OrganismNames[];

enum EnvironmentalTax
{
	eUnclassified,
	eEnvironmental_samples,
	eEnvironmental_sequence,
	eEnvironmentalTaxEnd
};

extern string EnvironmentalTaxNames[];

class NCBI_CDUTILS_EXPORT CdUpdateParameters
{
public:
	CdUpdateParameters();
	~CdUpdateParameters();
	string toString();

public:
	static string getBlastTypeName(BlastType bt);
	static string getBlastTypeDefline();

	static string getBlastDatabaseName(BlastDatabase db);
	static string getBlastDatabaseDefline();

	static string getOrganismName(Organism org);
	static string getOrganismDefline();

	static string getEnvironmentalTaxName(EnvironmentalTax bt);
	static string getEnvironmentalTaxDefline();

//members
	BlastType blastType; //0=blast, 1=psi-blast
	BlastDatabase database;
	Organism organism;
	string entrezQuery;
	int numHits;
	double evalue;
	int timeToCheck; //in milliseconds
	int missingResidueThreshold;
	int excludingTaxNodes;  //  flag for identity of excluded tax nodes; uses EnvironmentalTax enum above
	bool nonRedundify;
	bool refresh;
    bool useNRPrefs;  //  if true, use BlastDbInfo preferences; otherwise, query Blast server w/o reseting preference values
	bool noFilter;
	bool replaceOldAcc;
	int identityThreshold;
    //  < 0  turn off overlap w/ CD rows update filter
    //  == 0 filter out all updates with an overlap to an existing row(s)  [DEFAULT]
    //  == i filter out any update that overlaps an existing row on either termini by > i residues
    int allowedOverlapWithCDRow;  
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
