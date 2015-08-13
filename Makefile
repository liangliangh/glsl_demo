all:
	g++ -o a.out main.cpp -lpthread -lgomp -lopencv_core -lopencv_highgui -lX11 -lGL # GLX
clean:
	rm -f a.out
	rm -f out.png
