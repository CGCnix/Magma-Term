project('magma', 'c')

add_project_arguments('-D_XOPEN_SOURCE=700', language: 'c')

deps = [ dependency('fontconfig'), dependency('libdrm'), dependency('freetype2'), dependency('xcb'), dependency('xkbcommon'), dependency('xkbcommon-x11'), dependency('xcb-image'), dependency('wayland-client')]

wayland_protocols = dependency('wayland-protocols')
wayland_scanner = dependency('wayland-scanner')
wayland_scanner_bin = find_program(wayland_scanner.get_variable('wayland_scanner'))

protodirs = wayland_protocols.get_variable('pkgdatadir')

xdg_shell_proto = protodirs + '/stable/xdg-shell/xdg-shell.xml'

message(xdg_shell_proto)

xdg_shell_headers = custom_target(
	'xdg_shell_header',
	output: '@BASENAME@.h',
	input: xdg_shell_proto,
	command: [wayland_scanner_bin, 'client-header', '@INPUT@', '@OUTPUT@'])

xdg_shell_src = custom_target(
	'xdg_shell_src',
	output: '@BASENAME@.c',
	input: xdg_shell_proto,
	command: [wayland_scanner_bin, 'private-code', '@INPUT@', '@OUTPUT@']
)


inc = include_directories('includes')

executable('magma', [ 'src/main.c', 'src/logger/log.c', 'src/backend/xcb.c', 'src/backend/wl.c', 'src/backend/drm.c', 'src/backend/backend.c', xdg_shell_src, xdg_shell_headers], dependencies : deps, include_directories : inc)
