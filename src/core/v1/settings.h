#pragma once

#include "core/v1/types.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace panebind::core::v1 {

// Portable configuration values, not native input structures. Modifier bits:
// Alt=1, Control=2, Shift=4, Super=8. Buttons: left/right/middle/X1/X2=1..5.
// Regions: title=0, left/right/top/bottom=1..4, TL/TR/BL/BR=5..8.
struct Hotkey {
    unsigned modifiers{}, key{};
    Action action{Action::None};
    bool operator==(const Hotkey&) const = default;
};
struct MouseBinding {
    int region{}, button{1}, clicks{1};
    unsigned modifiers{};
    Action action{Action::None};
    bool operator==(const MouseBinding&) const = default;
};
struct Rule {
    std::string exe, window_class;
    bool excluded{};
    std::uint32_t feature_mask{0xffffffffU};
    bool operator==(const Rule&) const = default;
};
struct Settings {
    unsigned schema{1};
    std::uint64_t generation{1};
    bool enabled{true}, magnet_move{true}, magnet_resize{true}, glue{true};
    bool docking{true}, stretch{true}, smart_docking{true}, smart_stretch{true};
    bool preview{true}, feedback{true}, drag_transparency{}, shake{}, animations{}, startup{};
    int attraction_dip{10}, release_dip{16}, speed_dip{2000}, adjacency_dip{2};
    int step_dip{10}, preview_alpha{100}, preview_color{0xD77800};
    int drag_alpha{190}, shake_alpha{190}, shake_amplitude_dip{24};
    int shake_reversals{3}, shake_window_ms{700}, seam_dwell_ms{400};
    // left, right, top, bottom, TL, TR, BL, BR. Top defaults to maximize;
    // selecting DockTop instead enables the configurable upper half-screen.
    std::array<Action, 8> dock_actions{
        Action::DockLeft, Action::DockRight, Action::Maximize, Action::DockBottom,
        Action::DockTopLeft, Action::DockTopRight, Action::DockBottomLeft, Action::DockBottomRight};
    std::vector<Hotkey> hotkeys;
    std::vector<MouseBinding> mouse_bindings;
    std::vector<Rule> rules;
    bool operator==(const Settings&) const = default;
};

