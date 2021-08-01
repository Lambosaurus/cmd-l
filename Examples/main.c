
#include "STM32X.h"
#include "Core.h"
#include "USB.h"

#include "Cmd.h"


// Test 1 expects an argument
static void Test1Function(Cmd_Line_t * line, Cmd_ArgValue_t * args)
{
	Cmd_Printf(line, Cmd_Reply_Info, "Test 1: Value is %d\r\n", args[0].number);
}

static const Cmd_Arg_t gTest1Args[] = {
	CMD_ARGUMENT(Cmd_Arg_Number, "input")
};

static const Cmd_Node_t gTest1Node = CMD_AFUNCTION("test-1", Test1Function, gTest1Args);


// Test 2 expects no arguments.
static void Test2Function(Cmd_Line_t * line, Cmd_ArgValue_t * args)
{
	Cmd_Printf(line, Cmd_Reply_Info, "Test 2.\r\n");
}

static const Cmd_Node_t gTest2Node = CMD_FUNCTION("test-2", Test2Function);


// The menu contains both items
static const Cmd_Node_t * gRootItems[] = {
	&gTest1Node,
	&gTest2Node
};
static const Cmd_Node_t gRootMenu = CMD_MENU("root", gRootItems);

int main(void)
{
	CORE_Init();
	USB_Init();

	uint8_t heap[256];
	Cmd_Line_t line;
	Cmd_Init(&line, &gRootMenu, USB_Write, heap, sizeof(heap));
	line.cfg.echo = true;

	while(1)
	{
		uint8_t bfr[64];
		uint32_t count = USB_Read(bfr, sizeof(bfr));
		Cmd_Parse(&line, bfr, count);
		CORE_Idle();
	}
}

