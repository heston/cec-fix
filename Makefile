BUILD_DIR = ./build

all: cec-fix

cec-fix: main.cc ir | $(BUILD_DIR)/
	g++ -Wall -Iinclude -I/usr/include -I/opt/vc/include -Lbuild -L/opt/vc/lib -L/usr/lib -lbcm_host -lvchiq_arm -lvcos ir main.cc -o build/cec-fix

ir: ir.cpp ir.hpp | $(BUILD_DIR)/
	g++ -Wall -Iinclude -I/usr/include -lm -lpigpio ir.cpp -o build/ir

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
