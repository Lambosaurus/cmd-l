#ifndef COMMAND_PARSE_H
#define COMMAND_PARSE_H

#include "CmdConf.h"

#include <stdint.h>
#include <stdbool.h>

/*
 * PUBLIC DEFINITIONS
 */

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

bool Cmd_ParseNumber(const char ** str, uint32_t * value);

#ifdef CMD_USE_STRING_ARGS
bool Cmd_ParseString(const char ** str, char * value, uint32_t size, uint32_t * count);
uint32_t Cmd_FormatString(char * dst, uint32_t size, uint8_t * data, uint32_t count, char delimiter);
#endif

#ifdef CMD_USE_BYTE_ARGS
bool Cmd_ParseBytes(const char ** str, uint8_t * value, uint32_t size, uint32_t * count);
uint32_t Cmd_FormatBytes(char * dst, uint8_t * data, uint32_t count, char space);
#endif

#endif //COMMAND_PARSE_H
