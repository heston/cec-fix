BUILD_DIR = ./build

all: cec-fix| $(BUILD_DIR)/

cec-fix: ir.o main.o
	g++ -Wall -L/opt/vc/lib -L/usr/lib -lbcm_host -lvchiq_arm -lvcos -lm -lpigpio -lpthread build/ir.o build/main.o -o build/cec-fix

main.o: main.cc
	g++ -Wall -c -I. -Iinclude -I/usr/include -I/opt/vc/include main.cc -o build/main.o

ir.o: ir.cpp
	g++ -Wall -c -Iinclude -I/usr/include ir.cpp -o build/ir.o

$(BUILD_DIR)/:
	mkdir -p $@

clean:
	rm $(BUILD_DIR)/*

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
