## Start the CN 

## Wait few seconds

## Start the RN

## Connect the PI first
It will get .66 IP

## Connect the laptop second
It will get the .67 IP

## if it fails try running the connection comands few times, 
## if it does not work in less than 2 minutes kill the CN and RN and start again

# Connection sequence commands

```bash
#!/bin/bash

# --- CONFIGURATION ---
# Change these values based on the machine you are currently on
MODEM_DEV="/dev/cdc-wdm0"  # Or /dev/cdc-wdm2
WWAN_IFACE="wwan0"         # Or whatever your interface is named (e.g., wwan1)
# ---------------------

echo "1. Priming and Toggling Radio..."
sudo mbimcli -p -d ${MODEM_DEV} --query-subscriber-ready-status > /dev/null
sudo mbimcli -p -d ${MODEM_DEV} --query-registration-state > /dev/null
sudo mbimcli -p -d ${MODEM_DEV} --set-radio-state=off
sleep 2
sudo mbimcli -p -d ${MODEM_DEV} --set-radio-state=on

echo "2. Waiting 4 seconds for AMF..."
sleep 4
sudo mbimcli -p -d ${MODEM_DEV} --query-registration-state > /dev/null

echo "3. Attaching and Connecting..."
sudo mbimcli -p -d ${MODEM_DEV} --attach-packet-service
sudo mbimcli -p -d ${MODEM_DEV} --connect=session-id=0,access-string=oai.ipv4,ip-type=ipv4

echo "4. Setting Route and Pinging..."
sudo ip link set ${WWAN_IFACE} up
sudo ip addr flush dev ${WWAN_IFACE}
```

Add the ip, use correct commands bellow is for the PI
```bash
# NOTE: Ensure these IPs match the output from the 'connect' command above!
# ..... add <Device IP> peer <Gateway> dev <Interface>
sudo ip addr add 12.1.1.66 peer 12.1.1.65 dev ${WWAN_IFACE} 
# ..... route add <network IP /24>
sudo ip route add 12.1.1.0/24 dev ${WWAN_IFACE} 2>/dev/null
```
bellow is for the laptop
```bash
# 1. Assign the IP with its proper /29 subnet mask 
sudo ip addr add 12.1.1.67/29 dev ${WWAN_IFACE} 
# 2. Tell the kernel to route the 5G traffic via your specific Gateway 
sudo ip route add 12.1.1.0/24 via 12.1.1.68 dev ${WWAN_IFACE}


# if file exists error apears use this
sudo ip route replace 12.1.1.0/24 via 12.1.1.68 dev ${WWAN_IFACE}
```
finaly try the google dns ping (note: DNS queries do not work on the 5g network, thus ping google.com wont be succesfull)
```bash
# Ping the gateway to keep the RRC link alive
ping -I ${WWAN_IFACE} 8.8.8.8
```