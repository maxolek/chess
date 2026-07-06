/**
 * Iterations tab — iterative deepening analysis.
 * Shows how metrics evolve per depth iteration within searches.
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, plotPanel, getIterTable, getSearchTable } from '../util.js';

export async function renderIterations() {
  const iterTable = getIterTable();
  if (!iterTable) {
    return el('div', { class: 'panel' }, el('p', {}, 'No iteration data table found.'));
  }

  const container = el('div', {});
  const $filter = Selection.crossfilter();

  // Depth selector (slider)
  const depthSlider = vg.slider({
    from: iterTable,
    column: 'depth',
    as: $filter,
    label: 'Max Depth',
  });

  const controls = el('div', { class: 'panel', style: { display: 'flex', gap: '16px', alignItems: 'center' } });
  controls.appendChild(depthSlider);
  container.appendChild(controls);

  // Nodes by iteration depth (aggregated)
  const nodesLine = vg.plot(
    vg.lineY(vg.from(iterTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('nodes'),
      stroke: 'var(--accent)',
      marker: true,
    }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(70),
    vg.xLabel('Iteration Depth'),
    vg.yLabel('Avg Nodes'),
  );
  container.appendChild(plotPanel('Nodes by Iteration Depth', nodesLine));

  // Eval by iteration depth
  const evalLine = vg.plot(
    vg.lineY(vg.from(iterTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('eval'),
      stroke: 'var(--accent2)',
      marker: true,
    }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(70),
    vg.xLabel('Iteration Depth'),
    vg.yLabel('Avg Eval'),
  );
  container.appendChild(plotPanel('Eval by Iteration Depth', evalLine));

  // Time by iteration depth
  const timeLine = vg.plot(
    vg.lineY(vg.from(iterTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('time_ms'),
      stroke: 'var(--success)',
      marker: true,
    }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(70),
    vg.xLabel('Iteration Depth'),
    vg.yLabel('Avg Time (ms)'),
  );
  container.appendChild(plotPanel('Time by Iteration Depth', timeLine));

  // Branching factor: nodes[d] / nodes[d-1] heatmap style
  const failHighLine = vg.plot(
    vg.lineY(vg.from(iterTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('fail_highs'),
      stroke: 'var(--warning)',
      marker: true,
    }),
    vg.width(700),
    vg.height(250),
    vg.marginLeft(70),
    vg.xLabel('Iteration Depth'),
    vg.yLabel('Avg Fail Highs'),
  );
  container.appendChild(plotPanel('Fail Highs by Iteration Depth', failHighLine));

  return container;
}
