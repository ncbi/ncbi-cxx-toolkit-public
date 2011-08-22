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

/// @file gene_info_writer.hpp
/// Defines a class for processing Gene files.
///
/// Defines the file processing class that reads the original Gene files,
/// extracts necessary information, sorts and filters it, and writes
/// binary processed files for fast Gene info retrieval by the corresponding
/// Gene info reader library.

#ifndef OBJTOOLS_BLAST_GENE_INFO_WRITER___GENE_INFO_WRITER__HPP
#define OBJTOOLS_BLAST_GENE_INFO_WRITER___GENE_INFO_WRITER__HPP

//==========================================================================//

#include <objtools/blast/gene_info_reader/gene_info_reader.hpp>

#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE

//==========================================================================//

/// CGeneFileWriter
///
/// Class for processing the original Gene files and writing new binary
/// Gene info files.
///
/// CGeneFileWriter provides a simple interface for reading
/// and processing the original Gene files, given their paths,
/// and generating several binary files in the given
/// directory. These binary files are used for fast retrieval of
/// Gene information.
///
/// The generated binary files include:
/// (Gi, GeneID) pairs, sorted by Gi,
/// (GeneID, GeneDataOffset) pairs, sorted by GeneID,
/// (Gi, GeneDataOffset) pairs, sorted by Gi,
/// (GeneID, GiRNA, GiProtein, GiGenomic) 4-tuples, sorted by GeneID,
/// and Gene Data file, which contains all the Gene info records.
///
/// The class also generates a file with general information about the
/// processed Gene files, e.g. how many Gene ID's were found, how many
/// Gi's were filtered, and so on.

class CGeneFileWriter : public CGeneFileUtils
{
public:
    // For the sake of inner classes, which WorkShop doesn't properly
    // trust even when given friend declarations!

    /// Vector type for two-integer records.
    typedef vector<STwoIntRecord> TTwoIntRecordVec;

private:
    /// Integer-to-integer map type.
    typedef map<int, int> TIntToIntMap;

    /// Four-integer record type (used in Gene ID to Gi file).
    typedef SMultiIntRecord<4> TFourIntRecord;

    /// Vector type for four-integer records.
    typedef vector<TFourIntRecord> TFourIntRecordVec;

    /// Structure representing a parsed gene->accession line.
    struct SGene2AccnLine
    {
        /// Taxonomy ID.
        int nTaxId;
        /// Gene ID.
        int geneId;
        /// RNA Gi corresponding to this Gene ID (0 if none).
        int giRNANucl;
        /// Protein Gi corresponding to this Gene ID (0 if none).
        int giProt;
        /// Genomic Gi corresponding to this Gene ID (0 if none).
        int giGenomicNucl;
    };

    /// Structure representing a parsed gene info line.
    struct SGeneInfoLine
    {
        /// Taxonomy ID.
        int nTaxId;
        /// Gene ID.
        int geneId;
        /// Gene Symbol.
        string strSymbol;
        /// Gene Description (plain text, may include several sentences).
        string strDescription;
    };

    /// Structure representing a parsed gene->pubmed line.
    struct SGene2PMLine
    {
        /// Gene ID.
        int geneId;
        /// PubMed ID.
        int nPMID;
    };

    /// Types of Gis read from Gene files.
    enum EGiType {eRNAGi, eProtGi, eGenomicGi};

    /// Line processor base class.
    ///
    /// Classes derived from this base class provide a meaningful
    /// line parsing operation, which transforms a line of text
    /// as read from a text file into one or more two-integer
    /// records and adds them to the provided record vector.
    /// In addition, the class stores a pointer to the calling
    /// instance of CGeneFileWriter, so that the line parsing
    /// operation may have any number of side effects on the calling
    /// object, such as populating temporary maps, tables, etc.
    class CLineProcessor
    {
    protected:
        /// Pointer to the calling instance of CGeneFileWriter.
        CGeneFileWriter* m_pThis;
    public:
        /// Constructor taking a pointer to the calling object.
        CLineProcessor(CGeneFileWriter* pThis) : m_pThis(pThis) {}
        /// Destructor.
        virtual ~CLineProcessor() {}
        /// Parse the given line and populate the vector of records.
        virtual void Process(const string& strLine,
                             TTwoIntRecordVec& vecRecords) = 0;
    };

    friend class CLineProcessor;

private:
    /// Path to Gene to Accession input text file.
    string m_strGene2AccessionFile;

    /// Path to Gene Info input text file.
    string m_strGeneInfoFile;
    
    /// Path to Gene to PubMed input file.
    string m_strGene2PubMedFile;

    /// Path to Gene Data output file.
    string m_strAllGeneDataFile;

    /// Path to Gi to GeneID output file.
    string m_strGi2GeneFile;

