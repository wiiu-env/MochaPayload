import socket
import threading
import time
import random
import sys

# --- Configuration ---
TCP_PORT = 7965
UDP_BEACON_PORT = 4445

# Global state
current_client = None
client_lock = threading.Lock()

def udp_broadcaster():
    """Beacon so Wii U finds us automatically"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    msg = b"WIIU_TCP_SYSLOG_BEACON_V1"
    
    print(f"[*] UDP Beacon active on port {UDP_BEACON_PORT}")
    
    while True:
        # Only broadcast if no one is connected
        if current_client is None:
            try:
                sock.sendto(msg, ('<broadcast>', UDP_BEACON_PORT))
            except:
                pass
        time.sleep(0.5)

def heartbeat_loop(sock):
    """Sends a hidden ping every second to keep Wii U connection alive."""
    while True:
        with client_lock:
            # If socket is no longer the active client, stop thread
            if sock != current_client:
                break
        
        try:
            time.sleep(1.0)
            # Send a NULL byte. The Wii U logic ignores this but resets watchdog.
            sock.sendall(b'\x00') 
        except:
            # If send fails, the main handler will catch it eventually, 
            # or we can force close here.
            break


def handle_client(sock, addr):
    """Reads logs from Wii U and prints them"""
    global current_client
    
    print(f"\n[+] Connected to {addr[0]}")
    print("[*] Ready for logs (Type commands anytime)...\n")
    
    # Start Heartbeat
    hb_thread = threading.Thread(target=heartbeat_loop, args=(sock,), daemon=True)
    hb_thread.start()
    
    sock.settimeout(1.0)
    rx_buffer = ""

    while True:
        try:
            data = sock.recv(4096)
            if not data: 
                print("[!] Remote closed connection.")
                break
                                            
            # Decode and process lines
            text = data.decode('utf-8', errors='replace')
            
            # Filter NULL bytes (Heartbeat artifacts)
            text = text.replace('\x00', '')
            
            rx_buffer += text
            
            while "\r" in rx_buffer:
                line, rx_buffer = rx_buffer.split("\r", 1)
                # Print immediately
                print(f"{line}")
                
        except socket.timeout:
            continue
        except Exception as e:
            print(f"[!] Connection Error: {e}")
            break
            
    print(f"[-] Disconnected from {addr[0]}")
    print(f"Waiting for new connections")
    
    with client_lock:
        if current_client == sock:
            current_client = None
    try:
        sock.close()
    except: pass

def server_listener():
    """Waits for incoming TCP connections"""
    global current_client
    
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('0.0.0.0', TCP_PORT))
    except Exception as e:
        print(f"[!] Fatal: Could not bind port {TCP_PORT}. {e}")
        return

    server.listen(1)
    print(f"[*] TCP Server listening on {TCP_PORT}")
    
    while True:
        try:
            client, addr = server.accept()       

            current_client = client
            
            print(f"[*] Server connected on port {TCP_PORT}")
            # Handle this client in this thread (blocks until disconnect)
            handle_client(client, addr)
         
        except Exception as e:
            print(f"[!] Server Error: {e}")

if __name__ == "__main__":
    # 1. Start UDP Beacon (Background)
    threading.Thread(target=udp_broadcaster, daemon=True).start()
    
    # 2. Start TCP Server (Background)
    threading.Thread(target=server_listener, daemon=True).start()
    
    # 3. Main Thread: Handle User Input
    try:
        while True:
            # Simple input loop
            cmd = input() 
            
            with client_lock:
                if current_client:
                    try:
                        current_client.sendall((cmd + "\r\n").encode())
                    except:
                        print("[!] Failed to send command (Connection dead?)")
                        # Force close to reset state
                        try: current_client.close() 
                        except: pass
                        current_client = None
                else:
                    print("[!] Not connected to Wii U")
                    
    except KeyboardInterrupt:
        print("\n[*] Exiting...")
        sys.exit(0)