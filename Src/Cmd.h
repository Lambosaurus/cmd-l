#ifndef COMMAND_H
#define COMMAND_H

#include "CmdConf.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * PUBLIC DEFINITIONS
 */

#define LENGTH(x)		(sizeof(x) / sizeof(*(x)))

// Macros for creating menus.
#define CMD_ARGUMENT(_type, _name) 		\
	{									\
		.type = _type,					\
		.name = _name					\
	}

#define CMD_FUNCTION(_name, _callback)	\
	{									\
		.type = Cmd_Node_Function, 		\
		.name = _name,					\
		.func = {						\
			.callback = _callback,		\
			.arglen = 0					\
		} 								\
	}

#define CMD_AFUNCTION(_name, _callback, _arglist) \
	{									\
		.type = Cmd_Node_Function, 		\
		.name = _name,					\
		.func = {						\
			.callback = _callback,		\
			.args = _arglist,			\
			.arglen = LENGTH(_arglist)	\
		} 								\
	}

#define CMD_MENU(_name, _nodelist)		\
	{									\
		.type = Cmd_Node_Menu,			\
		.name = _name,					\
		.menu = {						\
			.nodes = _nodelist,			\
			.count = LENGTH(_nodelist)	\
		}								\
	}

/*
 * PUBLIC TYPES
 */

typedef enum {
	Cmd_Arg_Number,
#ifdef CMD_USE_BOOL_ARGS
	Cmd_Arg_Bool,
#endif
#ifdef CMD_USE_BYTE_ARGS
	Cmd_Arg_Bytes,
#endif
#ifdef CMD_USE_STRING_ARGS
	Cmd_Arg_String,
#endif
	Cmd_Arg_Mask = 0x7F,
	Cmd_Arg_Optional = 0x80,
} Cmd_ArgType_t;

typedef enum {
	Cmd_Node_Function,
	Cmd_Node_Menu,
} Cmd_NodeType_t;

typedef enum {
	Cmd_Reply_Info,
	Cmd_Reply_Warn,
	Cmd_Reply_Error,
} Cmd_ReplyLevel_t;

typedef struct Cmd_Node_s Cmd_Node_t;
typedef struct Cmd_Line_s Cmd_Line_t;

typedef struct {
	const char * name;
	uint8_t type; // Cmd_ArgType_t
} Cmd_Arg_t;

typedef struct {
	union {
		uint32_t number;
#ifdef CMD_USE_BOOL_ARGS
		bool boolean;
#endif
#ifdef CMD_USE_BYTE_ARGS
		struct {
			uint32_t size;
			uint8_t * data;
		}bytes;
#endif
#ifdef CMD_USE_STRING_ARGS
		const char * str;
#endif
	};
	bool present;
} Cmd_ArgValue_t;

typedef struct Cmd_Node_s {
	const char * name;
	uint8_t type; // Cmd_NodeType_t
	union {
		struct {
			const Cmd_Node_t ** nodes;
			uint32_t count;
		}menu;
		struct {
			const Cmd_Arg_t * args;
			uint32_t arglen;
			void (*callback)(Cmd_Line_t * line, Cmd_ArgValue_t * argv);
		}func;
	};
} Cmd_Node_t;

typedef struct
{
#ifdef CMD_USE_COLOR
		bool color;
#endif
#ifdef CMD_USE_BELL
		bool bell;
#endif
#ifdef CMD_USE_ECHO
		bool echo;
#endif
#ifdef CMD_PROMPT
		bool prompt;
#endif
} Cmd_LineConfig_t;

typedef struct Cmd_Line_s {
	struct {
		uint32_t index;
		uint32_t size;
		char * data;
		uint32_t recall_index;
	}bfr;
	void (*print)(const uint8_t * data, uint32_t size);
	const Cmd_Node_t * root;
	struct {
		void * heap;
		uint32_t size;
		void * head;
	}mem;
	Cmd_LineConfig_t cfg;
	char last_ch;
#ifdef CMD_USE_ANSI
	uint8_t ansi; // Cmd_AnsiState_t
#endif
} Cmd_Line_t;

/*
 * PUBLIC FUNCTIONS
 */

// Initialise the command line module.
// The heap is used for holding arguments and lines. It should be approximately 4x the maximum line size.
void Cmd_Init(Cmd_Line_t * line, const Cmd_Node_t * root, void (*print)(const uint8_t * data, uint32_t size), void * heap, uint32_t heapSize);

// This starts a new 'session' discarding any previous state
// The prompt will be re-printed if enabled. This is required to print the prompt first time, as Cmd_Init will not.
// Calling this before Cmd_Parse is NOT required
void Cmd_Start(Cmd_Line_t * line);

// Parses incoming data. This can parse partial or multiple lines.
void Cmd_Parse(Cmd_Line_t * line, const uint8_t * data, uint32_t count);

// Commands can use these for putting formatted responses back on the command line.
void Cmd_Print(Cmd_Line_t * line, Cmd_ReplyLevel_t level, const char * data, uint32_t count);
void Cmd_Prints(Cmd_Line_t * line, Cmd_ReplyLevel_t level, const char * str);
void Cmd_Printf(Cmd_Line_t * line, Cmd_ReplyLevel_t level, const char * fmt, ...);

// Used internally for accessing the command heap. This may be used for commands.
// Note: this is not a smart heap. Items should be freed in order.
void * Cmd_Malloc(Cmd_Line_t * line, uint32_t size);
void Cmd_Free(Cmd_Line_t * line, void * ptr);

#endif //COMMAND_H
