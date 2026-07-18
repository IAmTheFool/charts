#pragma once

// ============================================================================
// flux_candlestick_chart.hpp  —  FluxCandlestickChartWidget
//
// Cartesian OHLC chart. Axis/grid/margin math is NOT shared with core's
// GraphWidget (flux_graph.hpp) — core can't depend on this package, so that
// direction of sharing is architecturally impossible, only the reverse.
// What IS reused from flux_chart_common.hpp: ChartTheme, tooltip drawing,
// chartContains, and computeAutoRange (fed both the high[] and low[]
// series here, which is exactly the "any number of value lists" case it
// was designed for).
//
// Candle spacing uses fixed-width bodies with a gap between them (Option A)
// — the conventional trading-chart look, same cell-based spacing pattern
// GraphWidget::BarGeom uses for bar charts.
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
// Candle
// ============================================================================

struct Candle
{
  std::string label; // x-axis tick, e.g. a date/time string
  float open = 0.f;
  float high = 0.f;
  float low = 0.f;
  float close = 0.f;
};

// ============================================================================
// FluxCandlestickChartWidget
// ============================================================================

class FluxCandlestickChartWidget : public Widget
{
public:
  std::vector<Candle> candles;
  std::string title;
  ChartTheme theme = ChartTheme::defaults();

  Color bullColor = Color::fromRGB(38, 166, 91); // close >= open
  Color bearColor = Color::fromRGB(217, 60, 60); // close < open

  bool showGrid = true;
  bool autoRange = true;
  float yMin = 0.f, yMax = 1.f; // used when autoRange == false

  // Callback: called with the candle index on click.
  std::function<void(int)> onCandleClick;

  FluxCandlestickChartWidget()
  {
    autoWidth = false;
    autoHeight = false;
    width = 500;
    height = 320;
    isFocusable = true;
  }

  // ── Fluent API ─────────────────────────────────────────────────────────

