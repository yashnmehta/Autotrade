#!/usr/bin/env python3
"""
Test script to fetch index lists from XTS API for NSECM and BSECM
This will help identify correct underlying asset tokens for indices
"""

import requests
import json
import sys

# XTS API Configuration
XTS_INTERACTIVE_URL = "https://developers.symphonyfintech.in"
API_KEY = "YOUR_API_KEY_HERE"  # Replace with your API key
SECRET_KEY = "YOUR_SECRET_KEY_HERE"  # Replace with your secret key

def get_auth_token():
    """Login and get authentication token"""
    url = f"{XTS_INTERACTIVE_URL}/interactive/user/session"
    
    payload = {
        "appKey": API_KEY,
        "secretKey": SECRET_KEY,
        "source": "WEBAPI"
    }
    
    headers = {
        "Content-Type": "application/json"
    }
    
    try:
        response = requests.post(url, json=payload, headers=headers)
        response.raise_for_status()
        
        data = response.json()
        if data.get('type') == 'success':
            token = data['result']['token']
            user_id = data['result']['userID']
            print(f"✅ Login successful - UserID: {user_id}")
            return token
        else:
            print(f"❌ Login failed: {data}")
            return None
            
    except Exception as e:
        print(f"❌ Login error: {e}")
        return None

def get_index_list(token, exchange_segment):
    """
    Get index list for specified exchange segment
    
    Args:
        token: Authentication token
        exchange_segment: Exchange segment ID (1=NSECM, 11=BSECM, etc.)
    
    Returns:
        List of indices with their details
    """
    url = f"{XTS_INTERACTIVE_URL}/interactive/instruments/indexlist"
    
    payload = {
        "exchangeSegmentList": [exchange_segment]
    }
    
    headers = {
        "Content-Type": "application/json",
        "Authorization": token
    }
    
    try:
        response = requests.post(url, json=payload, headers=headers)
        response.raise_for_status()
        
        data = response.json()
        if data.get('type') == 'success':
            return data['result'].get('indexList', [])
        else:
            print(f"❌ Failed to get index list: {data}")
            return []
            
    except Exception as e:
        print(f"❌ Error fetching index list: {e}")
        return []

def main():
    print("=" * 80)
    print("XTS Index List API Test - NSECM and BSECM")
    print("=" * 80)
    print()
    
    # Get authentication token
    print("Step 1: Authenticating...")
    token = get_auth_token()
    
    if not token:
        print("\n⚠️  Authentication failed. Please check your API credentials.")
        sys.exit(1)
    
    print()
    print("=" * 80)
    
    # Fetch NSECM indices
    print("\nStep 2: Fetching NSECM (NSE Cash Market) indices...")
    print("-" * 80)
    
    nsecm_indices = get_index_list(token, 1)  # 1 = NSECM
    
    if nsecm_indices:
        print(f"\n✅ Found {len(nsecm_indices)} NSECM indices:\n")
        
        # Group by common names for F&O underlyings
        fo_underlyings = ['NIFTY', 'BANKNIFTY', 'FINNIFTY', 'MIDCPNIFTY', 'NIFTYNXT50']
        
        for index in nsecm_indices:
            name = index.get('name', '')
            token_val = index.get('exchangeInstrumentID', 0)
            description = index.get('description', '')
            
            # Highlight F&O underlying indices
            marker = "⭐" if any(name.startswith(fo) for fo in fo_underlyings) else "  "
            
            print(f"{marker} {name:30s} Token: {token_val:10d} - {description}")
        
        # Save to file
        with open('/tmp/nsecm_indices.json', 'w') as f:
            json.dump(nsecm_indices, f, indent=2)
        print(f"\n📄 Saved to: /tmp/nsecm_indices.json")
    else:
        print("❌ No NSECM indices found")
    
    print()
    print("=" * 80)
    
    # Fetch BSECM indices
    print("\nStep 3: Fetching BSECM (BSE Cash Market) indices...")
    print("-" * 80)
    
    bsecm_indices = get_index_list(token, 11)  # 11 = BSECM
    
    if bsecm_indices:
        print(f"\n✅ Found {len(bsecm_indices)} BSECM indices:\n")
        
        for index in bsecm_indices:
            name = index.get('name', '')
            token_val = index.get('exchangeInstrumentID', 0)
            description = index.get('description', '')
            
            # Highlight common indices
            marker = "⭐" if name in ['SENSEX', 'BANKEX'] else "  "
            
            print(f"{marker} {name:30s} Token: {token_val:10d} - {description}")
        
        # Save to file
        with open('/tmp/bsecm_indices.json', 'w') as f:
            json.dump(bsecm_indices, f, indent=2)
        print(f"\n📄 Saved to: /tmp/bsecm_indices.json")
    else:
        print("❌ No BSECM indices found")
    
    print()
    print("=" * 80)
    print("\n✅ Test completed!")
    print("\nNext steps:")
    print("  1. Update the index token mapping in NSEFORepository.cpp")
    print("  2. Add any missing indices to the getUnderlyingAssetToken() function")
    print()

if __name__ == "__main__":
    main()
