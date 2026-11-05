#include "core/v1/settings_editor.h"

#include <iostream>
#include <string>

namespace v = panebind::core::v1;
namespace edit = v::settings_editor;
namespace {
int checks{}, failures{};
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
}
int main() {
    auto draft = v::default_settings();
    const auto runtime = draft;
    auto form = draft.hotkeys[4];
    form.key = 'J';
    check(draft == runtime, "cancelled form has not changed the Settings draft");
    check(edit::put_hotkey(draft, 4, form).empty() && draft.hotkeys[4].key == 'J' && runtime.hotkeys[4].key == 'C',
          "form confirmation edits draft only, not running snapshot");
    const auto before_rejection = draft;
    auto duplicate = v::Hotkey{3, 'J', v::Action::Restore};
    auto error = edit::put_hotkey(draft, std::nullopt, duplicate);
    check(error.find("5") != std::string::npos && draft == before_rejection,
          "duplicate keyboard edit names conflicting row and leaves draft intact");
    check(!edit::put_hotkey(draft, std::nullopt, {3, 0x7b, v::Action::Center}).empty() && draft == before_rejection,
          "F12 rejected transactionally");
    check(!edit::put_hotkey(draft, std::nullopt, {6, 0x7a, v::Action::Center}).empty() && draft == before_rejection,
          "Ctrl Shift F11 rejected transactionally");
    check(!edit::put_hotkey(draft, std::nullopt, {3, 'B', v::Action::None}).empty() && draft == before_rejection,
          "None is not a saveable action");
    check(edit::put_hotkey(draft, std::nullopt, {1, 0xf5, v::Action::Lower}).empty(),
          "legal uncommon key remains saveable without a preset entry");
    check(!edit::hotkey_label(draft.hotkeys.back()).empty() && draft.hotkeys.back().key == 0xf5,
          "uncommon key display does not replace its value");
    check(edit::put_hotkey(draft, draft.hotkeys.size() - 1, {1, 0xf5, v::Action::Center}).empty(),
          "editing own row does not self-conflict");
    check(edit::erase_hotkey(draft, draft.hotkeys.size() - 1) && draft == before_rejection,
          "delete removes the selected mapping, no None tombstone");
    check(!edit::erase_hotkey(draft, draft.hotkeys.size()), "out of range delete does not change list");

    const auto mouse_before = draft.mouse_bindings;
    const v::MouseBinding middle{0, 3, 1, 2, v::Action::Center};
    check(edit::put_mouse(draft, std::nullopt, middle).empty() && draft.mouse_bindings.back() == middle,
          "structured title middle Ctrl single mapping added");
    const auto mouse_added = draft;
    error = edit::put_mouse(draft, std::nullopt, {0, 3, 1, 2, v::Action::Lower});
    check(error.find(std::to_string(mouse_added.mouse_bindings.size())) != std::string::npos && draft == mouse_added,
          "mouse duplicate combination names row and retains old action");
    check(!edit::put_mouse(draft, std::nullopt, {9, 3, 1, 2, v::Action::Center}).empty() && draft == mouse_added,
          "existing nine region boundary remains enforced");
    check(edit::erase_mouse(draft, draft.mouse_bindings.size() - 1) && draft.mouse_bindings == mouse_before,
          "delete exact added mouse row restores previous mappings");

    v::Rule original{"C:\\测试 资料\\我的程序.exe", "Frame Class", false, 0xa5a5002dU};
    check(edit::put_rule(draft, std::nullopt, original).empty(), "plain Unicode spaced program path requires no user escaping");
    auto exclusion = edit::rule_features(original, true, 0);
    check(exclusion.excluded && exclusion.feature_mask == original.feature_mask,
          "exclude changes only excluded, retaining original known and unknown bits");
    auto customized = edit::rule_features(exclusion, false, 0x12);
    check(!customized.excluded && customized.feature_mask == 0xa5a50012U,
          "custom checkboxes change low six bits and preserve unknown high bits");
    check(edit::put_rule(draft, 0, customized).empty(), "editing rule retains exact path and class");
    const auto rules_added = draft;
    auto same = customized; same.exe = "c:/测试 资料/我的程序.EXE"; same.window_class = "frame class";
    error = edit::put_rule(draft, std::nullopt, same);
    check(error.find("1") != std::string::npos && draft == rules_added,
          "equivalent program plus class rejected with row and no silent duplicate");
    auto another_class = customized; another_class.window_class = "Frame/Other";
    check(edit::put_rule(draft, std::nullopt, another_class).empty(), "distinct exact window classes remain separate rules");
    const auto saved = v::serialize(draft);
    check(saved.find("%20") != std::string::npos && saved.find("hotkey_count=") != std::string::npos,
          "serializer generates counts and escaping from lists");
    const auto imported = v::parse(saved);
    check(imported.value && *imported.value == draft && runtime.hotkeys[4].key == 'C' && runtime.rules.empty(),
          "schema one round trip preserves mappings masks and Unicode without applying runtime");
    check(edit::erase_rule(draft, 1) && draft.rules.size() == 1, "rule delete removes only selected row");
    check(!edit::put_rule(draft, std::nullopt, {"*.exe", "", true, 0xffffffffU}).empty(),
          "structured entry retains runtime wildcard prohibition");

    check(edit::color_hex(0xd77800) == "#0078D7" && edit::parse_color_hex("#0078D7") == 0xd77800,
          "independent default COLORREF blue is RGB 0078D7");
    check(edit::parse_color_hex("#FF0000") == 0x0000ff && edit::color_hex(0x0000ff) == "#FF0000",
          "red RGB is low byte in COLORREF");
    check(edit::parse_color_hex("#0000FF") == 0xff0000 && edit::color_hex(0xff0000) == "#0000FF",
          "blue RGB is high byte in COLORREF");
    check(edit::parse_color_hex("#aB12cD") == 0xcd12ab, "hex editor accepts case without channel swap");
    check(!edit::parse_color_hex("0078D7") && !edit::parse_color_hex("#12345") &&
          !edit::parse_color_hex("#1234567") && !edit::parse_color_hex("#12X456") &&
          !edit::parse_color_hex("# 12345") && !edit::parse_color_hex("#-12345"),
          "invalid color strings cannot become saveable colors");
    auto color_file = v::default_settings(); color_file.preview_color = 0x563412;
    const auto color_import = v::parse(v::serialize(color_file));
    check(color_import.value && edit::parse_color_hex(edit::color_hex(color_import.value->preview_color)) == color_file.preview_color,
          "old schema color displays and reexports without red blue inversion");

    auto pages = draft;
    pages.enabled = false; pages.startup = true; pages.animations = true;
    pages.attraction_dip = 3; pages.release_dip = 30;
    pages.preview_color = 0x563412; pages.feedback = false; pages.step_dip = 99;
    pages.generation = 501;
    const auto modified = pages;
    edit::restore_page(pages, 3);
    check(pages.hotkeys == runtime.hotkeys && pages.mouse_bindings == runtime.mouse_bindings && pages.step_dip == runtime.step_dip,
          "mapping page default restores both lists and step");
    auto expect_mapping_only = modified;
    expect_mapping_only.hotkeys = runtime.hotkeys; expect_mapping_only.mouse_bindings = runtime.mouse_bindings;
    expect_mapping_only.step_dip = runtime.step_dip;
    check(pages == expect_mapping_only, "mapping page default preserves other pages and schema generation");
    edit::restore_page(pages, 4);
    check(pages.rules.empty() && pages.preview_color == 0x563412 && !pages.enabled && pages.attraction_dip == 3,
          "rules default does not discard unrelated page drafts");
    edit::restore_page(pages, 2);
    check(pages.preview_color == runtime.preview_color && pages.preview_alpha == runtime.preview_alpha &&
          pages.dock_actions == runtime.dock_actions && pages.attraction_dip == 3 && !pages.feedback,
          "dock page default includes color while preserving magnet visual and running draft");
    edit::restore_all(pages);
    auto expect_all = runtime; expect_all.generation = 501;
    check(pages == expect_all, "all defaults preserves schema generation and uses existing defaults");
    for (unsigned i = 1; i <= static_cast<unsigned>(v::Action::Exit); ++i)
        check(!edit::action_label(static_cast<v::Action>(i)).empty(), "all existing valid actions have shared Chinese label");
    check(edit::hotkey_label({3, 'C', v::Action::Center}) == L"Ctrl+Alt+C" &&
          edit::region_label(0) == L"标题栏" && edit::button_label(3) == L"中键",
          "lists and help share readable combinations");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
