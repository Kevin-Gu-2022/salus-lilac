"""
Send file to Files of TagoIO
"""
import requests
import base64
import os
from dotenv import load_dotenv

FILE_BASE_URL = "https://api.tago.io/file/6808dca1d2bacd000a4f1731/img/"

load_dotenv()

def send_data(filename: str):

    data_endpoint = 'https://api.us-e1.tago.io/data'

     # Load API keys
    device_token = os.getenv("DEVICE_TOKEN")
    if not device_token:
        raise RuntimeError("Missing DEVICE_TOKEN in .env")
    
    headers = {
        'Content-Type': 'application/json',
        'device-token': device_token
    }

    # JSON payload sent in POST request to TagoIO
    payload = [
        {"variable": "eventId", "value": 1},
        {"variable": "image", "value": FILE_BASE_URL + filename}
    ]

    # Send image
    response = requests.post(data_endpoint, headers=headers, json=payload)
    print(f"Data: {response.json()}")


def send_image(filename: str) -> None:
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
    with open(filename, 'rb') as f:
        encoded_file = base64.b64encode(f.read()).decode('utf-8')

    payload = {
        'file': encoded_file,
        'filename': "img/" + filename,
        'content_type': 'image/jpeg',
        'public': True
    }

    # Send request
    response = requests.post(files_endpoint, headers=headers, json=payload)
    print(f"Image: {response.json()}")

    # Send rest of data
    send_data(filename)


if __name__ == "__main__":
    send_image('person_door.jpg')