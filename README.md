C-code generator for docopt language based on Yacc & Lex
========================================================

### Step 1. Describe your CLI in docopt language

```
Naval Fate.

Usage:
  naval_fate.py ship create <name>...
  naval_fate.py ship <name> move <x> <y> [--speed=<kn>]
  naval_fate.py ship shoot <x> <y>
  naval_fate.py mine (set|remove) <x> <y> [--moored|--drifting]
  naval_fate.py --help
  naval_fate.py --version

Options:
  -h --help     Show this screen.
  --version     Show version.
  --speed=<kn>  Speed in knots [default: 10].
  --moored      Moored (anchored) mine.
  --drifting    Drifting mine.
```

### Step 2. Build a docopt parser

```bash
$ make
```

you will require flex and bison tools installed.

### Step 3. Generate grammar and scanner for your command line parser

```bash
$ ./docopt cmd.docopt
```

This generates three files:

 * **cmd.y**    yacc grammar for your command line parser
 * **cmd.l**    lexical scanner for your command line parser
 * **cmd.h**    C header, which describes command line structure 'struct cli'
                and has two functions declarations:

           int cli_parse(int argc, char **argv, struct cli *cli);
           int cli_free(struct cli *cli);


### Step 4. Compile your own command line parser

```bash
$ flex cmd.l
$ bison -d cmd.y
$ gcc lex.yy.c cmd.tab.c -O2 -DMAIN_EXAMPLE
```

### Step 4. Try it out!

No options are given:

```bash
$ ./a.out
Naval Fate.

Usage:
  naval_fate ship new <name>...
  naval_fate ship <name> move <x> <y> [--speed=<kn>]
  naval_fate ship shoot <x> <y>
  naval_fate mine (set|remove) <x> <y> [--moored|--drifting]
  naval_fate -h | --help
  naval_fate --version

Options:
  -h --help     Show this screen.
  --version     Show version.
  --speed=<kn>  Speed in knots [default: 10].
  --moored      Moored (anchored) mine.
  --drifting    Drifting mine.
```

New ships example:

```bash
$ ./a.out ship new Titanic Lincoln
'ship' = '1'
'new' = '1'
'name_arr[0]' = 'Titanic'
'name_arr[1]' = 'Lincoln'
'move' = '0'
'x' = '(null)'
'y' = '(null)'
'speed' = '(null)'
'shoot' = '0'
'mine' = '0'
'set' = '0'
'remove' = '0'
'moored' = '0'
'drifting' = '0'
'h' = '0'
'help' = '0'
'version' = '0'

```

Set moored mine example:

```bash
$ ./a.out mine set 1245 666 --moored
'ship' = '0'
'new' = '0'
'move' = '0'
'x' = '1245'
'y' = '666'
'speed' = '(null)'
'shoot' = '0'
'mine' = '1'
'set' = '1'
'remove' = '0'
'moored' = '1'
'drifting' = '0'
'h' = '0'
'help' = '0'
'version' = '0'
```

And so on.

Development
===========

Parser was inspired by the idea taken from project:

   http://docopt.org

Similar C-code generator for docopt language can be found here:

   https://github.com/docopt/docopt.c

which at this point (as documentation says) does not handle positional
arguments, commands and pattern matching.

This yacc & lex generator is able to parse options, positional arguments
and supports groups of optional and required arguments.

In case of more complicated requirements (e.g. patterns matching) yacc
and lex output can be changed accordingly, which gives a lot more freedom.

Author
======

Roman Pen <r.peniaev@gmail.com>
