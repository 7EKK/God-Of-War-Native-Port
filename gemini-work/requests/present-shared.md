Use read_file only (no shell commands). Read-only analysis; do not edit anything.

Goal: understand the existing "shared presentation" path (GS renderer thread GL context -> raylib host window, zero-copy) in this
runtime, so the native GSHwBackend can reuse it instead of glReadPixels.

Files (worktree ../ext/PS2Recomp-kl-present/ps2xRuntime):
- src/lib/gs/gpu/gs_gl.cpp, src/lib/gs/gpu/gs_gl.h (namespace GSSharedPresent, PublishShared, context creation / sharing with the host)
- src/lib/gs/gs_host_present.cpp, include/runtime/gs/gs_host_present.h
- src/lib/gs/gs_threaded_backend.cpp, include/runtime/gs/gs_threaded_backend.h (m_sharedPresentation)
- src/lib/gs/gs_frontend.cpp (around lines 740-830: usesSharedPresentation, recordAcquiredSharedFrame)
- whichever file draws the window (grep-free: look in src/ for the raylib frontend that calls GSHostPresentation::renderFrame,
  probably src/lib/ps2_runtime*.cpp or src/runner/main*.cpp not generated; name it).

Answer, with file:line for every claim:
1. How the GS GL context is created and whether it shares objects with the raylib/GLFW host context (wglShareLists? created from
   the host's HGLRC captured by CaptureHostContext?). Under which condition sharing is enabled/disabled (env vars?).
2. PublishShared: what `buffer` is (a PBO? a texture?), how the frame reaches the host (fences, copies, which thread does what),
   and Acquire() on the host side.
3. How GSThreadedBackend decides m_sharedPresentation, and how the host (GSHostPresentation::prepare/renderFrame) chooses shared vs
   pixel upload, and how it draws the texture (filter, shader — is there a bicubic upscale and where is it?).
4. Where screenshot/zone captures (GOW_PAD_CAPTURAS, any "capture" in host present) read frame pixels.
Keep the answer under 80 lines. Mark anything unverified.
