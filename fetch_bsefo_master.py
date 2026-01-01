import requests
import json
import os
import configparser

def get_bsefo_master():
    config = configparser.ConfigParser()
    config_path = "/home/ubuntu/Desktop/trading_terminal_cpp/configs/config.ini"
    
    with open(config_path, 'r') as f:
        config_string = '[GLOBAL]\n' + f.read()
    config.read_string(config_string)

    base_url = config.get("XTS", "url")
    md_url = base_url + "/apimarketdata"
    
    app_key = config.get("CREDENTIALS", "marketdata_appkey")
    secret_key = config.get("CREDENTIALS", "marketdata_secretkey")
    source = config.get("CREDENTIALS", "source")

    # Step 1: Login
    login_url = f"{md_url}/auth/login"
    login_payload = {
        "appKey": app_key,
        "secretKey": secret_key,
        "source": source
    }
    
    print(f"Logging in to {login_url}...")
    response = requests.post(login_url, json=login_payload)
    if response.status_code != 200:
        print(f"Login failed: {response.text}")
        return

    login_data = response.json()
    if login_data.get("type") != "success":
        print(f"Login failed in response: {login_data}")
        return

    token = login_data["result"]["token"]
    
    # Step 2: Download Master
    master_url = f"{md_url}/instruments/master"
    master_payload = {
        "exchangeSegmentList": ["BSEFO"]
    }
    headers = {
        "Authorization": token,
        "Content-Type": "application/json"
    }
    
    print(f"Downloading BSEFO master from {master_url}...")
    master_response = requests.post(master_url, json=master_payload, headers=headers)
    
    if master_response.status_code != 200:
        print(f"Master download failed: {master_response.text}")
        return

    raw_output_path = "/home/ubuntu/Desktop/trading_terminal_cpp/bsefo_master_raw.json"
    with open(raw_output_path, "w") as f:
        f.write(master_response.text)
    
    print(f"Raw BSEFO master output stored at: {raw_output_path}")
    print(f"Size: {len(master_response.text)} bytes")

if __name__ == "__main__":
    get_bsefo_master()
