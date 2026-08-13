# APK Thumbnailer

APK Thumbnailer is a fast and lightweight icon extractor for Android APK files, written in C.

It extracts the launcher icon from an APK and generates a thumbnail image, making it useful for file managers that support the [Freedesktop Thumbnail Management Specification](https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html).

---

## Dependencies

The following libraries and tools are required:

- [libzip](https://libzip.org)
- [cairo](https://cairographics.org/)
- [librsvg](https://wiki.gnome.org/Projects/LibRsvg)
- [libwebp](https://developers.google.com/speed/webp)
- Meson
- Ninja
- A C compiler with C11 support (GCC or Clang)

---

## Building

```bash
meson setup build
meson compile -C build
```

The executable will be created at:

```bash
build/apk-thumbnailer
```

---

## Installation

```bash
sudo meson install -C build
```

This installs:

- `apk-thumbnailer` into `$prefix/bin`
- `apk-thumbnailer.thumbnailer` into `$prefix/share/thumbnailers`

---

## Usage

```bash
apk-thumbnailer -i input.apk -o output.png -s SIZE
```

### Options

| Option          | Description              |
| --------------- | ------------------------ |
| `-i, --input`   | Input APK file           |
| `-o, --output`  | Output PNG file          |
| `-s, --size`    | Thumbnail size in pixels |
| `-v, --verbose` | Enable verbose output    |
| `-h, --help`    | Show help message        |

Example:

```bash
apk-thumbnailer -i app.apk -o thumbnail.png -s 256
```

---

## How It Works

1. Reads `AndroidManifest.xml` from the APK
2. Finds the application icon resource
3. Resolves icon paths and color values from `resources.arsc`
4. Detects file MIME types to properly handle PNG, WEBP, or XML data
5. Parses Android Binary XML to support Adaptive Icons and Vector Drawables, rendering SVG data and compositing layers
6. Resizes and exports the thumbnail as a PNG

---

## Limitations

- APK split resources are not supported

---

## Credits

- [kde-thumbnailer-apk](https://github.com/z3ntu/kde-thumbnailer-apk) for the original implementation and APK parsing logic.
- [Just An Application](https://justanapplication.wordpress.com/tag/androidbinaryxml) for the documentation on the Android Binary XML format.
- [Sens' AndroidBinaryXml Structure Visual](https://raw.githubusercontent.com/senswrong/AndroidBinaryXml/main/AndroidBinaryXml.png) for the visual breakdown of the chunk formats.
- [Android VectorDrawable Documentation](https://developer.android.com/reference/android/graphics/drawable/VectorDrawable.html) for mapping Android attributes to standard vector geometry.
- [avd-to-svg](https://github.com/restorer/avd-to-svg/blob/master/README.md) for providing a reference roadmap on transforming Android Vector Drawables into standard SVG markup.
- [Android Platform Framework ResourceTypes.h](https://android.googlesource.com/platform/frameworks/base/+/master/libs/androidfw/include/androidfw/ResourceTypes.h) for the native implementation layouts of the AXML chunks.
- [Android SDK TypedValue.java](https://github.com/AndroidSDKSources/android-sdk-sources-for-api-level-25/blob/master/android/util/TypedValue.java) for the bit-masking algorithms used to decode dynamic dimensions, fractions, and radix values.
