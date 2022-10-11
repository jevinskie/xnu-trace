def inner_bit_idx(bit_idx, word_bits):
    return bit_idx // word_bits * word_bits


def inner_word_idx(inner_bit_idx, t_bits):
    return inner_bit_idx // t_bits


def even_bit_idx(idx, t_bits):
    return idx % (2 * t_bits)


def odd_bit_idx(idx, t_bits):
    return (idx + t_bits) % (2 * t_bits)


def bit_idx(idx, word_bits, t_bits):
    wsbidx = inner_bit_idx(idx, word_bits)
    wswidx = inner_word_idx(wsbidx, t_bits)
    eidx = even_bit_idx(idx, t_bits)
    oidx = odd_bit_idx(idx, t_bits)
    if wswidx % 2 == 0:
        bidx = eidx
    else:
        bidx = oidx
    return bidx


def bitarray_access_info(idx, word_bits, t_bits):
    bidx = idx * word_bits
    wsbidx = inner_bit_idx(bidx, word_bits)
    wswidx = inner_word_idx(wsbidx, t_bits)
    sb_idx = bit_idx(bidx, word_bits, t_bits)
    return (wswidx, sb_idx)
