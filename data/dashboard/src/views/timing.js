/**
 * Timing tab — function-level timing breakdown from search profiling.
 */
import { coordinator, Selection } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { sql, el, panel, plotPanel, fmt, metricCard } from '../util.js';

export async function renderTiming() {
  const tables = window.__tables || [];
  if (!tables.includes('search_timings')) {
    return el('div', { class: 'panel' }, el('p', {}, 'No search_timings table found.'));
  }

  const container = el('div', {});

  // Summary: total time and call counts by function
  const summary = await sql(`
    SELECT 
      function,
      SUM(total_time_ms) as total_ms,
      SUM(num_calls) as total_calls,
      AVG(total_time_ms) as avg_ms_per_search
    FROM search_timings
    GROUP BY function
    ORDER BY total_ms DESC
    LIMIT 20
  `);

  // Top functions bar chart
  const funcBar = vg.plot(
    vg.barX(
      vg.from('search_timings', {
        select: { function: 'function', total_ms: vg.sql`SUM(total_time_ms)` },
        groupBy: ['function'],
        orderBy: [{ total_ms: 'desc' }],
        limit: 15,
      }),
      { x: 'total_ms', y: 'function', fill: 'steelblue', sort: { y: '-x' } }
    ),
    vg.width(700),
    vg.height(400),
    vg.marginLeft(180),
    vg.xLabel('Total Time (ms)'),
    vg.yLabel('Function'),
  );
  container.appendChild(plotPanel('Top Functions by Total Time', funcBar));

  // Call count bar
  const callBar = vg.plot(
    vg.barX(
      vg.from('search_timings', {
        select: { function: 'function', total_calls: vg.sql`SUM(num_calls)` },
        groupBy: ['function'],
        orderBy: [{ total_calls: 'desc' }],
        limit: 15,
      }),
      { x: 'total_calls', y: 'function', fill: 'var(--accent)', sort: { y: '-x' } }
    ),
    vg.width(700),
    vg.height(400),
    vg.marginLeft(180),
    vg.xLabel('Total Calls'),
    vg.yLabel('Function'),
  );
  container.appendChild(plotPanel('Top Functions by Call Count', callBar));

  // Time per call scatter
  const tpcPlot = vg.plot(
    vg.dot(
      vg.from('search_timings', {
        select: {
          function: 'function',
          avg_time_per_call: vg.sql`total_time_ms / NULLIF(num_calls, 0)`,
          num_calls: 'num_calls',
        },
      }),
      { x: 'num_calls', y: 'avg_time_per_call', fill: 'function', opacity: 0.5, r: 3 }
    ),
    vg.width(700),
    vg.height(350),
    vg.marginLeft(70),
    vg.xLabel('Calls'),
    vg.yLabel('Avg Time per Call (ms)'),
    vg.xScale('log'),
  );
  container.appendChild(plotPanel('Time per Call vs Call Count', tpcPlot));

  return container;
}
