#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "ldde/settings/settings_types.hpp"
#include "ldde/settings/settings_value.hpp"
#include "ldde/settings/settings_schema.hpp"
#include "ldde/settings/settings_store.hpp"
#include "ldde/settings/settings_search.hpp"
#include "ldde/settings/settings_navigation.hpp"
#include "ldde/settings/settings_layout.hpp"
#include "ldde/settings/settings_controller.hpp"
#include "ldde/settings/settings_view.hpp"
#include "ldde/display/display_info.hpp"
#include "ldde/display/display_policy.hpp"
#include "ldde/config/config.hpp"
#include "ldde/shell/shm_buffer.hpp"
#include "ldde/shell/theme.hpp"
#include "ldde/shell/design_tokens.hpp"

using namespace ldde;
using namespace ldde::settings;

namespace {

display::DisplayPolicy make_test_policy(int32_t width = 720, int32_t height = 1280) {
    display::DisplayInfo info;
    info.id = 1;
    info.name = "WL-1";
    info.width = width;
    info.height = height;
    info.pixel_width = width;
    info.pixel_height = height;
    info.logical_width = width;
    info.logical_height = height;
    info.geometry = core::Rect{0, 0, width, height};
    info.safe_insets = display::SafeInsets{32, 48, 0, 0};
    return display::DisplayPolicy(info);
}

} // namespace

// ============================================================================
// 1. Settings Types & Conversions Tests
// ============================================================================

TEST(SettingsTypesTest, CategoryConversions) {
    EXPECT_EQ(category_name(SettingsCategory::Appearance), "Appearance");
    EXPECT_EQ(category_name(SettingsCategory::Display), "Display");
    EXPECT_EQ(category_name(SettingsCategory::Windows), "Windows");
    EXPECT_EQ(category_name(SettingsCategory::Desktop), "Desktop");
    EXPECT_EQ(category_name(SettingsCategory::Dock), "Dock");
    EXPECT_EQ(category_name(SettingsCategory::Launcher), "Launcher");
    EXPECT_EQ(category_name(SettingsCategory::Input), "Input");
    EXPECT_EQ(category_name(SettingsCategory::Notifications), "Notifications");
    EXPECT_EQ(category_name(SettingsCategory::SystemUI), "System UI");
    EXPECT_EQ(category_name(SettingsCategory::About), "About");

    EXPECT_EQ(parse_category("Appearance"), SettingsCategory::Appearance);
    EXPECT_EQ(parse_category("display"), SettingsCategory::Display);
    EXPECT_EQ(parse_category("System UI"), SettingsCategory::SystemUI);
    EXPECT_EQ(parse_category("system_ui"), SettingsCategory::SystemUI);
    EXPECT_EQ(parse_category("about"), SettingsCategory::About);
    EXPECT_EQ(parse_category("unknown_cat"), std::nullopt);
}

TEST(SettingsTypesTest, SettingTypeConversions) {
    EXPECT_EQ(setting_type_name(SettingType::Bool), "Bool");
    EXPECT_EQ(setting_type_name(SettingType::Int), "Int");
    EXPECT_EQ(setting_type_name(SettingType::Double), "Double");
    EXPECT_EQ(setting_type_name(SettingType::String), "String");
    EXPECT_EQ(setting_type_name(SettingType::Enum), "Enum");

    EXPECT_EQ(SettingType::Bool, SettingType::Bool);
    EXPECT_EQ(SettingType::Int, SettingType::Int);
    EXPECT_EQ(SettingType::Double, SettingType::Double);
}

TEST(SettingsTypesTest, ApplyModeConversions) {
    EXPECT_EQ(apply_mode_name(ApplyMode::Immediate), "Immediate");
    EXPECT_EQ(apply_mode_name(ApplyMode::SessionLevel), "Session-Level");
    EXPECT_EQ(apply_mode_name(ApplyMode::RestartRequired), "Restart Required");
}

TEST(SettingsTypesTest, WindowModeConversions) {
    EXPECT_EQ(window_mode_name(SettingsWindowMode::Closed), "Closed");
    EXPECT_EQ(window_mode_name(SettingsWindowMode::Normal), "Normal");
    EXPECT_EQ(window_mode_name(SettingsWindowMode::Maximized), "Maximized");
    EXPECT_EQ(window_mode_name(SettingsWindowMode::Minimized), "Minimized");
}

