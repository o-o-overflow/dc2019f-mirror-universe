#!/usr/bin/env python3
import sys

original_size = 24806400

if __name__ == "__main__":
    with open('/opt/usim/system-78-48.LOD', 'rb') as new:
        new_size = len(new.read())
        if new_size != original_size:
            print(f"PUBLIC: File size must match original. Yours was {new_size}, expected {original_size}")
            sys.exit(-1)
        else:
            sys.exit(0)
