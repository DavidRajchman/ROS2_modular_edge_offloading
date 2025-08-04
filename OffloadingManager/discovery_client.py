"""
Discovery Service Client for the Offloading Manager
Handles registration, keepalives, and component discovery
"""

import socket
import threading
import time
import logging
from typing import Dict, Set, Optional, List, Callable
from enum import Enum
from dataclasses import dataclass

from config_manager import ConfigManager
from algorithm import (DISCOVERY_QUERY_INTERVAL_SECONDS, DISCOVERY_INITIAL_QUERY_DELAY,
                      DISCOVERY_KEEPALIVE_MAX_FAILURES, DISCOVERY_KEEPALIVE_TIMEOUT_SECONDS,
                      DISCOVERY_REGISTRATION_TIMEOUT_MINUTES, DISCOVERY_REGISTRATION_RETRY_DELAY_SECONDS,
                      DISCOVERY_REGISTRATION_BACKOFF_MULTIPLIER, DISCOVERY_REGISTRATION_MAX_DELAY_SECONDS)

class ComponentType(Enum):
    """Discovery Service component types"""
    VEHICLE = "V"
    MEC = "M"
    BRIDGE = "B"
    OFFLOAD_MANAGER = "O"
    TEST = "T"


class ResponseCode(Enum):
    """Discovery Service response codes"""
    SUCCESS = 0
    WAIT = 1
    ID_CONFLICT = 2
    INVALID_REQUEST = 3
    GENERAL_ERROR = 4


@dataclass
class ComponentInfo:
    """Information about a discovered component"""
    component_type: str
    group_id: int
    id_in_group: int
    component_subtype: int
    component_id: str  # "group_id:id_in_group"
    
    @classmethod
    def from_discovery_data(cls, data: str) -> 'ComponentInfo':
        """Parse component info from discovery data string"""
        # Format: "component_type:group_id:id_in_group:component_subtype"
        parts = data.split(':')
        if len(parts) != 4:
            raise ValueError(f"Invalid component data format: {data}")
            
        component_type = parts[0]
        group_id = int(parts[1])
        id_in_group = int(parts[2])
        component_subtype = int(parts[3])
        component_id = f"{group_id}:{id_in_group}"
        
        return cls(component_type, group_id, id_in_group, component_subtype, component_id)


class DiscoveryProtocolError(Exception):
    """Raised when there's a protocol-level error"""
    pass


