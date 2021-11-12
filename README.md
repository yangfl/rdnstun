# ICMP Path Faker

A tool similar to [job/ipv6-traceroute-faker](https://github.com/job/ipv6-traceroute-faker) but supports IPv4 as well, used to display a long path in a `traceroute`-like tool.

```
traceroute to 192.168.2.1 (192.168.2.1), 30 hops max, 60 byte packets
 1  192.168.2.10  0.094 ms  0.068 ms  0.058 ms
 2  192.168.2.9  0.050 ms  0.036 ms  0.029 ms
 3  192.168.2.8  0.030 ms  0.024 ms  0.017 ms
 4  192.168.2.7  0.058 ms  0.023 ms  0.023 ms
 5  192.168.2.6  0.022 ms  0.022 ms  0.022 ms
 6  192.168.2.5  0.021 ms  0.060 ms  0.025 ms
 7  192.168.2.4  0.023 ms  0.022 ms  0.023 ms
 8  192.168.2.3  0.022 ms  0.023 ms  0.023 ms
 9  192.168.2.2  0.023 ms  0.021 ms  0.022 ms
10  192.168.2.1  0.022 ms  0.059 ms  0.025 ms
```

**Note:** You can use punycode in your rdns records!


## Example

```bash
# optional, add a persistent tun device
sudo ip tuntap add dev tun-rdns mode tun
# append 'multi_queue' if you want multithread

# start rdnstun
sudo ./rdnstun -4 192.168.2.10-192.168.2.1 -6 3000::f-3000::1 -6 route=3000:0:0:1::/64,3000:0:0:1::f-3000:0:0:1::1

# add corresponding routes
sudo ip a a 192.168.2.254/24 dev tun-rdns
sudo ip a a 3000::ffff/48 dev tun-rdns
sudo ip l s up dev tun-rdns

# test it
ping 192.168.2.1
traceroute 192.168.2.1
ping 3000::1
traceroute6 3000::1

# cleanup
sudo ip tuntap del dev tun-rdns mode tun
```


## License
WTFPL-2
