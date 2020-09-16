#ifndef ENTRY_H
#define ENTRY_H

#include "ftablock.h"
BEGIN_NCBI_SCOPE

DataBlkPtr LoadEntry(ParserPtr pp, size_t offset, size_t len);

END_NCBI_SCOPE

#endif // ENTRY_H
