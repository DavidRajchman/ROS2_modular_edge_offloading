"""
Logging configuration for the Offloading Manager
Provides structured logging with detailed context for research purposes
"""

import logging
import sys
from datetime import datetime


class OMFormatter(logging.Formatter):
    """Custom formatter for OM logs with structured format"""
    
    def format(self, record):
        # Create structured log format: [TIMESTAMP] [LEVEL] [COMPONENT] [FUNCTION] - MESSAGE
        timestamp = datetime.fromtimestamp(record.created).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        
        # Extract component from logger name (e.g., 'OM.server' -> 'server')
        component = record.name.replace('OM.', '') if record.name.startswith('OM.') else record.name
        
        # Format the message
        formatted = f"[{timestamp}] [{record.levelname:5}] [{component:12}] [{record.funcName:15}] - {record.getMessage()}"
        
        # Add exception info if present
        if record.exc_info:
            formatted += '\n' + self.formatException(record.exc_info)
            
        return formatted


def setup_logging(level: str = 'INFO'):
    """Setup logging configuration for OM"""
    
    # Create root logger
    root_logger = logging.getLogger()
    root_logger.setLevel(getattr(logging, level))
    
    # Remove existing handlers
    for handler in root_logger.handlers[:]:
        root_logger.removeHandler(handler)
    
    # Create console handler with custom formatter
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(getattr(logging, level))
    console_handler.setFormatter(OMFormatter())
    
    # Add handler to root logger
    root_logger.addHandler(console_handler)
    
    # Create file handler for persistent logging
    file_handler = logging.FileHandler('/home/ubuntu/OffloadingManager/om.log')
    file_handler.setLevel(logging.DEBUG)  # Always log everything to file
    file_handler.setFormatter(OMFormatter())
    root_logger.addHandler(file_handler)
    
    # Set specific logger levels to reduce noise
    logging.getLogger('urllib3').setLevel(logging.WARNING)
    logging.getLogger('requests').setLevel(logging.WARNING)