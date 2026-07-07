/**
 * Compare tab — side-by-side engine comparison with cross-filtering.
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, plotPanel, getSearchTable } from '../util.js';

export async function renderCompare() {
  const searchTable = getSearchTable();
  const tables = window.__tables || [];

  if (!searchTable || !tables.includes('engines')) {
    return el('div', { class: 'panel' }, el('p', {}, 'Need search and engines tables for comparison.'));
  }

  const container = el('div', {});
  const $filter = Selection.crossfilter();

  // Engine selector
  const engineSelect = vg.menu({
    from: 'engines',
    column: 'id',
    label: 'Filter Engine',
    as: $filter,
  });

  const controls = el('div', { class: 'panel', style: { display: 'flex', gap: '16px', alignItems: 'center' } });
  controls.appendChild(engineSelect);
  container.appendChild(controls);

  // Box plots comparing engines
  const depthBox = vg.plot(
    vg.rectY(vg.from(searchTable, { filterBy: $filter }), {
      x: 'engine_id',
      y: 'completed_depth',
    }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(60),
    vg.xLabel('Engine'),
    vg.yLabel('Depth'),
  );
  container.appendChild(plotPanel('Depth Distribution by Engine', depthBox));

  // Nodes box plot
  const nodesBox = vg.plot(
    vg.rectY(vg.from(searchTable, { filterBy: $filter }), {
      x: 'engine_id',
      y: 'total_nodes',
    }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(60),
    vg.xLabel('Engine'),
    vg.yLabel('Nodes'),
  );
  container.appendChild(plotPanel('Nodes Distribution by Engine', nodesBox));

  // Time comparison
  const timeBox = vg.plot(
    vg.rectY(vg.from(searchTable, { filterBy: $filter }), {
      x: 'engine_id',
      y: 'total_time_ms',
    }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(60),
    vg.xLabel('Engine'),
    vg.yLabel('Time (ms)'),
  );
  container.appendChild(plotPanel('Search Time by Engine', timeBox));

  // TT efficiency: hits / stores ratio
  const ttPlot = vg.plot(
    vg.dot(vg.from(searchTable, { filterBy: $filter }), {
      x: 'total_tt_stores',
      y: 'total_tt_hits',
      fill: 'engine_id',
      opacity: 0.3,
      r: 2,
    }),
    vg.width(600),
    vg.height(300),
    vg.marginLeft(60),
    vg.xLabel('TT Stores'),
    vg.yLabel('TT Hits'),
    vg.colorLegend(true),
  );
  container.appendChild(plotPanel('TT Efficiency: Stores vs Hits', ttPlot));

  return container;
}
