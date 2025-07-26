CROSS = gcc
BASEFLAGS = -g -Wall -march=armv8.2-a+sve2
OBJS = main.o scene.o bvh.o

# Default build (Regression mode)
CFLAGS = $(BASEFLAGS)

all: raytrace.elf

raytrace.elf: $(OBJS)
	$(CROSS) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CROSS) $(CFLAGS) -c $< -o $@

# Golden Reference Generation
generate: clean
	$(MAKE) CFLAGS="$(BASEFLAGS) -DGENERATE_GOLDEN" raytrace.elf
	mkdir -p reference_ppm
	qemu-aarch64 -cpu max,sve=on ./raytrace.elf
	$(MAKE) header

# Regression Test
regression: clean
	$(MAKE) CFLAGS="$(BASEFLAGS)" raytrace.elf
	qemu-aarch64 -cpu max,sve=on ./raytrace.elf

# Convert generated PPMs into reference_images.h
header:
	python3 ppm_to_header.py > reference_images.h

clean:
	rm -f *.o raytrace.elf
	rm -rf reference_ppm

