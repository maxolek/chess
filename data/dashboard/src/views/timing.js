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
      total_ms/NULLIF(total_calls, 0)  as avg_ms_per_search
    FROM search_timings
    GROUP BY function
    ORDER BY total_ms DESC
    LIMIT 20
  `);

  // Top functions bar chart
  await coordinator().exec(`CREATE OR REPLACE TEMP VIEW timing_by_func AS SELECT "function", SUM(total_time_ms)/NULLIF(SUM(num_calls), 0) as avg_ms_per_search FROM search_timings WHERE "function" <> 'ROOT' GROUP BY "function" ORDER BY avg_ms_per_search DESC LIMIT 15`);
  const funcBar = vg.plot(
    vg.barX(
      vg.from('timing_by_func'),
      { x: 'avg_ms_per_search', y: 'function', fill: 'steelblue', sort: { y: '-x' } }
    ),
    vg.width(700),
    vg.height(400),
    vg.marginLeft(180),
    vg.xLabel('Total Time (ms)'),
    vg.yLabel('Function'),
  );
  container.appendChild(plotPanel('Top Functions by Total Time', funcBar));

  // Call count bar
  await coordinator().exec(`CREATE OR REPLACE TEMP VIEW calls_by_func AS SELECT "function", SUM(num_calls) as total_calls FROM search_timings GROUP BY "function" ORDER BY total_calls DESC LIMIT 15`);
  const callBar = vg.plot(
    vg.barX(
      vg.from('calls_by_func'),
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
  await coordinator().exec(`CREATE OR REPLACE TEMP VIEW timing_per_call AS SELECT "function", total_time_ms / NULLIF(num_calls, 0) as avg_time_per_call, num_calls FROM search_timings`);
  const tpcPlot = vg.plot(
    vg.dot(
      vg.from('timing_per_call'),
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
