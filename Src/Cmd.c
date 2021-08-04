
#include "Cmd.h"
#include "CmdParse.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>


/*
 * PRIVATE DEFINITIONS
 */

#define DEL				0x7F

#define LF				CMD_LINE_END

/*
 * PRIVATE TYPES
 */

typedef struct {
	const char * str;
	char delimiter;
	uint32_t size;
}Cmd_Token_t;

typedef enum {
	Cmd_Token_Ok,
	Cmd_Token_Empty,
	Cmd_Token_Broken,
} Cmd_TokenStatus_t;

#ifdef CMD_USE_ANSI
typedef enum {
	Cmd_Ansi_None,
	Cmd_Ansi_Escaped,
	Cmd_Ansi_CSI,
} Cmd_AnsiState_t;
#endif

/*
 * PRIVATE PROTOTYPES
 */

static void Cmd_FreeAll(Cmd_Line_t * line);
static uint32_t Cmd_MemRemaining(Cmd_Line_t * line);

static Cmd_TokenStatus_t Cmd_ParseToken(const char ** str, Cmd_Token_t * token);
static Cmd_TokenStatus_t Cmd_NextToken(Cmd_Line_t * line, const char ** str, Cmd_Token_t * token);
static bool Cmd_ParseArg(Cmd_Line_t * line, const Cmd_Arg_t * arg, Cmd_ArgValue_t * value, Cmd_Token_t * token);
static const char * Cmd_ArgTypeStr(Cmd_Line_t * line, const Cmd_Arg_t * arg);

static void Cmd_Run(Cmd_Line_t * line, const Cmd_Node_t * node, const char * str);
static void Cmd_RunRoot(Cmd_Line_t * line, const char * str);
static void Cmd_RunMenu(Cmd_Line_t * line, const Cmd_Node_t * node, const char * str);
static void Cmd_RunFunction(Cmd_Line_t * line, const Cmd_Node_t * node, const char * str);

#ifdef CMD_HELP_TOKEN
static void Cmd_PrintMenuHelp(Cmd_Line_t * line, const Cmd_Node_t * node);
static void Cmd_PrintFunctionHelp(Cmd_Line_t * line, const Cmd_Node_t * node);
#endif

#ifdef CMD_USE_BELL
static void Cmd_Bell(Cmd_Line_t * line);
#endif

#ifdef CMD_USE_TABCOMPLETE
static const char * Cmd_TabComplete(Cmd_Line_t * line, const char * str);
static const char * Cmd_TabCompleteMenu(Cmd_Line_t * line, const Cmd_Node_t * node, const char * str);
#endif

#ifdef CMD_USE_ANSI
static void Cmd_HandleAnsi(Cmd_Line_t * line, char ch);
static void Cmd_ClearLine(Cmd_Line_t * line);
static void Cmd_RecallLine(Cmd_Line_t * line);
#endif

/*
 * PRIVATE VARIABLES
 */

/*
 * PUBLIC FUNCTIONS
 */

void Cmd_Init(Cmd_Line_t * line, const Cmd_Node_t * root, void (*print)(const uint8_t * data, uint32_t size), void * heap, uint32_t heapSize)
{
	line->bfr.data = heap;
	line->bfr.size = CMD_MAX_LINE;
	line->root = root;
	line->print = print;

	line->mem.heap = heap + CMD_MAX_LINE;
	line->mem.size = heapSize - CMD_MAX_LINE;
	line->mem.head = line->mem.heap;

	memset(&line->cfg, 0, sizeof(line->cfg));

	// Note, do not set properties that will be set by Cmd_Start.
	Cmd_Start(line);
}

void Cmd_Start(Cmd_Line_t * line)
{
	line->bfr.index = 0;
	line->bfr.recall_index = 0;
	line->last_ch = 0;
#ifdef CMD_USE_ANSI
	line->ansi = Cmd_Ansi_None;
#endif
#ifdef CMD_PROMPT
	if (line->cfg.prompt)
	{
		line->print((uint8_t *)CMD_PROMPT, strlen(CMD_PROMPT));
	}
#endif //CMD_PROMPT
}

