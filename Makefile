# Makefile for Yarc library.

all:
	cd Source && make all
	cd TestClient && make all
	cd YarcTester/Source && make all

clean:
	cd Source && make clean
	cd TestClient && make clean
	cd YarcTester/Source && make clean