    /// Path to GeneID to Offset output file.
    string m_strGene2OffsetFile;

    /// Path to Gi to Offset output file.
    string m_strGi2OffsetFile;

    /// Path to GeneID to Gi output file.
    string m_strGene2GiFile;

    /// Path to the general info/stats output file.
    string m_strInfoFile;

    /// Are multiple Gene IDs allowed for RNA Gis.
    bool m_bAllowMultipleIds_RNAGis;

    /// Are multiple Gene IDs allowed for Protein Gis.
    bool m_bAllowMultipleIds_ProtGis;

    /// Are multiple Gene IDs allowed for Genomic Gis.
    bool m_bAllowMultipleIds_GenomicGis;

    /// SeqDB object used to convert taxID to organism name.
    CRef<CSeqDBExpert> m_seqDb;

    /// Temporary map for GeneID to Offset conversion.
    TIntToIntMap m_mapIdToOffset;

    /// Temporary map for GeneID to PMID conversion.
    TIntToIntMap m_mapIdToNumPMIDs;

    /// Temporary vector storing all the records from gene->accession
    /// file in the form (GeneId, RNAGi, ProteinGi, GenomicGi).
    TFourIntRecordVec m_vecGeneIdToGiRecords;

    /// Temporary map storing Gi types.
    TIntToIntMap m_mapGiToType;

    /// Temporary output file stream for the Gene Data file.
    CNcbiOfstream m_outAllData;

    /// Current offset into the Gene Data file.
    int m_nCurrentOffset;

    /// Temporary output file stream for the general info/stats file.
    CNcbiOfstream m_outInfo;

    /// Total number of Gis, for the info/stats file.
    int m_nTotalGis;

    /// Total number of RNA Gis, for the info/stats file.
    int m_nRNAGis;
    
    /// Total number of Protein Gis, for the info/stats file.
    int m_nProtGis;
    
    /// Total number of Genomic Gis, for the info/stats file.
    int m_nGenomicGis;

    /// Total number of Gene IDs, for the info/stats file.
    int m_nGeneIds;

private:
    // =================================================================== //
    // General file processing

    /// Process a text file and generate an array of records.
    ///
    /// Reads all lines in a text file, parses them,
    /// and converts each line to one or more two-integer records.
    /// 
    /// @param in
    ///     Text file input stream.
    /// @param pLineProcessor
    ///     Pointer to a line processor function object.
    /// @param vecRecords
    ///     Vector of records to populate with data from the file.
    /// @param nMinLineLength
    ///     Minimum length of a valid line in the input file.
    void x_ReadAndProcessFile
    (
        CNcbiIfstream& in,
        CLineProcessor* pLineProcessor,
        TTwoIntRecordVec& vecRecords,
        int nMinLineLength
    );

    // =================================================================== //
    // Data conversion and comparison functions

    /// Get the scientific name of the organism given its TaxID.
    ///
    /// Uses SeqDB to retrieve the scientific name of the organism
    /// given its taxonomy ID. The function will always set the strName
    /// variable, in case of an unknown TaxID or a failed lookup the
    /// value will be set to "unknown".
    /// 
    /// @param nTaxId
    ///     Taxonomy ID to look up.
    /// @param strName
    ///     Set to the scientific name of the organism.
    void x_GetOrgnameForTaxId(int nTaxId, string& strName);

    /// Get Gene Data offset given the Gene ID.
    ///
    /// If Gene ID has been already processed, the function
    /// finds the corresponding offset into the Gene Data file.
    ///
    /// @param geneId
    ///     Gene ID to look up.
    /// @param nOffset
    ///     Set to the offset into the Gene Data file.
    /// @return
    ///     True, if the Gene ID was found.
    bool x_GetOffsetForGeneId(int geneId, int& nOffset);

    /// Get number of PubMed links given the Gene ID.
    ///
    /// If Gene ID has been already processed, the function
    /// finds the number of PubMed links for that Gene ID.
    ///
    /// @param geneId
    ///     Gene ID to look up.
    /// @return
    ///     Number of PubMed links for the Gene ID, 0 if none
    ///     or if the Gene ID was not found.
    int x_GetNumPubMedLinksForGeneId(int geneId);

    /// Compare two-integer records.
    ///
    /// Compares the given two-integer records. The values of the
    /// first field are compared first. If equal, the values of
    /// the second field are compared. If the first value is less than
    /// the second one, the function returns true.
    ///
    /// @param record1
    ///     First record to compare.
    /// @param record2
    ///     Second record to compare.
    /// @return
    ///     True if the first record precedes the second one.
    static bool
        x_CompareTwoIntRecords(const STwoIntRecord& record1,
                               const STwoIntRecord& record2);

