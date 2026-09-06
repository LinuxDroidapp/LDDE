#include <gtest/gtest.h>
#include "ldde/application/application_id.hpp"
#include "ldde/application/application_icon.hpp"
#include "ldde/application/desktop_entry_source.hpp"
#include "ldde/application/desktop_entry.hpp"
#include "ldde/application/desktop_entry_reader.hpp"
#include "ldde/application/desktop_entry_parser.hpp"
#include "ldde/application/application_metadata.hpp"
#include "ldde/application/application_discovery_policy.hpp"
#include "ldde/application/application_catalog.hpp"
#include "ldde/application/application_discovery.hpp"
#include "ldde/config/config.hpp"
#include <filesystem>
#include <fstream>

using namespace ldde::application;
using namespace ldde::core;

// =============================================================================
// 1. ApplicationId Tests
// =============================================================================

TEST(ApplicationIdTest, ValidationAndBasename) {
    ApplicationId valid_id("org.gnome.Calculator.desktop");
    EXPECT_TRUE(valid_id.is_valid());
    EXPECT_EQ(valid_id.value(), "org.gnome.Calculator.desktop");
    EXPECT_EQ(valid_id.basename_without_extension(), "org.gnome.Calculator");

    ApplicationId invalid1("");
    EXPECT_FALSE(invalid1.is_valid());

    ApplicationId invalid2("app");
    EXPECT_FALSE(invalid2.is_valid());

    ApplicationId invalid3(".desktop");
    EXPECT_FALSE(invalid3.is_valid());

    ApplicationId invalid_path("path/to/app.desktop");
    EXPECT_FALSE(invalid_path.is_valid());
}

TEST(ApplicationIdTest, SubdirectoryToHyphenatedId) {
    std::filesystem::path rel_path = std::filesystem::path("kde") / "plasma-editor.desktop";
    ApplicationId id = ApplicationId::from_relative_path(rel_path);
    EXPECT_EQ(id.value(), "kde-plasma-editor.desktop");
    EXPECT_TRUE(id.is_valid());
}

// =============================================================================
// 2. ApplicationIconReference Tests
// =============================================================================

TEST(ApplicationIconTest, ThemeNameVsFilePath) {
    ApplicationIconReference theme_icon("htop");
    EXPECT_TRUE(theme_icon.has_icon());
    EXPECT_TRUE(theme_icon.is_theme_name());
    EXPECT_FALSE(theme_icon.is_file_path());
    EXPECT_EQ(theme_icon.raw(), "htop");

    ApplicationIconReference path_icon("/usr/share/pixmaps/app.png");
    EXPECT_TRUE(path_icon.has_icon());
    EXPECT_FALSE(path_icon.is_theme_name());
    EXPECT_TRUE(path_icon.is_file_path());

    ApplicationIconReference rel_path("./icons/icon.svg");
    EXPECT_TRUE(rel_path.is_file_path());

    ApplicationIconReference empty_icon("");
    EXPECT_FALSE(empty_icon.has_icon());
    EXPECT_EQ(empty_icon.type(), IconType::None);
}

// =============================================================================
// 3. DesktopEntryParser Tests
// =============================================================================

