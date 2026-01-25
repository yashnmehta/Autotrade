#!/usr/bin/env python3
"""
XTS Index Master Downloader

Downloads NSE CM index list from XTS API and saves it in the format expected
by the trading terminal application.

Usage:
    python download_index_master.py
    
Requirements:
    None (uses standard library only)
"""

import json
import os
import sys
from datetime import datetime
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError


class XTSIndexDownloader:
    """Downloads index master from XTS Market Data API"""
    
    def __init__(self, api_key, secret_key, base_url=None, source="TWSAPI"):
        self.api_key = api_key
        self.secret_key = secret_key
        self.source = source
        self.base_url = base_url or "https://mtrade.arhamshare.com"
        # Remove /marketdataapi suffix if present (we'll add specific paths)
        if self.base_url.endswith("/apimarketdata"):
            self.base_url = self.base_url[:-len("/apimarketdata")]
        # Ensure base_url doesn't end with /
        self.base_url = self.base_url.rstrip('/')
        # Add marketdata path
        if not self.base_url.endswith("/marketdata"):
            self.base_url = f"{self.base_url}/marketdata"
        self.token = None
        
    def login(self):
        """Login to XTS Market Data API and get auth token"""
        url = f"{self.base_url}/auth/login"
        payload = {
            "appKey": self.api_key,
            "secretKey": self.secret_key,
            "source": self.source
        }
        
        print(f"[1/3] Logging in to XTS Market Data API...")
        
        try:
            req = Request(url)
            req.add_header("Content-Type", "application/json")
            data_bytes = json.dumps(payload).encode('utf-8')
            req.add_header("Content-Length", str(len(data_bytes)))
            
            with urlopen(req, data_bytes, timeout=10) as response:
                response_data = json.loads(response.read().decode('utf-8'))
                
            if response_data.get("type") != "success":
                print(f"❌ Login failed: {response_data.get('description', 'Unknown error')}")
                return False
                
            self.token = response_data["result"]["token"]
            print(f"✅ Login successful")
            return True
            
        except HTTPError as e:
            print(f"❌ Login failed: HTTP {e.code}")
            print(e.read().decode('utf-8'))
            return False
        except URLError as e:
            print(f"❌ Network error: {e.reason}")
            return False
        except Exception as e:
            print(f"❌ Unexpected error: {e}")
            return False
    
    def get_index_list(self, exchange_segment=1):
        """
        Fetch index list from XTS API
        
        Args:
            exchange_segment: 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
        """
        if not self.token:
            print("❌ Not logged in. Call login() first.")
            return None
            
        url = f"{self.base_url}/instruments/indexlist?exchangeSegment={exchange_segment}"
        
        segment_name = {1: "NSECM", 2: "NSEFO", 11: "BSECM", 12: "BSEFO"}.get(
            exchange_segment, f"Segment {exchange_segment}"
        )
        print(f"[2/3] Fetching index list for {segment_name}...")
        
        try:
            req = Request(url)
            req.add_header("Content-Type", "application/json")
            req.add_header("authorization", self.token)
            
            with urlopen(req, timeout=10) as response:
                response_data = json.loads(response.read().decode('utf-8'))
            
            if response_data.get("type") != "success":
                print(f"❌ Failed: {response_data.get('description', 'Unknown error')}")
                return None
                
            indices = response_data.get("result", [])
            print(f"✅ Fetched {len(indices)} indices")
            return indices
            
        except HTTPError as e:
            print(f"❌ Failed to fetch index list: HTTP {e.code}")
            print(e.read().decode('utf-8'))
            return None
        except URLError as e:
            print(f"❌ Network error: {e.reason}")
            return None
        except Exception as e:
            print(f"❌ Unexpected error: {e}")
            return None
    
    def save_as_csv(self, indices, output_file):
        """
        Save indices in CSV format compatible with the application
        
        Expected format:
        id,index_name,token,created_at
        """
        print(f"[3/3] Saving to {output_file}...")
        
        os.makedirs(os.path.dirname(output_file), exist_ok=True)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            # Write header
            f.write("id,index_name,token,created_at\n")
            
            # Write data
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            for idx, item in enumerate(indices, start=1):
                # XTS format: {name: "Nifty 50_26000"}
                # Parse: name_token format
                full_name = item.get("name", "")
                
                if "_" in full_name:
                    index_name, token = full_name.rsplit("_", 1)
                else:
                    # Fallback if format is different
                    index_name = full_name
                    token = item.get("exchangeInstrumentID", "")
                
                f.write(f"{idx},{index_name},{token},{timestamp}\n")
        
        print(f"✅ Saved {len(indices)} indices to {output_file}")
        return True
    
    def save_raw_json(self, indices, output_file):
        """Save raw JSON response for reference"""
        os.makedirs(os.path.dirname(output_file), exist_ok=True)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(indices, f, indent=2)
        
        print(f"✅ Saved raw JSON to {output_file}")


