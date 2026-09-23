#!/bin/bash
uptime=$(cut -d " " -f 1 < /proc/uptime)
uptime=${uptime%.*}
if [[ $uptime -gt 40 ]]; then
	 sudo /opt/astrix/loadallastrix
	 sudo /opt/astrix/astrix -o stratum+tcp://fill_in_pool:5555 -u fill_in_wallet.workerID --max-temp=85 -t 500,500,500 -p 4
else
	sleep 40
	 sudo /opt/astrix/loadallastrix
	 sudo /opt/astrix/astrix -o stratum+tcp://fill_in_pool:5555 -u fill_in_wallet.workerID --max-temp=85 -t 500,500,500 -p 4
fi
#THE BELOW LINE VERY IMPORTANT, WE ARE USING FOR ANOTHER PERPOSE, PLEASE DO NOT REMOVE/EDIT
#{"running_mode":"0","a":"astrix","o":"stratum+tcp://fill_in_pool:5555","u":"fill_in_wallet","miner":"workerID","fpga_clk_core":"500","fpga_clk_core1":"500","fpga_clk_core2":"500"}