TEST(DesktopEntryParserTest, BasicValidEntry) {
    std::string content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Calculator\n"
        "GenericName=Scientific Calculator\n"
        "Comment=Perform arithmetic and scientific calculations\n"
        "Exec=gnome-calculator\n"
        "Icon=org.gnome.Calculator\n"
        "Terminal=false\n"
        "Categories=Utility;Calculator;\n"
        "Keywords=calculation;arithmetic;math;\n";

    auto res = DesktopEntryParser::parse(content);
    ASSERT_TRUE(res.is_ok());

    const auto& entry = res.value();
    EXPECT_TRUE(entry.is_valid_application());
    EXPECT_EQ(entry.get_string("Desktop Entry", "Name"), "Calculator");
    EXPECT_EQ(entry.get_string("Desktop Entry", "GenericName"), "Scientific Calculator");
    EXPECT_EQ(entry.get_string("Desktop Entry", "Exec"), "gnome-calculator");
    EXPECT_EQ(entry.get_string("Desktop Entry", "Icon"), "org.gnome.Calculator");
    EXPECT_FALSE(entry.get_bool("Desktop Entry", "Terminal"));

    auto cats = entry.get_string_list("Desktop Entry", "Categories");
    ASSERT_EQ(cats.size(), 2u);
    EXPECT_EQ(cats[0], "Utility");
    EXPECT_EQ(cats[1], "Calculator");

    auto kws = entry.get_string_list("Desktop Entry", "Keywords");
    ASSERT_EQ(kws.size(), 3u);
    EXPECT_EQ(kws[0], "calculation");
    EXPECT_EQ(kws[1], "arithmetic");
    EXPECT_EQ(kws[2], "math");
}

TEST(DesktopEntryParserTest, CommentsWhitespaceAndCRLF) {
    std::string content =
        "# Top level comment\r\n"
        "  \r\n"
        "[Desktop Entry]  \r\n"
        "# Comment inside group\r\n"
        "Type = Application  \r\n"
        "Name = Htop Process Viewer  \r\n"
        "Exec = htop \r\n"
        "Terminal = 1 \r\n";

    auto res = DesktopEntryParser::parse(content);
    ASSERT_TRUE(res.is_ok());

    const auto& entry = res.value();
    EXPECT_TRUE(entry.is_valid_application());
    EXPECT_EQ(entry.get_string("Desktop Entry", "Name"), "Htop Process Viewer");
    EXPECT_EQ(entry.get_string("Desktop Entry", "Exec"), "htop");
    EXPECT_TRUE(entry.get_bool("Desktop Entry", "Terminal"));
}

TEST(DesktopEntryParserTest, EscapeSequences) {
    std::string content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Line1\\nLine2\\sSpaced\n"
        "Comment=Tab\\tBackslash\\\\Semicolon\\;\n"
        "Exec=app\n";

    auto res = DesktopEntryParser::parse(content);
    ASSERT_TRUE(res.is_ok());

    const auto& entry = res.value();
    EXPECT_EQ(entry.get_string("Desktop Entry", "Name"), "Line1\nLine2 Spaced");
    EXPECT_EQ(entry.get_string("Desktop Entry", "Comment"), "Tab\tBackslash\\Semicolon;");
}

TEST(DesktopEntryParserTest, LocalizationMatchingAndFallback) {
    std::string content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Editor\n"
        "Name[en_GB]=Editor (UK)\n"
        "Name[fr]=Éditeur\n"
        "Name[fr_CA]=Éditeur Canadien\n"
        "Name[de]=Texteditor\n"
        "Exec=editor\n";

    auto res = DesktopEntryParser::parse(content);
    ASSERT_TRUE(res.is_ok());

    const auto& entry = res.value();

    // Exact match fr_CA
    EXPECT_EQ(entry.get_localized_string("Desktop Entry", "Name", "fr_CA.UTF-8"), "Éditeur Canadien");

    // Fallback fr_BE -> fr
    EXPECT_EQ(entry.get_localized_string("Desktop Entry", "Name", "fr_BE"), "Éditeur");

    // Match de
    EXPECT_EQ(entry.get_localized_string("Desktop Entry", "Name", "de_DE.UTF-8"), "Texteditor");

    // Fallback to unlocalized when locale not present
    EXPECT_EQ(entry.get_localized_string("Desktop Entry", "Name", "ja_JP"), "Editor");

    // Empty locale returns unlocalized
    EXPECT_EQ(entry.get_localized_string("Desktop Entry", "Name", ""), "Editor");
}

