cec-fix: main.cc
	g++ -Wall -I include -I /usr/include -I /opt/vc/include -L /opt/vc/lib -L /usr/lib -l bcm_host -l vchiq_arm -l vcos -l lirc /usr/lib/arm-linux-gnueabihf/liblirc_client.so main.cc -o cec-fix

clean:
	rm cec-fix

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