#include "fix16.h"

/* Subtraction and addition with overflow detection.
 * The versions without overflow detection are inlined in the header.
 */
fix16_t fix16_add(fix16_t a, fix16_t b)
{
	// Use unsigned integers because overflow with signed integers is
	// an undefined operation (http://www.airs.com/blog/archives/120).
	uint32_t _a = a, _b = b;
	uint32_t sum = _a + _b;

	// Overflow can only happen if sign of a == sign of b, and then
	// it causes sign of sum != sign of a.
	if (!((_a ^ _b) & 0x80000000) && ((_a ^ sum) & 0x80000000))
		return fix16_overflow;
	
	return sum;
}

fix16_t fix16_sub(fix16_t a, fix16_t b)
{
	uint32_t _a = a, _b = b;
	uint32_t diff = _a - _b;

	// Overflow can only happen if sign of a != sign of b, and then
	// it causes sign of diff != sign of a.
	if (((_a ^ _b) & 0x80000000) && ((_a ^ diff) & 0x80000000))
		return fix16_overflow;
	
	return diff;
}

/* Saturating arithmetic */
fix16_t fix16_sadd(fix16_t a, fix16_t b)
{
	fix16_t result = fix16_add(a, b);

	if (result == fix16_overflow)
		return (a >= 0) ? fix16_maximum : fix16_minimum;

	return result;
}	

fix16_t fix16_ssub(fix16_t a, fix16_t b)
{
	fix16_t result = fix16_sub(a, b);

	if (result == fix16_overflow)
		return (a >= 0) ? fix16_maximum : fix16_minimum;

	return result;
}

/* 8-bit implementation of fix16_mul. Fastest on e.g. Atmel AVR.
 * Uses 8*8->16bit multiplications, and also skips any bytes that
 * are zero.
 */
fix16_t fix16_mul(fix16_t inArg0, fix16_t inArg1)
{
	uint32_t _a = (inArg0 >= 0) ? inArg0 : (-inArg0);
	uint32_t _b = (inArg1 >= 0) ? inArg1 : (-inArg1);
	
	uint8_t va[4] = {_a, (_a >> 8), (_a >> 16), (_a >> 24)};
	uint8_t vb[4] = {_b, (_b >> 8), (_b >> 16), (_b >> 24)};
	
	uint32_t low = 0;
	uint32_t mid = 0;
	
	// Result column i depends on va[0..i] and vb[i..0]

	// i = 6
	if (va[3] && vb[3]) return fix16_overflow;
	
	// i = 5
	if (va[2] && vb[3]) mid += (uint16_t)va[2] * vb[3];
	if (va[3] && vb[2]) mid += (uint16_t)va[3] * vb[2];
	mid <<= 8;
	
	// i = 4
	if (va[1] && vb[3]) mid += (uint16_t)va[1] * vb[3];
	if (va[2] && vb[2]) mid += (uint16_t)va[2] * vb[2];
	if (va[3] && vb[1]) mid += (uint16_t)va[3] * vb[1];
	
	if (mid & 0xFF000000) return fix16_overflow;
	mid <<= 8;
	
	// i = 3
	if (va[0] && vb[3]) mid += (uint16_t)va[0] * vb[3];
	if (va[1] && vb[2]) mid += (uint16_t)va[1] * vb[2];
	if (va[2] && vb[1]) mid += (uint16_t)va[2] * vb[1];
	if (va[3] && vb[0]) mid += (uint16_t)va[3] * vb[0];
	
	if (mid & 0xFF000000) return fix16_overflow;
	mid <<= 8;
	
	// i = 2
	if (va[0] && vb[2]) mid += (uint16_t)va[0] * vb[2];
	if (va[1] && vb[1]) mid += (uint16_t)va[1] * vb[1];
	if (va[2] && vb[0]) mid += (uint16_t)va[2] * vb[0];		
	
	// i = 1
	if (va[0] && vb[1]) low += (uint16_t)va[0] * vb[1];
	if (va[1] && vb[0]) low += (uint16_t)va[1] * vb[0];
	low <<= 8;
	
	// i = 0
	if (va[0] && vb[0]) low += (uint16_t)va[0] * vb[0];
	
	low += 0x8000;
	mid += (low >> 16);
	
	if (mid & 0x80000000)
		return fix16_overflow;
	
	fix16_t result = mid;
	
	/* Figure out the sign of result */
	if ((inArg0 >= 0) != (inArg1 >= 0))
	{
		result = -result;
	}
	
	return result;
}