def main():
    """Main entry point"""
    
    # Configuration
    config_file = "../configs/config.ini"
    api_key = None
    secret_key = None
    base_url = None
    source = "TWSAPI"
    
    if os.path.exists(config_file):
        print(f"📄 Loading credentials from {config_file}")
        try:
            with open(config_file, 'r') as f:
                lines = f.readlines()
            
            # Simple parser for our config format
            for line in lines:
                line = line.strip()
                if '=' in line and not line.startswith(';') and not line.startswith('#'):
                    key, value = line.split('=', 1)
                    key = key.strip()
                    value = value.strip().strip('"\'')
                    
                    if key == 'marketdata_appkey':
                        api_key = value
                    elif key == 'marketdata_secretkey':
                        secret_key = value
                    elif key == 'source':
                        source = value
                    elif key == 'mdurl':
                        base_url = value
                    elif key == 'url' and not base_url:
                        # Fallback to url if mdurl not found
                        base_url = value
            
            if api_key and secret_key:
                print(f"✅ Loaded credentials from config")
                print(f"   API Key: {api_key[:10]}...")
                print(f"   Source: {source}")
                if base_url:
                    print(f"   URL: {base_url}")
        except Exception as e:
            print(f"⚠️  Could not read config file: {e}")
    
    # Option 2: Environment variables
    if not api_key:
        api_key = os.getenv("XTS_API_KEY")
        secret_key = os.getenv("XTS_SECRET_KEY")
        base_url = os.getenv("XTS_URL")
    
    # Option 3: Prompt user
    if not api_key:
        print("\n📝 Enter XTS Market Data Credentials:")
        api_key = input("API Key (publicKey): ").strip()
        secret_key = input("Secret Key: ").strip()
        base_url = input("Base URL (press Enter for default): ").strip() or None
    
    if not api_key or not secret_key:
        print("❌ API credentials required!")
        print("\nUsage:")
        print("  1. Set environment variables: XTS_API_KEY, XTS_SECRET_KEY")
        print("  2. Or run script and enter credentials when prompted")
        sys.exit(1)
    
    # Output paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    csv_output = os.path.join(project_root, "MasterFiles/nse_cm_index_master.csv")
    json_output = os.path.join(project_root, "MasterFiles/nse_cm_index_master.json")
    
    # Download
    print("\n" + "="*60)
    print("XTS Index Master Downloader")
    print("="*60 + "\n")
    
    downloader = XTSIndexDownloader(api_key, secret_key, base_url, source)
    
    # Step 1: Login
    if not downloader.login():
        sys.exit(1)
    
    # Step 2: Fetch indices
    indices = downloader.get_index_list(exchange_segment=1)  # NSECM
    if not indices:
        sys.exit(1)
    
    # Step 3: Save files
    downloader.save_as_csv(indices, csv_output)
    downloader.save_raw_json(indices, json_output)
    
    print("\n" + "="*60)
    print("✅ Download Complete!")
    print("="*60)
    print(f"\nFiles saved:")
    print(f"  CSV:  {csv_output}")
    print(f"  JSON: {json_output}")
    print(f"\nNext steps:")
    print(f"  1. Copy to build directory:")
    print(f"     cp {csv_output} build/TradingTerminal.app/Contents/MacOS/Masters/")
    print(f"  2. Or rebuild application to auto-copy")
    print()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
