/************************************
 * Copyright (c) 2024 Roger Brown.
 * Licensed under the MIT License.
 */

int mbcsLen(const unsigned char* p, size_t avail);
wchar_t mbcsToChar(const unsigned char* p, int len);
int mbcsFromChar(wchar_t ch, unsigned char* p);

