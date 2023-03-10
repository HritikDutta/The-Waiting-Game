#include "utils.h"

#include <cstdlib>
#include <cstring>
#include "core/logger.h"
#include "core/types.h"
#include "containers/string.h"

void to_string(String& str, s32 integer, u32 radix)
{
    gn_assert_with_message(str.data, "Destination string for integer to string conversion points to null!");
    gn_assert_with_message(radix >= 2 && radix <= 16, "Radix value for converting number to string is not valid! (radix: %)", radix);

    constexpr char digits[] = "0123456789abcdef";

    if (integer == 0)
    {
        str.data[0] = '0';
        str.size = 1;
        return;
    }

    s32 num = abs(integer);
    u64 num_digits = (log(num) / log(radix)) + 1;

    u64 i = 0;
    while (num)
    {
        s32 index = num % radix;
        str.data[i++] = digits[index];
        num /= radix;
    }

    if (integer < 0)
        str.data[i++] = '-';
    
    str.size = i;
    reverse(str);
}

void to_string(String& str, s64 integer, u32 radix)
{
    gn_assert_with_message(str.data, "Destination string for integer to string conversion points to null!");
    gn_assert_with_message(radix >= 2 && radix <= 16, "Radix value for converting number to string is not valid! (radix: %)", radix);

    constexpr char digits[] = "0123456789abcdef";

    if (integer == 0)
    {
        str.data[0] = '0';
        str.size = 1;
        return;
    }

    s64 num = abs(integer);
    u64 num_digits = (log(num) / log(radix)) + 1;

    u64 i = 0;
    while (num)
    {
        s64 index = num % radix;
        str.data[i++] = digits[index];
        num /= radix;
    }

    if (integer < 0)
        str.data[i++] = '-';
    
    str.size = i;
    reverse(str);
}

void to_string(String& str, u32 integer, u32 radix)
{
    gn_assert_with_message(str.data, "Destination string for integer to string conversion points to null!");
    gn_assert_with_message(radix >= 2 && radix <= 16, "Radix value for converting number to string is not valid! (radix: %)", radix);

    constexpr char digits[] = "0123456789abcdef";

    if (integer == 0)
    {
        str.data[0] = '0';
        str.size = 1;
        return;
    }

    u32 num = integer;
    u64 num_digits = (log(num) / log(radix)) + 1;

    u64 i = 0;
    while (num)
    {
        u32 index = num % radix;
        str.data[i++] = digits[index];
        num /= radix;
    }

    str.size = i;
    reverse(str);
}

void to_string(String& str, u64 integer, u32 radix)
{
    gn_assert_with_message(str.data, "Destination string for integer to string conversion points to null!");
    gn_assert_with_message(radix >= 2 && radix <= 16, "Radix value for converting number to string is not valid! (radix: %)", radix);

    constexpr char digits[] = "0123456789abcdef";

    if (integer == 0)
    {
        str.data[0] = '0';
        str.size = 1;
        return;
    }

    u64 num = integer;
    u64 num_digits = (log(num) / log(radix)) + 1;

    u64 i = 0;
    while (num)
    {
        u64 index = num % radix;
        str.data[i++] = digits[index];
        num /= radix;
    }

    str.size = i;
    reverse(str);
}

void to_string(String& str, f32 number, u32 after_decimal)
{
    // TODO: This is not working! Have a look at this: https://github.com/antongus/stm32tpl/blob/master/ftoa.c
    
    gn_assert_with_message(str.data, "Destination string for float to string conversion points to null!");

    u64 old_size = str.size;

    s32 integer = (s32) number;
    f32 fractional = number - (f32) integer;

    to_string(str, integer);

    if (after_decimal > 0)
    {
        str.data[str.size] = '.';

        String fraction_string = { str.data + str.size + 1, old_size - str.size };

        fractional *= pow(10.0, after_decimal);
        to_string(fraction_string, abs((s32) fractional));
        
        str.size += fraction_string.size + 1;
    }
}
void to_string(String& str, f64 number, u32 after_decimal)
{
    // TODO: This is not working! Have a look at this: https://github.com/antongus/stm32tpl/blob/master/ftoa.c

    gn_assert_with_message(str.data, "Destination string for float to string conversion points to null!");

    u64 old_size = str.size;

    s64 integer = (s64) number;
    f64 fractional = number - (f64) integer;

    to_string(str, integer);

    if (after_decimal > 0)
    {
        str.data[str.size] = '.';

        String fraction_string = { str.data + str.size + 1, old_size - str.size };

        fractional *= pow(10.0, after_decimal);
        to_string(fraction_string, abs((s64) fractional));
        
        str.size += fraction_string.size + 1;
    }
}