void Cmd_Parse(Cmd_Line_t * line, const uint8_t * data, uint32_t count)
{
#ifdef CMD_USE_ECHO
	const uint8_t * echo_data = data;
#endif //CMD_USE_ECHO

	while(count--)
	{
		char ch = *data++;

#ifdef CMD_USE_ANSI
		if (line->ansi != Cmd_Ansi_None)
		{
			Cmd_HandleAnsi(line, ch);
#ifdef CMD_USE_ECHO
			// Do not echo ansi sequences
			echo_data = data;
#endif //CMD_USE_ECHO
		}
		else
#endif //CMD_USE_ANSI
		{
			switch (ch)
			{
			case '\n':
				if (line->last_ch == '\r')
				{
					// completion of a \r\n.
					break;
				}
				// fallthrough
			case '\r':
			case 0:
#ifdef CMD_USE_ECHO
				if (line->cfg.echo)
				{
					// Print everything up until now excluding the current char
					line->print(echo_data, (data - echo_data) - 1);
					echo_data = data;
					// Now print a full eol.
					line->print((uint8_t *)LF, 2);
				}
#endif //CMD_USE_ECHO
				if (line->bfr.index)
				{
					// null terminate command and run it.
					line->bfr.data[line->bfr.index] = 0;
					Cmd_RunRoot(line, line->bfr.data);
					line->bfr.index = 0;
				}
#ifdef CMD_PROMPT
				if (line->cfg.prompt)
				{
					line->print((uint8_t *)CMD_PROMPT, strlen(CMD_PROMPT));
				}
#endif //CMD_PROMPT
				break;
#ifdef CMD_USE_TABCOMPLETE
			case '\t':
				line->bfr.data[line->bfr.index] = 0;
				const char * append = Cmd_TabComplete(line, line->bfr.data);
				if (append != NULL)
				{
					uint32_t append_count = strlen(append);
					uint32_t newindex = line->bfr.index + append_count;
					if (newindex < line->bfr.size)
					{
						// A tab complete should not overflow the line buffer.
						memcpy(line->bfr.data + line->bfr.index, append, append_count);
						line->bfr.index += append_count;
						line->bfr.recall_index = line->bfr.index;
						line->print((uint8_t *)append, append_count);
					}
				}
#ifdef CMD_USE_BELL
				else
				{
					Cmd_Bell(line);
				}
#endif //CMD_USE_BELL
#ifdef CMD_USE_ECHO
				if (line->cfg.echo)
				{
					// Print everything up until now excluding the current char
					// This is needed to swallow the \t char.
					line->print(echo_data, (data - echo_data) - 1);
					echo_data = data;
				}
#endif //CMD_USE_ECHO
				break;
#endif //CMD_USE_TABCOMPLETE
			case DEL:
				if (line->bfr.index)
				{
					line->bfr.index--;
				}
#ifdef CMD_USE_BELL
				else
				{
					Cmd_Bell(line);
				}
#endif //CMD_USE_BELL
				break;
#ifdef CMD_USE_ANSI
			case '\e':
#ifdef CMD_USE_ECHO
				if (line->cfg.echo)
				{
					// Swallow this char.
					line->print(echo_data, (data - echo_data) - 1);
					echo_data = data;
				}
#endif //CMD_USE_ECHO
				line->ansi = Cmd_Ansi_Escaped;
				break;
#endif // CMD_USE_ANSI
			default:
				if (line->bfr.index < line->bfr.size - 1)
				{
					// Need to leave room for at least a null char.
					line->bfr.data[line->bfr.index++] = ch;
				}
				else
				{
					// Discard the line
					line->bfr.index = 0;
				}
				line->bfr.recall_index = line->bfr.index;
				break;
			}
			line->last_ch = ch;
		}
	}
#ifdef CMD_USE_ECHO
	if (line->cfg.echo && echo_data < data)
	{
		line->print(echo_data, data - echo_data);
	}
#endif //CMD_USE_ECHO
}

