export module hxlings.icons;

import std;
import huxerui;

using namespace huxerui;

namespace hxlings::detail {

StrokeStyle RoundStroke(float width = 2.0F) {
    return StrokeStyle{.width = width, .cap = StrokeCap::Round, .join = StrokeJoin::Round};
}

} // namespace hxlings::detail

export namespace hxlings::icons {

VectorAsset LogoIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        b.StrokePath(Path::RoundedRect({3.0F, 3.0F, 18.0F, 18.0F}, CornerRadii{5.0F}), w, detail::RoundStroke(1.9F));
        Path x1;
        x1.MoveTo({8.0F, 8.0F}).LineTo({16.0F, 16.0F});
        b.StrokePath(x1, w, detail::RoundStroke(2.2F));
        Path x2;
        x2.MoveTo({16.0F, 8.0F}).LineTo({8.0F, 16.0F});
        b.StrokePath(x2, w, detail::RoundStroke(2.2F));
    });
}

VectorAsset DashboardIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        b.FillPath(Path::RoundedRect({3.5F, 3.5F, 7.5F, 7.5F}, CornerRadii{2.0F}), w);
        b.FillPath(Path::RoundedRect({13.0F, 3.5F, 7.5F, 7.5F}, CornerRadii{2.0F}), w);
        b.FillPath(Path::RoundedRect({3.5F, 13.0F, 7.5F, 7.5F}, CornerRadii{2.0F}), w);
        b.FillPath(Path::RoundedRect({13.0F, 13.0F, 7.5F, 7.5F}, CornerRadii{2.0F}), w);
    });
}

VectorAsset PackagesIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(1.9F);
        Path hex;
        hex.MoveTo({12.0F, 2.5F})
            .LineTo({20.5F, 7.2F})
            .LineTo({20.5F, 16.8F})
            .LineTo({12.0F, 21.5F})
            .LineTo({3.5F, 16.8F})
            .LineTo({3.5F, 7.2F})
            .Close();
        b.StrokePath(hex, w, st);
        Path y1;
        y1.MoveTo({3.5F, 7.2F}).LineTo({12.0F, 12.0F}).LineTo({20.5F, 7.2F});
        b.StrokePath(y1, w, st);
        Path y2;
        y2.MoveTo({12.0F, 12.0F}).LineTo({12.0F, 21.5F});
        b.StrokePath(y2, w, st);
    });
}

VectorAsset StoreIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(1.9F);
        b.StrokePath(Path::RoundedRect({4.0F, 7.5F, 16.0F, 13.5F}, CornerRadii{2.5F}), w, st);
        Path handle;
        handle.MoveTo({8.5F, 7.5F})
            .LineTo({8.5F, 5.5F})
            .CubicTo({8.5F, 3.8F}, {15.5F, 3.8F}, {15.5F, 5.5F})
            .LineTo({15.5F, 7.5F});
        b.StrokePath(handle, w, st);
        Path plus_h;
        plus_h.MoveTo({9.0F, 14.2F}).LineTo({15.0F, 14.2F});
        b.StrokePath(plus_h, w, st);
        Path plus_v;
        plus_v.MoveTo({12.0F, 11.2F}).LineTo({12.0F, 17.2F});
        b.StrokePath(plus_v, w, st);
    });
}

VectorAsset SubosIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(1.9F);
        b.StrokePath(Path::RoundedRect({3.0F, 4.0F, 13.0F, 11.0F}, CornerRadii{2.5F}), w, st);
        b.StrokePath(Path::RoundedRect({8.0F, 9.0F, 13.0F, 11.0F}, CornerRadii{2.5F}), w, st);
    });
}

VectorAsset IndexIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(1.9F);
        b.StrokePath(Path::RoundedRect({4.0F, 3.5F, 16.0F, 5.0F}, CornerRadii{2.0F}), w, st);
        b.StrokePath(Path::RoundedRect({4.0F, 10.0F, 16.0F, 5.0F}, CornerRadii{2.0F}), w, st);
        b.StrokePath(Path::RoundedRect({4.0F, 16.5F, 16.0F, 4.5F}, CornerRadii{2.0F}), w, st);
    });
}

