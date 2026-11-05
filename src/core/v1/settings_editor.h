#pragma once

#include "core/v1/settings.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace panebind::core::v1::settings_editor {

// User-facing action names are shared by the editor, lists and help. Values
// remain the existing Action/schema values; this is not a second mapping model.
inline constexpr std::array<std::wstring_view, 37> action_labels{
    L"不执行", L"最小化", L"最大化", L"恢复", L"居中", L"置后",
    L"向左微移", L"向右微移", L"向上微移", L"向下微移", L"减小宽度", L"增加宽度", L"减小高度", L"增加高度",
    L"移到最近相邻显示器", L"切换置顶", L"降低不透明度", L"提高不透明度",
    L"左半屏", L"右半屏", L"上半屏", L"下半屏", L"左上四分屏", L"右上四分屏", L"左下四分屏", L"右下四分屏",
    L"向左拉伸", L"向右拉伸", L"向上拉伸", L"向下拉伸", L"水平双向拉伸", L"垂直双向拉伸", L"双轴拉伸",
    L"撤销布局", L"暂停／恢复", L"打开设置", L"退出"};
inline std::wstring_view action_label(Action action) {
    return settings_detail::valid_action(action) ? action_labels[static_cast<std::size_t>(action)] : L"未知动作";
}
inline constexpr std::array<std::wstring_view, 9> region_labels{
    L"标题栏", L"左边", L"右边", L"上边", L"下边", L"左上角", L"右上角", L"左下角", L"右下角"};
inline constexpr std::array<std::wstring_view, 5> button_labels{L"左键", L"右键", L"中键", L"侧键1", L"侧键2"};
inline std::wstring_view region_label(int value) {
    return value >= 0 && value < static_cast<int>(region_labels.size()) ? region_labels[static_cast<std::size_t>(value)] : L"未知区域";
}
inline std::wstring_view button_label(int value) {
    return value > 0 && value <= static_cast<int>(button_labels.size()) ? button_labels[static_cast<std::size_t>(value - 1)] : L"未知按键";
}
inline std::wstring modifier_label(unsigned value) {
    std::wstring out;
    for (const auto& [bit, name] : std::array<std::pair<unsigned, std::wstring_view>, 4>{
             {{2, L"Ctrl"}, {1, L"Alt"}, {4, L"Shift"}, {8, L"Win"}}}) {
        if ((value & bit) != 0) { if (!out.empty()) out += L"+"; out += name; }
    }
    return out.empty() ? L"无" : out;
}
inline std::wstring key_label(unsigned key) {
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return {static_cast<wchar_t>(key)};
    if (key >= 0x70 && key <= 0x87) return L"F" + std::to_wstring(key - 0x70 + 1);
    if (key >= 0x60 && key <= 0x69) return L"数字键盘 " + std::to_wstring(key - 0x60);
    switch (key) {
    case 0x08: return L"Backspace"; case 0x09: return L"Tab"; case 0x0d: return L"Enter";
    case 0x1b: return L"Esc"; case 0x20: return L"空格"; case 0x21: return L"Page Up";
    case 0x22: return L"Page Down"; case 0x23: return L"End"; case 0x24: return L"Home";
    case 0x25: return L"左方向键"; case 0x26: return L"上方向键";
    case 0x27: return L"右方向键"; case 0x28: return L"下方向键";
    case 0x2d: return L"Insert"; case 0x2e: return L"Delete";
    case 0x6a: return L"数字键盘 *"; case 0x6b: return L"数字键盘 +";
    case 0x6d: return L"数字键盘 -"; case 0x6e: return L"数字键盘 ."; case 0x6f: return L"数字键盘 /";
    case 0xba: return L";"; case 0xbb: return L"="; case 0xbc: return L",";
    case 0xbd: return L"-"; case 0xbe: return L"."; case 0xbf: return L"/";
    case 0xc0: return L"`"; case 0xdb: return L"["; case 0xdc: return L"\\";
    case 0xdd: return L"]"; case 0xde: return L"'";
    default: return L"现有按键（" + std::to_wstring(key) + L"）";
    }
}
inline std::wstring hotkey_label(const Hotkey& value) {
    return (value.modifiers ? modifier_label(value.modifiers) + L"+" : L"") + key_label(value.key);
}

