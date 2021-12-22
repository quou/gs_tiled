mkdir -p bin
mkdir -p obj

if [ src/gs.c -nt obj/gs.o ]; then
	cc -c -g -Igunslinger -Igs_xml src/gs.c -o obj/gs.o
fi

cc -c -g -Igunslinger -Igs_xml src/renderer.c -o obj/renderer.o

cc -g -Igunslinger -Igs_xml src/main.c obj/gs.o obj/renderer.o -lglfw -lm -ldl -lpthread -o bin/gs_tiled
