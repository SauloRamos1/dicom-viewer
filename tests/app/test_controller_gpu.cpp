#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>

#include "AppTestSupport.h"
#include "DicomController.h"
#include "ErrorText.h"
#include "GpuTexture.h"
#include "TestSupport.h"
#include "Windowing.h"

using namespace dv::test;

namespace {

void openAndWait(DicomController &controller, const std::filesystem::path &path)
{
    controller.open(urlOf(path));
    REQUIRE(controller.busy());
    REQUIRE(pumpUntil([&] { return !controller.busy(); }));
}

// Counts the emissions of one signal (QSignalSpy would need Qt Test).
class Emissions
{
public:
    template<typename Object, typename Signal>
    Emissions(Object *sender, Signal signal)
    {
        QObject::connect(sender, signal, sender, [this] { ++m_count; });
    }
    int count() const { return m_count; }

private:
    int m_count = 0;
};

QString errorTitle(const DicomController &controller, int row = 0)
{
    return controller.errors()->data(controller.errors()->index(row), ErrorModel::TitleRole).toString();
}

QString errorSeverity(const DicomController &controller, int row = 0)
{
    return controller.errors()->data(controller.errors()->index(row), ErrorModel::SeverityRole).toString();
}

// What the packed texture of `frame` must contain, as a QImage.
QImage expectedPacked(const std::filesystem::path &path, int frame)
{
    const auto file = dv::DicomFile::load(path);
    REQUIRE(file.has_value());
    const auto packed = dv::packFrame(*file, frame);
    REQUIRE(packed.has_value());
    QImage image(packed->width, packed->height, QImage::Format_RGBA8888);
    for (int y = 0; y < packed->height; ++y)
        std::memcpy(image.scanLine(y), packed->rgba.data() + static_cast<size_t>(y) * packed->width * 4,
                    static_cast<size_t>(packed->width) * 4);
    return image;
}

QImage expectedCpu(const std::filesystem::path &path, int frame, const dv::RenderParams &params)
{
    const auto file = dv::DicomFile::load(path);
    REQUIRE(file.has_value());
    const auto rendered = dv::renderFrame(*file, frame, params);
    REQUIRE(rendered.has_value());
    return toQImage(*rendered);
}

// Two 4x3 frames of an 8/16-bit image: frame 0 counts up from 0, frame 1 from 1000.
std::filesystem::path makeStack(const TempDir &dir, int bits = 16)
{
    dv::test::MonoImageSpec spec{.rows = 3, .columns = 4, .frames = 2, .bitsAllocated = bits, .bitsStored = bits};
    for (int f = 0; f < 2; ++f)
        for (int i = 0; i < 12; ++i)
            spec.samples.push_back(static_cast<std::uint16_t>(bits == 8 ? f * 100 + i : f * 1000 + i));
    writeMonoImage(dir / "stack.dcm", spec);
    return dir / "stack.dcm";
}

} // namespace

TEST_CASE("The render backend starts on CPU with the GPU assumed available", "[app][gpu]")
{
    DicomController controller;
    CHECK(controller.renderBackend() == DicomController::Cpu);
    CHECK(controller.gpuAvailable());
    CHECK_FALSE(controller.gpuActive());
    CHECK_FALSE(controller.gpuCapable());
    CHECK(controller.averageRenderMs() == 0);
    CHECK(controller.gpuImageRevision() == 0);
    CHECK(controller.gpuFrameStore()->get().isNull());

    Emissions changed(&controller, &DicomController::renderBackendChanged);
    controller.setRenderBackend(DicomController::Gpu); // allowed with no file open
    CHECK(controller.renderBackend() == DicomController::Gpu);
    CHECK(changed.count() == 1);
    controller.setRenderBackend(DicomController::Gpu);
    CHECK(changed.count() == 1);
}

