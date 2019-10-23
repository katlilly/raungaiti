
all : raungaiti serialise

raungaiti : raungaiti.cpp pack512.cpp
	g++ -Wall -march=native -std=c++11 pack512.cpp raungaiti.cpp -o raungaiti

serialise : write_compressed_lists.cpp pack512.cpp
	g++ -Wall -march=native -std=c++11 pack512.cpp write_compressed_lists.cpp -o serialise

clean:
	- rm -f *.gcno *.gcda *.gcov raungaiti serialise *~

