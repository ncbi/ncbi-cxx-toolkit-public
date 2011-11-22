#ifndef CONNECT___NCBI_CONN_TEST__HPP
#define CONNECT___NCBI_CONN_TEST__HPP

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
 * Author:  Anton Lavrentiev
 *
 * @file
 * File Description:
 *   NCBI connectivity test suite.
 *
 */

#include <connect/ncbi_conn_stream.hpp>


/** @addtogroup Utility
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XCONNECT_EXPORT CConnTest : protected CConnIniter
{
public:
    /// Note that each stage has a previous one as a prerequisite, with the
    /// only exception for the stateful service (last check) that may work
    /// when forced into the stateless mode even if the firewall connections
    /// (in preceding check) have been found non-operational...
    ///
    enum EStage {
        eHttp,                  ///< Check whether HTTP works
        eDispatcher,            ///< Check whether NCBI dispatcher works
        eStatelessService,      ///< Check whether simplest NCBI service works
        eFirewallConnPoints,    ///< Obtain all FW ports for stateful services
        eFirewallConnections,   ///< Check all FW ports one by one
        eStatefulService        ///< Check whether NCBI stateful service works
    };

    /// Create test suite
    /// @param timeout
    ///  non-NULL pointer to a finite timeout, or NULL (infinite); or
    ///  default timeout (which is 30 seconds).
    /// @param out
    ///  test results get posted to the stream pointed to by this parameter;
    ///  no output is produced if "out" is NULL.
    ///
    CConnTest(const STimeout* timeout = kDefaultTimeout,
              CNcbiOstream* output = 0, SIZE_TYPE width = 72);

    void     SetWidth(SIZE_TYPE width = 72)
    { m_Width = width; }

    void     SetOutput(CNcbiOstream* output = 0)
    { m_Output = output; }

    void     SetTimeout(const STimeout* timeout = kDefaultTimeout);

    virtual ~CConnTest() { /*nothing*/ }

    /// Execute the test suite from the very first (eHttp) up to
    ///  and including the requested stage "stage".
    ///
    /// It is expected that the call advances to the next check only
    /// if the previous one was successful (or conditionally successful,
    /// meaning even though it may have formally failed, it still returns
    /// eIO_Success and creates favorable preconditions for the following
    /// check to likely succeed).
    ///
    /// @param stage
    ///  final stage requested or performed, when the call returns
    /// @param reason
    ///  a pointer to a string to get a failure explanation
    /// @return
    ///  eIO_Success if all requested tests completed successfully;
    ///  other code if not, and then also return an explanatory
    ///  message at "*reason" (if "reason" passed non-NULL).
    ///
    /// NOTE that "*reason" may contain non-empty string when the call
    /// completes successfully.  Always check return code, instead of
    /// making any assumption on the contents of "*reason".
    ///
    /// @note
    ///  Running the test for a firewall mode client will globally modify the
    ///  behavior of service-related stateful data retrievals.  That is, when
    ///  the test is complete, only the safest route found operational will be
    ///  used to connect to NCBI for the life of the program.  Re-running the
    ///  test may clobber these results and, thus, is highly unrecommended!
    ///
    virtual EIO_Status Execute(EStage& stage, string* reason = 0);

    ///
    virtual void       Cancel(void);

