#ifndef ALGO_ALIGN_MESSAGES_HPP
#define ALGO_ALIGN_MESSAGES_HPP

/* $Id$ */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

const char g_msg_InvalidTranscriptSymbol[] = "Invalid transcript symbol";

const char g_msg_InvalidBacktraceData[] = "Invalid backtrace data";

const char g_msg_InvalidAddress[] = "Invalid address specified";

const char g_msg_InvalidSequenceChars[] = 
    "Invalid sequence character(s) detected";

const char g_msg_InvalidSpliceTypeIndex[] = "Invalid splice type index";

const char g_msg_NullParameter[] = "Null pointer or data passed";

const char g_msg_InconsistentArguments[] = "Inconsistent arguments";

const char g_msg_DataNotAvailable[] = 
    "Sequence data not available for one or both sequences";

const char g_msg_OutOfSpace[] = "Out of space";

const char g_msg_NoAlignment[] = "Sequence not aligned yet";


END_NCBI_SCOPE

/*
 * $Log$
 * Revision 1.1  2004/11/29 14:37:15  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 */

#endif
