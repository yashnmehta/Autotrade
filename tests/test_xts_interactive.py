import requests
import json
import logging

# Configuration
BASE_URL = "http://192.168.102.9:3000"
APP_KEY = "5820d8e017294c81d71873"
SECRET_KEY = "Ibvk668@NX"
SOURCE = "WEBAPI" # or TWSAPI, trying WEBAPI as per previous turn's docs

# Setup logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def interactive_login():
    url = f"{BASE_URL}/interactive/user/session"
    payload = {
        "appKey": APP_KEY,
        "secretKey": SECRET_KEY,
        "source": SOURCE
    }
    headers = {
        "Content-Type": "application/json"
    }
    
    logger.info(f"Logging in to Interactive API: {url}")
    try:
        response = requests.post(url, json=payload, headers=headers)
        response.raise_for_status()
        data = response.json()
        
        if data.get("type") == "success":
            result = data.get("result", {})
            token = result.get("token")
            user_id = result.get("userID")
            client_codes = result.get("clientCodes", [])
            client_id = client_codes[0] if client_codes else user_id
            
            logger.info(f"Login Successful. UserID: {user_id}, ClientID: {client_id}")
            return token, client_id, user_id
        else:
            logger.error(f"Login Failed: {data}")
            return None, None, None
            
    except Exception as e:
        logger.error(f"Login Exception: {e}")
        return None, None, None

def get_user_profile(token, client_id, user_id):
    # Try 1: With clientID
    url = f"{BASE_URL}/interactive/user/profile?clientID={client_id}"
    headers = {
        "Authorization": token,
        "Content-Type": "application/json"
    }
    
    logger.info(f"Fetching User Profile (Try 1 - ClientID): {url}")
    try:
        response = requests.get(url, headers=headers)
        if response.status_code == 200:
             logger.info("User Profile Fetched Successfully (Try 1)")
             print(json.dumps(response.json().get("result", {}), indent=4))
             return
        else:
             logger.error(f"Try 1 Failed: {response.status_code} {response.text}")
    except Exception as e:
        logger.error(f"Try 1 Exception: {e}")

    # Try 3: With UserID as clientID
    url = f"{BASE_URL}/interactive/user/profile?clientID={user_id}"
    logger.info(f"Fetching User Profile (Try 3 - UserID as clientID): {url}")
    try:
        response = requests.get(url, headers=headers)
        if response.status_code == 200:
             logger.info("User Profile Fetched Successfully (Try 3)")
             print(json.dumps(response.json().get("result", {}), indent=4))
             return
        else:
             logger.error(f"Try 3 Failed: {response.status_code} {response.text}")

    except Exception as e:
        logger.error(f"Try 3 Exception: {e}")

if __name__ == "__main__":
    token, client_id, user_id = interactive_login()
    if token:
        get_user_profile(token, client_id, user_id) 