    /// Compare four-integer records.
    ///
    /// Compares the given four-integer records. The values of the
    /// first field are compared first. If equal, the values of
    /// the second field are compared, and so on. If the first value
    /// is less than the second one, the function returns true.
    ///
    /// @param record1
    ///     First record to compare.
    /// @param record2
    ///     Second record to compare.
    /// @return
    ///     True if the first record precedes the second one.
    static bool
        x_CompareFourIntRecords(const TFourIntRecord& record1,
                                const TFourIntRecord& record2);

    // =================================================================== //
    // Gene->Accession file processing

    /// Parse a Gene->Accession line.
    ///
    /// Parses the given line and fills the lineData structure
    /// with all the available data. Fields that are not available
    /// are set to 0. The function expects the line to conform to 
    /// the known format and will throw an exception otherwise.
    ///
    /// @param strLine
    ///     Line to parse.
    /// @param lineData
    ///     Structure to fill with the data from the parsed line.
    /// @return
    ///     True if the data was successfully extracted, false if the
    ///     line was empty (e.g. a comment line).
    bool x_Gene2Accn_ParseLine(const string& strLine,
                               SGene2AccnLine& lineData);

    /// Convert a parsed Gene->Accession line to one or more records.
    ///
    /// This function is called on each parsed line to populate
    /// the records array and perform any necessary side effects
    /// (e.g. writes Gene ID to Gi records to a temporary vector).
    ///
    /// @param lineData
    ///     Parsed line data.
    /// @param vecRecords
    ///     Vector of records to populate with data from the line.
    void x_Gene2Accn_LineToRecord(const SGene2AccnLine& lineData,
                                  TTwoIntRecordVec& vecRecords);

    /// Gene->Accession line processor.
    ///
    /// Class derived from CLineProcessor for Gene->Accession line
    /// parsing and processing.
    class CGene2AccnProcessor : public CLineProcessor
    {
    public:
        /// Constructor taking a pointer to the calling object.
        CGene2AccnProcessor(CGeneFileWriter* pThis)
            : CLineProcessor(pThis) {}
        /// Parse the given line and populate the vector of records.
        virtual void Process(const string& strLine,
                             TTwoIntRecordVec& vecRecords);
    };

    friend class CGene2AccnProcessor;

    /// Filtering step for processing Gene->Accession records.
    ///
    /// The function adds vecRecords[iRec-1] to the filtered vector
    /// in one of the following cases:
    /// 1. it's the last record in the group of records with the
    ///    the same Gi, and the group consisted of identical records
    ///    (i.e. the Gene IDs were the same as well)
    /// 2. it's an RNA/Protein/Genomic Gi and having multiple Gene IDs
    ///    is allowed (enabled) for the corresponding type of Gi.
    ///
    /// @param vecRecords
    ///     The original vector of records.
    /// @param iRec
    ///     Current index in the original vector of records.
    /// @param bUnique
    ///     Flag set to true if the current record has a unique Gene ID.
    /// @param vecFiltered
    ///     The filtered vector of records.
    void x_Gene2Accn_Filter(const TTwoIntRecordVec& vecRecords,
                            size_t iRec,
                            bool& bUnique,
                            TTwoIntRecordVec& vecFiltered);

    /// Process the Gene->Accession text file.
    ///
    /// Reads the original Gene->Accession lines and writes sorted
    /// Gi->GeneID and Gi->Offset records to the binary files.
    ///
    /// @param bOverwrite
    ///     If true, the function overwrites the files if they exist.
    void x_Gene2Accn_ProcessFile(bool bOverwrite);

    // =================================================================== //
    // Gene Info file processing

    /// Parse a Gene Info line.
    ///
    /// Parses the given line and fills the lineData structure
    /// with all the available data. Fields that are not available
    /// are set to 0. The function expects the line to conform to 
    /// the known format and will throw an exception otherwise.
    ///
    /// @param strLine
    ///     Line to parse.
    /// @param lineData
    ///     Structure to fill with the data from the parsed line.
    /// @return
    ///     True if the data was successfully extracted, false if the
    ///     line was empty (e.g. a comment line).
    bool x_GeneInfo_ParseLine(const string& strLine,
                              SGeneInfoLine& lineData);

    /// Convert a parsed Gene Info line to a record.
    ///
    /// This function is called on each parsed line to populate
    /// the records array. The function also constructs full
    /// Gene Info objects (including the organism name, number of
    /// PubMed links, etc.) and writes them to the Gene Data file.
    ///
    /// @param lineData
    ///     Parsed line data.
    /// @param vecRecords
    ///     Vector of records to populate with data from the line.
    void x_GeneInfo_LineToRecord(const SGeneInfoLine& lineData,
                                 TTwoIntRecordVec& vecRecords);

