"""
Decision Engine for Offloading Manager
Handles resource allocation decisions with pluggable algorithm support
"""

import logging
import time
from typing import Dict, Optional, Set
from dataclasses import dataclass
from threading import Lock

from config_manager import ConfigManager
from algorithm import OffloadingAlgorithm  # Pluggable algorithm


@dataclass
class DecisionResult:
    """Result of an offloading decision"""
    approved: bool
    assigned_mec_id: Optional[str] = None
    reason: str = ""
    reason_code: int = 4001  # Default: No available resources


@dataclass  
class SessionInfo:
    """Information about an active session"""
    request_id: int
    task_id: int
    mgwcp_component_id: str
    assigned_mec_id: Optional[str]
    timestamp: float
    

class DecisionEngine:
    """Core decision engine for resource allocation"""
    
    def __init__(self, config_manager: ConfigManager):
        self.logger = logging.getLogger('OM.decision')
        self.config_manager = config_manager
        self.algorithm = OffloadingAlgorithm()  # Pluggable algorithm
        
        # Session tracking
        self.active_sessions: Dict[int, SessionInfo] = {}
        self.session_lock = Lock()
        
        # Resource state (will be populated by Discovery Service integration)
        self.available_mecs: Set[str] = set()
        self.mec_assignments: Dict[str, Optional[str]] = {}  # mec_id -> assigned_vhc_id
        
        self.logger.info("Decision engine initialized")
        
    def make_decision(self, request_id: int, task_id: int, mgwcp_component_id: str) -> DecisionResult:
        """Make offloading decision for incoming request"""
        start_time = time.time()
        
        self.logger.info(f"Making decision for request {request_id}: task={task_id}, vhc={mgwcp_component_id}")
        
        try:
            # Check if task exists in configuration
            task_config = self.config_manager.get_task_config(task_id)
            if not task_config:
                reason = f"Unknown task_id {task_id}"
                self.logger.warning(f"Request {request_id} denied: {reason}")
                return DecisionResult(approved=False, reason=reason, reason_code=4002)
            
            # Check concurrent session limit
            with self.session_lock:
                max_sessions = self.config_manager.config['max_concurrent_sessions']
                if len(self.active_sessions) >= max_sessions:
                    reason = f"Maximum concurrent sessions ({max_sessions}) reached"
                    self.logger.warning(f"Request {request_id} denied: {reason}")
                    return DecisionResult(approved=False, reason=reason, reason_code=4003)
                
                # Check if request_id already exists
                if request_id in self.active_sessions:
                    reason = f"Duplicate request_id {request_id}"
                    self.logger.warning(f"Request {request_id} denied: {reason}")
                    return DecisionResult(approved=False, reason=reason, reason_code=4004)
            
            # Use pluggable algorithm to make decision
            decision = self.algorithm.make_allocation_decision(
                request_id=request_id,
                task_id=task_id,
                task_config=task_config,
                mgwcp_component_id=mgwcp_component_id,
                available_mecs=self.available_mecs.copy(),
                mec_assignments=self.mec_assignments.copy(),
                active_sessions=len(self.active_sessions)
            )
            
            decision_time = time.time() - start_time
            self.logger.info(f"Algorithm decision for request {request_id}: "
                           f"approved={decision.approved}, time={decision_time:.3f}s")
            
            # If approved, track the session
            if decision.approved and decision.assigned_mec_id:
                with self.session_lock:
                    session_info = SessionInfo(
                        request_id=request_id,
                        task_id=task_id,
                        mgwcp_component_id=mgwcp_component_id,
                        assigned_mec_id=decision.assigned_mec_id,
                        timestamp=time.time()
                    )
                    self.active_sessions[request_id] = session_info
                    
                    # Update MEC assignment
                    self.mec_assignments[decision.assigned_mec_id] = mgwcp_component_id
                    
                self.logger.info(f"Session {request_id} created: MEC {decision.assigned_mec_id} assigned to VHC {mgwcp_component_id}")
            
            return decision
            
        except Exception as e:
            self.logger.error(f"Error making decision for request {request_id}: {e}")
            return DecisionResult(
                approved=False, 
                reason=f"Internal decision error: {str(e)}", 
                reason_code=5000
            )
    
    def update_session_keepalive(self, request_id: int):
        """Update session keepalive timestamp"""
        with self.session_lock:
            if request_id in self.active_sessions:
                self.active_sessions[request_id].timestamp = time.time()
                self.logger.debug(f"Updated keepalive for session {request_id}")
            else:
                self.logger.warning(f"Keepalive for unknown session {request_id}")
    
    def handle_session_failure(self, request_id: int, reason: str):
        """Handle session failure notification"""
        with self.session_lock:
            if request_id in self.active_sessions:
                session = self.active_sessions[request_id]
                
                # Free up MEC assignment
                if session.assigned_mec_id and session.assigned_mec_id in self.mec_assignments:
                    self.mec_assignments[session.assigned_mec_id] = None
                    
                # Remove session
                del self.active_sessions[request_id]
                
                self.logger.info(f"Session {request_id} failed and cleaned up: {reason}")
            else:
                self.logger.warning(f"Failure notification for unknown session {request_id}")
    
    def get_session_count(self) -> int:
        """Get current active session count"""
        with self.session_lock:
            return len(self.active_sessions)
    
    def get_session_info(self, request_id: int) -> Optional[SessionInfo]:
        """Get session information"""
        with self.session_lock:
            return self.active_sessions.get(request_id)
    
    def update_available_mecs(self, mec_list: Set[str]):
        """Update list of available MECs (called by Discovery Service integration)"""
        with self.session_lock:
            old_mecs = self.available_mecs.copy()
            self.available_mecs = mec_list.copy()
            
            # Initialize assignments for new MECs
            for mec_id in mec_list:
                if mec_id not in self.mec_assignments:
                    self.mec_assignments[mec_id] = None
                    
            # Clean up assignments for removed MECs
            removed_mecs = old_mecs - mec_list
            for mec_id in removed_mecs:
                if mec_id in self.mec_assignments:
                    del self.mec_assignments[mec_id]
                    
            if old_mecs != mec_list:
                self.logger.info(f"MEC availability updated: {len(mec_list)} available MECs")
                self.logger.debug(f"Available MECs: {sorted(mec_list)}")
    
    def update_available_resources(self, mec_list: Set[str]):
        """Update available resources - callback method for Discovery Service integration"""
        self.logger.debug(f"Received resource update callback with {len(mec_list)} MECs")
        self.update_available_mecs(mec_list)