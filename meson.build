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

zip_dep = dependency('libzip', version: '>=1.7.3')
cairo_dep = dependency('cairo', version: '>=1.16.0')
webp_dep = dependency('libwebp', version: '>=1.0.0')
rsvg_dep = dependency('librsvg-2.0', version: '>=2.46.0')

deps = [zip_dep, cairo_dep, webp_dep, rsvg_dep]

src = files(
    'src/arsc.c',
    'src/apk.c',
    'src/binary_reader.c',
    'src/chunk.c',
    'src/image.c',
    'src/main.c',
    'src/resource_value.c',
    'src/string_pool.c',
    'src/svg.c',
    'src/svg_writer.c',
    'src/xml.c',
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
thumbnailer_conf.set(
    'MIME_TYPES',
    'application/vnd.android.package-archive;',
)

configure_file(
    input: 'apk-thumbnailer.thumbnailer.in',
    output: 'apk-thumbnailer.thumbnailer',
    configuration: thumbnailer_conf,
    install: true,
    install_dir: get_option('datadir') / 'thumbnailers',
)
