#pragma once

// ============================================================================
// flux_pie_chart.hpp  —  FluxPieChartWidget (pie + donut)
//
// Donut is NOT a separate widget/file. innerRadiusRatio == 0 renders a plain
// pie; > 0 cuts a hole in the middle. Pie and donut are the same geometry —
// forking the file would duplicate the arc/hit-test math for no real
// difference in behavior. See the package's design notes for the reasoning.
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
// PieSlice
// ============================================================================

struct PieSlice
{
  std::string label;
  float value = 0.f;
  Color color = Color::fromRGB(51, 153, 255);
};

// ============================================================================
// FluxPieChartWidget
// ============================================================================

class FluxPieChartWidget : public Widget
{
public:
  std::vector<PieSlice> slices;
  std::string title;
  bool showLegend = true;
  bool showLabels = true;       // percentage labels drawn near each slice
  float innerRadiusRatio = 0.f; // 0 = pie, (0,1) = donut hole size
  ChartTheme theme = ChartTheme::defaults();

  // Callback: called with the slice index on click.
  std::function<void(int)> onSliceClick;

  FluxPieChartWidget()
  {
    autoWidth = false;
    autoHeight = false;
    width = 400;
    height = 300;
    isFocusable = true;
  }

  // ── Fluent API ─────────────────────────────────────────────────────────

