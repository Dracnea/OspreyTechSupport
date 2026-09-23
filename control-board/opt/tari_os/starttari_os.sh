#!/bin/bash
uptime=$(cut -d " " -f 1 < /proc/uptime)
uptime=${uptime%.*}
if [[ $uptime -gt 40 ]]; then
	 sudo /opt/tari_os/loadalltari_os
	 sudo /opt/tari_os/tari_os -o stratum+tcp://fill_in_pool:5555 -u fill_in_wallet.workerID --max-temp=85 -t 525,525,525 -p 4
else
	sleep 40
	 sudo /opt/tari_os/loadalltari_os
	 sudo /opt/tari_os/tari_os -o stratum+tcp://fill_in_pool:5555 -u fill_in_wallet.workerID --max-temp=85 -t 525,525,525 -p 4
fi
#THE BELOW LINE VERY IMPORTANT, WE ARE USING FOR ANOTHER PERPOSE, PLEASE DO NOT REMOVE/EDIT
#{"running_mode":"0","a":"tari_os","o":"stratum+tcp://fill_in_pool:5555","u":"fill_in_wallet","miner":"workerID","fpga_clk_core":"525","fpga_clk_core1":"525","fpga_clk_core2":"525"}
