import argparse
import os
import struct

parser = argparse.ArgumentParser()
parser.add_argument("input_file", help=".bin to append files to")
parser.add_argument("output_file")
parser.add_argument("append_file", nargs="*", help="File(s) to append")

args = parser.parse_args()

# grab metadata / length
in_file = open(args.input_file, "rb")
head = in_file.read(20)
head_off = 0

if head[:4] == b'RELO':
    num_relocs = struct.unpack('<I', head[4:8])[0]
    head_off = num_relocs * 4 + 8
    in_file.seek(head_off)
    head = in_file.read(20)

in_file_end = struct.unpack('<I', head[16:20])[0] & 0x1FFFFFF

in_file.seek(in_file_end + head_off)
metadata = in_file.read()
in_file.seek(0)

out_file = open(args.output_file, "wb")
out_file.write(in_file.read(in_file_end + head_off))

# header
out_file.write(b"APPFILES")
out_file.write(struct.pack("<I", len(args.append_file)))

for filename in args.append_file:
    basename = "doom-data/" + os.path.basename(filename)
    out_file.write(struct.pack("HxxI", len(basename), os.stat(filename).st_size))

for filename in args.append_file:
    basename = "doom-data/" + os.path.basename(filename)
    out_file.write(basename.encode())
    out_file.write(open(filename, "rb").read())

new_end = out_file.tell() - head_off
out_file.write(metadata)

# patch end pointer
out_file.seek(head_off + 16)
out_file.write(struct.pack("<I", 0x90000000 | new_end))