/* Wrapper around fix16_mul to add saturating arithmetic. */
fix16_t fix16_smul(fix16_t inArg0, fix16_t inArg1)
{
	fix16_t result = fix16_mul(inArg0, inArg1);
	
	if (result == fix16_overflow)
	{
		if ((inArg0 >= 0) == (inArg1 >= 0))
			return fix16_maximum;
		else
			return fix16_minimum;
	}
	
	return result;
}

/* Alternative 32-bit implementation of fix16_div. Fastest on e.g. Atmel AVR.
 * This does the division manually, and is therefore good for processors that
 * do not have hardware division.
 */
fix16_t fix16_div(fix16_t a, fix16_t b)
{
	// This uses the basic binary restoring division algorithm.
	// It appears to be faster to do the whole division manually than
	// trying to compose a 64-bit divide out of 32-bit divisions on
	// platforms without hardware divide.
	
	if (b == 0)
		return fix16_minimum;
	
	uint32_t remainder = (a >= 0) ? a : (-a);
	uint32_t divider = (b >= 0) ? b : (-b);

	uint32_t quotient = 0;
	uint32_t bit = 0x10000;
	
	/* The algorithm requires D >= R */
	while (divider < remainder)
	{
		divider <<= 1;
		bit <<= 1;
	}
	
	if (!bit)
		return fix16_overflow;
	
	if (divider & 0x80000000)
	{
		// Perform one step manually to avoid overflows later.
		// We know that divider's bottom bit is 0 here.
		if (remainder >= divider)
		{
				quotient |= bit;
				remainder -= divider;
		}
		divider >>= 1;
		bit >>= 1;
	}
	
	/* Main division loop */
	while (bit && remainder)
	{
		if (remainder >= divider)
		{
				quotient |= bit;
				remainder -= divider;
		}
		
		remainder <<= 1;
		bit >>= 1;
	}	 
			
	if (remainder >= divider)
	{
		quotient++;
	}
	
	fix16_t result = quotient;
	
	/* Figure out the sign of result */
	if ((a ^ b) & 0x80000000)
	{
		if (result == fix16_minimum)
				return fix16_overflow;
		
		result = -result;
	}
	
	return result;
}

/* Wrapper around fix16_div to add saturating arithmetic. */
fix16_t fix16_sdiv(fix16_t inArg0, fix16_t inArg1)
{
	fix16_t result = fix16_div(inArg0, inArg1);
	
	if (result == fix16_overflow)
	{
		if ((inArg0 >= 0) == (inArg1 >= 0))
			return fix16_maximum;
		else
			return fix16_minimum;
	}
	
	return result;
}

fix16_t fix16_mod(fix16_t x, fix16_t y)
{
		/* The reason we do this, rather than use a modulo operator
		 * is that if you don't have a hardware divider, this will result
		 * in faster operations when the angles are close to the bounds. 
		 */
		while(x >=  y) x -= y;
		while(x <= -y) x += y;

	return x;
}

/* The square root algorithm is quite directly from
 * http://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Binary_numeral_system_.28base_2.29
 * An important difference is that it is split to two parts
 * in order to use only 32-bit operations.
 *
 * Note that for negative numbers we return -sqrt(-inValue).
 * Not sure if someone relies on this behaviour, but not going
 * to break it for now. It doesn't slow the code much overall.
 */
