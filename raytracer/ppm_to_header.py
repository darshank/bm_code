#!/usr/bin/env python3
import glob
import re

WIDTH=640
HEIGHT=480
IMG_SIZE=WIDTH*HEIGHT*3

ppm_files=sorted(glob.glob("reference_ppm/output_*.ppm"),key=lambda x:int(re.findall(r'\d+',x)[0]))
print("#include <stdint.h>")
print(f"#define NUM_REF_IMAGES {len(ppm_files)}")
print(f"#define IMG_SIZE ({WIDTH}*{HEIGHT}*3)")
print("static const uint8_t reference_images[NUM_REF_IMAGES][IMG_SIZE]={")
for ppm in ppm_files:
    with open(ppm,"rb") as f:
        f.readline();f.readline();f.readline(); # Skip header
        data=f.read()
        if len(data)!=IMG_SIZE:
            raise ValueError(f"{ppm} size mismatch!")
        vals=",".join(str(b) for b in data)
        print("{"+vals+"},")
print("};")

