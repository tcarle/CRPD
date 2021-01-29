
CXXFLAGS += $(shell otawa-config --cflags) -g3
LIBS += $(shell otawa-config --libs --rpath)
all: compute_crpd

compute_crpd: remap_task.o compute_crpd.o 
	$(CXX) $(CXXFLAGS)  -o $@ $^ $(LIBS)


remap_task: remap_task.o
	$(CXX) $(CXXFLAGS)  -o $@ $^ $(LIBS)


clean:
	rm *.o
