# Vita gio-lite: libgio-2.0 with only GListModel in it (see gio/giotypes.h).
gio_lite_inc = include_directories('.')

install_headers('gio/gio.h', 'gio/giotypes.h', 'gio/glistmodel.h', install_dir : get_option('includedir') / 'glib-2.0' / 'gio')

libgio = library('gio-2.0', 'glistmodel.c',
  include_directories : [configinc, glibinc, gio_lite_inc],
  dependencies : [libglib_dep, libgobject_dep],
  c_args : ['-DG_LOG_DOMAIN="GLib-GIO"', '-DGIO_COMPILATION'],
  install : true,
)

pkg.generate(libgio,
  requires : ['glib-2.0', 'gobject-2.0'],
  version : glib_version,
  install_dir : glib_pkgconfigreldir,
  filebase : 'gio-2.0',
  name : 'GIO',
  description : 'glib I/O library (PS Vita build: GListModel only)',
)

libgio_dep = declare_dependency(link_with : libgio,
  include_directories : [gio_lite_inc],
  dependencies : [libgobject_dep])
meson.override_dependency('gio-2.0', libgio_dep)
