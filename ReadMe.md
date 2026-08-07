# io3DEngine

Dynamic Link Library (DLL) — DirectX 9 (Direct3D 9) based real-time 3D engine for game and interactive application development. Covers rendering, scene management, animation, particle system, physics/collision, GUI, sound, and video playback.

---

## Directory Structure

| Path | Description |
|------|-------------|
| `io*.cpp` / `io*.h` | Main engine source files. See **Engine Modules** below. |
| `Bullet/` | Bullet physics library (third-party). |
| `DevIL/` | DevIL image loading library (third-party). |
| `Opcode/` | Opcode collision detection library (AABB / OBB tree). |
| `Squish/` | DXT texture compression library (third-party). |
| `Themida/` | Protection / obfuscation tool temp files. |
| `TinyXML/` | XML parser library. **Already using TinyXML v2** (`tinyxml2.h`, `tinyxml2.lib`). Legacy version (`tinyxml.h`) retained for backward compatibility. |
| `res/` | Resource files (`.rc`, icons, bitmaps). |
| `Release/` | Build output — Release configuration. |
| `Shipping/` | Build output — Shipping configuration (final release). |

---

## Core Files

- **`io3DEngine.cpp`** — DLL entry point (`DllMain`). Initializes engine log (`info/engine.log`) and releases singleton managers (`ioThreadTaskManager`, `ioResourceLoader`) on detach.
- **`io3DEngine.vcxproj`** — Visual Studio project file. Build configs: Debug / Release / Shipping.
- **`StdAfx.h` / `StdAfx.cpp`** — Precompiled header (PCH). All `.cpp` files include `StdAfx.h` first.
- **`io3DEngine.rc`** — Resource script.
- **`Version.h`** — Engine version header.

---

## Engine Modules

### Rendering
| File | Description |
|------|-------------|
| `ioRenderSystem` | Core D3D9 device, swap chain, render state. |
| `ioRenderQueue` / `ioRenderQueueGroup` | Render queue & sorting. |
| `ioRenderable` / `ioRenderableList` | Renderable interface. |
| `ioShader` / `ioShaderManager` | Vertex/Pixel shader abstraction. |
| `ioMaterial` / `ioMaterialManager` | Material, technique, pass. |
| `ioTexture` / `ioTextureManager` | Texture resource management. |
| `ioEffect` / `ioEffectManager` | D3DX Effect (`.fx`) wrapper. |
| `ioPostFilter` / `ioPostOveray` | Post-processing (filter, overlay). |
| `ioGlowPostProcess` | Glow post-process effect. |

### Scene Management
| File | Description |
|------|-------------|
| `ioSceneManager` | Root scene manager. |
| `ioSceneNode` | Scene graph node (transform hierarchy). |
| `ioEntity` / `ioSubEntity` | Renderable entity in scene. |
| `ioMesh` / `ioSubMesh` / `ioMeshManager` | Mesh data & resource management. |
| `ioCamera` / `ioCameraController` | Camera & controllers (FPS, look-at, slerp). |
| `ioLight` / `ioLightSystem` | Lighting. |
| `ioSkyDome` | Sky dome rendering. |
| `ioLandScape` | Terrain / landscape. |
| `ioDecal` / `ioDecalMaker` | Decal system. |
| `ioEdgeRender` / `ioLineRender` | Edge & line rendering. |

### Animation
| File | Description |
|------|-------------|
| `ioAnimation` / `ioAnimationManager` | Keyframe animation. |
| `ioAniController` | Animation controller (state machine). |
| `ioAniTrack` / `ioRcAniTrack` | Animation track data. |
| `ioBiped` | Character / biped skeleton + mesh. |
| `ioSkeleton` / `ioSkeletonManager` | Skeleton hierarchy. |
| `ioAnimateFX` / `ioAnimateFXManager` | Animation effect (FX overlay). |
| `ioAnimationEvent` / `ioAnimationEventManager` | Timeline event system. |
| `ioPatch` / `ioPatchDefault` / `ioPatchMorphSW` | Patch / morph target animation. |

