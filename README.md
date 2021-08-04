# CMD-L
A command line handler for embedded devices.

This supports a variety of useful features, aimed at user friendly-ness, without an excessive overhead for an embedded devices.

![image](https://user-images.githubusercontent.com/11606026/127823021-a12764eb-289e-40ef-a8a9-a2a94ea7c976.png)

## Features
### Argument parsing:
Arguments are described in data alongside your functions. This means arguments can be validated and parsed to the appropriate types before your funcition is called.

The following argument types are supported:
* Numbers: `9600`
  * Hex `0x2580`
  * Engineering notation `9k6`
* Booleans `0` or `1`
* Strings `'Hey'`
  * Python style escape sequences `"\"Hey\"\r\x0A"`
* Byte arrays `6865790d0a` or `[68 65 79 0D 0A]` (and more)


### Tree menu structure:
Menus are built out of nodes, which may in turn be functions or more menus. This leads to a natural command tree.

```
─ root
  ├─ spi
  │   ├─ read(number)
  │   └─ write(bytes)
  ├─ uart
  │   ├─ read(number)
  │   └─ write(bytes)
  └─ version()
```

For example, a valid command may be `uart write 'hello world'`

### PuTTY friendly design
While this could be used for machine interfaces - this module is targeted at human use.
Entering commands should be forgiving, and rich in feedback. The menus can be explored without needing to know the exact syntax or arguments.

The following features support this.
* Input Echo
* Backspace
* Colors for warnings and errors
* Bell for errors & halts
* Tab completion
* A symbol `?` to get information about a menu or function
* Error messages to describe any parsing failures.

## Usage
* Add `/Src/` to your build and include directories
* Copy `/Templates/CmdConf.h` into your project, and modify to suit
* `#include "Cmd.h"` in your files and get to work

## Examples
An example based around STM32X on can be found [here](https://github.com/Lambosaurus/cmd-l/blob/main/Examples/main.c)