namespace settings_detail {
inline constexpr std::array<std::string_view, 37> action_names{
    "None", "Minimize", "Maximize", "Restore", "Center", "Lower",
    "MoveLeft", "MoveRight", "MoveUp", "MoveDown", "WidthLess", "WidthMore", "HeightLess", "HeightMore",
    "NextDisplay", "ToggleTopmost", "OpacityDown", "OpacityUp",
    "DockLeft", "DockRight", "DockTop", "DockBottom", "DockTopLeft", "DockTopRight", "DockBottomLeft", "DockBottomRight",
    "StretchLeft", "StretchRight", "StretchTop", "StretchBottom", "StretchHorizontal", "StretchVertical", "StretchAll",
    "Undo", "Pause", "Settings", "Exit"};
inline constexpr std::size_t max_file_bytes = 262144;
inline constexpr std::size_t max_hotkeys = 128, max_mouse = 360, max_rules = 256;
inline bool valid_action(Action action) {
    const auto i = static_cast<std::size_t>(action);
    return i < action_names.size();
}
inline bool utf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto a = static_cast<unsigned char>(text[i++]);
        if (a < 0x80) { if (a == 0) return false; continue; }
        std::uint32_t cp{};
        unsigned continuation{};
        if (a >= 0xc2 && a <= 0xdf) { cp = a & 0x1fU; continuation = 1; }
        else if (a >= 0xe0 && a <= 0xef) { cp = a & 0x0fU; continuation = 2; }
        else if (a >= 0xf0 && a <= 0xf4) { cp = a & 7U; continuation = 3; }
        else return false;
        if (i + continuation > text.size()) return false;
        for (unsigned n = 0; n < continuation; ++n) {
            const auto b = static_cast<unsigned char>(text[i++]);
            if ((b & 0xc0U) != 0x80U) return false;
            cp = (cp << 6U) | (b & 0x3fU);
        }
        if ((continuation == 1 && cp < 0x80) || (continuation == 2 && cp < 0x800) ||
            (continuation == 3 && cp < 0x10000) || cp > 0x10ffff ||
            (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}
inline bool rule_text(std::string_view value, std::size_t limit) {
    if (value.size() > limit || !utf8(value)) return false;
    for (const unsigned char c : value) if (c < 32 || c == 127) return false;
    return true;
}
inline std::string ascii_case_fold(std::string_view value) {
    std::string result(value);
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return result;
}
inline std::string ascii_fold(std::string_view value) {
    auto result=ascii_case_fold(value);
    for(char& c:result)if(c=='/')c='\\';
    return result;
}
inline std::string escape(std::string_view value) {
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c : value) {
        if (c == '%' || c == '|' || c == '=' || c == '#' || c == ' ' || c < 32 || c == 127) {
            out += '%'; out += digits[c >> 4U]; out += digits[c & 15U];
        } else out += static_cast<char>(c);
    }
    return out;
}
inline std::optional<std::string> unescape(std::string_view value) {
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '%') { out += value[i]; continue; }
        if (i + 2 >= value.size()) return std::nullopt;
        const int a = hex(value[i + 1]), b = hex(value[i + 2]);
        if (a < 0 || b < 0) return std::nullopt;
        out += static_cast<char>((a << 4) | b); i += 2;
    }
    return out;
}
template <typename T> inline bool number(std::string_view value, T& out) {
    if (value.empty()) return false;
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
    return ec == std::errc{} && end == value.data() + value.size();
}
inline std::vector<std::string_view> split(std::string_view value, char delimiter) {
    std::vector<std::string_view> out;
    for (;;) {
        const auto end = value.find(delimiter);
        out.push_back(value.substr(0, end));
        if (end == std::string_view::npos) return out;
        value.remove_prefix(end + 1);
    }
}
inline std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}
struct BoolField { std::string_view name; bool Settings::* member; };
inline constexpr std::array bool_fields{
    BoolField{"enabled", &Settings::enabled}, BoolField{"magnet_move", &Settings::magnet_move},
    BoolField{"magnet_resize", &Settings::magnet_resize}, BoolField{"glue", &Settings::glue},
    BoolField{"docking", &Settings::docking}, BoolField{"stretch", &Settings::stretch},
    BoolField{"smart_docking", &Settings::smart_docking}, BoolField{"smart_stretch", &Settings::smart_stretch},
    BoolField{"preview", &Settings::preview}, BoolField{"feedback", &Settings::feedback},
    BoolField{"drag_transparency", &Settings::drag_transparency}, BoolField{"shake", &Settings::shake},
    BoolField{"animations", &Settings::animations}, BoolField{"startup", &Settings::startup}};
struct IntField { std::string_view name; int Settings::* member; int minimum, maximum; };
inline constexpr std::array int_fields{
    IntField{"attraction_dip", &Settings::attraction_dip, 0, 100},
    IntField{"release_dip", &Settings::release_dip, 1, 200},
    IntField{"speed_dip", &Settings::speed_dip, 0, 20000},
    IntField{"adjacency_dip", &Settings::adjacency_dip, 0, 20},
    IntField{"step_dip", &Settings::step_dip, 1, 1000},
    IntField{"preview_alpha", &Settings::preview_alpha, 1, 255},
    IntField{"preview_color", &Settings::preview_color, 0, 0xffffff},
    IntField{"drag_alpha", &Settings::drag_alpha, 16, 255},
    IntField{"shake_alpha", &Settings::shake_alpha, 16, 255},
    IntField{"shake_amplitude_dip", &Settings::shake_amplitude_dip, 4, 300},
    IntField{"shake_reversals", &Settings::shake_reversals, 2, 20},
    IntField{"shake_window_ms", &Settings::shake_window_ms, 100, 5000},
    IntField{"seam_dwell_ms", &Settings::seam_dwell_ms, 100, 3000}};
} // namespace settings_detail

