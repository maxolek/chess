/**
 * Overview tab — high-level summary metrics and distributions.
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, metricCard, fmt, plotPanel, getSearchTable } from '../util.js';

export async function renderOverview() {
  const searchTable = getSearchTable();
  const tables = window.__tables || [];

  // Gather summary counts
  const [engines, games, searches, experiments] = await Promise.all([
    tables.includes('engines') ? sql('SELECT COUNT(*) as n FROM engines') : [{ n: 0 }],
    tables.includes('game_stats') ? sql('SELECT COUNT(*) as n FROM game_stats') : [{ n: 0 }],
    searchTable ? sql(`SELECT COUNT(*) as n FROM ${searchTable}`) : [{ n: 0 }],
    tables.includes('experiments') ? sql('SELECT COUNT(*) as n FROM experiments') : [{ n: 0 }],
  ]);

  const container = el('div', {});

  // Metric cards
  const metrics = el('div', { class: 'grid-4' },
    metricCard(fmt(engines[0]?.n), 'Engines'),
    metricCard(fmt(games[0]?.n), 'Games'),
    metricCard(fmt(searches[0]?.n), 'Searches'),
    metricCard(fmt(experiments[0]?.n), 'Experiments'),
  );
  container.appendChild(panel('Summary', metrics));

  if (!searchTable) {
    container.appendChild(el('div', { class: 'panel' }, el('p', {}, 'No search table found.')));
    return container;
  }

  // Depth distribution (histogram)
  const depthPlot = vg.plot(
    vg.rectY(vg.from(searchTable), { x: vg.bin('depth'), y: vg.count(), fill: 'steelblue' }),
    vg.width(600),
    vg.height(250),
    vg.marginLeft(50),
    vg.xLabel('Completed Depth'),
    vg.yLabel('Count'),
  );
  container.appendChild(plotPanel('Depth Distribution', depthPlot));

  // Eval distribution
  await coordinator().exec(`CREATE OR REPLACE TEMP VIEW eval_filtered AS SELECT * FROM ${searchTable} WHERE ABS(eval) <= 1500`);
  const evalPlot = vg.plot(
    vg.rectY(vg.from('eval_filtered'), {
      x: vg.bin('eval'),
      y: vg.count(),
      fill: 'steelblue',
    }),
    vg.width(600),
    vg.height(250),
    vg.marginLeft(50),
    vg.xLabel('Evaluation (cp)'),
    vg.yLabel('Count'),
  );
  container.appendChild(plotPanel('Eval Distribution (±1500cp)', evalPlot));

  // Nodes distribution (log scale)
  await coordinator().exec(`CREATE OR REPLACE TEMP VIEW nodes_filtered AS SELECT * FROM ${searchTable} WHERE nodes > 0`);
  const nodesPlot = vg.plot(
    vg.rectY(vg.from('nodes_filtered'), {
      x: vg.bin('nodes'),
      y: vg.count(),
      fill: 'steelblue',
    }),
    vg.width(600),
    vg.height(250),
    vg.marginLeft(50),
    vg.xLabel('Nodes'),
    vg.yLabel('Count'),
    vg.xScale('log'),
  );
  container.appendChild(plotPanel('Nodes Distribution', nodesPlot));

  // Engine breakdown
  if (tables.includes('engines')) {
    await coordinator().exec(`CREATE OR REPLACE TEMP VIEW engine_counts AS SELECT engine_id, COUNT(*) as count FROM ${searchTable} GROUP BY engine_id`);
    const enginePlot = vg.plot(
      vg.barX(
        vg.from('engine_counts'),
        { x: 'count', y: 'engine_id', fill: 'var(--accent)', sort: { y: '-x' } }
      ),
      vg.width(600),
      vg.height(Math.max(150, engines[0]?.n * 30)),
      vg.marginLeft(80),
      vg.xLabel('Searches'),
      vg.yLabel('Engine'),
    );
    container.appendChild(plotPanel('Searches by Engine', enginePlot));
  }

  return container;
}
