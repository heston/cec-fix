OBJDIR := build
CXXFLAGS := -std=c++11 -Wall -pthread -I. -Iinclude

all: $(OBJDIR)/cec-fix | $(OBJDIR)/

$(OBJDIR)/cec-fix: $(OBJDIR)/fifo.o $(OBJDIR)/lan.o $(OBJDIR)/main.o | $(OBJDIR)/
	g++ -Wall -L/usr/lib $(OBJDIR)/fifo.o $(OBJDIR)/lan.o $(OBJDIR)/main.o -lbcm_host -lvchiq_arm -lvcos -lpthread -o $(OBJDIR)/cec-fix

$(OBJDIR)/main.o: lan.hpp fifo.hpp cec-event-queue.hpp cec-notifications.hpp main.cpp | $(OBJDIR)/
	g++ -Wall -c -I. -Iinclude -I/usr/include -I/opt/vc/include main.cpp -o $(OBJDIR)/main.o

$(OBJDIR)/lan.o: lan.hpp lan.cpp include/socket_with_timeout.h | $(OBJDIR)/
	g++ -Wall -c -I. -Iinclude -I/usr/include lan.cpp -o $(OBJDIR)/lan.o

$(OBJDIR)/lan-test: lan-test.cpp $(OBJDIR)/lan.o | $(OBJDIR)/
	g++ -Wall -I. -Iinclude lan-test.cpp $(OBJDIR)/lan.o -o $(OBJDIR)/lan-test

$(OBJDIR)/fifo.o: fifo.hpp fifo.cpp | $(OBJDIR)/
	g++ -Wall -c -I. -Iinclude -I/usr/include fifo.cpp -o $(OBJDIR)/fifo.o

$(OBJDIR)/fifo-test: fifo-test.cpp $(OBJDIR)/fifo.o | $(OBJDIR)/
	g++ -Wall -I. -Iinclude fifo-test.cpp $(OBJDIR)/fifo.o -o $(OBJDIR)/fifo-test

$(OBJDIR)/:
	mkdir -p $@

$(OBJDIR)/lan-probe: tests/lan-probe.cpp lan.cpp lan.hpp include/socket_with_timeout.h | $(OBJDIR)/
	$(CXX) $(CXXFLAGS) tests/lan-probe.cpp lan.cpp -o $@

$(OBJDIR)/fifo-regression: tests/fifo-regression.cpp fifo.cpp fifo.hpp | $(OBJDIR)/
	$(CXX) $(CXXFLAGS) tests/fifo-regression.cpp fifo.cpp -o $@

.PHONY: test
$(OBJDIR)/cec-queue-regression: tests/cec-queue-regression.cpp cec-event-queue.hpp | $(OBJDIR)/
	$(CXX) $(CXXFLAGS) tests/cec-queue-regression.cpp -o $@

$(OBJDIR)/socket-timeout-regression: tests/socket-timeout-regression.cpp include/socket_with_timeout.h | $(OBJDIR)/
	$(CXX) $(CXXFLAGS) tests/socket-timeout-regression.cpp -o $@

$(OBJDIR)/cec-notifications-regression: tests/cec-notifications-regression.cpp cec-notifications.hpp | $(OBJDIR)/
	$(CXX) $(CXXFLAGS) tests/cec-notifications-regression.cpp -o $@

test: $(OBJDIR)/lan-probe $(OBJDIR)/fifo-regression $(OBJDIR)/cec-queue-regression $(OBJDIR)/socket-timeout-regression $(OBJDIR)/cec-notifications-regression
	CEC_LAN_PROBE=$(abspath $(OBJDIR))/lan-probe python3 tests/test_lan.py
	$(OBJDIR)/fifo-regression
	$(OBJDIR)/cec-queue-regression
	$(OBJDIR)/socket-timeout-regression
	$(OBJDIR)/cec-notifications-regression

clean:
	rm $(OBJDIR)/*

.PHONY: install
install:
	sudo sed "s|{{DIR}}|$$(dirname $$(realpath cecfix.service))|g" \
		cecfix.service \
		> /lib/systemd/system/cecfix.service
	sudo chmod 644 /lib/systemd/system/cecfix.service
	sudo systemctl daemon-reload
	sudo systemctl enable cecfix.service
	sudo systemctl start cecfix
	sudo systemctl status cecfix

.PHONY: uninstall
uninstall:
	sudo systemctl stop cecfix
	sudo systemctl disable cecfix.service
	sudo rm /lib/systemd/system/cecfix.service
	sudo systemctl daemon-reload
