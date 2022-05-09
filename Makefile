OBJDIR := build

all: $(OBJDIR)/cec-fix | $(OBJDIR)/

$(OBJDIR)/cec-fix: $(OBJDIR)/lan.o $(OBJDIR)/main.o | $(OBJDIR)/
	g++ -Wall -L/usr/lib $(OBJDIR)/lan.o $(OBJDIR)/main.o -lbcm_host -lvchiq_arm -lvcos -lpthread -o $(OBJDIR)/cec-fix

$(OBJDIR)/main.o: lan.hpp main.cpp | $(OBJDIR)/
	g++ -Wall -c -I. -Iinclude -I/usr/include -I/opt/vc/include main.cpp -o $(OBJDIR)/main.o

$(OBJDIR)/lan.o: lan.hpp lan.cpp | $(OBJDIR)/
	g++ -Wall -c -I. -Iinclude -I/usr/include lan.cpp -o $(OBJDIR)/lan.o

$(OBJDIR)/lan-test: lan-test.cpp $(OBJDIR)/lan.o | $(OBJDIR)/
	g++ -Wall -I. -Iinclude lan-test.cpp $(OBJDIR)/lan.o -o $(OBJDIR)/lan-test

$(OBJDIR)/fifo.o: fifo.hpp fifo.cpp | $(OBJDIR)/
	g++ -Wall -c -I. -Iinclude -I/usr/include fifo.cpp -o $(OBJDIR)/fifo.o

$(OBJDIR)/fifo-test: fifo-test.cpp $(OBJDIR)/fifo.o | $(OBJDIR)/
	g++ -Wall -I. -Iinclude fifo-test.cpp $(OBJDIR)/fifo.o -o $(OBJDIR)/fifo-test

$(OBJDIR)/:
	mkdir -p $@

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
