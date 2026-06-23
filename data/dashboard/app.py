"""
Dash application instance.
"""
import dash

app = dash.Dash(__name__, suppress_callback_exceptions=True)
app.title = "Chess Engine Analytics"

# Custom index with fonts and styles
app.index_string = """
<!DOCTYPE html>
<html>
<head>
    {%metas%}
    <title>{%title%}</title>
    {%favicon%}
    {%css%}
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600;700&family=Syne:wght@400;600;800&display=swap" rel="stylesheet">
    <style>
        *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            background: #0d0f14;
            color: #e8eaf0;
            font-family: 'Syne', sans-serif;
            min-height: 100vh;
            -webkit-font-smoothing: antialiased;
        }
        ::-webkit-scrollbar { width: 5px; height: 5px; }
        ::-webkit-scrollbar-track { background: transparent; }
        ::-webkit-scrollbar-thumb { background: #252a3866; border-radius: 4px; }
        ::-webkit-scrollbar-thumb:hover { background: #00d2ff55; }

        .Select-control { background-color: #1a1f2e !important; border-color: #252a38 !important; color: #e8eaf0 !important; border-radius: 6px !important; min-height: 32px !important; }
        .Select-menu-outer { background-color: #1a1f2e !important; border-color: #252a38 !important; border-radius: 0 0 6px 6px !important; }
        .Select-option { background-color: #1a1f2e !important; color: #e8eaf0 !important; padding: 6px 10px !important; }
        .Select-option:hover, .Select-option.is-focused { background-color: #00d2ff18 !important; }
        .Select-value-label { color: #e8eaf0 !important; }
        .Select-placeholder { color: #8892a4 !important; font-size: 12px !important; }
        .Select-arrow { border-top-color: #8892a4 !important; }
        .VirtualizedSelectOption { background-color: #1a1f2e !important; color: #e8eaf0 !important; }
        .Select-multi-value-wrapper .Select-value { background-color: #00d2ff18 !important; border-color: #00d2ff44 !important; border-radius: 4px !important; }
        .Select-multi-value-wrapper .Select-value-label { color: #00d2ff !important; font-size: 11px !important; }

        .tab-content { animation: fadeIn 0.18s ease-out; }
        @keyframes fadeIn { from { opacity: 0; transform: translateY(6px); } to { opacity: 1; transform: translateY(0); } }

        .metric-card {
            background: linear-gradient(135deg, #141720 0%, #181d2a 100%);
            border: 1px solid #252a38;
            border-radius: 10px;
            padding: 18px 20px 14px;
            position: relative;
            overflow: hidden;
            transition: border-color 0.2s, box-shadow 0.2s, transform 0.15s;
        }
        .metric-card:hover {
            border-color: #00d2ff44;
            box-shadow: 0 4px 20px rgba(0, 210, 255, 0.06);
            transform: translateY(-1px);
        }
        .metric-card::before {
            content: '';
            position: absolute;
            top: 0; left: 0; right: 0;
            height: 2px;
            background: linear-gradient(90deg, #00d2ff, #ff6b35);
            opacity: 0.7;
        }
        .metric-val {
            font-family: 'JetBrains Mono', monospace;
            font-size: 24px;
            font-weight: 700;
            color: #00d2ff;
            line-height: 1.2;
        }
        .metric-lbl {
            font-size: 9px;
            font-weight: 700;
            letter-spacing: 0.12em;
            text-transform: uppercase;
            color: #8892a4;
            margin-top: 6px;
        }

        .section-title {
            font-family: 'JetBrains Mono', monospace;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 0.18em;
            text-transform: uppercase;
            color: #00d2ff;
            border-bottom: 1px solid #252a38;
            padding-bottom: 6px;
            margin-bottom: 12px;
        }

        .panel {
            background: #141720;
            border: 1px solid #252a38;
            border-radius: 10px;
            padding: 16px 18px;
            transition: border-color 0.2s, box-shadow 0.2s;
        }
        .panel:hover {
            border-color: #252a3888;
            box-shadow: 0 2px 16px rgba(0, 0, 0, 0.25);
        }

        .dash-table-container .dash-spreadsheet-container .dash-spreadsheet-inner td,
        .dash-table-container .dash-spreadsheet-container .dash-spreadsheet-inner th {
            background-color: #141720 !important;
            color: #e8eaf0 !important;
            border-color: #1e2333 !important;
            font-family: 'JetBrains Mono', monospace !important;
            font-size: 11px !important;
            padding: 5px 8px !important;
        }
        .dash-table-container .dash-spreadsheet-container .dash-spreadsheet-inner th {
            background-color: #0f1118 !important;
            color: #6b7a8d !important;
            font-weight: 700 !important;
            font-size: 10px !important;
            letter-spacing: 0.08em !important;
            text-transform: uppercase !important;
        }
        .dash-table-container .dash-spreadsheet-container .dash-spreadsheet-inner tr:hover td {
            background-color: #00d2ff08 !important;
        }
        .dash-table-container .previous-next-container button,
        .dash-table-container .previous-page, .dash-table-container .next-page,
        .dash-table-container .first-page, .dash-table-container .last-page {
            background-color: #1a1f2e !important;
            color: #e8eaf0 !important;
            border-color: #252a38 !important;
            border-radius: 4px !important;
            font-size: 11px !important;
        }
        input[type="text"] {
            background-color: #1a1f2e !important;
            color: #e8eaf0 !important;
            border-color: #252a38 !important;
            border-radius: 4px !important;
        }

        .rc-slider-track { background-color: #00d2ff !important; }
        .rc-slider-handle { border-color: #00d2ff !important; background-color: #00d2ff !important; }

        .modebar { opacity: 0; transition: opacity 0.2s; }
        .js-plotly-plot:hover .modebar { opacity: 1; }
        .modebar-btn path { fill: #8892a4 !important; }
        .modebar-btn:hover path { fill: #00d2ff !important; }

        .tab--selected { position: relative; }

        .metric-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
            gap: 12px;
            margin-bottom: 12px;
        }

        /* Loading spinner override */
        .dash-loading { color: #00d2ff !important; }
        ._dash-loading-callback { background-color: rgba(13,15,20,0.6) !important; }
    </style>
</head>
<body>
    {%app_entry%}
    <footer>
        {%config%}
        {%scripts%}
        {%renderer%}
    </footer>
</body>
</html>
"""