inline std::string_view action_name(Action action) {
    return settings_detail::valid_action(action) ?
        settings_detail::action_names[static_cast<std::size_t>(action)] : std::string_view{};
}
inline std::optional<Action> action_from_name(std::string_view name) {
    for (std::size_t i = 0; i < settings_detail::action_names.size(); ++i)
        if (settings_detail::action_names[i] == name) return static_cast<Action>(i);
    return std::nullopt;
}
inline Settings default_settings() {
    Settings value;
    // PaneBind defaults deliberately leave native Windows+arrow untouched.
    value.hotkeys = {{3, 0x25, Action::DockLeft}, {3, 0x27, Action::DockRight},
        {3, 0x26, Action::Maximize}, {3, 0x28, Action::DockBottom},
        {3, 'C', Action::Center}, {3, 'R', Action::Restore}, {3, 'U', Action::Undo},
        {3, 'T', Action::ToggleTopmost}, {3, 'N', Action::NextDisplay}};
    // Double-click on a border stretches only the selected border; Shift is
    // resolved by the command service using the locked modifier snapshot.
    value.mouse_bindings = {{0, 1, 2, 0, Action::Maximize},
        {1, 1, 2, 0, Action::StretchLeft}, {2, 1, 2, 0, Action::StretchRight},
        {3, 1, 2, 0, Action::StretchTop}, {4, 1, 2, 0, Action::StretchBottom},
        {1, 1, 2, 4, Action::StretchHorizontal}, {2, 1, 2, 4, Action::StretchHorizontal},
        {3, 1, 2, 4, Action::StretchVertical}, {4, 1, 2, 4, Action::StretchVertical}};
    return value;
}

inline std::string validate(const Settings& value) {
    using namespace settings_detail;
    if (value.schema != 1) return "不支持的配置 schema；原配置未更改";
    if (value.generation == 0) return "配置代际必须大于零";
    for (const auto& f : int_fields)
        if (value.*(f.member) < f.minimum || value.*(f.member) > f.maximum)
            return "参数超出范围：" + std::string(f.name) + " [" + std::to_string(f.minimum) + "," + std::to_string(f.maximum) + "]";
    if (value.release_dip <= value.attraction_dip) return "释放距离必须严格大于吸引距离";
    for (const auto action : value.dock_actions) {
        if (!(action == Action::None || action == Action::Maximize ||
              (action >= Action::DockLeft && action <= Action::DockBottomRight)))
            return "八方向停靠动作仅接受停靠、最大化或 None";
    }
    if (value.hotkeys.size() > max_hotkeys || value.mouse_bindings.size() > max_mouse || value.rules.size() > max_rules)
        return "配置条目超过安全上限";
    std::set<std::pair<unsigned, unsigned>> keys;
    for (const auto& h : value.hotkeys) {
        if (h.modifiers > 15 || h.key == 0 || h.key > 254 || !valid_action(h.action) || h.action == Action::None)
            return "快捷键参数无效；清空映射请删除该条目";
        if (h.key == 0x7b || (h.key == 0x7a && h.modifiers == 6))
            return "F12 为系统保留；Ctrl+Shift+F11 为紧急暂停保留";
        if (!keys.emplace(h.modifiers, h.key).second) return "快捷键映射冲突";
    }
    std::set<std::tuple<int, int, int, unsigned>> buttons;
    for (const auto& m : value.mouse_bindings) {
        if (m.region < 0 || m.region > 8 || m.button < 1 || m.button > 5 ||
            m.clicks < 1 || m.clicks > 2 || m.modifiers > 15 || !valid_action(m.action) || m.action == Action::None)
            return "鼠标映射参数无效；仅支持九区、五键、单击或双击";
        if (!buttons.emplace(m.region, m.button, m.clicks, m.modifiers).second) return "鼠标映射冲突";
    }
    std::set<std::pair<std::string, std::string>> rules;
    // Conservative serialized-size budget. Every accepted snapshot must fit
    // the same bounded import format; a successful validation cannot later
    // produce an empty serialization merely because many long rules expand.
    std::size_t file_budget = 2048 + value.hotkeys.size() * 64 + value.mouse_bindings.size() * 64;
    for (const auto& r : value.rules) {
        if (r.exe.empty() || !rule_text(r.exe, 1024) || !rule_text(r.window_class, 256))
            return "应用规则需要有效的 UTF-8 程序名称/路径及可选类名";
        if (r.exe.find_first_of("*?\"<>|") != std::string::npos)
            return "应用规则不支持通配符或模糊路径";
        if (!rules.emplace(ascii_fold(r.exe), ascii_case_fold(r.window_class)).second)
            return "应用规则重复；路径匹配不区分 ASCII 大小写和斜杠方向";
        file_budget += escape(r.exe).size() + escape(r.window_class).size() + 64;
    }
    if (file_budget > max_file_bytes) return "配置及转义后的应用规则超过 256 KiB 安全上限";
    return {};
}

