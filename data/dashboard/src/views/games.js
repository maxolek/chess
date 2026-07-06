/**
 * Games tab — game results, openings, and outcomes analysis.
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, plotPanel, fmt, metricCard } from '../util.js';

export async function renderGames() {
  const tables = window.__tables || [];
  if (!tables.includes('game_stats')) {
    return el('div', { class: 'panel' }, el('p', {}, 'No game_stats table found.'));
  }

  const container = el('div', {});

  // Summary metrics
  const [counts] = await sql(`
    SELECT 
      COUNT(*) as total,
      COUNT(*) FILTER (WHERE result = 'white') as white_wins,
      COUNT(*) FILTER (WHERE result = 'black') as black_wins,
      COUNT(*) FILTER (WHERE result = 'draw') as draws
    FROM game_stats
  `);

  const metrics = el('div', { class: 'grid-4' },
    metricCard(fmt(counts.total), 'Total Games'),
    metricCard(fmt(counts.white_wins), 'White Wins'),
    metricCard(fmt(counts.black_wins), 'Black Wins'),
    metricCard(fmt(counts.draws), 'Draws'),
  );
  container.appendChild(panel('Game Results', metrics));

  const $filter = Selection.crossfilter();

  // Result distribution (bar)
  const resultBar = vg.plot(
    vg.barX(vg.from('game_stats'), {
      x: vg.count(),
      y: 'result',
      fill: 'result',
    }),
    vg.width(600),
    vg.height(150),
    vg.marginLeft(80),
    vg.xLabel('Count'),
    vg.yLabel('Result'),
    vg.colorDomain(['white', 'black', 'draw']),
    vg.colorRange(['#e2e2e2', '#333333', '#6366f1']),
  );
  container.appendChild(plotPanel('Results', resultBar));

  // Termination type breakdown
  const termBar = vg.plot(
    vg.barX(vg.from('game_stats'), {
      x: vg.count(),
      y: 'termination',
      fill: 'steelblue',
      sort: { y: '-x' },
    }),
    vg.width(600),
    vg.height(200),
    vg.marginLeft(100),
    vg.xLabel('Count'),
    vg.yLabel('Termination'),
  );
  container.appendChild(plotPanel('Termination Types', termBar));

  // Opening frequency (top 20)
  const openingBar = vg.plot(
    vg.barX(
      vg.from('game_stats', {
        select: { opening: 'opening', count: vg.sql`COUNT(*)` },
        groupBy: ['opening'],
        orderBy: [{ count: 'desc' }],
        limit: 20,
      }),
      { x: 'count', y: 'opening', fill: 'steelblue', sort: { y: '-x' } }
    ),
    vg.width(700),
    vg.height(400),
    vg.marginLeft(200),
    vg.xLabel('Games'),
    vg.yLabel('Opening'),
  );
  container.appendChild(plotPanel('Top 20 Openings', openingBar));

  // Game length distribution
  const lengthHist = vg.plot(
    vg.rectY(vg.from('game_stats', { filterBy: vg.sql`run_time_s IS NOT NULL` }), {
      x: vg.bin('run_time_s'),
      y: vg.count(),
      fill: 'steelblue',
    }),
    vg.width(600),
    vg.height(250),
    vg.marginLeft(50),
    vg.xLabel('Game Duration (seconds)'),
    vg.yLabel('Count'),
  );
  container.appendChild(plotPanel('Game Duration Distribution', lengthHist));

  return container;
}