// ============================================================================
// 2. SettingsValue Variant & Serialization Tests
// ============================================================================

TEST(SettingsValueTest, ValueVariantsAndAccessors) {
    SettingsValue val_bool(true);
    EXPECT_EQ(val_bool.type(), SettingType::Bool);
    EXPECT_TRUE(val_bool.as_bool().value());
    EXPECT_EQ(val_bool.to_string(), "true");

    SettingsValue val_int(static_cast<int64_t>(42));
    EXPECT_EQ(val_int.type(), SettingType::Int);
    EXPECT_EQ(val_int.as_int().value(), 42);
    EXPECT_EQ(val_int.to_string(), "42");

    SettingsValue val_real(3.1415);
    EXPECT_EQ(val_real.type(), SettingType::Double);
    EXPECT_NEAR(val_real.as_double().value(), 3.1415, 0.0001);

    SettingsValue val_str("dark");
    EXPECT_EQ(val_str.type(), SettingType::String);
    EXPECT_EQ(val_str.as_string().value(), "dark");
    EXPECT_EQ(val_str.to_string(), "dark");
}

TEST(SettingsValueTest, DeserializationFromString) {
    auto b_true = SettingsValue::from_string(SettingType::Bool, "true");
    ASSERT_TRUE(b_true.has_value());
    EXPECT_TRUE(b_true->as_bool().value());

    auto b_false = SettingsValue::from_string(SettingType::Bool, "0");
    ASSERT_TRUE(b_false.has_value());
    EXPECT_FALSE(b_false->as_bool().value());

    auto i_val = SettingsValue::from_string(SettingType::Int, "-128");
    ASSERT_TRUE(i_val.has_value());
    EXPECT_EQ(i_val->as_int().value(), -128);

    auto r_val = SettingsValue::from_string(SettingType::Double, "1.75");
    ASSERT_TRUE(r_val.has_value());
    EXPECT_NEAR(r_val->as_double().value(), 1.75, 0.001);

    auto bad_int = SettingsValue::from_string(SettingType::Int, "not_an_int");
    EXPECT_FALSE(bad_int.has_value());

    auto bad_real = SettingsValue::from_string(SettingType::Double, "not_a_real");
    EXPECT_FALSE(bad_real.has_value());
}

TEST(SettingsValueTest, EqualityComparison) {
    SettingsValue v1(static_cast<int64_t>(100));
    SettingsValue v2(static_cast<int64_t>(100));
    SettingsValue v3(static_cast<int64_t>(200));
    SettingsValue v4(true);

    EXPECT_EQ(v1, v2);
    EXPECT_NE(v1, v3);
    EXPECT_NE(v1, v4);
}

// ============================================================================
// 3. SettingsSchema & Validation Tests
// ============================================================================

TEST(SettingsSchemaTest, DefaultSchemaCompleteness) {
    auto schema = SettingsSchema::create_default_schema();

    // Verify all 10 categories are populated
    for (int cat_idx = 0; cat_idx <= static_cast<int>(SettingsCategory::About); ++cat_idx) {
        auto cat = static_cast<SettingsCategory>(cat_idx);
        auto settings = schema.settings_in_category(cat);
        EXPECT_FALSE(settings.empty()) << "Category " << category_name(cat) << " has no settings!";
    }

    // Lookup known settings
    const auto* theme_mode = schema.find("appearance.theme_mode");
    ASSERT_NE(theme_mode, nullptr);
    EXPECT_EQ(theme_mode->type, SettingType::Enum);
    EXPECT_EQ(theme_mode->category, SettingsCategory::Appearance);
    EXPECT_EQ(theme_mode->apply_mode, ApplyMode::Immediate);

    const auto* scale = schema.find("display.scale_factor");
    ASSERT_NE(scale, nullptr);
    EXPECT_EQ(scale->type, SettingType::Double);
    EXPECT_EQ(scale->category, SettingsCategory::Display);
    EXPECT_TRUE(scale->min_value.has_value());
    EXPECT_TRUE(scale->max_value.has_value());

    const auto* position = schema.find("dock.position");
    ASSERT_NE(position, nullptr);
    EXPECT_EQ(position->type, SettingType::Enum);
    EXPECT_FALSE(position->enum_values.empty());

    const auto* about_name = schema.find("about.system_name");
    ASSERT_NE(about_name, nullptr);
    EXPECT_EQ(about_name->category, SettingsCategory::About);
}

