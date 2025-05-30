"""
Send file to Files of TagoIO
"""
import requests
import base64
import os
import json
from dotenv import load_dotenv

FILE_BASE_URL = "https://api.tago.io/file/6808dca1d2bacd000a4f1731/img/"

load_dotenv()

def send_data(payload: list[dict]):
    """
    Send a JSON payload
    payload: A list of dictionaries, i.e., a JSON array
    """
    data_endpoint = 'https://api.us-e1.tago.io/data'

     # Load API keys
    device_token = os.getenv("DEVICE_TOKEN")
    if not device_token:
        raise RuntimeError("Missing DEVICE_TOKEN in .env")
    
    headers = {
        'Content-Type': 'application/json',
        'device-token': device_token
    }

    # Send data
    response = requests.post(data_endpoint, headers=headers, json=payload)
    print(f"Data: {response.json()}")


def send_image(filepath: str) -> None:
    """
    Send a file to the File storage area in TagoIO under img directory
    """
    files_endpoint = 'https://api.tago.io/files'

    # Load API keys
    account_token = os.getenv("ACCOUNT_TOKEN")
    if not account_token:
        raise RuntimeError("Missing ACCOUNT_TOKEN in .env")
    
    headers = {
        'Device-Token': account_token,
        'Content-Type': 'application/json'
    }

    # Encode the image in base64
    with open(filepath, 'rb') as f:
        encoded_file = base64.b64encode(f.read()).decode('utf-8')

    payload = {
        'file': encoded_file,
        'filename': "img/" + filepath,
        'content_type': 'image/jpeg',
        'public': True
    }

    # Send POST request with just the image
    response = requests.post(files_endpoint, headers=headers, json=payload)
    print(f"Image: {response.json()}")

    # JSON payload sent in POST request to TagoIO with associated metadata
    metaDataPayload = [
        {"variable": "image", "value": FILE_BASE_URL + filepath}
    ]

    # Send rest of data
    send_data(metaDataPayload)


def send_status(status_data: dict) -> None:
    """
    Takes a dictionary and converts it to a format TagoIO can understand
    """
    try:
        key_map = {
            "event": "Event_Type",
            "mag_meas": "Magnetometer",
            "ultra_meas": "Ultrasonic",
            "user": "Detected_User",
            "MAC": "Device_MAC_Address"
        }

        exclude_keys = {"timestamp", "prev_hash", "curr_hash"}
        sensor_keys = {"mag_meas", "ultra_meas"}

        tago_data = []
        for k, v in status_data.items():
            if k in exclude_keys:
                continue
            if k in sensor_keys and v == "N/A":
                continue

            mapped_key = key_map.get(k, k)  # fallback to original key if not in map
            entry = {"variable": mapped_key, "value": v}
            if k == "ultra_meas":
                entry["unit"] = "m"
            tago_data.append(entry)
        
        send_data(tago_data)

    except KeyError as e:
        raise ValueError(f"Missing required key: {e}") from e
    


if __name__ == "__main__":
    json_str = r'''{"timestamp":"5","event":"TAMPERING","mag_meas":"0.345","ultra_meas":"0.54","user":"Intruder","MAC":"N/A","prev_hash":"9821044afcd9963adfe5ca31864d83cfa0617134fc2df25aaab6150d2b294c27","curr_hash":"74dc2cae201ce7ce350ca3476da6a5dd35d4dcf13469f615ccc92f626855d82b"}'''
    data = json.loads(json_str)
    send_status(data)