"""
Pluggable Offloading Algorithm for the OM
This file contains the decision-making algorithm that can be easily modified for research purposes.

ALGORITHM CONFIGURATION - Modify these values to change algorithm behavior:
"""

import logging
import random
import os
from typing import Dict, Set, Optional, Any
from dataclasses import dataclass

# ===================== ALGORITHM CONFIGURATION =====================
# This section contains all configurable parameters for the algorithm.
# Modify these values to change algorithm behavior without touching the main OM code.

ALGORITHM_NAME = "AUTO_APPROVE_V1"
ALGORITHM_VERSION = "1.0.0"

# Auto-approval behavior
AUTO_APPROVE_ALL_REQUESTS = True  # If True, approves all valid requests
USE_RANDOM_MEC_SELECTION = True   # If True, randomly selects MECs; if False, uses first available

# MEC selection strategy (used when USE_RANDOM_MEC_SELECTION = False)
MEC_SELECTION_STRATEGY = "FIRST_AVAILABLE"  # Options: "FIRST_AVAILABLE", "LEAST_LOADED"

# Logging level for algorithm decisions
ALGORITHM_LOG_LEVEL = "INFO"  # Options: "DEBUG", "INFO", "WARNING"

# Discovery Service query configuration
DISCOVERY_QUERY_INTERVAL_SECONDS = 10  # How often to query for component updates
DISCOVERY_INITIAL_QUERY_DELAY = 2      # Delay before first query after registration

# Future research parameters (currently unused but available for algorithm extensions)
ENABLE_LOAD_BALANCING = False
ENABLE_LATENCY_OPTIMIZATION = False
ENABLE_TASK_AFFINITY = False

# Discovery Service connection reliability
DISCOVERY_KEEPALIVE_MAX_FAILURES = 3  # Number of consecutive failures before shutdown
DISCOVERY_KEEPALIVE_TIMEOUT_SECONDS = 10.0  # Timeout for keepalive responses
DISCOVERY_CONNECTION_RETRY_ATTEMPTS = 3  # Number of reconnection attempts
DISCOVERY_RECONNECT_DELAY_SECONDS = 5.0  # Delay between reconnection attempts

# NEW: Registration retry configuration
DISCOVERY_REGISTRATION_TIMEOUT_MINUTES = 5.0  # Total timeout for registration attempts
DISCOVERY_REGISTRATION_RETRY_DELAY_SECONDS = 2.0  # Delay between registration retry attempts
DISCOVERY_REGISTRATION_BACKOFF_MULTIPLIER = 1.5  # Exponential backoff multiplier for delays
DISCOVERY_REGISTRATION_MAX_DELAY_SECONDS = 30.0  # Maximum delay between retry attempts

# Discovery Service endpoint (moved here for centralized configuration)
# Default points to macvlan-assigned Discovery Service; overridable via environment
DISCOVERY_SERVICE_HOST = os.getenv("DISCOVERY_SERVICE_HOST", "192.168.50.114")
try:
    DISCOVERY_SERVICE_PORT = int(os.getenv("DISCOVERY_SERVICE_PORT", "9090"))
except ValueError:
    DISCOVERY_SERVICE_PORT = 9090

# External data sources configuration
# Configure HTTP API endpoints for algorithm data collection
EXTERNAL_DATA_SOURCES = {
    # Example configuration - modify as needed for your research
    # "traffic_data": {
    #     "url": "http://traffic-api.example.com/current",
    #     "max_cache_size": 1000,
    #     "update_interval": 30,
    #     "timeout_seconds": 5,
    #     "auto_update": True
    # },
    # "weather_data": {
    #     "url": "http://weather-api.example.com/conditions",
    #     "max_cache_size": 500,
    #     "update_interval": 60,
    #     "timeout_seconds": 3,
    #     "auto_update": False
    # }
}

# External data source global settings
EXTERNAL_DATA_DEFAULT_TIMEOUT = 5.0  # Default HTTP timeout in seconds
EXTERNAL_DATA_DEFAULT_MAX_CACHE_SIZE = 1000  # Default maximum cached data length
EXTERNAL_DATA_DEFAULT_UPDATE_INTERVAL = 60  # Default auto-update interval in seconds
EXTERNAL_DATA_ENABLE_BACKGROUND_UPDATES = True  # Enable background data updates

# ===================================================================


@dataclass
class AlgorithmDecision:
    """Result of algorithm decision making"""
    approved: bool
    assigned_mec_id: Optional[str] = None
    reason: str = ""
    reason_code: int = 4001


# Import external data source functionality from separate module
from external_data_sources import DataSourceStatus, DataSourceHandler, ExternalDataManager


