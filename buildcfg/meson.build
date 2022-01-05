
# configure arch specific settings

simd_compiler_flags_map = {
    'SSE4' : ['-mmmx', '-msse', '-msse2', '-msse3', '-mssse3', '-msse4', '-msse4.1', '-msse4.2'],
    'AESNI' : ['-maes'],
    'CX16' : ['-mcx16'],
    'AVX2' : ['-mavx', '-mavx2'],
    'NOAVX' : ['-mno-avx', '-mno-avx2', '-march=westmere'],
    'AVX512' : ['-mavx512']
}

simd_define_map = {
    'AESNI' : 'HAS_AESNI',
    'CX16' : 'HAS_CX16',
    'AVX2' : 'HAS_AVX2',
    'AVX512' : 'HAS_AVX512',
}

simd_flags = []

# We always want at least up to SSE4.2 and AESNI
simd_flags += 'SSE4'
simd_flags += 'AESNI'
simd_flags += 'CX16'

if opt_reduced_simd_level
    simd_flags += 'NOAVX'
else
    simd_flags += 'AVX2'
endif

if opt_enable_avx512
    simd_flags += 'AVX512'
endif

foreach simd_flag : simd_flags
    cxx_flags += simd_compiler_flags_map.get(simd_flag)
endforeach

foreach simd_flag, simd_define : simd_define_map
    cfg.set(simd_define, simd_flag in simd_flags)
endforeach
