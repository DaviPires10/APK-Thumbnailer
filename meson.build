project(
  'apk-thumbnailer',
  'c',
  version: '1.0.0',
  meson_version: '>=1.2',
  default_options: [
    'buildtype=release',
    'c_std=gnu11',
  ],
)

zip_dep = dependency('libzip', version: '>=1.0')
magick_dep = dependency('MagickWand', version: '>=7.0')

deps = [zip_dep, magick_dep]

src = files(
  'src/main.c',
  'src/apk.c',
  'src/utils.c',
)


executable(
  'apk-thumbnailer',
  src,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'),
)

# Install .thumbnailer file
thumbnailer_conf = configuration_data()
thumbnailer_conf.set('BINDIR', get_option('prefix') / get_option('bindir'))
thumbnailer_conf.set('MIME_TYPES', 'application/vnd.android.package-archive;application/x-apk;')

configure_file(
  input: 'apk-thumbnailer.thumbnailer.in',
  output: 'apk-thumbnailer.thumbnailer',
  configuration: thumbnailer_conf,
  install: true,
  install_dir: get_option('datadir') / 'thumbnailers',
)

test(
  'apk-thumbnailer-help',
  find_program('apk-thumbnailer', dirs: meson.current_build_dir()),
  args: ['-h'],
  should_fail: false,
  workdir: meson.current_build_dir(),
)