### Particle System
| File | Description |
|------|-------------|
| `ioParticleSystem` | Core particle system. |
| `ioParticle` | Particle data. |
| `ioParticleEmitter` / `ioEmitPointGenerator` / `ioModelEmitter` | Emitters (point, model). |
| `ioParticleAffector` / `ioRotationAffector` / `ioScaleAffector` / `ioLinearForceAffector` / `ioTexRotationAffector` | Affectors. |
| `ioModelParticleSystem` | Model-based particle system. |
| `ioParticleColorTable` | Color gradient table. |

### Physics & Collision
| File | Description |
|------|-------------|
| `Bullet/` | Bullet physics library. |
| `ioBulletHelper` | Engine ↔ Bullet glue. |
| `Opcode/` | Opcode collision library. |
| `ioOpcodeManager` / `ioOpcodeManagerImpl` | Opcode collision manager. |
| `ioOpcodeShape` / `ioOpcodeShapeImpl` | Collision shape (AABB/OBB tree). |

### Math
| File | Description |
|------|-------------|
| `ioMath` | Vector, matrix, quaternion utilities. |
| `ioPlane` / `ioRay` / `ioSegment` / `ioSphere` / `ioCylinder` | Geometry primitives. |
| `ioAxisAlignBox` / `ioOrientBox` | Bounding volumes. |
| `ioIntersectionUtility` | Intersection test utility. |
| `FastQuaternionSlerp.h` | Optimized quaternion slerp. |
| `QuaternionCompression` | Quaternion compression for network. |

### Resource & File I/O
| File | Description |
|------|-------------|
| `ioResource` / `ioResourceManager` / `ioResourceLoader` | Resource management base. |
| `ioINILoader` / `ioINIParser` | INI file parser. |
| `ioXMLDocument` / `ioXMLElement` | XML parser wrapper (uses **TinyXML v2**). |
| `ioStream` / `ioMemFile` / `ioDataChunk` | Stream & memory file I/O. |
| `ioHashString` | Hashed string for fast comparison. |
| `ioStringConverter` | String conversion utility. |

### GUI / Window
| File | Description |
|------|-------------|
| `ioGUIManager` | GUI root manager. |
| `ioWnd` / `ioWndEX` | Base window class. |
| `ioButton` / `ioEdit` / `ioScroll` / `ioList` / `ioTabControl` | UI widgets. |
| `ioButtonWndEX` / `ioEditBox` / `ioCheckBoxEX` / `ioRadioButtonEX` / `ioFrameWndEX` | Extended widgets. |
| `ioUIImage` / `ioUIImageSet` / `ioUIImageSetManager` | UI image & image-set. |
| `ioUIRenderElement` / `ioUIRenderer` / `ioUIRenderFrame` / `ioUIRenderImage` | UI rendering layer. |
| `ioPaperDoll` | Paper doll (equipment display). |
| `ioProgressBar` | Progress bar. |

### Input
| File | Description |
|------|-------------|
| `ioMouse` | Mouse input. |
| `ioIME` | IME (Input Method Editor) for text input. |

### Sound
| File | Description |
|------|-------------|
| `ioSound` / `ioSoundManager` | Sound system. |
| `ioOggSound` / `ioOggCallBack` | Ogg Vorbis playback. |
| `ioWaveFile` | WAV file loader. |

### Video
| File | Description |
|------|-------------|
| `ioVideoPlayer` | Video player (`.mp4`) via Media Foundation (`IMFMediaSession` + EVR). Outputs to D3D9 dynamic texture. Supports 3 render modes: UI, cinematic, and in-world (via `ioVideoTexture`). |
| `ioCinematicPlayer` | **Extended class from `ioVideoPlayer`.** Fullscreen cinematic video layer for cutscenes / intros. `IsFinished()` signals video completion. |
| `ioVideoTexture` | Adapter to bind video frame to in-world mesh (in-world video mode). |

### Font & Text
| File | Description |
|------|-------------|
| `ioFont` / `ioFontManager` / `ioFontWorkSpace` | Font resource & workspace. |
| `ioComplexStringPrinterBase` | Complex string printer (rich text). |
| `ioRichLabel` | Rich text label. |

