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
 * Author: Christiam Camacho
 *
 */

/** @file clusterdbcmd.cpp
 * Command line tool to examine the contents of the SQLite database used to
 * store clustered nr BLAST database metadata (cluster membership, taxonomy
 * and common ancestor information).
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbimisc.hpp>   // for TTaxId, INVALID_TAX_ID
#include <algo/blast/api/version.hpp>
#include <algo/blast/api/blast_usage_report.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp> // for CSeqDBException
#include <objtools/blast/seqdb_reader/tax4blastsqlite.hpp>
#include <db/sqlite/sqlitewrapp.hpp>
#include <climits>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

/// Default name of the SQLite database with the clustered nr metadata
static const string kDefaultDb("nr_cluster_seq.sqlite3");

/// @name Command line argument names
/// @{
static const string kArgDb("db");
static const string kArgRepresentative("representative");
static const string kArgTaxid("taxid");
static const string kArgAccession("accession");
static const string kArgGetCommonAncestor("get-common-ancestor");
static const string kArgExactMatch("exact-match");
static const string kArgOutput("out");
static const string kArgOutFmt("outfmt");
/// @}

/// Bundles the fields that can be retrieved for a single row of the
/// ClusterInfoView/ClusterCommonAncestorView views, to be used to populate
/// the requested output format
struct SClusterRow
{
    /// Cluster representative accession
    string representative;
    /// Cluster member accession (not populated/applicable for -taxid and
    /// -accession)
    string member_accession;
    /// NCBI taxonomy ID (member's taxid, or the cluster's common ancestor
    /// taxid when -get-common-ancestor is used)
    TTaxId taxid;
    /// Sequence title (not populated for -get-common-ancestor)
    string title;

    SClusterRow() : taxid(INVALID_TAX_ID) {}
};

/// The application class
class CClusterDBCmdApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CClusterDBCmdApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
        m_StopWatch.Start();
        if (m_UsageReport.IsEnabled()) {
            m_UsageReport.AddParam(CBlastUsageReport::eVersion, GetVersion().Print());
            m_UsageReport.AddParam(CBlastUsageReport::eProgram, (string) "clusterdbcmd");
        }
    }
    ~CClusterDBCmdApp() {
        m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
    }

private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// Opens the connection to the SQLite database provided via -db and
    /// validates that the tables/views required by this application exist
    void x_OpenDb();

    /// Retrieves the members of the cluster identified by -representative
    /// @return 0 on success, 1 if the cluster was not found
    int x_ProcessRepresentative(CNcbiOstream& out, const string& outfmt);

    /// Retrieves the NCBI taxID for the common ancestor of the cluster
    /// identified by -representative
    /// @return 0 on success, 1 if the cluster was not found
    int x_ProcessCommonAncestor(CNcbiOstream& out, const string& outfmt);

    /// Retrieves the cluster representative(s) whose members match -taxid
    /// (or its descendants, unless -exact-match is specified)
    /// @return 0 on success, 1 if no matches were found
    int x_ProcessTaxid(CNcbiOstream& out, const string& outfmt);

    /// Retrieves the cluster representative for the cluster containing
    /// -accession
    /// @return 0 on success, 1 if no matches were found
    int x_ProcessAccession(CNcbiOstream& out, const string& outfmt);

    /// Formats and prints a single row using the provided output format
    /// string
    void x_PrintRow(CNcbiOstream& out, const string& outfmt,
                     const SClusterRow& row);

    /// Validates that the provided output format string only contains
    /// format specifiers applicable to the given mode
    /// @param outfmt output format string as provided by the user [in]
    /// @param allowed set of format specifiers allowed for this mode [in]
    /// @param mode_description human readable description of the current
    /// retrieval mode, used in the exception message if validation fails [in]
    void x_ValidateOutFmt(const string& outfmt, const string& allowed,
                           const string& mode_description);

    /// Detects whether the literal (non-specifier) text in the provided
    /// output format string uses a comma as a field separator, and if so,
    /// sets m_FieldDelimiter accordingly so that fields which may contain
    /// commas (e.g. sequence titles) get quoted in the output
    /// @param outfmt output format string as provided by the user [in]
    void x_DetectFieldDelimiter(const string& outfmt);

    /// Convenience method to determine if this object is dealing with CSV output formats
    inline bool x_IsCsv() const { return m_FieldDelimiter == ","; }

    /// Connection to the SQLite database
    unique_ptr<CSQLITE_Connection> m_DbConn;

    /// Resolved path to the SQLite database
    string m_DbName;

    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;
    string m_FieldDelimiter;   ///< Delimiter character for fields to print.
};

void CClusterDBCmdApp::Init()
{
    HideStdArgs(fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto arg_desc = make_unique<CArgDescriptions>();

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        "Clustered nr BLAST database client, version " + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("Database options");
    arg_desc->AddDefaultKey(kArgDb, "dbname",
                            "Path to the SQLite database containing the "
                            "clustered database metadata (cluster membership, "
                            "taxonomy and common ancestor information)",
                            CArgDescriptions::eString, kDefaultDb);

    arg_desc->SetCurrentGroup("Retrieval options");
    arg_desc->AddOptionalKey(kArgRepresentative, "representative_accession",
                            "Cluster representative accession. Retrieves "
                            "the cluster members. Default outfmt is '%m %t'",
                            CArgDescriptions::eString);

    arg_desc->AddOptionalKey(kArgTaxid, "taxid",
                            "Retrieves the representative ID(s) whose "
                            "cluster contains a member with this taxid (or "
                            "one of its descendant taxids, unless "
                            "-exact-match is specified). If -outfmt is used "
                            "to customize the output, '%m' is not "
                            "applicable. Default outfmt is '%r'",
                            CArgDescriptions::eInteger);
    arg_desc->SetConstraint(kArgTaxid, new CArgAllow_Integers(0, INT_MAX));

    arg_desc->AddOptionalKey(kArgAccession, "accession",
                            "Retrieves the representative ID for the "
                            "cluster that contains this accession. If "
                            "-outfmt is used to customize the output, '%m' "
                            "is not applicable. Default outfmt is '%r %t'",
                            CArgDescriptions::eString);

    arg_desc->SetDependency(kArgRepresentative, CArgDescriptions::eExcludes,
                            kArgTaxid);
    arg_desc->SetDependency(kArgRepresentative, CArgDescriptions::eExcludes,
                            kArgAccession);
    arg_desc->SetDependency(kArgTaxid, CArgDescriptions::eExcludes,
                            kArgAccession);

    arg_desc->AddFlag(kArgGetCommonAncestor,
                      "Only works with -representative. Returns the NCBI "
                      "taxID for the common ancestor of all members in the "
                      "cluster. Default outfmt is '%T', no other output "
                      "format specifier is applicable", true);
    arg_desc->SetDependency(kArgGetCommonAncestor, CArgDescriptions::eRequires,
                            kArgRepresentative);

    arg_desc->AddFlag(kArgExactMatch,
                      "Only applicable with -taxid. If specified, only "
                      "clusters with a member whose taxid exactly matches "
                      "the requested taxid are retrieved (by default, the "
                      "taxid is expanded to its descendant taxids", true);
    arg_desc->SetDependency(kArgExactMatch, CArgDescriptions::eRequires,
                            kArgTaxid);

    arg_desc->SetCurrentGroup("Output options");
    arg_desc->AddDefaultKey(kArgOutput, "output_file", "Output file name",
                            CArgDescriptions::eOutputFile, "-");

    arg_desc->AddOptionalKey(kArgOutFmt, "format",
            "Output format, where the available format specifiers are:\n"
            "\t\t%r means representative\n"
            "\t\t%T means taxid\n"
            "\t\t%m means cluster member\n"
            "\t\t%t means the sequence title\n"
            "\tDefault format depends on the retrieval option used: "
            "'%m %t' for -representative, '%T' for -get-common-ancestor, "
            "'%r' for -taxid, and '%r %t' for -accession.",
            CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}

void CClusterDBCmdApp::x_OpenDb()
{
    const CArgs& args = GetArgs();
    const string& dbname = args[kArgDb].AsString();

    m_DbName = SeqDB_ResolveDbPath(dbname);
    if (m_DbName.empty()) {
        NCBI_THROW(CSeqDBException, eFileErr,
                   "Database '" + dbname + "' not found");
    }

    CSQLITE_Connection::TOperationFlags opFlags =
            CSQLITE_Connection::fExternalMT |
            CSQLITE_Connection::fTempToMemory |
            CSQLITE_Connection::fReadOnly;
    m_DbConn = make_unique<CSQLITE_Connection>(m_DbName, opFlags);

    const vector<pair<string, string>> kRequiredElements {
        { "view", "ClusterInfoView" },
        { "view", "ClusterCommonAncestorView" }
    };
    const string kSqlStmt =
        "SELECT COUNT(*) FROM sqlite_master WHERE type=? and name=?;";
    for (auto& [type, name] : kRequiredElements) {
        auto s = make_unique<CSQLITE_Statement>(&*m_DbConn, kSqlStmt);
        s->Bind(1, type);
        s->Bind(2, name);
        if (s->Step()) {
            if (s->GetInt(0) != 1) {
                NCBI_THROW(CSeqDBException, eArgErr,
                           "Database '" + m_DbName + "' does not have " +
                           type + " " + name + ". Please make sure this is "
                           "a valid clustered metadata database.");
            }
        } else {
            NCBI_THROW(CSeqDBException, eArgErr,
                       "Failed to check for " + type + " " + name + " in '" +
                       m_DbName + "'");
        }
    }
}

void CClusterDBCmdApp::x_ValidateOutFmt(const string& outfmt,
                                        const string& allowed,
                                        const string& mode_description)
{
    for (SIZE_TYPE i = 0; i < outfmt.size(); i++) {
        if (outfmt[i] != '%') {
            continue;
        }
        if (i + 1 >= outfmt.size()) {
            NCBI_THROW(CArgException, eInvalidArg,
                       "Invalid output format string: trailing '%'");
        }
        const char spec = outfmt[++i];
        if (spec == '%') {
            continue;   // escaped literal '%'
        }
        if (allowed.find(spec) == string::npos) {
            NCBI_THROW(CArgException, eInvalidArg,
                       string("Output format specifier '%") + spec +
                       "' is not applicable " + mode_description);
        }
    }
}

void CClusterDBCmdApp::x_DetectFieldDelimiter(const string& outfmt)
{
    for (SIZE_TYPE i = 0; i < outfmt.size(); i++) {
        if (outfmt[i] == '%' && i + 1 < outfmt.size()) {
            i++;   // skip over the format specifier character
            continue;
        }
        if (outfmt[i] == ',') {
            m_FieldDelimiter = ",";
            break;
        }
    }
}

void CClusterDBCmdApp::x_PrintRow(CNcbiOstream& out, const string& outfmt,
                                  const SClusterRow& row)
{
    for (SIZE_TYPE i = 0; i < outfmt.size(); i++) {
        if (outfmt[i] != '%' || i + 1 >= outfmt.size()) {
            out << outfmt[i];
            continue;
        }
        switch (outfmt[++i]) {
        case '%':
            out << '%';
            break;
        case 'r':
            out << row.representative;
            break;
        case 'T':
            out << row.taxid;
            break;
        case 'm':
            out << row.member_accession;
            break;
        case 't':
            out << (x_IsCsv() ? NStr::Quote(row.title) : row.title);
            break;
        default:
            // Should not happen, x_ValidateOutFmt() should have caught this
            out << '%' << outfmt[i];
            break;
        }
    }
    out << NcbiEndl;
}

int CClusterDBCmdApp::x_ProcessRepresentative(CNcbiOstream& out,
                                              const string& outfmt)
{
    const CArgs& args = GetArgs();
    const string& repr = args[kArgRepresentative].AsString();

    const string kSqlStmt =
        "SELECT member_accession, member_taxid, member_title "
        "FROM ClusterInfoView WHERE representative = ? "
        "ORDER BY member_accession DESC;";
    CSQLITE_Statement stmt(&*m_DbConn, kSqlStmt);
    stmt.Bind(1, repr);

    bool found = false;
    while (stmt.Step()) {
        found = true;
        SClusterRow row;
        row.representative = repr;
        row.member_accession = stmt.GetString(0);
        row.taxid = static_cast<TTaxId>(stmt.GetInt(1));
        row.title = stmt.GetString(2);
        x_PrintRow(out, outfmt, row);
    }

    if (!found) {
        ERR_POST(Error << "No cluster found for representative '" << repr << "'");
        return 1;
    }
    return 0;
}

int CClusterDBCmdApp::x_ProcessCommonAncestor(CNcbiOstream& out,
                                              const string& outfmt)
{
    const CArgs& args = GetArgs();
    const string& repr = args[kArgRepresentative].AsString();

    const string kSqlStmt =
        "SELECT taxid FROM ClusterCommonAncestorView WHERE representative = ?;";
    CSQLITE_Statement stmt(&*m_DbConn, kSqlStmt);
    stmt.Bind(1, repr);

    bool found = false;
    while (stmt.Step()) {
        found = true;
        SClusterRow row;
        row.representative = repr;
        row.taxid = static_cast<TTaxId>(stmt.GetInt(0));
        x_PrintRow(out, outfmt, row);
    }

    if (!found) {
        ERR_POST(Error << "No cluster found for representative '" << repr << "'");
        return 1;
    }
    return 0;
}

int CClusterDBCmdApp::x_ProcessTaxid(CNcbiOstream& out, const string& outfmt)
{
    const CArgs& args = GetArgs();
    const int taxid = args[kArgTaxid].AsInteger();

    vector<int> taxids(1, taxid);
    if (!args[kArgExactMatch]) {
        try {
            CTaxonomy4BlastSQLite tb;
            vector<int> descendants;
            tb.GetLeafNodeTaxids(taxid, descendants);
            taxids.insert(taxids.end(), descendants.begin(), descendants.end());
        } catch (const CException& e) {
            ERR_POST(Warning << "Unable to expand taxid " << taxid <<
                     " to its descendant taxids: " << e.GetMsg() <<
                     ". Only exact matches will be retrieved.");
        }
    }

    CNcbiOstrstream oss;
    oss << "SELECT DISTINCT representative, member_taxid, member_title "
           "FROM ClusterInfoView WHERE member_accession = representative "
           "AND member_taxid IN (" << NStr::Join(taxids, ",") << ");";
    const string kSqlStmt = CNcbiOstrstreamToString(oss);

    CSQLITE_Statement stmt(&*m_DbConn, kSqlStmt);

    bool found = false;
    while (stmt.Step()) {
        found = true;
        SClusterRow row;
        row.representative = stmt.GetString(0);
        row.taxid = static_cast<TTaxId>(stmt.GetInt(1));
        row.title = stmt.GetString(2);
        x_PrintRow(out, outfmt, row);
    }

    if (!found) {
        ERR_POST(Error << "No cluster representative found for taxid " << taxid);
        return 1;
    }
    return 0;
}

int CClusterDBCmdApp::x_ProcessAccession(CNcbiOstream& out, const string& outfmt)
{
    const CArgs& args = GetArgs();
    const string& accession = args[kArgAccession].AsString();

    const string kSqlStmt =
        "SELECT representative, member_taxid, member_title "
        "FROM ClusterInfoView WHERE member_accession = ?;";
    CSQLITE_Statement stmt(&*m_DbConn, kSqlStmt);
    stmt.Bind(1, accession);

    bool found = false;
    while (stmt.Step()) {
        found = true;
        SClusterRow row;
        row.representative = stmt.GetString(0);
        row.taxid = static_cast<TTaxId>(stmt.GetInt(1));
        row.title = stmt.GetString(2);
        x_PrintRow(out, outfmt, row);
    }

    if (!found) {
        ERR_POST(Error << "No cluster found for accession '" << accession << "'");
        return 1;
    }
    return 0;
}

int CClusterDBCmdApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();

    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostPrefix("clusterdbcmd");

    try {
        if (!args[kArgRepresentative].HasValue() &&
            !args[kArgTaxid].HasValue() &&
            !args[kArgAccession].HasValue()) {
            NCBI_THROW(CArgException, eNoArg,
                       "Must specify exactly one of -" + kArgRepresentative +
                       ", -" + kArgTaxid + ", or -" + kArgAccession);
        }

        x_OpenDb();
        CNcbiOstream& out = args[kArgOutput].AsOutputFile();

        if (args[kArgRepresentative].HasValue()) {
            if (args[kArgGetCommonAncestor]) {
                const string outfmt = args[kArgOutFmt].HasValue() ?
                    args[kArgOutFmt].AsString() : "%T";
                x_ValidateOutFmt(outfmt, "T",
                                 "when -get-common-ancestor is used");
                x_DetectFieldDelimiter(outfmt);
                status = x_ProcessCommonAncestor(out, outfmt);
                if (m_UsageReport.IsEnabled())
                    m_UsageReport.AddParam(CBlastUsageReport::eDBInfo, (string) "get-common-ancestor");
            } else {
                const string outfmt = args[kArgOutFmt].HasValue() ?
                    args[kArgOutFmt].AsString() : "%m %t";
                x_ValidateOutFmt(outfmt, "rTmt", "for -representative");
                x_DetectFieldDelimiter(outfmt);
                status = x_ProcessRepresentative(out, outfmt);
            }
        } else if (args[kArgTaxid].HasValue()) {
            const string outfmt = args[kArgOutFmt].HasValue() ?
                args[kArgOutFmt].AsString() : "%r";
            x_ValidateOutFmt(outfmt, "rTt", "for -taxid");
            x_DetectFieldDelimiter(outfmt);
            status = x_ProcessTaxid(out, outfmt);
        } else if (args[kArgAccession].HasValue()) {
            const string outfmt = args[kArgOutFmt].HasValue() ?
                args[kArgOutFmt].AsString() : "%r %t";
            x_ValidateOutFmt(outfmt, "rTt", "for -accession");
            x_DetectFieldDelimiter(outfmt);
            status = x_ProcessAccession(out, outfmt);
        }

        if (args[kArgOutFmt].HasValue() && m_UsageReport.IsEnabled()) {
            m_UsageReport.AddParam(CBlastUsageReport::eOutputFmt, args[kArgOutFmt].AsString());
        }
    }
    catch (const CArgException& e) {
        ERR_POST(Error << "Command line argument error: " << e.GetMsg());
        status = 1;
    }
    catch (const CSeqDBException& e) {
        ERR_POST(Error << "Database error: " << e.GetMsg());
        status = 1;
    }
    catch (const CException& e) {
        ERR_POST(Error << e.GetMsg());
        status = 1;
    }
    catch (const std::exception& e) {
        ERR_POST(Error << e.what());
        status = 1;
    }
    catch (...) {
        ERR_POST(Error << "Unknown error");
        status = 1;
    }

    m_UsageReport.AddParam(CBlastUsageReport::eExitStatus, status);
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    return CClusterDBCmdApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
