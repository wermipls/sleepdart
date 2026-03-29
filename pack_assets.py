#!/bin/python3

import io
import sys
import zipfile
import os

def main(args):
    if len(args) < 4:
        print(f"At least 3 arguments required: {args[0]} output.h rootdir inputs")
        return 1

    outfile  = args[1]
    root_dir = args[2]
    infiles  = args[3:]

    with io.BytesIO() as f:
        with zipfile.ZipFile(f, "w") as zf:
            for infile in infiles:
                zf.write(infile, arcname=os.path.relpath(infile, root_dir))
            
        with open(outfile, 'w') as h:
            f.seek(0)
            h.write('#pragma once\n\n')
            h.write(f'static const unsigned char {outfile.removesuffix('.h')}[] = {{\n    ')
            for byte in f.read():
                h.write(f"0x{byte:x},")
            h.write("\n};")

        with open(outfile+".zip", "wb") as zip_debug:
            f.seek(0)
            zip_debug.write(f.read())

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
