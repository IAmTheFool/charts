#pragma once

// ============================================================================
// flux_charts.hpp  —  Umbrella include for the flux_charts package
//
// #include "flux_charts/flux_charts.hpp" to pull in every chart widget this
// package provides. GraphWidget (Line/Bar/Area/Scatter) is NOT re-exported
// here — it ships in flux core (flux_graph.hpp) and stays directly
// accessible from there.
// ============================================================================

#include "flux_chart_common.hpp"
#include "flux_pie_chart.hpp"
#include "flux_candlestick_chart.hpp"
// Future chart types (heatmap, ...) get added here as they land.