/**
 * Main entry point for the Chess Analytics Dashboard.
 * Initializes DuckDB-WASM, sets up navigation, and renders views.
 */
import { coordinator, wasmConnector } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';
import { getCount, getTables, tableExists } from './connection.js';
import { renderOverview } from './views/overview.js';
import { renderSearch } from './views/search.js';
import { renderGames } from './views/games.js';
import { renderTrends } from './views/trends.js';
import { renderCompare } from './views/compare.js';
import { renderIterations } from './views/iterations.js';
import { renderTree } from './views/tree.js';
import { renderTiming } from './views/timing.js';
import { renderSprt } from './views/sprt.js';

// ─────────────────────────────────────────────────────────────────────────────
// TABS CONFIGURATION
// ─────────────────────────────────────────────────────────────────────────────

const TABS = [
  { id: 'overview',   label: 'Overview',    render: renderOverview },
  { id: 'search',     label: 'Searches',    render: renderSearch },
  { id: 'games',      label: 'Games',       render: renderGames },
  { id: 'trends',     label: 'Trends',      render: renderTrends },
  { id: 'compare',    label: 'Compare',     render: renderCompare },
  { id: 'iterations', label: 'Iterations',  render: renderIterations },
  { id: 'tree',       label: 'Tree Depth',  render: renderTree },
  { id: 'timing',     label: 'Timing',      render: renderTiming },
  { id: 'sprt',       label: 'SPRT',        render: renderSprt },
];

let activeTab = 'overview';

// ─────────────────────────────────────────────────────────────────────────────
// INITIALIZATION
// ─────────────────────────────────────────────────────────────────────────────

async function init() {
  const status = document.getElementById('db-status');
  
  try {
    status.textContent = 'Initializing DuckDB-WASM...';
    
    const wasm = await wasmConnector();
    coordinator().databaseConnector(wasm);
    
    status.textContent = 'Loading database...';
    
    // Try to load from the default path (configure this for your setup)
    // For local development, we'll use a file input fallback
    const dbPath = getDbPath();
    
    if (dbPath) {
      await coordinator().exec(`ATTACH '${dbPath}' AS db (READ_ONLY)`);
      await coordinator().exec(`USE db`);
    } else {
      // Show file picker
      showFilePicker(status);
      return;
    }
    
    status.textContent = 'Connected';
    status.className = 'connected';
    
    await setupDashboard();
  } catch (e) {
    console.error('Init failed:', e);
    status.textContent = `Error: ${e.message}`;
    status.className = 'error';
    showFilePicker(status);
  }
}

function getDbPath() {
  // Check URL params for db path
  const params = new URLSearchParams(window.location.search);
  return params.get('db') || null;
}

function showFilePicker(status) {
  const content = document.getElementById('content');
  content.innerHTML = `
    <div class="panel" style="max-width: 500px; margin: 60px auto; text-align: center;">
      <div class="panel-title">Load Database</div>
      <p style="color: var(--text-sec); margin-bottom: 16px; font-size: 13px;">
        Select your DuckDB analytics file, or pass <code>?db=path/to/file.duckdb</code> in the URL.
      </p>
      <input type="file" id="db-file-input" accept=".duckdb,.db,.parquet" 
             style="margin: 16px 0;" />
      <p style="color: var(--text-sec); font-size: 11px; margin-top: 12px;">
        Supports .duckdb files. All processing happens in your browser — no data is uploaded.
      </p>
    </div>
  `;
  
  document.getElementById('db-file-input').addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    
    status.textContent = `Loading ${file.name}...`;
    try {
      const arrayBuffer = await file.arrayBuffer();
      const uint8 = new Uint8Array(arrayBuffer);
      
      // Register and attach
      await coordinator().exec(`INSTALL httpfs`);
      // For local file, use opfs or direct buffer registration
      const db = coordinator().databaseConnector();
      await db.db.registerFileBuffer(file.name, uint8);
      await coordinator().exec(`ATTACH '${file.name}' AS db (READ_ONLY)`);
      await coordinator().exec(`USE db`);
      
      status.textContent = 'Connected';
      status.className = 'connected';
      await setupDashboard();
    } catch (err) {
      console.error('File load failed:', err);
      status.textContent = `Error: ${err.message}`;
      status.className = 'error';
    }
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// DASHBOARD SETUP
// ─────────────────────────────────────────────────────────────────────────────

async function setupDashboard() {
  // Detect available tables
  const tables = await detectTables();
  
  // Render tabs
  renderTabs();
  
  // Render initial tab
  await switchTab('overview');
}

async function detectTables() {
  try {
    const result = await coordinator().query(`SHOW TABLES`);
    const tables = result.map(r => r.name);
    window.__tables = tables;
    return tables;
  } catch {
    window.__tables = [];
    return [];
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// TAB NAVIGATION
// ─────────────────────────────────────────────────────────────────────────────

function renderTabs() {
  const nav = document.getElementById('tabs');
  nav.innerHTML = '';
  
  for (const tab of TABS) {
    const btn = document.createElement('button');
    btn.className = `tab-btn${tab.id === activeTab ? ' active' : ''}`;
    btn.textContent = tab.label;
    btn.dataset.tab = tab.id;
    btn.addEventListener('click', () => switchTab(tab.id));
    nav.appendChild(btn);
  }
}

async function switchTab(tabId) {
  activeTab = tabId;
  
  // Update button styles
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.tab === tabId);
  });
  
  // Render tab content
  const content = document.getElementById('content');
  content.innerHTML = '<div class="loading">Loading...</div>';
  
  const tab = TABS.find(t => t.id === tabId);
  if (tab) {
    try {
      const el = await tab.render();
      content.innerHTML = '';
      if (typeof el === 'string') {
        content.innerHTML = el;
      } else {
        content.appendChild(el);
      }
    } catch (e) {
      console.error(`Tab ${tabId} render failed:`, e);
      content.innerHTML = `<div class="panel"><p style="color: var(--danger);">Error: ${e.message}</p></div>`;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// BOOT
// ─────────────────────────────────────────────────────────────────────────────

init();
