CXX = g++
CXXFLAGS = -g
LPFLAG = -lpthread

all: server

server: support.o httpConnection.o webServer.o locker.o logger.o main.o
	$(CXX) $(CXXFLAGS) support.o httpConnection.o webServer.o locker.o logger.o main.o -o server $(LPFLAG) 

locker.o: ./lock/locker.cpp ./lock/locker.h
	$(CXX) $(CXXFLAGS) -c ./lock/locker.cpp

logger.o: ./log/logger.cpp ./log/logger.h ./log/blockQueue.h ./lock/locker.h
	$(CXX) $(CXXFLAGS) -c ./log/logger.cpp

support.o: ./src/support.cpp ./src/support.h
	$(CXX) $(CXXFLAGS) -c ./src/support.cpp

httpConnection.o: ./http/httpConnection.cpp ./http/httpConnection.h ./src/support.h ./log/logger.h ./lock/locker.h\
				./threadpool/threadPool.h  	./timer/timeWheelTimer.h
	$(CXX) $(CXXFLAGS) -c ./http/httpConnection.cpp

webServer.o: ./web_server/webServer.cpp ./web_server/webServer.h ./http/httpConnection.h ./src/support.h ./log/logger.h \
			./threadpool/threadPool.h ./timer/timeWheelTimer.h 
	$(CXX) $(CXXFLAGS) -c ./web_server/webServer.cpp

main.o: ./main.cpp ./log/logger.h ./web_server/webServer.h
	$(CXX) $(CXXFLAGS) -c ./main.cpp

clean:
	rm  -f server *.o
