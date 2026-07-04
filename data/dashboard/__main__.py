"""
Entry point: python -m data.dashboard
"""
import webbrowser
import os 
import threading

from .app import app
from . import layout      # noqa: F401 — registers app.layout
from . import callbacks   # noqa: F401 — registers callbacks

if __name__ == "__main__":
    # only open browswer once (from reloader parent), with a delay
    # so server has time to sart in the child process
    if not os.environ.get("WERKZEUG_RUN_MAIN"):
        threading.Timer(1.5, webbrowser.open, args=["http://127.0.0.1:8050"]).start()
    app.run(debug=True, port=8050)