fix16_t fix16_sqrt(fix16_t inValue)
{
	uint8_t  neg = (inValue < 0);
	uint32_t num = (neg ? -inValue : inValue);
	uint32_t result = 0;
	uint32_t bit;
	uint8_t  n;
	
	// Many numbers will be less than 15, so
	// this gives a good balance between time spent
	// in if vs. time spent in the while loop
	// when searching for the starting value.
	if (num & 0xFFF00000)
		bit = (uint32_t)1 << 30;
	else
		bit = (uint32_t)1 << 18;
	
	while (bit > num) bit >>= 2;
	
	// The main part is executed twice, in order to avoid
	// using 64 bit values in computations.
	for (n = 0; n < 2; n++)
	{
		// First we get the top 24 bits of the answer.
		while (bit)
		{
			if (num >= result + bit)
			{
				num -= result + bit;
				result = (result >> 1) + bit;
			}
			else
			{
				result = (result >> 1);
			}
			bit >>= 2;
		}
		
		if (n == 0)
		{
			// Then process it again to get the lowest 8 bits.
			if (num > 65535)
			{
				// The remainder 'num' is too large to be shifted left
				// by 16, so we have to add 1 to result manually and
				// adjust 'num' accordingly.
				// num = a - (result + 0.5)^2
				//	 = num + result^2 - (result + 0.5)^2
				//	 = num - result - 0.5
				num -= result;
				num = (num << 16) - 0x8000;
				result = (result << 16) + 0x8000;
			}
			else
			{
				num <<= 16;
				result <<= 16;
			}
			
			bit = 1 << 14;
		}
	}

	// Finally, if next bit would have been 1, round the result upwards.
	if (num > result)
	{
		result++;
	}
	
	return (neg ? -(fix16_t)result : (fix16_t)result);
}

fix16_t fix16_exp(fix16_t inValue) {
	if(inValue == 0        ) return fix16_one;
	if(inValue == fix16_one) return fix16_e;
	if(inValue >= 681391   ) return fix16_maximum;
	if(inValue <= -772243  ) return 0;
                      
	/* The algorithm is based on the power series for exp(x):
	 * http://en.wikipedia.org/wiki/Exponential_function#Formal_definition
	 * 
	 * From term n, we get term n+1 by multiplying with x/n.
	 * When the sum term drops to zero, we can stop summing.
	 */
            
	// The power-series converges much faster on positive values
	// and exp(-x) = 1/exp(x).
	bool neg = (inValue < 0);
	if (neg) inValue = -inValue;
            
	fix16_t result = inValue + fix16_one;
	fix16_t term = inValue;

	uint_fast8_t i;        
	for (i = 2; i < 30; i++)
	{
		term = fix16_mul(term, fix16_div(inValue, fix16_from_int(i)));
		result += term;
                
		if ((term < 500) && ((i > 15) || (term < 20)))
			break;
	}
            
	if (neg) result = fix16_div(fix16_one, result);
            
	return result;
}



fix16_t fix16_log(fix16_t inValue)
{
	fix16_t guess = fix16_from_int(2);
	fix16_t delta;
	int scaling = 0;
	int count = 0;
	
	if (inValue <= 0)
		return fix16_minimum;
	
	// Bring the value to the most accurate range (1 < x < 100)
	const fix16_t e_to_fourth = 3578144;
	while (inValue > fix16_from_int(100))
	{
		inValue = fix16_div(inValue, e_to_fourth);
		scaling += 4;
	}
	
	while (inValue < fix16_one)
	{
		inValue = fix16_mul(inValue, e_to_fourth);
		scaling -= 4;
	}
	
	do
	{
		// Solving e(x) = y using Newton's method
		// f(x) = e(x) - y
		// f'(x) = e(x)
		fix16_t e = fix16_exp(guess);
		delta = fix16_div(inValue - e, e);
		
		// It's unlikely that logarithm is very large, so avoid overshooting.
		if (delta > fix16_from_int(3))
			delta = fix16_from_int(3);
		
		guess += delta;
	} while ((count++ < 10)
		&& ((delta > 1) || (delta < -1)));
	
	return guess + fix16_from_int(scaling);
}



static inline fix16_t fix16_rs(fix16_t x)
{
    fix16_t y = (x >> 1) + (x & 1);
    return y;
}

/**
 * This assumes that the input value is >= 1.
 * 
 * Note that this is only ever called with inValue >= 1 (because it has a wrapper to check. 
 * As such, the result is always less than the input. 
 */
static fix16_t fix16__log2_inner(fix16_t x)
{
	fix16_t result = 0;
	
	while(x >= fix16_from_int(2))
	{
		result++;
		x = fix16_rs(x);
	}

	if(x == 0) return (result << 16);

	uint_fast8_t i;
	for(i = 16; i > 0; i--)
	{
		x = fix16_mul(x, x);
		result <<= 1;
		if(x >= fix16_from_int(2))
		{
			result |= 1;
			x = fix16_rs(x);
		}
	}
    x = fix16_mul(x, x);
    if(x >= fix16_from_int(2)) result++;

	
	return result;
}



