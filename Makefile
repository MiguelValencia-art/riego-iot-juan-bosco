CXX ?= g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

test: test_control
	./test_control

sim: sim_escenarios
	./sim_escenarios

test_control: test_control.cpp ../firmware/riego_iot/control_riego.h ../firmware/riego_iot/sensor_utils.h
	$(CXX) $(CXXFLAGS) test_control.cpp -o test_control

sim_escenarios: sim_escenarios.cpp ../firmware/riego_iot/control_riego.h ../firmware/riego_iot/sensor_utils.h
	$(CXX) $(CXXFLAGS) sim_escenarios.cpp -o sim_escenarios

clean:
	rm -f test_control sim_escenarios
