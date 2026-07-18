#pragma once

// ============================================================================
// flux_chart_common.hpp  —  Shared helpers for the flux_charts package
//
// Free functions + small POD structs reused across every FluxXChartWidget
// (pie, donut, candlestick, ...). Deliberately NOT a base class — each chart
// widget stays fully self-contained (same shape as GraphWidget in core),
// just calling into these instead of reimplementing tooltip/legend/title
// drawing and range computation from scratch.
//
// Everything here is chart-shape-agnostic: nothing in this file knows what
// a "series" or "slice" is. Cartesian-specific concerns (axes, grid lines,
// plot-area margins, bar/line/scatter geometry) stay in flux_graph.hpp in
// core and are NOT duplicated or abstracted here — see the design note in
// the repo's planning thread for why that boundary was drawn deliberately.
// ============================================================================

#include "flux/flux_core.hpp"
#include "flux/flux_widget.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// ============================================================================
// ChartLegendEntry
// ============================================================================

struct ChartLegendEntry
{
  std::string label;
  Color color;
};

// ============================================================================
// ChartTheme
//
// Shared default palette + font sizes. Mirrors GraphWidget's inline color
// members so visual defaults stay consistent between core's GraphWidget and
// every widget in this package. A widget can copy ChartTheme::defaults()
// and override individual fields, same as GraphWidget's public color
// members can be reassigned today.
// ============================================================================

struct ChartTheme
{
  Color bgColor         = Color::fromRGB(22, 22, 35);
  Color titleColor      = Color::fromRGB(220, 220, 235);
  Color labelColor      = Color::fromRGB(160, 160, 175);
  Color tooltipBgColor  = Color::fromRGBA(30, 30, 50, 230);
  Color tooltipTextColor = Color::fromRGB(230, 230, 240);

  int titleFontSize   = 13;
  int labelFontSize   = 10;
  int legendFontSize  = 10;
  int tooltipFontSize = 11;

  static ChartTheme defaults() { return ChartTheme(); }
};

// ============================================================================
// ChartTooltipState
//
// Plain data a chart widget owns as a member (analogous to GraphWidget's
// tooltipVisible_/tooltipX_/tooltipY_/tooltipText_ fields). The functions
// below read/write it; the widget is responsible for calling
// updateChartTooltip() from its own hit-testing in handleMouseMove().
// ============================================================================

struct ChartTooltipState
{
  bool visible = false;
  int x = 0, y = 0;
  std::string text;
};

// ============================================================================
// Point-in-bounds
//
// Shared replacement for GraphWidget::contains(). Operates on the owning
// widget's absolute x/y/width/height, same as pointInWidget() in
// flux_core.cpp — duplicated here since core doesn't expose a public one.
// ============================================================================

inline bool chartContains(const Widget *w, int mx, int my)
{
  if (!w)
    return false;
  return mx >= w->x && mx < w->x + w->width &&
         my >= w->y && my < w->y + w->height;
}

// ============================================================================
// Auto-range with padding
//
// Generalizes GraphWidget::computeRange()'s scan-and-pad pattern over any
// number of value lists, so a numeric-axis chart type (candlestick's
// high/low, a future gauge/sparkline) doesn't need its own copy. Matches
// GraphWidget's behavior exactly for the degenerate cases:
//   - no values at all            -> {0.f, 1.f}
//   - all values identical        -> {v - 1.f, v + 1.f}
//   - otherwise                   -> [min, max] padded by paddingFraction
// ============================================================================

struct ChartRange
{
  float min = 0.f;
  float max = 1.f;
};

inline ChartRange computeAutoRange(const std::vector<const std::vector<float> *> &series,
                                   float paddingFraction = 0.08f)
{
  ChartRange range;
  bool first = true;

  for (const auto *values : series)
  {
    if (!values)
      continue;
    for (float v : *values)
    {
      if (first)
      {
        range.min = range.max = v;
        first = false;
      }
      else
      {
        range.min = std::min(range.min, v);
        range.max = std::max(range.max, v);
      }
    }
  }

  if (first)
  {
    range.min = 0.f;
    range.max = 1.f;
    return range;
  }

  if (range.min == range.max)
  {
    range.min -= 1.f;
    range.max += 1.f;
    return range;
  }

  float pad = (range.max - range.min) * paddingFraction;
  range.min -= pad;
  range.max += pad;
  return range;
}

