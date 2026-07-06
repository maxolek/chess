/**
 * DuckDB-WASM connection manager for the chess analytics dashboard.
 * 
 * Loads a local .duckdb file into DuckDB-WASM and exposes it through
 * the Mosaic coordinator for cross-filtered visualizations.
 */
import { coordinator, wasmConnector } from '@uwdata/mosaic-core';
import * as vg from '@uwdata/vgplot';

let _db = null;
let _ready = false;

/**
 * Initialize the DuckDB-WASM connection and Mosaic coordinator.
 * @param {string} dbPath - Path or URL to the .duckdb file (or Parquet files)
 */
export async function initDatabase(dbPath) {
  const wasm = wasmConnector();
  coordinator().databaseConnector(wasm);
  
  // Attach the analytics database
  await coordinator().exec(`ATTACH '${dbPath}' AS analytics (READ_ONLY)`);
  await coordinator().exec(`USE analytics`);
  
  _db = wasm;
  _ready = true;
  return wasm;
}

/**
 * Initialize from a local file selected by the user via file picker.
 */
export async function initFromFile(file) {
  const wasm = wasmConnector();
  coordinator().databaseConnector(wasm);
  
  // Register the file with DuckDB-WASM
  const arrayBuffer = await file.arrayBuffer();
  await coordinator().exec([
    `ATTACH ':memory:' AS analytics`,
    `USE analytics`,
  ]);
  
  // Load from the uploaded file
  const uint8 = new Uint8Array(arrayBuffer);
  await wasm.db.registerFileBuffer(file.name, uint8);
  await coordinator().exec(`ATTACH '${file.name}' AS analytics (READ_ONLY)`);
  await coordinator().exec(`USE analytics`);
  
  _db = wasm;
  _ready = true;
  return wasm;
}

/**
 * Run a raw SQL query and return results as an array of objects.
 */
export async function query(sql) {
  const result = await coordinator().query(sql);
  return result;
}

/**
 * Get table names in the database.
 */
export async function getTables() {
  const result = await coordinator().query(`SHOW TABLES`);
  return result.map(r => r.name);
}

/**
 * Get row count for a table.
 */
export async function getCount(table) {
  const result = await coordinator().query(`SELECT COUNT(*) as n FROM ${table}`);
  return result[0]?.n ?? 0;
}

/**
 * Check if a table exists.
 */
export async function tableExists(table) {
  try {
    const tables = await getTables();
    return tables.includes(table);
  } catch {
    return false;
  }
}

export function isReady() { return _ready; }