TEST(SettingsSchemaTest, ValidationRules) {
    SettingDefinition def_int;
    def_int.key = "test.int";
    def_int.type = SettingType::Int;
    def_int.min_value = 10.0;
    def_int.max_value = 50.0;

    EXPECT_TRUE(def_int.validate(SettingsValue(static_cast<int64_t>(30))).is_ok());
    EXPECT_TRUE(def_int.validate(SettingsValue(static_cast<int64_t>(10))).is_ok());
    EXPECT_TRUE(def_int.validate(SettingsValue(static_cast<int64_t>(50))).is_ok());
    EXPECT_TRUE(def_int.validate(SettingsValue(static_cast<int64_t>(5))).is_error());
    EXPECT_TRUE(def_int.validate(SettingsValue(static_cast<int64_t>(60))).is_error());
    EXPECT_TRUE(def_int.validate(SettingsValue(true)).is_error()); // Type mismatch

    SettingDefinition def_enum;
    def_enum.key = "test.enum";
    def_enum.type = SettingType::Enum;
    def_enum.enum_values = {"bottom", "top", "left", "right"};

    EXPECT_TRUE(def_enum.validate(SettingsValue("bottom")).is_ok());
    EXPECT_TRUE(def_enum.validate(SettingsValue("left")).is_ok());
    EXPECT_TRUE(def_enum.validate(SettingsValue("center")).is_error());
}

// ============================================================================
// 4. SettingsStore & Atomic Persistence Tests
// ============================================================================

TEST(SettingsStoreTest, GetAndSetValues) {
    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, "");

    // Default value before any write
    auto dock_en = store.get("dock.enabled");
    ASSERT_TRUE(dock_en.has_value());
    EXPECT_TRUE(dock_en->as_bool().value());

    // Set new value
    core::Status s = store.set("dock.enabled", SettingsValue(false), false);
    EXPECT_TRUE(s.is_ok());

    auto updated = store.get("dock.enabled");
    ASSERT_TRUE(updated.has_value());
    EXPECT_FALSE(updated->as_bool().value());

    // Out of bounds validation error
    s = store.set("display.scale_factor", SettingsValue(10.5), false);
    EXPECT_TRUE(s.is_error());
}

TEST(SettingsStoreTest, TransactionCommitAndRollback) {
    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, "");

    store.begin_transaction();
    EXPECT_TRUE(store.in_transaction());

    EXPECT_TRUE(store.set("dock.enabled", SettingsValue(false), false).is_ok());
    EXPECT_TRUE(store.set("dock.item_size", SettingsValue(static_cast<int64_t>(64)), false).is_ok());

    // Values read during transaction reflect staged values
    EXPECT_FALSE(store.get("dock.enabled")->as_bool().value());
    EXPECT_EQ(store.get("dock.item_size")->as_int().value(), 64);

    // Rollback
    store.rollback();
    EXPECT_FALSE(store.in_transaction());

    // Values reverted to original
    EXPECT_TRUE(store.get("dock.enabled")->as_bool().value());
    EXPECT_EQ(store.get("dock.item_size")->as_int().value(), 48);

    // Now test Commit
    store.begin_transaction();
    EXPECT_TRUE(store.set("dock.item_size", SettingsValue(static_cast<int64_t>(56)), false).is_ok());
    EXPECT_TRUE(store.commit(false).is_ok());
    EXPECT_FALSE(store.in_transaction());
    EXPECT_EQ(store.get("dock.item_size")->as_int().value(), 56);
}

TEST(SettingsStoreTest, ResetOperations) {
    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, "");

    EXPECT_TRUE(store.set("dock.enabled", SettingsValue(false), false).is_ok());
    EXPECT_FALSE(store.get("dock.enabled")->as_bool().value());

    // Reset single setting
    EXPECT_TRUE(store.reset_setting("dock.enabled", false).is_ok());
    EXPECT_TRUE(store.get("dock.enabled")->as_bool().value());

    // Modify multiple settings in Dock
    EXPECT_TRUE(store.set("dock.item_size", SettingsValue(static_cast<int64_t>(64)), false).is_ok());
    EXPECT_TRUE(store.set("dock.position", SettingsValue("top"), false).is_ok());

    // Reset Dock category
    EXPECT_TRUE(store.reset_category(SettingsCategory::Dock, false).is_ok());
    EXPECT_EQ(store.get("dock.item_size")->as_int().value(), 48);
    EXPECT_EQ(store.get("dock.position")->as_string().value(), "bottom");
}