// File format v1: UTF-8 key=value lines with explicit list counts. All scalar
// fields and all eight dock entries are required, so a partial/truncated save
// cannot silently restore defaults. Rule strings percent-escape separators.
// No window title, document content, token, or transient HWND is stored here.
inline std::string serialize(const Settings& value) {
    if (!validate(value).empty()) return {};
    std::string out = "# PaneBind configuration v1; UTF-8; modifiers Alt=1 Ctrl=2 Shift=4 Super=8\n";
    auto line = [&](std::string_view key, std::string_view data) {
        out += key; out += '='; out += data; out += '\n';
    };
    line("schema", std::to_string(value.schema));
    line("generation", std::to_string(value.generation));
    for (const auto& f : settings_detail::bool_fields) line(f.name, value.*(f.member) ? "true" : "false");
    for (const auto& f : settings_detail::int_fields) line(f.name, std::to_string(value.*(f.member)));
    for (std::size_t i = 0; i < value.dock_actions.size(); ++i)
        line("dock." + std::to_string(i), action_name(value.dock_actions[i]));
    line("hotkey_count", std::to_string(value.hotkeys.size()));
    for (std::size_t i = 0; i < value.hotkeys.size(); ++i) {
        const auto& h = value.hotkeys[i];
        line("hotkey." + std::to_string(i), std::to_string(h.modifiers) + "," + std::to_string(h.key) + "," + std::string(action_name(h.action)));
    }
    line("mouse_count", std::to_string(value.mouse_bindings.size()));
    for (std::size_t i = 0; i < value.mouse_bindings.size(); ++i) {
        const auto& m = value.mouse_bindings[i];
        line("mouse." + std::to_string(i), std::to_string(m.region) + "," + std::to_string(m.button) + "," +
            std::to_string(m.clicks) + "," + std::to_string(m.modifiers) + "," + std::string(action_name(m.action)));
    }
    line("rule_count", std::to_string(value.rules.size()));
    for (std::size_t i = 0; i < value.rules.size(); ++i) {
        const auto& r = value.rules[i];
        line("rule." + std::to_string(i), settings_detail::escape(r.exe) + "|" + settings_detail::escape(r.window_class) + "|" +
            (r.excluded ? "true" : "false") + "|" + std::to_string(r.feature_mask));
    }
    return out.size() <= settings_detail::max_file_bytes ? out : std::string{};
}

