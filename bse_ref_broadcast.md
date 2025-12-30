
cpp_broadcast_bsefo
 


"""
BSE UDP Market Data Reader - Dual Feed Implementation
======================================================

Supports simultaneous connections to:
- Port 26001: Equity Cash (CM) - ~4,689 stocks
- Port 26002: Derivatives (F&O) - ~40,000 contracts

Features:
- Configurable enable/disable for each segment via config.json
- Separate CSV files for each segment (YYYYMMDD_CM_quotes.csv, YYYYMMDD_FO_quotes.csv)
- Unified token mapping from BhavCopy + Contract Master CSVs
- NO dependency on token_details.json
- Proper Ctrl+C termination on Windows

Configuration (config.json):
    "segments": {
        "cm_enabled": true,   // Enable Equity Cash feed (port 26001)
        "fo_enabled": true    // Enable Derivatives feed (port 26002)
    }

Usage:
    python src/main.py

Author: BSE Integration Team
Date: November 2025
"""

import sys
import json
import logging
import signal
import socket
import threading
import time
from pathlib import Path
from typing import Dict, Optional

# Add src directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from connection import BSEMulticastConnection
from packet_receiver import PacketReceiver
from live_stats import LiveStatsTracker, get_tracker, reset_tracker

# Configure logging - less verbose for cleaner benchmark output
file_handler = logging.FileHandler('bse_reader.log')
file_handler.setLevel(logging.DEBUG)  # Full debug to file

console_handler = logging.StreamHandler()
console_handler.setLevel(logging.WARNING)  # Less console noise

logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[file_handler, console_handler]
)
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)  # Main module at INFO level

# Global shutdown event for thread coordination
shutdown_event = threading.Event()


def signal_handler(signum, frame):
    """Handle Ctrl+C gracefully - works on Windows."""
    logger.info("\n⚠ Shutdown signal received (Ctrl+C)")
    shutdown_event.set()  # Signal all threads to stop
    # Force exit after a short delay if threads don't stop
    def force_exit():
        time.sleep(2)
        if not shutdown_event.is_set():
            return
        logger.info("Force exiting...")
        import os
        os._exit(0)
    threading.Thread(target=force_exit, daemon=True).start()


def load_config(config_path: str = 'config.json') -> dict:
    """Load configuration from JSON file."""
    try:
        logger.info(f"Loading configuration from {config_path}...")
        with open(config_path, 'r') as f:
            config = json.load(f)
        
        # Log segment configuration
        segments = config.get('segments', {})
        cm_enabled = segments.get('cm_enabled', False)
        fo_enabled = segments.get('fo_enabled', False)
        
        logger.info(f"✓ Configuration loaded successfully")
        logger.info(f"  Segments enabled:")
        logger.info(f"    CM (Equity Cash, port 26001): {'✅ ENABLED' if cm_enabled else '❌ DISABLED'}")
        logger.info(f"    FO (Derivatives, port 26002): {'✅ ENABLED' if fo_enabled else '❌ DISABLED'}")
        
        return config
        
    except FileNotFoundError:
        logger.error(f"✗ Configuration file not found: {config_path}")
        raise
    except json.JSONDecodeError as e:
        logger.error(f"✗ Invalid JSON in configuration file: {e}")
        raise


def load_token_map_unified(use_api: bool = True, keep_days: int = 2) -> dict:
    """
    Load unified token mapping from BSE daily CSV files.
    
    Uses BSETokenMapper to combine:
    - BhavCopy CSV (Equity Cash, port 26001) - ~4,689 stocks
    - Contract Master CSV (F&O, port 26002) - ~40,000 derivatives
    """
    try:
        from token_mapper import BSETokenMapper
        
        logger.info("=" * 80)
        logger.info("📥 LOADING UNIFIED TOKEN MAPPER (No JSON dependency)")
        logger.info("=" * 80)
        
        mapper = BSETokenMapper()
        
        if use_api:
            logger.info("Updating token mappings from BSE API...")
            success = mapper.update_all(keep_days=keep_days)
            
            if not success:
                logger.warning("⚠️  API fetch failed, trying cached files...")
                mapper.load_from_cache()
        else:
            logger.info("Loading from cached CSV files (API disabled)...")
            mapper.load_from_cache()
        
        # Build token_map dict
        token_map = {}
        
        # Add Equity tokens
        for token_str, contract in mapper.equity_fetcher.contracts.items():
            token_map[token_str] = {
                'symbol': contract['symbol'],
                'segment': 'EQ',
                'company_name': contract.get('company_name', ''),
                'isin': contract.get('isin', '')
            }
        
        # Add F&O tokens
        for token_str, contract in mapper.contract_manager.contracts.items():
            token_map[token_str] = {
                'symbol': contract['symbol'],
                'expiry': contract.get('expiry', ''),
                'option_type': contract.get('option_type', ''),
                'strike_price': contract.get('strike', ''),
                'lot_size': contract.get('lot_size', ''),
                'instrument_name': contract.get('instrument_name', ''),
                'full_name': contract.get('full_name', ''),
                'contract_type': contract.get('contract_type', ''),
                'segment': 'FO'
            }
        
        logger.info(f"✅ Unified token map loaded: {len(token_map):,} total contracts")
        logger.info(f"   Equity Cash: {mapper.stats['equity_tokens']:,} stocks")
        logger.info(f"   Derivatives: {mapper.stats['fo_tokens']:,} F&O contracts")
        logger.info("=" * 80)
        
        return token_map
        
    except Exception as e:
        logger.error(f"❌ Failed to load unified token map: {e}")
        logger.error("Falling back to legacy F&O-only loader...")
        return load_token_map_legacy(use_api)


def load_token_map_legacy(use_api: bool = True) -> dict:
    """LEGACY: Load token mapping from BSE daily CSV (F&O only)."""
    try:
        from contract_manager import BSEContractManager
        
        logger.info("=" * 80)
        logger.info("📥 LOADING BSE CONTRACT MASTER (F&O Only - Legacy)")
        logger.info("=" * 80)
        
        api_url = "http://192.168.102.166:2060/v1/sftp-files"
        manager = BSEContractManager(api_url)
        
        if use_api:
            manager.update_contracts()
        
        fetcher = manager.load_latest_contracts()
        
        token_map = {}
        for token_str, contract in fetcher.contracts.items():
            token_map[token_str] = {
                'symbol': contract['symbol'],
                'expiry': contract['expiry'],
                'option_type': contract['option_type'],
                'strike_price': contract['strike'],
                'lot_size': contract['lot_size'],
                'instrument_name': contract['instrument_name'],
                'full_name': contract['full_name'],
                'contract_type': contract['contract_type'],
                'segment': 'FO'
            }
        
        logger.info(f"✅ Token map loaded: {len(token_map):,} F&O contracts")
        logger.info("=" * 80)
        
        return token_map
        
    except Exception as e:
        logger.error(f"❌ Failed to load F&O contracts: {e}")
        return {}


def create_segment_connection(config: dict, segment: str) -> BSEMulticastConnection:
    """
    Create connection for a specific segment.
    
    Args:
        config: Configuration dictionary
        segment: 'CM' for Equity Cash, 'FO' for Derivatives
    
    Returns:
        BSEMulticastConnection configured for the segment
    """
    if segment == 'CM':
        mc_config = config.get('multicast_cm', {})
    else:  # 'FO'
        mc_config = config.get('multicast_fo', config.get('multicast', {}))
    
    return BSEMulticastConnection(
        multicast_ip=mc_config.get('ip', '239.1.2.5'),
        port=mc_config.get('port', 26001 if segment == 'CM' else 26002),
        buffer_size=config.get('buffer_size', 2048)
    )


