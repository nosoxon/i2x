# i2x

## Help
```
i2x - i2c swiss army knife

Usage:  i2x [options] [--] BUS:ADDR CMD_LIST

Execute CMD_LIST on i2c bus BUS (decimal), addressing messages to
ADDR (hex). CMD_LIST need not be quoted, and spacing within it is
mostly optional.

Example:

        $ i2x 2:3c 8b w 04 919d7458 t100 / 8b-8e r8

        8b: 07 46 cd be 0c 7b 6f cf
        8c: 07 9d 74 58 55 0f 17 d1
        8d: 07 04 7c ac 03 2a af 3c
        8e: 07 06 9a 1b cc 8c ae 91

Options:
  -d, --dec        pretty print responses in decimal
  -x, --hex        pretty print responses in hex (default)
  -p, --plain      print responses in plain hex format
  -r, --raw        output raw response data

  -v, --verbose    show more details
  -n, --dry-run    don't dispatch messages, implies --verbose
  -t, --tree       just show parse tree
  -h, --help       display this help

For more details see i2x(8)
```

## Sample commands
```
i2x 2bff, d1b8, dfee : w 8afd ab70 cf38 3493, r8
i2x w0f06040d99837f00 w0f04000d9d16 w0a03000d9e w09r4 w0a03000d9c w09r4
i2x fd - 02 w 4
i2x w 07 r7
i2x w c0 01
i2x c0 w 01
i2x 4567, 2I416-01a3 : w 5d1e b00b13 4i666 r 24 w 8 . r24, w 45 33 / 5c-5e r5 / w100000
i2x 5c w 04 b4 99 56 80 / 5c-5e r5
i2x 4567, 2I416-01a3 : w 5d1e b00b13 4i666 r 24 w 8 . r24, w 45 33 / 5c-5e r5 / w100000
i2x w0f06 040d 9984 7f00 w0f04000d9d16 w0a03000d9e w09r4 w0a03000d9c w09r4 / 5c: w 04b4995680 /5c-5er5
```
