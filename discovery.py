import socket
import time

def discover_esp(port=12345):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(2.0)

    msg = b"what is your ip"
    print(f"Broadcasting discovery packet to port {port}...")

    try:
        # Broadcast to local network
        sock.sendto(msg, ('<broadcast>', port))

        while True:
            try:
                data, addr = sock.recvfrom(1024)
                print(f"Received response from {addr}: {data.decode('utf-8')}")
            except socket.timeout:
                print("No more responses.")
                break
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    discover_esp()
