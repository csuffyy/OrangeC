/* Copyright (C) 1996 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <wctype.h>
#define isblank(x)  (_ctype[x] & _IS_BLK)
int
main (int argc, char *argv[])
{
  int result = 0;
  wctype_t bit_alnum = wctype ("alnum");
  wctype_t bit_alpha = wctype ("alpha");
  wctype_t bit_cntrl = wctype ("cntrl");
  wctype_t bit_digit = wctype ("digit");
  wctype_t bit_graph = wctype ("graph");
  wctype_t bit_lower = wctype ("lower");
  wctype_t bit_print = wctype ("print");
  wctype_t bit_punct = wctype ("punct");
  wctype_t bit_space = wctype ("space");
  wctype_t bit_upper = wctype ("upper");
  wctype_t bit_xdigit = wctype ("xdigit");
  wctype_t bit_blank = wctype ("blank");
  wctrans_t tt_lower = wctrans("tolower");
  wctrans_t tt_upper = wctrans("toupper");
  int ch;

  if (wctype ("does not exist") != 0)
    {
      puts ("wctype return value != 0 for non existing property");
      result = 1;
    }

  for (ch = 0; ch < 256; ++ch)
    {
#define TEST(test)							      \
      do							      \
    if ((is##test (ch) == 0) != (iswctype (ch, bit_##test)) == 0)	      \
      {								      \
        printf ("class `%s' test for character \\%o failed\n",	      \
            #test, ch);						      \
        result = 1;							      \
      }								      \
      while (0)

      TEST (alnum);
      TEST (alpha);
      TEST (cntrl);
      TEST (digit);
      TEST (graph);
      TEST (lower);
      TEST (print);
      TEST (punct);
      TEST (space);
      TEST (upper);
      TEST (xdigit);
      TEST (blank);
    }
   printf("%c %c %c %c\n",towupper('a'), towupper('A'), towlower('c'), towlower('C'));
   printf("%c %c %c %c\n",towctrans('a',tt_upper), towctrans('A',tt_upper), towctrans('c',tt_lower), towctrans('C',tt_lower));
  if (result == 0)
    puts ("All test successful!");
  exit (result);
}
