/**
 * Tree Depth tab — analysis of search tree depth statistics.
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, plotPanel, getTreeTable } from '../util.js';

export async function renderTree() {
  const treeTable = getTreeTable();
  if (!treeTable) {
    return el('div', { class: 'panel' }, el('p', {}, 'No tree depth data table found.'));
  }

  const container = el('div', {});
  const $filter = Selection.crossfilter();

  // Nodes by tree depth
  const nodesLine = vg.plot(
    vg.lineY(vg.from(treeTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('nodes'),
      stroke: 'var(--accent)',
      marker: true,
    }),
    vg.intervalX({ as: $filter }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(70),
    vg.xLabel('Tree Depth (ply)'),
    vg.yLabel('Avg Nodes'),
  );
  container.appendChild(plotPanel('Nodes by Tree Depth', nodesLine));

  // QNodes by tree depth
  const qnodesLine = vg.plot(
    vg.lineY(vg.from(treeTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('qnodes'),
      stroke: 'var(--accent2)',
      marker: true,
    }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(70),
    vg.xLabel('Tree Depth (ply)'),
    vg.yLabel('Avg QNodes'),
  );
  container.appendChild(plotPanel('QNodes by Tree Depth', qnodesLine));

  // Fail highs / lows at each depth
  const failPlot = vg.plot(
    vg.lineY(vg.from(treeTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('fail_highs'),
      stroke: 'var(--warning)',
      marker: true,
    }),
    vg.lineY(vg.from(treeTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('fail_lows'),
      stroke: 'var(--danger)',
      marker: true,
    }),
    vg.width(700),
    vg.height(300),
    vg.marginLeft(70),
    vg.xLabel('Tree Depth (ply)'),
    vg.yLabel('Count'),
  );
  container.appendChild(plotPanel('Fail Highs/Lows by Tree Depth', failPlot));

  // NMP by tree depth
  const nmpLine = vg.plot(
    vg.lineY(vg.from(treeTable, { filterBy: $filter }), {
      x: 'depth',
      y: vg.avg('nmp'),
      stroke: 'var(--success)',
      marker: true,
    }),
    vg.width(700),
    vg.height(250),
    vg.marginLeft(70),
    vg.xLabel('Tree Depth (ply)'),
    vg.yLabel('Avg NMP Activations'),
  );
  container.appendChild(plotPanel('Null Move Pruning by Tree Depth', nmpLine));

  return container;
}
