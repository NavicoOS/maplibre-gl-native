#include <mbgl/test/util.hpp>
#include <mbgl/test/stub_file_source.hpp>
#include <mbgl/test/fixture_log_observer.hpp>

#include <mbgl/style/style_impl.hpp>
#include <mbgl/style/source_impl.hpp>
#include <mbgl/style/sources/vector_source.hpp>
#include <mbgl/style/layer.hpp>
#include <mbgl/style/layers/line_layer.hpp>
#include <mbgl/util/io.hpp>
#include <mbgl/util/run_loop.hpp>

#include <memory>

#ifdef WIN32
#include <Windows.h>
#endif

using namespace mbgl;
using namespace mbgl::style;

TEST(Style, Properties) {
    util::RunLoop loop;

    auto fileSource = std::make_shared<StubFileSource>();
    Style::Impl style { fileSource, 1.0 };

    style.loadJSON(R"STYLE({"name": "Test"})STYLE");
    ASSERT_EQ("Test", style.getName());

    style.loadJSON(R"STYLE({"center": [10, 20]})STYLE");
    ASSERT_EQ("", style.getName());
    ASSERT_EQ((LatLng{20, 10}), *style.getDefaultCamera().center);

    style.loadJSON(R"STYLE({"bearing": 24})STYLE");
    ASSERT_EQ("", style.getName());
    ASSERT_EQ(LatLng {}, *style.getDefaultCamera().center);
    ASSERT_EQ(24, *style.getDefaultCamera().bearing);

    style.loadJSON(R"STYLE({"zoom": 13.3})STYLE");
    ASSERT_EQ("", style.getName());
    ASSERT_EQ(13.3, *style.getDefaultCamera().zoom);

    style.loadJSON(R"STYLE({"pitch": 60})STYLE");
    ASSERT_EQ("", style.getName());
    ASSERT_EQ(60, *style.getDefaultCamera().pitch);

    style.loadJSON(R"STYLE({})STYLE");
    ASSERT_EQ(Milliseconds(300), *style.getTransitionOptions().duration);
    ASSERT_EQ(optional<Duration> {}, style.getTransitionOptions().delay);

    style.loadJSON(R"STYLE({"transition": { "duration": 500, "delay": 50 }})STYLE");
    ASSERT_EQ(Milliseconds(500), *style.getTransitionOptions().duration);
    ASSERT_EQ(Milliseconds(50), *style.getTransitionOptions().delay);

    style.loadJSON(R"STYLE({"name": 23, "center": {}, "bearing": "north", "zoom": null, "pitch": "wide"})STYLE");
    ASSERT_EQ("", style.getName());
    ASSERT_EQ(LatLng {}, *style.getDefaultCamera().center);
    ASSERT_EQ(0, *style.getDefaultCamera().zoom);
    ASSERT_EQ(0, *style.getDefaultCamera().bearing);
    ASSERT_EQ(0, *style.getDefaultCamera().pitch);
}

TEST(Style, DuplicateSource) {
    util::RunLoop loop;

    auto fileSource = std::make_shared<StubFileSource>();
    Style::Impl style { fileSource, 1.0 };

    style.loadJSON(util::read_file("test/fixtures/resources/style-unused-sources.json"));

    style.addSource(std::make_unique<VectorSource>("sourceId", "mptiler://tiles/contours"));
    
    try {
        style.addSource(std::make_unique<VectorSource>("sourceId", "mptiler://tiles/contours"));
        FAIL() << "Should not have been allowed to add a duplicate source id";
    } catch (const std::runtime_error&) {
        // Expected
    }
}

TEST(Style, RemoveSourceInUse) {
    util::RunLoop loop;

    auto log = new FixtureLogObserver();
    Log::setObserver(std::unique_ptr<Log::Observer>(log));

    auto fileSource = std::make_shared<StubFileSource>();
    Style::Impl style { fileSource, 1.0 };

    style.loadJSON(util::read_file("test/fixtures/resources/style-unused-sources.json"));

    style.addSource(std::make_unique<VectorSource>("sourceId", "mptiler://tiles/contours"));
    style.addLayer(std::make_unique<LineLayer>("layerId", "sourceId"));

    // Should not remove the source
    auto removed = style.removeSource("sourceId");
    ASSERT_EQ(nullptr, removed);
    ASSERT_NE(nullptr, style.getSource("sourceId"));

    const FixtureLogObserver::LogMessage logMessage {
            EventSeverity::Warning,
            Event::General,
            int64_t(-1),
            "Source 'sourceId' is in use, cannot remove",
    };

#if defined(WIN32)
    Sleep(1000);
#endif

    EXPECT_EQ(log->count(logMessage), 1u);
}

TEST(Style, SourceImplsOrder) {
    util::RunLoop loop;
    auto fileSource = std::make_shared<StubFileSource>();
    Style::Impl style{fileSource, 1.0};

    style.addSource(std::make_unique<VectorSource>("c", "mptiler://tiles/contours"));
    style.addSource(std::make_unique<VectorSource>("b", "mptiler://tiles/contours"));
    style.addSource(std::make_unique<VectorSource>("a", "mptiler://tiles/contours"));

    auto sources = style.getSources();
    ASSERT_EQ(3u, sources.size());
    EXPECT_EQ("c", sources[0]->getID());
    EXPECT_EQ("b", sources[1]->getID());
    EXPECT_EQ("a", sources[2]->getID());

    const auto& sourceImpls = *style.getSourceImpls();
    ASSERT_EQ(3u, sourceImpls.size());
    EXPECT_EQ("a", sourceImpls[0]->id);
    EXPECT_EQ("b", sourceImpls[1]->id);
    EXPECT_EQ("c", sourceImpls[2]->id);
}

TEST(Style, AddRemoveImage) {
    util::RunLoop loop;
    auto fileSource = std::make_shared<StubFileSource>();
    Style::Impl style{fileSource, 1.0};
    style.addImage(std::make_unique<style::Image>("one", PremultipliedImage({16, 16}), 2));
    style.addImage(std::make_unique<style::Image>("two", PremultipliedImage({16, 16}), 2));
    style.addImage(std::make_unique<style::Image>("three", PremultipliedImage({16, 16}), 2));

    style.removeImage("one");
    style.removeImage("two");

    EXPECT_TRUE(!!style.getImage("three"));
    EXPECT_FALSE(!!style.getImage("two"));
    EXPECT_FALSE(!!style.getImage("four"));
}

TEST(Style, AddRemoveRemoveImage) {
    // regression test for https://github.com/mapbox/mapbox-gl-native/pull/16391
    util::RunLoop loop;
    auto fileSource = std::make_shared<StubFileSource>();
    Style::Impl style{fileSource, 1.0};
    style.addImage(std::make_unique<style::Image>("one", PremultipliedImage({16, 16}), 2));
    style.addImage(std::make_unique<style::Image>("two", PremultipliedImage({16, 16}), 2));
    style.addImage(std::make_unique<style::Image>("three", PremultipliedImage({16, 16}), 2));

    style.removeImage("one");
    style.removeImage("two");
    style.removeImage("two");

    EXPECT_TRUE(!!style.getImage("three"));
    EXPECT_FALSE(!!style.getImage("two"));
    EXPECT_FALSE(!!style.getImage("four"));
}
