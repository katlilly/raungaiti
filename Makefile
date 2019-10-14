
all : raungaiti

raungaiti : raungaiti.cpp pack512.cpp
	g++ -Wall -march=native -std=c++11 pack512.cpp raungaiti.cpp -o raungaiti

clean:
	- rm -f *.gcno *.gcda *.gcov raungaiti *~

