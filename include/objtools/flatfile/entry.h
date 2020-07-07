#ifndef ENTRY_H
#define ENTRY_H

#include <objtools/flatfile/ftablock.h>

DataBlkPtr LoadEntry(ParserPtr pp, size_t offset, size_t len);

#endif // ENTRY_H
