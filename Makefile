# Makefile for Yarc library.

all:
	cd Source && make all
	cd TestClient && make all

clean:
	cd Source && make clean
	cd TestClient && make clean
