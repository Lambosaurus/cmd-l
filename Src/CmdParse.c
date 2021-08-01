
#include "CmdParse.h"
#include <stdio.h>
#include <string.h>

/*
 * PRIVATE DEFINITIONS
 */

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static char Cmd_Lowchar(char ch);
#ifdef CMD_USE_NUMBER_HEX
static bool Cmd_ParseHexPrefix(const char ** str);
static bool Cmd_ParseHex(const char ** str, uint32_t * value);
#endif
static bool Cmd_ParseNibble(char ch, uint32_t * n);
#ifdef CMD_USE_BYTE_ARGS
static bool Cmd_ParseByte(const char ** str, uint8_t * value);
#endif
static bool Cmd_ParseUint(const char ** str, uint32_t * value);
#ifdef CMD_USE_NUMBER_ENG
static bool Cmd_ParseFixedUint(const char ** str, uint32_t length, uint32_t * value);
#endif

/*
 * PRIVATE VARIABLES
 */

#ifdef CMD_USE_STRING_ESC
static const char gEscCharmap[] = "abtnvfr";
#endif

/*
 * PUBLIC FUNCTIONS
 */

bool Cmd_ParseNumber(const char ** str, uint32_t * value)
{
#ifdef CMD_USE_NUMBER_HEX
	if (Cmd_ParseHexPrefix(str))
	{
		return Cmd_ParseHex(str, value);
	}
#endif

	uint32_t prefix;
	if (Cmd_ParseUint(str, &prefix))
	{
#ifdef CMD_USE_NUMBER_ENG
		uint32_t power = 0;
		char ch = Cmd_Lowchar(**str);
		if (ch == 'k')
		{
			power = 3;
		}
		else if (ch == 'm')
		{
			power = 6;
		}
		if (power > 0)
		{
			*str += 1;
			for (uint32_t p = 0; p < power; p++)
			{
				prefix *= 10;
			}

			uint32_t suffix;
			if (Cmd_ParseFixedUint(str, power, &suffix))
			{
				prefix += suffix;
			}
		}
#endif //CMD_USE_NUMBER_ENG
		*value = prefix;
		return true;
	}
	else
	{
		return false;
	}
}

#ifdef CMD_USE_BYTE_ARGS
bool Cmd_ParseBytes(const char ** str, uint8_t * value, uint32_t size, uint32_t * count)
{
	uint32_t n = 0;
	while ( Cmd_ParseByte(str, value++) )
	{
		n++;
		if (n >= size)
		{
			break;
		}
		char next = **str;
		if (next == '-' || next == ':' || next == ',' || next == ' ')
		{
			// Bytes may use these as delimiters.
			(*str)++;
		}
	}
	*count = n;
	return true;
}

uint32_t Cmd_FormatBytes(char * dst, uint8_t * hex, uint32_t count, char space)
{
	char * start = dst;
	while(count--)
	{
		dst += sprintf(dst, "%02X", *hex++);
		if (space != 0 && count)
		{
			*dst++ = space;
		}
	}
	*dst = 0;
	return dst - start;
}
#endif //CMD_USE_BYTE_ARGS

#ifdef CMD_USE_STRING_ARGS
#ifdef CMD_USE_STRING_ESC
bool Cmd_ParseString(const char ** str, char * value, uint32_t size, uint32_t * count)
{
	bool escaped = false;
	uint32_t written;
	const char * head = *str;
	for (written = 0; written < size - 1;)
	{
		char ch = *head++;
		if (ch == 0)
		{
			head--;
			break;
		}
		else if (escaped)
		{
			bool esc_found = false;
			for (uint32_t i = 0; i < sizeof(gEscCharmap); i++)
			{
				if (ch == gEscCharmap[i])
				{
					ch = '\a' + i;
					esc_found = true;
					break;
				}
			}
			if (!esc_found)
			{
				// We did not match onto the esc table
				if (ch == 'x')
				{
					if (!Cmd_ParseByte(&head, (uint8_t*)&ch))
					{
						return false;
					}
				}
				else if (ch == '0')
				{
					ch = 0;
				}
				// Otherwise we just interpret it as literal.
				// Other chars (', ", \, ?) can get their literal interpretation.
			}
			escaped = false;
			*value++ = ch;
			written++;
		}
		else if (ch == '\\')
		{
			escaped = true;
		}
		else
		{
			*value++ = ch;
			written++;
		}
	}
	*value = 0; // Enforce null char.
	*str = head;
	*count = written;
	return true;
}

