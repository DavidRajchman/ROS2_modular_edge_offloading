"""
Configuration Manager for the Offloading Manager
Handles loading and validation of static global configuration
"""

import json
import logging
from pathlib import Path
from typing import Dict, List, Any, Optional


class ConfigManager:
    """Manages OM configuration loading and validation"""
    
    def __init__(self):
        self.logger = logging.getLogger('OM.config')
        self.config: Dict[str, Any] = {}
        self.config_file_path: Optional[str] = None
        
    def load_config(self, config_file: str) -> bool:
        """Load configuration from JSON file"""
        try:
            self.config_file_path = config_file
            config_path = Path(config_file)
            
            if not config_path.exists():
                self.logger.error(f"Configuration file does not exist: {config_file}")
                return False
                
            if not config_path.is_file():
                self.logger.error(f"Configuration path is not a file: {config_file}")
                return False
                
            self.logger.info(f"Loading configuration from: {config_file}")
            
            with open(config_path, 'r') as f:
                self.config = json.load(f)
                
            if not self._validate_config():
                return False
                
            self.logger.info("Configuration validation successful")
            self._log_config_summary()
            return True
            
        except json.JSONDecodeError as e:
            self.logger.error(f"Invalid JSON in configuration file: {e}")
            return False
        except Exception as e:
            self.logger.error(f"Error loading configuration: {e}")
            return False
            
    def _validate_config(self) -> bool:
        """Validate configuration structure and values"""
        required_fields = ['available_tasks', 'default_session_timeout', 'max_concurrent_sessions']
        
        for field in required_fields:
            if field not in self.config:
                self.logger.error(f"Missing required configuration field: {field}")
                return False
                
        # Validate available_tasks
        if not isinstance(self.config['available_tasks'], list):
            self.logger.error("'available_tasks' must be a list")
            return False
            
        task_ids = set()
        for i, task in enumerate(self.config['available_tasks']):
            if not self._validate_task(task, i):
                return False
                
            # Check for duplicate task IDs
            task_id = task['task_id']
            if task_id in task_ids:
                self.logger.error(f"Duplicate task_id found: {task_id}")
                return False
            task_ids.add(task_id)
            
        # Validate timeouts and limits
        if not isinstance(self.config['default_session_timeout'], int) or self.config['default_session_timeout'] <= 0:
            self.logger.error("'default_session_timeout' must be a positive integer")
            return False
            
        if not isinstance(self.config['max_concurrent_sessions'], int) or self.config['max_concurrent_sessions'] <= 0:
            self.logger.error("'max_concurrent_sessions' must be a positive integer")
            return False
            
        return True
        
    def _validate_task(self, task: Dict[str, Any], index: int) -> bool:
        """Validate individual task configuration"""
        required_task_fields = ['task_id', 'task_name', 'input_message_types', 'output_message_types']
        
        for field in required_task_fields:
            if field not in task:
                self.logger.error(f"Task {index}: missing required field '{field}'")
                return False
                
        # Validate task_id
        if not isinstance(task['task_id'], int) or task['task_id'] <= 0:
            self.logger.error(f"Task {index}: 'task_id' must be a positive integer")
            return False
            
        # Validate task_name
        if not isinstance(task['task_name'], str) or not task['task_name'].strip():
            self.logger.error(f"Task {index}: 'task_name' must be a non-empty string")
            return False
            
        # Validate message types
        for msg_type_field in ['input_message_types', 'output_message_types']:
            msg_types = task[msg_type_field]
            if not isinstance(msg_types, list):
                self.logger.error(f"Task {index}: '{msg_type_field}' must be a list")
                return False
                
            for msg_type in msg_types:
                if not isinstance(msg_type, int) or msg_type <= 0:
                    self.logger.error(f"Task {index}: message types must be positive integers")
                    return False
                    
        return True
        
    def _log_config_summary(self):
        """Log configuration summary for diagnostics"""
        self.logger.info("=== Configuration Summary ===")
        self.logger.info(f"Available tasks: {len(self.config['available_tasks'])}")
        
        for task in self.config['available_tasks']:
            self.logger.info(f"  Task {task['task_id']}: {task['task_name']} "
                           f"(in: {task['input_message_types']}, out: {task['output_message_types']})")
                           
        self.logger.info(f"Default session timeout: {self.config['default_session_timeout']} seconds")
        self.logger.info(f"Max concurrent sessions: {self.config['max_concurrent_sessions']}")
        
    def get_config(self) -> Dict[str, Any]:
        """Get the loaded configuration"""
        return self.config.copy()
        
    def get_task_config(self, task_id: int) -> Optional[Dict[str, Any]]:
        """Get configuration for specific task ID"""
        for task in self.config.get('available_tasks', []):
            if task['task_id'] == task_id:
                return task.copy()
        return None
        
    def get_config_json_string(self) -> str:
        """Get configuration as JSON string for Discovery Service"""
        return json.dumps(self.config, separators=(',', ':'))