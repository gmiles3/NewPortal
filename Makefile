LDFLAGS=-lpcap
CCFLAGS=-Wall -Wpedantic -g -O0

all: NodeStateMachine HubStateMachine wifibroadcast/tx wifibroadcast/rx

%.o: %.c
	gcc -c -o $@ $< $(CCFLAGS)


wifibroadcast/rx: wifibroadcast/rx.o wifibroadcast/lib.o wifibroadcast/radiotap.o wifibroadcast/fec.o
	gcc -o $@ $^ $(LDFLAGS)


wifibroadcast/tx: wifibroadcast/tx.o wifibroadcast/lib.o wifibroadcast/radiotap.o wifibroadcast/fec.o
	gcc -o $@ $^ $(LDFLAGS)

NodeStateMachine: NodeStateMachine.o wifibroadcast/libtx.o wifibroadcast/librx.o wifibroadcast/lib.o wifibroadcast/fec.o wifibroadcast/radiotap.o
	gcc -o $@ $^ $(LDFLAGS)

HubStateMachine: HubStateMachine.o wifibroadcast/libtx.o wifibroadcast/librx.o wifibroadcast/lib.o wifibroadcast/fec.o wifibroadcast/radiotap.o
	gcc -o $@ $^ $(LDFLAGS)


clean:
	rm -f wifibroadcast/rx wifibroadcast/tx *~ *.o wifibroadcast/*.o NodeStateMachine HubStateMachine
