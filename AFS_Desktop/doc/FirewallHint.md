# FirewallHint

Module comment for FirewallHint.h / FirewallHint.cpp.

Finds out whether the Linux firewall ufw probably blocks the port the phones connect to, and
builds the command that opens it, for the pairing dialog (PairDeviceDialog.md#firewall-hint).
ufw is what Ubuntu and Mint ship; other firewalls get the generic hint.

## setting

ufw keeps its switches as KEY=value lines, the value optionally in double quotes. A missing
file or key reads as empty.

## ufw_blocks_incoming

True when ufw is enabled (ufw/ufw.conf) and its default for incoming connections is not
ACCEPT (default/ufw; ufw itself uses DROP when the key is missing). Only these two files are
readable without root; the rules in user.rules are not, so a rule the user already added for
the port cannot be seen. The dialog therefore phrases the hint as "probably". The directory is
a parameter so that tests can use files of their own instead of /etc.

## local_ipv4_networks

The networks of the interfaces the QR code lists (DevicePairing.md#local_ipv4_addresses), as
"address/prefix", sorted and without duplicates: a PC with cable and Wi-Fi in the same home
network has two addresses but one network. Empty on Windows, where ufw does not exist.

## ufw_allow_command

One "sudo ufw allow from <network> to any port <port>" per network, joined with "&&", so that
the port opens only for the home network and not in a café's Wi-Fi. No protocol is named: ufw
then allows TCP for the pairing and UDP for the discovery (DeviceLink.md#port) with one rule.
Without any known network the command allows the port from everywhere, which is still better
than a pairing that never arrives.