struct ParseResult { std::optional<Settings> value; std::string error; };
inline ParseResult parse(std::string_view text) {
    using namespace settings_detail;
    auto fail = [](std::string reason) -> ParseResult { return {std::nullopt, std::move(reason)}; };
    if (text.empty() || text.size() > max_file_bytes) return fail("配置为空或超过 256 KiB 上限");
    if (!utf8(text)) return fail("配置不是有效 UTF-8 或含 NUL");
    if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    std::map<std::string, std::string> fields;
    std::size_t line_number{};
    while (!text.empty()) {
        ++line_number;
        const auto end = text.find('\n');
        auto row = text.substr(0, end);
        if (end == std::string_view::npos) text = {};
        else text.remove_prefix(end + 1);
        if (row.ends_with('\r')) row.remove_suffix(1);
        if (row.size() > 4096 || row.find('\r') != std::string_view::npos)
            return fail("配置行过长或行分隔符无效；行 " + std::to_string(line_number));
        row = trim(row);
        if (row.empty() || row.starts_with('#')) continue;
        const auto separator = row.find('=');
        if (separator == std::string_view::npos) return fail("配置缺少等号；行 " + std::to_string(line_number));
        const auto key = trim(row.substr(0, separator));
        const auto data = trim(row.substr(separator + 1));
        if (key.empty() || key.size() > 64 || !fields.emplace(std::string(key), std::string(data)).second)
            return fail("配置键为空、过长或重复；行 " + std::to_string(line_number));
    }
    auto take = [&](std::string_view key) -> std::optional<std::string> {
        const auto it = fields.find(std::string(key));
        if (it == fields.end()) return std::nullopt;
        auto value = std::move(it->second); fields.erase(it); return value;
    };
    auto get_number = [&](std::string_view key, auto& out) {
        const auto value = take(key); return value && number(*value, out);
    };
    auto get_bool = [&](std::string_view key, bool& out) {
        const auto value = take(key);
        if (!value || (*value != "true" && *value != "false")) return false;
        out = *value == "true"; return true;
    };
    Settings value;
    if (!get_number("schema", value.schema) || value.schema != 1) return fail("不支持或缺少 schema；原配置未更改");
    if (!get_number("generation", value.generation)) return fail("缺少或无效 generation");
    for (const auto& f : bool_fields) if (!get_bool(f.name, value.*(f.member))) return fail("缺少或无效开关：" + std::string(f.name));
    for (const auto& f : int_fields) if (!get_number(f.name, value.*(f.member))) return fail("缺少或无效参数：" + std::string(f.name));
    for (std::size_t i = 0; i < value.dock_actions.size(); ++i) {
        const auto raw = take("dock." + std::to_string(i));
        const auto action = raw ? action_from_name(*raw) : std::nullopt;
        if (!action) return fail("缺少或无效停靠动作");
        value.dock_actions[i] = *action;
    }
    std::size_t count{};
    if (!get_number("hotkey_count", count) || count > max_hotkeys) return fail("缺少或无效 hotkey_count");
    for (std::size_t i = 0; i < count; ++i) {
        const auto raw = take("hotkey." + std::to_string(i));
        if (!raw) return fail("快捷键列表不完整");
        const auto parts = split(*raw, ','); Hotkey h;
        if (parts.size() != 3 || !number(parts[0], h.modifiers) || !number(parts[1], h.key)) return fail("快捷键条目格式无效");
        const auto action = action_from_name(parts[2]);
        if (!action) return fail("未知快捷键动作");
        h.action = *action; value.hotkeys.push_back(h);
    }
    if (!get_number("mouse_count", count) || count > max_mouse) return fail("缺少或无效 mouse_count");
    for (std::size_t i = 0; i < count; ++i) {
        const auto raw = take("mouse." + std::to_string(i));
        if (!raw) return fail("鼠标映射列表不完整");
        const auto parts = split(*raw, ','); MouseBinding m;
        if (parts.size() != 5 || !number(parts[0], m.region) || !number(parts[1], m.button) ||
            !number(parts[2], m.clicks) || !number(parts[3], m.modifiers)) return fail("鼠标映射条目格式无效");
        const auto action = action_from_name(parts[4]);
        if (!action) return fail("未知鼠标动作");
        m.action = *action; value.mouse_bindings.push_back(m);
    }
    if (!get_number("rule_count", count) || count > max_rules) return fail("缺少或无效 rule_count");
    for (std::size_t i = 0; i < count; ++i) {
        const auto raw = take("rule." + std::to_string(i));
        if (!raw) return fail("应用规则列表不完整");
        const auto parts = split(*raw, '|'); Rule r;
        if (parts.size() != 4 || (parts[2] != "true" && parts[2] != "false") || !number(parts[3], r.feature_mask))
            return fail("应用规则条目格式无效");
        const auto exe = unescape(parts[0]), cls = unescape(parts[1]);
        if (!exe || !cls) return fail("应用规则转义无效");
        r.exe = *exe; r.window_class = *cls; r.excluded = parts[2] == "true"; value.rules.push_back(std::move(r));
    }
    if (!fields.empty()) return fail("未知配置字段：" + fields.begin()->first);
    if (auto error = validate(value); !error.empty()) return fail(std::move(error));
    return {std::move(value), {}};
}

} // namespace panebind::core::v1