  std::shared_ptr<FluxPieChartWidget>
  addSlice(const std::string &label, float value,
           Color color = Color::fromRGB(51, 153, 255))
  {
    slices.push_back({label, value, color});
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxPieChartWidget> setSlices(std::vector<PieSlice> s)
  {
    slices = std::move(s);
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxPieChartWidget> setTitle(const std::string &t)
  {
    title = t;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxPieChartWidget> setShowLegend(bool v)
  {
    showLegend = v;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxPieChartWidget> setShowLabels(bool v)
  {
    showLabels = v;
    markNeedsPaint();
    return self();
  }

  /// 0 = pie. Anything in (0, 1) cuts a hole of that fraction of the outer
  /// radius, i.e. a donut. Values are clamped to [0, 0.95] to always leave a
  /// visible ring.
  std::shared_ptr<FluxPieChartWidget> setInnerRadiusRatio(float ratio)
  {
    innerRadiusRatio = std::clamp(ratio, 0.f, 0.95f);
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxPieChartWidget> setTheme(const ChartTheme &t)
  {
    theme = t;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxPieChartWidget> setOnSliceClick(std::function<void(int)> cb)
  {
    onSliceClick = std::move(cb);
    return self();
  }

  std::shared_ptr<FluxPieChartWidget> setSize(int w, int h)
  {
    width = w;
    height = h;
    autoWidth = autoHeight = false;
    markNeedsLayout();
    return self();
  }

  std::shared_ptr<FluxPieChartWidget> clearSlices()
  {
    slices.clear();
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

    Painter p(ctx, this);

    p.fillRoundedRect(x, y, width, height, borderRadius > 0 ? borderRadius : 6,
                      theme.bgColor);
    p.pushClipRect(x, y, width, height);

    int marginT = (!title.empty() ? theme.titleFontSize + 10 : 0) + 10;
    int legendW = showLegend && !slices.empty()
                      ? legendWidthEstimate(buildLegendEntries()) + 16
                      : 0;

    int plotW = width - legendW;
    int plotH = height - marginT - 12;

    centerX_ = x + plotW / 2;
    centerY_ = y + marginT + plotH / 2;
    radius_ = std::max(0, std::min(plotW, plotH) / 2 - 8);

    if (radius_ > 0 && !slices.empty())
    {
      drawSlices(p);
      if (showLabels)
        drawSliceLabels(p, fontCache);
    }

    drawChartTitle(p, this, fontCache, title, theme);

    if (showLegend && !slices.empty())
    {
      auto entries = buildLegendEntries();
      int lx = x + plotW + 8;
      int ly = y + marginT + std::max(0, (plotH - legendHeight(entries)) / 2);
      drawChartLegend(p, fontCache, entries, lx, ly, theme);
    }

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

    int idx = hitTestSlice(mx, my);
    bool changed = (idx != hoveredSlice_);
    hoveredSlice_ = idx;

    if (idx >= 0)
    {
      float total = totalValue();
      float pct = total > 0.f ? (slices[idx].value / total) * 100.f : 0.f;
      char buf[96];
      std::snprintf(buf, sizeof(buf), "%s: %.1f%%", slices[idx].label.c_str(), pct);
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
    if (!chartContains(this, mx, my) || !onSliceClick)
      return false;
    int idx = hitTestSlice(mx, my);
    if (idx >= 0)
    {
      onSliceClick(idx);
      return true;
    }
    return false;
  }

  bool handleMouseLeave() override
  {
    bool changed = tooltip_.visible || hoveredSlice_ >= 0;
    tooltip_.visible = false;
    hoveredSlice_ = -1;
    if (changed)
      markNeedsPaint();
    return false;
  }

private:
  // ── Cached geometry (set each render, read by hit-testing) ────────────
  int centerX_ = 0, centerY_ = 0, radius_ = 0;
  int hoveredSlice_ = -1;
  ChartTooltipState tooltip_;

  static constexpr int kArcSegments = 48; // points sampled per full-circle arc

  std::shared_ptr<FluxPieChartWidget> self()
  {
    return std::static_pointer_cast<FluxPieChartWidget>(shared_from_this());
  }

  float totalValue() const
  {
    float sum = 0.f;
    for (const auto &s : slices)
      sum += std::max(0.f, s.value);
    return sum;
  }

  std::vector<ChartLegendEntry> buildLegendEntries() const
  {
    std::vector<ChartLegendEntry> entries;
    entries.reserve(slices.size());
    for (const auto &s : slices)
      entries.push_back({s.label, s.color});
    return entries;
  }

  int legendHeight(const std::vector<ChartLegendEntry> &entries) const
  {
    return static_cast<int>(entries.size()) * (theme.legendFontSize + 6);
  }

  // Slice angles start at 12 o'clock (-90 degrees) and go clockwise,
  // matching the conventional pie-chart reading order.
  float angleForFraction(float cumulativeFraction) const
  {
    return -90.f + cumulativeFraction * 360.f;
  }

  // ── Drawing ─────────────────────────────────────────────────────────────

  void drawSlices(Painter &p) const
  {
    float total = totalValue();
    if (total <= 0.f)
      return;

    float innerR = radius_ * innerRadiusRatio;
    float cumulative = 0.f;

    for (const auto &slice : slices)
    {
      float fraction = std::max(0.f, slice.value) / total;
      if (fraction <= 0.f)
        continue;

      float startDeg = angleForFraction(cumulative);
      float endDeg = angleForFraction(cumulative + fraction);
      cumulative += fraction;

      std::vector<std::pair<int, int>> poly = buildWedgePolygon(startDeg, endDeg, innerR);
      p.fillPolygonAlpha(poly, slice.color);
    }
  }

  // Samples points along the outer arc; if innerR > 0 also samples the
  // inner arc (back-to-front) to produce a ring segment, otherwise closes
  // through the center point to produce a pie wedge.
  std::vector<std::pair<int, int>> buildWedgePolygon(float startDeg, float endDeg,
                                                      float innerR) const
  {
    std::vector<std::pair<int, int>> poly;
    float sweep = endDeg - startDeg;
    int segments = std::max(1, static_cast<int>(std::abs(sweep) / 360.f * kArcSegments));

    poly.reserve(segments * 2 + 2);

    for (int i = 0; i <= segments; ++i)
    {
      float t = startDeg + sweep * (static_cast<float>(i) / segments);
      float rad = t * 3.14159265f / 180.f;
      poly.emplace_back(centerX_ + static_cast<int>(std::cos(rad) * radius_),
                        centerY_ + static_cast<int>(std::sin(rad) * radius_));
    }

    if (innerR > 0.f)
    {
      for (int i = segments; i >= 0; --i)
      {
        float t = startDeg + sweep * (static_cast<float>(i) / segments);
        float rad = t * 3.14159265f / 180.f;
        poly.emplace_back(centerX_ + static_cast<int>(std::cos(rad) * innerR),
                          centerY_ + static_cast<int>(std::sin(rad) * innerR));
      }
    }
    else
    {
      poly.emplace_back(centerX_, centerY_);
    }

    return poly;
  }

  void drawSliceLabels(Painter &p, FontCache &fc) const
  {
    float total = totalValue();
    if (total <= 0.f)
      return;

    NativeFont font = fc.getFont(theme.labelFontSize, FontWeight::Normal);
    float innerR = radius_ * innerRadiusRatio;
    float labelR = innerR + (radius_ - innerR) * 0.65f; // mid-ring placement
    float cumulative = 0.f;

    for (const auto &slice : slices)
    {
      float fraction = std::max(0.f, slice.value) / total;
      if (fraction <= 0.f)
        continue;

      float midDeg = angleForFraction(cumulative + fraction / 2.f);
      cumulative += fraction;

      // Skip labels on slivers too thin to read.
      if (fraction < 0.03f)
        continue;

      float rad = midDeg * 3.14159265f / 180.f;
      int lx = centerX_ + static_cast<int>(std::cos(rad) * labelR);
      int ly = centerY_ + static_cast<int>(std::sin(rad) * labelR);

      char buf[16];
      std::snprintf(buf, sizeof(buf), "%.0f%%", fraction * 100.f);
      int tw = 0, th = 0;
      p.measureText(toWideString(buf), font, tw, th);
      p.drawTextA(buf, lx - tw / 2, ly - th / 2, tw + 2, th + 2, font,
                 theme.tooltipTextColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
  }

  // ── Hit-testing ────────────────────────────────────────────────────────

  int hitTestSlice(int mx, int my) const
  {
    if (radius_ <= 0 || slices.empty())
      return -1;

    float dx = static_cast<float>(mx - centerX_);
    float dy = static_cast<float>(my - centerY_);
    float dist = std::sqrt(dx * dx + dy * dy);

    float innerR = radius_ * innerRadiusRatio;
    if (dist > radius_ || dist < innerR)
      return -1;

    float angleDeg = std::atan2(dy, dx) * 180.f / 3.14159265f; // [-180, 180]
    // Normalize into the same [-90, 270) space angleForFraction() produces,
    // then convert to a [0, 1) fraction-of-circle for comparison against
    // cumulative slice fractions.
    float normalized = angleDeg + 90.f;
    if (normalized < 0.f)
      normalized += 360.f;
    float fraction = normalized / 360.f;

    float total = totalValue();
    if (total <= 0.f)
      return -1;

    float cumulative = 0.f;
    for (int i = 0; i < static_cast<int>(slices.size()); ++i)
    {
      float sliceFraction = std::max(0.f, slices[i].value) / total;
      if (sliceFraction <= 0.f)
        continue;
      if (fraction >= cumulative && fraction < cumulative + sliceFraction)
        return i;
      cumulative += sliceFraction;
    }
    return -1;
  }
};

using PieChartWidgetPtr = std::shared_ptr<FluxPieChartWidget>;

inline PieChartWidgetPtr PieChart() { return std::make_shared<FluxPieChartWidget>(); }

inline PieChartWidgetPtr PieChart(int w, int h)
{
  return std::make_shared<FluxPieChartWidget>()->setSize(w, h);
}

/// Convenience factory: a pie chart with a hole in the middle. Equivalent to
/// PieChart()->setInnerRadiusRatio(ratio) — donut is not a separate widget.
inline PieChartWidgetPtr DonutChart(float innerRadiusRatio = 0.55f)
{
  return std::make_shared<FluxPieChartWidget>()->setInnerRadiusRatio(innerRadiusRatio);
}

inline PieChartWidgetPtr DonutChart(int w, int h, float innerRadiusRatio = 0.55f)
{
  auto widget = std::make_shared<FluxPieChartWidget>();
  widget->setSize(w, h);
  widget->setInnerRadiusRatio(innerRadiusRatio);
  return widget;
}