TEST(SettingsStoreTest, AtomicPersistenceToDisk) {
    char temp_template[] = "/tmp/ldde_settings_test_XXXXXX";
    char* dir = mkdtemp(temp_template);
    ASSERT_NE(dir, nullptr);
    std::string config_file = std::string(dir) + "/desktop.conf";

    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, config_file);

    EXPECT_TRUE(store.set("dock.enabled", SettingsValue(false), true).is_ok());

    // Verify file exists on disk
    EXPECT_TRUE(std::filesystem::exists(config_file));

    // Reload in fresh Config
    config::Config reloaded;
    EXPECT_TRUE(reloaded.load_file(config_file).is_ok());
    EXPECT_FALSE(reloaded.get_bool_or("dock", "enabled", true));

    std::filesystem::remove_all(dir);
}

TEST(SettingsStoreTest, SettingChangeNotifications) {
    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, "");

    std::string notified_key;
    SettingsValue notified_val(false);

    store.on_setting_changed([&](const std::string& k, const SettingsValue& v) {
        notified_key = k;
        notified_val = v;
    });

    EXPECT_TRUE(store.set("dock.enabled", SettingsValue(false), false).is_ok());
    EXPECT_EQ(notified_key, "dock.enabled");
    EXPECT_FALSE(notified_val.as_bool().value());
}

// ============================================================================
// 5. SettingsSearch Tests
// ============================================================================

TEST(SettingsSearchTest, SearchRelevance) {
    auto schema = SettingsSchema::create_default_schema();
    SettingsSearch search(schema);

    // Exact title match
    auto res_scale = search.search("Display Scale");
    ASSERT_FALSE(res_scale.empty());
    EXPECT_EQ(res_scale[0]->key, "display.scale_factor");

    // Keyword match
    auto res_theme = search.search("theme");
    ASSERT_FALSE(res_theme.empty());
    bool found_theme = false;
    for (const auto* def : res_theme) {
        if (def->key == "appearance.theme_mode") found_theme = true;
    }
    EXPECT_TRUE(found_theme);

    // Category match
    auto res_dock = search.search("dock");
    ASSERT_FALSE(res_dock.empty());
    for (const auto* def : res_dock) {
        EXPECT_EQ(def->category, SettingsCategory::Dock);
    }

    // Empty query returns empty
    EXPECT_TRUE(search.search("").empty());
    EXPECT_TRUE(search.search("   ").empty());
}

// ============================================================================
// 6. SettingsNavigation Tests
// ============================================================================

TEST(SettingsNavigationTest, NavigationLifecycle) {
    SettingsNavigation nav;

    EXPECT_EQ(nav.active_category(), SettingsCategory::Appearance);
    EXPECT_FALSE(nav.is_in_category_detail());
    EXPECT_FALSE(nav.is_searching());

    nav.select_category(SettingsCategory::Dock);
    EXPECT_EQ(nav.active_category(), SettingsCategory::Dock);

    nav.drill_down(SettingsCategory::Dock);
    EXPECT_TRUE(nav.is_in_category_detail());
    EXPECT_EQ(nav.active_category(), SettingsCategory::Dock);

    EXPECT_TRUE(nav.navigate_back());
    EXPECT_FALSE(nav.is_in_category_detail());

    // Search query
    nav.set_search_query("scale");
    EXPECT_TRUE(nav.is_searching());
    EXPECT_EQ(nav.search_query(), "scale");

    nav.clear_search();
    EXPECT_FALSE(nav.is_searching());
    EXPECT_TRUE(nav.search_query().empty());
}

TEST(SettingsNavigationTest, KeyboardIndexSelection) {
    SettingsNavigation nav;
    EXPECT_EQ(nav.selected_index(), 0);

    nav.select_next(5);
    EXPECT_EQ(nav.selected_index(), 1);

    nav.select_next(5);
    EXPECT_EQ(nav.selected_index(), 2);

    nav.select_prev(5);
    EXPECT_EQ(nav.selected_index(), 1);

    nav.select_prev(5);
    EXPECT_EQ(nav.selected_index(), 0);

    nav.select_prev(5); // Wraps from 0 to 4
    EXPECT_EQ(nav.selected_index(), 4);
}