def run_segment_receiver(
    config: dict,
    token_map: dict,
    segment: str,
    connection: BSEMulticastConnection,
    stats_tracker: LiveStatsTracker
):
    """
    Run packet receiver for a specific segment in a thread.
    
    Args:
        config: Configuration dictionary
        token_map: Token mapping dictionary
        segment: 'CM' or 'FO'
        connection: BSEMulticastConnection for this segment
        stats_tracker: LiveStatsTracker for benchmark statistics
    """
    segment_name = "Equity Cash" if segment == 'CM' else "Derivatives (F&O)"
    port = 26001 if segment == 'CM' else 26002
    
    # Get segment stats from tracker
    seg_stats = stats_tracker.get_segment(segment)
    
    try:
        logger.info(f"[{segment}] Starting {segment_name} receiver on port {port}...")
        
        # Connect
        sock = connection.connect()
        
        logger.info(f"[{segment}] ✅ Connected to 239.1.2.5:{port}")
        
        # Initialize receiver with segment
        receiver_config = {
            'raw_packets_dir': 'data/raw_packets',
            'processed_json_dir': 'data/processed_json',
            'store_limit': config.get('store_limit', 100),
            'timeout': config.get('timeout', 30)
        }
        
        # Filter token map for this segment
        segment_token_map = {
            k: v for k, v in token_map.items()
            if v.get('segment') == ('EQ' if segment == 'CM' else 'FO')
        }
        
        # If segment filter results in empty map, use full map
        if not segment_token_map:
            segment_token_map = token_map
            logger.warning(f"[{segment}] Using full token map (no segment-specific tokens found)")
        else:
            logger.info(f"[{segment}] Using {len(segment_token_map):,} tokens for {segment}")
        
        receiver = PacketReceiver(sock, receiver_config, segment_token_map, segment=segment)
        
        logger.info(f"[{segment}] 📦 Receiver initialized")
        logger.info(f"[{segment}] 📁 CSV output: data/processed_csv/YYYYMMDD_{segment}_quotes.csv")
        
        # Run receive loop with shutdown check
        while not shutdown_event.is_set():
            try:
                # Receive with short timeout to check shutdown_event frequently
                sock.settimeout(0.5)
                
                # Start timing for latency measurement
                start_time = time.perf_counter()
                
                packet, addr = sock.recvfrom(2000)
                
                # Decode timing
                decode_start = time.perf_counter()
                receiver._process_packet(packet, addr)
                decode_end = time.perf_counter()
                
                process_end = time.perf_counter()
                
                # Calculate latencies in microseconds
                decode_time_us = (decode_end - decode_start) * 1_000_000
                process_time_us = (process_end - start_time) * 1_000_000
                
                receiver.stats['packets_received'] += 1
                receiver.stats['bytes_received'] += len(packet)
                
                # Update benchmark stats
                # Estimate records from packet size (66 bytes per record after 36-byte header)
                records_estimate = max(1, (len(packet) - 36) // 66)
                quotes_saved = receiver.stats.get('quotes_saved', 0)
                
                seg_stats.add_packet(
                    packet_size=len(packet),
                    records=records_estimate,
                    quotes=1 if quotes_saved > 0 else 0,
                    decode_time_us=decode_time_us,
                    process_time_us=process_time_us
                )
                
                # Log every 10 packets (less verbose now that we have live stats)
                if receiver.stats['packets_received'] % 100 == 0:
                    logger.debug(
                        f"[{segment}] 📦 Packets: {receiver.stats['packets_received']}, "
                        f"Valid: {receiver.stats['packets_valid']}, "
                        f"Type 2020: {receiver.stats['packets_2020']}"
                    )
            except socket.timeout:
                continue  # Check shutdown_event and retry
            except Exception as e:
                if not shutdown_event.is_set():
                    logger.error(f"[{segment}] Receive error: {e}")
                break
        
        logger.info(f"[{segment}] 🛑 Receiver stopped (shutdown requested)")
        
    except Exception as e:
        logger.error(f"[{segment}] ❌ Error in receiver: {e}")
    finally:
        if connection:
            connection.disconnect()
            logger.info(f"[{segment}] 🔌 Disconnected")


def main():
    """
    Main application entry point - Dual Feed Implementation.
    
    Workflow:
    1. Load configuration
    2. Check which segments are enabled
    3. Load unified token map
    4. Create connections for enabled segments
    5. Start receiver threads
    6. Wait for shutdown signal
    """
    # Register signal handler for Ctrl+C (works on Windows)
    signal.signal(signal.SIGINT, signal_handler)
    try:
        signal.signal(signal.SIGBREAK, signal_handler)  # Windows-specific
    except AttributeError:
        pass  # Not on Windows
    
    # Initialize stats tracker
    stats_tracker = reset_tracker()
    
    connections = []
    threads = []
    
    try:
        # Step 1: Load configuration (silently for cleaner output)
        logging.getLogger().setLevel(logging.WARNING)  # Reduce logging during startup
        config = load_config('config.json')
        
        # Step 2: Check enabled segments
        segments = config.get('segments', {})
        cm_enabled = segments.get('cm_enabled', False)
        fo_enabled = segments.get('fo_enabled', False)
        
        if not cm_enabled and not fo_enabled:
            logger.error("❌ No segments enabled! Enable cm_enabled and/or fo_enabled in config.json")
            return 1
        
        # Get multicast config
        mc_cm = config.get('multicast_cm', {})
        mc_fo = config.get('multicast_fo', config.get('multicast', {}))
        multicast_ip = mc_cm.get('ip', '239.1.2.5')
        cm_port = mc_cm.get('port', 26001)
        fo_port = mc_fo.get('port', 26002)
        buffer_size = config.get('buffer_size', 2048)
        
        # Print benchmark header
        stats_tracker.print_header(
            multicast_ip=multicast_ip,
            cm_port=cm_port,
            fo_port=fo_port,
            buffer_size=buffer_size,
            cm_enabled=cm_enabled,
            fo_enabled=fo_enabled
        )
        
        # Step 3: Load unified token map (silently)
        keep_days = config.get('data_management', {}).get('keep_days', 2)
        token_map = load_token_map_unified(use_api=True, keep_days=keep_days)
        
        if not token_map:
            logger.error("❌ Failed to load token map!")
            return 1
        
        # Restore logging level
        logging.getLogger().setLevel(logging.INFO)
        
        # Step 4: Create connections for enabled segments
        if cm_enabled:
            cm_connection = create_segment_connection(config, 'CM')
            connections.append(('CM', cm_connection))
            stats_tracker.add_segment('CM')
        
        if fo_enabled:
            fo_connection = create_segment_connection(config, 'FO')
            connections.append(('FO', fo_connection))
            stats_tracker.add_segment('FO')
        
        # Step 5: Start receiver threads (NOT daemon - for proper cleanup)
        for segment, connection in connections:
            thread = threading.Thread(
                target=run_segment_receiver,
                args=(config, token_map, segment, connection, stats_tracker),
                name=f"Receiver-{segment}",
                daemon=False  # Non-daemon for proper cleanup
            )
            threads.append(thread)
            thread.start()
        
        # Print connected message
        stats_tracker.print_connected()
        
        # Start live stats tracking (every 5 seconds)
        stats_tracker.start_interval_tracking(interval_sec=5)
        
        # Step 6: Wait for shutdown signal or threads to finish
        while not shutdown_event.is_set():
            # Check if all threads are dead
            if all(not t.is_alive() for t in threads):
                break
            time.sleep(0.5)  # Check every 500ms
        
        # Signal shutdown to all threads
        shutdown_event.set()
        
        # Stop stats tracking
        stats_tracker.stop()
        
        # Wait for threads to finish (with timeout)
        print("\n⏳ Waiting for threads to finish...")
        for thread in threads:
            thread.join(timeout=2.0)
            if thread.is_alive():
                logger.warning(f"Thread {thread.name} did not stop in time")
        
    except FileNotFoundError:
        logger.error("\n❌ Configuration file not found")
        return 1
    except Exception as e:
        logger.error(f"\n❌ Application error: {e}")
        logger.exception("Full error traceback:")
        return 1
    finally:
        # Cleanup
        shutdown_event.set()
        
        # Stop stats tracking if running
        try:
            stats_tracker.stop()
        except:
            pass
        
        # Disconnect all connections
        for segment, connection in connections:
            try:
                connection.disconnect()
            except:
                pass
        
        # Print final benchmark report
        if stats_tracker.segments:
            print("\n⚠️  Received interrupt signal, stopping...")
            stats_tracker.print_final_report()
    
    return 0


if __name__ == '__main__':
    exit_code = main()
    sys.exit(exit_code)




#########################

"""
BSE Packet Receiver Module
===========================

Phase 2 + Phase 3 Implementation: Receives raw UDP packets from BSE NFCAST feed,
validates, filters, decodes, decompresses, normalizes, and saves market data.

Key Features:
- Continuous packet reception with configurable timeout
- Packet validation (size, header structure, message type)
- Token extraction from market data records
- **Phase 3: Full packet decoding (header + base values)**
- **Phase 3: NFCAST differential decompression (Open/High/Low + Best 5)**
- **Phase 3: Quote normalization and symbol resolution**
- **Phase 3: JSON/CSV output with timestamped files**
- Raw packet storage (.bin files)
- Token metadata storage (JSON)
- Comprehensive statistics tracking

Packet Structure (from BSE_Final_Analysis_Report.md):
- Leading zeros: 0x00000000 (bytes 0-3)
- Format ID: 0x0124 (300B) or 0x022C (556B) at offset 4-5 (Big-Endian)
- Type field: 0x07E4 (2020) or 0x07E5 (2021) at offset 8-9 (Little-Endian)
- Market data records: Start at offset 36, every 64 bytes
- Token: First 4 bytes of each record (Little-Endian uint32)

Phase 3 Pipeline:
1. Validate packet → 2. Decode header + records → 3. Decompress differentials →
4. Collect quotes (normalize + resolve symbols) → 5. Save to JSON/CSV

BSE Market Hours: 9:00 AM - 3:30 PM IST (Mon-Fri)
"""

import struct
import logging
import socket
import json
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Optional, Tuple

# Phase 3 imports
from decoder import PacketDecoder
from decompressor import NFCASTDecompressor
from data_collector import MarketDataCollector
from saver import DataSaver
from database import DatabaseHandler

logger = logging.getLogger(__name__)


class PacketReceiver:
    """
    Receives and processes UDP packets from BSE NFCAST multicast feed.
    
    Responsibilities:
    - Continuous packet reception loop
    - Packet validation and filtering for types 2020/2021
    - Token extraction from market data records
    - Storage of raw packets and extracted metadata
    - Statistics tracking and logging
    """
    
    # Message types we're interested in (from BSE NFCAST Manual page 22)
    MSG_TYPE_MARKET_PICTURE = 2020  # 0x07E4 in Little-Endian
    MSG_TYPE_MARKET_PICTURE_COMPLEX = 2021  # 0x07E5 in Little-Endian
    
    # Packet format identifiers (Big-Endian at offset 4-5)
    # CRITICAL: Format ID = packet size in decimal!
    FORMAT_300B = 0x012C  # 300 bytes (300 decimal)
    FORMAT_564B = 0x022C  # 564 bytes (564 decimal)
    FORMAT_828B = 0x033C  # 828 bytes (828 decimal) - PRODUCTION
    
    # Packet size to token mapping (record size = 264 bytes)
    # Header = 36 bytes, each record = 264 bytes
    PACKET_FORMAT_MAP = {
        300: {'token_count': 1, 'offsets': [36]},
        564: {'token_count': 2, 'offsets': [36, 300]},
        828: {'token_count': 3, 'offsets': [36, 300, 564]},
        1092: {'token_count': 4, 'offsets': [36, 300, 564, 828]},
        1356: {'token_count': 5, 'offsets': [36, 300, 564, 828, 1092,1356,1620]}
    }
    
    # BSE production feed uses DYNAMIC packet sizes!
    # Format ID at bytes 4-5 (Big-Endian) indicates packet size
    # We'll validate: format_id == len(packet)
    
    # Header constants
    HEADER_SIZE = 36
    RECORD_SIZE = 64  # Each instrument record is 64 bytes
    
    def __init__(self, sock: socket.socket, config: dict, token_map: Dict[str, dict], segment: str = None):
        """
        Initialize packet receiver with Phase 3 components.
        
        Args:
            sock: Connected UDP multicast socket
            config: Configuration dictionary with storage paths and limits
            token_map: Dictionary mapping token IDs to contract details (for symbol resolution)
            segment: Segment identifier ('CM', 'FO', or None) for file naming
        """
        self.socket = sock
        self.config = config
        self.segment = segment  # 'CM', 'FO', or None
        
        # Setup storage directories
        self.raw_packets_dir = Path(config.get('raw_packets_dir', 'data/raw_packets'))
        self.processed_json_dir = Path(config.get('processed_json_dir', 'data/processed_json'))
        
        self.raw_packets_dir.mkdir(parents=True, exist_ok=True)
        self.processed_json_dir.mkdir(parents=True, exist_ok=True)
        
        # Storage limits
        self.store_limit = config.get('store_limit', 100)
        self.stored_count = 0
        
        # Statistics
        self.stats = {
            'packets_received': 0,
            'packets_valid': 0,
            'packets_invalid': 0,
            'packets_2020': 0,
            'packets_2021': 0,
            'packets_other': 0,
            'tokens_extracted': 0,
            'bytes_received': 0,
            'errors': 0,
            # Phase 3 stats
            'packets_decoded': 0,
            'packets_decompressed': 0,
            'quotes_collected': 0,
            'quotes_saved': 0
        }
        
        # Tokens storage file
        self.tokens_file = self.processed_json_dir / 'tokens.json'
        
        # Phase 3: Initialize decoder, decompressor, collector, saver
        self.decoder = PacketDecoder()
        self.decompressor = NFCASTDecompressor()
        self.collector = MarketDataCollector(token_map, segment=segment)  # Pass segment for CM/FO differentiation
        self.saver = DataSaver(output_dir='data', segment=segment)
        
        # # Phase 3: Initialize database handler (NEW)
        # db_path = config.get('db_path', 'data/bse_market.db')
        # self.database = DatabaseHandler(db_path=db_path)
        
        segment_info = f" [{segment}]" if segment else ""
        logger.info(f"PacketReceiver{segment_info} initialized - storing up to {self.store_limit} packets")
        logger.info(f"Raw packets: {self.raw_packets_dir}")
        logger.info(f"Processed JSON: {self.processed_json_dir}")
        # logger.info(f"Database: {db_path}")
        logger.info(f"Phase 3 pipeline enabled: decode → decompress → collect → save → database")
        logger.info(f"Token map loaded: {len(token_map)} tokens")
    
    def receive_loop(self, max_packets: Optional[int] = None):
        """
        Main packet reception loop.
        
        Continuously receives packets from UDP socket, validates them,
        filters for message types 2020/2021, extracts tokens, and stores data.
        
        Args:
            max_packets: Optional limit on number of packets to receive (for testing)
        
        The loop continues until:
        - max_packets reached (if specified)
        - Keyboard interrupt (Ctrl+C)
        - Fatal socket error
        """
        logger.info("Starting packet reception loop...")
        logger.info("BSE Market Hours: 9:00 AM - 3:30 PM IST (Mon-Fri)")
        logger.info("Press Ctrl+C to stop")
        logger.info("")
        logger.info("ℹ️  Waiting for packets from BSE multicast feed...")
        logger.info("ℹ️  If outside market hours or on test feed, you may not receive packets")
        logger.info("ℹ️  Socket timeout is 1 second (this allows Ctrl+C to work)")
        logger.info("")
        
        timeout_counter = 0  # Counter to log status periodically
        
        try:
            while True:
                # Check if we've reached the storage limit
                if max_packets and self.stats['packets_received'] >= max_packets:
                    logger.info(f"Reached max packets limit: {max_packets}")
                    break
                
                try:
                    # Receive packet (2000-byte buffer as per BSE specification)
                    # Socket has 1-second timeout to allow Ctrl+C to work
                    packet, addr = self.socket.recvfrom(2000)
                    
                    # Reset timeout counter when packet received
                    timeout_counter = 0
                    
                    self.stats['packets_received'] += 1
                    self.stats['bytes_received'] += len(packet)
                    
                    # Log every 10th packet to avoid flooding logs
                    if self.stats['packets_received'] % 10 == 0:
                        logger.info(
                            f"📦 Packets received: {self.stats['packets_received']}, "
                            f"Valid: {self.stats['packets_valid']}, "
                            f"Type 2020: {self.stats['packets_2020']}, "
                            f"Type 2021: {self.stats['packets_2021']}"
                        )
                    
                    # Process the packet
                    self._process_packet(packet, addr)
                
                except socket.timeout:
                    # Timeout waiting for packet - this is normal
                    # Socket has 1-second timeout to allow Ctrl+C to work
                    # Log status every 30 seconds (30 timeouts)
                    timeout_counter += 1
                    if timeout_counter >= 30:
                        logger.info(
                            f"⏱️  Still waiting for packets... ({self.stats['packets_received']} received so far)"
                        )
                        if self.stats['packets_received'] == 0:
                            logger.info(
                                "   💡 Tip: Check if BSE market is open (9:00 AM - 3:30 PM IST, Mon-Fri)"
                            )
                            logger.info(
                                "   💡 Tip: Simulation feed may not have live data"
                            )
                        timeout_counter = 0
                    continue
                
                except socket.error as e:
                    self.stats['errors'] += 1
                    logger.error(f"Socket error: {e}")
                    # On fatal errors, break the loop
                    if e.errno in [9, 10038]:  # Bad file descriptor, socket not open
                        logger.error("Fatal socket error - stopping receiver")
                        break
                    continue
        
        except KeyboardInterrupt:
            logger.info("Received keyboard interrupt - stopping receiver")
        
        finally:
            self._print_statistics()
    
    def _process_packet(self, packet: bytes, addr: Tuple[str, int]):
        """
        Process a received packet: validate, filter, extract tokens, decode, decompress, and save.
        
        Phase 2 + Phase 3 Pipeline:
        1. Validate packet structure
        2. Filter for message types 2020/2021
        3. Extract tokens (Phase 2 compatibility)
        4. **Decode packet → extract header + base values (Phase 3)**
        5. **Decompress differentials → full OHLC + Best 5 (Phase 3)**
        6. **Collect quotes → normalize + resolve symbols (Phase 3)**
        7. **Save to JSON/CSV (Phase 3)**
        8. Store raw packet + metadata (Phase 2 compatibility)
        
        Args:
            packet: Raw packet bytes
            addr: Source address tuple (ip, port)
        """
        # Validate packet
        # print("Validating packet...", packet, addr)
        if not self._validate_packet(packet):
            self.stats['packets_invalid'] += 1
            return
        
        # Extract message type
        msg_type = self._extract_message_type(packet)
        
        # Filter for message types 2020 or 2021
        if msg_type == self.MSG_TYPE_MARKET_PICTURE:
            self.stats['packets_2020'] += 1
        elif msg_type == self.MSG_TYPE_MARKET_PICTURE_COMPLEX:
            self.stats['packets_2021'] += 1
        else:
            self.stats['packets_other'] += 1
            # Log first few non-2020/2021 packets to see what we're getting
            if self.stats['packets_other'] <= 5:
                logger.warning(f"Ignoring packet with message type: {msg_type} (0x{msg_type:04X})")
            return
        
        self.stats['packets_valid'] += 1
        
        # Extract tokens from the packet (Phase 2 - for metadata storage)
        tokens = self._extract_tokens(packet)
        
        if tokens:
            self.stats['tokens_extracted'] += len(tokens)
            logger.debug(f"Extracted {len(tokens)} tokens from packet: {tokens}")
        
        # ========== PHASE 3 PIPELINE ==========
        try:
            # Step 1: Decode packet → extract header + base values
            decoded_data = self.decoder.decode_packet(packet)
            if not decoded_data or not decoded_data.get('records'):
                logger.debug("Packet decoded but no records found")
                return
            
            self.stats['packets_decoded'] += 1
            logger.debug(f"Decoded {len(decoded_data['records'])} records from packet")
            
            # Step 2: Decompress each record → full OHLC + Best 5
            decompressed_records = []
            for record in decoded_data['records']:
                decompressed = self.decompressor.decompress_record(packet, record)
                if decompressed:
                    decompressed_records.append(decompressed)
            
            if not decompressed_records:
                logger.debug("No records successfully decompressed")
                return
            
            self.stats['packets_decompressed'] += 1
            logger.debug(f"Decompressed {len(decompressed_records)} records")
            
            # Step 3: Collect quotes → normalize + resolve symbols
            quotes = self.collector.collect_quotes(decoded_data['header'], decompressed_records)
            if not quotes:
                logger.debug("No quotes collected from decompressed records")
                return
            
            self.stats['quotes_collected'] += len(quotes)
            logger.debug(f"Collected {len(quotes)} normalized quotes")
            
            # Step 4: Save to JSON/CSV
            # save_success = self.saver.save_quotes(quotes, save_json=True, save_csv=True)
            save_success = self.saver.save_quotes(quotes, save_csv=True)
            if save_success:
                self.stats['quotes_saved'] += len(quotes)
                logger.info(f"✓ Phase 3 pipeline complete: {len(quotes)} quotes saved")
            else:
                logger.warning("Failed to save some quotes")
            
            # # Step 5: Save to SQLite Database (NEW)
            # date_str = datetime.now().strftime('%Y%m%d')
            # db_success = self.database.save_quotes(quotes, date_str=date_str)
            # if db_success:
            #     logger.info(f"✓ Saved {len(quotes)} quotes to database: {date_str}")
            # else:
            #     logger.warning("Failed to save quotes to database")
        
        except Exception as e:
            logger.error(f"Error in Phase 3 pipeline: {e}", exc_info=True)
            self.stats['errors'] += 1
        # ========== END PHASE 3 PIPELINE ==========
        
        # Store packet if under limit (Phase 2 compatibility)
        if self.stored_count < self.store_limit:
            self._store_packet(packet, msg_type, tokens, addr)
            self.stored_count += 1
        elif self.stored_count == self.store_limit:
            logger.warning(f"Reached storage limit of {self.store_limit} packets - no longer storing")
            self.stored_count += 1  # Increment to avoid logging again
    
    def _validate_packet(self, packet: bytes) -> bool:
        """
        Validate packet structure with DYNAMIC size validation.
        
        BSE production feed sends packets with MULTIPLE different sizes:
        - 300 bytes (Format ID 0x012C)
        - 564 bytes (Format ID 0x0234) - Most common in production
        - 828 bytes (Format ID 0x033C)
        
        KEY INSIGHT: Format ID (bytes 4-5, LITTLE-ENDIAN!) = packet size in decimal!
        
        CRITICAL ENDIANNESS DISCOVERY:
        - Bytes 4-5 in packet: 0x2C01
        - Read as BIG-ENDIAN: 0x012C = 300 ✓
        - BUT actual bytes are: [0x2C, 0x01]
        - This is LITTLE-ENDIAN representation!
        - Read as LITTLE-ENDIAN: 0x012C = 300 ✓
        
        Validation checks:
        1. Minimum size (at least header size)
        2. Leading zeros (bytes 0-3 must be 0x00000000)
        3. Format ID (LITTLE-ENDIAN) matches packet length
        4. Message type is valid (2020 or 2021)
        
        Args:
            packet: Raw packet bytes
        
        Returns:
            True if packet is valid, False otherwise
        """
        # Track validation failures for first 5 packets
        verbose_logging = (self.stats['packets_received'] <= 5)
        
        # Check minimum size
        if len(packet) < self.HEADER_SIZE:
            if verbose_logging:
                logger.warning(f"❌ VALIDATION FAILED: Packet too small: {len(packet)} bytes (need at least {self.HEADER_SIZE})")
            return False
        
        # Check leading zeros (offset 0-3 should be 0x00000000)
        # This is the key identifier from BSE_Final_Analysis_Report.md
        leading_bytes = packet[0:4]
        if leading_bytes != b'\x00\x00\x00\x00':
            if verbose_logging:
                logger.warning(f"❌ VALIDATION FAILED: Invalid leading bytes: {leading_bytes.hex()} (expected 00000000)")
                logger.warning(f"   First 20 bytes (hex): {packet[:20].hex()}")
            return False
        
        # Extract Format ID (offset 4-5, LITTLE-ENDIAN!!!)
        format_id = struct.unpack('<H', packet[4:6])[0]  # ← CHANGED TO LITTLE-ENDIAN
        
        # CRITICAL: Format ID should match packet size in decimal!
        if format_id != len(packet):
            if verbose_logging:
                logger.warning(f"❌ VALIDATION FAILED: Format ID mismatch")
                logger.warning(f"   Format ID (LE): {format_id} (0x{format_id:04X})")
                logger.warning(f"   Packet size: {len(packet)} bytes")
                logger.warning(f"   First 20 bytes (hex): {packet[:20].hex()}")
            return False
        
        # Extract message type (offset 8-9, Little-Endian)
        msg_type_bytes = struct.unpack('<H', packet[8:10])[0]
        
        # Check if message type is valid (2020 or 2021)
        if msg_type_bytes not in [self.MSG_TYPE_MARKET_PICTURE, self.MSG_TYPE_MARKET_PICTURE_COMPLEX]:
            if verbose_logging:
                logger.warning(f"❌ VALIDATION FAILED: Invalid message type: {msg_type_bytes} (0x{msg_type_bytes:04X})")
                logger.warning(f"   Expected: 2020 (0x07E4) or 2021 (0x07E5)")
                logger.warning(f"   First 20 bytes (hex): {packet[:20].hex()}")
            return False
        
        # If we got here, packet is valid!
        if verbose_logging:
            logger.info(f"✅ VALIDATION PASSED:")
            logger.info(f"   Size: {len(packet)} bytes")
            logger.info(f"   Format ID (LE): {format_id} (0x{format_id:04X})")
            logger.info(f"   Message Type: {msg_type_bytes}")
        
        return True
        return True
    
    def _extract_message_type(self, packet: bytes) -> int:
        """
        Extract message type from packet header.
        
        Message type is at offset 8-9 (2 bytes, Little-Endian).
        Note: This is LITTLE-ENDIAN despite most of the packet being Big-Endian.
        
        Args:
            packet: Raw packet bytes
        
        Returns:
            Message type as integer (e.g., 2020, 2021)
        """
        # Extract type field at offset 8-9 (Little-Endian uint16)
        # This is 0x07E4 (2020) or 0x07E5 (2021)
        msg_type = struct.unpack('<H', packet[8:10])[0]
        return msg_type
    
    def _extract_tokens(self, packet: bytes) -> List[int]:
        """
        Extract token IDs from market data records.
        
        Market data records start at offset 36 and repeat every 64 bytes.
        Each record starts with a 4-byte token ID (Little-Endian uint32).
        Token ID of 0 indicates an empty slot.
        
        Args:
            packet: Raw packet bytes
        
        Returns:
            List of valid (non-zero) token IDs
        """
        tokens = []
        
        # Calculate number of possible records based on packet size
        # Records start at offset 36, each record is 64 bytes
        header_size = 36
        record_size = 64
        
        if len(packet) < header_size:
            return tokens
        
        available_space = len(packet) - header_size
        num_records = available_space // record_size
        
        # Parse records at calculated offsets
        for i in range(num_records):
            offset = header_size + (i * record_size)
            
            # Check if we have enough data for this record
            if offset + 4 > len(packet):
                break
            
            # Extract token (Little-Endian uint32)
            token = struct.unpack('<I', packet[offset:offset+4])[0]
            
            # Token 0 indicates empty slot - skip it
            if token != 0:
                tokens.append(token)
        
        return tokens
    
    def _store_packet(self, packet: bytes, msg_type: int, tokens: List[int], addr: Tuple[str, int]):
        """
        Store raw packet and extracted metadata.
        
        Stores:
        1. Raw packet as .bin file in data/raw_packets/
        2. Token metadata as JSON entry in data/processed_json/tokens.json
        
        Args:
            packet: Raw packet bytes
            msg_type: Message type (2020 or 2021)
            tokens: List of extracted token IDs
            addr: Source address (ip, port)
        """
        timestamp = datetime.now()
        
        # Generate filenames with timestamp
        timestamp_str = timestamp.strftime('%Y%m%d_%H%M%S_%f')
        
        # Store raw packet as .bin file
        raw_filename = f"{timestamp_str}_type{msg_type}_packet.bin"
        raw_path = self.raw_packets_dir / raw_filename
        
        try:
            with open(raw_path, 'wb') as f:
                f.write(packet)
            logger.debug(f"Stored raw packet: {raw_filename}")
        except Exception as e:
            logger.error(f"Failed to store raw packet: {e}")
            self.stats['errors'] += 1
        
        # Store token metadata as JSON
        metadata = {
            'timestamp': timestamp.isoformat(),
            'msg_type': msg_type,
            'packet_size': len(packet),
            'tokens': tokens,
            'source_ip': addr[0],
            'source_port': addr[1],
            'raw_file': raw_filename
        }
        
        try:
            # # Append to tokens.json file
            # with open(self.tokens_file, 'a') as f:
            #     json.dump(metadata, f)
            #     f.write('\n')  # Newline-delimited JSON for easy parsing
            
            logger.info(f"Stored packet metadata: {len(tokens)} tokens, type {msg_type}")
        except Exception as e:
            logger.error(f"Failed to store token metadata: {e}")
            self.stats['errors'] += 1
    
    def _print_statistics(self):
        """Print comprehensive statistics about received packets and Phase 3 pipeline."""
        logger.info("=" * 70)
        logger.info("PACKET RECEIVER STATISTICS (Phase 2 + Phase 3)")
        logger.info("=" * 70)
        logger.info(f"Packets Received:     {self.stats['packets_received']:,}")
        logger.info(f"Packets Valid:        {self.stats['packets_valid']:,}")
        logger.info(f"Packets Invalid:      {self.stats['packets_invalid']:,}")
        logger.info(f"Packets Type 2020:    {self.stats['packets_2020']:,}")
        logger.info(f"Packets Type 2021:    {self.stats['packets_2021']:,}")
        logger.info(f"Packets Other Types:  {self.stats['packets_other']:,}")
        logger.info(f"Tokens Extracted:     {self.stats['tokens_extracted']:,}")
        logger.info(f"Bytes Received:       {self.stats['bytes_received']:,}")
        logger.info("-" * 70)
        logger.info("Phase 3 Pipeline:")
        logger.info(f"Packets Decoded:      {self.stats['packets_decoded']:,}")
        logger.info(f"Packets Decompressed: {self.stats['packets_decompressed']:,}")
        logger.info(f"Quotes Collected:     {self.stats['quotes_collected']:,}")
        logger.info(f"Quotes Saved:         {self.stats['quotes_saved']:,}")
        logger.info("-" * 70)
        logger.info(f"Errors:               {self.stats['errors']:,}")
        logger.info(f"Packets Stored (raw): {min(self.stored_count, self.store_limit):,}")
        logger.info("=" * 70)
        
        # Print Phase 3 component statistics
        logger.info("\nPhase 3 Component Statistics:")
        logger.info("-" * 70)
        
        decoder_stats = self.decoder.get_stats()
        logger.info("Decoder:")
        for key, value in decoder_stats.items():
            logger.info(f"  {key}: {value:,}")
        
        decompressor_stats = self.decompressor.get_stats()
        logger.info("Decompressor:")
        for key, value in decompressor_stats.items():
            logger.info(f"  {key}: {value:,}")
        
        collector_stats = self.collector.get_stats()
        logger.info("Data Collector:")
        for key, value in collector_stats.items():
            logger.info(f"  {key}: {value:,}")
        
        saver_stats = self.saver.get_stats()
        logger.info("Data Saver:")
        for key, value in saver_stats.items():
            logger.info(f"  {key}: {value:,}")
        
        # # Print Database statistics (NEW)
        # db_stats = self.database.get_stats()
        # logger.info("Database Handler:")
        # for key, value in db_stats.items():
        #     logger.info(f"  {key}: {value:,}")
          
        logger.info("=" * 70)


# For testing the module independently
if __name__ == '__main__':
    print("PacketReceiver module - Phase 2 implementation")
    print("Use via main.py for full functionality")



######################

"""
BSE Packet Decoder Module
==========================

Decodes BSE NFCAST packets (300-byte and 556-byte formats).
Parses packet headers and uncompressed market data fields.

Phase 3: Full Packet Decoding
- Parse header (36 bytes): leading zeros, format ID, message type, timestamp
- Parse market data records (64 bytes each): token and uncompressed fields
- Extract: token, num_trades, volume, close_rate, LTQ, LTP (uncompressed base values)
- Returns structured data for decompressor to process compressed fields

Protocol Details (from BSE_Final_Analysis_Report.md):
- Leading zeros: 0x00000000 at offset 0-3
- Format ID: 0x0124 (300B) or 0x022C (556B) at offset 4-5 (Big-Endian)
- Message type: 2020/2021 at offset 8-9 (Little-Endian)
- Timestamp: HH:MM:SS at offsets 20-25 (3x uint16 Big-Endian)
- Records: Start at offset 36, every 64 bytes, up to 6 instruments
- Token: Little-Endian uint32 at record start
- Uncompressed fields: Big-Endian int32 (prices in paise)

Author: BSE Integration Team
Phase: Phase 3 - Decoding & Decompression
"""

import struct
import logging
from typing import Dict, List, Optional
from datetime import datetime

logger = logging.getLogger(__name__)


class PacketDecoder:
    """
    Decoder for BSE NFCAST packet formats.
    
    Handles:
    - 300-byte format (4 instruments max)
    - 556-byte format (6 instruments max)
    - Header parsing with mixed endianness
    - Uncompressed field extraction
    """
    
    def __init__(self):
        """Initialize decoder with statistics tracking."""
        self.stats = {
            'packets_decoded': 0,
            'decode_errors': 0,
            'invalid_headers': 0,
            'empty_records': 0,
            'records_decoded': 0
        }
        logger.info("PacketDecoder initialized")
    
    def decode_packet(self, packet: bytes) -> Optional[Dict]:
        """
        Decode a BSE packet into structured data.
        
        Args:
            packet: Raw packet bytes (300 or 556 bytes)
        
        Returns:
            Dictionary with 'header' and 'records' lists, or None if invalid
            
        Format:
        {
            'header': {
                'format_id': int,
                'msg_type': int,
                'timestamp': datetime,
                'packet_size': int
            },
            'records': [
                {
                    'token': int,
                    'num_trades': int,
                    'volume': int,
                    'close_rate': int,  # paise
                    'ltq': int,
                    'ltp': int,  # paise (base for decompression)
                    'compressed_offset': int  # where compressed data starts
                },
                ...
            ]
        }
        """
        try:
            packet_size = len(packet)
            logger.debug(f"Decoding packet: size={packet_size} bytes")
            
            # Parse header (36 bytes)
            header = self._parse_header(packet)
            if not header:
                self.stats['invalid_headers'] += 1
                logger.warning(f"Invalid header in packet (size={packet_size})")
                return None
            
            header['packet_size'] = packet_size
            
            # Determine number of records based on packet size
            num_records = self._get_num_records(packet_size)
            logger.debug(f"Expected {num_records} records for {packet_size}B packet")
            
            # Parse records at FIXED 264-byte intervals
            # CONFIRMED: Each record slot is exactly 264 bytes (null-padded if smaller)
            # Pattern: 36 (header) + N×264 (records) = total packet size
            records = []
            record_slot_size = 264
            
            logger.debug(f"Packet {packet_size}B → {num_records} records at 264-byte intervals")
            
            for record_index in range(num_records):
                # Calculate record offset (fixed 264-byte slots)
                record_offset = 36 + (record_index * record_slot_size)
                
                # Read exactly 264 bytes for this record slot
                record_bytes = packet[record_offset:record_offset + record_slot_size]
                
                if len(record_bytes) < record_slot_size:
                    logger.warning(f"Record {record_index} incomplete: {len(record_bytes)}/{record_slot_size} bytes")
                    break
                
                # Parse the record (will handle null padding internally)
                record = self._parse_record(record_bytes, record_offset)
                if record:
                    records.append(record)
                    self.stats['records_decoded'] += 1
                    logger.debug(f"Decoded record {record_index}: token={record['token']}, "
                               f"ltp={record['ltp']}, offset={record_offset}")
                else:
                    self.stats['empty_records'] += 1
            
            self.stats['packets_decoded'] += 1
            # logger.info(f"Successfully decoded packet: {len(records)} records extracted")
            
            return {
                'header': header,
                'records': records
            }
            
        except Exception as e:
            self.stats['decode_errors'] += 1
            logger.error(f"Decode error: {e}", exc_info=True)
            return None
    
    def _parse_header(self, packet: bytes) -> Optional[Dict]:
        """
        Parse 36-byte packet header.
        
        Header Structure (from BSE_Final_Analysis_Report.md):
        Offset 0-3:   Leading zeros (0x00000000)
        Offset 4-5:   Format ID (0x0124=300B, 0x022C=556B) Big-Endian
        Offset 8-9:   Message type (2020/2021) Little-Endian ⚠️
        Offset 20-21: Hour (Big-Endian uint16)
        Offset 22-23: Minute (Big-Endian uint16)
        Offset 24-25: Second (Big-Endian uint16)
        """
        if len(packet) < 36:
            logger.error("Packet too short for header (<36 bytes)")
            return None
        
        try:
            # Check leading zeros (offset 0-3)
            leading = struct.unpack('>I', packet[0:4])[0]  # Big-Endian
            if leading != 0x00000000:
                logger.warning(f"Leading zeros check failed: {leading:#010x}")
            
            # Format ID (offset 4-5, Big-Endian)
            format_id = struct.unpack('>H', packet[4:6])[0]
            logger.debug(f"Format ID: {format_id:#06x} ({format_id})")
            
            # Message type (offset 8-9, Little-Endian ⚠️)
            msg_type = struct.unpack('<H', packet[8:10])[0]
            logger.debug(f"Message type: {msg_type} (0x{msg_type:04x})")
            
            # Timestamp (offsets 20-25, Little-Endian uint16 each - per COMPLETE_PACKET_STRUCTURE_ANALYSIS.md)
            hour = struct.unpack('<H', packet[20:22])[0]
            minute = struct.unpack('<H', packet[22:24])[0]
            second = struct.unpack('<H', packet[24:26])[0]
            
            # Always use system current time with milliseconds
            # BSE header timestamp often invalid or doesn't include milliseconds
            # System time is more accurate for HFT timestamping
            timestamp = datetime.now()
            
            # Debug logging for first few packets only
            if self.stats.get('packets_decoded', 0) < 3:
                logger.info(f"🕐 BSE Header: {hour:02d}:{minute:02d}:{second:02d} | Using System: {timestamp.strftime('%H:%M:%S.%f')[:-3]}")
            
            return {
                'format_id': format_id,
                'msg_type': msg_type,
                'timestamp': timestamp
            }
            
        except struct.error as e:
            logger.error(f"Header parsing struct error: {e}")
            return None
    
    def _parse_record(self, record_bytes: bytes, offset: int) -> Optional[Dict]:
        """
        Parse market data record with uncompressed fields.
        
        Record Structure (CONFIRMED from live data analysis):
        
        RECORDS ARE VARIABLE LENGTH:
        - Full records (with order book depth): 264 bytes
        - Minimal records (no depth): 64-108 bytes
        - Pattern: 36 (header) + N×264 (records) = 300, 564, 828, 1092, 1356, 1620
        - Max 6 records per message type 2020
        
        ✅ CONFIRMED OFFSETS (from live market data):
        Offset 0-3:   Token (uint32 LE) ✓ CONFIRMED
        Offset 4-7:   Open Price (int32 LE, paise) ✓ CONFIRMED
        Offset 8-11:  Previous Close (int32 LE, paise) ✓ CONFIRMED
        Offset 12-15: High Price (int32 LE, paise) ✓ CONFIRMED
        Offset 16-19: Low Price (int32 LE, paise) ✓ CONFIRMED
        Offset 20-23: Unknown Field (int32 LE) ❓ NOT close price!
        Offset 24-27: Volume (int32 LE) ✓ CONFIRMED
        Offset 28-31: Turnover in Lakhs (uint32 LE) ✓ CONFIRMED - Traded Value / 100,000
        Offset 32-35: Lot Size (uint32 LE) ✓ CONFIRMED - Contract lot size
        Offset 36-39: LTP - Last Traded Price (int32 LE, paise) ✓ CONFIRMED
        Offset 40-43: Unknown Field (uint32 LE) ❓ Always zero
        Offset 44-47: Market Sequence Number (uint32 LE) ✓ CONFIRMED - Increments by 1 per tick
        Offset 84-87: ATP - Average Traded Price (int32 LE, paise) ✓ CONFIRMED
        Offset 104-107: Best Bid Price (int32 LE, paise) ✓ CONFIRMED - Also Order Book Bid Level 1
        Offset 104-263: 5-Level Order Book (160 bytes) ✓ CONFIRMED - Interleaved Bid/Ask
        
        All prices are in paise (divide by 100 for rupees).
        All integer fields use Little-Endian byte order.
        """
        # Minimum record size based on your confirmed fields
        # We need at least 40 bytes to read LTP at offset 36-39
        if len(record_bytes) < 40:
            logger.warning(f"Record too short: {len(record_bytes)} < 40 bytes (minimum for LTP)")
            return None
        
        try:
            # Token (offset 0-3, Little-Endian ⚠️ - ONLY field that's Little-Endian!)
            token = struct.unpack('<I', record_bytes[0:4])[0]
            
            # Skip empty slots (token = 0)
            if token == 0:
                logger.debug(f"Empty record slot at offset {offset}")
                return None
            
            logger.debug(f"Raw record bytes (first 70): {record_bytes[:70].hex()}")
            
            # Parse uncompressed fields - CONFIRMED structure (99% sure)
            # Field offsets verified from live market data analysis:
            # Offset +4:  Open Price - 4 bytes, Little-Endian ✓ CONFIRMED
            # Offset +8:  Prev Close - 4 bytes, Little-Endian ✓ CONFIRMED
            # Offset +12: High Price - 4 bytes, Little-Endian ✓ CONFIRMED
            # Offset +16: Low Price - 4 bytes, Little-Endian ✓ CONFIRMED
            # LTP location: UNKNOWN - still needs to be identified
            # All prices are in paise (divide by 100 for rupees)
            
            # Offset +4: Open Price (4 bytes, Little-Endian) ✓ CONFIRMED
            open_price = struct.unpack('<i', record_bytes[4:8])[0]
            
            # Offset +8: Prev Close Price (4 bytes, Little-Endian) ✓ CONFIRMED
            prev_close = struct.unpack('<i', record_bytes[8:12])[0]
            
            # Offset +12: High Price (4 bytes, Little-Endian) ✓ CONFIRMED
            high_price = struct.unpack('<i', record_bytes[12:16])[0]
            
            # Offset +16: Low Price (4 bytes, Little-Endian) ✓ CONFIRMED
            low_price = struct.unpack('<i', record_bytes[16:20])[0]
            
            # Offset +20: Field 20-23 (4 bytes, Little-Endian) - ⚠️ NOT Close Price!
            # Live data shows values like ₹15.33 when LTP is ₹84,535 - clearly wrong
            # Possible interpretations: Change from prev close? Points? Different field?
            # TODO: Investigate actual meaning of this field
            field_20_23 = struct.unpack('<i', record_bytes[20:24])[0]
            
            # Offset +24: Volume (4 bytes, int32 Little-Endian) ✓ CONFIRMED
            volume = struct.unpack('<i', record_bytes[24:28])[0]
            
            # Offset +28: Total Turnover in Lakhs ✓ CONFIRMED
            # Total Turnover = Traded Value / 1,00,000 (standard F&O field)
            # Example: 8,728 lakhs = ₹872,800,000 traded value
            turnover_lakhs = struct.unpack('<I', record_bytes[28:32])[0] if len(record_bytes) >= 32 else None
            
            # Offset +32: Lot Size ✓ CONFIRMED
            # Contract lot size (e.g., 20 for Sensex futures)
            # Used to calculate actual quantity in contracts
            lot_size = struct.unpack('<I', record_bytes[32:36])[0] if len(record_bytes) >= 36 else None
            
            # Offset +36: LTP - Last Traded Price (4 bytes, Little-Endian) ✓ CONFIRMED
            ltp = struct.unpack('<i', record_bytes[36:40])[0]
            
            # UNKNOWN FIELDS 40-47 (for manual identification)
            # Offset +40: Always zero so far - flags/padding?
            unknown_40_43 = struct.unpack('<I', record_bytes[40:44])[0] if len(record_bytes) >= 44 else None
            
            # Offset +44: Market Sequence Number ✓ CONFIRMED
            # Packet/market sequence number - increments by 1 with each tick
            # Critical for detecting missing/out-of-order UDP packets
            # Used to maintain data integrity in UDP multicast stream
            sequence_number = struct.unpack('<I', record_bytes[44:48])[0] if len(record_bytes) >= 48 else None
            
            # Optional fields (only present in longer records):
            # Offset +84: ATP - Average Traded Price (4 bytes, Little-Endian) ✓ CONFIRMED
            atp = struct.unpack('<i', record_bytes[84:88])[0] if len(record_bytes) >= 88 else 0
            
            # Offset +104: Best Bid Price ✓ CONFIRMED
            # This is also the first level of order book (Bid Level 1)
            bid_price = struct.unpack('<i', record_bytes[104:108])[0] if len(record_bytes) >= 108 else 0
            
            # 📊 ORDER BOOK PARSING - ✅ STRUCTURE DISCOVERED!
            # Order book uses 16-byte blocks per level (5 bids + 5 asks)
            # Block structure: [Price 4B][Qty 4B][Flag 4B][Unknown 4B]
            # BID/ASK INTERLEAVED: Bid1, Ask1, Bid2, Ask2, Bid3, Ask3, Bid4, Ask4, Bid5, Ask5
            # Starts at offset 104 (Bid1 price = Best Bid)
            # Total: 160 bytes for 10 levels (5 bid + 5 ask)
            order_book = None
            if len(record_bytes) >= 264:
                order_book = self._parse_order_book(record_bytes)
            
            # Fields still to find:
            ltq = 0  # Last Traded Quantity - Location TBD
            num_trades = 0  # Number of Trades - Location TBD
            
            # Record length determines what fields are available
            record_length = len(record_bytes)
            has_depth = record_length >= 264  # Full order book depth
            
            # Best ask from order book (first ask level price)
            ask_price = 0
            if order_book and order_book['asks']:
                ask_price = int(order_book['asks'][0]['price'] * 100)  # Convert back to paise
            
            # Compression starts after uncompressed section
            compressed_offset = offset + record_length
            
            logger.debug(f"Parsed record: token={token}, len={record_length}B, "
                       f"ltp={ltp} (Rs.{ltp/100:.2f}), "
                       f"open={open_price}, high={high_price}, low={low_price}, prev_close={prev_close}, "
                       f"volume={volume}, turnover={turnover_lakhs} lakhs, lot_size={lot_size}, "
                       f"atp={atp}, bid={bid_price}, ask={ask_price}, seq={sequence_number}, "
                       f"has_depth={has_depth}, order_book={bool(order_book)}")
            
            return {
                'token': token,
                'num_trades': num_trades,  # Location TBD
                'volume': volume,  # ✓ CONFIRMED (offset 24-27)
                'field_20_23': field_20_23,  # paise - Unknown field (NOT close price!)
                'prev_close': prev_close,  # paise - Previous close ✓ CONFIRMED
                'ltq': ltq,  # Location TBD
                'ltp': ltp,  # paise - Last Traded Price ✓ CONFIRMED (offset 36-39)
                'atp': atp,  # paise - Average Traded Price ✓ CONFIRMED (offset 84-87)
                'bid': bid_price,  # paise - Best Bid Price ✓ CONFIRMED (offset 104-107)
                'ask': ask_price,  # paise - Best Ask (from order book first level)
                'open': open_price,  # paise - Open price ✓ CONFIRMED
                'high': high_price,  # paise - High price ✓ CONFIRMED
                'low': low_price,   # paise - Low price ✓ CONFIRMED
                'order_book': order_book,  # ✅ 5-level bid/ask depth (offsets 104-263)
                'compressed_offset': compressed_offset,
                # ✅ CONFIRMED FIELDS:
                'turnover_lakhs': turnover_lakhs,  # ✓ Total turnover in lakhs (offset 28-31)
                'lot_size': lot_size,  # ✓ Contract lot size (offset 32-35)
                'sequence_number': sequence_number,  # ✓ Market sequence number (offset 44-47)
                # ❓ UNKNOWN FIELDS:
                'unknown_40_43': unknown_40_43,  # Always zero?
            }
            
        except struct.error as e:
            logger.error(f"Record parsing error at offset {offset}: {e}")
            return None
    
    def _parse_order_book(self, record_bytes: bytes) -> Optional[Dict]:
        """
        Parse 5-level order book depth (bid and ask).
        
        ✅ STRUCTURE CONFIRMED from live data analysis:
        - Starts at offset 104 (NOT 108!)
        - INTERLEAVED structure: Bid1, Ask1, Bid2, Ask2, Bid3, Ask3, Bid4, Ask4, Bid5, Ask5
        - Each bid/ask uses 16-byte block: [Price 4B][Qty 4B][Flag 4B][Unknown 4B]
        - Total: 5 levels × 32 bytes/level = 160 bytes (offset 104-263)
        
        Block structure:
        Bytes 0-3:   Price (int32 LE) in paise
        Bytes 4-7:   Quantity (int32 LE)
        Bytes 8-11:  Flag (int32 LE) - usually 1
        Bytes 12-15: Unknown (int32 LE) - usually 0
        
        Interleaved pattern per level:
        - Bid block (16 bytes)
        - Ask block (16 bytes)
        = 32 bytes per level
        
        Args:
            record_bytes: Full record bytes (must be ≥264 bytes)
        
        Returns:
            Dictionary with 'bids' and 'asks' arrays, or None if invalid
        """
        if len(record_bytes) < 264:
            logger.debug(f"Record too short for order book: {len(record_bytes)} < 264 bytes")
            return None
        
        try:
            order_book = {
                'bids': [],
                'asks': []
            }
            
            # Parse 5 levels (interleaved bid/ask pairs)
            for i in range(5):
                # Each level occupies 32 bytes (16 bid + 16 ask)
                bid_base = 104 + (i * 32)  # Bid block
                ask_base = bid_base + 16    # Ask block immediately follows
                
                # Parse BID (offset pattern: Price, Qty, Flag, Unknown)
                bid_price = struct.unpack('<i', record_bytes[bid_base:bid_base+4])[0]
                bid_qty = struct.unpack('<i', record_bytes[bid_base+4:bid_base+8])[0]
                bid_flag = struct.unpack('<i', record_bytes[bid_base+8:bid_base+12])[0]
                bid_unknown = struct.unpack('<i', record_bytes[bid_base+12:bid_base+16])[0]
                
                # Parse ASK (offset pattern: Price, Qty, Flag, Unknown)
                ask_price = struct.unpack('<i', record_bytes[ask_base:ask_base+4])[0]
                ask_qty = struct.unpack('<i', record_bytes[ask_base+4:ask_base+8])[0]
                ask_flag = struct.unpack('<i', record_bytes[ask_base+8:ask_base+12])[0]
                ask_unknown = struct.unpack('<i', record_bytes[ask_base+12:ask_base+16])[0]
                
                # Validate bid (skip if invalid)
                if bid_qty > 0 and bid_price > 0:
                    order_book['bids'].append({
                        'price': bid_price / 100.0,  # Convert paise to rupees
                        'quantity': bid_qty,
                        'flag': bid_flag,  # Unknown purpose (usually 1)
                        'unknown': bid_unknown  # Usually 0, sometimes other values
                    })
                else:
                    logger.debug(f"Invalid bid level {i+1}: qty={bid_qty}, price={bid_price}")
                
                # Validate ask (skip if invalid)
                if ask_qty > 0 and ask_price > 0:
                    order_book['asks'].append({
                        'price': ask_price / 100.0,  # Convert paise to rupees
                        'quantity': ask_qty,
                        'flag': ask_flag,  # Unknown purpose (usually 1)
                        'unknown': ask_unknown  # Usually 0, sometimes other values
                    })
                else:
                    logger.debug(f"Invalid ask level {i+1}: qty={ask_qty}, price={ask_price}")
            
            logger.debug(f"Parsed order book: {len(order_book['bids'])} bids, "
                        f"{len(order_book['asks'])} asks")
            
            return order_book
            
        except struct.error as e:
            logger.error(f"Order book parsing error: {e}")
            return None
    
    def _get_num_records(self, packet_size: int) -> int:
        """
        Determine number of records based on packet size.
        
        CONFIRMED PATTERN (from empirical analysis):
        - Each record occupies EXACTLY 264 bytes (fixed slot size)
        - Actual data varies (64-264 bytes), rest is NULL-padded
        - Pattern: 36 (header) + N×264 (records)
        
        Packet sizes and record counts:
        - 300 bytes = 36 + 1×264 → 1 record
        - 564 bytes = 36 + 2×264 → 2 records
        - 828 bytes = 36 + 3×264 → 3 records
        - 1092 bytes = 36 + 4×264 → 4 records
        - 1356 bytes = 36 + 5×264 → 5 records
        - 1620 bytes = 36 + 6×264 → 6 records (max for msg type 2020)
        """
        header_size = 36
        record_slot_size = 264  # Fixed slot size per record
        
        if packet_size < header_size:
            logger.warning(f"Packet size {packet_size} < header size {header_size}")
            return 0
        
        data_size = packet_size - header_size
        num_records = data_size // record_slot_size
        
        logger.debug(f"Packet {packet_size}B → {num_records} records "
                   f"(36 header + {num_records}×264 slots)")
        
        return num_records
    
    def get_stats(self) -> Dict:
        """Get decoder statistics."""
        return self.stats.copy()
    
    def reset_stats(self):
        """Reset statistics counters."""
        for key in self.stats:
            self.stats[key] = 0
        logger.info("Decoder statistics reset")


cpp_broadcast_bsefo
 


analyse python implementation indepth and check if we have missed anything frompython implementation 