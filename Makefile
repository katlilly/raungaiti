
all : raungaiti serialise multithread_decompress

raungaiti : raungaiti.cpp pack512.cpp
	g++ -Wall -march=native -std=c++11 pack512.cpp raungaiti.cpp -o raungaiti

serialise : write_compressed_lists.cpp pack512.cpp
	g++ -Wall -march=native -std=c++11 pack512.cpp write_compressed_lists.cpp -o serialise

multithread_decompress : multithreaded_decompress_lists.cpp pack512.cpp
	g++ -Wall -march=native -std=c++11 pack512.cpp multithreaded_decompress_lists.cpp -o multithread_decompress -lpthread


clean:
	- rm -f *.gcno *.gcda *.gcov raungaiti serialise multithread_decompress *~

