# Base Node
The base node is the central processing device that:
- receives sensor event signals over MQTT from an ultrasonic distance sensor and magnetometer
- attempts to create a BLE connection with any authorised mobile nodes
- rejects connection attempts from unauthorised mobile nodes
- classifies event type based on sensor signals and passcode entry attempts
- receives passcode entry attempts over BLE from the mobile node, and sends a corresponding response to indicate a successful/unsuccessful entry
- sends signals over MQTT to the Arducam to capture an image during an event
- logs all events to the immutable blockchain, and sends the event to the PC
- controls the actuators (servo motor and speaker) to display visual/audio responses to events


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