TEST_CASE("On the GPU backend the first frame is uploaded packed and not rendered on the CPU", "[app][gpu]")
{
    DicomController controller;
    controller.setRenderBackend(DicomController::Gpu);
    const auto path = dataFile("CT_small.dcm");
    openAndWait(controller, path);

    REQUIRE(controller.hasImage());
    CHECK(controller.gpuCapable());
    CHECK(controller.gpuActive());
    CHECK(controller.gpuImageRevision() > 0);
    CHECK(controller.frameStore()->get().isNull()); // no CPU image yet

    const QImage packed = controller.gpuFrameStore()->get();
    CHECK(packed.format() == QImage::Format_RGBA8888);
    CHECK(packed == expectedPacked(path, 0));

    const auto file = dv::DicomFile::load(path);
    const auto params = dv::gpuParams(file->info());
    CHECK(controller.rescaleSlope() == params.slope);
    CHECK(controller.rescaleIntercept() == params.intercept);
    CHECK(controller.storedMin() == params.storedMin);
    CHECK(controller.monochrome1() == params.monochrome1);
    CHECK(controller.windowCenter() == file->info().defaultVoi.value_or(*dv::presetVoi(dv::WindowPreset::MinMax, *file)).center);
}

TEST_CASE("On the GPU backend W/L, invert and colormap only change uniforms", "[app][gpu]")
{
    DicomController controller;
    controller.setRenderBackend(DicomController::Gpu);
    openAndWait(controller, dataFile("CT_small.dcm"));
    const int gpuRevision = controller.gpuImageRevision();
    const int cpuRevision = controller.imageRevision();
    const QImage packed = controller.gpuFrameStore()->get();

    controller.setWindowCenter(-100);
    controller.setWindowWidth(500);
    controller.setInvert(true);
    controller.setColormap(DicomController::Jet);
    pumpFor(150);

    CHECK(controller.windowCenter() == -100);
    CHECK(controller.windowWidth() == 500);
    CHECK(controller.invert());
    CHECK(controller.colormap() == DicomController::Jet);
    CHECK(controller.gpuImageRevision() == gpuRevision);
    CHECK(controller.imageRevision() == cpuRevision);
    CHECK(controller.gpuFrameStore()->get() == packed); // no new upload
    CHECK(controller.frameStore()->get().isNull());
    CHECK(controller.gpuActive());
}

TEST_CASE("On the GPU backend a frame change uploads the new frame", "[app][gpu]")
{
    const TempDir dir;
    const auto path = makeStack(dir);
    DicomController controller;
    controller.setRenderBackend(DicomController::Gpu);
    openAndWait(controller, path);
    REQUIRE(controller.frameCount() == 2);
    REQUIRE(controller.gpuFrameStore()->get() == expectedPacked(path, 0));

    const int revision = controller.gpuImageRevision();
    controller.setFrame(1);
    REQUIRE(pumpUntil([&] { return controller.gpuImageRevision() != revision; }));
    CHECK(controller.gpuFrameStore()->get() == expectedPacked(path, 1));
    CHECK(controller.gpuActive());
}

TEST_CASE("Switching the backend keeps the displayed window state", "[app][gpu]")
{
    const auto path = dataFile("CT_small.dcm");
    DicomController controller;
    openAndWait(controller, path);
    controller.setWindowCenter(-100);
    controller.setWindowWidth(500);
    controller.setInvert(true);
    controller.setColormap(DicomController::Hot);
    const dv::RenderParams params{.voi = {-100, 500}, .invert = true, .colormap = dv::Colormap::Hot};
    REQUIRE(pumpUntil([&] { return controller.frameStore()->get() == expectedCpu(path, 0, params); }));
    REQUIRE_FALSE(controller.gpuActive());

    SECTION("CPU to GPU")
    {
        Emissions active(&controller, &DicomController::gpuActiveChanged);
        controller.setRenderBackend(DicomController::Gpu);
        CHECK_FALSE(controller.gpuActive()); // the CPU frame stays up until the upload is ready
        REQUIRE(pumpUntil([&] { return controller.gpuActive(); }));
        CHECK(active.count() == 1);
        CHECK(controller.gpuFrameStore()->get() == expectedPacked(path, 0));
    }

    SECTION("GPU to CPU")
    {
        controller.setRenderBackend(DicomController::Gpu);
        REQUIRE(pumpUntil([&] { return controller.gpuActive(); }));
        // Changed while on the GPU: only the uniforms move.
        controller.setWindowCenter(0);
        controller.setWindowWidth(1000);
        controller.setColormap(DicomController::Bone);
        controller.setInvert(false);
        const dv::RenderParams next{.voi = {0, 1000}, .invert = false, .colormap = dv::Colormap::Bone};

        controller.setRenderBackend(DicomController::Cpu);
        CHECK(controller.gpuActive()); // the GPU frame stays up until the CPU frame is ready
        REQUIRE(pumpUntil([&] { return !controller.gpuActive(); }));
        CHECK(controller.frameStore()->get() == expectedCpu(path, 0, next));
        CHECK(controller.renderBackend() == DicomController::Cpu);
        CHECK(controller.windowCenter() == 0);
        CHECK(controller.windowWidth() == 1000);
        CHECK(controller.colormap() == DicomController::Bone);
        CHECK_FALSE(controller.invert());
    }

    CHECK(controller.hasImage());
}