    /// Gene Info line processor.
    ///
    /// Class derived from CLineProcessor for Gene Info line
    /// parsing and processing.
    class CGeneInfoProcessor : public CLineProcessor
    {
    public:
        /// Constructor taking a pointer to the calling object.
        CGeneInfoProcessor(CGeneFileWriter* pThis)
            : CLineProcessor(pThis) {}
        /// Parse the given line and populate the vector of records.
        virtual void Process(const string& strLine,
                             TTwoIntRecordVec& vecRecords);
    };

    friend class CGeneInfoProcessor;

    /// Process the Gene Info text file.
    ///
    /// Reads the original Gene Info lines and writes sorted
    /// GeneID->Offset records, as well as the full Gene Info
    /// objects, to the binary files. It also populates the
    /// temporary GeneID->Offset map used in processing the
    /// Gene->Accession file.
    ///
    /// @param bOverwrite
    ///     If true, the function overwrites the files if they exist.
    void x_GeneInfo_ProcessFile(bool bOverwrite);

    // =================================================================== //
    // Gene->PubMed file processing

    /// Parse a Gene->PubMed line.
    ///
    /// Parses the given line and fills the lineData structure
    /// with all the available data. Fields that are not available
    /// are set to 0. The function expects the line to conform to 
    /// the known format and will throw an exception otherwise.
    ///
    /// @param strLine
    ///     Line to parse.
    /// @param lineData
    ///     Structure to fill with the data from the parsed line.
    /// @return
    ///     True if the data was successfully extracted, false if the
    ///     line was empty (e.g. a comment line).
    bool x_Gene2PM_ParseLine(const string& strLine,
                             SGene2PMLine& lineData);

    /// Convert a parsed Gene->PubMed line to a record.
    ///
    /// This function is called on each parsed line to populate
    /// the records array.
    ///
    /// @param lineData
    ///     Parsed line data.
    /// @param vecRecords
    ///     Vector of records to populate with data from the line.
    void x_Gene2PM_LineToRecord(const SGene2PMLine& lineData,
                                TTwoIntRecordVec& vecRecords);

    /// Gene->PubMed line processor.
    ///
    /// Class derived from CLineProcessor for Gene->PubMed line
    /// parsing and processing.
    class CGene2PMProcessor : public CLineProcessor
    {
    public:
        /// Constructor taking a pointer to the calling object.
        CGene2PMProcessor(CGeneFileWriter* pThis)
            : CLineProcessor(pThis) {}
        /// Parse the given line and populate the vector of records.
        virtual void Process(const string& strLine,
                             TTwoIntRecordVec& vecRecords);
    };

    friend class CGene2PMProcessor;

    /// Process the Gene Info text file.
    ///
    /// Reads the original Gene->PubMed lines and populates the
    /// GeneID->NumberOfPMIDs temporary map.
    void x_Gene2PM_ProcessFile();

public:
    // =================================================================== //
    // Main interface

    /// Construct using direct paths.
    ///
    /// The constructor takes direct paths to the input files and
    /// a path to the output directory and initializes the object.
    ///
    /// @param strGene2AccessionFile
    ///     Path to the original Gene->Accession input text file.
    /// @param strGeneInfoFile
    ///     Path to the original Gene Info input text file.
    /// @param strGene2PubMedFile
    ///     Path to the original Gene->PubMed input text file.
    /// @param strOutputDirPath
    ///     Directory to output the binary files to.
    CGeneFileWriter(const string& strGene2AccessionFile,
                      const string& strGeneInfoFile,
                      const string& strGene2PubMedFile,
                      const string& strOutputDirPath);

    /// Destructor.
    virtual ~CGeneFileWriter();

    /// Enable/disable storing multiple Gene IDs for RNA Gis.
    void EnableMultipleGeneIdsForRNAGis(bool bEnable);
    /// Enable/disable storing multiple Gene IDs for Protein Gis.
    void EnableMultipleGeneIdsForProteinGis(bool bEnable);
    /// Enable/disable storing multiple Gene IDs for Genomic Gis.
    void EnableMultipleGeneIdsForGenomicGis(bool bEnable);

    /// Process all the input files and generate the binary files.
    ///
    /// This is the top-level function that processes all the text files
    /// provided at the constructor, and generates the processed binary
    /// files for Gi to GeneID, GeneID to Gi, GeneID to GeneDataOffset, and
    /// Gi to GeneDataOffset mapping, as well as the Gene Data file with
    /// all Gene Info records, and a general info/stats file.
    ///
    /// @param bOverwrite
    ///     If true, the function overwrites the processed binary files
    ///     if they exist. Otherwise it will simply exit if the files exist.
    void ProcessFiles(bool bOverwrite = false);
};

//==========================================================================//

END_NCBI_SCOPE

#endif