TEST(DesktopEntryParserTest, DesktopActionsParsing) {
    std::string content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Browser\n"
        "Exec=browser\n"
        "Actions=NewWindow;Private;\n"
        "\n"
        "[Desktop Action NewWindow]\n"
        "Name=New Window\n"
        "Exec=browser --new-window\n"
        "\n"
        "[Desktop Action Private]\n"
        "Name=Private Window\n"
        "Exec=browser --incognito\n";

    auto res = DesktopEntryParser::parse(content);
    ASSERT_TRUE(res.is_ok());

    const auto& entry = res.value();
    auto acts = entry.actions();
    ASSERT_EQ(acts.size(), 2u);
    EXPECT_EQ(acts[0], "NewWindow");
    EXPECT_EQ(acts[1], "Private");

    EXPECT_EQ(entry.get_string("Desktop Action NewWindow", "Name"), "New Window");
    EXPECT_EQ(entry.get_string("Desktop Action NewWindow", "Exec"), "browser --new-window");
    EXPECT_EQ(entry.get_string("Desktop Action Private", "Name"), "Private Window");
    EXPECT_EQ(entry.get_string("Desktop Action Private", "Exec"), "browser --incognito");
}

TEST(DesktopEntryParserTest, InvalidEntriesRejected) {
    // Missing [Desktop Entry]
    std::string missing_group = "Type=Application\nName=App\nExec=app\n";
    auto res1 = DesktopEntryParser::parse(missing_group);
    EXPECT_TRUE(res1.is_error());

    // Type is Link, not Application
    std::string link_type = "[Desktop Entry]\nType=Link\nName=Web\nURL=https://example.com\n";
    auto res2 = DesktopEntryParser::parse(link_type);
    ASSERT_TRUE(res2.is_ok());
    EXPECT_FALSE(res2.value().is_valid_application());

    // Missing Exec
    std::string missing_exec = "[Desktop Entry]\nType=Application\nName=NoExec\n";
    auto res3 = DesktopEntryParser::parse(missing_exec);
    ASSERT_TRUE(res3.is_ok());
    EXPECT_FALSE(res3.value().is_valid_application());
}

// =============================================================================
// 4. Exec Field Parsing Tests
// =============================================================================

TEST(ExecFieldParsingTest, StructuredExecAndFieldCodes) {
    ParsedExec pe1 = DesktopEntryParser::parse_exec("gedit --new-window %U");
    EXPECT_EQ(pe1.executable, "gedit");
    ASSERT_EQ(pe1.arguments.size(), 2u);
    EXPECT_EQ(pe1.arguments[0], "--new-window");
    EXPECT_EQ(pe1.arguments[1], "%U");
    ASSERT_EQ(pe1.field_codes.size(), 1u);
    EXPECT_EQ(pe1.field_codes[0], "%U");

    ParsedExec pe2 = DesktopEntryParser::parse_exec("\"/opt/My App/bin/launcher\" -f \"file name.txt\" %f");
    EXPECT_EQ(pe2.executable, "/opt/My App/bin/launcher");
    ASSERT_EQ(pe2.arguments.size(), 3u);
    EXPECT_EQ(pe2.arguments[0], "-f");
    EXPECT_EQ(pe2.arguments[1], "file name.txt");
    EXPECT_EQ(pe2.arguments[2], "%f");
    ASSERT_EQ(pe2.field_codes.size(), 1u);
    EXPECT_EQ(pe2.field_codes[0], "%f");
}

// =============================================================================
// 5. ApplicationMetadata & Visibility Tests
// =============================================================================

TEST(ApplicationMetadataTest, VisibilityFilteringRules) {
    std::string content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=TestApp\n"
        "Exec=testapp\n"
        "OnlyShowIn=GNOME;LinuxDroid;\n"
        "NotShowIn=KDE;\n";

    auto res = DesktopEntryParser::parse(content);
    ASSERT_TRUE(res.is_ok());

    ApplicationId id("testapp.desktop");
    DesktopEntrySource src("/usr/share/applications/testapp.desktop", DesktopEntrySourceType::System);
    auto meta_res = ApplicationMetadata::from_desktop_entry(id, res.value(), src);
    ASSERT_TRUE(meta_res.is_ok());

    const auto& meta = meta_res.value();
    EXPECT_TRUE(meta.is_visible_in_desktop("LinuxDroid"));
    EXPECT_TRUE(meta.is_visible_in_desktop("GNOME"));
    EXPECT_FALSE(meta.is_visible_in_desktop("XFCE")); // Not in OnlyShowIn
    EXPECT_FALSE(meta.is_visible_in_desktop("KDE"));  // In NotShowIn

    EXPECT_TRUE(meta.is_visible_to_user("LinuxDroid"));
}