TEST_CASE("Colour images stay on the CPU even when the GPU backend is chosen", "[app][gpu]")
{
    DicomController controller;
    controller.setRenderBackend(DicomController::Gpu);
    openAndWait(controller, dataFile("SC_rgb_small_odd.dcm"));

    REQUIRE(controller.hasImage());
    CHECK(controller.renderBackend() == DicomController::Gpu); // the choice is kept
    CHECK_FALSE(controller.gpuCapable());
    CHECK_FALSE(controller.gpuActive());
    CHECK_FALSE(controller.frameStore()->get().isNull());
    CHECK(controller.gpuFrameStore()->get().isNull());
    CHECK(controller.errors()->count() == 0);

    // A GPU capable file afterwards goes back to the shader.
    openAndWait(controller, dataFile("MR_small.dcm"));
    CHECK(controller.gpuCapable());
    CHECK(controller.gpuActive());
}

TEST_CASE("8-bit images take the same packed path", "[app][gpu]")
{
    const TempDir dir;
    const auto path = makeStack(dir, 8);
    DicomController controller;
    controller.setRenderBackend(DicomController::Gpu);
    openAndWait(controller, path);
    CHECK(controller.gpuCapable());
    CHECK(controller.gpuActive());
    const QImage packed = controller.gpuFrameStore()->get();
    REQUIRE(packed.size() == QSize(4, 3));
    CHECK(packed.pixelColor(3, 0) == QColor(0, 3, 0, 255)); // high byte 0, low byte 3
    CHECK(packed == expectedPacked(path, 0));
}

TEST_CASE("Signed 16-bit and MONOCHROME1 files report the shader constants", "[app][gpu]")
{
    const TempDir dir;
    writeMonoImage(dir / "m1.dcm", {.rows = 1, .columns = 3, .pixelRepresentation = 1, .slope = 2.0, .intercept = 10.0,
                                    .photometric = "MONOCHROME1",
                                    .samples = {0, 1, static_cast<std::uint16_t>(-5)}});
    DicomController controller;
    openAndWait(controller, dir / "m1.dcm");
    CHECK(controller.monochrome1());
    CHECK(controller.storedMin() == -32768.0);
    CHECK(controller.rescaleSlope() == 2.0);
    CHECK(controller.rescaleIntercept() == 10.0);
}

