#!/usr/bin/env python3
"""
Component Simulator for Discovery Service

Usage:
    python3 component_simulator.py -OM -BRIDGE -VHC -MEC
    python3 component_simulator.py --OM --BRIDGE --VHC --MEC

This script simulates multiple components connecting to the Discovery Service
and maintains persistent connections with proper keepalives.
"""

import socket
import time
import sys
import threading
import argparse
import signal

# --- Configuration ---
DISCOVERY_HOST = "localhost"
DISCOVERY_PORT = 9090
KEEPALIVE_INTERVAL_S = 5  # Should be < server timeout (15s)

# Global configuration string to be sent by the Offloading Manager
GLOBAL_CONFIGURATION = '{"system_mode":"production","log_level":"info","max_vehicles":10}'

class ComponentSimulator:
    """
    Simulates a single component connecting to the DiscoveryService.
    Maintains persistent connection and handles keepalives.
    """
    def __init__(self, component_type, component_name, group_id, id_in_group, listen_port):
        self.component_type = component_type
        self.name = component_name
        self.group_id = group_id
        self.id_in_group = id_in_group
        self.listen_port = listen_port
        self.socket = None
        self.component_id_str = f"{group_id}.{id_in_group}"
        self._keepalive_thread = None
        self._stop_event = threading.Event()
        self.registered = False
        
    def connect(self):
        """Establishes connection to the Discovery Service."""
        try:
            print(f"[{self.name}] Connecting to {DISCOVERY_HOST}:{DISCOVERY_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10.0)
            self.socket.connect((DISCOVERY_HOST, DISCOVERY_PORT))
            print(f"[{self.name}] Connected successfully.")
            return True
        except Exception as e:
            print(f"[{self.name}] Failed to connect: {e}")
            return False
    
    def register(self):
        """Sends registration request to the Discovery Service."""
        if not self.socket:
            return False
            
        placeholder_ip = "0.0.0.0"
        
        if self.component_type == "O":  # Offloading Manager
            message = f'DISC:REG;{self.component_type};S;{self.group_id};{self.id_in_group};{self.name};{placeholder_ip};{self.listen_port};{GLOBAL_CONFIGURATION}'
        else:
            message = f'DISC:REG;{self.component_type};S;{self.group_id};{self.id_in_group};{self.name};{placeholder_ip};{self.listen_port};{self.name} requesting registration'
        
        try:
            print(f"[{self.name}] Sending registration...")
            self.socket.sendall(message.encode('utf-8'))
            
            response = self.socket.recv(2048).decode('utf-8').strip()
            print(f"[{self.name}] Registration response: {response}")
            
            if response.startswith("DISC:ACK;0;"):
                self.registered = True
                print(f"[{self.name}] Successfully registered!")
                return True
            elif response.startswith("DISC:ACK;1;"):
                print(f"[{self.name}] Registration status: WAIT")
                return False
            else:
                print(f"[{self.name}] Registration failed: {response}")
                return False
                
        except Exception as e:
            print(f"[{self.name}] Registration error: {e}")
            return False
    
    def start_keepalives(self):
        """Starts the keepalive thread."""
        if not self.registered:
            print(f"[{self.name}] Cannot start keepalives: not registered")
            return
            
        self._stop_event.clear()
        self._keepalive_thread = threading.Thread(target=self._keepalive_loop, daemon=True)
        self._keepalive_thread.start()
        print(f"[{self.name}] Keepalive thread started")
    
    def _keepalive_loop(self):
        """Keepalive loop that runs in a separate thread."""
        while not self._stop_event.is_set():
            try:
                ping_msg = f"DISC:PNG;{self.component_id_str};OK;Keepalive from {self.name}"
                self.socket.sendall(ping_msg.encode('utf-8'))
                
                response = self.socket.recv(1024).decode('utf-8').strip()
                
                if not response.startswith("DISC:PON;OK;"):
                    print(f"[{self.name}] Keepalive failed: {response}")
                    break
                    
            except Exception as e:
                print(f"[{self.name}] Keepalive error: {e}")
                break
                
            self._stop_event.wait(KEEPALIVE_INTERVAL_S)
        
        print(f"[{self.name}] Keepalive thread stopped")
    
    def stop(self):
        """Stops the component and closes connection."""
        self._stop_event.set()
        if self._keepalive_thread and self._keepalive_thread.is_alive():
            self._keepalive_thread.join(timeout=2.0)
        
        if self.socket:
            self.socket.close()
            self.socket = None
        
        print(f"[{self.name}] Stopped")

class DiscoveryServiceSimulator:
    """Main simulator that manages multiple components."""
    
    def __init__(self):
        self.components = []
        self.running = True
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
    
    def _signal_handler(self, signum, frame):
        """Handles shutdown signals."""
        print(f"\nReceived signal {signum}. Shutting down...")
        self.running = False
    
    def add_component(self, comp_type, name, group_id, id_in_group, port):
        """Adds a component to be simulated."""
        component = ComponentSimulator(comp_type, name, group_id, id_in_group, port)
        self.components.append(component)
        return component
    
    def run(self):
        """Main execution loop."""
        if not self.components:
            print("No components to simulate. Exiting.")
            return
        
        print("=" * 60)
        print("Discovery Service Component Simulator")
        print("=" * 60)
        print(f"Global Configuration: {GLOBAL_CONFIGURATION}")
        print(f"Components to simulate: {len(self.components)}")
        print("=" * 60)
        
        # Connect all components
        for component in self.components:
            if not component.connect():
                print(f"Failed to connect {component.name}. Exiting.")
                return
            time.sleep(0.5)  # Brief delay between connections
        
        # Register components in proper order
        max_retries = 10
        for component in self.components:
            retries = 0
            while retries < max_retries and self.running:
                if component.register():
                    break
                
                retries += 1
                print(f"[{component.name}] Registration attempt {retries}/{max_retries} failed. Retrying in 3 seconds...")
                time.sleep(3)
            
            if not component.registered:
                print(f"[{component.name}] Failed to register after {max_retries} attempts. Exiting.")
                return
            
            time.sleep(1)  # Brief delay between registrations
        
        # Start keepalives for all registered components
        for component in self.components:
            component.start_keepalives()
        
        print("\nAll components registered and keepalives started.")
        print("Press Ctrl+C to stop the simulator.")
        
        # Main loop - just wait for shutdown signal
        try:
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nShutdown requested by user.")
        
        # Clean shutdown
        print("\nStopping all components...")
        for component in self.components:
            component.stop()
        
        print("All components stopped. Goodbye!")

def main():
    parser = argparse.ArgumentParser(description='Discovery Service Component Simulator')
    parser.add_argument('-OM', '--OM', action='store_true', help='Simulate Offloading Manager')
    parser.add_argument('-BRIDGE', '--BRIDGE', action='store_true', help='Simulate Bridge')
    parser.add_argument('-VHC', '--VHC', action='store_true', help='Simulate Vehicle')
    parser.add_argument('-MEC', '--MEC', action='store_true', help='Simulate MEC')
    
    args = parser.parse_args()
    
    if not any([args.OM, args.BRIDGE, args.VHC, args.MEC]):
        print("Error: You must specify at least one component type to simulate.")
        print("Usage: python3 component_simulator.py -OM -BRIDGE -VHC -MEC")
        sys.exit(1)
    
    simulator = DiscoveryServiceSimulator()
    
    # Add components in the correct order: OM first, then Bridge, then VHC/MEC
    if args.OM:
        simulator.add_component("O", "OffloadManager", 1, 1, 8000)
    
    if args.BRIDGE:
        simulator.add_component("B", "Bridge-Simulator", 5, 1, 7000)
    
    if args.VHC:
        simulator.add_component("V", "Vehicle-Simulator", 50, 10, 6000)
    
    if args.MEC:
        simulator.add_component("M", "MEC-Simulator", 60, 10, 8080)
    
    simulator.run()

if __name__ == "__main__":
    main()