// Each form first edits its own value. Only a successful confirmation replaces
// a row in the single Settings draft, leaving cancel and failed validation inert.
inline std::string put_hotkey(Settings& draft, std::optional<std::size_t> index, const Hotkey& value) {
    if (index && *index >= draft.hotkeys.size()) return "所选快捷键已不存在，请重新选择";
    if (value.key == 0x7b) return "F12 为系统保留，不能保存此快捷键";
    if (value.key == 0x7a && value.modifiers == 6) return "Ctrl+Shift+F11 为紧急暂停保留，不能保存此快捷键";
    for (std::size_t i = 0; i < draft.hotkeys.size(); ++i) {
        if (index && i == *index) continue;
        const auto& row = draft.hotkeys[i];
        if (row.modifiers == value.modifiers && row.key == value.key)
            return "快捷键组合与第 " + std::to_string(i + 1) + " 行重复，请编辑或删除原条目";
    }
    auto candidate = draft;
    if (index) candidate.hotkeys[*index] = value;
    else candidate.hotkeys.push_back(value);
    if (const auto error = validate(candidate); !error.empty()) return error;
    draft = std::move(candidate);
    return {};
}
inline std::string put_mouse(Settings& draft, std::optional<std::size_t> index, const MouseBinding& value) {
    if (index && *index >= draft.mouse_bindings.size()) return "所选鼠标操作已不存在，请重新选择";
    for (std::size_t i = 0; i < draft.mouse_bindings.size(); ++i) {
        if (index && i == *index) continue;
        const auto& row = draft.mouse_bindings[i];
        if (std::tie(row.region, row.button, row.clicks, row.modifiers) ==
            std::tie(value.region, value.button, value.clicks, value.modifiers))
            return "鼠标组合与第 " + std::to_string(i + 1) + " 行重复，请编辑或删除原条目";
    }
    auto candidate = draft;
    if (index) candidate.mouse_bindings[*index] = value;
    else candidate.mouse_bindings.push_back(value);
    if (const auto error = validate(candidate); !error.empty()) return error;
    draft = std::move(candidate);
    return {};
}
inline std::string put_rule(Settings& draft, std::optional<std::size_t> index, const Rule& value) {
    if (index && *index >= draft.rules.size()) return "所选应用规则已不存在，请重新选择";
    for (std::size_t i = 0; i < draft.rules.size(); ++i) {
        if (index && i == *index) continue;
        const auto& row = draft.rules[i];
        if (settings_detail::ascii_fold(row.exe) == settings_detail::ascii_fold(value.exe) &&
            settings_detail::ascii_case_fold(row.window_class) == settings_detail::ascii_case_fold(value.window_class))
            return "程序和窗口类与第 " + std::to_string(i + 1) + " 行重复，请编辑原规则";
    }
    auto candidate = draft;
    if (index) candidate.rules[*index] = value;
    else candidate.rules.push_back(value);
    if (const auto error = validate(candidate); !error.empty()) return error;
    draft = std::move(candidate);
    return {};
}
inline bool erase_hotkey(Settings& draft, std::size_t index) {
    if (index >= draft.hotkeys.size()) return false;
    draft.hotkeys.erase(draft.hotkeys.begin() + static_cast<std::ptrdiff_t>(index)); return true;
}
inline bool erase_mouse(Settings& draft, std::size_t index) {
    if (index >= draft.mouse_bindings.size()) return false;
    draft.mouse_bindings.erase(draft.mouse_bindings.begin() + static_cast<std::ptrdiff_t>(index)); return true;
}
inline bool erase_rule(Settings& draft, std::size_t index) {
    if (index >= draft.rules.size()) return false;
    draft.rules.erase(draft.rules.begin() + static_cast<std::ptrdiff_t>(index)); return true;
}
inline Rule rule_features(Rule original, bool excluded, std::uint32_t known_bits) {
    original.excluded = excluded;
    // Excluding does not discard the prior custom selection, including unknown
    // bits. A custom edit changes only the currently understood six feature bits.
    if (!excluded) original.feature_mask = (original.feature_mask & ~0x3fU) | (known_bits & 0x3fU);
    return original;
}

// Settings stores Win32 COLORREF order (00BBGGRR), not display RGB order.
inline std::string color_hex(int colorref) {
    constexpr std::string_view digits = "0123456789ABCDEF";
    const auto c = static_cast<unsigned>(colorref);
    const unsigned rgb = ((c & 0xffU) << 16U) | (c & 0xff00U) | ((c >> 16U) & 0xffU);
    std::string result(7, '#');
    for (unsigned i = 0; i < 6; ++i) result[i + 1] = digits[(rgb >> ((5U - i) * 4U)) & 15U];
    return result;
}
inline std::optional<int> parse_color_hex(std::string_view text) {
    if (text.size() != 7 || text[0] != '#') return std::nullopt;
    unsigned rgb{};
    const auto [last, error] = std::from_chars(text.data() + 1, text.data() + 7, rgb, 16);
    if (error != std::errc{} || last != text.data() + 7) return std::nullopt;
    return static_cast<int>(((rgb & 0xffU) << 16U) | (rgb & 0xff00U) | ((rgb >> 16U) & 0xffU));
}

inline void restore_page(Settings& draft, int page) {
    const auto base = default_settings();
    switch (page) {
    case 0:
        draft.enabled = base.enabled; draft.startup = base.startup; draft.animations = base.animations; break;
    case 1:
        draft.magnet_move = base.magnet_move; draft.magnet_resize = base.magnet_resize; draft.glue = base.glue;
        draft.attraction_dip = base.attraction_dip; draft.release_dip = base.release_dip;
        draft.speed_dip = base.speed_dip; draft.adjacency_dip = base.adjacency_dip; break;
    case 2:
        draft.docking = base.docking; draft.stretch = base.stretch;
        draft.smart_docking = base.smart_docking; draft.smart_stretch = base.smart_stretch;
        draft.preview = base.preview; draft.preview_alpha = base.preview_alpha; draft.preview_color = base.preview_color;
        draft.seam_dwell_ms = base.seam_dwell_ms; draft.dock_actions = base.dock_actions; break;
    case 3:
        draft.step_dip = base.step_dip; draft.hotkeys = base.hotkeys; draft.mouse_bindings = base.mouse_bindings; break;
    case 4: draft.rules = base.rules; break;
    case 5:
        draft.feedback = base.feedback; draft.drag_transparency = base.drag_transparency; draft.shake = base.shake;
        draft.drag_alpha = base.drag_alpha; draft.shake_alpha = base.shake_alpha;
        draft.shake_amplitude_dip = base.shake_amplitude_dip; draft.shake_reversals = base.shake_reversals;
        draft.shake_window_ms = base.shake_window_ms; break;
    default: break;
    }
}
inline void restore_all(Settings& draft) {
    const auto schema = draft.schema; const auto generation = draft.generation;
    draft = default_settings(); draft.schema = schema; draft.generation = generation;
}

} // namespace panebind::core::v1::settings_editor