class DiscoveryClient:
    """Discovery Service client for the OM"""
    
    def __init__(self, config_manager: ConfigManager, shutdown_event: threading.Event,
                 host: str = "192.168.65.5", port: int = 9090):
        self.logger = logging.getLogger('OM.discovery')
        self.config_manager = config_manager
        self.shutdown_event = shutdown_event
        self.host = host
        self.port = port
        
        # Connection state
        self.socket: Optional[socket.socket] = None
        self.connected = False
        self.registered = False
        self.detected_ip: Optional[str] = None
        
        # Component tracking
        self.discovered_mecs: Set[str] = set()
        self.discovered_vhcs: Set[str] = set()
        self.discovery_lock = threading.Lock()
        
        # Callback for resource updates
        self.resource_update_callback: Optional[Callable[[Set[str]], None]] = None
        
        # Threading
        self.keepalive_thread: Optional[threading.Thread] = None
        self.query_thread: Optional[threading.Thread] = None
        
        # OM component details
        self.component_type = ComponentType.OFFLOAD_MANAGER.value
        self.group_id = 1
        self.id_in_group = 1
        self.component_name = "OffloadManager"
        self.listen_port = 8100
        self.subtype = 0  # OM always uses subtype 0
        
        self.logger.info(f"Discovery client initialized for {self.host}:{self.port}")
        
    def set_resource_update_callback(self, callback: Callable[[Set[str]], None]):
        """Set callback for when resource availability changes"""
        self.resource_update_callback = callback
        
    def connect_and_register(self) -> bool:
        """Connect to Discovery Service and register OM with retry logic"""
        import time
        import math
        
        start_time = time.time()
        timeout_seconds = DISCOVERY_REGISTRATION_TIMEOUT_MINUTES * 60.0
        attempt = 0
        delay = DISCOVERY_REGISTRATION_RETRY_DELAY_SECONDS
        
        self.logger.info(f"Starting Discovery Service connection and registration process (timeout: {DISCOVERY_REGISTRATION_TIMEOUT_MINUTES} minutes)")
        
        while time.time() - start_time < timeout_seconds:
            attempt += 1
            time_remaining = timeout_seconds - (time.time() - start_time)
            
            self.logger.info(f"Registration attempt {attempt} (time remaining: {time_remaining:.1f}s)")
            
            try:
                # Clean up any previous connection
                self._cleanup_connection()
                
                self.logger.info("Connecting to Discovery Service...")
                
                # Create socket connection
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.settimeout(10.0)  # 10 second timeout for individual operations
                self.socket.connect((self.host, self.port))
                self.connected = True
                
                self.logger.info(f"Connected to Discovery Service at {self.host}:{self.port}")
                
                # Send registration request
                if not self._send_registration():
                    self.logger.error("Failed to send registration request")
                    self._cleanup_connection()
                    # Don't return immediately, continue to retry logic
                else:
                    self.logger.info("Registration completed successfully")
                    
                    # Start background threads
                    self._start_background_threads()
                    
                    total_time = time.time() - start_time
                    self.logger.info(f"Discovery Service registration successful after {attempt} attempts in {total_time:.2f} seconds")
                    return True
                    
            except socket.timeout:
                self.logger.warning(f"Registration attempt {attempt} timed out after 10 seconds")
            except ConnectionRefusedError:
                self.logger.warning(f"Registration attempt {attempt} failed: Connection refused to {self.host}:{self.port}")
            except socket.gaierror as e:
                self.logger.warning(f"Registration attempt {attempt} failed: DNS resolution error for {self.host}: {e}")
            except Exception as e:
                self.logger.warning(f"Registration attempt {attempt} failed: {e}")
            
            # Clean up failed connection
            self._cleanup_connection()
            
            # Check if we have time for another attempt
            if time.time() - start_time >= timeout_seconds:
                break
                
            # Calculate delay for next attempt with exponential backoff
            actual_delay = min(delay, DISCOVERY_REGISTRATION_MAX_DELAY_SECONDS)
            remaining_time = timeout_seconds - (time.time() - start_time)
            
            if remaining_time <= actual_delay:
                self.logger.warning(f"Insufficient time remaining ({remaining_time:.1f}s) for retry delay ({actual_delay:.1f}s)")
                break
                
            self.logger.info(f"Waiting {actual_delay:.1f} seconds before retry {attempt + 1}")
            time.sleep(actual_delay)
            
            # Increase delay for next attempt (exponential backoff)
            delay *= DISCOVERY_REGISTRATION_BACKOFF_MULTIPLIER
        
        # All attempts failed
        total_time = time.time() - start_time
        self.logger.error(f"Failed to register with Discovery Service after {attempt} attempts over {total_time:.2f} seconds")
        self.logger.error(f"Discovery Service: {self.host}:{self.port}")
        self.logger.error("Check that Discovery Service is running and accessible")
        
        return False
                
    def _send_registration(self) -> bool:
        """Send registration request to Discovery Service"""
        try:
            # Get global config as JSON string
            global_config_json = self.config_manager.get_config_json_string()
            
            # Build registration message
            # Format: DISC:REG;O;S;1;1;OffloadManager;0.0.0.0;8100;{global_config_json}
            registration_msg = (
                f"DISC:REG;"
                f"{self.component_type};"
                f"S;"  # Static ID request type
                f"{self.group_id};"
                f"{self.id_in_group};"
                f"{self.component_name};"
                f"0.0.0.0;"  # Placeholder IP, Discovery Service will detect actual IP
                f"{self.listen_port};"
                f"{global_config_json}"
            )
            
            self.logger.info("Sending registration request to Discovery Service")
            self.logger.debug(f"Registration message: {registration_msg}")
            
            # Send registration
            self._send_message(registration_msg)
            
            # Receive response
            response = self._receive_message()
            if not response:
                self.logger.error("No response received from Discovery Service")
                return False
                
            self.logger.debug(f"Registration response: {response}")
            
            # Parse and validate response
            return self._parse_registration_response(response)
            
        except Exception as e:
            self.logger.error(f"Error during registration: {e}", exc_info=True)
            return False
                
    def _parse_registration_response(self, response: str) -> bool:
        """Parse registration response from Discovery Service"""
        try:
            # Expected format: DISC:ACK;0;1;1;;;;;{detected_ip};Registration successful
            parts = response.strip().split(';')
            
            if len(parts) < 3:
                self.logger.error(f"Invalid registration response format: {response}")
                return False
                
            if not response.startswith("DISC:ACK"):
                self.logger.error(f"Unexpected response type: {response}")
                return False
                
            response_code = int(parts[1])
            
            if response_code == ResponseCode.SUCCESS.value:
                # Extract detected IP if available
                if len(parts) >= 10:
                    self.detected_ip = parts[8]
                    self.logger.info(f"Discovery Service detected OM IP as: {self.detected_ip}")
                
                self.registered = True
                self.logger.info("Successfully registered with Discovery Service")
                return True
                
            elif response_code == ResponseCode.WAIT.value:
                self.logger.warning("Discovery Service responded with WAIT - this should not happen for OM")
                return False
                
            elif response_code == ResponseCode.ID_CONFLICT.value:
                self.logger.error("Registration failed: Component ID conflict")
                return False
                
            else:
                self.logger.error(f"Registration failed with response code: {response_code}")
                return False
                
        except Exception as e:
            self.logger.error(f"Error parsing registration response: {e}", exc_info=True)
            return False
            
    def _start_background_threads(self):
        """Start keepalive and query threads"""
        # Start keepalive thread
        self.keepalive_thread = threading.Thread(
            target=self._keepalive_loop,
            name="DiscoveryKeepalive"
        )
        self.keepalive_thread.daemon = True
        self.keepalive_thread.start()
        
        # Start component query thread
        self.query_thread = threading.Thread(
            target=self._query_loop,
            name="DiscoveryQuery"
        )
        self.query_thread.daemon = True
        self.query_thread.start()
        
        self.logger.info("Discovery Service background threads started")
        
    def _keepalive_loop(self):
        """Background thread for sending keepalives"""
        keepalive_interval = 5.0  # 5 seconds as per protocol
        consecutive_failures = 0
        
        self.logger.info("Keepalive thread started - sending pings every 5 seconds")
        
        while not self.shutdown_event.is_set() and self.connected:
            try:
                # Send keepalive ping
                ping_msg = f"DISC:PNG;{self.group_id}.{self.id_in_group};OK;Keepalive from OM"
                self._send_message(ping_msg)
                
                # Receive response
                response = self._receive_message(timeout=DISCOVERY_KEEPALIVE_TIMEOUT_SECONDS)
                if response and response.startswith("DISC:PON;OK;"):
                    self.logger.debug("Keepalive successful")
                    consecutive_failures = 0  # Reset failure counter on success
                else:
                    consecutive_failures += 1
                    self.logger.warning(f"Unexpected keepalive response: {response} (failure {consecutive_failures}/{DISCOVERY_KEEPALIVE_MAX_FAILURES})")
                    
                    if consecutive_failures >= DISCOVERY_KEEPALIVE_MAX_FAILURES:
                        self.logger.error(f"Discovery Service keepalive failed {DISCOVERY_KEEPALIVE_MAX_FAILURES} times - triggering OM shutdown")
                        self.shutdown_event.set()  # Trigger main application shutdown
                        break
                    
            except Exception as e:
                consecutive_failures += 1
                self.logger.error(f"Keepalive error: {e} (failure {consecutive_failures}/{DISCOVERY_KEEPALIVE_MAX_FAILURES})", exc_info=True)
                
                if consecutive_failures >= DISCOVERY_KEEPALIVE_MAX_FAILURES:
                    self.logger.error(f"Discovery Service connection failed {DISCOVERY_KEEPALIVE_MAX_FAILURES} times - triggering OM shutdown")
                    self.shutdown_event.set()  # Trigger main application shutdown
                    break
                    
            # Wait for next keepalive interval (but break immediately if shutdown requested)
            if self.shutdown_event.wait(keepalive_interval):
                break
        
        # Mark as disconnected when keepalive loop exits
        self.connected = False
        self.registered = False
        
        self.logger.info("Keepalive thread stopped")
        
    def _query_loop(self):
        """Background thread for querying components"""
        # Initial delay before first query
        self.logger.info(f"Component query thread started - initial delay {DISCOVERY_INITIAL_QUERY_DELAY}s")
        time.sleep(DISCOVERY_INITIAL_QUERY_DELAY)
        
        while not self.shutdown_event.is_set() and self.connected:
            try:
                # Query for MECs
                self._query_components("M")
                
                # Query for VHCs (for research data)
                self._query_components("V")
                
            except Exception as e:
                self.logger.error(f"Component query error: {e}", exc_info=True)
                
            # Wait for next query interval
            self.shutdown_event.wait(DISCOVERY_QUERY_INTERVAL_SECONDS)
        
        self.logger.info("Component query thread stopped")
        
    def _query_components(self, component_type: str):
        """Query Discovery Service for specific component type"""
        try:
            # Send query message
            # Format: DISC:QRY;component_type_filter;HumanReadableMessage
            query_msg = f"DISC:QRY;{component_type};Query for {component_type} components from OM"
            
            self.logger.debug(f"Querying for component type: {component_type}")
            self._send_message(query_msg)
            
            # Receive response
            response = self._receive_message(timeout=10.0)
            if not response:
                self.logger.warning(f"No response to {component_type} query")
                return
                
            # Parse query response
            components = self._parse_query_response(response)
            
            # Update component tracking
            with self.discovery_lock:
                if component_type == "M":
                    old_mecs = self.discovered_mecs.copy()
                    self.discovered_mecs = {comp.component_id for comp in components if comp.component_type == "M"}
                    
                    if old_mecs != self.discovered_mecs:
                        self.logger.info(f"MEC availability updated: {len(self.discovered_mecs)} MECs available")
                        self.logger.debug(f"Available MECs: {sorted(self.discovered_mecs)}")
                        
                        # Notify decision engine of resource changes
                        if self.resource_update_callback:
                            self.resource_update_callback(self.discovered_mecs)
                            
                elif component_type == "V":
                    old_vhcs = self.discovered_vhcs.copy()
                    self.discovered_vhcs = {comp.component_id for comp in components if comp.component_type == "V"}
                    
                    if old_vhcs != self.discovered_vhcs:
                        self.logger.info(f"VHC discovery updated: {len(self.discovered_vhcs)} VHCs available")
                        self.logger.debug(f"Available VHCs: {sorted(self.discovered_vhcs)}")
                
        except Exception as e:
            self.logger.error(f"Error querying {component_type} components: {e}", exc_info=True)
            
    def _parse_query_response(self, response: str) -> List[ComponentInfo]:
        """Parse component query response"""
        try:
            # Expected format: DISC:LST;component_count;component1_data;component2_data;...
            if not response.startswith("DISC:LST;"):
                self.logger.warning(f"Unexpected query response format: {response}")
                return []
                
            parts = response.split(';')
            if len(parts) < 2:
                self.logger.warning(f"Invalid query response: {response}")
                return []
                
            component_count = int(parts[1])
            self.logger.debug(f"Query response indicates {component_count} components")
            
            components = []
            for i in range(2, min(len(parts), 2 + component_count)):
                try:
                    component_data = parts[i]
                    if component_data:  # Skip empty entries
                        component = ComponentInfo.from_discovery_data(component_data)
                        components.append(component)
                        self.logger.debug(f"Parsed component: {component.component_id} ({component.component_type})")
                except Exception as e:
                    self.logger.warning(f"Failed to parse component data '{component_data}': {e}")
                    
            return components
            
        except Exception as e:
            self.logger.error(f"Error parsing query response: {e}", exc_info=True)
            return []
            
    def query_mecs_on_demand(self) -> Set[str]:
        """Query MECs on demand (for future MEC creation events)"""
        try:
            self.logger.info("Performing on-demand MEC query")
            self._query_components("M")
            
            with self.discovery_lock:
                return self.discovered_mecs.copy()
                
        except Exception as e:
            self.logger.error(f"Error in on-demand MEC query: {e}", exc_info=True)
            return set()
            
    def get_available_resources(self) -> tuple[Set[str], Set[str]]:
        """Get current available MECs and VHCs"""
        with self.discovery_lock:
            return self.discovered_mecs.copy(), self.discovered_vhcs.copy()
            
    def _send_message(self, message: str):
        """Send message to Discovery Service"""
        if not self.socket or not self.connected:
            raise DiscoveryProtocolError("Not connected to Discovery Service")
            
        try:
            message_bytes = message.encode('utf-8')
            self.socket.send(message_bytes)
            self.logger.debug(f"Sent message to Discovery Service: {message}")
        except Exception as e:
            self.logger.error(f"Failed to send message to Discovery Service: {e}")
            raise DiscoveryProtocolError(f"Send failed: {e}")
        
    def _receive_message(self, timeout: float = 10.0) -> Optional[str]:
        """Receive message from Discovery Service"""
        if not self.socket or not self.connected:
            raise DiscoveryProtocolError("Not connected to Discovery Service")
            
        try:
            # Set socket timeout
            original_timeout = self.socket.gettimeout()
            self.socket.settimeout(timeout)
            
            # Receive response
            data = self.socket.recv(2048)
            
            # Restore original timeout
            self.socket.settimeout(original_timeout)
            
            if not data:
                self.logger.warning("Received empty data from Discovery Service")
                return None
                
            response = data.decode('utf-8').strip()
            self.logger.debug(f"Received message from Discovery Service: {response}")
            return response
            
        except socket.timeout:
            self.logger.warning(f"Timeout ({timeout}s) waiting for Discovery Service response")
            return None
        except Exception as e:
            self.logger.error(f"Failed to receive message from Discovery Service: {e}")
            raise DiscoveryProtocolError(f"Receive failed: {e}")
            
    def disconnect(self):
        """Disconnect from Discovery Service"""
        self.logger.info("Disconnecting from Discovery Service")
        self.connected = False
        self.registered = False
        
        # Stop background threads
        if self.keepalive_thread and self.keepalive_thread.is_alive():
            self.logger.debug("Waiting for keepalive thread to stop")
            self.keepalive_thread.join(timeout=2.0)
            
        if self.query_thread and self.query_thread.is_alive():
            self.logger.debug("Waiting for query thread to stop")
            self.query_thread.join(timeout=2.0)
            
        # Close socket
        self._cleanup_connection()
        
        self.logger.info("Discovery Service disconnection complete")
        
    def _cleanup_connection(self):
        """Clean up socket connection"""
        if self.socket:
            try:
                self.socket.close()
            except Exception as e:
                self.logger.debug(f"Error closing Discovery Service socket: {e}")
            self.socket = None
            
        self.connected = False
        self.registered = False