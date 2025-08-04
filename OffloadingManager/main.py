#!/usr/bin/env python3
"""
Offloading Manager (OM) - Main Entry Point

Central decision-making component for the modular gateway offloading system.
Handles Bridge CP connections and manages resource allocation decisions.
"""

import argparse
import signal
import sys
import threading
import time
import logging
from pathlib import Path

from om_server import OffloadingManagerServer
from config_manager import ConfigManager
from logger_config import setup_logging
from discovery_client import DiscoveryClient


class OffloadingManager:
    def __init__(self, config_file: str):
        self.logger = logging.getLogger('OM.main')
        self.config_file = config_file
        self.server = None
        self.discovery_client = None
        self.shutdown_event = threading.Event()
        self.shutdown_reason = "Unknown"
        
        # Setup signal handlers for graceful shutdown
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
        
    def _signal_handler(self, signum, frame):
        """Handle shutdown signals"""
        signal_names = {signal.SIGINT: 'SIGINT', signal.SIGTERM: 'SIGTERM'}
        self.shutdown_reason = f"Received signal {signal_names.get(signum, signum)}"
        self.logger.info(f"Signal handler triggered: {self.shutdown_reason}")
        self.shutdown()
        
    def start(self):
        """Start the Offloading Manager"""
        try:
            self.logger.info("=== Offloading Manager Starting ===")
            self.logger.info(f"Configuration file: {self.config_file}")
            
            # Load and validate configuration
            config_manager = ConfigManager()
            if not config_manager.load_config(self.config_file):
                self.shutdown_reason = "Configuration loading failed"
                self._shutdown_with_error(self.shutdown_reason)
                return False
                
            self.logger.info("Configuration loaded successfully")
            self.logger.info(f"Available tasks: {len(config_manager.config.get('available_tasks', []))}")
            self.logger.info(f"Max concurrent sessions: {config_manager.config.get('max_concurrent_sessions', 'Not set')}")
            
            # Initialize Discovery Service client
            self.logger.info("Initializing Discovery Service client...")
            self.discovery_client = DiscoveryClient(config_manager, self.shutdown_event)
            
            # Connect and register with Discovery Service
            self.logger.info("Registering with Discovery Service...")
            if not self.discovery_client.connect_and_register():
                self.shutdown_reason = "Discovery Service registration failed"
                self._shutdown_with_error(self.shutdown_reason)
                return False
                
            self.logger.info("Successfully registered with Discovery Service")
            
            # Create and start the OM server (only after successful registration)
            self.logger.info("Starting OM TCP server...")
            self.server = OffloadingManagerServer(config_manager, self.shutdown_event)
            
            # Set up resource update callback from Discovery Service to Decision Engine
            self.discovery_client.set_resource_update_callback(
                self.server.decision_engine.update_available_resources
            )
            
            # Start server in separate thread
            server_thread = threading.Thread(target=self.server.start, name="OMServer")
            server_thread.daemon = True
            server_thread.start()
            
            self.logger.info("OM Server started on localhost:8100")
            self.logger.info("=== OM Ready for Bridge CP connections ===")
            
            # Main loop - wait for shutdown signal
            while not self.shutdown_event.is_set():
                time.sleep(0.1)
                
            self.logger.info("Shutdown event received, initiating graceful shutdown")
            return True
            
        except Exception as e:
            self.shutdown_reason = f"Startup error: {str(e)}"
            self._shutdown_with_error(self.shutdown_reason, e)
            return False
            
    def shutdown(self):
        """Initiate graceful shutdown"""
        self.logger.info(f"Shutdown initiated: {self.shutdown_reason}")
        self.shutdown_event.set()
        
        # Stop OM server
        if self.server:
            self.server.stop()
            
        # Disconnect from Discovery Service
        if self.discovery_client:
            self.discovery_client.disconnect()
            
        self.logger.info("=== OM Shutdown Complete ===")
        
    def _shutdown_with_error(self, reason: str, exception: Exception = None):
        """Shutdown with error logging"""
        self.logger.error(f"CRITICAL ERROR - {reason}")
        if exception:
            self.logger.error(f"Exception details: {exception}", exc_info=True)
        self.shutdown()


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Offloading Manager (OM)')
    parser.add_argument('--config', '-c', 
                       default='/home/ubuntu/OffloadingManager/config.json',
                       help='Path to configuration JSON file')
    parser.add_argument('--log-level', 
                       choices=['DEBUG', 'INFO', 'WARNING', 'ERROR'],
                       default='INFO',
                       help='Logging level')
    
    args = parser.parse_args()
    
    # Setup logging
    setup_logging(args.log_level)
    logger = logging.getLogger('OM.main')
    
    # Validate config file exists
    config_path = Path(args.config)
    if not config_path.exists():
        logger.error(f"Configuration file not found: {args.config}")
        sys.exit(1)
        
    # Create and start OM
    om = OffloadingManager(args.config)
    success = om.start()
    
    # Ensure all logs are flushed before exit
    logging.shutdown()
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()