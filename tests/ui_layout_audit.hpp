#pragma once

#include "ui_layout_seed.hpp"

namespace hxlings_test {

struct AuditContext {
    std::vector<std::string> anomalies;

    void Flag(const std::string& scope, const std::string& msg) {
        anomalies.push_back("[" + scope + "] " + msg);
    }
};

std::string FmtRect(const Rect& r) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1)
       << "{x=" << r.x << ", y=" << r.y << ", w=" << r.width << ", h=" << r.height << "}";
    return os.str();
}

bool IsFiniteRect(const Rect& r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) && std::isfinite(r.height) &&
           r.width >= 0.0F && r.height >= 0.0F;
}

void CheckSnapshotSanity(AuditContext& ctx, const std::string& scope, const UiTest& ui) {
    const std::string snap = ui.CaptureSnapshot().ToString();
    if (snap.empty()) {
        ctx.Flag(scope, "CaptureSnapshot().ToString() is empty");
        return;
    }
    const std::array<std::string_view, 5> bad_tokens{" nan", ": nan", " inf", ": inf", "-inf"};
    for (const auto tok : bad_tokens) {
        if (snap.find(tok) != std::string::npos) {
            ctx.Flag(scope, "Snapshot contains non-finite numeric token: '" + std::string(tok) + "'");
        }
    }
}

void CheckSplitPaneNodes(
    AuditContext& ctx,
    const std::string& scope,
    const UiNodeQuery& page_q,
    const Rect& left_bounds,
    const Rect& right_bounds
) {
    const float split_top = std::min(left_bounds.y, right_bounds.y) - 0.5F;

    auto check_list = [&](const std::string& kind_name, UiSelector sel) {
        for (const auto& n : page_q.Find(std::move(sel)).All()) {
            if (!n.participates_in_layout || !n.in_viewport) continue;
            if (!IsFiniteRect(n.bounds) || !IsFiniteRect(n.visible_bounds)) continue;
            if (n.bounds.y + n.bounds.height <= split_top) continue;

            const float cx = n.bounds.x + n.bounds.width * 0.5F;
            const bool in_left = cx < right_bounds.x;
            const Rect& pane = in_left ? left_bounds : right_bounds;
            const char* pane_label = in_left ? "split-left" : "split-right";
            const float min_x = pane.x - 1.0F;
            const float max_x = pane.x + pane.width + 1.0F;

            if (n.bounds.x < min_x || n.bounds.x + n.bounds.width > max_x) {
                std::string extra = n.text.has_value() ? (" text=\"" + n.text->substr(0, 48) + "\"") : "";
                ctx.Flag(scope, std::string(pane_label) + " " + kind_name + " horizontal overflow" + extra +
                                    " bounds=" + FmtRect(n.bounds) + " limit=[" + std::to_string(min_x) + ", " +
                                    std::to_string(max_x) + "]");
            }
            if (n.bounds.width > 1.0F && n.visible_bounds.width + 2.0F < n.bounds.width) {
                std::string extra = n.text.has_value() ? (" text=\"" + n.text->substr(0, 48) + "\"") : "";
                ctx.Flag(scope, std::string(pane_label) + " " + kind_name + " horizontally clipped" + extra +
                                    " bounds=" + FmtRect(n.bounds) + " visible=" + FmtRect(n.visible_bounds));
            }
        }
    };

    check_list("Text", UiSelector::Type<Text>());
    check_list("TextField", UiSelector::Type<TextField>());
    check_list("Checkbox", UiSelector::Type<Checkbox>());
    check_list("Switch", UiSelector::Type<Switch>());
    check_list("Image", UiSelector::Type<Image>());
}