TEST(ApplicationMetadataTest, HiddenAndNoDisplaySuppression) {
    std::string hidden_content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=HiddenApp\n"
        "Exec=hiddenapp\n"
        "Hidden=true\n";

    auto res1 = DesktopEntryParser::parse(hidden_content);
    ASSERT_TRUE(res1.is_ok());
    ApplicationId id1("hidden.desktop");
    DesktopEntrySource src1("/usr/share/applications/hidden.desktop", DesktopEntrySourceType::System);
    auto m1 = ApplicationMetadata::from_desktop_entry(id1, res1.value(), src1).value();
    EXPECT_TRUE(m1.is_hidden());
    EXPECT_FALSE(m1.is_visible_to_user("LinuxDroid"));

    std::string nodisp_content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Helper\n"
        "Exec=helper\n"
        "NoDisplay=true\n";

    auto res2 = DesktopEntryParser::parse(nodisp_content);
    ASSERT_TRUE(res2.is_ok());
    ApplicationId id2("helper.desktop");
    DesktopEntrySource src2("/usr/share/applications/helper.desktop", DesktopEntrySourceType::System);
    auto m2 = ApplicationMetadata::from_desktop_entry(id2, res2.value(), src2).value();
    EXPECT_TRUE(m2.is_no_display());
    EXPECT_FALSE(m2.is_visible_to_user("LinuxDroid"));
}

TEST(ApplicationMetadataTest, SearchQueryMatching) {
    std::string content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Firefox Web Browser\n"
        "GenericName=Internet Browser\n"
        "Comment=Explore the World Wide Web\n"
        "Exec=firefox %u\n"
        "Categories=Network;WebBrowser;\n"
        "Keywords=internet;web;mozilla;\n";

    auto res = DesktopEntryParser::parse(content);
    ASSERT_TRUE(res.is_ok());
    ApplicationId id("firefox.desktop");
    DesktopEntrySource src("/usr/share/applications/firefox.desktop", DesktopEntrySourceType::System);
    auto meta = ApplicationMetadata::from_desktop_entry(id, res.value(), src).value();

    EXPECT_TRUE(meta.matches_search_query("firefox"));
    EXPECT_TRUE(meta.matches_search_query("Browser"));
    EXPECT_TRUE(meta.matches_search_query("internet"));
    EXPECT_TRUE(meta.matches_search_query("mozilla"));
    EXPECT_TRUE(meta.matches_search_query("WebBrowser"));
    EXPECT_FALSE(meta.matches_search_query("calculator"));
}

// =============================================================================
// 6. ApplicationCatalog Tests
// =============================================================================

