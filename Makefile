OBJDIR := build

all: $(OBJDIR)/cec-fix | $(OBJDIR)/

$(OBJDIR)/cec-fix: $(OBJDIR)/ir.o $(OBJDIR)/main.o | $(OBJDIR)/
	g++ -Wall -L/opt/vc/lib -L/usr/lib -lbcm_host -lvchiq_arm -lvcos -lm -lpigpio -lpthread build/ir.o build/main.o -o build/cec-fix

$(OBJDIR)/main.o: main.cpp | $(OBJDIR)/
	g++ -Wall -c -I. -Iinclude -I/usr/include -I/opt/vc/include main.cpp -o build/main.o

$(OBJDIR)/ir.o: ir.cpp | $(OBJDIR)/
	g++ -Wall -c -Iinclude -I/usr/include ir.cpp -o build/ir.o

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