TEST_CASE("An unavailable GPU forces the CPU backend and logs one info entry", "[app][gpu]")
{
    const auto path = dataFile("CT_small.dcm");
    DicomController controller;

    SECTION("before a file is open")
    {
        controller.setRenderBackend(DicomController::Gpu);
        Emissions available(&controller, &DicomController::gpuAvailableChanged);
        controller.reportGpuUnavailable(QStringLiteral("software scene graph"));

        CHECK_FALSE(controller.gpuAvailable());
        CHECK(available.count() == 1);
        CHECK(controller.renderBackend() == DicomController::Cpu);
        REQUIRE(controller.errors()->count() == 1); // the sample errors are replaced
        CHECK(errorTitle(controller) == ErrorText::message(dv::ErrorCode::GpuUnavailable));
        CHECK(errorSeverity(controller) == QStringLiteral("info"));
        CHECK(controller.errors()->data(controller.errors()->index(0), ErrorModel::DetailRole).toString()
              == QStringLiteral("software scene graph"));

        controller.reportGpuUnavailable(QStringLiteral("again")); // one-time
        CHECK(controller.errors()->count() == 1);
        CHECK(available.count() == 1);

        controller.setRenderBackend(DicomController::Gpu); // refused
        CHECK(controller.renderBackend() == DicomController::Cpu);
        openAndWait(controller, path);
        CHECK_FALSE(controller.gpuActive());
        CHECK_FALSE(controller.frameStore()->get().isNull());
    }

    SECTION("while the GPU is showing the image")
    {
        controller.setRenderBackend(DicomController::Gpu);
        openAndWait(controller, path);
        REQUIRE(controller.gpuActive());
        controller.setWindowWidth(300);

        controller.reportGpuUnavailable(QStringLiteral("shader failed"));
        CHECK(controller.renderBackend() == DicomController::Cpu);
        REQUIRE(pumpUntil([&] { return !controller.gpuActive(); }));
        CHECK(controller.frameStore()->get()
              == expectedCpu(path, 0, {.voi = {controller.windowCenter(), 300}}));
        CHECK(controller.windowWidth() == 300);
        CHECK(controller.errors()->count() == 1); // the info entry survives
    }
}

TEST_CASE("CPU render times feed a rolling average of the last 10", "[app][gpu][timing]")
{
    DicomController controller;
    openAndWait(controller, dataFile("CT_small.dcm"));
    CHECK(controller.lastRenderBackend() == DicomController::Cpu);
    std::vector<double> samples{controller.lastRenderMs()};
    CHECK(controller.averageRenderMs() == Catch::Approx(samples.back()));

    for (int i = 0; i < 12; ++i) {
        const int revision = controller.imageRevision();
        controller.setWindowCenter(-200.0 + i * 10);
        REQUIRE(pumpUntil([&] { return controller.imageRevision() != revision; }));
        samples.push_back(controller.lastRenderMs());
    }
    const auto from = samples.end() - 10;
    CHECK(controller.averageRenderMs() == Catch::Approx(std::accumulate(from, samples.end(), 0.0) / 10.0));
    CHECK(controller.lastRenderBackend() == DicomController::Cpu);
}

TEST_CASE("GPU render times run from the change to the next frame swap", "[app][gpu][timing]")
{
    DicomController controller;
    controller.setRenderBackend(DicomController::Gpu);
    openAndWait(controller, dataFile("CT_small.dcm"));

    controller.reportFrameSwapped(); // the upload shows up
    REQUIRE(controller.lastRenderMs() > 0);
    CHECK(controller.lastRenderBackend() == DicomController::Gpu);
    const double upload = controller.lastRenderMs();
    CHECK(controller.averageRenderMs() == Catch::Approx(upload));

    Emissions timing(&controller, &DicomController::lastRenderMsChanged);
    controller.reportFrameSwapped(); // nothing pending: no sample
    CHECK(timing.count() == 0);

    controller.setWindowCenter(0);
    pumpFor(5);
    CHECK(timing.count() == 0); // not measured before the swap
    controller.reportFrameSwapped();
    CHECK(timing.count() == 1);
    CHECK(controller.lastRenderMs() >= 5.0);
    CHECK(controller.averageRenderMs() == Catch::Approx((upload + controller.lastRenderMs()) / 2.0));

    SECTION("switching backend starts a new average")
    {
        controller.setRenderBackend(DicomController::Cpu);
        REQUIRE(pumpUntil([&] { return !controller.gpuActive(); }));
        CHECK(controller.lastRenderBackend() == DicomController::Cpu);
        CHECK(controller.averageRenderMs() == Catch::Approx(controller.lastRenderMs()));
    }
}