TEST(ApplicationCatalogTest, DeterministicOrderingAndDiffing) {
    ApplicationCatalog catalog;

    // Create 3 application records
    std::string app_c = "[Desktop Entry]\nType=Application\nName=Calendar\nExec=cal\n";
    std::string app_a = "[Desktop Entry]\nType=Application\nName=calculator\nExec=calc\n";
    std::string app_b = "[Desktop Entry]\nType=Application\nName=Browser\nExec=browser\n";

    auto m_c = ApplicationMetadata::from_desktop_entry(
        ApplicationId("cal.desktop"),
        DesktopEntryParser::parse(app_c).value(),
        DesktopEntrySource("/usr/share/applications/cal.desktop", DesktopEntrySourceType::System)).value();

    auto m_a = ApplicationMetadata::from_desktop_entry(
        ApplicationId("calc.desktop"),
        DesktopEntryParser::parse(app_a).value(),
        DesktopEntrySource("/usr/share/applications/calc.desktop", DesktopEntrySourceType::System)).value();

    auto m_b = ApplicationMetadata::from_desktop_entry(
        ApplicationId("browser.desktop"),
        DesktopEntryParser::parse(app_b).value(),
        DesktopEntrySource("/usr/share/applications/browser.desktop", DesktopEntrySourceType::System)).value();

    std::vector<ApplicationMetadata> apps = {m_c, m_a, m_b};

    size_t added_count = 0;
    catalog.on_application_added([&](const ApplicationMetadata&) {
        added_count++;
    });

    CatalogDiff diff = catalog.update_applications(apps);
    EXPECT_EQ(diff.added.size(), 3u);
    EXPECT_EQ(added_count, 3u);
    EXPECT_EQ(catalog.count(), 3u);

    // Verify deterministic alphabetical ordering: "Browser", "calculator", "Calendar"
    auto visible = catalog.visible_applications();
    ASSERT_EQ(visible.size(), 3u);
    EXPECT_EQ(visible[0].name(), "Browser");
    EXPECT_EQ(visible[1].name(), "calculator");
    EXPECT_EQ(visible[2].name(), "Calendar");

    // Remove cal.desktop, modify calc.desktop
    std::string app_a_mod = "[Desktop Entry]\nType=Application\nName=Calculator Pro\nExec=calc --pro\n";
    auto m_a_mod = ApplicationMetadata::from_desktop_entry(
        ApplicationId("calc.desktop"),
        DesktopEntryParser::parse(app_a_mod).value(),
        DesktopEntrySource("/usr/share/applications/calc.desktop", DesktopEntrySourceType::System)).value();

    CatalogDiff diff2 = catalog.update_applications({m_b, m_a_mod});
    EXPECT_EQ(diff2.added.size(), 0u);
    EXPECT_EQ(diff2.removed.size(), 1u);
    EXPECT_EQ(diff2.removed[0].id().value(), "cal.desktop");
    EXPECT_EQ(diff2.changed.size(), 1u);
    EXPECT_EQ(diff2.changed[0].id().value(), "calc.desktop");
    EXPECT_EQ(diff2.changed[0].name(), "Calculator Pro");

    EXPECT_EQ(catalog.count(), 2u);
}

// =============================================================================
// 7. Discovery Precedence & Filesystem Overrides Tests
// =============================================================================

TEST(ApplicationDiscoveryTest, UserDirectoryPrecedenceOverSystem) {
    char template_dir[] = "/tmp/ldde_discovery_XXXXXX";
    char* root = mkdtemp(template_dir);
    ASSERT_NE(root, nullptr);
    std::filesystem::path root_path(root);

    std::filesystem::path sys_dir = root_path / "usr" / "share" / "applications";
    std::filesystem::path user_dir = root_path / "home" / ".local" / "share" / "applications";
    std::filesystem::create_directories(sys_dir);
    std::filesystem::create_directories(user_dir);

    // 1. System defines text-editor.desktop (Name="System Editor")
    std::ofstream ofs_sys(sys_dir / "editor.desktop");
    ofs_sys << "[Desktop Entry]\nType=Application\nName=System Editor\nExec=sys-editor\n";
    ofs_sys.close();

    // 2. User overrides text-editor.desktop (Name="My Custom Editor")
    std::ofstream ofs_usr(user_dir / "editor.desktop");
    ofs_usr << "[Desktop Entry]\nType=Application\nName=My Custom Editor\nExec=user-editor\n";
    ofs_usr.close();

    ApplicationCatalog catalog;
    ApplicationDiscoveryPolicy policy;
    // Add user directory with higher precedence (priority 0)
    policy.add_search_directory(user_dir, DesktopEntrySourceType::User, 0);
    // Add system directory with lower precedence (priority 1)
    policy.add_search_directory(sys_dir, DesktopEntrySourceType::System, 1);

    ApplicationDiscovery discovery(catalog, policy);
    Status s = discovery.scan_and_refresh();
    EXPECT_TRUE(s.is_ok());

    EXPECT_EQ(catalog.count(), 1u);
    const auto* meta = catalog.find(ApplicationId("editor.desktop"));
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->name(), "My Custom Editor");
    EXPECT_EQ(meta->exec(), "user-editor");
    EXPECT_TRUE(meta->source().is_user_level());

    // Clean up
    std::filesystem::remove_all(root_path);
}

