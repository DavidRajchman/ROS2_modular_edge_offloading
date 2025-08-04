"""
OM TCP Server for Bridge CP connections
Handles JSON message protocol and coordinates with decision engine
"""

import socket
import threading
import json
import logging
import time
from typing import Dict, Optional, Set
from dataclasses import dataclass
from enum import IntEnum

from config_manager import ConfigManager
from decision_engine import DecisionEngine


class MessageCode(IntEnum):
    """Control plane message codes"""
    OFFLOAD_REQUEST = 100
    SESSION_KEEPALIVE = 102
    SESSION_APPROVED = 200
    SESSION_DENIED = 201
    BRIDGE_DP_FAILURE = 301
    ACK = 900


@dataclass
class ConnectionState:
    """State for individual Bridge CP connection"""
    client_socket: socket.socket
    address: tuple
    sequence_number: int = 0
    active: bool = True
    

class OffloadingManagerServer:
    """TCP Server for handling Bridge CP connections"""
    
    def __init__(self, config_manager: ConfigManager, shutdown_event: threading.Event):
        self.logger = logging.getLogger('OM.server')
        self.config_manager = config_manager
        self.shutdown_event = shutdown_event
        self.decision_engine = DecisionEngine(config_manager)
        
        # Server state
        self.server_socket: Optional[socket.socket] = None
        self.connections: Dict[int, ConnectionState] = {}
        self.connection_counter = 0
        self.connection_lock = threading.Lock()
        
        # Component ID for OM (always 1:1)
        self.component_id = "1:1"
        
    def start(self):
        """Start the TCP server"""
        try:
            self.logger.info("Starting OM TCP server on localhost:8100")
            
            # Create server socket
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind(('localhost', 8100))
            self.server_socket.listen(5)
            self.server_socket.settimeout(1.0)  # Non-blocking accept
            
            self.logger.info("OM Server listening for Bridge CP connections")
            
            while not self.shutdown_event.is_set():
                try:
                    client_socket, address = self.server_socket.accept()
                    self.logger.info(f"New Bridge CP connection from {address}")
                    
                    # Create connection state
                    with self.connection_lock:
                        conn_id = self.connection_counter
                        self.connection_counter += 1
                        self.connections[conn_id] = ConnectionState(client_socket, address)
                    
                    # Handle connection in separate thread
                    conn_thread = threading.Thread(
                        target=self._handle_connection,
                        args=(conn_id,),
                        name=f"BridgeCP-{conn_id}"
                    )
                    conn_thread.daemon = True
                    conn_thread.start()
                    
                except socket.timeout:
                    continue  # Check shutdown event
                except Exception as e:
                    if not self.shutdown_event.is_set():
                        self.logger.error(f"Error accepting connection: {e}")
                        
        except Exception as e:
            self.logger.error(f"Server startup error: {e}")
            self.shutdown_event.set()
            
    def stop(self):
        """Stop the server and close all connections"""
        self.logger.info("Stopping OM server")
        
        # Close all client connections
        with self.connection_lock:
            for conn_id, conn_state in self.connections.items():
                try:
                    conn_state.active = False
                    conn_state.client_socket.close()
                    self.logger.info(f"Closed connection {conn_id}")
                except Exception as e:
                    self.logger.warning(f"Error closing connection {conn_id}: {e}")
                    
        # Close server socket
        if self.server_socket:
            try:
                self.server_socket.close()
                self.logger.info("Server socket closed")
            except Exception as e:
                self.logger.warning(f"Error closing server socket: {e}")
                
    def _handle_connection(self, conn_id: int):
        """Handle individual Bridge CP connection"""
        conn_state = self.connections[conn_id]
        self.logger.info(f"Handling Bridge CP connection {conn_id} from {conn_state.address}")
        
        try:
            while conn_state.active and not self.shutdown_event.is_set():
                try:
                    # Receive message
                    data = conn_state.client_socket.recv(4096)
                    if not data:
                        self.logger.info(f"Connection {conn_id} closed by peer")
                        break
                        
                    message_str = data.decode('utf-8').strip()
                    self.logger.debug(f"Received from {conn_id}: {message_str}")
                    
                    # Process message
                    self._process_message(conn_id, message_str)
                    
                except socket.timeout:
                    continue
                except Exception as e:
                    self.logger.error(f"Error handling connection {conn_id}: {e}")
                    break
                    
        finally:
            # Cleanup connection
            with self.connection_lock:
                if conn_id in self.connections:
                    try:
                        self.connections[conn_id].client_socket.close()
                    except:
                        pass
                    del self.connections[conn_id]
                    
            self.logger.info(f"Connection {conn_id} cleaned up")
            
    def _process_message(self, conn_id: int, message_str: str):
        """Process incoming JSON message from Bridge CP"""
        try:
            # Parse JSON
            message = json.loads(message_str)
            self.logger.info(f"Processing message from {conn_id}: {message.get('message_type', 'UNKNOWN')}")
            
            # Validate message structure
            required_fields = ['component_id', 'message_code', 'message_type', 'sequence_number', 'payload']
            for field in required_fields:
                if field not in message:
                    self.logger.error(f"Missing required field '{field}' in message from {conn_id}")
                    return
                    
            # Send ACK immediately
            self._send_ack(conn_id, message['sequence_number'])
            
            # Process based on message type
            message_code = message['message_code']
            
            if message_code == MessageCode.OFFLOAD_REQUEST:
                self._handle_offload_request(conn_id, message)
            elif message_code == MessageCode.SESSION_KEEPALIVE:
                self._handle_session_keepalive(conn_id, message)
            elif message_code == MessageCode.BRIDGE_DP_FAILURE:
                self._handle_bridge_dp_failure(conn_id, message)
            elif message_code == MessageCode.ACK:
                self._handle_ack(conn_id, message)
            else:
                self.logger.warning(f"Unknown message code {message_code} from {conn_id}")
                
        except json.JSONDecodeError as e:
            self.logger.error(f"Invalid JSON from {conn_id}: {e}")
        except Exception as e:
            self.logger.error(f"Error processing message from {conn_id}: {e}")
            
    def _handle_offload_request(self, conn_id: int, message: Dict):
        """Handle OFFLOAD_REQUEST message"""
        payload = message['payload']
        request_id = payload.get('request_id')
        task_id = payload.get('task_id')
        mgwcp_component_id = payload.get('mgwcp_component_id')
        
        self.logger.info(f"Offload request {request_id}: task_id={task_id}, from={mgwcp_component_id}")
        
        # Use decision engine to make allocation decision
        decision = self.decision_engine.make_decision(request_id, task_id, mgwcp_component_id)
        
        if decision.approved:
            self.logger.info(f"APPROVED request {request_id}: assigned MEC {decision.assigned_mec_id}")
            self._send_session_approved(conn_id, request_id, decision.assigned_mec_id)
        else:
            self.logger.info(f"DENIED request {request_id}: {decision.reason}")
            self._send_session_denied(conn_id, request_id, decision.reason_code, decision.reason)
            
    def _handle_session_keepalive(self, conn_id: int, message: Dict):
        """Handle SESSION_KEEPALIVE message"""
        payload = message['payload']
        request_id = payload.get('request_id')
        self.logger.debug(f"Session keepalive for request {request_id}")
        
        # Update session timestamp in decision engine
        self.decision_engine.update_session_keepalive(request_id)
        
    def _handle_bridge_dp_failure(self, conn_id: int, message: Dict):
        """Handle BRIDGE_DP_FAILURE message"""
        payload = message['payload']
        request_id = payload.get('request_id')
        reason = payload.get('reason', 'unknown')
        
        self.logger.warning(f"Bridge DP failure for request {request_id}: {reason}")
        
        # Notify decision engine to clean up session
        self.decision_engine.handle_session_failure(request_id, reason)
        
    def _handle_ack(self, conn_id: int, message: Dict):
        """Handle ACK message"""
        payload = message['payload']
        ack_seq = payload.get('ack_sequence_number')
        self.logger.debug(f"Received ACK from {conn_id} for sequence {ack_seq}")
        
    def _send_ack(self, conn_id: int, ack_sequence_number: int):
        """Send ACK message"""
        response = {
            "component_id": self.component_id,
            "message_code": MessageCode.ACK,
            "message_type": "ACK",
            "sequence_number": self._get_next_sequence(conn_id),
            "payload": {
                "ack_sequence_number": ack_sequence_number
            }
        }
        self._send_message(conn_id, response)
        
    def _send_session_approved(self, conn_id: int, request_id: int, assigned_mec_id: str):
        """Send SESSION_APPROVED message"""
        response = {
            "component_id": self.component_id,
            "message_code": MessageCode.SESSION_APPROVED,
            "message_type": "SESSION_APPROVED",
            "sequence_number": self._get_next_sequence(conn_id),
            "payload": {
                "request_id": request_id,
                "assigned_mec_id": assigned_mec_id
            }
        }
        self._send_message(conn_id, response)
        
    def _send_session_denied(self, conn_id: int, request_id: int, reason_code: int, reason_description: str):
        """Send SESSION_DENIED message"""
        response = {
            "component_id": self.component_id,
            "message_code": MessageCode.SESSION_DENIED,
            "message_type": "SESSION_DENIED",
            "sequence_number": self._get_next_sequence(conn_id),
            "payload": {
                "request_id": request_id,
                "reason_code": reason_code,
                "reason_description": reason_description
            }
        }
        self._send_message(conn_id, response)
        
    def _send_message(self, conn_id: int, message: Dict):
        """Send JSON message to Bridge CP"""
        try:
            conn_state = self.connections.get(conn_id)
            if not conn_state or not conn_state.active:
                self.logger.warning(f"Cannot send message to inactive connection {conn_id}")
                return
                
            message_json = json.dumps(message, separators=(',', ':'))
            self.logger.debug(f"Sending to {conn_id}: {message_json}")
            
            conn_state.client_socket.send(message_json.encode('utf-8'))
            
        except Exception as e:
            self.logger.error(f"Error sending message to {conn_id}: {e}")
            
    def _get_next_sequence(self, conn_id: int) -> int:
        """Get next sequence number for connection"""
        conn_state = self.connections.get(conn_id)
        if conn_state:
            conn_state.sequence_number += 1
            return conn_state.sequence_number
        return 1