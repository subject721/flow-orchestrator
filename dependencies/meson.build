boost_modules = [
  'system',
  'program_options',
  'date_time'
]

if opt_use_conan_deps
    local_cmake_descr_search_path = 'dependencies'
endif

dep_null = dependency('', required: false)


#
dep_threads = dependency('threads')

dep_boost = dep_null
dep_fmt = dep_null
dep_dpdk = dep_null
dep_asmjit = dep_null
dep_luajit = dep_null
dep_sol = dep_null


if opt_use_conan_deps
    error('Conan dependencies not supported yet!')

    dep_boost = dependency('boost', modules : boost_modules, method: 'cmake', cmake_module_path: local_cmake_descr_search_path)

    dep_dpdk = dependency('libdpdk-libs', method: 'cmake', cmake_module_path: local_cmake_descr_search_path)

    dep_luajit = dependency('luajit', method: 'cmake', cmake_module_path: local_cmake_descr_search_path)
else
    dep_boost = dependency('boost', modules : boost_modules)

    dep_dpdk = dependency('libdpdk-libs', required: false)

    if not dep_dpdk.found()
        dep_dpdk = dependency('dpdk', required: false)
    endif

    if not dep_dpdk.found()
        error('No usable DPDK installation found')
    endif

    cxx_flags += '-DALLOW_EXPERIMENTAL_API=1'

    dep_luajit = dependency('luajit')

    dep_sol = dependency('sol2', method: 'cmake')

    cxx_flags += '-DSOL_EXCEPTIONS_SAFE_PROPAGATION'
    cxx_flags += '-DSOL_LUAJIT=1'

    dep_fmt = dependency('fmt')
endif

dep_foreign = []

dep_foreign += dep_threads
dep_foreign += dep_boost
dep_foreign += dep_dpdk
dep_foreign += dep_asmjit
dep_foreign += dep_luajit
dep_foreign += dep_sol
dep_foreign += dep_fmt

