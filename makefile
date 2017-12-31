all:UDPSocket Tahoe Reno

UDPSocket:UDPSocket.cpp
	g++ -pthread UDPSocket.cpp -o UDPSocket
	
Tahoe:Tahoe.cpp
	g++ -pthread Tahoe.cpp -o Tahoe
	
Reno:Reno.cpp
	g++ -pthread Reno.cpp -o Reno
	
clean:
	rm -f UDPSocket
