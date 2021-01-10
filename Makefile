build:
	gcc producer.c -o producer.out -lm
	gcc consumer.c -o consumer.out -lm


all: clean build