TEST(ApplicationDiscoveryTest, UserHiddenOverrideDeletesApplication) {
    char template_dir[] = "/tmp/ldde_discovery_hidden_XXXXXX";
    char* root = mkdtemp(template_dir);
    ASSERT_NE(root, nullptr);
    std::filesystem::path root_path(root);

    std::filesystem::path sys_dir = root_path / "usr" / "share" / "applications";
    std::filesystem::path user_dir = root_path / "home" / ".local" / "share" / "applications";
    std::filesystem::create_directories(sys_dir);
    std::filesystem::create_directories(user_dir);

    // System defines bloatware.desktop
    std::ofstream ofs_sys(sys_dir / "bloatware.desktop");
    ofs_sys << "[Desktop Entry]\nType=Application\nName=Bloatware\nExec=bloat\n";
    ofs_sys.close();

    // User hides bloatware.desktop with Hidden=true
    std::ofstream ofs_usr(user_dir / "bloatware.desktop");
    ofs_usr << "[Desktop Entry]\nType=Application\nName=Bloatware\nExec=bloat\nHidden=true\n";
    ofs_usr.close();

    ApplicationCatalog catalog;
    ApplicationDiscoveryPolicy policy;
    policy.add_search_directory(user_dir, DesktopEntrySourceType::User, 0);
    policy.add_search_directory(sys_dir, DesktopEntrySourceType::System, 1);

    ApplicationDiscovery discovery(catalog, policy);
    Status s = discovery.scan_and_refresh();
    EXPECT_TRUE(s.is_ok());

    // Found in catalog as hidden, but NOT in visible applications
    EXPECT_EQ(catalog.count(), 1u);
    EXPECT_EQ(catalog.visible_count(), 0u);
    auto visible = catalog.visible_applications();
    EXPECT_TRUE(visible.empty());

    // Clean up
    std::filesystem::remove_all(root_path);
}

TEST(ApplicationDiscoveryTest, CorruptFileDoesNotBreakDiscovery) {
    char template_dir[] = "/tmp/ldde_discovery_corrupt_XXXXXX";
    char* root = mkdtemp(template_dir);
    ASSERT_NE(root, nullptr);
    std::filesystem::path root_path(root);

    std::filesystem::path sys_dir = root_path / "usr" / "share" / "applications";
    std::filesystem::create_directories(sys_dir);

    // 1. Corrupt invalid file
    std::ofstream ofs_corrupt(sys_dir / "broken.desktop");
    ofs_corrupt << "This is not a desktop file and has no group\n";
    ofs_corrupt.close();

    // 2. Valid file
    std::ofstream ofs_good(sys_dir / "good.desktop");
    ofs_good << "[Desktop Entry]\nType=Application\nName=Good App\nExec=good\n";
    ofs_good.close();

    ApplicationCatalog catalog;
    ApplicationDiscoveryPolicy policy;
    policy.add_search_directory(sys_dir, DesktopEntrySourceType::System, 0);

    ApplicationDiscovery discovery(catalog, policy);
    Status s = discovery.scan_and_refresh();
    EXPECT_TRUE(s.is_ok());

    // Only good app was cataloged
    EXPECT_EQ(catalog.count(), 1u);
    const auto* meta = catalog.find(ApplicationId("good.desktop"));
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->name(), "Good App");

    // Clean up
    std::filesystem::remove_all(root_path);
}

