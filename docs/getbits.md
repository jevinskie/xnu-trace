# getbits

```cpp
template <unsigned int n, unsigned int m> inline unsigned long getbits(unsigned long[] bits) {
    const unsigned bitsPerLong   = sizeof(unsigned long) * CHAR_BIT;
    const unsigned int bitsToGet = m - n;
    BOOST_STATIC_ASSERT(bitsToGet < bitsPerLong);
    const unsigned mask = (1UL << bitsToGet) - 1;
    const size_t index0 = n / bitsPerLong;
    const size_t index1 = m / bitsPerLong;
    // Do the bits to extract straddle a boundary?
    if (index0 == index1) {
        return (bits[index0] >> (n % bitsPerLong)) & mask;
    } else {
        return ((bits[index0] >> (n % bitsPerLong)) +
                (bits[index1] << (bitsPerLong - (m % bitsPerLong)))) &
               mask;
    }
}
```