void Cmd_Print(Cmd_Line_t * line, Cmd_ReplyLevel_t level, const char * data, uint32_t count)
{
#ifdef CMD_USE_COLOR
	if (line->cfg.color)
	{
		switch (level)
		{
		case Cmd_Reply_Warn:
			line->print((uint8_t *)"\x00\x1b[33m", 6);
			break;
		case Cmd_Reply_Error:
			line->print((uint8_t *)"\x00\x1b[31m", 6);
			break;
		case Cmd_Reply_Info:
			break;
		}
	}
#endif //CMD_USE_COLOR
	line->print((uint8_t *)data, count);
#ifdef CMD_USE_COLOR
	if (line->cfg.color)
	{
		switch (level)
		{
		case Cmd_Reply_Warn:
		case Cmd_Reply_Error:
			line->print((uint8_t *)"\x00\x1b[0m", 5);
			break;
		case Cmd_Reply_Info:
			break;
		}
	}
#endif //CMD_USE_COLOR
#ifdef CMD_USE_BELL
	if (level == Cmd_Reply_Error)
	{
		Cmd_Bell(line);
	}
#endif //CMD_USE_BELL
}

void Cmd_Prints(Cmd_Line_t * line, Cmd_ReplyLevel_t level, const char * str)
{
	Cmd_Print(line, level, str, strlen(str));
}

void Cmd_Printf(Cmd_Line_t * line, Cmd_ReplyLevel_t level, const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    // Take whatevers left - because we will immediately free it.
    uint32_t free = Cmd_MemRemaining(line);
    char * bfr = Cmd_Malloc(line, free);
    uint32_t written = vsnprintf(bfr, free, fmt, ap);
    va_end(ap);
    Cmd_Print(line, level, bfr, written);
    Cmd_Free(line, bfr);
}

void * Cmd_Malloc(Cmd_Line_t * line, uint32_t size)
{
	if (Cmd_MemRemaining(line) < size)
	{
		Cmd_Prints(line, Cmd_Reply_Error, "MEMORY OVERRUN" LF);
	}
	// Ignore overrun and do it anyway.....
	void * ptr = line->mem.head;
	line->mem.head += size;
	return ptr;
}

void Cmd_Free(Cmd_Line_t * line, void * ptr)
{
	if (ptr >= line->mem.heap)
	{
		line->mem.head = ptr;
	}
}

/*
 * PRIVATE FUNCTIONS
 */

static void Cmd_FreeAll(Cmd_Line_t * line)
{
	line->mem.head = line->mem.heap;
}

uint32_t Cmd_MemRemaining(Cmd_Line_t * line)
{
	int32_t rem = line->mem.size - (line->mem.head - line->mem.heap);
	if (rem < 0) { return 0; }
	return rem;
}

static Cmd_TokenStatus_t Cmd_ParseToken(const char ** str, Cmd_Token_t * token)
{
	const char * head = *str;
	while (1)
	{
		// Find the first char
		char ch = *head;
		if (ch == ' ' || ch == '\t')
		{
			head++;
		}
		else if (ch == 0)
		{
			return Cmd_Token_Empty;
		}
		else
		{
			break;
		}
	}
	char startc = *head;
	if (startc == '"' || startc == '\'' || startc == '[' || startc == '<')
	{
		token->delimiter = startc;
		if (startc == '<' || startc == '[')
		{
			// Close braces are always 2 chars after open.
			startc += 2;
		}
		// Check if its a quoted token
		head++;
		token->str = head;
		bool escaped = false;
		while (1)
		{
			char ch = *head;

			if (ch == 0)
			{
				return Cmd_Token_Broken;
			}
			else if (escaped)
			{
				// Do not consider the following character as esc or end
				escaped = false;
			}
			else if (ch == '\\')
			{
				escaped = true;
			}
			else if (ch == startc)
			{
				break;
			}
			head++;
		}

		token->size = head - token->str;
		head++;
	}
	else // Non quoted token
	{
		token->delimiter = 0;
		token->str = head;
		while (1)
		{
			char ch = *head;
			if (ch == ' ' || ch == '\t' || ch == 0)
			{
				break;
			}
			head++;
		}

		token->size = head - token->str;
	}

	*str = head;

	return Cmd_Token_Ok;
}

static Cmd_TokenStatus_t Cmd_NextToken(Cmd_Line_t * line, const char ** str, Cmd_Token_t * token)
{
	Cmd_TokenStatus_t status = Cmd_ParseToken(str, token);
	if (status == Cmd_Token_Ok)
	{
		// Copy token from a ref to an allocated buffer
		char * bfr = Cmd_Malloc(line, token->size + 1);
		memcpy(bfr, token->str, token->size);
		bfr[token->size] = 0;
		token->str = bfr;
	}
	return status;
}