// ============================================================================
// 7. SettingsLayout & Hit Testing Tests
// ============================================================================

TEST(SettingsLayoutTest, PortraitMobileLayout) {
    auto policy = make_test_policy(720, 1280);
    SettingsNavigation nav;
    auto schema = SettingsSchema::create_default_schema();
    auto visible = schema.settings_in_category(SettingsCategory::Appearance);

    SettingsLayout layout;
    layout.update(policy, nav, visible, false);

    EXPECT_TRUE(layout.is_portrait());
    EXPECT_GE(layout.window_rect().width, 320);
    EXPECT_GE(layout.window_rect().height, 400);

    // In root portrait view, content area contains the categories
    EXPECT_GT(layout.content_rect().width, 0);
    EXPECT_GT(layout.category_rects().size(), 0);

    // Category touch targets >= 48dp
    for (const auto& item : layout.category_rects()) {
        EXPECT_GE(item.second.height, 48);
    }
}

TEST(SettingsLayoutTest, LandscapeSplitLayout) {
    auto policy = make_test_policy(1920, 1080);
    SettingsNavigation nav;
    auto schema = SettingsSchema::create_default_schema();
    auto visible = schema.settings_in_category(SettingsCategory::Appearance);

    SettingsLayout layout;
    layout.update(policy, nav, visible, false);

    EXPECT_FALSE(layout.is_portrait());
    EXPECT_GT(layout.sidebar_rect().width, 0);
    EXPECT_GT(layout.content_rect().width, 0);
    EXPECT_LT(layout.sidebar_rect().width, layout.window_rect().width);

    // Content setting touch targets >= 48dp
    for (const auto& item : layout.setting_rows()) {
        EXPECT_GE(item.row_rect.height, 48);
    }
}

TEST(SettingsLayoutTest, HitTestingControls) {
    auto policy = make_test_policy(720, 1280);
    SettingsNavigation nav;
    auto schema = SettingsSchema::create_default_schema();
    auto visible = schema.settings_in_category(SettingsCategory::Appearance);

    SettingsLayout layout;
    layout.update(policy, nav, visible, false);

    // Close button
    auto close_rect = layout.close_button_rect();
    auto hit_close = layout.hit_test(close_rect.x + 5, close_rect.y + 5);
    EXPECT_EQ(hit_close.type, SettingsHitType::CloseButton);

    // Maximize button
    auto max_rect = layout.maximize_button_rect();
    auto hit_max = layout.hit_test(max_rect.x + 5, max_rect.y + 5);
    EXPECT_EQ(hit_max.type, SettingsHitType::MaximizeButton);

    // Titlebar drag
    auto title_rect = layout.titlebar_rect();
    auto hit_title = layout.hit_test(title_rect.x + 10, title_rect.y + 10);
    EXPECT_EQ(hit_title.type, SettingsHitType::Titlebar);

    // Outside window
    auto hit_outside = layout.hit_test(-10, -10);
    EXPECT_EQ(hit_outside.type, SettingsHitType::None);
}

// ============================================================================
// 8. SettingsController Interaction Tests
// ============================================================================

TEST(SettingsControllerTest, TouchCategoryNavigation) {
    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, "");
    SettingsNavigation nav;
    SettingsLayout layout;
    SettingsSearch search(schema);

    auto policy = make_test_policy(720, 1280);
    SettingsController controller(store, nav, layout, search);
    layout.update(policy, nav, controller.current_visible_settings(), false);

    bool rendered = false;
    controller.on_request_render([&]() { rendered = true; });

    // Tap on a category item in portrait mode
    ASSERT_FALSE(layout.category_rects().empty());
    auto first_cat = layout.category_rects()[1]; // Second category (Display)

    int32_t cx = first_cat.second.x + first_cat.second.width / 2;
    int32_t cy = first_cat.second.y + first_cat.second.height / 2;

    EXPECT_TRUE(controller.handle_touch_down(cx, cy));
    EXPECT_TRUE(controller.handle_touch_up(cx, cy));
    EXPECT_TRUE(rendered);
    EXPECT_TRUE(nav.is_in_category_detail());
    EXPECT_EQ(nav.active_category(), SettingsCategory::Display);
}

