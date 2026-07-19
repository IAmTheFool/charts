# flux_charts

[![build-dev-sandbox](https://github.com/IAmTheFool/charts/actions/workflows/build-dev-sandbox.yml/badge.svg)](https://github.com/IAmTheFool/charts/actions/workflows/build-dev-sandbox.yml)

Chart widgets for [FluxUI](https://github.com/IAmTheFool/flux) — pie, donut, candlestick, and heatmap charts, with more chart types on the way.

`GraphWidget` (Line / Bar / Area / Scatter) already ships in FluxUI core and is accessible directly from there — this package adds the chart types core doesn't cover.

Header-only. No new build target, no linking — installing this package just adds an include path to your project.

Current version: **v0.1.0**

## Install

```bash
flux add charts
```

This resolves to the version pinned in flux's own package registry (`v0.1.0` as of this writing). To pin a specific version yourself:

```bash
flux add charts --ref v0.1.0
```

Then re-run your CMake configure step to pick it up.

## Usage

```cpp
#include "flux_charts/flux_charts.hpp"
```

### Pie chart

```cpp
auto pie = PieChart(360, 320)
               ->setTitle("Browser Share")
               ->addSlice("Chrome", 65, Color::fromRGB(66, 133, 244))
               ->addSlice("Safari", 18, Color::fromRGB(255, 149, 0))
               ->addSlice("Firefox", 9, Color::fromRGB(255, 90, 95))
               ->addSlice("Edge", 5, Color::fromRGB(0, 178, 148))
               ->addSlice("Other", 3, Color::fromRGB(140, 140, 150))
               ->setOnSliceClick([](int idx) { /* ... */ });
```

### Donut chart

Not a separate widget — a pie chart with a hole. `DonutChart()` is a thin factory over `FluxPieChartWidget` with `innerRadiusRatio` pre-set:

```cpp
auto donut = DonutChart(360, 320, /*innerRadiusRatio=*/0.6f)
                 ->setTitle("Storage Used")
                 ->addSlice("Photos", 42, Color::fromRGB(255, 193, 7))
                 ->addSlice("Videos", 30, Color::fromRGB(33, 150, 243))
                 ->addSlice("Docs", 12, Color::fromRGB(76, 175, 80))
                 ->addSlice("Free", 16, Color::fromRGB(60, 60, 70));
```

### Candlestick chart

```cpp
auto candles = CandlestickChart(420, 320)
                   ->setTitle("Price Action")
                   ->addCandle("Mon", /*open=*/100, /*high=*/108, /*low=*/97, /*close=*/105)
                   ->addCandle("Tue", 105, 110, 103, 102)
                   ->addCandle("Wed", 102, 106, 99, 104)
                   ->setOnCandleClick([](int idx) { /* ... */ });
```

### Heatmap chart

```cpp
auto heatmap = HeatmapChart(420, 320)
                   ->setTitle("Weekly Activity")
                   ->setColumnLabels({"Mon", "Tue", "Wed", "Thu", "Fri"})
                   ->addRow("Alice", {2, 5, 1, 8, 3})
                   ->addRow("Bob",   {6, 3, 7, 2, 9})
                   ->addRow("Carla", {1, 1, 4, 6, 2})
                   ->setOnCellClick([](int row, int col) { /* ... */ });
```

Cell color is a discrete banded scale (nearest color stop, not a smooth gradient) — set `->setColorStops({...})` with your own list to override the default cool-to-warm palette.

Every widget accepts a `ChartTheme` for palette/font-size overrides:

```cpp
ChartTheme theme = ChartTheme::defaults();
theme.titleColor = Color::fromRGB(255, 255, 255);
pie->setTheme(theme);
```

## Package structure

```
include/flux_charts/
├── flux_charts.hpp              # umbrella include — pulls in every chart widget
├── flux_chart_common.hpp        # shared tooltip/legend/title/theme helpers
├── flux_pie_chart.hpp           # FluxPieChartWidget (pie + donut)
├── flux_candlestick_chart.hpp   # FluxCandlestickChartWidget
└── flux_heatmap_chart.hpp       # FluxHeatmapChartWidget
```

Axis/grid layout code is intentionally **not** shared with core's `GraphWidget` — core can't depend on this package, so each cartesian chart type (like candlestick) implements its own axis math. What genuinely generalizes across chart shapes (tooltip, legend, title, theme defaults, auto-range-with-padding) lives in `flux_chart_common.hpp`.

## Roadmap

- [x] Pie / Donut
- [x] Candlestick
- [x] Heatmap

## Versioning

This package follows semver-style tags (`v0.1.0`, `v0.2.0`, ...). flux's package registry pins one tag at a time; if you need reproducibility independent of registry updates, pin explicitly with `flux add charts --ref <tag>`. Releases are cut via git tags matching `v*.*.*`, which triggers a build-gated GitHub Release (see `.github/workflows/release.yml`) — a tag is never published without the build passing on both CI platforms first.

## CI

Two platforms are tested: Windows (MSVC) and Linux (GCC). Every chart widget in this package is pure header-only logic written entirely against flux's already-cross-platform `Painter`/`Widget`/`FontCache` API, with no platform-specific code anywhere under `include/flux_charts/` — two different compilers/toolchains is what catches real portability bugs, and macOS/Android/Web/SSR would only be re-testing flux's own per-platform code, not this package's.

## Development

`dev/` is a local sandbox for working on chart widgets — a real, buildable FluxUI app (pinned via git submodule) that renders every widget in this package for visual testing. It is **not** part of the published package; a project that runs `flux add charts` never touches it.

```bash
git submodule update --init --recursive
cd dev
cmake -B build -S .
cmake --build build --target flux_app
```

## License

See [LICENSE](LICENSE).