/**
 * SPRT tab — experiment results and statistical testing.
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, plotPanel, fmt, metricCard } from '../util.js';

export async function renderSprt() {
  const tables = window.__tables || [];
  if (!tables.includes('sprt_runs')) {
    return el('div', { class: 'panel' }, el('p', {}, 'No sprt_runs table found.'));
  }

  const container = el('div', {});

  // Summary stats
  const [counts] = await sql(`
    SELECT 
      COUNT(*) as total,
      COUNT(*) FILTER (WHERE result = 'H1') as h1_count,
      COUNT(*) FILTER (WHERE result = 'H0') as h0_count,
      COUNT(*) FILTER (WHERE result IS NULL OR result = '') as incomplete
    FROM sprt_runs
  `);

  const metrics = el('div', { class: 'grid-4' },
    metricCard(fmt(counts.total), 'Total Tests'),
    metricCard(fmt(counts.h1_count), 'H1 (Improved)'),
    metricCard(fmt(counts.h0_count), 'H0 (No Gain)'),
    metricCard(fmt(counts.incomplete), 'Incomplete'),
  );
  container.appendChild(panel('SPRT Results', metrics));

  // Elo difference distribution
  const eloHist = vg.plot(
    vg.rectY(vg.from('sprt_runs', { filterBy: vg.sql`elo_diff IS NOT NULL` }), {
      x: vg.bin('elo_diff'),
      y: vg.count(),
      fill: 'steelblue',
    }),
    vg.width(600),
    vg.height(250),
    vg.marginLeft(50),
    vg.xLabel('Elo Difference (H1 - H0 bounds)'),
    vg.yLabel('Count'),
  );
  container.appendChild(plotPanel('Elo Bounds Distribution', eloHist));

  // Games played per SPRT
  const gamesHist = vg.plot(
    vg.rectY(vg.from('sprt_runs', { filterBy: vg.sql`games_played IS NOT NULL` }), {
      x: vg.bin('games_played'),
      y: vg.count(),
      fill: 'steelblue',
    }),
    vg.width(600),
    vg.height(250),
    vg.marginLeft(50),
    vg.xLabel('Games Played'),
    vg.yLabel('Count'),
  );
  container.appendChild(plotPanel('Games per SPRT Test', gamesHist));

  // Result by opening book
  const bookPlot = vg.plot(
    vg.barX(
      vg.from('sprt_runs', {
        select: { opening_book: 'opening_book', count: vg.sql`COUNT(*)` },
        groupBy: ['opening_book'],
        orderBy: [{ count: 'desc' }],
      }),
      { x: 'count', y: 'opening_book', fill: 'steelblue', sort: { y: '-x' } }
    ),
    vg.width(600),
    vg.height(200),
    vg.marginLeft(150),
    vg.xLabel('Tests'),
    vg.yLabel('Opening Book'),
  );
  container.appendChild(plotPanel('Tests by Opening Book', bookPlot));

  // Timeline of SPRT results
  if (await sql(`SELECT column_name FROM information_schema.columns WHERE table_name='sprt_runs' AND column_name='start_time_utc' LIMIT 1`).then(r => r.length > 0).catch(() => false)) {
    const timeline = vg.plot(
      vg.dot(vg.from('sprt_runs', { filterBy: vg.sql`start_time_utc IS NOT NULL` }), {
        x: 'start_time_utc',
        y: 'elo_diff',
        fill: 'result',
        r: 5,
        opacity: 0.7,
      }),
      vg.width(800),
      vg.height(300),
      vg.marginLeft(60),
      vg.xLabel('Date'),
      vg.yLabel('Elo Bounds'),
      vg.colorDomain(['H1', 'H0']),
      vg.colorRange(['var(--success)', 'var(--danger)']),
    );
    container.appendChild(plotPanel('SPRT Timeline', timeline));
  }

  return container;
}
