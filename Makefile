BUILD_DIR = ./build

cec-fix: main.cc ir | $(OUT)/
	g++ -Wall -I include -I /usr/include -I /opt/vc/include -L /opt/vc/lib -L /usr/lib -l bcm_host -l vchiq_arm -l vcos -p thread main.cc -o build/cec-fix

ir: ir.cpp | $(OUT)/
	g++ -Wall -I include -I /usr/include -l m -l pigpio ir.cpp -o build/ir

$(OUT)/:
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