VectorAsset SettingsIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(1.9F);
        for (int i = 0; i < 3; ++i) {
            const float y = 6.0F + static_cast<float>(i) * 6.0F;
            Path line;
            line.MoveTo({3.5F, y}).LineTo({20.5F, y});
            b.StrokePath(line, w, st);
        }
        b.FillPath(Path::RoundedRect({12.5F, 3.5F, 5.0F, 5.0F}, CornerRadii{2.5F}), w);
        b.FillPath(Path::RoundedRect({6.5F, 9.5F, 5.0F, 5.0F}, CornerRadii{2.5F}), w);
        b.FillPath(Path::RoundedRect({13.5F, 15.5F, 5.0F, 5.0F}, CornerRadii{2.5F}), w);
    });
}

VectorAsset SunIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        b.FillPath(Path::RoundedRect({7.5F, 7.5F, 9.0F, 9.0F}, CornerRadii{4.5F}), w);
        const auto ray = detail::RoundStroke(2.0F);
        for (int i = 0; i < 8; ++i) {
            const double angle = i * std::numbers::pi / 4.0;
            const auto c = static_cast<float>(std::cos(angle));
            const auto s = static_cast<float>(std::sin(angle));
            Path p;
            p.MoveTo({12.0F + 7.0F * c, 12.0F + 7.0F * s})
                .LineTo({12.0F + 9.5F * c, 12.0F + 9.5F * s});
            b.StrokePath(p, w, ray);
        }
    });
}

VectorAsset MoonIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        Path p;
        p.MoveTo({20.0F, 12.0F})
            .ArcTo({8.0F, 8.0F}, 0.0F, ArcSize::Large, ArcDirection::Clockwise, {4.0F, 12.0F})
            .ArcTo({8.0F, 8.0F}, 0.0F, ArcSize::Large, ArcDirection::Clockwise, {20.0F, 12.0F})
            .Close();
        p.MoveTo({17.5F, 9.5F})
            .ArcTo({6.5F, 6.5F}, 0.0F, ArcSize::Large, ArcDirection::Clockwise, {4.5F, 9.5F})
            .ArcTo({6.5F, 6.5F}, 0.0F, ArcSize::Large, ArcDirection::Clockwise, {17.5F, 9.5F})
            .Close();
        b.FillPath(p, Color::White(), PathFillRule::EvenOdd);
    });
}

VectorAsset RefreshIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(2.0F);
        Path arc;
        arc.MoveTo({19.5F, 12.0F})
            .ArcTo({7.5F, 7.5F}, 0.0F, ArcSize::Large, ArcDirection::Clockwise, {6.5F, 6.8F});
        b.StrokePath(arc, w, st);
        Path arrow;
        arrow.MoveTo({15.0F, 4.5F}).LineTo({19.5F, 5.2F}).LineTo({18.5F, 9.5F});
        b.StrokePath(arrow, w, st);
    });
}

VectorAsset TerminalIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(1.9F);
        b.StrokePath(Path::RoundedRect({3.0F, 4.5F, 18.0F, 15.0F}, CornerRadii{2.5F}), w, st);
        Path prompt;
        prompt.MoveTo({7.0F, 9.5F}).LineTo({10.5F, 12.0F}).LineTo({7.0F, 14.5F});
        b.StrokePath(prompt, w, st);
        Path under;
        under.MoveTo({12.5F, 14.5F}).LineTo({17.0F, 14.5F});
        b.StrokePath(under, w, st);
    });
}

VectorAsset SearchIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(2.0F);
        b.StrokePath(Path::RoundedRect({4.0F, 4.0F, 12.0F, 12.0F}, CornerRadii{6.0F}), w, st);
        Path tail;
        tail.MoveTo({14.5F, 14.5F}).LineTo({20.0F, 20.0F});
        b.StrokePath(tail, w, st);
    });
}

