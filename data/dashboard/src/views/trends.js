/**
 * Trends tab — engine version comparison over time.
 * Shows how key metrics evolve across versions.
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, plotPanel, getSearchTable } from '../util.js';

export async function renderTrends() {
  const searchTable = getSearchTable();
  const tables = window.__tables || [];

  if (!searchTable || !tables.includes('engines')) {
    return el('div', { class: 'panel' }, el('p', {}, 'Need search and engines tables for trends.'));
  }

  const container = el('div', {});

  // Get engine versions ordered by id (proxy for chronological)
  const engines = await sql(`
    SELECT id, name, version, name || ' (' || version || ')' as label
    FROM engines 
    ORDER BY id
  `);

  if (engines.length < 2) {
    return el('div', { class: 'panel' }, el('p', {}, 'Need at least 2 engine versions for trends.'));
  }

  // Aggregate key metrics per engine
  const trendMetrics = [
    { col: 'depth', label: 'Avg Depth', agg: 'AVG' },
    { col: 'nodes', label: 'Avg Nodes', agg: 'AVG' },
    { col: 'time_ms', label: 'Avg Time (ms)', agg: 'AVG' },
    { col: 'tt_hits', label: 'Avg TT Hits', agg: 'AVG' },
    { col: 'fail_highs', label: 'Avg Fail Highs', agg: 'AVG' },
    { col: 'nmp', label: 'Avg NMP', agg: 'AVG' },
  ];

  // Create a view for trend data
  await coordinator().exec(`
    CREATE OR REPLACE TEMP VIEW trend_agg AS
    SELECT 
      e.name || ' (' || e.version || ')' as engine_label,
      e.id as engine_order,
      AVG(s.completed_depth) as avg_depth,
      AVG(s.total_nodes) as avg_nodes,
      AVG(s.total_time_ms) as avg_time_ms,
      AVG(s.total_tt_hits) as avg_tt_hits,
      AVG(s.total_fail_highs) as avg_fail_highs,
      AVG(s.total_nmp) as avg_nmp,
      COUNT(*) as n_searches
    FROM ${searchTable} s
    JOIN engines e ON s.engine_id = e.id
    GROUP BY e.id, e.name, e.version
    ORDER BY e.id
  `);

  // Line charts for each metric
  const grid = el('div', { class: 'grid-2' });

  for (const m of trendMetrics) {
    const colName = `avg_${m.col}`;
    const plot = vg.plot(
      vg.lineY(vg.from('trend_agg'), {
        x: 'engine_label',
        y: colName,
        stroke: 'var(--accent)',
        marker: true,
      }),
      vg.width(450),
      vg.height(220),
      vg.marginLeft(60),
      vg.marginBottom(60),
      vg.xLabel('Engine Version'),
      vg.yLabel(m.label),
      vg.xTickRotate(-30),
    );
    grid.appendChild(plotPanel(m.label, plot));
  }

  container.appendChild(grid);

  // Search count per engine
  const countPlot = vg.plot(
    vg.barY(vg.from('trend_agg'), {
      x: 'engine_label',
      y: 'n_searches',
      fill: 'steelblue',
    }),
    vg.width(800),
    vg.height(250),
    vg.marginLeft(60),
    vg.marginBottom(80),
    vg.xLabel('Engine Version'),
    vg.yLabel('Searches'),
    vg.xTickRotate(-30),
  );
  container.appendChild(plotPanel('Data Volume per Engine', countPlot));

  return container;
}
