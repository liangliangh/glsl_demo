all:
	g++ -o a.out main.cpp -O3 -lpthread -lgomp -lopencv_core -lopencv_highgui -lX11 -lGL # GLX
clean:
	rm -f a.out
	rm -f out.png
