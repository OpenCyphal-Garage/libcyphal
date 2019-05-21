Libuavcan Build and Test Automation {#BuildCiGuide}
====================================================


## Pi worker

### Hardware

1. [Rasberry PI 3 B+](https://www.newark.com/raspberry-pi/rpi3-modbp/sbc-arm-cortex-a53-1gb-sdram/dp/49AC7637) with 3A power supply.
2. 16GB SDHC class 4 card.

### Software

Raspbian Stretch Lite

- Version:April 2019
- Release date:2019-04-08
- Kernel version:4.14
- SHA-256: 03ec326d45c6eb6cef848cf9a1d6c7315a9410b49a276a6b28e67a40b11fdfcf

### Post Install Steps

```
sudo raspi-config
```
1. Change the default password. Keep this password to yourself.
2. Enable SSH
3. Expand the filesystem to use the entire SD card.
4. Change the hostname to something useful and unique (this will appear as the agent name in buildkite. It must be changed from the default).
```
sudo apt update
sudo apt upgrade -y
sudo apt install git
sudo apt install -y python3-pip
git clone https://github.com/thirtytwobits/nanaimo.git
cd nanaimo
pip3 install --system .
```

#### [Segger JLink](https://www.segger.com/downloads/jlink)

Download the J-Link software for Linux ARM systems.

```
scp path/to/JLink_Linux_VXXX_arm.tgz pi@[ip address]:~
ssh pi@[ip address]
tar -xvf JLink_Linux_VXXX_arm.tgz
mv JLink_Linux_VXXX_arm /opt/
cd /opt/JLink_Linux_VXXX_arm
sudo cp 99-jlink.rules /etc/udev/rules.d/
sudo ln -s /opt/JLink_Linux_VXXX_arm/JLinkExe /bin/JLinkExe
sudo shutdown -r now
```
modify `/boot/config.txt` adding the following:

```
max_usb_current=1
```

#### Build Kite Agent

(taken from the [buildkit agent for Debian instructions](https://buildkite.com/docs/agent/v3/debian))

```
# Add the buildkit repo
sudo apt-get install -y apt-transport-https dirmngr
sudo apt-key adv --keyserver ipv4.pool.sks-keyservers.net --recv-keys 32A37959C2FA5C3C99EFBC32A79206696452D198
ssh-keygen -t rsa -b 4096 -C "[hostname]@uavcan.org"
"echo deb https://apt.buildkite.com/buildkite-agent stable main" | sudo tee /etc/apt/sources.list.d/buildkite-agent.list

# Install the Agent
sudo apt-get update && sudo apt-get install -y buildkite-agent

# Add the libuavcan token
sudo sed -i "s/xxx/INSERT-YOUR-AGENT-TOKEN-HERE/g" /etc/buildkite-agent/buildkite-agent.cfg

# Start the service
sudo systemctl enable buildkite-agent && sudo systemctl start buildkite-agent
```

You can tail the logs using journalctl:

```
sudo journalctl -f -u buildkite-agent
```

Allow the agent to use serial devices:

```
usermod -a -G dialout buildkite-agent
```

#### Serial port

TODO: setup environment variable indicating the serial device Nanaimo should use. Ideally Nanaimo would be able to find this automatically.

#### SSH

You can ignore the github deploy keys for these agents since no source is pulled to perform on-target testing.