static bool Cmd_ParseArg(Cmd_Line_t * line, const Cmd_Arg_t * arg, Cmd_ArgValue_t * value, Cmd_Token_t * token)
{
	const char * str = token->str;
	switch (arg->type & Cmd_Arg_Mask)
	{
	case Cmd_Arg_Number:
		return Cmd_ParseNumber(&str, &value->number) && (*str == 0);
#ifdef CMD_USE_BOOL_ARGS
	case Cmd_Arg_Bool:
	{
		uint32_t n;
		bool success = Cmd_ParseNumber(&str, &n) && (*str == 0);
		value->boolean = n;
		return success;
	}
#endif //CMD_USE_BOOL_ARGS
#ifdef CMD_USE_BYTE_ARGS
	case Cmd_Arg_Bytes:
	{
		uint32_t maxbytes = token->size + 1;
		uint8_t * bfr = Cmd_Malloc(line, maxbytes);
		value->bytes.data = bfr;
		char delim = token->delimiter;
		if (delim == '"' || delim == '\'')
		{
			return Cmd_ParseString(&str, (char *)bfr, maxbytes, &value->bytes.size) && (*str == 0);
		}
		else
		{
			return Cmd_ParseBytes(&str, bfr, maxbytes, &value->bytes.size) && (*str == 0);
		}
	}
#endif //CMD_USE_BYTE_ARGS
#ifdef CMD_USE_STRING_ARGS
	case Cmd_Arg_String:
	{
		uint32_t maxbytes = token->size + 1; // null char.
		char * bfr = Cmd_Malloc(line, maxbytes);
		value->str = bfr;
		return Cmd_ParseString(&str, bfr, maxbytes, &maxbytes) && (*str == 0);
	}
#endif //CMD_USE_STRING_ARGS
	default:
		return false;
	}
}

static const char * Cmd_ArgTypeStr_Internal(uint8_t type)
{
	switch (type)
	{
	case Cmd_Arg_Number:
		return "number";
#ifdef CMD_USE_BOOL_ARGS
	case Cmd_Arg_Bool:
		return "boolean";
#endif
#ifdef CMD_USE_BYTE_ARGS
	case Cmd_Arg_Bytes:
		return "bytes";
#endif
#ifdef CMD_USE_STRING_ARGS
	case Cmd_Arg_String:
		return "string";
#endif
	default:
		return "UNKNOWN";
	}
}

static const char * Cmd_ArgTypeStr(Cmd_Line_t * line, const Cmd_Arg_t * arg)
{
	if (arg->type & Cmd_Arg_Optional)
	{
		const char * str = Cmd_ArgTypeStr_Internal(arg->type & Cmd_Arg_Mask);
		uint32_t size = strlen(str);
		char * bfr = Cmd_Malloc(line, size + 2);
		memcpy(bfr, str, size);
		bfr[size] = '?';
		bfr[size + 1] = 0;
		return bfr;
	}
	return Cmd_ArgTypeStr_Internal(arg->type);
}

static void Cmd_Run(Cmd_Line_t * line, const Cmd_Node_t * node, const char * str)
{
	switch (node->type)
	{
	case Cmd_Node_Menu:
		Cmd_RunMenu(line, node, str);
		break;
	case Cmd_Node_Function:
		Cmd_RunFunction(line, node, str);
		break;
	}
}

static void Cmd_RunRoot(Cmd_Line_t * line, const char * str)
{
	Cmd_Run(line, line->root, str);
	Cmd_FreeAll(line);
}

