Graphic Programming Language disassembler
=========================================

This is an ISO C99 implementation of a disassembler for the
Texas Instruments "Graphic Programming Language", used in the
TI-99/4A.

It disassembles both standard GPL opcodes and FMT opcodes.
The mnemonics of FMT opcodes seem poorly standardized, but I'm using
those from [Thierry Nouspikel's GPL page](
https://www.unige.ch/medecine/nouspikel/ti99/gpl.htm).

Note that the operand order `OPCODE src,dest` is used by this
disassembler.  This matches the order used by Thierry's assembler, but
differs from the disassemblies printed in the TI99/4A INTERN book by
Heiner Martin.


Building
--------

A trivial `Makefile` is included for building the disassembler, but
really just compiling the single source file with any C compiler should
do.  :-)



Usage
-----

The `disgpl` program takes two to four arguments:

```
disgpl file base_addr [start_addr] [func_info_file]
```

The first argument is the file containing the binary dump of the GROM(s)
to disassemble.  The second argument is the base address corresponding
to the first byte of the file, in hexadecimal.  The third argument
specifies the address at which to start the disassembly, in hexadecimal.
This argument is optional and if omitted the base address will be used
as the address at which to start disassembly.  Typically you will want
to specify an address past the GROM headers.

The fourth and last argument is a file containing information about the
amount of `FETC` data various subroutines requires.  This allows the
disassembler to display this data as `DATA` directives following the
`CALL` opcode, instead of incorrectly disassembling this data as GPL
instructions.  The file should contain lines each containing first
the address of a GPL subroutine, as hexadecimal, and then a colon,
and finally the number of bytes of data that follows a call to the
subroutine at that address.  An example of such a file follows:

```
0010 : 1
001e : 4
14a9 : 1
2010 : 4
284c : 2
284e : 2
2d99 : 2
30b9 : 1
4cb9 : 1
```

The fourth argument can be omitted, in which case no special handling
of `FETC` data is performed.
