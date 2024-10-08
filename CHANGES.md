# Changelog

## [v3.1.1][v3.1.1] - 08 Oct 2024

* Fix build on Xcode 16 ([gh-10][gh-10])
* CI: add macOS 15, remove macOS 12

[v3.1.1]: https://github.com/gavv/libASPL/releases/tag/v3.1.1

[gh-10]: https://github.com/gavv/libASPL/issues/10

## [v3.1.0][v3.1.0] - 04 May 2024

* Support granular overriding of device I/O operations ([gh-7][gh-7], [90e83][90e83])
* Add Device::GetControlHandler() and GetIOHandler() ([db0af][db0af])
* Add protection from adding/removing same device multiple times ([ed832][ed832])
* Add Plugin::HasDevice() ([dca6f][dca6f])
* Fix case when AddStreamAsync() or other configuration change is done before calling AddDevice() ([2f2ad][2f2ad])
* Fix race when device configuration change happen concurrently with AddDevice()/RemoveDevice() ([2f2ad][2f2ad])
* Fix building as a standalone project ([gh-6][gh-6])
* Fix `launchctl kickstart` in examples ([gh-8][gh-8])
* Fix tests on M1 ([b6920][b6920])
* Improve CI (bump macOS versions, add ARM64 runner)
* Improve documentation

[v3.1.0]: https://github.com/gavv/libASPL/releases/tag/v3.1.0

[gh-6]: https://github.com/gavv/libASPL/pull/6
[gh-7]: https://github.com/gavv/libASPL/issues/7
[gh-8]: https://github.com/gavv/libASPL/issues/8

[b6920]: https://github.com/gavv/libASPL/commit/b6920f3e49f061d94c98d632f3f8f21f0a20adc5
[db0af]: https://github.com/gavv/libASPL/commit/db0afc8ecc9c983e460621bd2f2cda145bf9050c
[90e83]: https://github.com/gavv/libASPL/commit/90e83424fd1174ef62e342726938b91520390db4
[2f2ad]: https://github.com/gavv/libASPL/commit/2f2ad68cabd0438281397e6cb02aa0ec247dc071
[ed832]: https://github.com/gavv/libASPL/commit/ed8328d8cdcae25ef85c72a233fe9e2f45fed333
[dca6f]: https://github.com/gavv/libASPL/commit/dca6f729bded554bdfd1a2071383054d10e90c81

## [v3.0.0][v3.0.0] - 20 Sep 2023

* **Breaking change:** rework sample rate handling ([af501][af501])
* Implement Persistent Storage API ([67fbc][67fbc], [22097][22097])
* Implement DriverRequestHandler API ([af0a6][af0a6])
* Support using raw pointers for handlers ([96e58][96e58])
* Fix overflow and hang when Tracer calls are unpaired ([6eef0][6eef0])
* Fix unpaired Tracer calls in SetPhysicalSampleRateAsync and SetVirtualSampleRateAsync ([6eef0][6eef0])
* Add depth limits in Tracer ([dbc46][dbc46])
* Add missing codes to StatusToString ([75ed0][75ed0])
* Add input device example (SinewaveDevice)
* Improve documentation

[v3.0.0]: https://github.com/gavv/libASPL/releases/tag/v3.0.0

[af501]: https://github.com/gavv/libASPL/commit/af501ee92670b07e3b5796e702dc7a943f1a8b14
[67fbc]: https://github.com/gavv/libASPL/commit/67fbc7033c6d26bd5a7ea647dde40875a13ce468
[22097]: https://github.com/gavv/libASPL/commit/22097e0d57c583a4511f6365b8a9869953f79dfe
[af0a6]: https://github.com/gavv/libASPL/commit/af0a62e7a29cbb42d675793c8d2ae48239ac6696
[96e58]: https://github.com/gavv/libASPL/commit/96e5808d40f36c4cd76bcbc5581cbe39663da15c
[6eef0]: https://github.com/gavv/libASPL/commit/6eef024d98b3125743614281cc9cb0df85305b0f
[dbc46]: https://github.com/gavv/libASPL/commit/dbc462b3e21254f2c1e4414ec69ebe8475f71c26
[75ed0]: https://github.com/gavv/libASPL/commit/75ed0eabfdd3256ebe2c5c51252346b78d6fb468

## [v2.0.2][v2.0.2] - 15 May 2023

* Fix build when used as submodule ([e3092][e3092])
* Fix in comments
* Add macOS 13 to CI

[v2.0.2]: https://github.com/gavv/libASPL/releases/tag/v2.0.2

[e3092]: https://github.com/gavv/libASPL/commit/e3092677b13614b8eafe5f93fe7f072feb4eff4d

## [v2.0.1][v2.0.1] - 23 Mar 2023

* Cleanup compiler flags
* Exclude code generation from build (hence users don't need Jinja2)
* Fix typos in README

[v2.0.1]: https://github.com/gavv/libASPL/releases/tag/v2.0.1

## [v2.0.0][v2.0.0] - 25 Nov 2022

* **Breaking change:** change argument types of setters and constructors ([ef992][ef992], [43198][43198])
* Fix work with Audio MIDI Setup, Logic Pro, Garage Band ([gh-1][gh-1])
* Fix build on recent Xcode
* Fix deprecation warnings
* Fix version exported by CMake package
* Tracer improvements

[v2.0.0]: https://github.com/gavv/libASPL/releases/tag/v2.0.0

[gh-1]: https://github.com/gavv/libASPL/issues/1

[ef992]: https://github.com/gavv/libASPL/commit/ef992e8bb4d6986e3b91b47b94d0aab2dd5e07ac
[43198]: https://github.com/gavv/libASPL/commit/4319814f5e55c20c300f6888d16b9e78d9066cbb

## [v1.1.1][v1.1.0] - 24 Nov 2022

* Fix compilation errors on older systems

[v1.1.1]: https://github.com/gavv/libASPL/releases/tag/v1.1.1

## [v1.1.0][v1.1.0] - 24 Nov 2022

* Support customizing tracer formatting
* Fix endian when printing 4-byte codes

[v1.1.0]: https://github.com/gavv/libASPL/releases/tag/v1.1.0

## [v1.0.1][v1.0.1] - 24 Aug 2021

* Don't build doxygen documentation by default

[v1.0.1]: https://github.com/gavv/libASPL/releases/tag/v1.0.1

## [v1.0.0][v1.0.0] - 02 Aug 2021

* Initial release

[v1.0.0]: https://github.com/gavv/libASPL/releases/tag/v1.0.0
