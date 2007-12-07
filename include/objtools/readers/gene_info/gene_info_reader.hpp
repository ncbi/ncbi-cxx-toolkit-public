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
 * Author:  Vahram Avagyan
 *
 */

/// @file gene_info_reader.hpp
/// Defines a class for reading Gene information from files.
///
/// Defines the CGeneInfoFileReader class which implements the
/// IGeneInfoInput interface. The class reads and memory-maps several
/// pre-computed and sorted binary files and uses them for fast
/// access to Gene information and Gi to/from Gene ID conversions.

#ifndef OBJTOOLS_READERS_GENE_INFO___GENE_INFO_READER__HPP
#define OBJTOOLS_READERS_GENE_INFO___GENE_INFO_READER__HPP

//==========================================================================//

#include <objtools/readers/gene_info/gene_info.hpp>
#include <objtools/readers/gene_info/file_utils.hpp>

#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE

//==========================================================================//

/// Name of the environment variable holding the path to Gene info files.
#define GENE_INFO_PATH_ENV_VARIABLE      "GENE_INFO_PATH"

/// Name of the processed "Gi to GeneID" file.
#define GENE_GI2GENE_FILE_NAME              "geneinfo.g2i"
/// Name of the processed "GeneID to Offset" file.
#define GENE_GENE2OFFSET_FILE_NAME          "geneinfo.i2o"
/// Name of the processed "Gi to Offset" file.
#define GENE_GI2OFFSET_FILE_NAME            "geneinfo.g2o"
/// Name of the processed "Gene ID to Gi" file.
#define GENE_GENE2GI_FILE_NAME              "geneinfo.i2g"
/// Name of the combined "Gene Data" file.
#define GENE_ALL_GENE_DATA_FILE_NAME        "geneinfo.dat"
/// Name of the general information/statistics file.
#define GENE_GENERAL_INFO_FILE_NAME         "geneinfo.log"

/// CGeneInfoFileReader
///
/// Class implementing the IGeneInfoInput interface using binary files.
///
/// CGeneInfoFileReader reads and memory-maps sorted binary files for fast
/// Gi to Gene ID, Gene ID to Gene Info, Gi to Gene Info, and Gene ID to Gi
/// conversions.
/// The Gene Info lookup is represented by two files,
/// one contains (Gi, Offset) or (Gene ID, Offset) pairs, the other one
/// contains all the Gene data. The lookup is performed in two steps: first,
/// the offset to the Gene data is obtained, then the Gene data line is
/// read, parsed, and the corresponding CGeneInfo object is constructed.
/// The paths to the pre-computed and sorted files are either provided
/// directly to the constructor, or the class attempts to read them from
/// a path stored in an environment variable (the preferred approach).

class CGeneInfoFileReader : public IGeneInfoInput,
                            public CGeneFileUtils
{
private:
    /// Path to the Gi to Gene ID file.
    string m_strGi2GeneFile;

    /// Path to the Gene ID to Offset file.
    string m_strGene2OffsetFile;

    /// Path to the Gi to Offset file.
    string m_strGi2OffsetFile;

    /// Path to the Gene ID to Gi file.
    string m_strGene2GiFile;

    /// Path to the file containing all the Gene data.
    string m_strAllGeneDataFile;

    /// Perform Gi to Offset lookups directly.
    bool m_bGiToOffsetLookup;

    /// Memory-mapped Gi to Gene ID file.
    auto_ptr<CMemoryFile> m_memGi2GeneFile;

    /// Memory-mapped Gene ID to Offset file.
    auto_ptr<CMemoryFile> m_memGene2OffsetFile;

    /// Memory-mapped Gi to Offset file.
    auto_ptr<CMemoryFile> m_memGi2OffsetFile;

    /// Memory-mapped Gene ID to Gi file.
    auto_ptr<CMemoryFile> m_memGene2GiFile;

    /// Input stream for the Gene data file. 
    CNcbiIfstream m_inAllData;

    /// Cached map of looked up Gene Info objects.
    TGeneIdToGeneInfoMap m_mapIdToInfo;

private:
    /// Memory-map all the files.
    void x_MapMemFiles();

    /// Unmap all the memory-mapped files.
    void x_UnmapMemFiles();

    /// Fill the Gene ID list given a Gi.
    bool x_GiToGeneId(int gi, list<int>& listGeneIds);

    /// Set the offset value given a Gene ID.
    bool x_GeneIdToOffset(int geneId, int& nOffset);

    /// Set the offset value given a Gi.
    bool x_GiToOffset(int gi, list<int>& listOffsets);

    /// Fill the Gi list given a Gene ID, and the Gi field index,
    /// which represents the Gi type to be read from the file.
    bool x_GeneIdToGi(int geneId, int iGiField, list<int>& listGis);

    /// Read Gene data at the given offset and create the info object.
    bool x_OffsetToInfo(int nOffset, CRef<CGeneInfo>& info);

public:
    /// Construct using direct paths.
    ///
    /// This version of the constructor takes the paths to
    /// the pre-computed binary files and attempts
    /// to open and map the files.
    ///
    /// @param strGi2GeneFile
    ///     Path to the Gi to Gene ID file
    /// @param strGene2OffsetFile
    ///     Path to the Gene ID to Offset file.
    /// @param strGi2OffsetFile
    ///     Path to the Gi to Offset file.
    /// @param strAllGeneDataFile
    ///     Path to the Gene data file.
    /// @param strGene2GiFile
    ///     Path to the Gene ID to Gi file.
    /// @param bGiToOffsetLookup
    ///     Perform Gi to Offset lookups directly.
    CGeneInfoFileReader(const string& strGi2GeneFile,
                        const string& strGene2OffsetFile,
                        const string& strGi2OffsetFile,
                        const string& strAllGeneDataFile,
                        const string& strGene2GiFile,
                        bool bGiToOffsetLookup = true);

    /// Construct using paths read from an environment variable.
    ///
    /// This version of the constructor reads the paths to
    /// the pre-computed binary files from an environment variable
    /// and attempts to open and map the files.
    ///
    /// @param bGiToOffsetLookup
    ///     Perform Gi to Offset lookups directly.
    CGeneInfoFileReader(bool bGiToOffsetLookup = true);

    /// Destructor.
    virtual ~CGeneInfoFileReader();

    /// GetGeneIdsForGi implementation, see IGeneInfoInput.
    virtual bool
        GetGeneIdsForGi(int gi, TGeneIdList& geneIdList);

    /// GetRNAGisForGeneId implementation, see IGeneInfoInput.
    virtual bool
        GetRNAGisForGeneId(int geneId, TGiList& giList);

    /// GetProteinGisForGeneId implementation, see IGeneInfoInput.
    virtual bool
        GetProteinGisForGeneId(int geneId, TGiList& giList);

    /// GetGenomicGisForGeneId implementation, see IGeneInfoInput.
    virtual bool
        GetGenomicGisForGeneId(int geneId, TGiList& giList);

    /// GetGeneInfoForGi implementation, see IGeneInfoInput.
    virtual bool
        GetGeneInfoForGi(int gi, TGeneInfoList& infoList);

    /// GetGeneInfoForId implementation, see IGeneInfoInput.
    virtual bool
        GetGeneInfoForId(int geneId, TGeneInfoList& infoList);
};

//==========================================================================//

END_NCBI_SCOPE

#endif

