#ifndef INCLUDE_UNISHIM_SPLIT_H
#define INCLUDE_UNISHIM_SPLIT_H

/* unishim_split.h - C99/C++ utf-8/utf-16/utf-32 conversion header

This file is released to the public domain under US law,
and also released under any version of the Creative Commons Zero license: 
https://creativecommons.org/publicdomain/zero/1.0/
https://creativecommons.org/publicdomain/zero/1.0/legalcode
*/

// REPLACES unishim.h -- only include one!

// #define UNISHIM_PUN_TYPE_IS_CHAR before including for the callback userdata type to be char* instead of void*. 
// This might work around broken strict aliasing optimizations that
// don't allow T* -> void* -> T*, only T* -> char* -> T*.

// #define UNISHIM_NO_STDLIB to not include stdlib.h and not use malloc/free in the code.
// This prevents utfX_to_utfY functions from being declared.

// #define UNISHIM_DECLARATION_PREFIX to change the declaration prefix from "static" to anything else

#include <stdint.h>
#ifndef UNISHIM_NO_STDLIB
#include <stdlib.h>
#endif
#include <iso646.h>

#ifdef UNISHIM_PUN_TYPE_IS_CHAR
#define UNISHIM_PUN_TYPE char
#else
#define UNISHIM_PUN_TYPE void
#endif

#ifndef UNISHIM_DECLARATION_PREFIX
#define UNISHIM_DECLARATION_PREFIX static
#endif

typedef int (*unishim_callback)(uint32_t codepoint, UNISHIM_PUN_TYPE * userdata);

/*
utf8: pointer to array of uint8_t values, storing utf-8 code units, encoding utf-8 text
max: zero if array is terminated by a null code unit, nonzero if array has a particular length
callback: int unishim_callback(uint32_t codepoint, void * userdata);
userdata: user data, given to callback

BUFFER utf8 MUST NOT BE MODIFIED BY ANOTHER THREAD DURING ITERATION.

Executes callback(codepoint, userdata) on each codepoint in the string, from start to end.
If callback is NULL/nullptr/0, callback is not executed. Useful if you only want the return status code.
If callback returns non-zero, execution returns immediately AND RETURNS THE VALUE THE CALLBACK RETURNED.
CALLBACK CAN BE CALLED WITH A CODEPOINT VALUE OF ZERO IF max IS NOT ZERO.

If an encoding error is encountered, an appropriate status code is returned:
-1: argument is invalid (utf8 buffer pointer is null)
0: no error, covered all codepoints
1: continuation code unit where initial code unit expected or initial code unit is illegal
2: codepoint encoding truncated by null terminator or end of buffer (position >= max)
3: initial code unit where continuation code unit expected
4: codepoint is a surrogate, which is forbidden
5: codepoint is too large to encode in utf-16, which is forbidden
6: codepoint is overlong (encoded using too many code units)

error 6 takes priority over error 4

CALLBACK IS NOT RUN ON ANY CODEPOINTS STARTING AT (INCLUSIVE) THE FIRST CODEPOINT TO CAUSE AN ERROR. RETURN IS IMMEDIATE.

Reads at most "max" CODE UNITS (uint8_t) from the utf8 buffer.
If "max" is zero, stops at null instead.
CALLBACK DOES NOT RUN ON NULL TERMINATOR IF MAX IS ZERO.
*/
UNISHIM_DECLARATION_PREFIX int utf8_iterate(uint8_t * utf8, size_t max, unishim_callback callback, void * userdata)
{
    if(!utf8)
        return -1;
    
    uint8_t * counter = utf8;
    size_t i = 0;
    
    while((max) ? (i < max) : (counter[0] != 0))
    {   
        // trivial byte
        if(counter[0] < 0x80)
        {
            if(callback)
            {
                int r = callback(counter[0], userdata);
                if(r) return r;
            }
            
            counter += 1;
            i += 1;
        }
        // continuation byte where initial byte expected
        else if(counter[0] < 0xC0)
        {
            return 1;
        }
        // two byte
        else if(counter[0] < 0xE0)
        {
            for(int index = 1; index <= 1; index++)
            {
                // unexpected termination
                if((max and index+i >= max) or counter[index] == 0)
                    return 2;
                // bad continuation byte
                else if(counter[index] < 0x80 or counter[index] >= 0xC0)
                    return 3;
            }
            
            uint32_t high = counter[0]&0x1F;
            uint32_t low = counter[1]&0x3F;
            
            uint32_t codepoint = (high<<6) | low;
            
            if(codepoint < 0x80)
                return 6;
            
            if(callback)
            {
                int r = callback(codepoint, userdata);
                if(r) return r;
            }
            
            counter += 2;
            i += 2;
        }
        // three byte
        else if(counter[0] < 0xF0)
        {
            for(int index = 1; index <= 2; index++)
            {
                // unexpected termination
                if((max and index+i >= max) or counter[index] == 0)
                    return 2;
                // bad continuation byte
                else if(counter[index] < 0x80 or counter[index] >= 0xC0)
                    return 3;
            }
            
            uint32_t high = counter[0]&0x0F;
            uint32_t mid = counter[1]&0x3F;
            uint32_t low = counter[2]&0x3F;
            
            uint32_t codepoint = (high<<12) | (mid<<6) | low;
            
            if(codepoint < 0x800)
                return 6;
            
            if(codepoint > 0xD800 and codepoint < 0xE000)
                return 4;
            
            if(callback)
            {
                int r = callback(codepoint, userdata);
                if(r) return r;
            }
            
            counter += 3;
            i += 3;
        }
        // four byte
        else if(counter[0] < 0xF8)
        {
            for(int index = 1; index <= 3; index++)
            {
                // unexpected termination
                if((max and index+i >= max) or counter[index] == 0)
                    return 2;
                // bad continuation byte
                else if(counter[index] < 0x80 or counter[index] >= 0xC0)
                    return 3;
            }
            
            uint32_t top = counter[0]&0x07;
            uint32_t high = counter[1]&0x3F;
            uint32_t mid = counter[2]&0x3F;
            uint32_t low = counter[3]&0x3F;
            
            uint32_t codepoint = (top<<18) | (high<<12) | (mid<<6) | low;
            
            if(codepoint < 0x10000)
                return 6;
            
            if(codepoint >= 0x110000)
                return 5;
            
            if(callback)
            {
                int r = callback(codepoint, userdata);
                if(r) return r;
            }
            
            counter += 4;
            i += 4;
        }
        else
            return 1;
    }
    return 0;
}

#endif
