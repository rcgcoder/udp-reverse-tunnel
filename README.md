# UDP reverse tunnel

Testing from RCGCoder   

## What kind of problem does this solve?

### Background story

I have a wireguard server running at home to be able to enter my home network from the outside. Wireguard is using UDP. Unfortunately I can not just open an IPv4 UDP port in my fritz box like it used to work in the old days because my provider has put me behind CG-NAT, I have to use IPv6 but IPv6 is not yet usable in my workplace.

So at home I have IPv6 and at work I have IPv4 and no way to reach my home network.

I also have a VPS with a public IPv4 to which I could establish reverse ssh tunnels for TCP traffic. Unfortunately the ssh tunnels only support TCP and one should not do VPN connections over TCP anyways, which is the reason wireguard is using only UDP.

### Problem

````
VPN-Server                                 VPN-Client
  (UDP)         |        |                 (IPv4 UDP)
    |           |        | not possible        |
     -----------|--------| <-------------------
                |        |
               NAT     CG-NAT

````

### Solution
````
VPN-Server                                 VPN-Client
  (UDP)                                    (IPv4 UDP)
    |                                          |
    |                                          |
inside agent                              outside agent
   tunnel                                    tunnel
    ||          |         |                    ||
    ||     punch|    punch|                    ||
    | --------->|-------->|-------------------- |
     -----------|<--------|<--------------------
                |         |
                |         |
               NAT     CG-NAT
````

The outside agent is running on a VPS with public IPv4

The inside agent will send UDP datagrams to the public IP and port of the outside agent, this will punch holes into both NATs, the outside agent will receive these datagrams and learn from their source address and port how to send datagrams back to the inside agent.

The VPN client can send UDP to the outside agent and these datagrams will be forwarded to the inside agent and from there to the VPN server, the VPN server can reply to the inside agent, these will be forwarded to the outside agent and from there to the VPN client.

The outside agent will appear as the VPN server to the client, and the inside agent will appear as the VPN client to the server.

Multiple client connections are possible because it will use a tunnel and a new socket on the inside agent for every new client connecting on the outside, so to the server it will appear as if multiple clients were running on the inside agent's host.

## Installation and Usage

Clone or unzip the code on on both machines and build it. You need at least make and gcc. Enter the source directory and use the command
````
$ make
````
After successful build you end up with the binary `udp-tunnel` in the same folder. Now you can either start it directly from a terminal (with the right options of course) to make a few quick tests, or you can install it with the help of the makefile.

### Quick test without installing

The compiled binary `udp-tunnel` can either act as the inside agent or as the outside agent, depending on what options you pass on the command line.

Assume as an example the VPN server is listening on UDP 1234, running on localhost (same as inside agent) and the outside machine is jump.example.com and we want that to listen on UDP port 9999.

On the inside host we start it with
````
$ ./udp-tunnel -s localhost:1234 -o jump.example.com:9999
````

On the outside host we start it with
````
$ ./udp-tunnel -l 9999
````

### Installation with makefile

The makefile contains 3 install targets: `install` to install only the binary, `install-outside` and `install-inside` to also install the systemd service files. The latter two need variables passed to make in order to work properly.

To install the outside agent on on the jump host (assuming you want port 9999) you execute this command:
````
$ sudo make install-outside listen=9999
````
This will install the binary into `/usr/local/bin/` and install a systemd service file into `/etc/systemd/system/` containing the needed command to start it in outside agent mode with port 9999.

To install the inside agent on the inside machine use the following command (assuming as an example your vpn server is localhost:1234 and your jump host is jump.example.com):
````
$ sudo make install-inside service=localhost:1234 outside=jump.example.com:9999
````
This will again install the binary into `/usr/local/bin/` and a systemd unit file into `/etc/systemd/system/`

At this point you might want to have a quick look into the systemd unit files to see how the binary is used and to check whether the options are correct. The options should look like described above in the quick test.

After the systemd files are installed and confirmed to be correct they are not yet enabled for autostart, you need to enable and start them. On the Inside machine:
````
$ sudo systemctl enable udp-tunnel-inside.service
$ sudo systemctl start udp-tunnel-inside.service
````
and on the outside machine
````
$ sudo systemctl enable udp-tunnel-outside.service
$ sudo systemctl start udp-tunnel-outside.service
````

## Security

There is no encryption. Packets are forwarded as they are, it is assumed that whatever service you are tunneling knows how to protect or encrypt its data on its own. Usually this is the case for VPN connections.

Additionally an attacker might want to spoof the keepalive packets from the inside agent to confuse the outside agent and divert the tunnel to his own machine, resulting in service disruption. To prevent this very simple attack the keepalive datagrams can be authenticated with a hash based message authentication code. You can use a pre shared password using the -k option on both tunnel ends to activate this feature.

On the inside host you would use it like this
````
$ ./udp-tunnel -s localhost:1234 -o jump.example.com:9999 -k mysecretpassword
````

On the outside host you would start it with
````
$ ./udp-tunnel -l 9999 -k mysecretpassword
````
After you got the installation steps from above successfully working you might want to manually edit your systemd files on both ends and add a -k option, then reload and restart on both ends.

The keepalive message will then contain an SHA-256 over this password and over a strictly increasing nonce that can only be used exactly once to prevent simple replay attacks.

### How it works

Both agents maintain a list of connections, each connection stores socket addresses and socket handles assiciated with that particular client conection.

On startup the inside agent will initiate an outgoing tunnel to the outside agent, mark it as unused and send keepalive packets over it. The outside agent will see these packets and add this tunnel with its source address it to its own connection list. The keepalive packets are signed with a nonce and an authentication code, so they cannot be spoofed or replayed.

When a client connects, the outside agent will send the client data down that unused spare tunnel, the inside agent will see this, mark the tunnel as active, create a socket for communicating with the service and forward data in both directions. It will then also immediately create another new outgoing spare tunnel to be ready for the next incoming client.

When the new spare tunnel arrives at the outside agent it will add it to its own connection list to be able to serve the next connecting client immediately.

At this point both agents have 2 tunnels in their list, one is active and one is spare, waiting for the next client connection.

inactivity timeouts will make sure that dead tunnels (those that had been marked active in the past but no client data for some time) will be deleted and their sockets will be closed. The inside agent will detect the lack of forwarded data for prolonged time, remove it from its own list and close its sockets, then some time later the outside agent will detect that there are no keepalives arriving anymore for this conection and remove it from its own list too.

## Beware

This code is still highly experimental, so don't base a multi million dollar business on it, at least not yet. It serves the purpuse perfectly well for me, but it might crash and burn and explode your server for you. You have been warned.
