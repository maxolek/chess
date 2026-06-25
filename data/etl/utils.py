"""Small helper utilities."""
import json


def safe_val(val):
    """Convert lists/dicts to JSON strings for DB insertion."""
    if isinstance(val, list) or isinstance(val, dict):
        return json.dumps(val)
    return val


def safe(arr, i):
    """Safe index into array, returning 0 if out of bounds."""
    return arr[i] if i < len(arr) else 0
