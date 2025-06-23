import socket
import time
import sys
import threading

# --- Configuration ---
DISCOVERY_HOST = "localhost"
DISCOVERY_PORT = 9090
KEEPALIVE_INTERVAL_S = 5  # Should be < server timeout (15s)
WAIT_FOR_TIMEOUT_S = 20 # Should be > server timeout (15s)

class ComponentClient:
    """
    Represents a single component connecting to the DiscoveryService.
    It maintains its own persistent TCP connection and can run a keepalive loop.
    """
    def __init__(self, component_name, host=DISCOVERY_HOST, port=DISCOVERY_PORT):
        self.name = component_name
        self.host = host
        self.port = port
        self.socket = None
        self.component_id_str = None
        self._keepalive_thread = None
        self._stop_event = threading.Event()
        print(f"Initialized client for component: {self.name}")

    def connect(self):
        """Establishes the persistent connection to the service."""
        if self.socket:
            return True
        try:
            print(f"[{self.name}] Connecting to {self.host}:{self.port}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(3.0)
            self.socket.connect((self.host, self.port))
            print(f"[{self.name}] Connection successful.")
            return True
        except Exception as e:
            print(f"[{self.name}] FAILED to connect: {e}")
            self.socket = None
            raise

    def close(self):
        """Stops any running threads and closes the socket."""
        self.stop_keepalives()
        if self.socket:
            print(f"[{self.name}] Closing connection.")
            self.socket.close()
            self.socket = None

    def send_and_receive(self, message, test_name="", expected_prefix=None, contains=None):
        """Sends a message, gets a response, and validates it."""
        if not self.socket:
            print(f"ERROR [{self.name}]: Not connected.")
            return False, "NOT_CONNECTED"

        if test_name:
            print(f"--- Test: {test_name} (via {self.name}) ---")
        print(f"SENDING: {message}")
        
        response = ""
        try:
            self.socket.sendall(message.encode('utf-8'))
            raw_response = self.socket.recv(2048)
            if not raw_response: # Connection closed gracefully by the server
                print(f"ERROR [{self.name}]: Connection was closed by the server.")
                self.socket = None
                response = "CONNECTION_LOST_ERROR"
            else:
                response = raw_response.decode('utf-8').strip()
        except socket.timeout:
            print(f"ERROR [{self.name}]: Receive timed out.")
            response = "TIMEOUT_ERROR"
        except (ConnectionResetError, BrokenPipeError):
            print(f"ERROR [{self.name}]: Connection was lost.")
            self.socket = None # Mark connection as dead
            response = "CONNECTION_LOST_ERROR"
        except Exception as e:
            print(f"ERROR [{self.name}]: An unexpected error occurred: {e}")
            response = "COMMUNICATION_ERROR"
        
        print(f"RECEIVED: {response}")

        if not test_name:
            return True, response # No validation needed for simple pings

        passed = True
        reason = ""
        if expected_prefix and not response.startswith(expected_prefix):
            passed = False
            reason = f"Expected prefix '{expected_prefix}'"
        
        if passed and contains:
            for substring in contains:
                if substring not in response:
                    passed = False
                    reason = f"Did not contain '{substring}'"
                    break

        if passed:
            print("RESULT: [PASS]")
        else:
            print(f"RESULT: [FAIL] - Reason: {reason}")
        
        print("-" * (len(test_name) + len(self.name) + 10))
        print()
        time.sleep(0.25)
        return passed, response

    def _keepalive_loop(self):
        """The function that runs in a thread, sending pings periodically."""
        if not self.component_id_str:
            print(f"[{self.name}] Cannot start keepalives: component ID not set.")
            return

        print(f"[{self.name}] Keepalive thread started. Sending pings every {KEEPALIVE_INTERVAL_S}s.")
        while not self._stop_event.is_set():
            ping_msg = f"DISC:PNG;{self.component_id_str};OK;Keepalive"
            _, response = self.send_and_receive(ping_msg)
            if "ERROR" in response:
                print(f"[{self.name}] Keepalive failed. Stopping thread.")
                break
            self._stop_event.wait(KEEPALIVE_INTERVAL_S)
        print(f"[{self.name}] Keepalive thread stopped.")

    def start_keepalives(self, group_id, id_in_group):
        """Starts the background keepalive thread."""
        self.component_id_str = f"{group_id}.{id_in_group}"
        if self._keepalive_thread:
            return
        self._stop_event.clear()
        self._keepalive_thread = threading.Thread(target=self._keepalive_loop, daemon=True)
        self._keepalive_thread.start()

    def stop_keepalives(self):
        """Signals the keepalive thread to stop."""
        self._stop_event.set()
        if self._keepalive_thread and self._keepalive_thread.is_alive():
            self._keepalive_thread.join(timeout=1.0)

def run_full_test_suite():
    """
    Executes a comprehensive, two-part test suite for the DiscoveryService.
    Part 1: Initial registration and validation tests.
    Part 2: Optional long-running keepalive and timeout tests.
    """
    clients = {
        "PreOM": ComponentClient("PreOMTester"),
        "OM": ComponentClient("OffloadManager"),
        "VHC1": ComponentClient("Vehicle-1"),
        "Bridge": ComponentClient("Bridge-Main"),
        "MEC1": ComponentClient("MEC-1"),
        "Conflict": ComponentClient("ConflictTester"),
        "Malformed": ComponentClient("MalformedClient"),
        "BadClient": ComponentClient("BadClient") # For keepalive test
    }

    initial_passed = 0
    initial_failed = 0
    
    try:
        print("=" * 60)
        print("Starting DiscoveryService Test Suite - PART 1: Initial Tests")
        print("=" * 60)
        time.sleep(1)

        print("\n--- Connecting all simulated clients ---")
        for client in clients.values():
            client.connect()
        print("--- All clients connected ---\n")

        config_string = '{"system_mode":"test","log_level":"debug"}'
        test_cases = [
            ("PreOM", 'DISC:REG;T;S;99;1;PreOM-Test;127.0.0.1;9998;Testing before OM', "Register Component Before OM", {"expected_prefix": "DISC:ACK;1;"}),
            ("OM", f'DISC:REG;O;S;1;1;OffloadManager;127.0.0.1;8000;{config_string}', "Register Offloading Manager", {"expected_prefix": "DISC:ACK;0;"}),
            ("VHC1", 'DISC:REG;V;S;2;10;TestVHC-01;10.0.0.5;6000;VHC requesting bridge', "Register Vehicle (No Bridge)", {"expected_prefix": "DISC:ACK;1;"}),
            ("Bridge", 'DISC:REG;B;S;5;1;TestBridge-Main;192.168.1.100;7000;Main bridge component', "Register Bridge", {"expected_prefix": "DISC:ACK;0;", "contains": [config_string]}),
            ("VHC1", 'DISC:REG;V;S;2;10;TestVHC-01;10.0.0.5;6000;VHC requesting bridge', "Re-Register Vehicle (Bridge Exists)", {"expected_prefix": "DISC:ACK;0;", "contains": ["B;192.168.1.100;7000", config_string]}),
            ("MEC1", 'DISC:REG;M;S;12;1;TestMEC-01;192.168.1.200;8080;MEC requesting bridge', "Register MEC (Bridge Exists)", {"expected_prefix": "DISC:ACK;0;", "contains": ["B;192.168.1.100;7000", config_string]}),
            ("Bridge", 'DISC:PNG;5.1;OK;Ping from Bridge', "Send Keepalive Ping", {"expected_prefix": "DISC:PON;OK;"}),
            ("Conflict", 'DISC:REG;T;S;5;1;ConflictTest;127.0.0.1;9999;Testing conflict', "Test ID Conflict", {"expected_prefix": "DISC:ACK;2;"}),
            ("Malformed", "This is not a valid protocol message", "Test Malformed Message", {"expected_prefix": "DISC:ERR;"})
        ]

        for client_name, msg, test_name, rules in test_cases:
            passed, _ = clients[client_name].send_and_receive(msg, test_name, **rules)
            if passed:
                initial_passed += 1
            else:
                initial_failed += 1

        print("=" * 60)
        print("PART 1 Summary")
        print(f"  Tests Passed: {initial_passed}")
        print(f"  Tests Failed: {initial_failed}")
        print("=" * 60)

        if initial_failed > 0:
            print("\nAborting due to failures in initial tests.")
            return

        # --- Part 2: Keepalive Tests ---
        print("\n" + "=" * 60)
        print("Starting Test Suite - PART 2: Keepalive and Timeout Tests")
        print(f"This test will wait {WAIT_FOR_TIMEOUT_S}s to check for disconnects.")
        print("=" * 60 + "\n")

        # Start keepalives for critical components
        clients["OM"].start_keepalives(1, 1)
        clients["Bridge"].start_keepalives(5, 1)

        # Register the non-compliant client
        bad_client_passed, _ = clients["BadClient"].send_and_receive(
            'DISC:REG;T;S;20;1;BadClient;127.0.0.1;2001;I will not send keepalives',
            "Register Non-Compliant Client",
            expected_prefix="DISC:ACK;0;"
        )
        
        if not bad_client_passed:
            print("Could not register the non-compliant client. Aborting keepalive test.")
            return

        print(f"\nAll components registered. Waiting {WAIT_FOR_TIMEOUT_S} seconds for server to purge stale clients...")
        time.sleep(WAIT_FOR_TIMEOUT_S)

        # Final checks
        print("\n--- Verifying client statuses after timeout period ---")
        
        # 1. Check the compliant client (Bridge)
        print("\nChecking compliant client (Bridge)...")
        _, bridge_resp = clients["Bridge"].send_and_receive("DISC:PNG;5.1;OK;Final Check", "Final check on compliant client")
        
        # 2. Check the non-compliant client (BadClient)
        print("\nChecking non-compliant client (BadClient)...")
        _, bad_client_resp = clients["BadClient"].send_and_receive("DISC:PNG;20.1;OK;Final Check", "Final check on non-compliant client")

        keepalive_passed = "DISC:PON" in bridge_resp
        timeout_passed = "CONNECTION_LOST_ERROR" in bad_client_resp

        print("\n" + "=" * 60)
        print("PART 2 Summary")
        print(f"  Compliant client remained connected: {'PASS' if keepalive_passed else 'FAIL'}")
        print(f"  Non-compliant client was disconnected: {'PASS' if timeout_passed else 'FAIL'}")
        print("=" * 60)

    except (Exception, KeyboardInterrupt) as e:
        print(f"\n!!! TEST SUITE ABORTED due to a critical error: {e} !!!")
    finally:
        print("\n--- Closing all client connections ---")
        for client in clients.values():
            client.close()
        print("--- All connections closed ---\n")
        
        if initial_failed > 0:
            sys.exit(1)

if __name__ == "__main__":
    run_full_test_suite()