### Threading & Performance
| File | Description |
|------|-------------|
| `ioThreadTaskManager` | Thread pool / task manager. |
| `ioCPU` / `CPUSpeedCheck` | CPU detection & speed check. |
| `ioSIMDGeneric` / `ioSIMDSSE` | SIMD (SSE) optimization. |
| `ioFrameTimer` / `ioTimer` | Frame & general timer. |
| `ioMemoryPool` | Memory pool template. |
| `ioVertexBufferHeap` / `ioIndexBufferHeap` | Buffer heap management. |
| `ioVertexBufferInstance` / `ioIndexBufferInstance` | Buffer instance management. |

### Image
| File | Description |
|------|-------------|
| `io2DImage` / `io2DImageManager` | 2D image resource. |
| `ioRc2DImage` | 2D image resource container. |
| `DevIL/` | DevIL image library (DDS, JPG, PNG, TGA, etc.). |

### Texture Compression
| File | Description |
|------|-------------|
| `Squish/` | DXT (S3TC) texture compression library. |

---

## Third-Party Libraries

| Library | Directory | Description |
|---------|-----------|-------------|
| Bullet | `Bullet/` | Physics simulation. |
| DevIL | `DevIL/` | Image loading. |
| Opcode | `Opcode/` | Collision detection (AABB/OBB tree). |
| Squish | `Squish/` | DXT texture compression. |
| TinyXML | `TinyXML/` | XML parser — **already using v2**. |
| Themida | `Themida/` | Protection tool (temp output). |

---

## Build Configuration

| File | Description |
|------|-------------|
| `DebugSheet.props` | Property sheet — Debug. |
| `ReleaseSheet.props` | Property sheet — Release. |
| `ShippingSheet.props` | Property sheet — Shipping (final). |
| `io3DEngine.vcxproj` | Visual Studio project file (VC++). |
| `io3DEngine.vcxproj.filters` | Solution Explorer filter file. |
| `io3DEngine.vcxproj.user` | User-specific project settings (debugging, etc.). |

---

## Notes

### Engine Log
On DLL attach (`DllMain`), the engine log is opened to `info/engine.log` and closed on detach.

### Video Player
- Uses Media Foundation (`mfplat.lib`, `mfuuid.lib`, `evr.lib`).
- Requires `_WIN32_WINNT >= 0x0601` (Windows 7+). See `ioVideoPlayer.h` for implementation details.
- `ioCinematicPlayer` is an extended class of `ioVideoPlayer` for fullscreen cinematic (cutscene) use.

### TinyXML
Already migrated to **TinyXML v2** (`tinyxml2.h` / `tinyxml2.lib`). Legacy `tinyxml.h` files are retained for backward compatibility with older code.

---

## ⚠️ Known Code Issues (Pending Fix)

### For-Loop Variable Scope (`int i;` at function scope)

Many source files declare loop variables at **function scope** using the old C-style pattern:

```cpp
// ❌ Current (bad practice — leaks `i` beyond the loop)
int i;
for (i = 0; i < count; i++)
{
    ...
}
```

This should be refactored to declare the variable **inside the for-loop** for proper scoping:

```cpp
// ✅ Desired (scoped to the loop)
for (int i = 0; i < count; i++)
{
    ...
}
```

**Affected engine files** (excluding third-party `Bullet/`):

| File | Lines |
|------|-------|
| `ioCurveGenerator.cpp` | 126 |
| `ioDecalMaker.cpp` | 173 |
| `ioDivisionMesh.cpp` | 19 |
| `ioEdit.cpp` | 307 |
| `ioGlowPostProcess.cpp` | 295 |
| `ioMath.cpp` | 955 |
| `ioMeshManager.cpp` | 307, 456 |
| `ioProgressiveMesh.cpp` | 585 |
| `ioRenderSystem.cpp` | 310, 705 |
| `ioRopeSpringCurve.cpp` | 56 |
| `ioSkeleton.cpp` | 253 |
| `ioSound.cpp` | 198 |
| `ioTextureUnitState.cpp` | 117 |

This is a **high-priority cleanup task** — fix before further refactoring to avoid scope-related bugs and improve code clarity.
