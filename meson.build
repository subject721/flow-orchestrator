project(
  'dpdk-flow-orchestrator', 
  ['cpp'],
  default_options : ['cpp_std=c++17', 'default_library=static', 'warning_level=2'],
  license: ['BSD 3-Clause License'],
  version: '0.1.0'
)

############################
# Base setup, modules, etc.
############################

system = host_machine.system()

cpp_compiler = meson.get_compiler('cpp')

project_root = meson.project_source_root()

cfg = configuration_data()

cxx_flags = []

cfg.set('version', meson.project_version())

cfg.set10('DEBUG', get_option('debug'))

opt_buildtype = get_option('buildtype')

opt_use_conan_deps = get_option('use-conan-deps')

opt_reduced_simd_level = get_option('reduced-simd-level')
opt_enable_avx512 = get_option('enable-avx512')

if opt_reduced_simd_level and opt_enable_avx512
    error('Cannot enable AVX512 while using a reduced SIMD feature set at the same time!')
endif

opt_enable_telemetry_if = get_option('enable-telemetry')


cfg.set10('TELEMETRY_ENABLED', opt_enable_telemetry_if)


cxx_flags += '-Wno-unused-parameter'

tool_xxd = find_program(['xxd'], required: true)


subdir('buildcfg')


############################
# Dependency setup
############################

subdir('dependencies')


############################
# Include subdirs 
############################

include_dirs = []
flow_orchestrator_sources = []

include_dirs += include_directories('include')

subdir('include')

subdir('src')


############################
# Define Build Targets
############################



flow_orchestrator_lib = library(
    'flow-orchestrator',
    flow_orchestrator_sources,
    dependencies: dep_foreign,
    cpp_args: cxx_flags,
    include_directories: include_dirs
)

dep_flow_orchestrator_lib = declare_dependency(dependencies: dep_foreign, include_directories: include_dirs, link_with: flow_orchestrator_lib)

flow_orchestrator = executable(
  'flow-orchestrator',
  main_source,
  dependencies: [dep_foreign, dep_flow_orchestrator_lib],
  cpp_args: cxx_flags,
  include_directories: include_dirs)

test_sources = {
    'test01' : files(['test/test01.cpp']),
    'test02' : files(['test/test02.cpp'])
}

test_executables = []

test_includes = [include_dirs, include_directories('test')]

test_file_names = []
test_file_targets = []

subdir('test')

foreach test_name, test_sources : test_sources

    test_sources_full = test_sources

    test_executables += executable(
      test_name,
      test_sources_full,
      dependencies: [dep_foreign, dep_flow_orchestrator_lib],
      cpp_args: cxx_flags,
      include_directories: include_dirs)
endforeach



summary({'Buildtype' : opt_buildtype,
        'Version' : cfg.get('version'),
        'Using Conan deps' : opt_use_conan_deps,
        'Using reduced SIMD level' : opt_reduced_simd_level,
        'Using AVX512' : opt_enable_avx512,
        'SIMD flags' : simd_flags,
        'Telemetry enabled' : opt_enable_telemetry_if
        }, section: 'Configuration')




