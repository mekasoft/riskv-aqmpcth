

## Directory Structure:


## Build Instructions



## Mosquitto MQTT

### Configuring the server
`/etc/mosquitto/mosquitto.conf` is the MQTT server configuration file. You can refer to the official website and search engines to configure your MQTT server according to your needs.

### Testing
Login into milkv

to subscribe type
```sh
mosquitto_sub -h 192.168.51.20 -p 1883 -t "homeassistant/riskv" -m "test"
```

to start a server
```sh
mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

### Using the library from c