// GOW-Port: render-target GS renderer ("native GS"). Instead of emulating the GS's VRAM byte for byte, every draw is rasterised by
// the GPU into scaled RGBA render targets (one per frame buffer base), textures are decoded once from a shadow VRAM and cached,
// and the CRTC output is composed from the render target at full output resolution. Transfers, CLUT and the VRAM model are
// delegated to a GSCpuBackend instance. Selected with PS2X_GS_HW=1; the VRAM-faithful backends stay available as oracle.
#pragma once

#include "runtime/gs/gs_backend.h"

#include <memory>

class GSHwBackend final : public GSRasterBackend
{
public:
    GSHwBackend();
    ~GSHwBackend() override;

    void Initialize(uint8_t *vram, uint32_t vramSize) override;
    void Reset() override;

    void Submit(const GSPrimitiveBatch &batch) override;
    void LoadClut(const GSTex0Reg &tex0, const GSTexClutReg &texclut) override;
    void BeginTransfer(const GSTransferCommand &command) override;
    void UploadImage(const uint8_t *data, uint32_t sizeBytes) override;

    void Flush() override;
    void TextureFlush() override;
    void Sync(GSSyncReason reason) override;
    PresentationFrame Present(const GSPresentationRequest &request) override;

    bool ClearFramebuffer(const GSContext &context, uint32_t rgba) override;
    uint32_t ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes) override;

    uint32_t ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const override;
    void WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value) override;
    void SnapshotVram(std::vector<uint8_t> &out) const override;
    GSTransferSnapshot GetTransferSnapshot() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
