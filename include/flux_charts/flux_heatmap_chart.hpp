#pragma once

// ============================================================================
// flux_heatmap_chart.hpp  —  FluxHeatmapChartWidget
//
// Grid of cells, each colored by value against a discrete color scale
// (nearest-stop banding, NOT smooth RGB interpolation). This is a
// deliberate choice, not a shortcut: Color's internal representation
// (component fields/accessors) is never exposed anywhere in the codebase
// this package has seen — only factory functions (fromRGB/fromRGBA) and a
// couple of named methods (darken/withAlpha). Interpolating between two
// Colors would require guessing at internals that might not exist.
// Nearest-stop banding needs no component access at all, and is itself a
// standard heatmap style (e.g. GitHub's contribution graph).
//
// Cell text contrast (when showValues is on) is decided from the already-
// known normalized value `t`, not from the rendered Color — same reasoning,
// zero dependency on Color internals.
// ============================================================================

#include "flux_chart_common.hpp"
#include "flux/flux_core.hpp"
#include "flux/flux_state.hpp"
#include "flux/flux_widget.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// ============================================================================
// FluxHeatmapChartWidget
// ============================================================================

class FluxHeatmapChartWidget : public Widget
{
public:
  // values[row][col]. Rows may have differing lengths defensively — a row
  // shorter than colLabels.size() just leaves those cells undrawn rather
  // than crashing.
  std::vector<std::vector<float>> values;
  std::vector<std::string> rowLabels;
  std::vector<std::string> colLabels;

  std::string title;
  ChartTheme theme = ChartTheme::defaults();

  // Color scale: nearest-stop banding, low-to-high. Defaults to a cool ->
  // warm 5-stop scale; replace via setColorStops() for anything else
  // (single-hue intensity, diverging scales, etc).
  std::vector<Color> colorStops = {
      Color::fromRGB(33, 45, 80),
      Color::fromRGB(46, 91, 145),
      Color::fromRGB(76, 175, 80),
      Color::fromRGB(255, 193, 7),
      Color::fromRGB(217, 60, 60),
  };

  bool showValues = true;
  bool autoRange = true;
  float valueMin = 0.f, valueMax = 1.f;

  // Callback: called with (row, col) on click.
  std::function<void(int, int)> onCellClick;

  FluxHeatmapChartWidget()
  {
    autoWidth = false;
    autoHeight = false;
    width = 420;
    height = 320;
    isFocusable = true;
  }

  // ── Fluent API ─────────────────────────────────────────────────────────

