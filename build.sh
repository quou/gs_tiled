mkdir -p bin
mkdir -p obj

if [ src/gs.c -nt obj/gs.o ]; then
	cc -c -Igunslinger -Igs_xml src/gs.c -o obj/gs.o
fi

cc -Igunslinger -Igs_xml src/main.c obj/gs.o -lglfw -lm -ldl -lpthread -o bin/gs_tiled
