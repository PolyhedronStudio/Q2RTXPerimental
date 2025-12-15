/********************************************************************
*
*
*   Bit(set) Utilities:
*
*
********************************************************************/
#pragma once


#define BIT(n)          (1U << (n))
#define BIT_ULL(n)      (1ULL << (n))
#define BIT_LL(n)		(1LL << (n))

#define Q_IsBitSet(data, bit)   (((data)[(bit) >> 3] & (1 << ((bit) & 7))) != 0)
#define Q_SetBit(data, bit)     ((data)[(bit) >> 3] |= (1 << ((bit) & 7)))
#define Q_ClearBit(data, bit)   ((data)[(bit) >> 3] &= ~(1 << ((bit) & 7)))