  std::shared_ptr<FluxHeatmapChartWidget>
  setGrid(std::vector<std::vector<float>> v, std::vector<std::string> rows,
          std::vector<std::string> cols)
  {
    values = std::move(v);
    rowLabels = std::move(rows);
    colLabels = std::move(cols);
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget>
  addRow(const std::string &label, std::vector<float> rowValues)
  {
    rowLabels.push_back(label);
    values.push_back(std::move(rowValues));
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget> setColumnLabels(std::vector<std::string> cols)
  {
    colLabels = std::move(cols);
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget> setTitle(const std::string &t)
  {
    title = t;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget> setTheme(const ChartTheme &t)
  {
    theme = t;
    markNeedsPaint();
    return self();
  }

  /// Replace the default 5-stop cool->warm scale. Needs at least 2 stops;
  /// values are banded to the nearest stop, not interpolated between them.
  std::shared_ptr<FluxHeatmapChartWidget> setColorStops(std::vector<Color> stops)
  {
    if (stops.size() >= 2)
      colorStops = std::move(stops);
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget> setValueRange(float mn, float mx)
  {
    valueMin = mn;
    valueMax = mx;
    autoRange = false;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget> setShowValues(bool v)
  {
    showValues = v;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget> setOnCellClick(std::function<void(int, int)> cb)
  {
    onCellClick = std::move(cb);
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget> setSize(int w, int h)
  {
    width = w;
    height = h;
    autoWidth = autoHeight = false;
    markNeedsLayout();
    return self();
  }

  std::shared_ptr<FluxHeatmapChartWidget> clearGrid()
  {
    values.clear();
    rowLabels.clear();
    colLabels.clear();
    markNeedsPaint();
    return self();
  }

  // ── Widget overrides ───────────────────────────────────────────────────

  void computeLayout(GraphicsContext & /*ctx*/,
                     const BoxConstraints &constraints,
                     FontCache & /*fontCache*/) override
  {
    if (!autoWidth)
      width = (constraints.maxWidth < width && constraints.maxWidth > 0)
                  ? constraints.maxWidth
                  : width;
    else
      width = constraints.clampWidth(constraints.maxWidth);

    if (!autoHeight)
      height = (constraints.maxHeight < height && constraints.maxHeight > 0)
                   ? constraints.maxHeight
                   : height;
    else
      height = constraints.clampHeight(constraints.maxHeight);

    applyConstraints();
    needsLayout = false;
  }

  void render(GraphicsContext &ctx, FontCache &fontCache) override
  {
    if (!visible || width <= 0 || height <= 0)
      return;

    computeRange();

    Painter p(ctx, this);

    p.fillRoundedRect(x, y, width, height, borderRadius > 0 ? borderRadius : 6,
                      theme.bgColor);
    p.pushClipRect(x, y, width, height);

    int numRows = static_cast<int>(rowLabels.size());
    int numCols = static_cast<int>(colLabels.size());

    int marginL = computeMarginL(fontCache);
    int marginR = 12;
    int marginT = (!title.empty() ? theme.titleFontSize + 10 : 0) + 10 +
                  (numCols > 0 ? theme.labelFontSize + 8 : 0);
    int marginB = 8;

    PlotArea pa{x + marginL, y + marginT,
               width - marginL - marginR, height - marginT - marginB};

    if (pa.w > 0 && pa.h > 0 && numRows > 0 && numCols > 0)
    {
      cachedPlotArea_ = pa;
      cachedRows_ = numRows;
      cachedCols_ = numCols;
      cachedPlotAreaValid_ = true;

      drawCells(p, fontCache, pa, numRows, numCols);
      drawRowLabels(p, fontCache, pa, numRows);
      drawColumnLabels(p, fontCache, pa, numCols);
    }

    drawChartTitle(p, this, fontCache, title, theme);
    drawChartTooltip(p, this, fontCache, tooltip_, theme);

    p.popClipRect();

    if (hasBorder)
      p.drawBorder(x, y, width, height, borderRadius > 0 ? borderRadius : 6,
                  getCurrentBorderColor(), borderWidth);

    needsPaint = false;
  }

  bool handleMouseMove(int mx, int my) override
  {
    if (!chartContains(this, mx, my))
    {
      if (tooltip_.visible)
      {
        tooltip_.visible = false;
        markNeedsPaint();
      }
      return false;
    }

    int row = -1, col = -1;
    hitTestCell(mx, my, row, col);
    bool changed = (row != hoveredRow_ || col != hoveredCol_);
    hoveredRow_ = row;
    hoveredCol_ = col;

    if (row >= 0 && col >= 0)
    {
      float v = cellValue(row, col);
      char buf[128];
      std::snprintf(buf, sizeof(buf), "%s / %s: %.2f",
                   rowLabels[row].c_str(), colLabels[col].c_str(), v);
      updateChartTooltip(tooltip_, mx, my, buf);
      changed = true;
    }
    else
    {
      if (tooltip_.visible)
        changed = true;
      tooltip_.visible = false;
    }

    if (changed)
      markNeedsPaint();
    return true;
  }

  bool handleMouseDown(int mx, int my) override
  {
    if (!chartContains(this, mx, my) || !onCellClick)
      return false;
    int row = -1, col = -1;
    hitTestCell(mx, my, row, col);
    if (row >= 0 && col >= 0)
    {
      onCellClick(row, col);
      return true;
    }
    return false;
  }

  bool handleMouseLeave() override
  {
    bool changed = tooltip_.visible || hoveredRow_ >= 0 || hoveredCol_ >= 0;
    tooltip_.visible = false;
    hoveredRow_ = hoveredCol_ = -1;
    if (changed)
      markNeedsPaint();
    return false;
  }

private:
  struct PlotArea
  {
    int x, y, w, h;
  };

  mutable PlotArea cachedPlotArea_{0, 0, 0, 0};
  mutable int cachedRows_ = 0, cachedCols_ = 0;
  mutable bool cachedPlotAreaValid_ = false;
  ChartTooltipState tooltip_;
  int hoveredRow_ = -1, hoveredCol_ = -1;

  static constexpr int kMaxColLabels = 10; // skip labels beyond this to avoid overlap

  std::shared_ptr<FluxHeatmapChartWidget> self()
  {
    return std::static_pointer_cast<FluxHeatmapChartWidget>(shared_from_this());
  }

  bool hasCell(int row, int col) const
  {
    return row >= 0 && row < static_cast<int>(values.size()) &&
           col >= 0 && col < static_cast<int>(values[row].size());
  }

  float cellValue(int row, int col) const
  {
    return hasCell(row, col) ? values[row][col] : 0.f;
  }

  // ── Range ────────────────────────────────────────────────────────────

  void computeRange()
  {
    if (!autoRange || values.empty())
      return;

    std::vector<const std::vector<float> *> rows;
    rows.reserve(values.size());
    for (const auto &row : values)
      rows.push_back(&row);

    ChartRange r = computeAutoRange(rows, /*paddingFraction=*/0.f);
    valueMin = r.min;
    valueMax = r.max;
  }

  // ── Color banding ────────────────────────────────────────────────────
  // Nearest-stop selection, not interpolation — see file header comment.

  Color colorForValue(float v) const
  {
    if (colorStops.empty())
      return theme.bgColor;
    if (colorStops.size() == 1 || valueMax <= valueMin)
      return colorStops.front();

    float t = std::clamp((v - valueMin) / (valueMax - valueMin), 0.f, 1.f);
    int idx = static_cast<int>(std::round(t * (colorStops.size() - 1)));
    idx = std::clamp(idx, 0, static_cast<int>(colorStops.size()) - 1);
    return colorStops[idx];
  }

  // Text contrast decided from the normalized value directly (not from the
  // rendered Color) — see file header comment for why.
  Color textColorForValue(float v) const
  {
    if (valueMax <= valueMin)
      return theme.tooltipTextColor;
    float t = std::clamp((v - valueMin) / (valueMax - valueMin), 0.f, 1.f);
    // Lower band (cool/dark stops in the default scale) reads better with
    // light text; upper band with dark text. Callers using a custom
    // colorStops scale with different luminance ordering can override by
    // setting showValues=false and drawing their own labels.
    return (t > 0.6f) ? Color::fromRGB(20, 20, 25)
                      : Color::fromRGB(240, 240, 245);
  }

  // ── Margin helpers ────────────────────────────────────────────────────

  int computeMarginL(FontCache & /*fc*/) const
  {
    // Rough char-count estimate rather than a real text measurement — same
    // heuristic GraphWidget/candlestick use for legend/label width
    // reservation (no live GraphicsContext is available at this call site;
    // margins are computed before the plot area exists). Good enough for
    // reserving space; drawRowLabels() measures precisely when it actually
    // draws.
    int maxW = 0;
    for (const auto &label : rowLabels)
      maxW = std::max(maxW, static_cast<int>(label.size()) * 7);
    return maxW + 12;
  }

  // ── Drawing ─────────────────────────────────────────────────────────────

  void drawCells(Painter &p, FontCache &fc, const PlotArea &pa, int numRows, int numCols) const
  {
    NativeFont valueFont = fc.getFont(theme.labelFontSize, FontWeight::Bold);
    float cellW = static_cast<float>(pa.w) / numCols;
    float cellH = static_cast<float>(pa.h) / numRows;

    for (int r = 0; r < numRows; ++r)
    {
      for (int c = 0; c < numCols; ++c)
      {
        if (!hasCell(r, c))
          continue;

        int cx = pa.x + static_cast<int>(c * cellW);
        int cy = pa.y + static_cast<int>(r * cellH);
        int cw = static_cast<int>((c + 1) * cellW) - static_cast<int>(c * cellW);
        int ch = static_cast<int>((r + 1) * cellH) - static_cast<int>(r * cellH);

        float v = values[r][c];
        Color cellColor = colorForValue(v);

        // Slight inset so cells read as distinct tiles, like a typical
        // heatmap/calendar-graph grid rather than one solid block.
        int inset = 1;
        p.fillRect(cx + inset, cy + inset, std::max(1, cw - inset * 2),
                  std::max(1, ch - inset * 2), cellColor);

        if (hoveredRow_ == r && hoveredCol_ == c)
          p.drawRectOutline(cx + inset, cy + inset, std::max(1, cw - inset * 2),
                            std::max(1, ch - inset * 2),
                            Color::fromRGBA(255, 255, 255, 200), 2);

        if (showValues && cw > 24 && ch > 16)
        {
          char buf[16];
          std::snprintf(buf, sizeof(buf), "%.1f", v);
          int tw = 0, th = 0;
          p.measureText(toWideString(buf), valueFont, tw, th);
          p.drawTextA(buf, cx + cw / 2 - tw / 2, cy + ch / 2 - th / 2, tw + 2, th + 2,
                     valueFont, textColorForValue(v), DT_LEFT | DT_TOP | DT_SINGLELINE);
        }
      }
    }
  }

  void drawRowLabels(Painter &p, FontCache &fc, const PlotArea &pa, int numRows) const
  {
    NativeFont font = fc.getFont(theme.labelFontSize, FontWeight::Normal);
    float cellH = static_cast<float>(pa.h) / numRows;

    for (int r = 0; r < numRows; ++r)
    {
      int cy = pa.y + static_cast<int>(r * cellH);
      int ch = static_cast<int>((r + 1) * cellH) - static_cast<int>(r * cellH);

      int tw = 0, th = 0;
      p.measureText(toWideString(rowLabels[r]), font, tw, th);
      p.drawTextA(rowLabels[r], pa.x - tw - 6, cy + ch / 2 - th / 2, tw + 2, th,
                 font, theme.labelColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
  }

  void drawColumnLabels(Painter &p, FontCache &fc, const PlotArea &pa, int numCols) const
  {
    NativeFont font = fc.getFont(theme.labelFontSize, FontWeight::Normal);
    float cellW = static_cast<float>(pa.w) / numCols;
    int labelY = pa.y - theme.labelFontSize - 6;

    int step = std::max(1, numCols / kMaxColLabels);
    for (int c = 0; c < numCols; c += step)
    {
      int cx = pa.x + static_cast<int>(c * cellW);
      int cw = static_cast<int>((c + 1) * cellW) - static_cast<int>(c * cellW);

      int tw = 0, th = 0;
      p.measureText(toWideString(colLabels[c]), font, tw, th);
      p.drawTextA(colLabels[c], cx + cw / 2 - tw / 2, labelY, tw + 2, th + 2,
                 font, theme.labelColor, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
  }

  // ── Hit-testing ────────────────────────────────────────────────────────

  void hitTestCell(int mx, int my, int &outRow, int &outCol) const
  {
    outRow = outCol = -1;
    if (!cachedPlotAreaValid_ || cachedRows_ <= 0 || cachedCols_ <= 0)
      return;
    const PlotArea &pa = cachedPlotArea_;
    if (mx < pa.x || mx >= pa.x + pa.w || my < pa.y || my >= pa.y + pa.h)
      return;

    float cellW = static_cast<float>(pa.w) / cachedCols_;
    float cellH = static_cast<float>(pa.h) / cachedRows_;
    if (cellW <= 0.f || cellH <= 0.f)
      return;

    int col = static_cast<int>((mx - pa.x) / cellW);
    int row = static_cast<int>((my - pa.y) / cellH);
    if (row < 0 || row >= cachedRows_ || col < 0 || col >= cachedCols_)
      return;
    if (!hasCell(row, col))
      return;

    outRow = row;
    outCol = col;
  }
};

using HeatmapChartWidgetPtr = std::shared_ptr<FluxHeatmapChartWidget>;

inline HeatmapChartWidgetPtr HeatmapChart()
{
  return std::make_shared<FluxHeatmapChartWidget>();
}

inline HeatmapChartWidgetPtr HeatmapChart(int w, int h)
{
  return std::make_shared<FluxHeatmapChartWidget>()->setSize(w, h);
}