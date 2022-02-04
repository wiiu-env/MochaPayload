#ifndef IMPORTS_H
#define IMPORTS_H

#include "types.h"
#include <stdarg.h>
#include <stdlib.h>

#define MCP_SVC_BASE ((void *) 0x050567EC)

void usleep(u32 time);

#endif