class OffloadingAlgorithm:
    """
    Pluggable offloading algorithm implementation.
    
    This class implements the decision-making logic for resource allocation.
    It is designed to be easily replaceable for research purposes.
    """
    
    def __init__(self):
        self.logger = logging.getLogger('OM.algorithm')
        # Apply configured log level
        try:
            self.logger.setLevel(getattr(logging, ALGORITHM_LOG_LEVEL.upper(), logging.INFO))
        except Exception:
            self.logger.setLevel(logging.INFO)
        self.logger.info(f"Algorithm initialized: {ALGORITHM_NAME} v{ALGORITHM_VERSION}")
        self.logger.info(f"Auto-approve mode: {AUTO_APPROVE_ALL_REQUESTS}")
        self.logger.info(f"MEC selection: {'RANDOM' if USE_RANDOM_MEC_SELECTION else MEC_SELECTION_STRATEGY}")
        self.logger.info(f"Discovery query interval: {DISCOVERY_QUERY_INTERVAL_SECONDS}s")
        self.logger.info(f"Discovery service target: {DISCOVERY_SERVICE_HOST}:{DISCOVERY_SERVICE_PORT}")
        if DISCOVERY_SERVICE_HOST in ("localhost", "0.0.0.0") or DISCOVERY_SERVICE_HOST.startswith("127."):
            self.logger.warning("Configured Discovery Service host is loopback; remote components may fail to connect.")
            
        # Initialize external data manager for algorithm research
        self.external_data = ExternalDataManager()
        if self.external_data.get_source_names():
            self.logger.info(f"External data sources available: {', '.join(self.external_data.get_source_names())}")
        else:
            self.logger.info("No external data sources configured")
            
        # Algorithm statistics for research
        self.decisions_made = 0
        self.approvals = 0
        self.denials = 0
    
    def make_allocation_decision(self, 
                               request_id: int,
                               task_id: int,
                               task_config: Dict[str, Any],
                               mgwcp_component_id: str,
                               available_mecs: Set[str],
                               mec_assignments: Dict[str, Optional[str]],
                               active_sessions: int) -> AlgorithmDecision:
        """
        Make resource allocation decision for an offload request.
        
        This is the main algorithm entry point that can be completely rewritten
        for different allocation strategies.
        """
        
        self.decisions_made += 1
        
        self.logger.info(f"[{ALGORITHM_NAME}] Processing request {request_id}")
        self.logger.info(f"  Task: {task_id} ({task_config['task_name']})")
        self.logger.info(f"  VHC: {mgwcp_component_id}")
        self.logger.info(f"  Available MECs: {len(available_mecs)}")
        self.logger.info(f"  Active sessions: {active_sessions}")
        
        # AUTO-APPROVE ALGORITHM IMPLEMENTATION
        if AUTO_APPROVE_ALL_REQUESTS:
            return self._auto_approve_strategy(
                request_id, task_id, mgwcp_component_id, 
                available_mecs, mec_assignments
            )
        else:
            # Placeholder for future sophisticated algorithms
            return self._sophisticated_strategy(
                request_id, task_id, task_config, mgwcp_component_id,
                available_mecs, mec_assignments, active_sessions
            )
    
    def _auto_approve_strategy(self, 
                             request_id: int,
                             task_id: int, 
                             mgwcp_component_id: str,
                             available_mecs: Set[str],
                             mec_assignments: Dict[str, Optional[str]]) -> AlgorithmDecision:
        """
        Simple auto-approve strategy: approve all requests with available MECs.
        """
        
        # Find available (unassigned) MECs
        free_mecs = [
            mec_id for mec_id in available_mecs 
            if mec_assignments.get(mec_id) is None
        ]
        
        if not free_mecs:
            self.denials += 1
            reason = "No available MECs for assignment"
            self.logger.info(f"[{ALGORITHM_NAME}] DENIED request {request_id}: {reason}")
            return AlgorithmDecision(
                approved=False,
                reason=reason,
                reason_code=4001
            )
        
        # Select MEC based on configuration
        if USE_RANDOM_MEC_SELECTION:
            selected_mec = random.choice(free_mecs)
            selection_method = "RANDOM"
        else:
            selected_mec = sorted(free_mecs)[0]  # First available
            selection_method = "FIRST_AVAILABLE"
        
        self.approvals += 1
        self.logger.info(f"[{ALGORITHM_NAME}] APPROVED request {request_id}")
        self.logger.info(f"  Assigned MEC: {selected_mec} (method: {selection_method})")
        self.logger.info(f"  Algorithm stats: {self.approvals}/{self.decisions_made} approved")
        
        return AlgorithmDecision(
            approved=True,
            assigned_mec_id=selected_mec,
            reason=f"Auto-approved with MEC {selected_mec}",
            reason_code=0
        )
    
    def _sophisticated_strategy(self, 
                              request_id: int,
                              task_id: int,
                              task_config: Dict[str, Any],
                              mgwcp_component_id: str,
                              available_mecs: Set[str],
                              mec_assignments: Dict[str, Optional[str]],
                              active_sessions: int) -> AlgorithmDecision:
        """
        Placeholder for future sophisticated allocation algorithms.
        
        This method can be implemented with:
        - Load balancing algorithms
        - Latency optimization
        - Task affinity considerations
        - Machine learning-based decisions
        - etc.
        """
        
        self.logger.info(f"[{ALGORITHM_NAME}] Using sophisticated strategy (not implemented)")
        
        # For now, fall back to auto-approve
        return self._auto_approve_strategy(
            request_id, task_id, mgwcp_component_id, 
            available_mecs, mec_assignments
        )
    
    def get_algorithm_stats(self) -> Dict[str, Any]:
        """Get algorithm statistics for research analysis"""
        return {
            "algorithm_name": ALGORITHM_NAME,
            "algorithm_version": ALGORITHM_VERSION,
            "decisions_made": self.decisions_made,
            "approvals": self.approvals,
            "denials": self.denials,
            "approval_rate": self.approvals / max(1, self.decisions_made),
            "configuration": {
                "auto_approve_all": AUTO_APPROVE_ALL_REQUESTS,
                "random_mec_selection": USE_RANDOM_MEC_SELECTION,
                "mec_selection_strategy": MEC_SELECTION_STRATEGY,
                "discovery_query_interval": DISCOVERY_QUERY_INTERVAL_SECONDS,
                "discovery_service_host": DISCOVERY_SERVICE_HOST,
                "discovery_service_port": DISCOVERY_SERVICE_PORT
            }
        }
    
    def cleanup(self):
        """
        Clean up algorithm resources including external data sources.
        
        This method should be called when the algorithm is being shut down
        to properly clean up background threads and resources.
        """
        self.logger.info("Cleaning up algorithm resources")
        if hasattr(self, 'external_data'):
            self.external_data.cleanup()
        self.logger.info("Algorithm cleanup complete")