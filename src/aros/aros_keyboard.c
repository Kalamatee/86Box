/*
    $Id$
*/

#define DEBUG 1
#include <aros/debug.h>

#include <stdint.h>

/* map amiga rawkeys to scancodes */
uint16_t	scancode_map[100] =
{
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    0x3b, /* 80 = F1 */
    0x3c,
    0x3d,
    0x3e,
    0x3f,
    0x40,
    0x41,
    0x42,
    0x43,
    0x44,
    0x57,
    0x58, /* F12 */
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
};

void amiga_dokeyevent(UWORD keycode)
{
    UWORD plainkey;

    D(bug("86Box:%s(%x)\n", __func__, keycode);)

    plainkey = keycode & ~0x80;
    if (plainkey == keycode)
        keyboard_input(1, scancode_map[plainkey]);
    else
        keyboard_input(0, scancode_map[plainkey]);
}
