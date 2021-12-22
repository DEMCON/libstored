import sys

def is_venv():
    return (hasattr(sys, 'real_prefix') or
        (hasattr(sys, 'base_prefix') and sys.base_prefix != sys.prefix))

sys.exit(1 if is_venv() else 0)