TEST(SettingsControllerTest, WindowActionCallbacks) {
    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, "");
    SettingsNavigation nav;
    SettingsLayout layout;
    SettingsSearch search(schema);

    auto policy = make_test_policy(720, 1280);
    SettingsController controller(store, nav, layout, search);
    layout.update(policy, nav, controller.current_visible_settings(), false);

    bool closed = false;
    bool maximized = false;
    bool minimized = false;

    controller.on_request_close([&]() { closed = true; });
    controller.on_request_maximize([&]() { maximized = true; });
    controller.on_request_minimize([&]() { minimized = true; });

    // Tap close button
    auto close_rect = layout.close_button_rect();
    EXPECT_TRUE(controller.handle_touch_down(close_rect.x + 5, close_rect.y + 5));
    EXPECT_TRUE(controller.handle_touch_up(close_rect.x + 5, close_rect.y + 5));
    EXPECT_TRUE(closed);

    // Tap maximize button
    auto max_rect = layout.maximize_button_rect();
    EXPECT_TRUE(controller.handle_touch_down(max_rect.x + 5, max_rect.y + 5));
    EXPECT_TRUE(controller.handle_touch_up(max_rect.x + 5, max_rect.y + 5));
    EXPECT_TRUE(maximized);

    // Tap minimize button
    auto min_rect = layout.minimize_button_rect();
    EXPECT_TRUE(controller.handle_touch_down(min_rect.x + 5, min_rect.y + 5));
    EXPECT_TRUE(controller.handle_touch_up(min_rect.x + 5, min_rect.y + 5));
    EXPECT_TRUE(minimized);
}

TEST(SettingsControllerTest, KeyboardNavigation) {
    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, "");
    SettingsNavigation nav;
    SettingsLayout layout;
    SettingsSearch search(schema);

    auto policy = make_test_policy(1920, 1080);
    SettingsController controller(store, nav, layout, search);
    layout.update(policy, nav, controller.current_visible_settings(), false);

    // Arrow Down (0xff54)
    EXPECT_TRUE(controller.handle_key(0xff54));
    EXPECT_EQ(nav.selected_index(), 1);

    // Arrow Up (0xff52)
    EXPECT_TRUE(controller.handle_key(0xff52));
    EXPECT_EQ(nav.selected_index(), 0);

    // Return (0xff0d)
    EXPECT_TRUE(controller.handle_key(0xff0d));

    // Escape (0xff1b)
    bool closed = false;
    controller.on_request_close([&]() { closed = true; });
    EXPECT_TRUE(controller.handle_key(0xff1b));
    EXPECT_TRUE(closed);
}

// ============================================================================
// 9. SettingsView Cairo Rendering Smoke Test
// ============================================================================

TEST(SettingsViewTest, CairoRenderBufferSafety) {
    auto policy = make_test_policy(720, 1280);
    config::Config cfg;
    auto schema = SettingsSchema::create_default_schema();
    SettingsStore store(cfg, schema, "");
    SettingsNavigation nav;
    SettingsLayout layout;
    SettingsView view;

    auto visible = schema.settings_in_category(SettingsCategory::Appearance);
    layout.update(policy, nav, visible, false);

    std::vector<uint8_t> mem(720 * 1280 * 4, 0);
    shell::ShmBuffer buffer(720, 1280, 720 * 4, mem.size(), -1, mem.data(), nullptr);

    shell::ShellTheme theme;
    shell::DesignTokens tokens = shell::DesignTokens::create_scaled(1.0);

    // Render should complete safely without crash
    view.render(buffer, theme, tokens, layout, nav, store, visible, 0);

    // Maximized landscape render
    auto landscape_policy = make_test_policy(1920, 1080);
    layout.update(landscape_policy, nav, visible, true);

    std::vector<uint8_t> l_mem(1920 * 1080 * 4, 0);
    shell::ShmBuffer landscape_buffer(1920, 1080, 1920 * 4, l_mem.size(), -1, l_mem.data(), nullptr);
    view.render(landscape_buffer, theme, tokens, layout, nav, store, visible, 0);
}

