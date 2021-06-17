#ifndef ALGO_ALIGN_MESSAGES_HPP
#define ALGO_ALIGN_MESSAGES_HPP

/* $Id$ */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

const char g_msg_InvalidTranscriptSymbol[] = "Invalid transcript symbol";

const char g_msg_InvalidBacktraceData[] = "Invalid backtrace data";

const char g_msg_UnexpectedTermIndex[] = "Term index not initialized";

const char g_msg_InvalidAddress[] = "Invalid address specified";

const char g_msg_InvalidSequenceChars[] = 
    "Invalid sequence character(s) detected";

const char g_msg_InvalidSpliceTypeIndex[] = "Invalid splice type index";

const char g_msg_NullParameter[] = "Null pointer or data passed";

const char g_msg_GuideCoreSizeTooLong[] = "Guide core is too long";

const char g_msg_InconsistentArguments[] = "Inconsistent arguments";

const char g_msg_DataNotAvailable[] = 
    "Sequence data not available for one or both sequences";

const char g_msg_OutOfSpace[] = "Out of space";

const char g_msg_HitSpaceLimit[] = "Space limit exceeded";

const char g_msg_IntronTooLong[] = "Cannot handle introns longer than 1MB";

const char g_msg_NoAlignment[] = "Sequence not aligned yet";

const char g_msg_GapsOnly[] = "Alignment consists of gaps only";

END_NCBI_SCOPE

#endif