static void Cmd_RunMenu(Cmd_Line_t * line, const Cmd_Node_t * node, const char * str)
{
	Cmd_Token_t token;
	switch(Cmd_NextToken(line, &str, &token))
	{
	case Cmd_Token_Empty:
		Cmd_Printf(line, Cmd_Reply_Info, "<menu: %s>" LF, node->name);
		return;
	case Cmd_Token_Broken:
		Cmd_Prints(line, Cmd_Reply_Error, "Incomplete token found" LF);
		return;
	case Cmd_Token_Ok:
		break; // Continue execution.
	}

#ifdef CMD_HELP_TOKEN
	if (strcmp(CMD_HELP_TOKEN, token.str) == 0)
	{
		Cmd_PrintMenuHelp(line, node);
	}
	else
#endif //CMD_HELP_TOKEN
	{
		const Cmd_Node_t * selected = NULL;
		for (uint32_t i = 0; i < node->menu.count; i++)
		{
			const Cmd_Node_t * child = node->menu.nodes[i];
			if (strcmp(child->name, token.str) == 0)
			{
				selected = child;
				break;
			}
		}
		if (selected == NULL)
		{
			Cmd_Printf(line, Cmd_Reply_Error, "'%s' is not an item within <menu: %s>" LF, token.str, node->name);
		}
		else
		{
			// we may as well free this token before we run the next menu.
			Cmd_Free(line, (void*)token.str);
			Cmd_Run(line, selected, str);
		}
	}
}

static void Cmd_RunFunction(Cmd_Line_t * line, const Cmd_Node_t * node, const char * str)
{
	Cmd_ArgValue_t args[CMD_MAX_ARGS + 1]; // We will parse an extra as a test.
	uint32_t argn = 0;

	Cmd_Token_t token;
	Cmd_TokenStatus_t tstat = Cmd_NextToken(line, &str, &token);

#ifdef CMD_HELP_TOKEN
	if (tstat == Cmd_Token_Ok && strcmp(CMD_HELP_TOKEN, token.str) == 0)
	{
		Cmd_PrintFunctionHelp(line, node);
		return;
	}
#endif

	for (argn = 0; argn < node->func.arglen; argn++)
	{
		const Cmd_Arg_t * arg = &node->func.args[argn];

		if (tstat == Cmd_Token_Ok)
		{
			if (Cmd_ParseArg(line, arg, args + argn, &token))
			{
				args[argn].present = true;
				// Check if there is a following token
				tstat = Cmd_NextToken(line, &str, &token);
				continue;
			}
		}
		else if (tstat == Cmd_Token_Empty)
		{
			if (arg->type & Cmd_Arg_Optional)
			{
				// Stop scanning for args without an error.
				// Assume all following arguments are also optional.
				break;
			}
		}
		else
		{
			Cmd_Prints(line, Cmd_Reply_Error, "Incomplete token found" LF);
			return;
		}

		// Parse failed or blank token found.
		Cmd_Printf(line, Cmd_Reply_Error, "Argument %d is <%s: %s>" LF, argn+1, Cmd_ArgTypeStr(line, arg), arg->name);
		return;
	}
	for (; argn < node->func.arglen; argn++)
	{
		// Any remaining nodes are not present.
		args[argn].present = false;
	}

	if (tstat != Cmd_Token_Empty)
	{
		Cmd_Printf(line, Cmd_Reply_Error, "<func: %s> takes maximum %d arguments" LF, node->name, node->func.arglen);
	}
	else
	{
		node->func.callback(line, args);
	}
}

#ifdef CMD_HELP_TOKEN
static void Cmd_PrintMenuHelp(Cmd_Line_t * line, const Cmd_Node_t * node)
{
	Cmd_Printf(line, Cmd_Reply_Info, "<menu: %s> contains %d nodes:" LF, node->name, node->menu.count);
	for (uint32_t i = 0; i < node->menu.count; i++)
	{
		const Cmd_Node_t * child = node->menu.nodes[i];
		Cmd_Printf(line, Cmd_Reply_Info, " - %s" LF, child->name);
	}
}

static void Cmd_PrintFunctionHelp(Cmd_Line_t * line, const Cmd_Node_t * node)
{
	Cmd_Printf(line, Cmd_Reply_Info, "<func: %s> takes %d arguments:" LF, node->name, node->func.arglen);
	for (uint32_t argn = 0; argn < node->func.arglen; argn++)
	{
		const Cmd_Arg_t * arg = &node->func.args[argn];
		Cmd_Printf(line, Cmd_Reply_Info, " - <%s: %s>" LF, Cmd_ArgTypeStr(line, arg), arg->name);
	}
}
#endif //CMD_HELP_TOKEN

