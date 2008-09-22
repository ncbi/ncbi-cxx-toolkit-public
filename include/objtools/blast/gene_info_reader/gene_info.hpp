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

/// @file gene_info.hpp
/// Gene information class and related interfaces.
///
/// Gene information is optionally presented in Blast output, along
/// with standard sequence deflines, in the form of one or more lines
/// describing the Gene, name of species, number of PubMed links, etc.
/// This file defines the Gene information class, the related exception
/// class, and the interface for obtaining Gene information from
/// an input source.

#ifndef OBJTOOLS_BLAST_GENE_INFO_READER___GENE_INFO__HPP
#define OBJTOOLS_BLAST_GENE_INFO_READER___GENE_INFO__HPP

//==========================================================================//

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistre.hpp>

BEGIN_NCBI_SCOPE


//==========================================================================//

/// CGeneInfoException
///
/// Class describing an exception thrown by the Gene information classes.
///
/// CGeneInfoException can be thrown while trying to read, process, or
/// output Gene information in any class declared in this header file,
/// classes derived from those, or other related classes.

class CGeneInfoException : public CException
{
public:
    /// Error types for Gene Information processing.
    enum EErrCode {
        eInputError,            //< Invalid user input
        eNetworkError,          //< Cannot access data via network
        eDataFormatError,       //< File format not recognized
        eFileNotFoundError,     //< File not found
        eMemoryError,           //< Not enough memory
        eInternalError          //< Internal/algorithmic error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
            case eInputError:        return "eInputError";
            case eNetworkError:      return "eNetworkError";
            case eDataFormatError:   return "eDataFormatError";
            case eFileNotFoundError: return "eFileNotFoundError";
            case eMemoryError:       return "eMemoryError";
            case eInternalError:     return "eInternalError";
        }
        return CException::GetErrCodeString();
    }

    /// Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CGeneInfoException, CException);
};


//==========================================================================//

/// CGeneInfo
///
/// Gene information storage and formatted output.
///
/// CGeneInfo is used to store and format Gene information. It contains
/// several basic fields from the Entrez Gene database, such as Gene
/// symbol, description, unique ID, etc. The class is derived from CObject
/// so that one can freely use CRefs with this class.

class NCBI_XOBJREAD_EXPORT CGeneInfo : public CObject
{
private:
    /// Is the object properly initialized.
    bool m_bIsInitialized;

    /// Numeric unique Gene ID.
    int m_nGeneId;

    /// Official symbol of the Gene entry.
    string m_strSymbol;

    /// Description of the Gene.
    string m_strDescription;

    /// Scientific name of the organism (e.g. Sus scrofa).
    string m_strOrgname;

    /// Number of PubMed links for this entry.
    int m_nPubMedLinks;

private:
    /// Appends strSrc to strDest.
    ///
    /// The function makes sure that no single line
    /// exceeds the maximum effective line length
    /// (which is the actual number of characters
    /// seen by the user, excluding HTML tags).
    /// 
    /// @param strDest
    ///     Destination string to write to.
    /// @param nCurLineEffLength
    ///     Length of the current line, the function
    ///     updates this variable as necessary.
    /// @param strSrc
    ///     Source string to copy the characters from.
    /// @param nSrcEffLength
    ///     Effective length of the source string,
    ///     excluding the HTML formatting tags, etc.
    /// @param nMaxLineLength
    ///     Maximum allowed effective length for a line.
    static void x_Append(string& strDest,
                         unsigned int& nCurLineEffLength,
                         const string& strSrc,
                         unsigned int nSrcEffLength,
                         unsigned int nMaxLineLength);

public:
    /// Default constructor.
    ///
    /// This version of the constructor makes a default,
    /// uninitialized Gene information object.
    CGeneInfo();

    /// Constructor for initializing Gene information.
    ///
    /// This version of the constructor makes a fully initialized
    /// Gene information object.
    ///
    /// @param nGeneId
    ///     Unique integer ID of the Gene entry.
    /// @param strSymbol
    ///     Official symbol of the Gene entry.
    /// @param strDescription
    ///     Description (full name) of the Gene entry.
    /// @param strOrgName
    ///     Scientific name of the organism.
    /// @param nPubMedLinks
    ///     Number (or estimate) of related PubMed links.
    CGeneInfo(int nGeneId,
              const string& strSymbol,
              const string& strDescription,
              const string& strOrgName,
              int nPubMedLinks);

    /// Destructor.
    virtual ~CGeneInfo();

    /// Check if the object has been properly initialized.
    bool IsInitialized() const {return m_bIsInitialized;}

    /// Get the numeric unique Gene ID.
    int GetGeneId() const {return m_nGeneId;}

    /// Get the official symbol of the Gene entry.
    const string& GetSymbol() const {return m_strSymbol;}

    /// Get the description of the Gene entry.
    const string& GetDescription() const {return m_strDescription;}

    /// Get the scientific name of the organism.
    const string& GetOrganismName() const {return m_strOrgname;}