/**
 * calculates the log base 2 of input.
 * Note that negative inputs are invalid! (will return fix16_overflow, since there are no exceptions)
 * 
 * i.e. 2 to the power output = input.
 * It's equivalent to the log or ln functions, except it uses base 2 instead of base 10 or base e.
 * This is useful as binary things like this are easy for binary devices, like modern microprocessros, to calculate.
 * 
 * This can be used as a helper function to calculate powers with non-integer powers and/or bases.
 */
fix16_t fix16_log2(fix16_t x)
{
	// Note that a negative x gives a non-real result.
	// If x == 0, the limit of log2(x)  as x -> 0 = -infinity.
	// log2(-ve) gives a complex result.
	if (x <= 0) return fix16_overflow;

	// If the input is less than one, the result is -log2(1.0 / in)
	if (x < fix16_one)
	{
		// Note that the inverse of this would overflow.
		// This is the exact answer for log2(1.0 / 65536)
		if (x == 1) return fix16_from_int(-16);

		fix16_t inverse = fix16_div(fix16_one, x);
		return -fix16__log2_inner(inverse);
	}

	// If input >= 1, just proceed as normal.
	// Note that x == fix16_one is a special case, where the answer is 0.
	return fix16__log2_inner(x);
}

/**
 * This is a wrapper for fix16_log2 which implements saturation arithmetic.
 */
fix16_t fix16_slog2(fix16_t x)
{
	fix16_t retval = fix16_log2(x);
	// The only overflow possible is when the input is negative.
	if(retval == fix16_overflow)
		return fix16_minimum;
	return retval;
}

static const uint32_t scales[8] = {
    /* 5 decimals is enough for full fix16_t precision */
    1, 10, 100, 1000, 10000, 100000, 100000, 100000
};

static char *itoa_loop(char *buf, uint32_t scale, uint32_t value, bool skip)
{
    while (scale)
    {
        unsigned digit = (value / scale);
    
        if (!skip || digit || scale == 1)
        {
            skip = false;
            *buf++ = '0' + digit;
            value %= scale;
        }
        
        scale /= 10;
    }
    return buf;
}

void fix16_to_str(fix16_t value, char *buf, int decimals)
{
    uint32_t uvalue = (value >= 0) ? value : -value;
    if (value < 0)
        *buf++ = '-';

    /* Separate the integer and decimal parts of the value */
    unsigned intpart = uvalue >> 16;
    uint32_t fracpart = uvalue & 0xFFFF;
    uint32_t scale = scales[decimals & 7];
    fracpart = fix16_mul(fracpart, scale);
    
    if (fracpart >= scale)
    {
        /* Handle carry from decimal part */
        intpart++;
        fracpart -= scale;    
    }
    
    /* Format integer part */
    buf = itoa_loop(buf, 10000, intpart, true);
    
    /* Format decimal part (if any) */
    if (scale != 1)
    {
        *buf++ = '.';
        buf = itoa_loop(buf, scale / 10, fracpart, false);
    }
    
    *buf = '\0';
}

fix16_t fix16_from_str(const char *buf)
{
    while (isspace(*buf))
        buf++;
    
    /* Decode the sign */
    bool negative = (*buf == '-');
    if (*buf == '+' || *buf == '-')
        buf++;

    /* Decode the integer part */
    uint32_t intpart = 0;
    int count = 0;
    while (isdigit(*buf))
    {
        intpart *= 10;
        intpart += *buf++ - '0';
        count++;
    }
    
    if (count == 0 || count > 5
        || intpart > 32768 || (!negative && intpart > 32767))
        return fix16_overflow;
    
    fix16_t value = intpart << 16;
    
    /* Decode the decimal part */
    if (*buf == '.' || *buf == ',')
    {
        buf++;
        
        uint32_t fracpart = 0;
        uint32_t scale = 1;
        while (isdigit(*buf) && scale < 100000)
        {
            scale *= 10;
            fracpart *= 10;
            fracpart += *buf++ - '0';
        }
        
        value += fix16_div(fracpart, scale);
    }
    
    /* Verify that there is no garbage left over */
    while (*buf != '\0')
    {
        if (!isdigit(*buf) && !isspace(*buf))
            return fix16_overflow;
        
        buf++;
    }
    
    return negative ? -value : value;
}