#ifdef CMD_USE_ANSI
static void Cmd_HandleAnsi(Cmd_Line_t * line, char ch)
{
	switch (line->ansi)
	{
	case Cmd_Ansi_Escaped:
		if (ch == '[')
		{
			line->ansi = Cmd_Ansi_CSI;
		}
		else
		{
			line->ansi = Cmd_Ansi_None;
		}
		break;
	case Cmd_Ansi_CSI:
		if (ch > 0x40)
		{
			switch (ch)
			{
			case 'A': // up
				if (line->bfr.recall_index > line->bfr.index)
				{
					Cmd_RecallLine(line);
				}
#ifdef CMD_USE_BELL
				else
				{
					Cmd_Bell(line);
				}
#endif // CMD_USE_BELL
				break;
			case 'B': // down
				if (line->bfr.index)
				{
					Cmd_ClearLine(line);
				}
#ifdef CMD_USE_BELL
				else
				{
					Cmd_Bell(line);
				}
#endif //CMD_USE_BELL
				break;
			case 'C': // fwd
			case 'D': // back
			default:
#ifdef CMD_USE_BELL
				Cmd_Bell(line);
#endif // CMD_USE_BELL
				break;
			}
			line->ansi = Cmd_Ansi_None;
		}
	case Cmd_Ansi_None:
		break;
	}
}

static void Cmd_ClearLine(Cmd_Line_t * line)
{
#ifdef CMD_USE_ECHO
	if (line->cfg.echo)
	{
		uint32_t size = line->bfr.index;
		char * bfr = Cmd_Malloc(line, size);
		memset(bfr, DEL, size);
		line->print((uint8_t *)bfr, size);
		Cmd_Free(line, bfr);
	}
#endif //CMD_USE_ECHO
	line->bfr.recall_index = line->bfr.index;
	line->bfr.index = 0;

}

static void Cmd_RecallLine(Cmd_Line_t * line)
{
#ifdef CMD_USE_ECHO
	if (line->cfg.echo)
	{
		line->print( (uint8_t *)(line->bfr.data + line->bfr.index), line->bfr.recall_index - line->bfr.index);
	}
#endif //CMD_USE_ECHO
	line->bfr.index = line->bfr.recall_index;
}
#endif //CMD_USE_ANSI

#ifdef CMD_USE_BELL
static void Cmd_Bell(Cmd_Line_t * line)
{
	if (line->cfg.bell)
	{
		uint8_t ch = '\a';
		line->print(&ch, 1);
	}
}
#endif //CMD_USE_BELL

#ifdef CMD_USE_TABCOMPLETE
static const char * Cmd_TabComplete(Cmd_Line_t * line, const char * str)
{
	const char * append = Cmd_TabCompleteMenu(line, line->root, str);
	Cmd_FreeAll(line);
	return append;
}

static const char * Cmd_TabCompleteMenu(Cmd_Line_t * line, const Cmd_Node_t * node, const char * str)
{
	Cmd_Token_t token;
	if (Cmd_NextToken(line, &str, &token) == Cmd_Token_Ok)
	{
		bool end = *str == 0;

		if (end)
		{
			const Cmd_Node_t * candidate = NULL;
			for (uint32_t i = 0; i < node->menu.count; i++)
			{
				const Cmd_Node_t * child = node->menu.nodes[i];
				// Check if the token matches so far
				if (strncmp(child->name, token.str, token.size) == 0)
				{
					if (candidate != NULL)
					{
						// There are more than one candidates. No decision can be made.
						return NULL;
					}
					candidate = child;
				}
			}
			if (candidate != NULL)
			{
				return candidate->name + token.size;
			}
		}
		else
		{
			for (uint32_t i = 0; i < node->menu.count; i++)
			{
				const Cmd_Node_t * child = node->menu.nodes[i];
				if (strcmp(child->name, token.str) == 0)
				{
					if (child->type == Cmd_Node_Menu)
					{
						return Cmd_TabCompleteMenu(line, child, str);
					}
					return NULL;
				}
			}
		}
	}
	return NULL;
}
#endif //CMD_USE_TABCOMPLETE

/*
 * INTERRUPT ROUTINES
 */
