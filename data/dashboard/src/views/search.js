/**
 * Search tab — cross-filtered scatter plots and distributions over search data.
 * This is the core analytical view: uses Mosaic cross-filtering so all charts
 * are linked (brush one, all others filter in real-time via DuckDB).
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, plotPanel, getSearchTable } from '../util.js';

export async function renderSearch() {
  const searchTable = getSearchTable();
  if (!searchTable) {
    return el('div', { class: 'panel' }, el('p', {}, 'No search table found.'));
  }

  const container = el('div', {});

  // Cross-filter selection: all plots are linked through this
  const $filter = Selection.crossfilter();

  // Engine selector (menu input)
  const engineMenu = vg.menu({
    from: searchTable,
    column: 'engine_id',
    as: $filter,
    label: 'Engine',
  });

  // Controls row
  const controls = el('div', { class: 'panel', style: { display: 'flex', gap: '16px', alignItems: 'center' } });
  controls.appendChild(engineMenu);
  container.appendChild(controls);

  // Main scatter: depth vs eval, coloured by engine
  const scatter = vg.plot(
    vg.dot(vg.from(searchTable), {
      x: 'depth',
      y: 'eval',
      fill: 'engine_id',
      opacity: 0.5,
      r: 2,
    }),
    vg.intervalXY({ as: $filter }),
    vg.width(800),
    vg.height(400),
    vg.marginLeft(60),
    vg.xLabel('Depth'),
    vg.yLabel('Eval (cp)'),
    vg.colorLegend(true),
  );
  container.appendChild(plotPanel('Depth vs Eval', scatter));

  // Linked histograms row
  const histRow = el('div', { class: 'grid-3' });

  // Depth histogram
  const depthHist = vg.plot(
    vg.rectY(vg.from(searchTable, { filterBy: $filter }), {
      x: vg.bin('depth'),
      y: vg.count(),
      fill: 'steelblue',
    }),
    vg.intervalX({ as: $filter }),
    vg.width(350),
    vg.height(200),
    vg.xLabel('Depth'),
    vg.yLabel('Count'),
  );
  histRow.appendChild(plotPanel('Depth', depthHist));

  // Nodes histogram
  const nodesHist = vg.plot(
    vg.rectY(vg.from(searchTable, { filterBy: $filter }), {
      x: vg.bin('total_nodes'),
      y: vg.count(),
      fill: 'steelblue',
    }),
    vg.intervalX({ as: $filter }),
    vg.width(350),
    vg.height(200),
    vg.xLabel('Nodes'),
    vg.yLabel('Count'),
  );
  histRow.appendChild(plotPanel('Nodes', nodesHist));

  // Time histogram
  const timeHist = vg.plot(
    vg.rectY(vg.from(searchTable, { filterBy: $filter }), {
      x: vg.bin('total_time_ms'),
      y: vg.count(),
      fill: 'steelblue',
    }),
    vg.intervalX({ as: $filter }),
    vg.width(350),
    vg.height(200),
    vg.xLabel('Time (ms)'),
    vg.yLabel('Count'),
  );
  histRow.appendChild(plotPanel('Search Time', timeHist));

  container.appendChild(histRow);

  // TT stats scatter
  const ttScatter = vg.plot(
    vg.dot(vg.from(searchTable, { filterBy: $filter }), {
      x: 'tt_hits',
      y: 'nodes',
      fill: 'engine_id',
      opacity: 0.4,
      r: 2,
    }),
    vg.width(600),
    vg.height(300),
    vg.marginLeft(60),
    vg.xLabel('TT Hits'),
    vg.yLabel('Nodes'),
    vg.colorLegend(true),
  );
  container.appendChild(plotPanel('TT Hits vs Nodes', ttScatter));

  // Fail high ratio scatter
  const fhScatter = vg.plot(
    vg.dot(vg.from(searchTable, { filterBy: $filter }), {
      x: 'depth',
      y: 'fail_highs',
      fill: 'engine_id',
      opacity: 0.4,
      r: 2,
    }),
    vg.width(600),
    vg.height(300),
    vg.marginLeft(60),
    vg.xLabel('Depth'),
    vg.yLabel('Fail Highs'),
    vg.colorLegend(true),
  );
  container.appendChild(plotPanel('Fail Highs by Depth', fhScatter));

  return container;
}
