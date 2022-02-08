flow_orchestrator_sources += files([
    'common/common.cpp',
    'common/file_utils.cpp',
    'common/lua_common.cpp',
    'common/network_utils.cpp',
    'dpdk/dpdk_common.cpp',
    'dpdk/dpdk_ethdev.cpp',
    'flow_base.cpp',
    'flow_builder_types.cpp',
    'flow_config.cpp',
    'flow_processor.cpp',
    'flow_endpoints.cpp',
    'flow_manager.cpp'
])

if opt_enable_telemetry_if
    flow_orchestrator_sources += files(['flow_telemetry.cpp'])
endif

main_source = files('main.cpp')

internal_lua_utils_src = files(['lua_utils.lua'])

internal_lua_utils_hdr = 'lua_utils.h'

internal_lua_utils_tgt = custom_target(
    'internal_lua_utils',
    output : internal_lua_utils_hdr,
    input : internal_lua_utils_src,
    command : [tool_xxd, '-i', '-C', '@INPUT@', '@OUTPUT@'],
)

flow_orchestrator_sources += internal_lua_utils_tgt