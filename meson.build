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


# TODO: remove the hardcoded flags
cxx_flags += '-DALLOW_EXPERIMENTAL_API=1'
cxx_flags += '-DSOL_LUAJIT=1'
cxx_flags += '-mno-avx'
cxx_flags += '-mno-avx2'
cxx_flags += '-march=westmere'
cxx_flags += '-mmmx'
cxx_flags += '-msse'
cxx_flags += '-msse2'
cxx_flags += '-msse3'
cxx_flags += '-mssse3'
cxx_flags += '-msse4'
cxx_flags += '-msse4.1'
cxx_flags += '-msse4.2'
cxx_flags += '-maes'
cxx_flags += '-mcx16'


cxx_flags += '-fno-rtti'

cxx_flags += '-Wno-unused-parameter'


############################
# Dependency setup
############################


dep_null = dependency('', required: false)


boost_modules = [
  'system',
  'program_options',
  'date_time'
]

dep_threads = dependency('threads')

dep_boost = dependency('boost', modules : boost_modules)

dep_fmt = dependency('fmt')

dep_dpdk = dependency('libdpdk-libs')

dep_asmjit = dep_null # dependency('asmjit', method: 'cmake')

dep_luajit = dependency('luajit')

dep_sol = dependency('sol2', method: 'cmake')


dep_all = []

dep_all += dep_threads
dep_all += dep_boost
dep_all += dep_dpdk
dep_all += dep_asmjit
dep_all += dep_luajit
dep_all += dep_sol
dep_all += dep_fmt

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
    dependencies: dep_all,
    cpp_args: cxx_flags,
    include_directories: include_dirs
)

dep_flow_orchestrator_lib = declare_dependency(dependencies: dep_all, include_directories: include_dirs, link_with: flow_orchestrator_lib)

flow_orchestrator = executable(
  'flow-orchestrator',
  main_source,
  dependencies: [dep_all, dep_flow_orchestrator_lib],
  cpp_args: cxx_flags,
  include_directories: include_dirs)

test_sources = {
    'test01' : files(['test/test01.cpp']),
    'test02' : files(['test/test02.cpp'])
}

test_executables = []

test_includes = [include_dirs, include_directories('test')]

foreach test_name, test_sources : test_sources
    test_executables += executable(
      test_name,
      test_sources,
      dependencies: [dep_all, dep_flow_orchestrator_lib],
      cpp_args: cxx_flags,
      include_directories: include_dirs)
endforeach




