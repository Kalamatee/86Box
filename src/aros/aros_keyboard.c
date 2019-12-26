/*
    $Id$
*/

#include <aros/debug.h>

#include <stdint.h>

#include "aros.h"

/* map amiga rawkeys to scancodes */
uint16_t	scancode_map[104] =
{
    0x29,       // 0 = '`'
    0x2,        // 1 = '1'
    0x3,        // 2 = '2'
    0x4,        // 3 = '3'
    0x5,        // 4 = '4'
    0x6,        // 5 = '5'
    0x7,        // 6 = '6'
    0x8,        // 7 = '7'
    0x9,        // 8 = '8'
    0xa,        // 9 = '9'
    0xb,        // 10 = '0'
    0xc,        // 11 = '-'
    0xd,        // 12 = '='
    0x2b,       // 13 = '\'
    14,
    15,
    16,         // 16 = 'Q'
    17,         // 17 = 'W'
    18,         // 18 = 'E'
    19,         // 19 = 'R'
    20,         // 20 = 'T'
    21,         // 21 = 'Y'
    22,         // 22 = 'U'
    23,         // 23 = 'I'
    24,         // 24 = 'O'
    25,         // 25 = 'P'
    26,         // 26 = '['
    27,         // 27 = ']'
    28,
    29,
    30,
    31,
    30,         // 32 = 'A'
    31,         // 33 = 'S'
    32,         // 34 = 'D'
    33,         // 35 = 'F'
    34,         // 36 = 'G'
    35,         // 37 = 'H'
    36,         // 38 = 'J'
    37,         // 39 = 'K'
    38,         // 40 = 'L'
    39,         // 41 = ';'
    40,         // 42 = '''
    43,         // 43 = ...................
    44,
    45,
    46,
    47,
    48,         // 48 = '<'
    44,         // 49 = 'Z'
    45,         // 50 = 'X'
    46,         // 51 = 'C'
    47,         // 52 = 'V'
    48,         // 53 = 'B'
    49,         // 54 = 'N'
    50,         // 55 = 'M'
    51,         // 56 = ','
    52,         // 57 = '.'
    53,         // 58 = '/'
    59,
    60,
    61,
    62,
    63,
    0x39,       // 64 = Space
    65,
    0xf,        // 66 = Tab
    0xe01c,     // 67 = Num Pad Enter
    0x1c,       // 68 = Return
    0x1,        // 69 = Escape
    0xE053,     // 70 = Del
    71,
    72,
    73,
    74,
    75,
    0xe048,     // 76 = cursor up
    0xe050,     // 77 = cursor down
    0xe04d,     // 78 = cursor right
    0xe04b,     // 79 = cursor left
    0x3b,       // 80 = F1
    0x3c,
    0x3d,
    0x3e,
    0x3f,
    0x40,
    0x41,
    0x42,
    0x43,
    0x44,       // 89 = F10
    0x57,       // 90 = Num Pad '['
    0x58,       // 91 = Num Pad ']'
    0xe035,     // 92 = Num Pad '/'
    93,
    94,
    95,
    96,
    97,
    98,
    0x1d,       // 99 = LCtrl
    0x38,       // 100 = LAlt
    0xe038,     // 101 = RAlt
    0xe05b,     // 102 = LAmiga
    0xe05c      // 103 = RAmiga
};

void aros_dokeyevent(UWORD keycode)
{
    UWORD plainkey;

    D(bug("86Box:%s(%x)\n", __func__, keycode);)

    plainkey = keycode & ~0x80;
    if (plainkey == keycode)
        keyboard_input(1, scancode_map[plainkey]);
    else
        keyboard_input(0, scancode_map[plainkey]);
}
