# Base Node
The base node is the central processing device that:
- reads from the file system on start-up to initialise stored user and sensor configurations
- performs time synchronisation with the sensors node
- receives sensor data over Bluetooth from an ultrasonic distance sensor and magnetometer
- processes the sensor data to determine whether an event has occured
- attempts to connect to any configured authorised mobile nodes
- processes keypad inputs to track the current passcode state
- handles reconnecting states for the sensor and mobile nodes
- classifies the event based on the sensor data and passcode entry attempts
- logs all events to the immutable blockchain, which is also stored in the file systemm
- sends the event over RTT to the PC to be displayed on the dashboard
- controls the servo motor for visual feedback after a successful passcode entry


## *user* Shell Command
To add, remove or view authorised users, the following shell command was created:

### Adding access priveleges for a user
```
user add -a <alias> -m <MAC address> -p <passcode>
```
### Removing a user's access priveleges by alias
```
user remove <alias>
```
### Viewing configuration details of a specific user
```
user view <alias>
```
### Viewing configuration details of *all* users 
```
user view -a
```

## *sensor* Shell Command
To modify the sensor thresholds, the following shell command was created:

### Setting magnetometer threshold (normalised^2)
```
sensor m <threshold>
```
### Setting ultrasonic threshold (m)
```
sensor u <threshold>
```
### Viewing sensor threshold configurations
```
sensor view
```