protected:

    /// Auxiliary class to hold FWDaemon CP(connection point)
    /// information and its current status.
    struct CFWConnPoint {
        unsigned int   host;  ///< Network byte order
        unsigned short port;  ///< Host byte order
        EIO_Status   status;  ///< Check result

        bool operator < (const CFWConnPoint& p) const
        { return port < p.port; }
    };

    /// User-redefinable checks for each stage.
    ///
    /// Every check must include at least one call of PreCheck()
    /// followed by PostCheck() with parameter "step" set to 0
    /// (meaning the "main" check); and as many as necessary optional
    /// substeps (nested or going back to back) enumerated with "step".
    /// A check returning eIO_Success means that its main check
    /// with all substeps have successfully passed.  Otherwise, it is a
    /// failing check that can return an explanation via the "reason"
    /// pointer (if non-NULL) or at least clear the string.

    virtual EIO_Status HttpOkay          (string* reason);
    virtual EIO_Status DispatcherOkay    (string* reason);
    virtual EIO_Status ServiceOkay       (string* reason);
    virtual EIO_Status GetFWConnections  (string* reason);
    virtual EIO_Status CheckFWConnections(string* reason);
    virtual EIO_Status StatefulOkay      (string* reason);

    /// User-defined rendering callbacks:  PreCheck() and PostCheck().
    /// Each callback receives a stage enumerator and a step within.
    /// At least one step (0) is required and denotes the main check.

    /// PreCheck gets called before the step starts, with the "title"
    /// containing either:
    ///   a single-lined step title; or
    ///   a multi-lined step description:  the first line being the actual
    ///   title, and remaining lines -- a verbal explanation.
    /// Lines are separated with '\n', and normally do not have any
    /// ending punctuation (but may be capitalized).
    /// The default callback does the following:
    ///   For the single-lined titles, it outputs the title into the output
    ///     stream (if provided in ctor), and then puts the ellipsis (...)
    ///     without an ending newline;
    ///   For the multi-lined description, the title is printed on
    ///     the first line, and then each line of the description follows
    ///     as a justified paragraph.  Last paragraph ends with a new line.
    /// Each PreCheck() is expected to reset the m_End member to "false".
    ///
    virtual void       PreCheck (EStage stage, unsigned int step,
                                 const string& title);

    /// PostCheck gets called upon successful ("status" contains eIO_Success)
    /// or unsuccessful (otherwise) completion of any step, with "reason"
    /// having an explanation in either case.  Successful completion
    /// expected to supply only a single line via the "reason" parameter;
    /// while a failing check can supply multiple lines (as an extended
    /// detailed explanation) separated with '\n'.
    /// The default callback does the following:
    ///   For a succeeding check it prints contents of "reason" and returns;
    ///   For a failing check, it prints the word "FAILED" followed by textual
    ///     representation of "status" and the return value of GetCheckPoint()
    ///     if non-empty.  It then prints each line of explanation (taken from
    ///     "reason") as a numbered list of justified paragraphs.  If there is
    ///     only a single line results, it won't be enumerated.
    /// Each PostCheck() is expected to set the m_End member to "true",
    /// so that the nested or back-to-back substeps can be distiguished
    /// by the order of encounter of m_End's values.
    ///
    virtual void       PostCheck(EStage stage, unsigned int step,
                                 EIO_Status status, const string& reason);

    /// Helper function that assures to return eIO_Success if the predicate
    /// "failure" is false;  and other code otherwise:  either taken from the
    /// underlying CONN object, or eIO_Unknown as a fallback.
    /// Also, it sets the m_CheckPoint member to contain the connection
    /// description if available (retrievable with GetCheckPoint()).
    ///
    virtual EIO_Status ConnStatus(bool failure, CConn_IOStream* io);

    /// Extended info of the last step IO
    const string&      GetCheckPoint(void) const { return m_CheckPoint; }

    /// Default timeout value
    static const STimeout kTimeout;

    /// As supplied in constructor
    const STimeout*       m_Timeout;
    CNcbiOstream*         m_Output;
    SIZE_TYPE             m_Width;

    /// Certain properties of communication as determined by configuration
    bool                  m_HttpProxy;
    bool                  m_Stateless;
    bool                  m_Firewall;

    /// Firewall daemon configuration
    vector<CFWConnPoint>  m_Fwd;    ///< Regular connection points
    vector<CFWConnPoint>  m_FwdFB;  ///< Fallback connection points

    /// Check step start / stop indicator
    bool                  m_End;

private:
    CConn_IOStream*       m_IO;
    volatile bool         m_Canceled;
    string                m_CheckPoint;
    STimeout              m_TimeoutStorage;

    /// Pretect from runaway stage argument
    EIO_Status x_CheckTrap(string* reason);
    /// Return timeout suggestion
    string     x_TimeoutMsg(void);
    /// Obtain and populate FWD connection points
    EIO_Status x_GetFirewallConfiguration(const SConnNetInfo* net_info);

public:
    /// Return TRUE if the client is inside NCBI, FALSE otherwise.
    /// @note  Do not use this method anywhere else other than when deciding
    ///        whether to proceed with Execute() in this class with more
    ///        thorough connection checks!
    static bool IsNcbiInhouseClient(void);
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_CONN_TEST__HPP */
