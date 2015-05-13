CVFLAGS=`pkg-config --cflags opencv`
LIBS=`pkg-config --libs opencv`

pub:
	g++ $(CVFLAGS) framepub.cpp -lzmq -pthread -o framepub $(LIBS)

sub:
	g++ $(CVFLAGS) framesub.cpp -lzmq -pthread -o framesub $(LIBS)