// ============================================================================
// Title
//
// Centered top title. Identical behavior to GraphWidget::drawTitle().
// ============================================================================

inline void drawChartTitle(Painter &p, Widget *owner, FontCache &fc,
                           const std::string &title, const ChartTheme &theme)
{
  if (title.empty() || !owner)
    return;

  NativeFont tf = fc.getFont(theme.titleFontSize, FontWeight::Bold);
  int tw = 0, th = 0;
  p.measureText(toWideString(title), tf, tw, th);
  p.drawTextA(title, owner->x + (owner->width - tw) / 2, owner->y + 6,
             tw + 4, th + 2, tf, theme.titleColor,
             DT_LEFT | DT_TOP | DT_SINGLELINE);
}

// ============================================================================
// Legend
//
// Swatch + label list. Unlike GraphWidget::drawLegend() (which hardcodes
// top-right-under-title placement), startX/startY are caller-supplied so a
// pie/donut chart can place the legend beside the circle instead of being
// forced into cartesian top-right placement.
// ============================================================================

inline int legendWidthEstimate(const std::vector<ChartLegendEntry> &entries)
{
  int maxLen = 0;
  for (const auto &e : entries)
    maxLen = std::max(maxLen, static_cast<int>(e.label.size()));
  return maxLen * 7 + 24; // matches GraphWidget::legendWidthEstimate()'s heuristic
}

inline void drawChartLegend(Painter &p, FontCache &fc,
                            const std::vector<ChartLegendEntry> &entries,
                            int startX, int startY, const ChartTheme &theme)
{
  if (entries.empty())
    return;

  NativeFont font = fc.getFont(theme.legendFontSize, FontWeight::Normal);
  int lineH = theme.legendFontSize + 6;
  int swatchW = 12;

  for (int i = 0; i < static_cast<int>(entries.size()); ++i)
  {
    int ly = startY + i * lineH;
    p.fillRoundedRect(startX, ly + 2, swatchW, theme.legendFontSize - 2, 2,
                      entries[i].color);

    int tw = 0, th = 0;
    p.measureText(toWideString(entries[i].label), font, tw, th);
    p.drawTextA(entries[i].label, startX + swatchW + 4, ly, tw + 4, lineH,
               font, theme.labelColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  }
}

// ============================================================================
// Tooltip
//
// Box-with-clamping tooltip — verbatim behavior of GraphWidget::drawTooltip()
// but parameterized on the owning widget's bounds rather than being a member
// function, so any chart type can reuse it regardless of hit-test shape
// (nearest-point for cartesian series, slice-angle for pie/donut, etc).
// ============================================================================

inline void updateChartTooltip(ChartTooltipState &state, int mx, int my,
                               const std::string &text)
{
  if (text.empty())
  {
    state.visible = false;
    return;
  }
  state.visible = true;
  state.x = mx + 12;
  state.y = my - 8;
  state.text = text;
}

inline void drawChartTooltip(Painter &p, Widget *owner, FontCache &fc,
                             const ChartTooltipState &state,
                             const ChartTheme &theme)
{
  if (!state.visible || state.text.empty() || !owner)
    return;

  NativeFont font = fc.getFont(theme.tooltipFontSize, FontWeight::Normal);
  int tw = 0, th = 0;
  p.measureText(toWideString(state.text), font, tw, th);

  int pad = 6;
  int bx = state.x;
  int by = state.y - th - pad * 2;

  // Keep inside the owning widget's bounds, same clamping order as
  // GraphWidget::drawTooltip(): horizontal first, then vertical.
  if (bx + tw + pad * 2 > owner->x + owner->width)
    bx = state.x - tw - pad * 2 - 14;
  if (bx < owner->x)
    bx = owner->x + 2;

  if (by < owner->y)
    by = state.y + 12;
  if (by + th + pad * 2 > owner->y + owner->height)
    by = owner->y + owner->height - th - pad * 2 - 4;

  p.fillRoundedRect(bx, by, tw + pad * 2, th + pad * 2, 4, theme.tooltipBgColor);
  p.drawBorder(bx, by, tw + pad * 2, th + pad * 2, 4,
              Color::fromRGBA(255, 255, 255, 40), 1);
  p.drawTextA(state.text, bx + pad, by + pad, tw + 2, th + 2, font,
             theme.tooltipTextColor, DT_LEFT | DT_TOP | DT_SINGLELINE);
}