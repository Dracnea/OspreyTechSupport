#!/bin/bash
uptime=$(cut -d " " -f 1 < /proc/uptime)
uptime=${uptime%.*}
if [[ $uptime -gt 40 ]]; then
	 sudo /opt/wala/loadallwala
	 sudo /opt/wala/wala -o stratum+tcp://fill_in_pool:5555 -u fill_in_wallet.workerID --max-temp=85 -t 350,350,350 -p 4
else
	sleep 40
	 sudo /opt/wala/loadallwala
	 sudo /opt/wala/wala -o stratum+tcp://fill_in_pool:5555 -u fill_in_wallet.workerID --max-temp=85 -t 350,350,350  -p 4
fi
#THE BELOW LINE VERY IMPORTANT, WE ARE USING FOR ANOTHER PERPOSE, PLEASE DO NOT REMOVE/EDIT
#{"running_mode":"0","a":"wala", "o":"stratum+tcp://fill_in_pool:5555", "u":"fill_in_wallet", "miner":"workerID", "fpga_clk_core":"350","fpga_clk_core1":"350","fpga_clk_core2":"350"}