# Task: summarize the new performance docs in the KIexster update

Read (local files only):
- ../ref/other-recomp-new/god-of-war-recomp-main/docs/RENDER_PRESENTACION_COMPARTIDA.md
- ../ref/other-recomp-new/god-of-war-recomp-main/docs/RENDER_UPLOAD_RING.md
- ../ref/other-recomp-new/god-of-war-recomp-main/docs/RENDIMIENTO_PRESENTACION.md
- any other new file in ../ref/other-recomp-new/god-of-war-recomp-main/docs/ not present in ../ref/other-recomp/god-of-war-recomp-main/docs/
- the diff of RENDIMIENTO.md and ESTADO.md between those two folders (compare the two files' ends)
They are in Spanish. Answer in English:
1. For each of the 8 new patches (gs-upload-ring, vu1-zero-factor, vu1-direct-store, vu1-cold-scalar,
   vu1-simd-zero, vu1-xgkick-flush, gs-shared-host, vu1-generator-dir): what it does, measured gain, any
   runtime env var / switch it adds or needs (exact names), any default that must be set to benefit.
2. Their latest measured FPS in loaded gameplay and the configuration used (renderer, env vars, VU1 compiled).
3. What they say the next bottleneck is.
Cite file + section for each fact. Mark anything you could not confirm as **unverified**.