VectorAsset PlusIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(2.2F);
        Path h;
        h.MoveTo({5.0F, 12.0F}).LineTo({19.0F, 12.0F});
        b.StrokePath(h, w, st);
        Path v;
        v.MoveTo({12.0F, 5.0F}).LineTo({12.0F, 19.0F});
        b.StrokePath(v, w, st);
    });
}

VectorAsset TrashIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(1.9F);
        Path top;
        top.MoveTo({4.5F, 7.0F}).LineTo({19.5F, 7.0F});
        b.StrokePath(top, w, st);
        Path cap;
        cap.MoveTo({9.0F, 7.0F}).LineTo({9.5F, 4.5F}).LineTo({14.5F, 4.5F}).LineTo({15.0F, 7.0F});
        b.StrokePath(cap, w, st);
        b.StrokePath(Path::RoundedRect({6.5F, 7.0F, 11.0F, 13.0F}, CornerRadii{2.0F}), w, st);
    });
}

VectorAsset CheckIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto st = detail::RoundStroke(2.3F);
        Path p;
        p.MoveTo({5.0F, 12.5F}).LineTo({10.0F, 17.5F}).LineTo({19.0F, 7.0F});
        b.StrokePath(p, Color::White(), st);
    });
}

VectorAsset WarningIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(2.0F);
        Path tri;
        tri.MoveTo({12.0F, 3.5F}).LineTo({21.0F, 19.5F}).LineTo({3.0F, 19.5F}).Close();
        b.StrokePath(tri, w, st);
        Path line;
        line.MoveTo({12.0F, 9.5F}).LineTo({12.0F, 14.0F});
        b.StrokePath(line, w, st);
        b.FillPath(Path::RoundedRect({11.0F, 16.0F, 2.0F, 2.0F}, CornerRadii{1.0F}), w);
    });
}

VectorAsset PlayIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        Path p;
        p.MoveTo({7.0F, 5.0F}).LineTo({19.0F, 12.0F}).LineTo({7.0F, 19.0F}).Close();
        b.FillPath(p, Color::White());
    });
}

VectorAsset CopyIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(1.8F);
        b.StrokePath(Path::RoundedRect({8.0F, 8.0F, 11.5F, 12.5F}, CornerRadii{2.0F}), w, st);
        Path back;
        back.MoveTo({15.0F, 8.0F})
            .LineTo({15.0F, 5.0F})
            .LineTo({5.5F, 5.0F})
            .LineTo({5.5F, 16.5F})
            .LineTo({8.0F, 16.5F});
        b.StrokePath(back, w, st);
    });
}

VectorAsset WrenchIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto st = detail::RoundStroke(2.0F);
        Path p;
        p.MoveTo({14.5F, 5.5F})
            .LineTo({18.5F, 9.5F})
            .LineTo({16.0F, 12.0F})
            .LineTo({7.5F, 20.5F})
            .LineTo({4.5F, 17.5F})
            .LineTo({13.0F, 9.0F})
            .Close();
        b.StrokePath(p, Color::White(), st);
    });
}

VectorAsset DownloadIcon() {
    return VectorAsset::Create({24.0F, 24.0F}, [](VectorBuilder& b) {
        const auto w = Color::White();
        const auto st = detail::RoundStroke(2.0F);
        Path shaft;
        shaft.MoveTo({12.0F, 4.0F}).LineTo({12.0F, 15.0F});
        b.StrokePath(shaft, w, st);
        Path head;
        head.MoveTo({7.5F, 11.0F}).LineTo({12.0F, 15.5F}).LineTo({16.5F, 11.0F});
        b.StrokePath(head, w, st);
        Path tray;
        tray.MoveTo({4.5F, 18.5F}).LineTo({19.5F, 18.5F});
        b.StrokePath(tray, w, st);
    });
}

} // namespace hxlings::icons
