import socket
import struct
import time

MCAST_GRP = '233.1.2.5'
MCAST_PORT = 34330 # and 34331

def listen(port):
    print(f"Listening on {MCAST_GRP}:{port}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('', port))
    
    mreq = struct.pack("4sl", socket.inet_aton(MCAST_GRP), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    
    sock.settimeout(5.0)
    
    count = 0
    start_time = time.time()
    try:
        while time.time() - start_time < 10:
            try:
                data, addr = sock.recvfrom(65535)
                count += 1
                if count == 1:
                    print(f"Received first packet from {addr}, size {len(data)}")
            except socket.timeout:
                continue
    except KeyboardInterrupt:
        pass
    
    print(f"Received {count} packets on port {port}")

if __name__ == "__main__":
    listen(34330)
    listen(34331)
