CC = gcc
TARGET = benchmark

$(TARGET) : frecovery.o benchmark.o 
	$(CC) -pthread -o $(TARGET) frecovery.o benchmark.o -lrt