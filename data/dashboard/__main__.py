"""
Entry point: python -m data.dashboard
"""
import webbrowser

from .app import app
from . import layout      # noqa: F401 — registers app.layout
from . import callbacks   # noqa: F401 — registers callbacks

if __name__ == "__main__":
    webbrowser.open("http://127.0.0.1:8050")
    app.run(debug=True, port=8050)
