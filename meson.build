project('magma', 'c')

add_project_arguments('-D_XOPEN_SOURCE=700 -Wall -Werror -pedantic', language: 'c')

deps = [ dependency('fontconfig'), dependency('freetype2'), dependency('xkbcommon'), dependency('xkbcommon-x11')]

src_files = [ 'src/main.c', 'src/font.c', 'src/vt.c', 'src/logger/log.c', 'src/backend/backend.c']

if get_option('disable-xcb')
  message('libxcb disabled you will be unable to run this build on X')
  add_project_arguments('-D_MAGMA_NO_XCB_', language: 'c')
else
  xcb = dependency('xcb')
  xcbimage = dependency('xcb-image')
  src_files += 'src/backend/xcb.c'
  deps += xcb
  deps += xcbimage
endif

if get_option('disable-drm')
  message('libdrm disabled you will be unable to run this build on drm')
  add_project_arguments('-D_MAGMA_NO_DRM_', language: 'c')
else
  src_files += 'src/backend/drm.c'

  libdrm = dependency('libdrm')
  deps += libdrm
endif


if get_option('disable-wl')
  message('wayland_client disabled you will be unable to run this build on wayland')
  add_project_arguments('-D_MAGMA_NO_WL_', language: 'c')
else
  src_files += 'src/backend/wl/wl.c'
  src_files += 'src/backend/wl/input.c'

  wayland_client = dependency('wayland-client')
  deps += wayland_client



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

  src_files += xdg_shell_src
  src_files += xdg_shell_headers
endif

inc = include_directories('includes')

executable('magma', src_files, dependencies : deps, include_directories : inc)
