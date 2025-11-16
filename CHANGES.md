# Change Log

### v1.2.0 - 2025-10-XX

##### Updates :arrow_up:

- Upgraded to support O3DE 25.10.
- Updated all `AZ_CLASS_ALLOCATOR` macro calls to remove deprecated third parameter (alignment) for O3DE 25.10 compatibility.
- Updated CMake minimum version requirement to 3.24 in `External/CMakeLists.txt` to meet O3DE 25.10 requirements.
- Updated root `CMakeLists.txt` to use new `o3de_read_json_key` pattern for O3DE 25.10.
- Updated `Code/CMakeLists.txt` to use new platform include pattern, replacing deprecated `ly_get_list_relative_pal_filename`.
- Created platform-specific `platform_*.cmake` files for all supported platforms (Windows, Linux, Mac, Android, iOS).
- Updated `gem.json` to include `dependencies` field and align with O3DE 25.10 gem.json format.
- Upgraded CesiumNative to v0.53.0 (latest stable version). See [Cesium Native v0.53.0](https://github.com/GameInstitute/cesium-native/tree/v0.53.0) for details.
- Fixed C++20 compatibility issue in async++ library by replacing deprecated `std::result_of` with `std::invoke_result_t`.
- Fixed O3DE 25.10 API changes:
  - `GetImageSubresourceLayout` now returns `DeviceImageSubresourceLayout` instead of `ImageSubresourceLayout`. Updated `BeginMip` calls to use `DeviceImageSubresourceLayout` directly.
  - `MaterialAssetCreator::Begin` now accepts 2 parameters instead of 3 (removed the third boolean parameter).
  - `AcquireMesh` now accepts only `MeshHandleDescriptor` parameter. The descriptor no longer includes `m_isExclusive`, `m_useFastApproximation`, or `m_materialAssignmentMap` members. Material assignment must be done separately using `SetMaterialAssignmentMap` after acquiring the mesh.
  - `SetMaterialAssignmentMap` now requires a `MaterialAssignmentMap` instead of a single material instance.
  - `GetWorldBounds` and `GetLocalBounds` methods are no longer part of `BoundsRequestBus` interface in O3DE 25.10. Removed `override` specifier from these methods.
  - Updated test fixtures from `UnitTest::AllocatorsTestFixture` to `UnitTest::LeakDetectionFixture` for O3DE 25.10 compatibility.

##### Fixes :wrench:

- Fixed CMake error when `External/Packages/Install/SHA256SUMS` file doesn't exist yet. The build system now handles missing SHA256SUMS file gracefully with a placeholder hash and warning message. Build the External package first to generate the proper SHA256SUMS file.
- Removed deprecated `AZ::AWSNativeSDKInit` dependency and updated code to use AWS SDK's native `Aws::InitAPI()` and `Aws::ShutdownAPI()` methods directly.
- Removed `Gem::Atom_Feature_Common.Static` dependency as it's no longer available in O3DE 25.10. The `MeshFeatureProcessorInterface` functionality should be available through `Gem::Atom_RPI.Public`.

### v1.1.0 - 2022-10-17

##### Fixes :wrench:

- Fixes support for O3DE v22.10.0.
- Change texture's addressU and addressV from wrap to clamp to fix the white seam when rendering imagery.

### v1.0.0 - 2022-02-15 - Initial Release

##### Features :tada:

- High-accuracy, global-scale WGS84 globe for visualization of real-world 3D content
- 3D Tiles runtime engine to stream massive 3D geospatial datasets, such as terrain, imagery, 3D cities, and photogrammetry
  - Streaming from the cloud, a private network, or the local machine.
  - Level-of-detail selection
  - Caching
  - Multithreaded loading
  - Batched 3D Model (B3DM) content, including the B3DM content inside Composite (CMPT) tiles
  - glTF content
  - `quantized-mesh` terrain loading and rendering
  - Bing Maps and Tile Map Service (TMS) raster overlays draped on terrain
- Integrated with Cesium ion for instant access to cloud based global 3D content.
- Integrated with O3DE Engine Editor, Entities and Components, and Script Canvas.