void AuditCurrentScreen(
    AuditContext& ctx,
    const UiTest& ui,
    float viewport_w,
    float viewport_h,
    const std::string& screen_name,
    const std::string& active_page_key,
    bool has_split = false,
    bool modal_open = false
) {
    std::ostringstream sc;
    sc << static_cast<int>(viewport_w) << "x" << static_cast<int>(viewport_h) << " / " << screen_name;
    const std::string scope = sc.str();

    // 1. Shell sidebar nav buttons
    const std::array<const char*, 7> nav_keys{
        "nav-dashboard", "nav-packages", "nav-store", "nav-subos", "nav-luban", "nav-indexes", "nav-settings"
    };
    std::vector<UiNodeInfo> nav_nodes;
    for (const char* nk : nav_keys) {
        auto q = ui.Find(UiSelector::Key(nk));
        if (!q.Exists()) {
            ctx.Flag(scope, std::string("Missing nav button: ") + nk);
            continue;
        }
        UiNodeInfo info = q.One();
        if (!IsFiniteRect(info.bounds) || !info.participates_in_layout || !info.in_viewport) {
            ctx.Flag(scope, std::string("Nav button not laid out in viewport: ") + nk + " bounds=" + FmtRect(info.bounds));
        } else {
            if (info.bounds.x + info.bounds.width > 214.0F) {
                ctx.Flag(scope, std::string("Nav button exceeds sidebar width: ") + nk + " bounds=" + FmtRect(info.bounds));
            }
            if (info.visible_bounds.width + 2.0F < info.bounds.width) {
                ctx.Flag(scope, std::string("Nav button clipped horizontally: ") + nk + " bounds=" + FmtRect(info.bounds));
            }
            nav_nodes.push_back(info);
        }
    }
    for (std::size_t i = 0; i + 1 < nav_nodes.size(); ++i) {
        if (nav_nodes[i].bounds.y + nav_nodes[i].bounds.height > nav_nodes[i + 1].bounds.y + 0.5F) {
            ctx.Flag(scope, "Nav buttons overlap vertically between index " + std::to_string(i) + " and " + std::to_string(i + 1));
        }
    }

    // 2. Active page bounds
    std::optional<Rect> active_page_bounds;
    if (!active_page_key.empty()) {
        auto page_q = ui.Find(UiSelector::Key(active_page_key));
        if (!page_q.Exists()) {
            ctx.Flag(scope, "Missing active page key: " + active_page_key);
        } else {
            const UiNodeInfo page_info = page_q.One();
            if (!IsFiniteRect(page_info.bounds) || !page_info.participates_in_layout) {
                ctx.Flag(scope, "Active page does not participate in layout: " + active_page_key);
            } else {
                active_page_bounds = page_info.bounds;
                if (page_info.bounds.x < 210.0F ||
                    page_info.bounds.x + page_info.bounds.width > viewport_w + 0.5F ||
                    page_info.bounds.y + page_info.bounds.height > viewport_h + 0.5F ||
                    page_info.bounds.width < 600.0F || page_info.bounds.height < 200.0F) {
                    ctx.Flag(scope, "Active page out of expected workspace bounds (" + active_page_key + "): " + FmtRect(page_info.bounds));
                }
            }

            if (has_split) {
                const auto left_matches = page_q.Find(UiSelector::Key("split-left")).All();
                const auto right_matches = page_q.Find(UiSelector::Key("split-right")).All();
                if (left_matches.empty() || right_matches.empty()) {
                    ctx.Flag(scope, "SplitLayout panes missing in " + active_page_key);
                } else {
                    const UiNodeInfo& left_info = left_matches.front();
                    const UiNodeInfo& right_info = right_matches.front();
                    if (!left_info.participates_in_layout || !right_info.participates_in_layout) {
                        ctx.Flag(scope, "Split pane not participating in layout in " + active_page_key);
                    } else {
                        if (left_info.bounds.width < 200.0F || left_info.bounds.height < 200.0F ||
                            right_info.bounds.width < 200.0F || right_info.bounds.height < 200.0F) {
                            ctx.Flag(scope, "Collapsed split pane in " + active_page_key +
                                                " left=" + FmtRect(left_info.bounds) +
                                                " right=" + FmtRect(right_info.bounds));
                        }
                        if (left_info.bounds.x + left_info.bounds.width > right_info.bounds.x + 1.5F) {
                            ctx.Flag(scope, "Split panes overlap horizontally: left=" + FmtRect(left_info.bounds) +
                                                " right=" + FmtRect(right_info.bounds));
                        }
                        if (right_info.bounds.x + right_info.bounds.width > viewport_w + 1.0F) {
                            ctx.Flag(scope, "Split right pane overflows viewport: " + FmtRect(right_info.bounds));
                        }
                        CheckSplitPaneNodes(ctx, scope, page_q, left_info.bounds, right_info.bounds);
                    }
                }
            }
        }
    }

    // 3. Global participating node geometry & horizontal overflow / clipping
    auto check_global_type = [&](const std::string& type_name, UiSelector sel) {
        for (const auto& n : ui.Find(std::move(sel)).All()) {
            if (!n.participates_in_layout) continue;
            if (!IsFiniteRect(n.bounds) || !IsFiniteRect(n.visible_bounds)) {
                ctx.Flag(scope, "Non-finite " + type_name + " bounds: " + FmtRect(n.bounds));
                continue;
            }
            if (!n.in_viewport) continue;
            if (n.bounds.x < -1.0F || n.bounds.x + n.bounds.width > viewport_w + 1.0F) {
                std::string extra = n.text.has_value() ? (" text=\"" + n.text->substr(0, 48) + "\"") : "";
                ctx.Flag(scope, type_name + " horizontal viewport overflow" + extra +
                                    " bounds=" + FmtRect(n.bounds));
            }
            if (type_name == "Text") {
                if (n.text.has_value() && !n.text->empty()) {
                    if (n.bounds.width <= 0.5F || n.bounds.height <= 0.5F) {
                        ctx.Flag(scope, "Zero-sized Text \"" + n.text->substr(0, 48) + "\" bounds=" + FmtRect(n.bounds));
                    } else if (n.visible_bounds.width + 2.0F < n.bounds.width) {
                        ctx.Flag(scope, "Horizontally clipped Text \"" + n.text->substr(0, 48) + "\" bounds=" +
                                            FmtRect(n.bounds) + " visible=" + FmtRect(n.visible_bounds));
                    }
                }
            } else if (type_name == "TextField") {
                if (n.bounds.width < 16.0F || n.bounds.height < 16.0F) {
                    ctx.Flag(scope, "Collapsed TextField bounds=" + FmtRect(n.bounds));
                } else if (n.visible_bounds.width + 2.0F < n.bounds.width) {
                    ctx.Flag(scope, "Horizontally clipped TextField bounds=" + FmtRect(n.bounds) +
                                        " visible=" + FmtRect(n.visible_bounds));
                }
            }
        }
    };

    check_global_type("Text", UiSelector::Type<Text>());
    check_global_type("TextField", UiSelector::Type<TextField>());
    check_global_type("Checkbox", UiSelector::Type<Checkbox>());
    check_global_type("Switch", UiSelector::Type<Switch>());
    check_global_type("Image", UiSelector::Type<Image>());

    // 4. Button semantics bounds & overlap check
    const Rect viewport_rect{0.0F, 0.0F, viewport_w, viewport_h};
    struct BtnBox {
        std::string label;
        Rect rect;
    };
    std::vector<BtnBox> visible_buttons;
    const auto all_buttons = ui.FindSemantics(UiSemanticSelector::Role(SemanticRole::Button)).All();
    for (const auto& btn : all_buttons) {
        if (!IsFiniteRect(btn.bounds)) {
            ctx.Flag(scope, "Button '" + btn.label + "' has non-finite bounds: " + FmtRect(btn.bounds));
            continue;
        }
        if (btn.bounds.IsEmpty() || btn.offscreen) continue;
        const Rect vis = btn.bounds.Intersection(viewport_rect);
        if (vis.IsEmpty()) continue;

        if (btn.bounds.x < -1.0F || btn.bounds.x + btn.bounds.width > viewport_w + 1.0F) {
            ctx.Flag(scope, "Button '" + btn.label + "' overflows viewport horizontally: bounds=" + FmtRect(btn.bounds));
        }
        if (btn.bounds.width < 16.0F || btn.bounds.height < 16.0F) {
            ctx.Flag(scope, "Collapsed Button '" + btn.label + "' bounds=" + FmtRect(btn.bounds));
        }
        if (vis.width + 2.0F < btn.bounds.width) {
            ctx.Flag(scope, "Button '" + btn.label + "' horizontally clipped by viewport: bounds=" + FmtRect(btn.bounds));
        }
        Rect effective = vis;
        if (active_page_bounds.has_value() &&
            btn.bounds.x >= active_page_bounds->x - 1.0F &&
            btn.bounds.y >= active_page_bounds->y - 1.0F &&
            btn.bounds.y < active_page_bounds->y + active_page_bounds->height) {
            effective = effective.Intersection(*active_page_bounds);
            if (effective.IsEmpty()) continue;
        }
        visible_buttons.push_back({btn.label, effective});
    }

    if (!modal_open) {
        auto contains = [](const Rect& outer, const Rect& inner) {
            return inner.x >= outer.x - 0.5F &&
                   inner.y >= outer.y - 0.5F &&
                   inner.x + inner.width <= outer.x + outer.width + 0.5F &&
                   inner.y + inner.height <= outer.y + outer.height + 0.5F;
        };
        for (std::size_t i = 0; i < visible_buttons.size(); ++i) {
            for (std::size_t j = i + 1; j < visible_buttons.size(); ++j) {
                const Rect inter = visible_buttons[i].rect.Intersection(visible_buttons[j].rect);
                if (inter.width * inter.height > 4.0F) {
                    if (contains(visible_buttons[i].rect, visible_buttons[j].rect) ||
                        contains(visible_buttons[j].rect, visible_buttons[i].rect)) {
                        ctx.Flag(scope, "Nested buttons detected: '" + visible_buttons[i].label + "' " +
                                            FmtRect(visible_buttons[i].rect) + " and '" +
                                            visible_buttons[j].label + "' " + FmtRect(visible_buttons[j].rect));
                    } else {
                        ctx.Flag(scope, "Overlapping buttons: '" + visible_buttons[i].label + "' " +
                                            FmtRect(visible_buttons[i].rect) + " vs '" +
                                            visible_buttons[j].label + "' " + FmtRect(visible_buttons[j].rect) +
                                            " inter=" + FmtRect(inter));
                    }
                }
            }
        }
    }

    // 5. Snapshot sanity
    CheckSnapshotSanity(ctx, scope, ui);
}

bool SafeTap(AuditContext& ctx, const std::string& scope, const UiNodeQuery& q, const std::string& desc) {
    try {
        q.Tap();
        return true;
    } catch (const UiTestFailure& ex) {
        ctx.Flag(scope, "Tap failed for " + desc + ": " + ex.what());
        return false;
    }
}

} // namespace hxlings_test
