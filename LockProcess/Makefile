# part of the makefile

MAIN: VISION  SERIAL SOCKECT  main.c
	gcc main.c -o main
VISION:
	g++ ./Vision/Vision.cpp -o ./Vision/Vision  `pkg-config --cflags --libs opencv`
SOCKECT:
	gcc ./Sockect/Sockect.c -o ./Sockect/Sockect
SERIAL:
	gcc ./Serial/Serial.c -o ./Serial/Serial
