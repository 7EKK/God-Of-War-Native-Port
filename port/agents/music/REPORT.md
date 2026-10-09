# Music restarting every ~6 s — root cause (agent report, applied 2026-10-09)
989snd (989NOMID.IRX routine 0x13258) streams music by polling each stream voice's NAX via
sceSdGetAddr(0x2240|voice<<1|core) and checks it lies in ring half 0 [start, half1) or half 1
[half1, half1+size/2). Outside both while playing → stream killed (error 0x35, 0x13418..0x13438 → 0x2028),
and the game restarts the music. Our Spu2::decodeBlock set v.nax = block+8 at decode time and read16 returned
it, so during the last block of half 1 NAX pointed past the ring (~0.6 ms per ring cycle, deterministic →
regular ~6 s period). PCSX2 keeps NextA in [block+1, block+7] (Mixer.cpp DecodeSamples, spu2sys.cpp Start,
RegTable.cpp NAX = NextA). Fix (fix.diff): VoiceState playBlock/playStarted; read16 returns naxRegister()
= PCSX2 range; playback unchanged. Ruled out: ENDX (not read by the poller), LSA (driver rewrites LSAX).
Disassemblies: work/. Verified: port/music2.pcm max self-correlation 1-14 s lag 0.23-0.32 (was 0.43-0.54).