    /// Get the number of PubMed links for this entry.
    int GetNumPubMedLinks() const {return m_nPubMedLinks;}

    /// Format the Gene information as a multiline string.
    ///
    /// This function combines all the Gene information in one string,
    /// forming one or more lines not exceeding nMaxLineLength,
    /// and adds several HTML elements, if requested.
    ///
    /// @param strGeneInfo
    ///     Destination string to fill with the Gene information.
    /// @param bFormatAsHTML
    ///     This flag enables HTML formatting of the string,
    ///     which includes links to the actual Entrez Gene entry, 
    ///     span tags for CSS processing, and so on.
    /// @param nMaxLineLength
    ///     Maximum allowed effective length for a line (this excludes
    ///     HTML elements invisible to the user). If set to 0,
    ///     the function will use a reasonable default value.
    void ToString(string& strGeneInfo,
                  bool bFormatAsHTML = false,
                  const string& strGeneLinkURL = "",
                  unsigned int nMaxLineLength = 0) const;
};

/// Output the Gene information formatted as HTML.
NCBI_XOBJREAD_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, const CGeneInfo& geneInfo);


//==========================================================================//

/// IGeneInfoInput
///
/// Gene information retrieval interface.
///
/// IGeneInfoInput defines the interface for obtaining Gene information
/// objects for a given Gi or a given Gene ID from any input source.
/// Additionally, the interface defines Gi to/from Gene ID conversions.

class NCBI_XOBJREAD_EXPORT IGeneInfoInput
{
public:
    /// List of Gis.
    typedef list<int>   TGiList;

    /// List of Gene IDs.
    typedef list<int>   TGeneIdList;

    /// Gene ID to Gene Information map.
    typedef map< int, CRef<CGeneInfo> > TGeneIdToGeneInfoMap;

    /// List of Gene Information objects.
    typedef vector< CRef<CGeneInfo> > TGeneInfoList;

public:
    /// Destructor.
    virtual ~IGeneInfoInput() {}

    /// Get all Gene IDs for a given Gi.
    ///
    /// Function takes a Gi and appends all available Gene IDs
    /// for that Gi to the Gene ID list. Notice that some Gis
    /// may be deliberately left out of the lookup process.
    ///
    /// @param gi
    ///     The Gi to look up.
    /// @param geneIdList
    ///     The Gene ID list to append to.
    /// @return
    ///     True if one or more Gene IDs were found for the Gi.
    virtual bool
        GetGeneIdsForGi(int gi, TGeneIdList& geneIdList) = 0;

    /// Get all RNA Gis for a given Gene ID.
    ///
    /// Function takes a Gene ID and appends all available RNA Gis
    /// for that Gene ID to the Gi list.
    ///
    /// @param geneId
    ///     The Gene ID to look up.
    /// @param giList
    ///     The Gi list to append to.
    /// @return
    ///     True if one or more Gis were found for the Gene ID.
    virtual bool
        GetRNAGisForGeneId(int geneId, TGiList& giList) = 0;

    /// Get all Protein Gis for a given Gene ID.
    ///
    /// Function takes a Gene ID and appends all available Protein Gis
    /// for that Gene ID to the Gi list.
    ///
    /// @param geneId
    ///     The Gene ID to look up.
    /// @param giList
    ///     The Gi list to append to.
    /// @return
    ///     True if one or more Gis were found for the Gene ID.
    virtual bool
        GetProteinGisForGeneId(int geneId, TGiList& giList) = 0;

    /// Get all Genomic Gis for a given Gene ID.
    ///
    /// Function takes a Gene ID and appends all available Genomic Gis
    /// for that Gene ID to the Gi list.
    ///
    /// @param geneId
    ///     The Gene ID to look up.
    /// @param giList
    ///     The Gi list to append to.
    /// @return
    ///     True if one or more Gis were found for the Gene ID.
    virtual bool
        GetGenomicGisForGeneId(int geneId, TGiList& giList) = 0;

    /// Get all Gene Information objects for a given Gi.
    ///
    /// Function takes a Gi, looks it up and appends all available
    /// Gene information objects to the given list. Notice that some Gis
    /// may be deliberately left out of the lookup process.
    ///
    /// @param gi
    ///     The Gi to look up.
    /// @param infoList
    ///     The Gene information list to append to.
    /// @return
    ///     True if any Gene information was found for the Gi.
    virtual bool
        GetGeneInfoForGi(int gi, TGeneInfoList& infoList) = 0;

    /// Get all Gene Information objects for a given Gene ID.
    ///
    /// Function takes a Gene ID, looks it up and appends all available
    /// Gene information objects to the given list.
    ///
    /// @param geneId
    ///     The Gene ID to look up.
    /// @param infoList
    ///     The Gene information list to append to.
    /// @return
    ///     True if any Gene information was found for the Gene ID.
    virtual bool
        GetGeneInfoForId(int geneId, TGeneInfoList& infoList) = 0;
};

//==========================================================================//


END_NCBI_SCOPE

#endif