  std::shared_ptr<FluxCandlestickChartWidget>
  addCandle(const std::string &label, float o, float h, float l, float c)
  {
    candles.push_back({label, o, h, l, c});
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> setCandles(std::vector<Candle> c)
  {
    candles = std::move(c);
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> setTitle(const std::string &t)
  {
    title = t;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> setTheme(const ChartTheme &t)
  {
    theme = t;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> setBullBearColors(Color bull, Color bear)
  {
    bullColor = bull;
    bearColor = bear;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> setYRange(float mn, float mx)
  {
    yMin = mn;
    yMax = mx;
    autoRange = false;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> setShowGrid(bool v)
  {
    showGrid = v;
    markNeedsPaint();
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> setOnCandleClick(std::function<void(int)> cb)
  {
    onCandleClick = std::move(cb);
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> setSize(int w, int h)
  {
    width = w;
    height = h;
    autoWidth = autoHeight = false;
    markNeedsLayout();
    return self();
  }

  std::shared_ptr<FluxCandlestickChartWidget> clearCandles()
  {
    candles.clear();
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

    int marginL = computeMarginL();
    int marginR = 12;
    int marginT = (!title.empty() ? theme.titleFontSize + 10 : 0) + 10;
    int marginB = theme.labelFontSize + 24;

    PlotArea pa{x + marginL, y + marginT,
               width - marginL - marginR, height - marginT - marginB};

    if (pa.w > 0 && pa.h > 0)
    {
      cachedPlotArea_ = pa;
      cachedPlotAreaValid_ = true;

      if (showGrid)
        drawGrid(p, pa);
      drawAxes(p, pa);
      drawAxisLabels(p, fontCache, pa);

      if (!candles.empty())
        drawCandles(p, pa);
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

    int idx = hitTestCandle(mx, my);
    bool changed = (idx != hoveredCandle_);
    hoveredCandle_ = idx;

    if (idx >= 0)
    {
      const Candle &c = candles[idx];
      char buf[160];
      std::snprintf(buf, sizeof(buf), "%s  O:%.2f H:%.2f L:%.2f C:%.2f",
                   c.label.c_str(), c.open, c.high, c.low, c.close);
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
    if (!chartContains(this, mx, my) || !onCandleClick)
      return false;
    int idx = hitTestCandle(mx, my);
    if (idx >= 0)
    {
      onCandleClick(idx);
      return true;
    }
    return false;
  }

  bool handleMouseLeave() override
  {
    bool changed = tooltip_.visible || hoveredCandle_ >= 0;
    tooltip_.visible = false;
    hoveredCandle_ = -1;
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
  mutable bool cachedPlotAreaValid_ = false;
  ChartTooltipState tooltip_;
  int hoveredCandle_ = -1;

  std::shared_ptr<FluxCandlestickChartWidget> self()
  {
    return std::static_pointer_cast<FluxCandlestickChartWidget>(shared_from_this());
  }

  // ── Range ────────────────────────────────────────────────────────────

  void computeRange()
  {
    if (!autoRange || candles.empty())
      return;

    std::vector<float> highs, lows;
    highs.reserve(candles.size());
    lows.reserve(candles.size());
    for (const auto &c : candles)
    {
      highs.push_back(c.high);
      lows.push_back(c.low);
    }

    ChartRange r = computeAutoRange({&highs, &lows});
    yMin = r.min;
    yMax = r.max;
  }

  // ── Margin / coordinate helpers ─────────────────────────────────────────

  int computeMarginL() const
  {
    float absMax = std::max(std::abs(yMin), std::abs(yMax));
    int digits = absMax >= 10000  ? 6
                 : absMax >= 1000 ? 5
                 : absMax >= 100  ? 4
                                  : 3;
    return digits * 7 + 18;
  }

  int dataToPixY(float v, const PlotArea &pa) const
  {
    if (yMax <= yMin)
      return pa.y + pa.h;
    float t = (v - yMin) / (yMax - yMin);
    return pa.y + pa.h - static_cast<int>(t * pa.h);
  }

  struct CandleGeom
  {
    int cellW; // width reserved per candle
    int bodyW; // width of the candle body
  };

  CandleGeom candleGeom(int n, const PlotArea &pa) const
  {
    CandleGeom g;
    g.cellW = (n > 0) ? pa.w / n : 4;
    g.bodyW = std::max(1, static_cast<int>(g.cellW * 0.6f));
    return g;
  }

  int candleCenterX(int i, const CandleGeom &g, const PlotArea &pa) const
  {
    return pa.x + i * g.cellW + g.cellW / 2;
  }

  // ── Drawing ─────────────────────────────────────────────────────────────

  void drawGrid(Painter &p, const PlotArea &pa) const
  {
    static constexpr int kYTicks = 5;
    for (int i = 0; i <= kYTicks; ++i)
    {
      int ly = pa.y + static_cast<int>(static_cast<float>(i) / kYTicks * pa.h);
      p.drawHLine(pa.x, ly, pa.w, Color::fromRGBA(255, 255, 255, 20), 1);
    }
  }

  void drawAxes(Painter &p, const PlotArea &pa) const
  {
    Color axisColor = Color::fromRGBA(180, 180, 180, 200);
    p.drawHLine(pa.x, pa.y + pa.h, pa.w, axisColor, 1);
    p.drawVLine(pa.x, pa.y, pa.h, axisColor, 1);
  }

  void drawAxisLabels(Painter &p, FontCache &fc, const PlotArea &pa) const
  {
    NativeFont font = fc.getFont(theme.labelFontSize, FontWeight::Normal);
    static constexpr int kYTicks = 5;

    for (int i = 0; i <= kYTicks; ++i)
    {
      float t = static_cast<float>(i) / kYTicks;
      float val = yMin + t * (yMax - yMin);
      int ly = pa.y + pa.h - static_cast<int>(t * pa.h);

      char buf[32];
      if (std::abs(val) >= 1000)
        std::snprintf(buf, sizeof(buf), "%.0f", val);
      else if (std::abs(val) >= 10)
        std::snprintf(buf, sizeof(buf), "%.1f", val);
      else
        std::snprintf(buf, sizeof(buf), "%.2f", val);

      int tw = 0, th = 0;
      p.measureText(toWideString(buf), font, tw, th);
      p.drawTextA(buf, pa.x - tw - 4, ly - th / 2, tw + 2, th, font,
                 theme.labelColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    int n = static_cast<int>(candles.size());
    if (n == 0)
      return;

    CandleGeom g = candleGeom(n, pa);
    int step = std::max(1, n / 8);
    for (int i = 0; i < n; i += step)
    {
      int cx = candleCenterX(i, g, pa);
      int tw = 0, th = 0;
      p.measureText(toWideString(candles[i].label), font, tw, th);
      p.drawTextA(candles[i].label, cx - tw / 2, pa.y + pa.h + 4, tw + 2, th + 2,
                 font, theme.labelColor, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
  }

  void drawCandles(Painter &p, const PlotArea &pa) const
  {
    int n = static_cast<int>(candles.size());
    CandleGeom g = candleGeom(n, pa);

    for (int i = 0; i < n; ++i)
    {
      const Candle &c = candles[i];
      int cx = candleCenterX(i, g, pa);
      Color col = (c.close >= c.open) ? bullColor : bearColor;

      // Wick: thin vertical line from high to low through the body center.
      int highY = dataToPixY(c.high, pa);
      int lowY = dataToPixY(c.low, pa);
      p.drawVLine(cx, highY, lowY - highY, col, 1);

      // Body: filled rect from open to close.
      int openY = dataToPixY(c.open, pa);
      int closeY = dataToPixY(c.close, pa);
      int bodyTop = std::min(openY, closeY);
      int bodyH = std::abs(closeY - openY);
      if (bodyH == 0)
        bodyH = 1; // doji — always draw at least 1px

      int bx = cx - g.bodyW / 2;
      p.fillRect(bx, bodyTop, g.bodyW, bodyH, col);
    }
  }

  // ── Hit-testing ────────────────────────────────────────────────────────
  // By design, hit-testing only considers the x column a candle occupies —
  // not the y position within it — so hovering anywhere in a candle's
  // vertical strip shows its tooltip, matching typical candlestick chart UX.

  int hitTestCandle(int mx, int my) const
  {
    if (!cachedPlotAreaValid_ || candles.empty())
      return -1;
    const PlotArea &pa = cachedPlotArea_;
    if (mx < pa.x || mx >= pa.x + pa.w || my < pa.y || my >= pa.y + pa.h)
      return -1;

    int n = static_cast<int>(candles.size());
    CandleGeom g = candleGeom(n, pa);
    if (g.cellW <= 0)
      return -1;

    int idx = (mx - pa.x) / g.cellW;
    if (idx < 0 || idx >= n)
      return -1;
    return idx;
  }
};

using CandlestickChartWidgetPtr = std::shared_ptr<FluxCandlestickChartWidget>;

inline CandlestickChartWidgetPtr CandlestickChart()
{
  return std::make_shared<FluxCandlestickChartWidget>();
}

inline CandlestickChartWidgetPtr CandlestickChart(int w, int h)
{
  return std::make_shared<FluxCandlestickChartWidget>()->setSize(w, h);
}