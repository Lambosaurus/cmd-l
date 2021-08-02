#ifndef COMMAND_CONF_H
#define COMMAND_CONF_H



/*
 * BUFFER CONFIGURATION
 */

// Maximum number of arguments to parse.
#define CMD_MAX_ARGS	4

// Maximum line length
#define CMD_MAX_LINE	64



/*
 * TERMINAL INPUT CONFIGURATION
 * 		These are recommended for interfacing with PuTTY-like terminals
 */

// Support for ANSI input sequences, such as arrow keys.
#define CMD_USE_ANSI

// Allow tab completion of commands
#define CMD_USE_TABCOMPLETE

// This token will produce info on the specified menu or or function
#define CMD_HELP_TOKEN		"?"


/*
 * TERMINAL OUTPUT CONFIGURATION
 * 		These are recommended for interfacing with PuTTY-like terminals
 * 		These are must also be enabled within the Cmd_LineConfig_t
 */

// Emit the bell char '\a' for errors and ignored commands
#define CMD_USE_BELL

// Echo input chars
#define CMD_USE_ECHO

// Emit color output sequences, for warnings and errors.
#define CMD_USE_COLOR

// A line starting sequence printed at the start of each new line
#define CMD_PROMPT		"> "

// The line ending sequence on internally generated replies.
#define CMD_LINE_END	"\r\n"


/*
 * ARGUMENT CONFIGURATION
 * 		These support different argument options
 */

// Support for booleans as argument input type
#define CMD_USE_BOOL_ARGS

// Support for strings as argument input type
#define CMD_USE_STRING_ARGS

// Supports bytes as an argument input type
#define CMD_USE_BYTE_ARGS

// Support backslash escape sequences for string parsing and formatting
// This supports byte literals "\x00", delimiters "\"", and control chars "\a\r\n\0"
#define CMD_USE_STRING_ESC

// Supports hexadecimal input types for numbers
#define CMD_USE_NUMBER_HEX

// Supports engineering notation input types for numbers
#define CMD_USE_NUMBER_ENG




#endif //COMMAND_CONF_H
