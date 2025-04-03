import paho.mqtt.client as mqtt
import time
import argparse
import json
import warnings


# Credentials for unauthenticated mode
BROKER_UNAUTH = "broker.hivemq.com"
PORT = 1883
ESP32_TOPIC = "luca/esp32/data"
ACK_TOPIC = "luca/esp32/acks"
CLIENT_ID = "ack-server-6789"

# Credentials for authenticated mode
BROKER_AUTH = "eu1.cloud.thethings.network"
MQTT_USERNAME = "loratest012@ttn"
MQTT_PASSWORD = "NNSXS.TGW57KBC7FKNDDLOY44D5SDEZHX25YTLLT7B44A.BTP6NYHTQIOEA5SMMVVKPP6GSH4HKTAR6PRRKN7AI6IGOPISAPOQ"
TTN_TOPIC = "#"

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    if(not authenticated):
        client.subscribe(ESP32_TOPIC)
        print(f"Subscribed to {ESP32_TOPIC}")
    else:
        client.subscribe(TTN_TOPIC)
        print(f"Subscribed to {TTN_TOPIC}")

def on_message(client, userdata, msg):

    if(not authenticated):
        print(f"Received message on [{msg.topic}]: {msg.payload.decode()}")
        # Send acknowledgment
        ack_message = msg.payload.decode()
        client.publish(ACK_TOPIC, ack_message)
        print(f"Sent ACK to {ACK_TOPIC}: {ack_message}")
    else:
        try:
            # Try to parse as JSON
            payload = json.loads(msg.payload.decode())
            
            # Print formatted output
            print("\n" + "="*50)
            print(f"Topic: {msg.topic}")
            print("-"*50)
            print(json.dumps(payload, indent=1, sort_keys=True))
            print("="*50 + "\n")
            
        except json.JSONDecodeError:
            # Fallback for non-JSON messages
            print("\n" + "="*50)
            print(f"Topic: {msg.topic}")
            print("-"*50)
            print(msg.payload.decode())
            print("="*50 + "\n")

def setup_client(use_auth=False):
    client = mqtt.Client()
    
    if use_auth:
        print("Using authenticated connection")
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    else:
        print("Using unauthenticated connection")
    
    client.on_connect = on_connect
    client.on_message = on_message
    
    if use_auth:
        client.connect(BROKER_AUTH, PORT, 60)
        print(f"Connect to {BROKER_AUTH} on port {PORT}")
    else:
        client.connect(BROKER_UNAUTH, PORT, 60)
        print(f"Connect to {BROKER_UNAUTH} on port {PORT}")


    return client

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='MQTT Acknowledgment Server')
    parser.add_argument('-a', '--auth', action='store_true', 
                       help='Use authenticated connection')
    args = parser.parse_args()
    
    if(args.auth):
        authenticated = True
    
    warnings.filterwarnings(
        "ignore",
        category=DeprecationWarning,
        message="Callback API version 1 is deprecated"
    )

    client = setup_client(use_auth=args.auth)
    
    try:
        print("Starting MQTT acknowledgment server...")
        client.loop_forever()
    except KeyboardInterrupt:
        print("Disconnecting...")
        client.disconnect()