uint32_t Cmd_FormatString(char * dst, uint32_t size, uint8_t * data, uint32_t count, char delimiter)
{
	char * end = dst + size;
	while (count-- && dst < end)
	{
		char ch = (char)*data++;

		if (ch == delimiter || ch == '\\' || ch == 0 || (ch >= '\a' && ch <= '\r'))
		{
			if (end - dst <= 2)
			{
				break;
			}
			*dst++ = '\\';
			if (ch == 0)
			{
				ch = '0';
			}
			else if (ch != delimiter && ch != '\\')
			{
				ch = gEscCharmap[ch - '\a'];
			}
			*dst++ = ch;
		}
		else if (ch >= ' ' && ch <= '~')
		{
			*dst++ = ch;
		}
		else
		{
			if (end - dst <= 4)
			{
				break;
			}
			dst += sprintf(dst, "\\x%02X", ch);
		}
	}
	*dst = 0;
	return dst - (end - size);
}
#else // CMD_STRING_ESC
bool Cmd_ParseString(const char ** str, char * value, uint32_t size, uint32_t * count)
{
	const char * head = *str;
	while (size-- && *head != 0)
	{
		*value++ = *head++;
	}
	*value = 0;
	*count = head - *str;
	*str = head;
	return true;
}

uint32_t Cmd_FormatString(char * dst, uint32_t size, uint8_t * data, uint32_t count, char delimiter)
{
	const char * head = (char*)data;
	while (size-- && *head != 0)
	{
		*dst++ = *head++;
	}
	*dst = 0;
	return head - (char*)data;
}
#endif //CMD_USE_STRING_ESC
#endif //CMD_USE_STRING_ARGS

/*
 * PRIVATE FUNCTIONS
 */

static char Cmd_Lowchar(char ch)
{
	if (ch >= 'A' && ch <= 'Z')
	{
		return ch + ('a' - 'A');
	}
	return ch;
}

#ifdef CMD_USE_NUMBER_HEX
static bool Cmd_ParseHexPrefix(const char ** str)
{
	const char * start = *str;
	if (*start == '0')
	{
		// We want to allow 0x or 0h prefix.
		start += 1;
	}
	char pfx = Cmd_Lowchar(*start);
	if (pfx == 'x' || pfx == 'h')
	{
		*str = start+1;
		return true;
	}
	return false;
}

static bool Cmd_ParseHex(const char ** str, uint32_t * value)
{
	uint32_t v = 0;
	const char * head = *str;
	while (1)
	{
		uint32_t d;
		if (!Cmd_ParseNibble(*head, &d))
		{
			break;
		}
		head++;
		v = (v << 4) | d;
	}
	if (head > *str)
	{
		// Ensure we read at least 1 char
		*value = v;
		*str = head;
		return true;
	}
	return false;
}
#endif //CMD_USE_NUMBER_HEX

static bool Cmd_ParseNibble(char ch, uint32_t * n)
{
	ch = Cmd_Lowchar(ch);
	if (ch >= '0' && ch <= '9')
	{
		*n = ch - '0';
		return true;
	}
	else if (ch >= 'a' && ch <= 'f')
	{
		*n = ch - ('a' - 10);
		return true;
	}
	return false;
}

#ifdef CMD_USE_BYTE_ARGS
static bool Cmd_ParseByte(const char ** str, uint8_t * value)
{
	const char * head = *str;

	uint32_t high;
	uint32_t low;
	if (Cmd_ParseNibble(*head++, &high) && Cmd_ParseNibble(*head++, &low))
	{
		*str = head;
		*value = (high << 4) | low;
		return true;
	}
	return false;
}
#endif

static bool Cmd_ParseUint(const char ** str, uint32_t * value)
{
	uint32_t v = 0;
	const char * head = *str;
	while (1)
	{
		char ch = *head;
		if (ch >= '0' && ch <= '9')
		{
			v = (v * 10) + (ch - '0');
			head++;
		}
		else
		{
			break;
		}
	}
	if (head > *str)
	{
		// Ensure we read at least 1 char
		*value = v;
		*str = head;
		return true;
	}
	return false;
}

#ifdef CMD_USE_NUMBER_ENG
static bool Cmd_ParseFixedUint(const char ** str, uint32_t length, uint32_t * value)
{
	const char * start = *str;
	if (!Cmd_ParseUint(str, value))
	{
		return false;
	}

	uint32_t count = *str - start;
	while (count > length)
	{
		count -= 1;
		*value /= 10;
	}
	while (count < length)
	{
		count += 1;
		*value *= 10;
	}
	return true;
}
#endif //CMD_USE_NUMBER_ENG

/*
 * INTERRUPT ROUTINES
 */
